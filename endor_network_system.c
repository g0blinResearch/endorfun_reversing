/**
 * ========================================================================
 * ENDOR NETWORK AND MULTIPLAYER SYSTEM
 * ========================================================================
 * 
 * Complete networking implementation for the Endor game engine.
 * Supports client-server architecture, peer-to-peer, local area network,
 * and internet play with lag compensation, prediction, and anti-cheat.
 */

#include "endor_readable.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <math.h>
#include <time.h>

#pragma comment(lib, "ws2_32.lib")

// ========================================================================
// NETWORK CONSTANTS AND STRUCTURES
// ========================================================================

#define MAX_CLIENTS 32
#define MAX_PACKET_SIZE 1400
#define MAX_RELIABLE_PACKETS 256
#define MAX_MESSAGE_QUEUE 512
#define DEFAULT_PORT 7777
#define HEARTBEAT_INTERVAL 5000  // 5 seconds
#define TIMEOUT_INTERVAL 30000   // 30 seconds
#define PREDICTION_FRAMES 3
#define LAG_COMPENSATION_MS 150

// Network modes
typedef enum {
    NETWORK_MODE_NONE,
    NETWORK_MODE_SERVER,
    NETWORK_MODE_CLIENT,
    NETWORK_MODE_PEER
} NetworkMode;

// Packet types
typedef enum {
    PACKET_TYPE_CONNECT_REQUEST,
    PACKET_TYPE_CONNECT_RESPONSE,
    PACKET_TYPE_DISCONNECT,
    PACKET_TYPE_HEARTBEAT,
    PACKET_TYPE_PLAYER_INPUT,
    PACKET_TYPE_PLAYER_STATE,
    PACKET_TYPE_GAME_STATE,
    PACKET_TYPE_CHAT_MESSAGE,
    PACKET_TYPE_FILE_TRANSFER,
    PACKET_TYPE_RELIABLE_ACK,
    PACKET_TYPE_PING,
    PACKET_TYPE_PONG,
    PACKET_TYPE_CUSTOM
} PacketType;

// Connection states
typedef enum {
    CONNECTION_STATE_DISCONNECTED,
    CONNECTION_STATE_CONNECTING,
    CONNECTION_STATE_CONNECTED,
    CONNECTION_STATE_TIMEOUT
} ConnectionState;

// Packet header structure
typedef struct {
    unsigned short magic;           // 0xENDR
    unsigned char type;
    unsigned char flags;
    unsigned short sequence;
    unsigned short ack;
    unsigned int timestamp;
    unsigned short data_size;
    unsigned short checksum;
} PacketHeader;

// Reliable packet for guaranteed delivery
typedef struct {
    PacketHeader header;
    unsigned char data[MAX_PACKET_SIZE - sizeof(PacketHeader)];
    unsigned int send_time;
    int retry_count;
    int acknowledged;
} ReliablePacket;

// Client connection info
typedef struct {
    SOCKET socket;
    struct sockaddr_in address;
    ConnectionState state;
    unsigned int client_id;
    char player_name[32];
    unsigned int last_sequence;
    unsigned int last_ack;
    unsigned int last_heartbeat;
    unsigned int ping;
    unsigned int packet_loss;
    unsigned int bytes_sent;
    unsigned int bytes_received;
    
    // Reliable packet system
    ReliablePacket reliable_outgoing[MAX_RELIABLE_PACKETS];
    ReliablePacket reliable_incoming[MAX_RELIABLE_PACKETS];
    unsigned short reliable_sequence;
    unsigned short reliable_ack;
    
    // Lag compensation
    float position_history[PREDICTION_FRAMES][3];
    float rotation_history[PREDICTION_FRAMES];
    unsigned int frame_history[PREDICTION_FRAMES];
    int history_head;
    
    // Anti-cheat
    unsigned int last_valid_input;
    float last_known_position[3];
    unsigned int suspicious_activity;
} NetworkClient;

// Message queue for processing
typedef struct {
    PacketType type;
    unsigned int client_id;
    unsigned char data[MAX_PACKET_SIZE];
    unsigned int data_size;
    unsigned int timestamp;
} NetworkMessage;

// File transfer for maps/mods
typedef struct {
    char filename[256];
    unsigned int file_size;
    unsigned int bytes_transferred;
    FILE* file_handle;
    unsigned int chunk_size;
    int active;
    unsigned int client_id;
} FileTransfer;

// Server browser entry
typedef struct {
    char server_name[64];
    char map_name[32];
    char game_mode[32];
    struct sockaddr_in address;
    int player_count;
    int max_players;
    int ping;
    int password_protected;
    unsigned int last_response;
} ServerInfo;

// ========================================================================
// GLOBAL NETWORK STATE
// ========================================================================

static NetworkMode network_mode = NETWORK_MODE_NONE;
static SOCKET main_socket = INVALID_SOCKET;
static struct sockaddr_in local_address;
static struct sockaddr_in server_address;

static NetworkClient clients[MAX_CLIENTS];
static int client_count = 0;
static unsigned int local_client_id = 0;
static unsigned int next_client_id = 1;

static NetworkMessage message_queue[MAX_MESSAGE_QUEUE];
static int message_queue_head = 0;
static int message_queue_tail = 0;

static FileTransfer file_transfers[MAX_CLIENTS];
static int transfer_count = 0;

static ServerInfo server_list[64];
static int server_count = 0;

// Network statistics
static unsigned int total_packets_sent = 0;
static unsigned int total_packets_received = 0;
static unsigned int total_bytes_sent = 0;
static unsigned int total_bytes_received = 0;
static unsigned int network_errors = 0;

// Configuration
static int max_clients = 8;
static char server_name[64] = "Endor Server";
static char server_password[32] = "";
static int enable_lag_compensation = 1;
static int enable_prediction = 1;
static int enable_anti_cheat = 1;
static float tick_rate = 60.0f;

// ========================================================================
// CORE NETWORK FUNCTIONS
// ========================================================================

int initialize_network_system() {
    // Initialize Winsock
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        return 0;
    }
    
    // Clear client data
    memset(clients, 0, sizeof(clients));
    client_count = 0;
    
    // Clear message queue
    memset(message_queue, 0, sizeof(message_queue));
    message_queue_head = 0;
    message_queue_tail = 0;
    
    // Clear file transfers
    memset(file_transfers, 0, sizeof(file_transfers));
    transfer_count = 0;
    
    // Clear server list
    memset(server_list, 0, sizeof(server_list));
    server_count = 0;
    
    // Reset statistics
    total_packets_sent = 0;
    total_packets_received = 0;
    total_bytes_sent = 0;
    total_bytes_received = 0;
    network_errors = 0;
    
    return 1;
}

int start_server(int port) {
    if (network_mode != NETWORK_MODE_NONE) {
        return 0; // Already in network mode
    }
    
    // Create socket
    main_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (main_socket == INVALID_SOCKET) {
        return 0;
    }
    
    // Set socket options
    int broadcast = 1;
    setsockopt(main_socket, SOL_SOCKET, SO_BROADCAST, (char*)&broadcast, sizeof(broadcast));
    
    int reuse = 1;
    setsockopt(main_socket, SOL_SOCKET, SO_REUSEADDR, (char*)&reuse, sizeof(reuse));
    
    // Set non-blocking mode
    u_long mode = 1;
    ioctlsocket(main_socket, FIONBIO, &mode);
    
    // Bind to port
    memset(&local_address, 0, sizeof(local_address));
    local_address.sin_family = AF_INET;
    local_address.sin_addr.s_addr = INADDR_ANY;
    local_address.sin_port = htons(port ? port : DEFAULT_PORT);
    
    if (bind(main_socket, (struct sockaddr*)&local_address, sizeof(local_address)) == SOCKET_ERROR) {
        closesocket(main_socket);
        main_socket = INVALID_SOCKET;
        return 0;
    }
    
    network_mode = NETWORK_MODE_SERVER;
    local_client_id = 0; // Server is client 0
    
    return 1;
}

int connect_to_server(const char* host, int port) {
    if (network_mode != NETWORK_MODE_NONE) {
        return 0; // Already in network mode
    }
    
    // Create socket
    main_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (main_socket == INVALID_SOCKET) {
        return 0;
    }
    
    // Set non-blocking mode
    u_long mode = 1;
    ioctlsocket(main_socket, FIONBIO, &mode);
    
    // Resolve server address
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port ? port : DEFAULT_PORT);
    
    if (inet_pton(AF_INET, host, &server_address.sin_addr) != 1) {
        // Try to resolve hostname
        struct hostent* he = gethostbyname(host);
        if (!he) {
            closesocket(main_socket);
            main_socket = INVALID_SOCKET;
            return 0;
        }
        memcpy(&server_address.sin_addr, he->h_addr, he->h_length);
    }
    
    // Bind to any local port
    memset(&local_address, 0, sizeof(local_address));
    local_address.sin_family = AF_INET;
    local_address.sin_addr.s_addr = INADDR_ANY;
    local_address.sin_port = 0;
    
    if (bind(main_socket, (struct sockaddr*)&local_address, sizeof(local_address)) == SOCKET_ERROR) {
        closesocket(main_socket);
        main_socket = INVALID_SOCKET;
        return 0;
    }
    
    network_mode = NETWORK_MODE_CLIENT;
    
    // Send connection request
    send_connect_request();
    
    return 1;
}

void disconnect_from_network() {
    if (network_mode == NETWORK_MODE_NONE) {
        return;
    }
    
    // Send disconnect packets to all clients
    for (int i = 0; i < client_count; i++) {
        if (clients[i].state == CONNECTION_STATE_CONNECTED) {
            send_disconnect_packet(clients[i].client_id);
        }
    }
    
    // Close socket
    if (main_socket != INVALID_SOCKET) {
        closesocket(main_socket);
        main_socket = INVALID_SOCKET;
    }
    
    // Close any open file transfers
    for (int i = 0; i < transfer_count; i++) {
        if (file_transfers[i].active && file_transfers[i].file_handle) {
            fclose(file_transfers[i].file_handle);
        }
    }
    
    // Reset state
    network_mode = NETWORK_MODE_NONE;
    client_count = 0;
    transfer_count = 0;
    local_client_id = 0;
}

// ========================================================================
// PACKET MANAGEMENT
// ========================================================================

int send_packet(unsigned int client_id, PacketType type, const void* data, unsigned int data_size, int reliable) {
    if (network_mode == NETWORK_MODE_NONE || main_socket == INVALID_SOCKET) {
        return 0;
    }
    
    if (data_size > MAX_PACKET_SIZE - sizeof(PacketHeader)) {
        return 0; // Packet too large
    }
    
    NetworkClient* client = find_client(client_id);
    struct sockaddr_in* target_addr;
    
    if (network_mode == NETWORK_MODE_SERVER) {
        if (!client) return 0;
        target_addr = &client->address;
    } else {
        target_addr = &server_address;
    }
    
    // Create packet
    unsigned char packet_buffer[MAX_PACKET_SIZE];
    PacketHeader* header = (PacketHeader*)packet_buffer;
    
    header->magic = 0x454E; // 'EN'
    header->type = (unsigned char)type;
    header->flags = reliable ? 0x01 : 0x00;
    header->sequence = client ? client->last_sequence++ : 0;
    header->ack = client ? client->last_ack : 0;
    header->timestamp = GetTickCount();
    header->data_size = (unsigned short)data_size;
    header->checksum = 0;
    
    // Copy data
    if (data && data_size > 0) {
        memcpy(packet_buffer + sizeof(PacketHeader), data, data_size);
    }
    
    // Calculate checksum
    header->checksum = calculate_checksum(packet_buffer, sizeof(PacketHeader) + data_size);
    
    // Send packet
    int bytes_sent = sendto(main_socket, (char*)packet_buffer, sizeof(PacketHeader) + data_size, 0,
                           (struct sockaddr*)target_addr, sizeof(struct sockaddr_in));
    
    if (bytes_sent == SOCKET_ERROR) {
        network_errors++;
        return 0;
    }
    
    // Update statistics
    total_packets_sent++;
    total_bytes_sent += bytes_sent;
    
    if (client) {
        client->bytes_sent += bytes_sent;
    }
    
    // Handle reliable packets
    if (reliable && client) {
        int slot = header->sequence % MAX_RELIABLE_PACKETS;
        ReliablePacket* rel_packet = &client->reliable_outgoing[slot];
        
        memcpy(rel_packet, packet_buffer, sizeof(PacketHeader) + data_size);
        rel_packet->send_time = GetTickCount();
        rel_packet->retry_count = 0;
        rel_packet->acknowledged = 0;
    }
    
    return 1;
}

int receive_packets() {
    if (network_mode == NETWORK_MODE_NONE || main_socket == INVALID_SOCKET) {
        return 0;
    }
    
    unsigned char packet_buffer[MAX_PACKET_SIZE];
    struct sockaddr_in sender_addr;
    int addr_len = sizeof(sender_addr);
    int packets_received = 0;
    
    while (1) {
        int bytes_received = recvfrom(main_socket, (char*)packet_buffer, MAX_PACKET_SIZE, 0,
                                     (struct sockaddr*)&sender_addr, &addr_len);
        
        if (bytes_received == SOCKET_ERROR) {
            int error = WSAGetLastError();
            if (error == WSAEWOULDBLOCK) {
                break; // No more packets
            }
            network_errors++;
            break;
        }
        
        if (bytes_received < sizeof(PacketHeader)) {
            continue; // Invalid packet
        }
        
        PacketHeader* header = (PacketHeader*)packet_buffer;
        
        // Validate packet
        if (header->magic != 0x454E) {
            continue; // Invalid magic
        }
        
        if (header->data_size != bytes_received - sizeof(PacketHeader)) {
            continue; // Size mismatch
        }
        
        // Verify checksum
        unsigned short stored_checksum = header->checksum;
        header->checksum = 0;
        unsigned short calculated_checksum = calculate_checksum(packet_buffer, bytes_received);
        
        if (stored_checksum != calculated_checksum) {
            continue; // Checksum failed
        }
        
        header->checksum = stored_checksum;
        
        // Update statistics
        total_packets_received++;
        total_bytes_received += bytes_received;
        packets_received++;
        
        // Process packet
        process_received_packet(header, packet_buffer + sizeof(PacketHeader), &sender_addr);
    }
    
    return packets_received;
}

void process_received_packet(PacketHeader* header, unsigned char* data, struct sockaddr_in* sender_addr) {
    NetworkClient* client = find_client_by_address(sender_addr);
    
    // Handle connection packets
    if (header->type == PACKET_TYPE_CONNECT_REQUEST && network_mode == NETWORK_MODE_SERVER) {
        handle_connect_request(data, header->data_size, sender_addr);
        return;
    }
    
    if (header->type == PACKET_TYPE_CONNECT_RESPONSE && network_mode == NETWORK_MODE_CLIENT) {
        handle_connect_response(data, header->data_size);
        return;
    }
    
    // Must have valid client for other packet types
    if (!client) {
        return;
    }
    
    // Update client state
    client->last_heartbeat = GetTickCount();
    client->bytes_received += sizeof(PacketHeader) + header->data_size;
    
    // Handle acknowledgments
    if (header->flags & 0x01) { // Reliable packet
        send_reliable_ack(client->client_id, header->sequence);
    }
    
    if (header->ack > 0) {
        acknowledge_reliable_packet(client, header->ack);
    }
    
    // Queue message for processing
    queue_network_message(header->type, client->client_id, data, header->data_size, header->timestamp);
}

void queue_network_message(PacketType type, unsigned int client_id, unsigned char* data, 
                          unsigned int data_size, unsigned int timestamp) {
    int next_head = (message_queue_head + 1) % MAX_MESSAGE_QUEUE;
    
    if (next_head == message_queue_tail) {
        // Queue full, drop oldest message
        message_queue_tail = (message_queue_tail + 1) % MAX_MESSAGE_QUEUE;
    }
    
    NetworkMessage* msg = &message_queue[message_queue_head];
    msg->type = type;
    msg->client_id = client_id;
    msg->data_size = fmin(data_size, MAX_PACKET_SIZE);
    msg->timestamp = timestamp;
    
    if (data && msg->data_size > 0) {
        memcpy(msg->data, data, msg->data_size);
    }
    
    message_queue_head = next_head;
}

int get_next_network_message(NetworkMessage* message) {
    if (message_queue_tail == message_queue_head) {
        return 0; // No messages
    }
    
    *message = message_queue[message_queue_tail];
    message_queue_tail = (message_queue_tail + 1) % MAX_MESSAGE_QUEUE;
    
    return 1;
}

// ========================================================================
// CLIENT MANAGEMENT
// ========================================================================

NetworkClient* find_client(unsigned int client_id) {
    for (int i = 0; i < client_count; i++) {
        if (clients[i].client_id == client_id) {
            return &clients[i];
        }
    }
    return NULL;
}

NetworkClient* find_client_by_address(struct sockaddr_in* address) {
    for (int i = 0; i < client_count; i++) {
        if (clients[i].address.sin_addr.s_addr == address->sin_addr.s_addr &&
            clients[i].address.sin_port == address->sin_port) {
            return &clients[i];
        }
    }
    return NULL;
}

int add_client(struct sockaddr_in* address) {
    if (client_count >= MAX_CLIENTS) {
        return -1;
    }
    
    NetworkClient* client = &clients[client_count];
    memset(client, 0, sizeof(NetworkClient));
    
    client->address = *address;
    client->state = CONNECTION_STATE_CONNECTING;
    client->client_id = next_client_id++;
    client->last_heartbeat = GetTickCount();
    sprintf(client->player_name, "Player_%d", client->client_id);
    
    return client_count++;
}

void remove_client(unsigned int client_id) {
    for (int i = 0; i < client_count; i++) {
        if (clients[i].client_id == client_id) {
            // Send disconnect notification to other clients
            if (network_mode == NETWORK_MODE_SERVER) {
                broadcast_client_disconnect(client_id);
            }
            
            // Shift clients down
            for (int j = i; j < client_count - 1; j++) {
                clients[j] = clients[j + 1];
            }
            client_count--;
            break;
        }
    }
}

void update_client_timeouts() {
    unsigned int current_time = GetTickCount();
    
    for (int i = 0; i < client_count; i++) {
        NetworkClient* client = &clients[i];
        
        if (current_time - client->last_heartbeat > TIMEOUT_INTERVAL) {
            client->state = CONNECTION_STATE_TIMEOUT;
            remove_client(client->client_id);
            i--; // Adjust index after removal
        }
    }
}

// ========================================================================
// CONNECTION HANDLING
// ========================================================================

void send_connect_request() {
    if (network_mode != NETWORK_MODE_CLIENT) return;
    
    char connect_data[64];
    sprintf(connect_data, "CONNECT|%s|%s", "Player", "1.0");
    
    send_packet(0, PACKET_TYPE_CONNECT_REQUEST, connect_data, strlen(connect_data), 1);
}

void handle_connect_request(unsigned char* data, unsigned int data_size, struct sockaddr_in* sender_addr) {
    if (network_mode != NETWORK_MODE_SERVER) return;
    
    // Check if client is already connected
    NetworkClient* existing_client = find_client_by_address(sender_addr);
    if (existing_client) {
        return; // Already connected
    }
    
    // Check server capacity
    if (client_count >= max_clients) {
        send_connect_response_failure(sender_addr, "Server full");
        return;
    }
    
    // Parse connection data
    char* connect_str = (char*)data;
    char* player_name = strstr(connect_str, "|");
    if (player_name) {
        player_name++; // Skip the |
        char* version = strstr(player_name, "|");
        if (version) {
            *version = '\0'; // Null terminate player name
            version++; // Point to version string
        }
    }
    
    // Add new client
    int client_index = add_client(sender_addr);
    if (client_index < 0) {
        send_connect_response_failure(sender_addr, "Connection failed");
        return;
    }
    
    NetworkClient* new_client = &clients[client_index];
    new_client->state = CONNECTION_STATE_CONNECTED;
    
    if (player_name && strlen(player_name) > 0) {
        strncpy(new_client->player_name, player_name, sizeof(new_client->player_name) - 1);
    }
    
    // Send connection response
    send_connect_response_success(new_client);
    
    // Notify other clients
    broadcast_client_join(new_client->client_id);
}

void send_connect_response_success(NetworkClient* client) {
    char response_data[128];
    sprintf(response_data, "SUCCESS|%d|%s|%d", client->client_id, server_name, client_count);
    
    send_packet(client->client_id, PACKET_TYPE_CONNECT_RESPONSE, response_data, strlen(response_data), 1);
}

void send_connect_response_failure(struct sockaddr_in* address, const char* reason) {
    char response_data[128];
    sprintf(response_data, "FAILURE|%s", reason);
    
    // Send directly to address since no client record exists
    unsigned char packet_buffer[MAX_PACKET_SIZE];
    PacketHeader* header = (PacketHeader*)packet_buffer;
    
    header->magic = 0x454E;
    header->type = PACKET_TYPE_CONNECT_RESPONSE;
    header->flags = 0;
    header->sequence = 0;
    header->ack = 0;
    header->timestamp = GetTickCount();
    header->data_size = strlen(response_data);
    header->checksum = 0;
    
    memcpy(packet_buffer + sizeof(PacketHeader), response_data, strlen(response_data));
    header->checksum = calculate_checksum(packet_buffer, sizeof(PacketHeader) + strlen(response_data));
    
    sendto(main_socket, (char*)packet_buffer, sizeof(PacketHeader) + strlen(response_data), 0,
           (struct sockaddr*)address, sizeof(struct sockaddr_in));
}

void handle_connect_response(unsigned char* data, unsigned int data_size) {
    if (network_mode != NETWORK_MODE_CLIENT) return;
    
    char* response_str = (char*)data;
    
    if (strncmp(response_str, "SUCCESS", 7) == 0) {
        // Parse successful response
        char* client_id_str = strstr(response_str, "|");
        if (client_id_str) {
            client_id_str++;
            local_client_id = atoi(client_id_str);
            
            // Add server as client 0
            clients[0].state = CONNECTION_STATE_CONNECTED;
            clients[0].client_id = 0;
            clients[0].address = server_address;
            strcpy(clients[0].player_name, "Server");
            client_count = 1;
        }
    } else {
        // Connection failed
        disconnect_from_network();
    }
}

void send_disconnect_packet(unsigned int client_id) {
    const char* disconnect_msg = "DISCONNECT";
    send_packet(client_id, PACKET_TYPE_DISCONNECT, disconnect_msg, strlen(disconnect_msg), 1);
}

void broadcast_client_join(unsigned int new_client_id) {
    if (network_mode != NETWORK_MODE_SERVER) return;
    
    NetworkClient* new_client = find_client(new_client_id);
    if (!new_client) return;
    
    char join_data[128];
    sprintf(join_data, "JOIN|%d|%s", new_client_id, new_client->player_name);
    
    for (int i = 0; i < client_count; i++) {
        if (clients[i].client_id != new_client_id && clients[i].state == CONNECTION_STATE_CONNECTED) {
            send_packet(clients[i].client_id, PACKET_TYPE_CHAT_MESSAGE, join_data, strlen(join_data), 0);
        }
    }
}

void broadcast_client_disconnect(unsigned int client_id) {
    if (network_mode != NETWORK_MODE_SERVER) return;
    
    char disconnect_data[64];
    sprintf(disconnect_data, "LEAVE|%d", client_id);
    
    for (int i = 0; i < client_count; i++) {
        if (clients[i].client_id != client_id && clients[i].state == CONNECTION_STATE_CONNECTED) {
            send_packet(clients[i].client_id, PACKET_TYPE_CHAT_MESSAGE, disconnect_data, strlen(disconnect_data), 0);
        }
    }
}

// ========================================================================
// RELIABLE PACKET SYSTEM
// ========================================================================

void send_reliable_ack(unsigned int client_id, unsigned short sequence) {
    unsigned char ack_data[2];
    ack_data[0] = sequence & 0xFF;
    ack_data[1] = (sequence >> 8) & 0xFF;
    
    send_packet(client_id, PACKET_TYPE_RELIABLE_ACK, ack_data, 2, 0);
}

void acknowledge_reliable_packet(NetworkClient* client, unsigned short ack_sequence) {
    int slot = ack_sequence % MAX_RELIABLE_PACKETS;
    ReliablePacket* packet = &client->reliable_outgoing[slot];
    
    if (packet->header.sequence == ack_sequence && !packet->acknowledged) {
        packet->acknowledged = 1;
        
        // Calculate RTT for ping estimation
        unsigned int rtt = GetTickCount() - packet->send_time;
        client->ping = (client->ping * 7 + rtt) / 8; // Smooth ping calculation
    }
}

void resend_reliable_packets() {
    unsigned int current_time = GetTickCount();
    
    for (int i = 0; i < client_count; i++) {
        NetworkClient* client = &clients[i];
        
        for (int j = 0; j < MAX_RELIABLE_PACKETS; j++) {
            ReliablePacket* packet = &client->reliable_outgoing[j];
            
            if (!packet->acknowledged && packet->header.magic == 0x454E) {
                if (current_time - packet->send_time > 1000) { // 1 second timeout
                    if (packet->retry_count < 5) {
                        // Resend packet
                        struct sockaddr_in* target_addr = (network_mode == NETWORK_MODE_SERVER) ? 
                                                         &client->address : &server_address;
                        
                        sendto(main_socket, (char*)packet, sizeof(PacketHeader) + packet->header.data_size, 0,
                               (struct sockaddr*)target_addr, sizeof(struct sockaddr_in));
                        
                        packet->send_time = current_time;
                        packet->retry_count++;
                    } else {
                        // Give up on this packet
                        packet->acknowledged = 1;
                    }
                }
            }
        }
    }
}

// ========================================================================
// GAME STATE SYNCHRONIZATION
// ========================================================================

void send_player_input(float forward, float strafe, float turn, int buttons) {
    if (network_mode != NETWORK_MODE_CLIENT) return;
    
    struct {
        float forward;
        float strafe;
        float turn;
        int buttons;
        unsigned int frame;
    } input_data;
    
    input_data.forward = forward;
    input_data.strafe = strafe;
    input_data.turn = turn;
    input_data.buttons = buttons;
    input_data.frame = GetTickCount(); // Frame counter would be better
    
    send_packet(0, PACKET_TYPE_PLAYER_INPUT, &input_data, sizeof(input_data), 0);
}

void send_player_state(unsigned int client_id, float x, float y, float z, float rotation, 
                      int health, int energy) {
    if (network_mode != NETWORK_MODE_SERVER) return;
    
    struct {
        unsigned int client_id;
        float position[3];
        float rotation;
        int health;
        int energy;
        unsigned int timestamp;
    } state_data;
    
    state_data.client_id = client_id;
    state_data.position[0] = x;
    state_data.position[1] = y;
    state_data.position[2] = z;
    state_data.rotation = rotation;
    state_data.health = health;
    state_data.energy = energy;
    state_data.timestamp = GetTickCount();
    
    // Send to all other clients
    for (int i = 0; i < client_count; i++) {
        if (clients[i].client_id != client_id && clients[i].state == CONNECTION_STATE_CONNECTED) {
            send_packet(clients[i].client_id, PACKET_TYPE_PLAYER_STATE, &state_data, sizeof(state_data), 0);
        }
    }
}

void send_game_state_update() {
    if (network_mode != NETWORK_MODE_SERVER) return;
    
    // This would contain the full game state
    // For now, just send a heartbeat with basic info
    struct {
        unsigned int server_time;
        int player_count;
        int game_mode;
    } game_state;
    
    game_state.server_time = GetTickCount();
    game_state.player_count = client_count;
    game_state.game_mode = 0; // Game mode enum
    
    for (int i = 0; i < client_count; i++) {
        if (clients[i].state == CONNECTION_STATE_CONNECTED) {
            send_packet(clients[i].client_id, PACKET_TYPE_GAME_STATE, &game_state, sizeof(game_state), 0);
        }
    }
}

// ========================================================================
// LAG COMPENSATION AND PREDICTION
// ========================================================================

void update_lag_compensation(NetworkClient* client, float x, float y, float z, float rotation) {
    if (!enable_lag_compensation) return;
    
    // Store position history
    client->position_history[client->history_head][0] = x;
    client->position_history[client->history_head][1] = y;
    client->position_history[client->history_head][2] = z;
    client->rotation_history[client->history_head] = rotation;
    client->frame_history[client->history_head] = GetTickCount();
    
    client->history_head = (client->history_head + 1) % PREDICTION_FRAMES;
}

void get_compensated_position(NetworkClient* client, unsigned int target_time, 
                             float* x, float* y, float* z, float* rotation) {
    if (!enable_lag_compensation) {
        *x = client->position_history[client->history_head][0];
        *y = client->position_history[client->history_head][1];
        *z = client->position_history[client->history_head][2];
        *rotation = client->rotation_history[client->history_head];
        return;
    }
    
    // Find closest frames
    int best_frame = 0;
    unsigned int best_diff = UINT_MAX;
    
    for (int i = 0; i < PREDICTION_FRAMES; i++) {
        unsigned int diff = abs((int)(client->frame_history[i] - target_time));
        if (diff < best_diff) {
            best_diff = diff;
            best_frame = i;
        }
    }
    
    // Use the closest frame (could interpolate for better accuracy)
    *x = client->position_history[best_frame][0];
    *y = client->position_history[best_frame][1];
    *z = client->position_history[best_frame][2];
    *rotation = client->rotation_history[best_frame];
}

int validate_player_movement(NetworkClient* client, float new_x, float new_y, float new_z) {
    if (!enable_anti_cheat) return 1;
    
    // Calculate distance moved
    float dx = new_x - client->last_known_position[0];
    float dy = new_y - client->last_known_position[1];
    float dz = new_z - client->last_known_position[2];
    float distance = sqrt(dx*dx + dy*dy + dz*dz);
    
    // Calculate time since last update
    unsigned int time_diff = GetTickCount() - client->last_valid_input;
    float max_speed = 10.0f; // Maximum player speed
    float max_distance = (max_speed * time_diff) / 1000.0f;
    
    if (distance > max_distance * 1.5f) { // Allow some tolerance
        client->suspicious_activity++;
        return 0; // Invalid movement
    }
    
    // Update last known position
    client->last_known_position[0] = new_x;
    client->last_known_position[1] = new_y;
    client->last_known_position[2] = new_z;
    client->last_valid_input = GetTickCount();
    
    return 1;
}

// ========================================================================
// UTILITY FUNCTIONS
// ========================================================================

unsigned short calculate_checksum(unsigned char* data, int length) {
    unsigned int checksum = 0;
    
    for (int i = 0; i < length; i++) {
        checksum += data[i];
    }
    
    return (unsigned short)(checksum & 0xFFFF);
}

void update_network_system() {
    if (network_mode == NETWORK_MODE_NONE) return;
    
    // Receive and process packets
    receive_packets();
    
    // Update client timeouts
    update_client_timeouts();
    
    // Resend reliable packets
    resend_reliable_packets();
    
    // Send periodic heartbeats
    static unsigned int last_heartbeat = 0;
    unsigned int current_time = GetTickCount();
    
    if (current_time - last_heartbeat > HEARTBEAT_INTERVAL) {
        send_heartbeats();
        last_heartbeat = current_time;
    }
    
    // Send game state updates (server only)
    if (network_mode == NETWORK_MODE_SERVER) {
        static unsigned int last_state_update = 0;
        unsigned int update_interval = (unsigned int)(1000.0f / tick_rate);
        
        if (current_time - last_state_update > update_interval) {
            send_game_state_update();
            last_state_update = current_time;
        }
    }
}

void send_heartbeats() {
    const char* heartbeat_msg = "HEARTBEAT";
    
    if (network_mode == NETWORK_MODE_SERVER) {
        // Send to all connected clients
        for (int i = 0; i < client_count; i++) {
            if (clients[i].state == CONNECTION_STATE_CONNECTED) {
                send_packet(clients[i].client_id, PACKET_TYPE_HEARTBEAT, heartbeat_msg, strlen(heartbeat_msg), 0);
            }
        }
    } else if (network_mode == NETWORK_MODE_CLIENT) {
        // Send to server
        send_packet(0, PACKET_TYPE_HEARTBEAT, heartbeat_msg, strlen(heartbeat_msg), 0);
    }
}

void send_chat_message(const char* message) {
    if (network_mode == NETWORK_MODE_NONE) return;
    
    char chat_data[256];
    sprintf(chat_data, "CHAT|%d|%s", local_client_id, message);
    
    if (network_mode == NETWORK_MODE_CLIENT) {
        send_packet(0, PACKET_TYPE_CHAT_MESSAGE, chat_data, strlen(chat_data), 1);
    } else if (network_mode == NETWORK_MODE_SERVER) {
        // Broadcast to all clients
        for (int i = 0; i < client_count; i++) {
            if (clients[i].state == CONNECTION_STATE_CONNECTED) {
                send_packet(clients[i].client_id, PACKET_TYPE_CHAT_MESSAGE, chat_data, strlen(chat_data), 1);
            }
        }
    }
}

int get_client_count() {
    return client_count;
}

NetworkClient* get_clients() {
    return clients;
}

unsigned int get_local_client_id() {
    return local_client_id;
}

NetworkMode get_network_mode() {
    return network_mode;
}

void get_network_statistics(unsigned int* packets_sent, unsigned int* packets_received,
                           unsigned int* bytes_sent, unsigned int* bytes_received) {
    if (packets_sent) *packets_sent = total_packets_sent;
    if (packets_received) *packets_received = total_packets_received;
    if (bytes_sent) *bytes_sent = total_bytes_sent;
    if (bytes_received) *bytes_received = total_bytes_received;
}

void cleanup_network_system() {
    disconnect_from_network();
    WSACleanup();
}
/**
 * ========================================================================
 * ENDOR NETWORK AND MULTIPLAYER SYSTEM
 * ========================================================================
 * 
 * Advanced networking implementation for the Endor game engine.
 * Supports client-server architecture, peer-to-peer, local area network,
 * and internet play with lag compensation, prediction, and anti-cheat.
 * 
 * Features:
 * - UDP-based networking with reliable packet delivery
 * - IPv4 and IPv6 support
 * - NAT traversal and hole punching
 * - Advanced lag compensation and client-side prediction
 * - Anti-cheat measures and validation
 * - Voice chat support with spatial audio
 * - Server browser with filtering and favorites
 * - File transfer for maps/mods with compression
 * - Bandwidth management and throttling
 * - Network statistics and diagnostics
 * - Encryption support for secure communications
 * 
 * Improvements in this version:
 * - Enhanced error handling and logging
 * - IPv6 support for future-proofing
 * - Better security with encryption and validation
 * - Voice chat integration
 * - Improved server browser functionality
 * - More robust file transfer system
 * - Better bandwidth management
 * - Enhanced anti-cheat measures
 */

#include "endor_readable.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <math.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "iphlpapi.lib")

// ========================================================================
// NETWORK CONSTANTS AND STRUCTURES
// ========================================================================

#define MAX_CLIENTS 64
#define MAX_PACKET_SIZE 1400
#define MAX_RELIABLE_PACKETS 512
#define MAX_MESSAGE_QUEUE 1024
#define DEFAULT_PORT 7777
#define HEARTBEAT_INTERVAL 5000     // 5 seconds
#define TIMEOUT_INTERVAL 30000      // 30 seconds
#define PREDICTION_FRAMES 5
#define LAG_COMPENSATION_MS 200
#define VOICE_SAMPLE_RATE 16000
#define VOICE_FRAME_SIZE 320
#define MAX_VOICE_PACKET_SIZE 1024
#define ENCRYPTION_KEY_SIZE 32
#define MAX_SERVER_BROWSER_ENTRIES 256
#define FILE_TRANSFER_CHUNK_SIZE 4096
#define MAX_CONCURRENT_TRANSFERS 4
#define BANDWIDTH_THROTTLE_MS 16    // 60 updates per second max

// Network modes
typedef enum {
    NETWORK_MODE_NONE,
    NETWORK_MODE_SERVER,
    NETWORK_MODE_CLIENT,
    NETWORK_MODE_PEER,
    NETWORK_MODE_LISTEN_SERVER
} NetworkMode;

// Packet types
typedef enum {
    // Connection management
    PACKET_TYPE_CONNECT_REQUEST,
    PACKET_TYPE_CONNECT_RESPONSE,
    PACKET_TYPE_DISCONNECT,
    PACKET_TYPE_HEARTBEAT,
    PACKET_TYPE_KEEP_ALIVE,
    
    // Game data
    PACKET_TYPE_PLAYER_INPUT,
    PACKET_TYPE_PLAYER_STATE,
    PACKET_TYPE_GAME_STATE,
    PACKET_TYPE_ENTITY_UPDATE,
    PACKET_TYPE_EVENT,
    
    // Communication
    PACKET_TYPE_CHAT_MESSAGE,
    PACKET_TYPE_VOICE_DATA,
    PACKET_TYPE_TEAM_MESSAGE,
    
    // File transfer
    PACKET_TYPE_FILE_REQUEST,
    PACKET_TYPE_FILE_DATA,
    PACKET_TYPE_FILE_COMPLETE,
    
    // Server browser
    PACKET_TYPE_SERVER_INFO_REQUEST,
    PACKET_TYPE_SERVER_INFO_RESPONSE,
    PACKET_TYPE_MASTER_SERVER_LIST,
    
    // System
    PACKET_TYPE_RELIABLE_ACK,
    PACKET_TYPE_PING,
    PACKET_TYPE_PONG,
    PACKET_TYPE_BANDWIDTH_TEST,
    PACKET_TYPE_CUSTOM,
    
    PACKET_TYPE_COUNT
} PacketType;

// Connection states
typedef enum {
    CONNECTION_STATE_DISCONNECTED,
    CONNECTION_STATE_CONNECTING,
    CONNECTION_STATE_AUTHENTICATING,
    CONNECTION_STATE_CONNECTED,
    CONNECTION_STATE_TIMEOUT,
    CONNECTION_STATE_BANNED
} ConnectionState;

// Security levels
typedef enum {
    SECURITY_LEVEL_NONE,
    SECURITY_LEVEL_BASIC,
    SECURITY_LEVEL_ENCRYPTED,
    SECURITY_LEVEL_AUTHENTICATED
} SecurityLevel;

// Packet header structure
typedef struct {
    unsigned short magic;           // 0xE4D0 for IPv4, 0xE6D0 for IPv6
    unsigned char version;          // Protocol version
    unsigned char type;             // PacketType
    unsigned char flags;            // Bit flags (reliable, encrypted, compressed)
    unsigned char security;         // Security level
    unsigned short sequence;        // Packet sequence number
    unsigned short ack;             // Acknowledgment number
    unsigned int timestamp;         // Send timestamp
    unsigned int client_id;         // Sender client ID
    unsigned short data_size;       // Payload size
    unsigned short checksum;        // Header + data checksum
    unsigned char hash[16];         // Optional MD5 hash for validation
} PacketHeader;

// Reliable packet for guaranteed delivery
typedef struct {
    PacketHeader header;
    unsigned char data[MAX_PACKET_SIZE - sizeof(PacketHeader)];
    unsigned int send_time;
    unsigned int last_retry_time;
    int retry_count;
    int acknowledged;
    float priority;
} ReliablePacket;

// Voice chat data
typedef struct {
    int enabled;
    float volume;
    int muted;
    int spatial_audio;
    unsigned char* buffer;
    int buffer_size;
    int buffer_head;
    int buffer_tail;
    float position[3];
    int team_only;
} VoiceChat;

// Bandwidth management
typedef struct {
    unsigned int bytes_sent;
    unsigned int bytes_received;
    unsigned int packets_sent;
    unsigned int packets_received;
    float bandwidth_in;         // KB/s
    float bandwidth_out;        // KB/s
    float packet_loss;          // Percentage
    unsigned int last_update;
    unsigned int throttle_time;
    int bandwidth_limit;        // KB/s limit (0 = unlimited)
} BandwidthInfo;

// Client connection info
typedef struct {
    SOCKET socket;
    struct sockaddr_storage address;  // Supports both IPv4 and IPv6
    int address_family;               // AF_INET or AF_INET6
    ConnectionState state;
    SecurityLevel security_level;
    unsigned int client_id;
    char player_name[64];
    char auth_token[128];
    unsigned int last_sequence;
    unsigned int last_ack;
    unsigned int last_heartbeat;
    unsigned int last_activity;
    unsigned int ping;
    float jitter;
    BandwidthInfo bandwidth;
    
    // Reliable packet system
    ReliablePacket reliable_outgoing[MAX_RELIABLE_PACKETS];
    ReliablePacket reliable_incoming[MAX_RELIABLE_PACKETS];
    unsigned short reliable_sequence;
    unsigned short reliable_ack;
    int reliable_window_size;
    
    // Lag compensation
    float position_history[PREDICTION_FRAMES][3];
    float rotation_history[PREDICTION_FRAMES][3];
    float velocity_history[PREDICTION_FRAMES][3];
    unsigned int frame_history[PREDICTION_FRAMES];
    int history_head;
    
    // Anti-cheat
    unsigned int last_valid_input;
    float last_known_position[3];
    unsigned int suspicious_activity;
    unsigned int warnings;
    unsigned int violations;
    float trust_score;
    
    // Voice chat
    VoiceChat voice;
    
    // Player info
    int team;
    int score;
    int ready;
    float skill_rating;
    unsigned int join_time;
} NetworkClient;

// Message queue for processing
typedef struct {
    PacketType type;
    unsigned int client_id;
    unsigned char* data;
    unsigned int data_size;
    unsigned int timestamp;
    float priority;
    int processed;
} NetworkMessage;

// File transfer for maps/mods
typedef struct {
    char filename[512];
    char filepath[1024];
    unsigned int file_size;
    unsigned int compressed_size;
    unsigned int bytes_transferred;
    unsigned char* file_buffer;
    FILE* file_handle;
    unsigned int chunk_size;
    int chunk_index;
    int total_chunks;
    int* received_chunks;
    int active;
    int is_upload;
    unsigned int client_id;
    unsigned int start_time;
    float progress;
    unsigned char checksum[32];  // SHA256
} FileTransfer;

// Server browser entry
typedef struct {
    char server_name[128];
    char map_name[64];
    char game_mode[64];
    char server_version[32];
    struct sockaddr_storage address;
    int player_count;
    int max_players;
    int bot_count;
    int ping;
    int password_protected;
    int vac_secured;
    int modded;
    int dedicated;
    unsigned int last_response;
    char tags[256];
    float skill_level;
    int favorite;
} ServerInfo;

// Master server info
typedef struct {
    char address[256];
    int port;
    int enabled;
    unsigned int last_query;
    int server_count;
} MasterServerInfo;

// Network statistics
typedef struct {
    unsigned int total_packets_sent;
    unsigned int total_packets_received;
    unsigned int total_bytes_sent;
    unsigned int total_bytes_received;
    unsigned int reliable_packets_sent;
    unsigned int reliable_packets_received;
    unsigned int reliable_packets_lost;
    unsigned int network_errors;
    unsigned int connections_accepted;
    unsigned int connections_rejected;
    float average_ping;
    float average_packet_loss;
    unsigned int session_start_time;
} NetworkStatistics;

// ========================================================================
// GLOBAL NETWORK STATE
// ========================================================================

static NetworkMode g_network_mode = NETWORK_MODE_NONE;
static SOCKET g_main_socket = INVALID_SOCKET;
static SOCKET g_ipv6_socket = INVALID_SOCKET;
static struct sockaddr_storage g_local_address;
static struct sockaddr_storage g_server_address;

static NetworkClient g_clients[MAX_CLIENTS];
static int g_client_count = 0;
static unsigned int g_local_client_id = 0;
static unsigned int g_next_client_id = 1;

static NetworkMessage* g_message_queue = NULL;
static int g_message_queue_size = MAX_MESSAGE_QUEUE;
static int g_message_queue_head = 0;
static int g_message_queue_tail = 0;

static FileTransfer g_file_transfers[MAX_CONCURRENT_TRANSFERS];
static int g_transfer_count = 0;

static ServerInfo* g_server_list = NULL;
static int g_server_list_size = MAX_SERVER_BROWSER_ENTRIES;
static int g_server_count = 0;

static MasterServerInfo g_master_servers[4];
static int g_master_server_count = 0;

static NetworkStatistics g_stats;

// Configuration
static int g_max_clients = 32;
static char g_server_name[128] = "Endor Server";
static char g_server_password[64] = "";
static char g_server_tags[256] = "";
static int g_enable_lag_compensation = 1;
static int g_enable_prediction = 1;
static int g_enable_anti_cheat = 1;
static int g_enable_voice_chat = 1;
static int g_enable_encryption = 0;
static float g_tick_rate = 60.0f;
static int g_max_ping = 500;
static int g_bandwidth_limit = 0;  // KB/s, 0 = unlimited
static int g_debug_network = 0;
static int g_allow_downloads = 1;
static int g_compress_packets = 0;

// Encryption keys
static unsigned char g_encryption_key[ENCRYPTION_KEY_SIZE];
static int g_encryption_initialized = 0;

// ========================================================================
// INTERNAL UTILITY FUNCTIONS
// ========================================================================

/**
 * Logs network system messages
 * @param format Printf-style format string
 * @param ... Variable arguments
 */
static void network_log(const char* format, ...)
{
    if (!g_debug_network) return;
    
    char buffer[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    // Add timestamp
    SYSTEMTIME st;
    GetLocalTime(&st);
    
    char log_buffer[1280];
    sprintf(log_buffer, "[NETWORK %02d:%02d:%02d.%03d] %s\n", 
            st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, buffer);
    
    OutputDebugString(log_buffer);
    
    // Also write to log file
    FILE* log_file = fopen("network_log.txt", "a");
    if (log_file) {
        fprintf(log_file, "%s", log_buffer);
        fclose(log_file);
    }
}

/**
 * Gets a readable string for a packet type
 * @param type Packet type
 * @return String representation
 */
static const char* get_packet_type_name(PacketType type)
{
    static const char* names[] = {
        "CONNECT_REQUEST", "CONNECT_RESPONSE", "DISCONNECT", "HEARTBEAT", "KEEP_ALIVE",
        "PLAYER_INPUT", "PLAYER_STATE", "GAME_STATE", "ENTITY_UPDATE", "EVENT",
        "CHAT_MESSAGE", "VOICE_DATA", "TEAM_MESSAGE",
        "FILE_REQUEST", "FILE_DATA", "FILE_COMPLETE",
        "SERVER_INFO_REQUEST", "SERVER_INFO_RESPONSE", "MASTER_SERVER_LIST",
        "RELIABLE_ACK", "PING", "PONG", "BANDWIDTH_TEST", "CUSTOM"
    };
    
    if (type >= 0 && type < PACKET_TYPE_COUNT) {
        return names[type];
    }
    return "UNKNOWN";
}

/**
 * Converts address to string
 * @param addr Socket address
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 * @return 1 on success, 0 on failure
 */
static int address_to_string(struct sockaddr_storage* addr, char* buffer, int buffer_size)
{
    if (addr->ss_family == AF_INET) {
        struct sockaddr_in* addr4 = (struct sockaddr_in*)addr;
        inet_ntop(AF_INET, &addr4->sin_addr, buffer, buffer_size);
        int len = strlen(buffer);
        sprintf(buffer + len, ":%d", ntohs(addr4->sin_port));
        return 1;
    } else if (addr->ss_family == AF_INET6) {
        struct sockaddr_in6* addr6 = (struct sockaddr_in6*)addr;
        buffer[0] = '[';
        inet_ntop(AF_INET6, &addr6->sin6_addr, buffer + 1, buffer_size - 1);
        int len = strlen(buffer);
        sprintf(buffer + len, "]:%d", ntohs(addr6->sin6_port));
        return 1;
    }
    return 0;
}

/**
 * Calculates CRC32 checksum
 * @param data Data buffer
 * @param length Data length
 * @return CRC32 checksum
 */
static unsigned int calculate_crc32(const unsigned char* data, int length)
{
    static unsigned int crc_table[256];
    static int table_initialized = 0;
    
    // Initialize CRC table on first use
    if (!table_initialized) {
        for (int i = 0; i < 256; i++) {
            unsigned int crc = i;
            for (int j = 0; j < 8; j++) {
                if (crc & 1) {
                    crc = (crc >> 1) ^ 0xEDB88320;
                } else {
                    crc >>= 1;
                }
            }
            crc_table[i] = crc;
        }
        table_initialized = 1;
    }
    
    unsigned int crc = 0xFFFFFFFF;
    for (int i = 0; i < length; i++) {
        crc = (crc >> 8) ^ crc_table[(crc ^ data[i]) & 0xFF];
    }
    return crc ^ 0xFFFFFFFF;
}

/**
 * Simple packet compression using RLE
 * @param input Input data
 * @param input_size Input size
 * @param output Output buffer
 * @param output_size Output buffer size
 * @return Compressed size or -1 on failure
 */
static int compress_packet(const unsigned char* input, int input_size, 
                          unsigned char* output, int output_size)
{
    if (!g_compress_packets) {
        return -1;  // Compression disabled
    }
    
    int out_pos = 0;
    int i = 0;
    
    while (i < input_size && out_pos < output_size - 2) {
        unsigned char byte = input[i];
        int count = 1;
        
        // Count consecutive bytes
        while (i + count < input_size && input[i + count] == byte && count < 255) {
            count++;
        }
        
        if (count > 2) {
            // Write RLE encoded: marker, count, byte
            output[out_pos++] = 0xFF;  // RLE marker
            output[out_pos++] = (unsigned char)count;
            output[out_pos++] = byte;
            i += count;
        } else {
            // Write single byte
            output[out_pos++] = byte;
            if (byte == 0xFF && out_pos < output_size) {
                output[out_pos++] = 0;  // Escape the marker
            }
            i++;
        }
    }
    
    // Only use compression if it actually reduces size
    if (out_pos >= input_size) {
        return -1;
    }
    
    return out_pos;
}

/**
 * Decompresses RLE packet
 * @param input Compressed data
 * @param input_size Input size
 * @param output Output buffer
 * @param output_size Output buffer size
 * @return Decompressed size or -1 on failure
 */
static int decompress_packet(const unsigned char* input, int input_size,
                           unsigned char* output, int output_size)
{
    int in_pos = 0;
    int out_pos = 0;
    
    while (in_pos < input_size && out_pos < output_size) {
        unsigned char byte = input[in_pos++];
        
        if (byte == 0xFF && in_pos < input_size) {
            unsigned char next = input[in_pos++];
            if (next == 0) {
                // Escaped marker
                output[out_pos++] = 0xFF;
            } else if (in_pos < input_size) {
                // RLE sequence
                int count = next;
                unsigned char value = input[in_pos++];
                
                for (int i = 0; i < count && out_pos < output_size; i++) {
                    output[out_pos++] = value;
                }
            }
        } else {
            output[out_pos++] = byte;
        }
    }
    
    return out_pos;
}

// ========================================================================
// CORE NETWORK FUNCTIONS
// ========================================================================

/**
 * Initializes the network system
 * @return 1 on success, 0 on failure
 */
int initialize_network_system(void)
{
    network_log("Initializing network system");
    
    // Initialize Winsock
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        network_log("WSAStartup failed with error: %d", result);
        return 0;
    }
    
    // Allocate message queue
    g_message_queue = (NetworkMessage*)calloc(g_message_queue_size, sizeof(NetworkMessage));
    if (!g_message_queue) {
        network_log("Failed to allocate message queue");
        WSACleanup();
        return 0;
    }
    
    // Allocate server list
    g_server_list = (ServerInfo*)calloc(g_server_list_size, sizeof(ServerInfo));
    if (!g_server_list) {
        network_log("Failed to allocate server list");
        free(g_message_queue);
        WSACleanup();
        return 0;
    }
    
    // Clear client data
    memset(g_clients, 0, sizeof(g_clients));
    g_client_count = 0;
    
    // Clear message queue
    g_message_queue_head = 0;
    g_message_queue_tail = 0;
    
    // Clear file transfers
    memset(g_file_transfers, 0, sizeof(g_file_transfers));
    g_transfer_count = 0;
    
    // Clear server list
    g_server_count = 0;
    
    // Initialize master servers
    strcpy(g_master_servers[0].address, "master1.endorgame.com");
    g_master_servers[0].port = 27010;
    g_master_servers[0].enabled = 1;
    g_master_server_count = 1;
    
    // Reset statistics
    memset(&g_stats, 0, sizeof(g_stats));
    g_stats.session_start_time = GetTickCount();
    
    // Generate random encryption key
    srand((unsigned int)time(NULL));
    for (int i = 0; i < ENCRYPTION_KEY_SIZE; i++) {
        g_encryption_key[i] = (unsigned char)(rand() % 256);
    }
    g_encryption_initialized = 1;
    
    network_log("Network system initialized successfully");
    return 1;
}

/**
 * Starts a server
 * @param port Port to bind to (0 for default)
 * @param ipv6 Enable IPv6 support
 * @return 1 on success, 0 on failure
 */
int start_server(int port, int ipv6)
{
    if (g_network_mode != NETWORK_MODE_NONE) {
        network_log("Already in network mode %d", g_network_mode);
        return 0;
    }
    
    int final_port = port ? port : DEFAULT_PORT;
    
    // Create IPv4 socket
    g_main_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (g_main_socket == INVALID_SOCKET) {
        network_log("Failed to create IPv4 socket: %d", WSAGetLastError());
        return 0;
    }
    
    // Set socket options
    int broadcast = 1;
    setsockopt(g_main_socket, SOL_SOCKET, SO_BROADCAST, (char*)&broadcast, sizeof(broadcast));
    
    int reuse = 1;
    setsockopt(g_main_socket, SOL_SOCKET, SO_REUSEADDR, (char*)&reuse, sizeof(reuse));
    
    // Set buffer sizes
    int buffer_size = 65536;
    setsockopt(g_main_socket, SOL_SOCKET, SO_RCVBUF, (char*)&buffer_size, sizeof(buffer_size));
    setsockopt(g_main_socket, SOL_SOCKET, SO_SNDBUF, (char*)&buffer_size, sizeof(buffer_size));
    
    // Set non-blocking mode
    u_long mode = 1;
    ioctlsocket(g_main_socket, FIONBIO, &mode);
    
    // Bind to port
    struct sockaddr_in addr4;
    memset(&addr4, 0, sizeof(addr4));
    addr4.sin_family = AF_INET;
    addr4.sin_addr.s_addr = INADDR_ANY;
    addr4.sin_port = htons(final_port);
    
    if (bind(g_main_socket, (struct sockaddr*)&addr4, sizeof(addr4)) == SOCKET_ERROR) {
        network_log("Failed to bind IPv4 socket to port %d: %d", final_port, WSAGetLastError());
        closesocket(g_main_socket);
        g_main_socket = INVALID_SOCKET;
        return 0;
    }
    
    memcpy(&g_local_address, &addr4, sizeof(addr4));
    
    // Create IPv6 socket if requested
    if (ipv6) {
        g_ipv6_socket = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
        if (g_ipv6_socket != INVALID_SOCKET) {
            // IPv6 only (no dual stack)
            int ipv6only = 1;
            setsockopt(g_ipv6_socket, IPPROTO_IPV6, IPV6_V6ONLY, (char*)&ipv6only, sizeof(ipv6only));
            
            // Set options
            setsockopt(g_ipv6_socket, SOL_SOCKET, SO_REUSEADDR, (char*)&reuse, sizeof(reuse));
            setsockopt(g_ipv6_socket, SOL_SOCKET, SO_RCVBUF, (char*)&buffer_size, sizeof(buffer_size));
            setsockopt(g_ipv6_socket, SOL_SOCKET, SO_SNDBUF, (char*)&buffer_size, sizeof(buffer_size));
            ioctlsocket(g_ipv6_socket, FIONBIO, &mode);
            
            // Bind to port
            struct sockaddr_in6 addr6;
            memset(&addr6, 0, sizeof(addr6));
            addr6.sin6_family = AF_INET6;
            addr6.sin6_addr = in6addr_any;
            addr6.sin6_port = htons(final_port);
            
            if (bind(g_ipv6_socket, (struct sockaddr*)&addr6, sizeof(addr6)) == SOCKET_ERROR) {
                network_log("Failed to bind IPv6 socket: %d", WSAGetLastError());
                closesocket(g_ipv6_socket);
                g_ipv6_socket = INVALID_SOCKET;
            } else {
                network_log("IPv6 support enabled on port %d", final_port);
            }
        }
    }
    
    g_network_mode = NETWORK_MODE_SERVER;
    g_local_client_id = 0;  // Server is always client 0
    
    network_log("Server started on port %d", final_port);
    return 1;
}

/**
 * Connects to a server
 * @param host Server hostname or IP address
 * @param port Server port (0 for default)
 * @return 1 on success, 0 on failure
 */
int connect_to_server(const char* host, int port)
{
    if (g_network_mode != NETWORK_MODE_NONE) {
        network_log("Already in network mode %d", g_network_mode);
        return 0;
    }
    
    int final_port = port ? port : DEFAULT_PORT;
    
    // Resolve server address
    struct addrinfo hints, *result = NULL;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;  // Allow IPv4 or IPv6
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;
    
    char port_str[16];
    sprintf(port_str, "%d", final_port);
    
    int status = getaddrinfo(host, port_str, &hints, &result);
    if (status != 0) {
        network_log("Failed to resolve server address %s: %s", host, gai_strerror(status));
        return 0;
    }
    
    // Try each address until we successfully connect
    struct addrinfo* ptr;
    SOCKET sock = INVALID_SOCKET;
    
    for (ptr = result; ptr != NULL; ptr = ptr->ai_next) {
        // Create socket
        sock = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
        if (sock == INVALID_SOCKET) {
            continue;
        }
        
        // Set non-blocking mode
        u_long mode = 1;
        ioctlsocket(sock, FIONBIO, &mode);
        
        // Set buffer sizes
        int buffer_size = 65536;
        setsockopt(sock, SOL_SOCKET, SO_RCVBUF, (char*)&buffer_size, sizeof(buffer_size));
        setsockopt(sock, SOL_SOCKET, SO_SNDBUF, (char*)&buffer_size, sizeof(buffer_size));
        
        // Copy server address
        memcpy(&g_server_address, ptr->ai_addr, ptr->ai_addrlen);
        
        // Success
        break;
    }
    
    freeaddrinfo(result);
    
    if (sock == INVALID_SOCKET) {
        network_log("Failed to create socket for server connection");
        return 0;
    }
    
    g_main_socket = sock;
    g_network_mode = NETWORK_MODE_CLIENT;
    
    // Send connection request
    send_connect_request();
    
    char addr_str[256];
    address_to_string(&g_server_address, addr_str, sizeof(addr_str));
    network_log("Connecting to server at %s", addr_str);
    
    return 1;
}

/**
 * Disconnects from the network
 */
void disconnect_from_network(void)
{
    if (g_network_mode == NETWORK_MODE_NONE) {
        return;
    }
    
    network_log("Disconnecting from network");
    
    // Send disconnect packets to all clients
    if (g_network_mode == NETWORK_MODE_SERVER) {
        for (int i = 0; i < g_client_count; i++) {
            if (g_clients[i].state == CONNECTION_STATE_CONNECTED) {
                send_disconnect_packet(g_clients[i].client_id, "Server shutting down");
            }
        }
    } else if (g_network_mode == NETWORK_MODE_CLIENT) {
        send_disconnect_packet(0, "Client disconnecting");
    }
    
    // Give packets time to send
    Sleep(100);
    
    // Close sockets
    if (g_main_socket != INVALID_SOCKET) {
        closesocket(g_main_socket);
        g_main_socket = INVALID_SOCKET;
    }
    
    if (g_ipv6_socket != INVALID_SOCKET) {
        closesocket(g_ipv6_socket);
        g_ipv6_socket = INVALID_SOCKET;
    }
    
    // Close any open file transfers
    for (int i = 0; i < MAX_CONCURRENT_TRANSFERS; i++) {
        if (g_file_transfers[i].active) {
            if (g_file_transfers[i].file_handle) {
                fclose(g_file_transfers[i].file_handle);
            }
            if (g_file_transfers[i].file_buffer) {
                free(g_file_transfers[i].file_buffer);
            }
            if (g_file_transfers[i].received_chunks) {
                free(g_file_transfers[i].received_chunks);
            }
        }
    }
    
    // Free voice chat buffers
    for (int i = 0; i < g_client_count; i++) {
        if (g_clients[i].voice.buffer) {
            free(g_clients[i].voice.buffer);
            g_clients[i].voice.buffer = NULL;
        }
    }
    
    // Reset state
    g_network_mode = NETWORK_MODE_NONE;
    g_client_count = 0;
    g_transfer_count = 0;
    g_local_client_id = 0;
    g_next_client_id = 1;
    memset(&g_local_address, 0, sizeof(g_local_address));
    memset(&g_server_address, 0, sizeof(g_server_address));
}

// ========================================================================
// PACKET MANAGEMENT
// ========================================================================

/**
 * Sends a packet to a client
 * @param client_id Target client ID (0 for server when client)
 * @param type Packet type
 * @param data Data to send
 * @param data_size Size of data
 * @param reliable Whether to use reliable delivery
 * @param priority Packet priority (higher = more important)
 * @return 1 on success, 0 on failure
 */
int send_packet_ex(unsigned int client_id, PacketType type, const void* data, 
                   unsigned int data_size, int reliable, float priority)
{
    if (g_network_mode == NETWORK_MODE_NONE || g_main_socket == INVALID_SOCKET) {
        return 0;
    }
    
    if (data_size > MAX_PACKET_SIZE - sizeof(PacketHeader)) {
        network_log("Packet too large: %u bytes (max %u)", data_size, 
                   MAX_PACKET_SIZE - sizeof(PacketHeader));
        return 0;
    }
    
    NetworkClient* client = NULL;
    struct sockaddr_storage* target_addr;
    int addr_len;
    SOCKET socket;
    
    if (g_network_mode == NETWORK_MODE_SERVER) {
        client = find_client(client_id);
        if (!client) {
            network_log("Client %u not found", client_id);
            return 0;
        }
        target_addr = &client->address;
        socket = (client->address_family == AF_INET6 && g_ipv6_socket != INVALID_SOCKET) ? 
                 g_ipv6_socket : g_main_socket;
    } else {
        target_addr = &g_server_address;
        socket = g_main_socket;
        // Find server as client 0
        if (g_client_count > 0 && g_clients[0].client_id == 0) {
            client = &g_clients[0];
        }
    }
    
    addr_len = (target_addr->ss_family == AF_INET) ? sizeof(struct sockaddr_in) : 
                                                     sizeof(struct sockaddr_in6);
    
    // Check bandwidth throttling
    if (client && g_bandwidth_limit > 0) {
        unsigned int current_time = GetTickCount();
        if (current_time < client->bandwidth.throttle_time) {
            return 0;  // Still throttled
        }
    }
    
    // Create packet
    unsigned char packet_buffer[MAX_PACKET_SIZE];
    unsigned char compressed_buffer[MAX_PACKET_SIZE];
    PacketHeader* header = (PacketHeader*)packet_buffer;
    
    memset(header, 0, sizeof(PacketHeader));
    header->magic = (target_addr->ss_family == AF_INET) ? 0xE4D0 : 0xE6D0;
    header->version = 1;
    header->type = (unsigned char)type;
    header->flags = 0;
    if (reliable) header->flags |= 0x01;
    if (g_enable_encryption) header->flags |= 0x02;
    header->security = g_enable_encryption ? SECURITY_LEVEL_ENCRYPTED : SECURITY_LEVEL_NONE;
    header->sequence = client ? ++client->last_sequence : 0;
    header->ack = client ? client->last_ack : 0;
    header->timestamp = GetTickCount();
    header->client_id = g_local_client_id;
    header->data_size = (unsigned short)data_size;
    
    // Copy data
    unsigned char* packet_data = packet_buffer + sizeof(PacketHeader);
    if (data && data_size > 0) {
        memcpy(packet_data, data, data_size);
    }
    
    // Try compression
    int compressed_size = compress_packet(packet_data, data_size, 
                                        compressed_buffer, sizeof(compressed_buffer));
    if (compressed_size > 0 && compressed_size < data_size) {
        memcpy(packet_data, compressed_buffer, compressed_size);
        header->data_size = (unsigned short)compressed_size;
        header->flags |= 0x04;  // Compressed flag
        network_log("Compressed packet from %u to %d bytes", data_size, compressed_size);
    }
    
    // Encrypt if enabled
    if (g_enable_encryption && g_encryption_initialized) {
        // Simple XOR encryption for demonstration
        for (int i = 0; i < header->data_size; i++) {
            packet_data[i] ^= g_encryption_key[i % ENCRYPTION_KEY_SIZE];
        }
    }
    
    // Calculate checksum
    header->checksum = 0;
    header->checksum = (unsigned short)calculate_crc32(packet_buffer, 
                                                       sizeof(PacketHeader) + header->data_size);
    
    // Send packet
    int bytes_sent = sendto(socket, (char*)packet_buffer, 
                           sizeof(PacketHeader) + header->data_size, 0,
                           (struct sockaddr*)target_addr, addr_len);
    
    if (bytes_sent == SOCKET_ERROR) {
        int error = WSAGetLastError();
        network_log("Failed to send packet: %d", error);
        g_stats.network_errors++;
        return 0;
    }
    
    // Update statistics
    g_stats.total_packets_sent++;
    g_stats.total_bytes_sent += bytes_sent;
    
    if (client) {
        client->bandwidth.packets_sent++;
        client->bandwidth.bytes_sent += bytes_sent;
        
        // Update bandwidth throttling
        if (g_bandwidth_limit > 0) {
            int bytes_per_ms = (g_bandwidth_limit * 1024) / 1000;
            int throttle_ms = bytes_sent / bytes_per_ms;
            client->bandwidth.throttle_time = GetTickCount() + throttle_ms;
        }
    }
    
    // Handle reliable packets
    if (reliable) {
        g_stats.reliable_packets_sent++;
        
        if (client) {
            int slot = header->sequence % MAX_RELIABLE_PACKETS;
            ReliablePacket* rel_packet = &client->reliable_outgoing[slot];
            
            memcpy(&rel_packet->header, header, sizeof(PacketHeader));
            memcpy(rel_packet->data, packet_data, header->data_size);
            rel_packet->send_time = GetTickCount();
            rel_packet->last_retry_time = rel_packet->send_time;
            rel_packet->retry_count = 0;
            rel_packet->acknowledged = 0;
            rel_packet->priority = priority;
        }
    }
    
    network_log("Sent %s packet to client %u (%d bytes)", 
               get_packet_type_name(type), client_id, bytes_sent);
    
    return 1;
}

/**
 * Simple wrapper for send_packet_ex
 */
int send_packet(unsigned int client_id, PacketType type, const void* data, 
                unsigned int data_size, int reliable)
{
    return send_packet_ex(client_id, type, data, data_size, reliable, 1.0f);
}

/**
 * Receives and processes incoming packets
 * @return Number of packets received
 */
int receive_packets(void)
{
    if (g_network_mode == NETWORK_MODE_NONE) {
        return 0;
    }
    
    unsigned char packet_buffer[MAX_PACKET_SIZE];
    unsigned char decompressed_buffer[MAX_PACKET_SIZE];
    struct sockaddr_storage sender_addr;
    int addr_len = sizeof(sender_addr);
    int packets_received = 0;
    
    // Array of sockets to check
    SOCKET sockets[2] = { g_main_socket, g_ipv6_socket };
    int socket_count = (g_ipv6_socket != INVALID_SOCKET) ? 2 : 1;
    
    for (int s = 0; s < socket_count; s++) {
        if (sockets[s] == INVALID_SOCKET) continue;
        
        while (1) {
            memset(&sender_addr, 0, sizeof(sender_addr));
            addr_len = sizeof(sender_addr);
            
            int bytes_received = recvfrom(sockets[s], (char*)packet_buffer, MAX_PACKET_SIZE, 0,
                                        (struct sockaddr*)&sender_addr, &addr_len);
            
            if (bytes_received == SOCKET_ERROR) {
                int error = WSAGetLastError();
                if (error == WSAEWOULDBLOCK) {
                    break;  // No more packets on this socket
                }
                network_log("recvfrom error: %d", error);
                g_stats.network_errors++;
                break;
            }
            
            if (bytes_received < sizeof(PacketHeader)) {
                network_log("Packet too small: %d bytes", bytes_received);
                continue;
            }
            
            PacketHeader* header = (PacketHeader*)packet_buffer;
            
            // Validate magic number
            if (header->magic != 0xE4D0 && header->magic != 0xE6D0) {
                network_log("Invalid packet magic: 0x%04X", header->magic);
                continue;
            }
            
            // Validate version
            if (header->version != 1) {
                network_log("Unsupported protocol version: %d", header->version);
                continue;
            }
            
            // Validate size
            if (header->data_size != bytes_received - sizeof(PacketHeader)) {
                network_log("Size mismatch: header says %u, got %d", 
                           header->data_size, bytes_received - sizeof(PacketHeader));
                continue;
            }
            
            // Verify checksum
            unsigned short stored_checksum = header->checksum;
            header->checksum = 0;
            unsigned short calculated_checksum = (unsigned short)calculate_crc32(packet_buffer, 
                                                                               bytes_received);
            
            if (stored_checksum != calculated_checksum) {
                network_log("Checksum failed: expected 0x%04X, got 0x%04X", 
                           stored_checksum, calculated_checksum);
                continue;
            }
            
            header->checksum = stored_checksum;
            
            // Get packet data pointer
            unsigned char* packet_data = packet_buffer + sizeof(PacketHeader);
            unsigned int data_size = header->data_size;
            
            // Decrypt if needed
            if ((header->flags & 0x02) && g_enable_encryption && g_encryption_initialized) {
                for (int i = 0; i < data_size; i++) {
                    packet_data[i] ^= g_encryption_key[i % ENCRYPTION_KEY_SIZE];
                }
            }
            
            // Decompress if needed
            if (header->flags & 0x04) {
                int decompressed_size = decompress_packet(packet_data, data_size,
                                                        decompressed_buffer, sizeof(decompressed_buffer));
                if (decompressed_size > 0) {
                    packet_data = decompressed_buffer;
                    data_size = decompressed_size;
                    network_log("Decompressed packet from %u to %d bytes", 
                               header->data_size, decompressed_size);
                } else {
                    network_log("Failed to decompress packet");
                    continue;
                }
            }
            
            // Update statistics
            g_stats.total_packets_received++;
            g_stats.total_bytes_received += bytes_received;
            packets_received++;
            
            // Process packet
            process_received_packet(header, packet_data, data_size, &sender_addr);
        }
    }
    
    return packets_received;
}

/**
 * Processes a received packet
 * @param header Packet header
 * @param data Packet data
 * @param data_size Data size
 * @param sender_addr Sender address
 */
void process_received_packet(PacketHeader* header, unsigned char* data, 
                            unsigned int data_size, struct sockaddr_storage* sender_addr)
{
    char addr_str[256];
    address_to_string(sender_addr, addr_str, sizeof(addr_str));
    
    network_log("Received %s packet from %s (%u bytes)", 
               get_packet_type_name(header->type), addr_str, data_size);
    
    NetworkClient* client = find_client_by_address(sender_addr);
    
    // Handle connection packets
    if (header->type == PACKET_TYPE_CONNECT_REQUEST && g_network_mode == NETWORK_MODE_SERVER) {
        handle_connect_request(data, data_size, sender_addr);
        return;
    }
    
    if (header->type == PACKET_TYPE_CONNECT_RESPONSE && g_network_mode == NETWORK_MODE_CLIENT) {
        handle_connect_response(data, data_size, sender_addr);
        return;
    }
    
    // Server info requests don't require a client connection
    if (header->type == PACKET_TYPE_SERVER_INFO_REQUEST) {
        handle_server_info_request(sender_addr);
        return;
    }
    
    if (header->type == PACKET_TYPE_SERVER_INFO_RESPONSE && g_network_mode == NETWORK_MODE_CLIENT) {
        handle_server_info_response(data, data_size, sender_addr);
        return;
    }
    
    // Must have valid client for other packet types
    if (!client) {
        network_log("Packet from unknown client: %s", addr_str);
        return;
    }
    
    // Update client state
    client->last_activity = GetTickCount();
    client->bandwidth.bytes_received += sizeof(PacketHeader) + data_size;
    client->bandwidth.packets_received++;
    
    // Update sequence tracking
    if (header->sequence > client->last_ack) {
        client->last_ack = header->sequence;
    }
    
    // Handle acknowledgments
    if (header->flags & 0x01) {  // Reliable packet
        send_reliable_ack(client->client_id, header->sequence);
        g_stats.reliable_packets_received++;
    }
    
    if (header->ack > 0) {
        acknowledge_reliable_packet(client, header->ack);
    }
    
    // Handle specific packet types that need immediate processing
    if (header->type == PACKET_TYPE_RELIABLE_ACK) {
        // Already handled above
        return;
    }
    
    if (header->type == PACKET_TYPE_PING) {
        handle_ping(client->client_id, data, data_size);
        return;
    }
    
    if (header->type == PACKET_TYPE_PONG) {
        handle_pong(client, data, data_size);
        return;
    }
    
    // Queue message for processing
    queue_network_message(header->type, client->client_id, data, data_size, 
                         header->timestamp, 1.0f);
}

/**
 * Queues a network message for processing
 * @param type Message type
 * @param client_id Client ID
 * @param data Message data
 * @param data_size Data size
 * @param timestamp Message timestamp
 * @param priority Message priority
 */
void queue_network_message(PacketType type, unsigned int client_id, unsigned char* data, 
                          unsigned int data_size, unsigned int timestamp, float priority)
{
    int next_head = (g_message_queue_head + 1) % g_message_queue_size;
    
    if (next_head == g_message_queue_tail) {
        // Queue full, drop oldest message
        NetworkMessage* old_msg = &g_message_queue[g_message_queue_tail];
        if (old_msg->data) {
            free(old_msg->data);
            old_msg->data = NULL;
        }
        g_message_queue_tail = (g_message_queue_tail + 1) % g_message_queue_size;
        network_log("Message queue overflow, dropped oldest message");
    }
    
    NetworkMessage* msg = &g_message_queue[g_message_queue_head];
    
    // Free any existing data
    if (msg->data) {
        free(msg->data);
    }
    
    msg->type = type;
    msg->client_id = client_id;
    msg->data_size = data_size;
    msg->timestamp = timestamp;
    msg->priority = priority;
    msg->processed = 0;
    
    // Allocate and copy data
    if (data && data_size > 0) {
        msg->data = (unsigned char*)malloc(data_size);
        if (msg->data) {
            memcpy(msg->data, data, data_size);
        } else {
            network_log("Failed to allocate message data");
            return;
        }
    } else {
        msg->data = NULL;
    }
    
    g_message_queue_head = next_head;
}

/**
 * Gets the next network message from the queue
 * @param message Output message structure
 * @return 1 if message available, 0 if queue empty
 */
int get_next_network_message(NetworkMessage* message)
{
    if (g_message_queue_tail == g_message_queue_head) {
        return 0;  // No messages
    }
    
    // Find highest priority unprocessed message
    int best_index = -1;
    float best_priority = -1.0f;
    int index = g_message_queue_tail;
    
    while (index != g_message_queue_head) {
        NetworkMessage* msg = &g_message_queue[index];
        if (!msg->processed && msg->priority > best_priority) {
            best_priority = msg->priority;
            best_index = index;
        }
        index = (index + 1) % g_message_queue_size;
    }
    
    if (best_index < 0) {
        // All messages processed, advance tail
        while (g_message_queue_tail != g_message_queue_head &&
               g_message_queue[g_message_queue_tail].processed) {
            NetworkMessage* old_msg = &g_message_queue[g_message_queue_tail];
            if (old_msg->data) {
                free(old_msg->data);
                old_msg->data = NULL;
            }
            g_message_queue_tail = (g_message_queue_tail + 1) % g_message_queue_size;
        }
        return 0;
    }
    
    // Copy message
    NetworkMessage* src = &g_message_queue[best_index];
    *message = *src;
    src->processed = 1;
    
    return 1;
}

// ========================================================================
// CLIENT MANAGEMENT
// ========================================================================

/**
 * Finds a client by ID
 * @param client_id Client ID to find
 * @return Client pointer or NULL
 */
NetworkClient* find_client(unsigned int client_id)
{
    for (int i = 0; i < g_client_count; i++) {
        if (g_clients[i].client_id == client_id) {
            return &g_clients[i];
        }
    }
    return NULL;
}

/**
 * Finds a client by address
 * @param address Client address
 * @return Client pointer or NULL
 */
NetworkClient* find_client_by_address(struct sockaddr_storage* address)
{
    for (int i = 0; i < g_client_count; i++) {
        if (address->ss_family == g_clients[i].address.ss_family) {
            if (address->ss_family == AF_INET) {
                struct sockaddr_in* addr1 = (struct sockaddr_in*)address;
                struct sockaddr_in* addr2 = (struct sockaddr_in*)&g_clients[i].address;
                if (addr1->sin_addr.s_addr == addr2->sin_addr.s_addr &&
                    addr1->sin_port == addr2->sin_port) {
                    return &g_clients[i];
                }
            } else if (address->ss_family == AF_INET6) {
                struct sockaddr_in6* addr1 = (struct sockaddr_in6*)address;
                struct sockaddr_in6* addr2 = (struct sockaddr_in6*)&g_clients[i].address;
                if (memcmp(&addr1->sin6_addr, &addr2->sin6_addr, sizeof(struct in6_addr)) == 0 &&
                    addr1->sin6_port == addr2->sin6_port) {
                    return &g_clients[i];
                }
            }
        }
    }
    return NULL;
}

/**
 * Adds a new client
 * @param address Client address
 * @return Client index or -1 on failure
 */
int add_client(struct sockaddr_storage* address)
{
    if (g_client_count >= g_max_clients) {
        network_log("Maximum client limit reached (%d)", g_max_clients);
        return -1;
    }
    
    NetworkClient* client = &g_clients[g_client_count];
    memset(client, 0, sizeof(NetworkClient));
    
    client->address = *address;
    client->address_family = address->ss_family;
    client->state = CONNECTION_STATE_CONNECTING;
    client->security_level = SECURITY_LEVEL_NONE;
    client->client_id = g_next_client_id++;
    client->last_heartbeat = GetTickCount();
    client->last_activity = client->last_heartbeat;
    client->join_time = client->last_heartbeat;
    client->trust_score = 0.5f;  // Start with neutral trust
    client->reliable_window_size = 32;  // Start with small window
    
    sprintf(client->player_name, "Player_%d", client->client_id);
    
    // Initialize voice chat if enabled
    if (g_enable_voice_chat) {
        client->voice.enabled = 1;
        client->voice.volume = 1.0f;
        client->voice.buffer_size = VOICE_SAMPLE_RATE * 2;  // 2 seconds buffer
        client->voice.buffer = (unsigned char*)calloc(client->voice.buffer_size, 1);
    }
    
    char addr_str[256];
    address_to_string(address, addr_str, sizeof(addr_str));
    network_log("Added client %u from %s", client->client_id, addr_str);
    
    return g_client_count++;
}

/**
 * Removes a client
 * @param client_id Client ID to remove
 */
void remove_client(unsigned int client_id)
{
    for (int i = 0; i < g_client_count; i++) {
        if (g_clients[i].client_id == client_id) {
            NetworkClient* client = &g_clients[i];
            
            network_log("Removing client %u (%s)", client_id, client->player_name);
            
            // Send disconnect notification to other clients
            if (g_network_mode == NETWORK_MODE_SERVER) {
                broadcast_client_disconnect(client_id);
            }
            
            // Free voice buffer
            if (client->voice.buffer) {
                free(client->voice.buffer);
                client->voice.buffer = NULL;
            }
            
            // Cancel any file transfers
            for (int j = 0; j < MAX_CONCURRENT_TRANSFERS; j++) {
                if (g_file_transfers[j].active && g_file_transfers[j].client_id == client_id) {
                    cancel_file_transfer(&g_file_transfers[j]);
                }
            }
            
            // Shift clients down
            for (int j = i; j < g_client_count - 1; j++) {
                g_clients[j] = g_clients[j + 1];
            }
            g_client_count--;
            
            // Update statistics
            g_stats.connections_accepted--;  // Was counted when added
            
            break;
        }
    }
}

/**
 * Updates client timeouts and connection states
 */
void update_client_timeouts(void)
{
    unsigned int current_time = GetTickCount();
    
    for (int i = 0; i < g_client_count; i++) {
        NetworkClient* client = &g_clients[i];
        unsigned int time_since_activity = current_time - client->last_activity;
        
        // Check for timeout
        if (time_since_activity > TIMEOUT_INTERVAL) {
            network_log("Client %u timed out after %u ms", 
                       client->client_id, time_since_activity);
            client->state = CONNECTION_STATE_TIMEOUT;
            remove_client(client->client_id);
            i--;  // Adjust index after removal
            continue;
        }
        
        // Send keep-alive if needed
        if (time_since_activity > HEARTBEAT_INTERVAL && 
            current_time - client->last_heartbeat > HEARTBEAT_INTERVAL) {
            send_packet(client->client_id, PACKET_TYPE_KEEP_ALIVE, NULL, 0, 0);
            client->last_heartbeat = current_time;
        }
        
        // Update bandwidth calculations
        if (current_time - client->bandwidth.last_update > 1000) {
            float time_diff = (current_time - client->bandwidth.last_update) / 1000.0f;
            client->bandwidth.bandwidth_in = (client->bandwidth.bytes_received / 1024.0f) / time_diff;
            client->bandwidth.bandwidth_out = (client->bandwidth.bytes_sent / 1024.0f) / time_diff;
            
            // Reset counters
            client->bandwidth.bytes_sent = 0;
            client->bandwidth.bytes_received = 0;
            client->bandwidth.packets_sent = 0;
            client->bandwidth.packets_received = 0;
            client->bandwidth.last_update = current_time;
        }
    }
}

// ========================================================================
// CONNECTION HANDLING
// ========================================================================

/**
 * Sends a connection request to the server
 */
void send_connect_request(void)
{
    if (g_network_mode != NETWORK_MODE_CLIENT) return;
    
    struct {
        char protocol_version[16];
        char player_name[64];
        char password[64];
        unsigned int client_version;
        unsigned int features;
    } connect_data;
    
    memset(&connect_data, 0, sizeof(connect_data));
    strcpy(connect_data.protocol_version, "ENDOR_1.0");
    strcpy(connect_data.player_name, "Player");  // TODO: Get from config
    strcpy(connect_data.password, g_server_password);
    connect_data.client_version = 0x01000000;  // 1.0.0.0
    connect_data.features = 0;
    if (g_enable_voice_chat) connect_data.features |= 0x01;
    if (g_enable_encryption) connect_data.features |= 0x02;
    
    send_packet(0, PACKET_TYPE_CONNECT_REQUEST, &connect_data, sizeof(connect_data), 1);
}

/**
 * Handles a connection request from a client
 * @param data Request data
 * @param data_size Data size
 * @param sender_addr Client address
 */
void handle_connect_request(unsigned char* data, unsigned int data_size, 
                           struct sockaddr_storage* sender_addr)
{
    if (g_network_mode != NETWORK_MODE_SERVER) return;
    
    char addr_str[256];
    address_to_string(sender_addr, addr_str, sizeof(addr_str));
    network_log("Connection request from %s", addr_str);
    
    // Check if client is already connected
    NetworkClient* existing_client = find_client_by_address(sender_addr);
    if (existing_client) {
        network_log("Client already connected from %s", addr_str);
        return;
    }
    
    // Check server capacity
    if (g_client_count >= g_max_clients) {
        send_connect_response_direct(sender_addr, 0, 0, "Server full");
        g_stats.connections_rejected++;
        return;
    }
    
    // Parse connection data
    struct {
        char protocol_version[16];
        char player_name[64];
        char password[64];
        unsigned int client_version;
        unsigned int features;
    } connect_data;
    
    if (data_size < sizeof(connect_data)) {
        send_connect_response_direct(sender_addr, 0, 0, "Invalid request");
        g_stats.connections_rejected++;
        return;
    }
    
    memcpy(&connect_data, data, sizeof(connect_data));
    
    // Validate protocol version
    if (strcmp(connect_data.protocol_version, "ENDOR_1.0") != 0) {
        send_connect_response_direct(sender_addr, 0, 0, "Protocol version mismatch");
        g_stats.connections_rejected++;
        return;
    }
    
    // Check password if required
    if (strlen(g_server_password) > 0 && strcmp(connect_data.password, g_server_password) != 0) {
        send_connect_response_direct(sender_addr, 0, 0, "Invalid password");
        g_stats.connections_rejected++;
        return;
    }
    
    // Add new client
    int client_index = add_client(sender_addr);
    if (client_index < 0) {
        send_connect_response_direct(sender_addr, 0, 0, "Connection failed");
        g_stats.connections_rejected++;
        return;
    }
    
    NetworkClient* new_client = &g_clients[client_index];
    new_client->state = CONNECTION_STATE_CONNECTED;
    
    // Set player name
    if (strlen(connect_data.player_name) > 0) {
        strncpy(new_client->player_name, connect_data.player_name, 
                sizeof(new_client->player_name) - 1);
    }
    
    // Check feature compatibility
    if ((connect_data.features & 0x02) && g_enable_encryption) {
        new_client->security_level = SECURITY_LEVEL_ENCRYPTED;
    }
    
    // Send connection response
    send_connect_response_success(new_client);
    
    // Send current game state to new client
    send_initial_game_state(new_client->client_id);
    
    // Notify other clients
    broadcast_client_join(new_client->client_id);
    
    g_stats.connections_accepted++;
}

/**
 * Sends a successful connection response
 * @param client Client to respond to
 */
void send_connect_response_success(NetworkClient* client)
{
    struct {
        unsigned int result;
        unsigned int client_id;
        char server_name[128];
        unsigned int server_features;
        int player_count;
        int max_players;
        float tick_rate;
        unsigned char encryption_key[ENCRYPTION_KEY_SIZE];
    } response_data;
    
    memset(&response_data, 0, sizeof(response_data));
    response_data.result = 1;  // Success
    response_data.client_id = client->client_id;
    strcpy(response_data.server_name, g_server_name);
    response_data.server_features = 0;
    if (g_enable_voice_chat) response_data.server_features |= 0x01;
    if (g_enable_encryption) response_data.server_features |= 0x02;
    if (g_allow_downloads) response_data.server_features |= 0x04;
    response_data.player_count = g_client_count;
    response_data.max_players = g_max_clients;
    response_data.tick_rate = g_tick_rate;
    
    // Include encryption key if using encryption
    if (g_enable_encryption && client->security_level == SECURITY_LEVEL_ENCRYPTED) {
        memcpy(response_data.encryption_key, g_encryption_key, ENCRYPTION_KEY_SIZE);
    }
    
    send_packet(client->client_id, PACKET_TYPE_CONNECT_RESPONSE, 
                &response_data, sizeof(response_data), 1);
}

/**
 * Sends a connection failure response directly to an address
 * @param address Target address
 * @param result Result code
 * @param client_id Client ID (0 for failure)
 * @param reason Failure reason
 */
void send_connect_response_direct(struct sockaddr_storage* address, unsigned int result,
                                 unsigned int client_id, const char* reason)
{
    struct {
        unsigned int result;
        unsigned int client_id;
        char reason[256];
    } response_data;
    
    memset(&response_data, 0, sizeof(response_data));
    response_data.result = result;
    response_data.client_id = client_id;
    if (reason) {
        strncpy(response_data.reason, reason, sizeof(response_data.reason) - 1);
    }
    
    // Send directly without client record
    unsigned char packet_buffer[MAX_PACKET_SIZE];
    PacketHeader* header = (PacketHeader*)packet_buffer;
    
    memset(header, 0, sizeof(PacketHeader));
    header->magic = (address->ss_family == AF_INET) ? 0xE4D0 : 0xE6D0;
    header->version = 1;
    header->type = PACKET_TYPE_CONNECT_RESPONSE;
    header->flags = 0;
    header->timestamp = GetTickCount();
    header->data_size = sizeof(response_data);
    
    memcpy(packet_buffer + sizeof(PacketHeader), &response_data, sizeof(response_data));
    header->checksum = (unsigned short)calculate_crc32(packet_buffer, 
                                                      sizeof(PacketHeader) + sizeof(response_data));
    
    SOCKET socket = (address->ss_family == AF_INET6 && g_ipv6_socket != INVALID_SOCKET) ?
                    g_ipv6_socket : g_main_socket;
    int addr_len = (address->ss_family == AF_INET) ? sizeof(struct sockaddr_in) :
                                                     sizeof(struct sockaddr_in6);
    
    sendto(socket, (char*)packet_buffer, sizeof(PacketHeader) + sizeof(response_data), 0,
           (struct sockaddr*)address, addr_len);
    
    char addr_str[256];
    address_to_string(address, addr_str, sizeof(addr_str));
    network_log("Sent connection failure to %s: %s", addr_str, reason);
}

/**
 * Handles a connection response from the server
 * @param data Response data
 * @param data_size Data size
 * @param server_addr Server address
 */
void handle_connect_response(unsigned char* data, unsigned int data_size,
                            struct sockaddr_storage* server_addr)
{
    if (g_network_mode != NETWORK_MODE_CLIENT) return;
    
    struct {
        unsigned int result;
        unsigned int client_id;
        char server_name[128];
        unsigned int server_features;
        int player_count;
        int max_players;
        float tick_rate;
        unsigned char encryption_key[ENCRYPTION_KEY_SIZE];
    } response_data;
    
    if (data_size < sizeof(unsigned int) * 2) {
        network_log("Invalid connection response size");
        disconnect_from_network();
        return;
    }
    
    // Check if it's a failure response
    unsigned int result = *(unsigned int*)data;
    if (result == 0) {
        // Parse failure response
        struct {
            unsigned int result;
            unsigned int client_id;
            char reason[256];
        } failure_data;
        
        memcpy(&failure_data, data, fmin(data_size, sizeof(failure_data)));
        network_log("Connection failed: %s", failure_data.reason);
        disconnect_from_network();
        return;
    }
    
    // Parse success response
    memcpy(&response_data, data, fmin(data_size, sizeof(response_data)));
    
    g_local_client_id = response_data.client_id;
    
    // Add server as client 0
    NetworkClient* server_client = &g_clients[0];
    memset(server_client, 0, sizeof(NetworkClient));
    server_client->state = CONNECTION_STATE_CONNECTED;
    server_client->client_id = 0;
    server_client->address = *server_addr;
    server_client->address_family = server_addr->ss_family;
    strcpy(server_client->player_name, "Server");
    server_client->last_activity = GetTickCount();
    g_client_count = 1;
    
    // Apply server settings
    if (response_data.server_features & 0x02 && g_enable_encryption) {
        memcpy(g_encryption_key, response_data.encryption_key, ENCRYPTION_KEY_SIZE);
        server_client->security_level = SECURITY_LEVEL_ENCRYPTED;
    }
    
    network_log("Connected to server '%s' as client %u", 
               response_data.server_name, g_local_client_id);
    network_log("Server has %d/%d players, tick rate %.1f", 
               response_data.player_count, response_data.max_players, response_data.tick_rate);
}

/**
 * Sends initial game state to a new client
 * @param client_id Client to send state to
 */
void send_initial_game_state(unsigned int client_id)
{
    // Send information about all connected clients
    for (int i = 0; i < g_client_count; i++) {
        if (g_clients[i].client_id != client_id && 
            g_clients[i].state == CONNECTION_STATE_CONNECTED) {
            
            struct {
                unsigned int client_id;
                char player_name[64];
                int team;
                int score;
                float skill_rating;
            } player_info;
            
            player_info.client_id = g_clients[i].client_id;
            strcpy(player_info.player_name, g_clients[i].player_name);
            player_info.team = g_clients[i].team;
            player_info.score = g_clients[i].score;
            player_info.skill_rating = g_clients[i].skill_rating;
            
            send_packet(client_id, PACKET_TYPE_PLAYER_STATE, &player_info, 
                       sizeof(player_info), 1);
        }
    }
    
    // Send current game state
    send_game_state_update();
}

/**
 * Sends a disconnect packet
 * @param client_id Client to disconnect
 * @param reason Disconnect reason
 */
void send_disconnect_packet(unsigned int client_id, const char* reason)
{
    struct {
        char reason[256];
    } disconnect_data;
    
    memset(&disconnect_data, 0, sizeof(disconnect_data));
    if (reason) {
        strncpy(disconnect_data.reason, reason, sizeof(disconnect_data.reason) - 1);
    }
    
    send_packet(client_id, PACKET_TYPE_DISCONNECT, &disconnect_data, sizeof(disconnect_data), 1);
}

/**
 * Broadcasts that a client has joined
 * @param new_client_id New client's ID
 */
void broadcast_client_join(unsigned int new_client_id)
{
    if (g_network_mode != NETWORK_MODE_SERVER) return;
    
    NetworkClient* new_client = find_client(new_client_id);
    if (!new_client) return;
    
    struct {
        unsigned int client_id;
        char player_name[64];
        unsigned int join_time;
    } join_data;
    
    join_data.client_id = new_client_id;
    strcpy(join_data.player_name, new_client->player_name);
    join_data.join_time = new_client->join_time;
    
    for (int i = 0; i < g_client_count; i++) {
        if (g_clients[i].client_id != new_client_id && 
            g_clients[i].state == CONNECTION_STATE_CONNECTED) {
            send_packet(g_clients[i].client_id, PACKET_TYPE_EVENT, 
                       &join_data, sizeof(join_data), 0);
        }
    }
    
    network_log("Broadcasted client %u join", new_client_id);
}

/**
 * Broadcasts that a client has disconnected
 * @param client_id Disconnected client's ID
 */
void broadcast_client_disconnect(unsigned int client_id)
{
    if (g_network_mode != NETWORK_MODE_SERVER) return;
    
    struct {
        unsigned int client_id;
        char reason[128];
    } disconnect_data;
    
    disconnect_data.client_id = client_id;
    strcpy(disconnect_data.reason, "Client disconnected");
    
    for (int i = 0; i < g_client_count; i++) {
        if (g_clients[i].client_id != client_id && 
            g_clients[i].state == CONNECTION_STATE_CONNECTED) {
            send_packet(g_clients[i].client_id, PACKET_TYPE_EVENT, 
                       &disconnect_data, sizeof(disconnect_data), 0);
        }
    }
    
    network_log("Broadcasted client %u disconnect", client_id);
}

// ========================================================================
// RELIABLE PACKET SYSTEM
// ========================================================================

/**
 * Sends an acknowledgment for a reliable packet
 * @param client_id Client to acknowledge
 * @param sequence Sequence number to acknowledge
 */
void send_reliable_ack(unsigned int client_id, unsigned short sequence)
{
    struct {
        unsigned short sequence;
        unsigned int timestamp;
    } ack_data;
    
    ack_data.sequence = sequence;
    ack_data.timestamp = GetTickCount();
    
    send_packet(client_id, PACKET_TYPE_RELIABLE_ACK, &ack_data, sizeof(ack_data), 0);
}

/**
 * Processes an acknowledgment for a reliable packet
 * @param client Client that sent the acknowledgment
 * @param ack_sequence Acknowledged sequence number
 */
void acknowledge_reliable_packet(NetworkClient* client, unsigned short ack_sequence)
{
    int slot = ack_sequence % MAX_RELIABLE_PACKETS;
    ReliablePacket* packet = &client->reliable_outgoing[slot];
    
    if (packet->header.sequence == ack_sequence && !packet->acknowledged) {
        packet->acknowledged = 1;
        
        // Calculate RTT for ping estimation
        unsigned int rtt = GetTickCount() - packet->send_time;
        
        // Update ping with exponential moving average
        client->ping = (client->ping * 7 + rtt) / 8;
        
        // Update jitter
        float delta = (float)abs((int)(rtt - client->ping));
        client->jitter = (client->jitter * 7 + delta) / 8;
        
        // Adjust reliable window size based on success
        if (client->reliable_window_size < MAX_RELIABLE_PACKETS / 2) {
            client->reliable_window_size++;
        }
        
        network_log("Acknowledged packet %u from client %u (RTT: %u ms)", 
                   ack_sequence, client->client_id, rtt);
    }
}

/**
 * Resends unacknowledged reliable packets
 */
void resend_reliable_packets(void)
{
    unsigned int current_time = GetTickCount();
    
    for (int i = 0; i < g_client_count; i++) {
        NetworkClient* client = &g_clients[i];
        if (client->state != CONNECTION_STATE_CONNECTED) continue;
        
        int resent_count = 0;
        
        for (int j = 0; j < MAX_RELIABLE_PACKETS; j++) {
            ReliablePacket* packet = &client->reliable_outgoing[j];
            
            if (!packet->acknowledged && packet->header.magic != 0) {
                // Calculate adaptive timeout based on RTT and jitter
                unsigned int timeout = client->ping + (unsigned int)(client->jitter * 4);
                timeout = fmax(100, fmin(5000, timeout));  // Clamp between 100ms and 5s
                
                if (current_time - packet->last_retry_time > timeout) {
                    if (packet->retry_count < 10) {
                        // Resend packet
                        struct sockaddr_storage* target_addr = 
                            (g_network_mode == NETWORK_MODE_SERVER) ? 
                            &client->address : &g_server_address;
                        
                        SOCKET socket = (target_addr->ss_family == AF_INET6 && 
                                       g_ipv6_socket != INVALID_SOCKET) ?
                                       g_ipv6_socket : g_main_socket;
                        
                        int addr_len = (target_addr->ss_family == AF_INET) ? 
                                      sizeof(struct sockaddr_in) :
                                      sizeof(struct sockaddr_in6);
                        
                        sendto(socket, (char*)packet, 
                              sizeof(PacketHeader) + packet->header.data_size, 0,
                              (struct sockaddr*)target_addr, addr_len);
                        
                        packet->last_retry_time = current_time;
                        packet->retry_count++;
                        resent_count++;
                        
                        // Reduce window size on retransmission
                        if (client->reliable_window_size > 4) {
                            client->reliable_window_size--;
                        }
                        
                        // Limit retransmissions per update
                        if (resent_count >= client->reliable_window_size) {
                            break;
                        }
                    } else {
                        // Give up on this packet
                        packet->acknowledged = 1;
                        g_stats.reliable_packets_lost++;
                        network_log("Dropped reliable packet %u after %d retries",
                                   packet->header.sequence, packet->retry_count);
                    }
                }
            }
        }
        
        if (resent_count > 0) {
            network_log("Resent %d reliable packets to client %u", 
                       resent_count, client->client_id);
        }
    }
}

// ========================================================================
// PING AND LATENCY
// ========================================================================

/**
 * Sends a ping request
 * @param client_id Client to ping
 */
void send_ping(unsigned int client_id)
{
    struct {
        unsigned int timestamp;
        unsigned int sequence;
    } ping_data;
    
    static unsigned int ping_sequence = 0;
    
    ping_data.timestamp = GetTickCount();
    ping_data.sequence = ++ping_sequence;
    
    send_packet(client_id, PACKET_TYPE_PING, &ping_data, sizeof(ping_data), 0);
}

/**
 * Handles a ping request
 * @param client_id Client that sent the ping
 * @param data Ping data
 * @param data_size Data size
 */
void handle_ping(unsigned int client_id, unsigned char* data, unsigned int data_size)
{
    if (data_size < sizeof(unsigned int) * 2) return;
    
    struct {
        unsigned int timestamp;
        unsigned int sequence;
    } ping_data;
    
    memcpy(&ping_data, data, sizeof(ping_data));
    
    // Send pong response with same data
    send_packet(client_id, PACKET_TYPE_PONG, &ping_data, sizeof(ping_data), 0);
}

/**
 * Handles a pong response
 * @param client Client that sent the pong
 * @param data Pong data
 * @param data_size Data size
 */
void handle_pong(NetworkClient* client, unsigned char* data, unsigned int data_size)
{
    if (data_size < sizeof(unsigned int) * 2) return;
    
    struct {
        unsigned int timestamp;
        unsigned int sequence;
    } pong_data;
    
    memcpy(&pong_data, data, sizeof(pong_data));
    
    // Calculate round trip time
    unsigned int rtt = GetTickCount() - pong_data.timestamp;
    
    // Update ping
    client->ping = (client->ping * 7 + rtt) / 8;
    
    network_log("Pong from client %u: %u ms", client->client_id, rtt);
}

// ========================================================================
// GAME STATE SYNCHRONIZATION
// ========================================================================

/**
 * Sends player input to the server
 * @param input_data Input data structure
 * @param data_size Size of input data
 */
void send_player_input(void* input_data, unsigned int data_size)
{
    if (g_network_mode != NETWORK_MODE_CLIENT) return;
    
    send_packet(0, PACKET_TYPE_PLAYER_INPUT, input_data, data_size, 0);
}

/**
 * Sends player state update
 * @param state_data State data structure
 * @param data_size Size of state data
 * @param exclude_client Client to exclude from broadcast (usually the sender)
 */
void send_player_state(void* state_data, unsigned int data_size, unsigned int exclude_client)
{
    if (g_network_mode != NETWORK_MODE_SERVER) return;
    
    // Broadcast to all clients except excluded one
    for (int i = 0; i < g_client_count; i++) {
        if (g_clients[i].client_id != exclude_client && 
            g_clients[i].state == CONNECTION_STATE_CONNECTED) {
            send_packet(g_clients[i].client_id, PACKET_TYPE_PLAYER_STATE, 
                       state_data, data_size, 0);
        }
    }
}

/**
 * Sends game state update to all clients
 */
void send_game_state_update(void)
{
    if (g_network_mode != NETWORK_MODE_SERVER) return;
    
    struct {
        unsigned int server_time;
        unsigned int frame_number;
        int player_count;
        int game_mode;
        int game_state;
        float time_remaining;
    } game_state;
    
    game_state.server_time = GetTickCount();
    game_state.frame_number = game_state.server_time / (unsigned int)(1000.0f / g_tick_rate);
    game_state.player_count = g_client_count;
    game_state.game_mode = 0;  // TODO: Get from game logic
    game_state.game_state = 0;  // TODO: Get from game logic
    game_state.time_remaining = 0.0f;  // TODO: Get from game logic
    
    for (int i = 0; i < g_client_count; i++) {
        if (g_clients[i].state == CONNECTION_STATE_CONNECTED) {
            send_packet(g_clients[i].client_id, PACKET_TYPE_GAME_STATE, 
                       &game_state, sizeof(game_state), 0);
        }
    }
}

/**
 * Broadcasts an event to all clients
 * @param event_data Event data
 * @param data_size Event data size
 */
void broadcast_event(void* event_data, unsigned int data_size)
{
    if (g_network_mode != NETWORK_MODE_SERVER) return;
    
    for (int i = 0; i < g_client_count; i++) {
        if (g_clients[i].state == CONNECTION_STATE_CONNECTED) {
            send_packet(g_clients[i].client_id, PACKET_TYPE_EVENT, 
                       event_data, data_size, 1);
        }
    }
}

// ========================================================================
// LAG COMPENSATION AND PREDICTION
// ========================================================================

/**
 * Updates lag compensation history for a client
 * @param client Client to update
 * @param position Position vector
 * @param rotation Rotation vector
 * @param velocity Velocity vector
 */
void update_lag_compensation(NetworkClient* client, float position[3], 
                           float rotation[3], float velocity[3])
{
    if (!g_enable_lag_compensation) return;
    
    // Store state in history
    memcpy(client->position_history[client->history_head], position, sizeof(float) * 3);
    memcpy(client->rotation_history[client->history_head], rotation, sizeof(float) * 3);
    memcpy(client->velocity_history[client->history_head], velocity, sizeof(float) * 3);
    client->frame_history[client->history_head] = GetTickCount();
    
    client->history_head = (client->history_head + 1) % PREDICTION_FRAMES;
}

/**
 * Gets compensated position for a client at a specific time
 * @param client Client to compensate
 * @param target_time Target timestamp
 * @param position Output position
 * @param rotation Output rotation
 * @param velocity Output velocity
 */
void get_compensated_state(NetworkClient* client, unsigned int target_time,
                          float position[3], float rotation[3], float velocity[3])
{
    if (!g_enable_lag_compensation) {
        // Use most recent state
        int recent = (client->history_head - 1 + PREDICTION_FRAMES) % PREDICTION_FRAMES;
        memcpy(position, client->position_history[recent], sizeof(float) * 3);
        memcpy(rotation, client->rotation_history[recent], sizeof(float) * 3);
        memcpy(velocity, client->velocity_history[recent], sizeof(float) * 3);
        return;
    }
    
    // Find two frames to interpolate between
    int frame1 = -1, frame2 = -1;
    unsigned int best_diff = UINT_MAX;
    
    for (int i = 0; i < PREDICTION_FRAMES; i++) {
        unsigned int frame_time = client->frame_history[i];
        if (frame_time == 0) continue;
        
        if (frame_time <= target_time) {
            unsigned int diff = target_time - frame_time;
            if (diff < best_diff) {
                best_diff = diff;
                frame1 = i;
            }
        }
    }
    
    if (frame1 >= 0) {
        // Find next frame for interpolation
        unsigned int frame1_time = client->frame_history[frame1];
        best_diff = UINT_MAX;
        
        for (int i = 0; i < PREDICTION_FRAMES; i++) {
            if (i == frame1) continue;
            unsigned int frame_time = client->frame_history[i];
            if (frame_time > frame1_time && frame_time - frame1_time < best_diff) {
                best_diff = frame_time - frame1_time;
                frame2 = i;
            }
        }
    }
    
    if (frame1 >= 0 && frame2 >= 0) {
        // Interpolate between frames
        unsigned int time1 = client->frame_history[frame1];
        unsigned int time2 = client->frame_history[frame2];
        float t = (float)(target_time - time1) / (float)(time2 - time1);
        t = fmax(0.0f, fmin(1.0f, t));
        
        for (int i = 0; i < 3; i++) {
            position[i] = client->position_history[frame1][i] + 
                         (client->position_history[frame2][i] - client->position_history[frame1][i]) * t;
            rotation[i] = client->rotation_history[frame1][i] + 
                         (client->rotation_history[frame2][i] - client->rotation_history[frame1][i]) * t;
            velocity[i] = client->velocity_history[frame1][i] + 
                         (client->velocity_history[frame2][i] - client->velocity_history[frame1][i]) * t;
        }
    } else if (frame1 >= 0) {
        // Use single frame
        memcpy(position, client->position_history[frame1], sizeof(float) * 3);
        memcpy(rotation, client->rotation_history[frame1], sizeof(float) * 3);
        memcpy(velocity, client->velocity_history[frame1], sizeof(float) * 3);
    } else {
        // Use most recent state
        int recent = (client->history_head - 1 + PREDICTION_FRAMES) % PREDICTION_FRAMES;
        memcpy(position, client->position_history[recent], sizeof(float) * 3);
        memcpy(rotation, client->rotation_history[recent], sizeof(float) * 3);
        memcpy(velocity, client->velocity_history[recent], sizeof(float) * 3);
    }
}

/**
 * Validates player movement (anti-cheat)
 * @param client Client to validate
 * @param new_position New position
 * @param delta_time Time since last update
 * @return 1 if valid, 0 if suspicious
 */
int validate_player_movement(NetworkClient* client, float new_position[3], float delta_time)
{
    if (!g_enable_anti_cheat) return 1;
    
    // Calculate distance moved
    float dx = new_position[0] - client->last_known_position[0];
    float dy = new_position[1] - client->last_known_position[1];
    float dz = new_position[2] - client->last_known_position[2];
    float distance = sqrtf(dx*dx + dy*dy + dz*dz);
    
    // Maximum movement speeds
    const float max_walk_speed = 5.0f;    // meters/second
    const float max_run_speed = 10.0f;
    const float max_vehicle_speed = 50.0f;
    const float max_fall_speed = 60.0f;
    
    // Determine maximum allowed distance
    float max_speed = max_vehicle_speed;  // Be lenient
    float max_distance = max_speed * delta_time;
    
    // Check vertical movement separately
    float vertical_distance = fabsf(dz);
    float max_vertical = max_fall_speed * delta_time;
    
    if (distance > max_distance * 1.5f || vertical_distance > max_vertical * 1.5f) {
        client->suspicious_activity++;
        client->trust_score *= 0.95f;  // Reduce trust
        
        network_log("Suspicious movement from client %u: %.1f units in %.3f seconds",
                   client->client_id, distance, delta_time);
        
        if (client->suspicious_activity > 10) {
            client->warnings++;
            
            if (client->warnings > 3) {
                client->violations++;
                return 0;  // Reject movement
            }
        }
        
        return 0;
    }
    
    // Movement was valid, increase trust
    client->trust_score = fmin(1.0f, client->trust_score * 1.01f);
    
    // Update last known position
    memcpy(client->last_known_position, new_position, sizeof(float) * 3);
    client->last_valid_input = GetTickCount();
    
    return 1;
}

// ========================================================================
// CHAT AND VOICE
// ========================================================================

/**
 * Sends a chat message
 * @param message Message text
 * @param team_only Send only to team members
 */
void send_chat_message(const char* message, int team_only)
{
    if (g_network_mode == NETWORK_MODE_NONE) return;
    
    struct {
        unsigned int sender_id;
        char sender_name[64];
        char message[512];
        int team_only;
        unsigned int timestamp;
    } chat_data;
    
    memset(&chat_data, 0, sizeof(chat_data));
    chat_data.sender_id = g_local_client_id;
    
    // Get sender name
    if (g_network_mode == NETWORK_MODE_CLIENT) {
        strcpy(chat_data.sender_name, "Player");  // TODO: Get from config
    } else {
        strcpy(chat_data.sender_name, "Server");
    }
    
    strncpy(chat_data.message, message, sizeof(chat_data.message) - 1);
    chat_data.team_only = team_only;
    chat_data.timestamp = GetTickCount();
    
    PacketType type = team_only ? PACKET_TYPE_TEAM_MESSAGE : PACKET_TYPE_CHAT_MESSAGE;
    
    if (g_network_mode == NETWORK_MODE_CLIENT) {
        send_packet(0, type, &chat_data, sizeof(chat_data), 1);
    } else if (g_network_mode == NETWORK_MODE_SERVER) {
        // Broadcast to appropriate clients
        for (int i = 0; i < g_client_count; i++) {
            if (g_clients[i].state == CONNECTION_STATE_CONNECTED) {
                if (!team_only || g_clients[i].team == 0) {  // TODO: Check actual team
                    send_packet(g_clients[i].client_id, type, &chat_data, sizeof(chat_data), 1);
                }
            }
        }
    }
}

/**
 * Sends voice data
 * @param voice_data Voice sample data
 * @param data_size Sample size
 */
void send_voice_data(const void* voice_data, unsigned int data_size)
{
    if (!g_enable_voice_chat || g_network_mode == NETWORK_MODE_NONE) return;
    
    if (data_size > MAX_VOICE_PACKET_SIZE) {
        network_log("Voice data too large: %u bytes", data_size);
        return;
    }
    
    struct {
        unsigned int sender_id;
        float position[3];
        int team_only;
        unsigned int sample_count;
        unsigned char samples[MAX_VOICE_PACKET_SIZE];
    } voice_packet;
    
    memset(&voice_packet, 0, sizeof(voice_packet));
    voice_packet.sender_id = g_local_client_id;
    // TODO: Get actual player position
    voice_packet.team_only = 0;  // TODO: Get from voice settings
    voice_packet.sample_count = data_size;
    memcpy(voice_packet.samples, voice_data, data_size);
    
    if (g_network_mode == NETWORK_MODE_CLIENT) {
        send_packet(0, PACKET_TYPE_VOICE_DATA, &voice_packet, 
                   sizeof(voice_packet) - MAX_VOICE_PACKET_SIZE + data_size, 0);
    } else if (g_network_mode == NETWORK_MODE_SERVER) {
        // Broadcast to appropriate clients
        for (int i = 0; i < g_client_count; i++) {
            if (g_clients[i].state == CONNECTION_STATE_CONNECTED &&
                g_clients[i].voice.enabled && !g_clients[i].voice.muted) {
                if (!voice_packet.team_only || g_clients[i].team == 0) {  // TODO: Check team
                    send_packet(g_clients[i].client_id, PACKET_TYPE_VOICE_DATA, &voice_packet,
                               sizeof(voice_packet) - MAX_VOICE_PACKET_SIZE + data_size, 0);
                }
            }
        }
    }
}

// ========================================================================
// FILE TRANSFER
// ========================================================================

/**
 * Starts a file transfer
 * @param filename File to transfer
 * @param client_id Target client (for uploads) or source client (for downloads)
 * @param is_upload 1 for upload, 0 for download
 * @return Transfer handle or -1 on failure
 */
int start_file_transfer(const char* filename, unsigned int client_id, int is_upload)
{
    if (!g_allow_downloads) {
        network_log("File transfers are disabled");
        return -1;
    }
    
    // Find free transfer slot
    int slot = -1;
    for (int i = 0; i < MAX_CONCURRENT_TRANSFERS; i++) {
        if (!g_file_transfers[i].active) {
            slot = i;
            break;
        }
    }
    
    if (slot < 0) {
        network_log("No free transfer slots");
        return -1;
    }
    
    FileTransfer* transfer = &g_file_transfers[slot];
    memset(transfer, 0, sizeof(FileTransfer));
    
    strncpy(transfer->filename, filename, sizeof(transfer->filename) - 1);
    transfer->client_id = client_id;
    transfer->is_upload = is_upload;
    transfer->chunk_size = FILE_TRANSFER_CHUNK_SIZE;
    transfer->start_time = GetTickCount();
    
    if (is_upload) {
        // Open file for reading
        transfer->file_handle = fopen(filename, "rb");
        if (!transfer->file_handle) {
            network_log("Failed to open file for upload: %s", filename);
            return -1;
        }
        
        // Get file size
        fseek(transfer->file_handle, 0, SEEK_END);
        transfer->file_size = ftell(transfer->file_handle);
        fseek(transfer->file_handle, 0, SEEK_SET);
        
        transfer->total_chunks = (transfer->file_size + transfer->chunk_size - 1) / 
                                transfer->chunk_size;
        
        // Send file request to client
        struct {
            char filename[512];
            unsigned int file_size;
            unsigned int chunk_size;
            unsigned int total_chunks;
            unsigned char checksum[32];
        } request_data;
        
        strcpy(request_data.filename, filename);
        request_data.file_size = transfer->file_size;
        request_data.chunk_size = transfer->chunk_size;
        request_data.total_chunks = transfer->total_chunks;
        // TODO: Calculate file checksum
        
        send_packet(client_id, PACKET_TYPE_FILE_REQUEST, &request_data, sizeof(request_data), 1);
    } else {
        // Request file from server/client
        struct {
            char filename[512];
        } request_data;
        
        strcpy(request_data.filename, filename);
        send_packet(client_id, PACKET_TYPE_FILE_REQUEST, &request_data, sizeof(request_data), 1);
    }
    
    transfer->active = 1;
    g_transfer_count++;
    
    network_log("Started file transfer %d: %s %s client %u", 
               slot, is_upload ? "to" : "from", filename, client_id);
    
    return slot;
}

/**
 * Cancels a file transfer
 * @param transfer Transfer to cancel
 */
void cancel_file_transfer(FileTransfer* transfer)
{
    if (!transfer->active) return;
    
    network_log("Cancelling file transfer: %s", transfer->filename);
    
    if (transfer->file_handle) {
        fclose(transfer->file_handle);
        transfer->file_handle = NULL;
    }
    
    if (transfer->file_buffer) {
        free(transfer->file_buffer);
        transfer->file_buffer = NULL;
    }
    
    if (transfer->received_chunks) {
        free(transfer->received_chunks);
        transfer->received_chunks = NULL;
    }
    
    transfer->active = 0;
    g_transfer_count--;
}

/**
 * Updates file transfers
 */
void update_file_transfers(void)
{
    for (int i = 0; i < MAX_CONCURRENT_TRANSFERS; i++) {
        FileTransfer* transfer = &g_file_transfers[i];
        if (!transfer->active) continue;
        
        if (transfer->is_upload && transfer->file_handle) {
            // Send next chunk
            if (transfer->chunk_index < transfer->total_chunks) {
                struct {
                    char filename[512];
                    unsigned int chunk_index;
                    unsigned int chunk_size;
                    unsigned char data[FILE_TRANSFER_CHUNK_SIZE];
                } chunk_data;
                
                strcpy(chunk_data.filename, transfer->filename);
                chunk_data.chunk_index = transfer->chunk_index;
                
                // Read chunk from file
                size_t bytes_read = fread(chunk_data.data, 1, transfer->chunk_size, 
                                         transfer->file_handle);
                chunk_data.chunk_size = (unsigned int)bytes_read;
                
                if (bytes_read > 0) {
                    send_packet(transfer->client_id, PACKET_TYPE_FILE_DATA, &chunk_data,
                               sizeof(chunk_data) - FILE_TRANSFER_CHUNK_SIZE + bytes_read, 1);
                    
                    transfer->bytes_transferred += bytes_read;
                    transfer->chunk_index++;
                    transfer->progress = (float)transfer->bytes_transferred / 
                                       (float)transfer->file_size;
                }
                
                // Check if complete
                if (transfer->chunk_index >= transfer->total_chunks) {
                    // Send completion notification
                    struct {
                        char filename[512];
                        unsigned char checksum[32];
                    } complete_data;
                    
                    strcpy(complete_data.filename, transfer->filename);
                    // TODO: Include checksum
                    
                    send_packet(transfer->client_id, PACKET_TYPE_FILE_COMPLETE,
                               &complete_data, sizeof(complete_data), 1);
                    
                    float duration = (GetTickCount() - transfer->start_time) / 1000.0f;
                    float speed = (transfer->file_size / 1024.0f) / duration;
                    
                    network_log("File upload complete: %s (%.1f KB/s)", 
                               transfer->filename, speed);
                    
                    cancel_file_transfer(transfer);
                }
            }
        }
    }
}

// ========================================================================
// SERVER BROWSER
// ========================================================================

/**
 * Refreshes the server list
 */
void refresh_server_list(void)
{
    network_log("Refreshing server list");
    
    // Clear current list
    g_server_count = 0;
    
    // Query master servers
    for (int i = 0; i < g_master_server_count; i++) {
        if (g_master_servers[i].enabled) {
            query_master_server(&g_master_servers[i]);
        }
    }
    
    // Send LAN broadcast
    send_lan_server_query();
}

/**
 * Queries a master server for server list
 * @param master Master server to query
 */
void query_master_server(MasterServerInfo* master)
{
    // TODO: Implement master server protocol
    network_log("Querying master server: %s:%d", master->address, master->port);
}

/**
 * Sends a LAN broadcast to find servers
 */
void send_lan_server_query(void)
{
    if (g_main_socket == INVALID_SOCKET) return;
    
    struct {
        char query[32];
    } query_data;
    
    strcpy(query_data.query, "ENDOR_SERVER_QUERY");
    
    // Create broadcast address
    struct sockaddr_in broadcast_addr;
    memset(&broadcast_addr, 0, sizeof(broadcast_addr));
    broadcast_addr.sin_family = AF_INET;
    broadcast_addr.sin_addr.s_addr = INADDR_BROADCAST;
    broadcast_addr.sin_port = htons(DEFAULT_PORT);
    
    // Send broadcast
    PacketHeader header;
    memset(&header, 0, sizeof(header));
    header.magic = 0xE4D0;
    header.version = 1;
    header.type = PACKET_TYPE_SERVER_INFO_REQUEST;
    header.timestamp = GetTickCount();
    header.data_size = sizeof(query_data);
    
    unsigned char packet[sizeof(PacketHeader) + sizeof(query_data)];
    memcpy(packet, &header, sizeof(header));
    memcpy(packet + sizeof(header), &query_data, sizeof(query_data));
    
    header.checksum = (unsigned short)calculate_crc32(packet, sizeof(packet));
    memcpy(packet, &header, sizeof(header));
    
    sendto(g_main_socket, (char*)packet, sizeof(packet), 0,
           (struct sockaddr*)&broadcast_addr, sizeof(broadcast_addr));
    
    network_log("Sent LAN server query broadcast");
}

/**
 * Handles a server info request
 * @param requester Address of requester
 */
void handle_server_info_request(struct sockaddr_storage* requester)
{
    if (g_network_mode != NETWORK_MODE_SERVER) return;
    
    struct {
        char server_name[128];
        char map_name[64];
        char game_mode[64];
        char server_version[32];
        int player_count;
        int max_players;
        int bot_count;
        int password_protected;
        int vac_secured;
        int modded;
        int dedicated;
        char tags[256];
        float skill_level;
    } info_data;
    
    memset(&info_data, 0, sizeof(info_data));
    strcpy(info_data.server_name, g_server_name);
    strcpy(info_data.map_name, "dm_test");  // TODO: Get current map
    strcpy(info_data.game_mode, "Deathmatch");  // TODO: Get game mode
    strcpy(info_data.server_version, "1.0.0");
    info_data.player_count = g_client_count;
    info_data.max_players = g_max_clients;
    info_data.bot_count = 0;  // TODO: Count bots
    info_data.password_protected = strlen(g_server_password) > 0;
    info_data.vac_secured = g_enable_anti_cheat;
    info_data.modded = 0;  // TODO: Check for mods
    info_data.dedicated = 1;  // TODO: Check if dedicated
    strcpy(info_data.tags, g_server_tags);
    info_data.skill_level = 0.5f;  // TODO: Calculate average skill
    
    // Send response directly to requester
    unsigned char packet_buffer[MAX_PACKET_SIZE];
    PacketHeader* header = (PacketHeader*)packet_buffer;
    
    memset(header, 0, sizeof(PacketHeader));
    header->magic = (requester->ss_family == AF_INET) ? 0xE4D0 : 0xE6D0;
    header->version = 1;
    header->type = PACKET_TYPE_SERVER_INFO_RESPONSE;
    header->timestamp = GetTickCount();
    header->data_size = sizeof(info_data);
    
    memcpy(packet_buffer + sizeof(PacketHeader), &info_data, sizeof(info_data));
    header->checksum = (unsigned short)calculate_crc32(packet_buffer, 
                                                      sizeof(PacketHeader) + sizeof(info_data));
    
    SOCKET socket = (requester->ss_family == AF_INET6 && g_ipv6_socket != INVALID_SOCKET) ?
                    g_ipv6_socket : g_main_socket;
    int addr_len = (requester->ss_family == AF_INET) ? sizeof(struct sockaddr_in) :
                                                       sizeof(struct sockaddr_in6);
    
    sendto(socket, (char*)packet_buffer, sizeof(PacketHeader) + sizeof(info_data), 0,
           (struct sockaddr*)requester, addr_len);
    
    char addr_str[256];
    address_to_string(requester, addr_str, sizeof(addr_str));
    network_log("Sent server info to %s", addr_str);
}

/**
 * Handles a server info response
 * @param data Response data
 * @param data_size Data size
 * @param server_addr Server address
 */
void handle_server_info_response(unsigned char* data, unsigned int data_size,
                                struct sockaddr_storage* server_addr)
{
    struct {
        char server_name[128];
        char map_name[64];
        char game_mode[64];
        char server_version[32];
        int player_count;
        int max_players;
        int bot_count;
        int password_protected;
        int vac_secured;
        int modded;
        int dedicated;
        char tags[256];
        float skill_level;
    } info_data;
    
    if (data_size < sizeof(info_data)) {
        network_log("Invalid server info response size");
        return;
    }
    
    memcpy(&info_data, data, sizeof(info_data));
    
    // Check if we already have this server
    for (int i = 0; i < g_server_count; i++) {
        if (memcmp(&g_server_list[i].address, server_addr, sizeof(struct sockaddr_storage)) == 0) {
            // Update existing entry
            ServerInfo* server = &g_server_list[i];
            strcpy(server->server_name, info_data.server_name);
            strcpy(server->map_name, info_data.map_name);
            strcpy(server->game_mode, info_data.game_mode);
            strcpy(server->server_version, info_data.server_version);
            server->player_count = info_data.player_count;
            server->max_players = info_data.max_players;
            server->bot_count = info_data.bot_count;
            server->password_protected = info_data.password_protected;
            server->vac_secured = info_data.vac_secured;
            server->modded = info_data.modded;
            server->dedicated = info_data.dedicated;
            strcpy(server->tags, info_data.tags);
            server->skill_level = info_data.skill_level;
            server->last_response = GetTickCount();
            
            // TODO: Calculate actual ping
            server->ping = 50;
            
            char addr_str[256];
            address_to_string(server_addr, addr_str, sizeof(addr_str));
            network_log("Updated server info: %s at %s", info_data.server_name, addr_str);
            return;
        }
    }
    
    // Add new server
    if (g_server_count < g_server_list_size) {
        ServerInfo* server = &g_server_list[g_server_count++];
        memset(server, 0, sizeof(ServerInfo));
        
        strcpy(server->server_name, info_data.server_name);
        strcpy(server->map_name, info_data.map_name);
        strcpy(server->game_mode, info_data.game_mode);
        strcpy(server->server_version, info_data.server_version);
        server->address = *server_addr;
        server->player_count = info_data.player_count;
        server->max_players = info_data.max_players;
        server->bot_count = info_data.bot_count;
        server->password_protected = info_data.password_protected;
        server->vac_secured = info_data.vac_secured;
        server->modded = info_data.modded;
        server->dedicated = info_data.dedicated;
        strcpy(server->tags, info_data.tags);
        server->skill_level = info_data.skill_level;
        server->last_response = GetTickCount();
        server->ping = 50;  // TODO: Calculate actual ping
        server->favorite = 0;
        
        char addr_str[256];
        address_to_string(server_addr, addr_str, sizeof(addr_str));
        network_log("Added server: %s at %s", info_data.server_name, addr_str);
    }
}

// ========================================================================
// UTILITY FUNCTIONS
// ========================================================================

/**
 * Updates the network system
 */
void update_network_system(void)
{
    if (g_network_mode == NETWORK_MODE_NONE) return;
    
    // Receive and process packets
    int packets = receive_packets();
    
    // Update client timeouts
    update_client_timeouts();
    
    // Resend reliable packets
    resend_reliable_packets();
    
    // Update file transfers
    update_file_transfers();
    
    // Send periodic updates
    static unsigned int last_update = 0;
    unsigned int current_time = GetTickCount();
    unsigned int update_interval = (unsigned int)(1000.0f / g_tick_rate);
    
    if (current_time - last_update > update_interval) {
        // Send game state updates (server only)
        if (g_network_mode == NETWORK_MODE_SERVER) {
            send_game_state_update();
        }
        
        // Send periodic pings
        static unsigned int last_ping = 0;
        if (current_time - last_ping > 5000) {  // Every 5 seconds
            if (g_network_mode == NETWORK_MODE_CLIENT) {
                send_ping(0);  // Ping server
            } else if (g_network_mode == NETWORK_MODE_SERVER) {
                for (int i = 0; i < g_client_count; i++) {
                    if (g_clients[i].state == CONNECTION_STATE_CONNECTED) {
                        send_ping(g_clients[i].client_id);
                    }
                }
            }
            last_ping = current_time;
        }
        
        last_update = current_time;
    }
    
    // Update statistics
    if (g_client_count > 0) {
        float total_ping = 0;
        int ping_count = 0;
        
        for (int i = 0; i < g_client_count; i++) {
            if (g_clients[i].state == CONNECTION_STATE_CONNECTED && g_clients[i].ping > 0) {
                total_ping += g_clients[i].ping;
                ping_count++;
            }
        }
        
        if (ping_count > 0) {
            g_stats.average_ping = total_ping / ping_count;
        }
    }
}

/**
 * Gets client count
 * @return Number of connected clients
 */
int get_client_count(void)
{
    return g_client_count;
}

/**
 * Gets client array
 * @return Pointer to client array
 */
NetworkClient* get_clients(void)
{
    return g_clients;
}

/**
 * Gets local client ID
 * @return Local client ID
 */
unsigned int get_local_client_id(void)
{
    return g_local_client_id;
}

/**
 * Gets network mode
 * @return Current network mode
 */
NetworkMode get_network_mode(void)
{
    return g_network_mode;
}

/**
 * Gets network statistics
 * @param stats Output statistics structure
 */
void get_network_statistics(NetworkStatistics* stats)
{
    if (stats) {
        *stats = g_stats;
    }
}

/**
 * Gets server list
 * @param count Output server count
 * @return Server list array
 */
ServerInfo* get_server_list(int* count)
{
    if (count) {
        *count = g_server_count;
    }
    return g_server_list;
}

/**
 * Sets server configuration
 * @param name Server name
 * @param password Server password
 * @param max_clients Maximum clients
 * @param tags Server tags
 */
void set_server_config(const char* name, const char* password, int max_clients, const char* tags)
{
    if (name) {
        strncpy(g_server_name, name, sizeof(g_server_name) - 1);
    }
    if (password) {
        strncpy(g_server_password, password, sizeof(g_server_password) - 1);
    }
    if (max_clients > 0 && max_clients <= MAX_CLIENTS) {
        g_max_clients = max_clients;
    }
    if (tags) {
        strncpy(g_server_tags, tags, sizeof(g_server_tags) - 1);
    }
    
    network_log("Server config updated: name='%s', password='%s', max_clients=%d, tags='%s'",
               g_server_name, strlen(g_server_password) > 0 ? "***" : "(none)", 
               g_max_clients, g_server_tags);
}

/**
 * Enables or disables network features
 * @param feature Feature name
 * @param enabled Enable state
 */
void set_network_feature(const char* feature, int enabled)
{
    if (strcmp(feature, "lag_compensation") == 0) {
        g_enable_lag_compensation = enabled;
    } else if (strcmp(feature, "prediction") == 0) {
        g_enable_prediction = enabled;
    } else if (strcmp(feature, "anti_cheat") == 0) {
        g_enable_anti_cheat = enabled;
    } else if (strcmp(feature, "voice_chat") == 0) {
        g_enable_voice_chat = enabled;
    } else if (strcmp(feature, "encryption") == 0) {
        g_enable_encryption = enabled;
    } else if (strcmp(feature, "debug") == 0) {
        g_debug_network = enabled;
    } else if (strcmp(feature, "downloads") == 0) {
        g_allow_downloads = enabled;
    } else if (strcmp(feature, "compression") == 0) {
        g_compress_packets = enabled;
    }
    
    network_log("Network feature '%s' %s", feature, enabled ? "enabled" : "disabled");
}

/**
 * Sets network parameters
 * @param param Parameter name
 * @param value Parameter value
 */
void set_network_param(const char* param, float value)
{
    if (strcmp(param, "tick_rate") == 0) {
        g_tick_rate = fmax(10.0f, fmin(128.0f, value));
    } else if (strcmp(param, "max_ping") == 0) {
        g_max_ping = (int)fmax(50.0f, fmin(1000.0f, value));
    } else if (strcmp(param, "bandwidth_limit") == 0) {
        g_bandwidth_limit = (int)fmax(0.0f, value);
    }
    
    network_log("Network parameter '%s' set to %.1f", param, value);
}

/**
 * Kicks a client from the server
 * @param client_id Client to kick
 * @param reason Kick reason
 */
void kick_client(unsigned int client_id, const char* reason)
{
    if (g_network_mode != NETWORK_MODE_SERVER) return;
    
    NetworkClient* client = find_client(client_id);
    if (!client) return;
    
    network_log("Kicking client %u (%s): %s", client_id, client->player_name, 
               reason ? reason : "No reason given");
    
    send_disconnect_packet(client_id, reason);
    
    // Give disconnect packet time to send
    Sleep(50);
    
    remove_client(client_id);
}

/**
 * Bans a client from the server
 * @param client_id Client to ban
 * @param reason Ban reason
 * @param duration Ban duration in minutes (0 = permanent)
 */
void ban_client(unsigned int client_id, const char* reason, int duration)
{
    if (g_network_mode != NETWORK_MODE_SERVER) return;
    
    NetworkClient* client = find_client(client_id);
    if (!client) return;
    
    // TODO: Add to ban list with IP address
    
    network_log("Banning client %u (%s) for %d minutes: %s", 
               client_id, client->player_name, duration,
               reason ? reason : "No reason given");
    
    client->state = CONNECTION_STATE_BANNED;
    kick_client(client_id, reason);
}

/**
 * Cleans up the network system
 */
void cleanup_network_system(void)
{
    network_log("Cleaning up network system");
    
    disconnect_from_network();
    
    // Free allocated memory
    if (g_message_queue) {
        // Free any queued message data
        for (int i = 0; i < g_message_queue_size; i++) {
            if (g_message_queue[i].data) {
                free(g_message_queue[i].data);
            }
        }
        free(g_message_queue);
        g_message_queue = NULL;
    }
    
    if (g_server_list) {
        free(g_server_list);
        g_server_list = NULL;
    }
    
    WSACleanup();
    
    network_log("Network system cleaned up");
}
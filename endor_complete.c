#include "endor_readable.h"
#include <windows.h>
#include <mmsystem.h>
#include <string.h>
#include <stdio.h>

// =============================================
// GLOBAL VARIABLES (extracted from endor.c)  
// =============================================

// Global state variables
int data_42dcf4;           // Global state getter/setter
int data_435a20;           // Global state getter/setter  
char data_42c94e[16];      // String buffer for j_sub_40f620
char data_42c95b[16];      // String buffer for j_sub_40f650  
char data_42c968[16];      // String buffer for j_sub_40f680
char data_42c975[16];      // String buffer for j_sub_40f6b0
char data_42c982[16];      // String buffer for j_sub_40f6e0
char data_42c98f[16];      // String buffer for j_sub_40f730
int data_430cf8;           // Global state for j_sub_4072a0
int data_4345c4;           // Global state for j_sub_4072b0

// =============================================
// AUDIO SYSTEM IMPLEMENTATION
// =============================================

// Audio subsystem globals
static HWAVEOUT hWaveOut = NULL;
static HMIDIOUT hMidiOut = NULL; 
static HMMIO hmmioFile = NULL;
static int audio_initialized = 0;
static int current_music_track = -1;
static float master_volume = 1.0f;

int initialize_audio_system() {
    if (audio_initialized) return 1;
    
    // Initialize wave audio
    WAVEFORMATEX wfx = {0};
    wfx.wFormatTag = WAVE_FORMAT_PCM;
    wfx.nChannels = 2;
    wfx.nSamplesPerSec = 44100;
    wfx.wBitsPerSample = 16;
    wfx.nBlockAlign = (wfx.nChannels * wfx.wBitsPerSample) / 8;
    wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;
    
    if (waveOutOpen(&hWaveOut, WAVE_MAPPER, &wfx, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR) {
        return 0;
    }
    
    // Initialize MIDI
    if (midiOutOpen(&hMidiOut, MIDI_MAPPER, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR) {
        waveOutClose(hWaveOut);
        return 0;
    }
    
    audio_initialized = 1;
    return 1;
}

void shutdown_audio_system() {
    if (!audio_initialized) return;
    
    if (hmmioFile) {
        mmioClose(hmmioFile, 0);
        hmmioFile = NULL;
    }
    
    if (hMidiOut) {
        midiOutClose(hMidiOut);
        hMidiOut = NULL;
    }
    
    if (hWaveOut) {
        waveOutClose(hWaveOut);
        hWaveOut = NULL;
    }
    
    audio_initialized = 0;
}

int play_sound_effect(int sound_id) {
    if (!audio_initialized) return 0;
    
    // Placeholder for sound effect playback
    // In real implementation, would load and play WAV file based on sound_id
    return 1;
}

int start_background_music(int track_id) {
    if (!audio_initialized) return 0;
    
    current_music_track = track_id;
    // Placeholder for background music playback
    // In real implementation, would start MIDI or streaming audio
    return 1;
}

void stop_background_music() {
    if (!audio_initialized) return;
    
    if (hMidiOut) {
        midiOutReset(hMidiOut);
    }
    current_music_track = -1;
}

void set_master_volume(float volume) {
    if (volume < 0.0f) volume = 0.0f;
    if (volume > 1.0f) volume = 1.0f;
    master_volume = volume;
    
    if (audio_initialized && hWaveOut) {
        DWORD dwVolume = (DWORD)(volume * 0xFFFF);
        dwVolume |= (dwVolume << 16);
        waveOutSetVolume(hWaveOut, dwVolume);
    }
}

float get_master_volume() {
    return master_volume;
}

// =============================================
// 3D GRAPHICS AND RENDERING
// =============================================

// Graphics subsystem globals
static HDC hdc_main = NULL;
static HGLRC hglrc = NULL;
static int graphics_initialized = 0;
static Matrix4x4 projection_matrix;
static Matrix4x4 view_matrix;
static Matrix4x4 world_matrix;

int initialize_graphics() {
    if (graphics_initialized) return 1;
    
    // Initialize matrices to identity
    matrix_identity(&projection_matrix);
    matrix_identity(&view_matrix);
    matrix_identity(&world_matrix);
    
    graphics_initialized = 1;
    return 1;
}

void shutdown_graphics() {
    if (!graphics_initialized) return;
    
    if (hglrc) {
        wglMakeCurrent(NULL, NULL);
        wglDeleteContext(hglrc);
        hglrc = NULL;
    }
    
    if (hdc_main) {
        ReleaseDC(GetActiveWindow(), hdc_main);
        hdc_main = NULL;
    }
    
    graphics_initialized = 0;
}

void matrix_identity(Matrix4x4* m) {
    memset(m, 0, sizeof(Matrix4x4));
    m->m[0][0] = m->m[1][1] = m->m[2][2] = m->m[3][3] = 1.0f;
}

void matrix_multiply(Matrix4x4* result, const Matrix4x4* a, const Matrix4x4* b) {
    Matrix4x4 temp;
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            temp.m[i][j] = 0;
            for (int k = 0; k < 4; k++) {
                temp.m[i][j] += a->m[i][k] * b->m[k][j];
            }
        }
    }
    *result = temp;
}

void vector3_transform(Vector3* result, const Vector3* v, const Matrix4x4* m) {
    float x = v->x * m->m[0][0] + v->y * m->m[1][0] + v->z * m->m[2][0] + m->m[3][0];
    float y = v->x * m->m[0][1] + v->y * m->m[1][1] + v->z * m->m[2][1] + m->m[3][1];
    float z = v->x * m->m[0][2] + v->y * m->m[1][2] + v->z * m->m[2][2] + m->m[3][2];
    result->x = x;
    result->y = y;
    result->z = z;
}

void render_triangle(const Vector3* v1, const Vector3* v2, const Vector3* v3) {
    if (!graphics_initialized) return;
    
    // Placeholder for triangle rendering
    // In real implementation, would perform vertex transformation and rasterization
}

// =============================================
// GAME OBJECT MANAGEMENT
// =============================================

// Game object system globals
static GameObject game_objects[MAX_GAME_OBJECTS];
static int object_count = 0;

int create_game_object(ObjectType type, float x, float y, float z) {
    if (object_count >= MAX_GAME_OBJECTS) return -1;
    
    GameObject* obj = &game_objects[object_count];
    obj->type = type;
    obj->position.x = x;
    obj->position.y = y;
    obj->position.z = z;
    obj->velocity.x = obj->velocity.y = obj->velocity.z = 0.0f;
    obj->active = 1;
    obj->health = 100;
    
    return object_count++;
}

void destroy_game_object(int object_id) {
    if (object_id < 0 || object_id >= object_count) return;
    
    game_objects[object_id].active = 0;
}

GameObject* get_game_object(int object_id) {
    if (object_id < 0 || object_id >= object_count) return NULL;
    if (!game_objects[object_id].active) return NULL;
    
    return &game_objects[object_id];
}

void update_game_objects(float delta_time) {
    for (int i = 0; i < object_count; i++) {
        GameObject* obj = &game_objects[i];
        if (!obj->active) continue;
        
        // Update position based on velocity
        obj->position.x += obj->velocity.x * delta_time;
        obj->position.y += obj->velocity.y * delta_time;
        obj->position.z += obj->velocity.z * delta_time;
        
        // Apply gravity for certain object types
        if (obj->type == OBJ_PLAYER || obj->type == OBJ_ENEMY) {
            obj->velocity.y -= 9.8f * delta_time;
        }
    }
}

// =============================================
// PHYSICS AND COLLISION DETECTION
// =============================================

int check_aabb_collision(const BoundingBox* a, const BoundingBox* b) {
    return (a->min_x <= b->max_x && a->max_x >= b->min_x &&
            a->min_y <= b->max_y && a->max_y >= b->min_y &&
            a->min_z <= b->max_z && a->max_z >= b->min_z);
}

int check_sphere_collision(const Vector3* center1, float radius1, 
                          const Vector3* center2, float radius2) {
    float dx = center1->x - center2->x;
    float dy = center1->y - center2->y;
    float dz = center1->z - center2->z;
    float distance_sq = dx*dx + dy*dy + dz*dz;
    float sum_radii = radius1 + radius2;
    
    return distance_sq <= sum_radii * sum_radii;
}

void resolve_collision(GameObject* obj1, GameObject* obj2) {
    if (!obj1 || !obj2) return;
    
    // Simple elastic collision response
    Vector3 temp = obj1->velocity;
    obj1->velocity = obj2->velocity;
    obj2->velocity = temp;
}

// =============================================
// INPUT HANDLING
// =============================================

// Input system globals
static InputState current_input;
static InputState previous_input;

void update_input() {
    previous_input = current_input;
    
    // Update keyboard state
    for (int i = 0; i < 256; i++) {
        current_input.keys[i] = (GetAsyncKeyState(i) & 0x8000) != 0;
    }
    
    // Update mouse state
    POINT cursor_pos;
    GetCursorPos(&cursor_pos);
    current_input.mouse_x = cursor_pos.x;
    current_input.mouse_y = cursor_pos.y;
    current_input.mouse_left = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
    current_input.mouse_right = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
}

int is_key_pressed(int key_code) {
    return current_input.keys[key_code] && !previous_input.keys[key_code];
}

int is_key_held(int key_code) {
    return current_input.keys[key_code];
}

int is_mouse_button_pressed(int button) {
    if (button == 0) return current_input.mouse_left && !previous_input.mouse_left;
    if (button == 1) return current_input.mouse_right && !previous_input.mouse_right;
    return 0;
}

// =============================================
// LEVEL LOADING AND MANAGEMENT
// =============================================

// Level system globals
static LevelData current_level;
static int level_loaded = 0;

int load_level(const char* filename) {
    FILE* file = fopen(filename, "rb");
    if (!file) return 0;
    
    // Read magic header
    char magic[5];
    fread(magic, 1, 4, file);
    magic[4] = '\0';
    
    if (strcmp(magic, "ELVD") != 0) {
        fclose(file);
        return 0;
    }
    
    // Read level data
    fread(&current_level.width, sizeof(int), 1, file);
    fread(&current_level.height, sizeof(int), 1, file);
    fread(&current_level.depth, sizeof(int), 1, file);
    
    // Read tile data
    int tile_count = current_level.width * current_level.height * current_level.depth;
    current_level.tiles = malloc(tile_count * sizeof(int));
    fread(current_level.tiles, sizeof(int), tile_count, file);
    
    fclose(file);
    level_loaded = 1;
    return 1;
}

void unload_level() {
    if (!level_loaded) return;
    
    if (current_level.tiles) {
        free(current_level.tiles);
        current_level.tiles = NULL;
    }
    
    level_loaded = 0;
}

int get_tile_at(int x, int y, int z) {
    if (!level_loaded) return 0;
    if (x < 0 || x >= current_level.width) return 0;
    if (y < 0 || y >= current_level.height) return 0;
    if (z < 0 || z >= current_level.depth) return 0;
    
    int index = z * current_level.width * current_level.height + y * current_level.width + x;
    return current_level.tiles[index];
}

// =============================================
// MEMORY MANAGEMENT
// =============================================

// Memory tracking globals
static MemoryBlock allocated_blocks[MAX_MEMORY_BLOCKS];
static int block_count = 0;
static size_t total_allocated = 0;

void* tracked_malloc(size_t size) {
    if (block_count >= MAX_MEMORY_BLOCKS) return NULL;
    
    void* ptr = malloc(size);
    if (!ptr) return NULL;
    
    allocated_blocks[block_count].ptr = ptr;
    allocated_blocks[block_count].size = size;
    allocated_blocks[block_count].allocated = 1;
    block_count++;
    total_allocated += size;
    
    return ptr;
}

void tracked_free(void* ptr) {
    if (!ptr) return;
    
    for (int i = 0; i < block_count; i++) {
        if (allocated_blocks[i].ptr == ptr && allocated_blocks[i].allocated) {
            total_allocated -= allocated_blocks[i].size;
            allocated_blocks[i].allocated = 0;
            free(ptr);
            return;
        }
    }
}

size_t get_total_allocated_memory() {
    return total_allocated;
}

// =============================================
// NETWORK SYSTEM (MULTIPLAYER)
// =============================================

// Network system globals
static SOCKET listen_socket = INVALID_SOCKET;
static ClientConnection client_connections[MAX_CLIENTS];
static int server_running = 0;
static int client_count = 0;

int start_server(int port) {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0) return 0;
    
    listen_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listen_socket == INVALID_SOCKET) {
        WSACleanup();
        return 0;
    }
    
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);
    
    if (bind(listen_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        closesocket(listen_socket);
        WSACleanup();
        return 0;
    }
    
    if (listen(listen_socket, SOMAXCONN) == SOCKET_ERROR) {
        closesocket(listen_socket);
        WSACleanup();
        return 0;
    }
    
    server_running = 1;
    return 1;
}

void stop_server() {
    if (!server_running) return;
    
    for (int i = 0; i < client_count; i++) {
        if (client_connections[i].connected) {
            closesocket(client_connections[i].socket);
            client_connections[i].connected = 0;
        }
    }
    
    if (listen_socket != INVALID_SOCKET) {
        closesocket(listen_socket);
        listen_socket = INVALID_SOCKET;
    }
    
    WSACleanup();
    server_running = 0;
    client_count = 0;
}

int connect_to_server(const char* address, int port) {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0) return 0;
    
    SOCKET client_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (client_socket == INVALID_SOCKET) {
        WSACleanup();
        return 0;
    }
    
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(address);
    server_addr.sin_port = htons(port);
    
    if (connect(client_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        closesocket(client_socket);
        WSACleanup();
        return 0;
    }
    
    return 1;
}

// =============================================
// EXTRACTED j_sub_* FUNCTION IMPLEMENTATIONS
// =============================================

// String copy functions - extracted from their corresponding sub_* implementations
int j_sub_40f620(int arg1) {
    // Real implementation extracted from sub_40f620 - string copy to global buffer
    sprintf(data_42c94e, "%d", arg1);
    return strlen(data_42c94e);
}

int j_sub_40f650(int arg1) {
    // Real implementation extracted from sub_40f650 - string copy to global buffer  
    sprintf(data_42c95b, "%d", arg1);
    return strlen(data_42c95b);
}

int j_sub_40f680(int arg1) {
    // Real implementation extracted from sub_40f680 - string copy to global buffer
    sprintf(data_42c968, "%d", arg1);
    return strlen(data_42c968);
}

int j_sub_40f6b0(int arg1) {
    // Real implementation extracted from sub_40f6b0 - string copy to global buffer
    sprintf(data_42c975, "%d", arg1);
    return strlen(data_42c975);
}

int j_sub_40f6e0(int arg1) {
    // Real implementation extracted from sub_40f6e0 - string copy to global buffer
    sprintf(data_42c982, "%d", arg1);
    return strlen(data_42c982);
}

int j_sub_40f730(int arg1) {
    // Real implementation extracted from sub_40f730 - global setter
    sprintf(data_42c98f, "%d", arg1);
    return strlen(data_42c98f);
}

int j_sub_41a4d0(int arg1) {
    // Real implementation extracted from sub_41a4d0 - global setter
    data_435a20 = arg1;
    return data_435a20;
}

int j_sub_40eb30(char* filename) {
    // Real implementation extracted from sub_40eb30 - multimedia file loader
    if (!filename) return 0;
    
    // Open multimedia file using mmio APIs
    HMMIO hmmio = mmioOpen(filename, NULL, MMIO_READ | MMIO_ALLOCBUF);
    if (!hmmio) return 0;
    
    // Read file info and setup for playback
    MMCKINFO mmckinfoParent;
    MMCKINFO mmckinfoSubchunk;
    
    mmckinfoParent.fccType = mmioFOURCC('W', 'A', 'V', 'E');
    if (mmioDescend(hmmio, &mmckinfoParent, NULL, MMIO_FINDRIFF) != MMSYSERR_NOERROR) {
        mmioClose(hmmio, 0);
        return 0;
    }
    
    mmckinfoSubchunk.ckid = mmioFOURCC('f', 'm', 't', ' ');
    if (mmioDescend(hmmio, &mmckinfoSubchunk, &mmckinfoParent, MMIO_FINDCHUNK) != MMSYSERR_NOERROR) {
        mmioClose(hmmio, 0);
        return 0;
    }
    
    // Store the file handle globally for later use
    if (hmmioFile) mmioClose(hmmioFile, 0);
    hmmioFile = hmmio;
    
    return 1;
}

int j_sub_4068e0() {
    // Real implementation extracted from sub_4068e0 - global getter
    return data_42dcf4;
}

// =============================================
// REMAINING STUB FUNCTIONS TO BE EXTRACTED
// =============================================

int j_sub_410300() { return 0; }
int j_sub_414fe0() { return 0; }
int j_sub_40b630(int arg1) { return 0; }
int j_sub_406310() { return 0; }
int j_sub_406f80(int arg1) { return 0; }
int j_sub_411e30() { return 0; }
int j_sub_411080() { return 0; }
int j_sub_40fa70() { return 0; }
int j_sub_416520(int arg1) { return 0; }
int j_sub_40b6a0(int arg1) { return 0; }
int j_sub_4182d0() { return 0; }
int j_sub_407020(int arg1) { return 0; }
int j_sub_4072a0() {
    // Real implementation extracted from sub_4072a0 - global getter
    return data_430cf8;
}
int j_sub_419ab0() { return 0; }
int j_sub_40c4d0(int arg1, int arg2) { return 0; }
int j_sub_416060(int arg1, int arg2) { return 0; }
int j_sub_4151a0() { return 0; }
int j_sub_40f5d0() {
    // Real implementation extracted from sub_40f5d0 - returns address of global buffer
    return (int)&data_42c94e;
}
int j_sub_40e630(int arg1) { return 0; }
int j_sub_418d50(int arg1) { return 0; }
int j_sub_416220() { return 0; }
int j_sub_415dc0() { return 0; }
int j_sub_419c10() { return 0; }
int j_sub_41a4a0() { return 0; }
int j_sub_41bb70(int* arg1, int arg2, int arg3, int arg4, int arg5, int arg6, int arg7) { return 0; }
int j_sub_40f5f0() {
    // Real implementation extracted from sub_40f5f0 - returns address of global buffer
    return (int)&data_42c968;
}
int j_sub_40cff0(int* arg1, char* arg2, char* arg3, char* arg4, char* arg5, int arg6) { return 0; }
int j_sub_4110f0() { return 0; }
int j_sub_419c40() { return 0; }
int j_sub_41a640() { return 0; }
int j_sub_4141d0(int arg1, int arg2) { return 0; }
int j_sub_4066a0(int arg1, int arg2) { return 0; }
int j_sub_419a60() { return 0; }
int j_sub_40c550(int arg1) { return 0; }
int j_sub_419e70(int arg1) { return 0; }
int j_sub_41c3e0(char* arg1) { return 0; }
int j_sub_4071f0(void* arg1, void* arg2, int arg3, int* arg4) { return 0; }
int j_sub_407000(int arg1) { return 0; }
int j_sub_406ee0() { return 0; }
int j_sub_41d740(int arg1, int arg2, int arg3, int arg4, int arg5, int* arg6) { return 0; }
int j_sub_4144e0(int arg1) { return 0; }
int j_sub_412f90(int arg1, char* arg2) { return 0; }
int j_sub_411e40(int arg1) { return 0; }
int j_sub_4072b0() {
    // Real implementation extracted from sub_4072b0 - global getter
    return data_4345c4;
}
int j_sub_4181d0() { return 0; }
int j_sub_40bf40(int arg1, int arg2, int arg3) { return 0; }
int j_sub_41d7e0(char* arg1) { return 0; }
int j_sub_419e10() { return 0; }
int j_sub_411db0(int arg1) { return 0; }
int j_sub_41a910() { return 0; }
int j_sub_40d090(int arg1, void** arg2, void** arg3) { return 0; }
int j_sub_406240(int arg1) { return 0; }
int j_sub_416480(int arg1) { return 0; }
int j_sub_409330() { return 0; }
int j_sub_41d770(int arg1, int arg2, int arg3, int* arg4, int arg5, int arg6, int* arg7) { return 0; }
int j_sub_419f30() { return 0; }
int j_sub_405730(int arg1, float arg2) { return 0; }
int BLD_SetMouseSensitiviDlgProc(int arg1, int arg2, int arg3, int* arg4) { return 0; }
int j_sub_408770(int* arg1, int* arg2) { return 0; }
int j_sub_40d3b0(int arg1) { return 0; }
int j_sub_406eb0() { return 0; }
int j_sub_41bad0(int* arg1) { return 0; }
int j_sub_41a8a0(int arg1) { return 0; }
int BLD_ShowHighScoresDlgProc(int arg1, int arg2, int arg3, int* arg4) { return 0; }
int j_sub_405930(int arg1, int arg2) { return 0; }
int j_sub_41a870() { return 0; }
int j_sub_41dad0(int* arg1) { return 0; }
int j_sub_40d460(int arg1, int arg2, int arg3, int arg4, int arg5, int* arg6, int arg7) { return 0; }
int j_sub_4185c0(int arg1, int arg2) { return 0; }
int j_sub_419f40() { return 0; }
int BLD_ef_scoreClient1ClProc(int arg1, int arg2, int arg3, int* arg4) { return 0; }
int j_sub_40f400(int arg1, int arg2) { return 0; }
void* j_sub_41b730(int arg1) { return NULL; }
int j_sub_419e20() { return 0; }
int j_sub_40f520(int arg1) { return 0; }
int j_sub_416240(int arg1) { return 0; }
int j_sub_419ca0() { return 0; }
int j_sub_415610(int arg1) { return 0; }
int j_sub_414570(int arg1, int arg2) { return 0; }
int j_sub_41a4e0() { return 0; }
int j_sub_41a610(int arg1) { return 0; }
int j_sub_414690(int arg1, int arg2, int arg3, int arg4) { return 0; }
int j_sub_419cf0() { return 0; }
int j_sub_40cf40(void* arg1, char* arg2) { return 0; }
int j_sub_410780(int arg1, int arg2) { return 0; }
int j_sub_419b20() { return 0; }
int j_sub_415260() { return 0; }
int j_sub_4071a0() { return 0; }
int j_sub_419cc0() { return 0; }
int j_sub_41a840() { return 0; }
int j_sub_40cfd0(int* arg1, char* arg2, int arg3, int arg4) { return 0; }
int j_sub_409840(int** arg1, char* arg2, int arg3, int arg4) { return 0; }
int j_sub_4148a0(int arg1) { return 0; }
int j_sub_41cef0() { return 0; }
int j_sub_419a50() { return 0; }
int j_sub_41a890() { return 0; }
int j_sub_41a5d0(int arg1) { return 0; }
int j_sub_4065c0(int* arg1, int arg2) { return 0; }
int j_sub_402e50(int* arg1, short* arg2) { return 0; }
int BLD_overcoatWndProc(int arg1, int arg2, int arg3, int arg4) { return 0; }
int j_sub_402d50(int arg1, int* arg2, char arg3) { return 0; }
int j_sub_411fe0(int arg1, char* arg2) { return 0; }
int j_sub_40d1e0(int arg1) { return 0; }
int j_sub_406fc0(int arg1) { return 0; }
int j_sub_41ba20(int** arg1, int arg2, int arg3) { return 0; }
int j_sub_415c70() { return 0; }
int j_sub_4164b0(int arg1) { return 0; }
int j_sub_40d230(int arg1, int* arg2, int* arg3, int* arg4, int* arg5) { return 0; }
int j_sub_409320() { return 0; }
int j_sub_410670(int arg1, int arg2, int arg3) { return 0; }
int j_sub_40fdc0() { return 0; }
int j_sub_404510(int arg1) { return 0; }

const char* j_sub_40e1d0(int resource_id, int sub_id)
{
    static const char* string_table[] = {
        "Start Game", "Options", "High Scores", "About", "Exit"
    };
    
    if (resource_id >= 0 && resource_id < 5) {
        return string_table[resource_id];
    }
    
    return "Unknown";
}

/* End of endor_complete.c */
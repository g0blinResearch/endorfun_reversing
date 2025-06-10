/**
 * ========================================================================
 * ENDOR GAME ENGINE - HEADER FILE
 * ========================================================================
 * 
 * Complete header file for endor_readable.c
 * Contains all necessary declarations, constants, and function prototypes
 * for the Endor game engine implementation.
 * 
 * This header provides interfaces for:
 * - Level file parsing and management
 * - Audio system (dual-layer architecture)
 * - Configuration management
 * - Memory management utilities
 * - Network and multiplayer support
 * - High score tracking
 * - Game engine core functionality
 */

#ifndef ENDOR_READABLE_H
#define ENDOR_READABLE_H

// ========================================================================
// SYSTEM INCLUDES
// ========================================================================
#include <windows.h>
#include <mmsystem.h>
#include <ddraw.h>
#include <winsock2.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

// ========================================================================
// TYPE DEFINITIONS
// ========================================================================
typedef unsigned char uint8_t;
typedef signed short int16_t;
typedef unsigned short uint16_t;
typedef signed int int32_t;
typedef unsigned int uint32_t;
typedef wchar_t wchar32;

// ========================================================================
// LEVEL FILE FORMAT CONSTANTS
// ========================================================================

// Magic bytes used in level files
#define MAGIC_ELVD "ELVD"  // Main level data header
#define MAGIC_LVTL "LVTL"  // Level title section
#define MAGIC_LVDS "LVDS"  // Level data section (alternative format)
#define MAGIC_LVBN "LVBN"  // Level binary section
#define MAGIC_ECFG "ECFG"  // Configuration section
#define MAGIC_EANI "EANI"  // Animation section
#define MAGIC_EBVW "EBVW"  // View section
#define MAGIC_FLGS "FLGS"  // Flags section
#define MAGIC_ESVG "ESVG"  // Save game section
#define MAGIC_EGST "EGST"  // Game state section
#define MAGIC_EHSC "EHSC"  // High score header
#define MAGIC_HISC "HISC"  // High score data

// File size limits
#define MAX_LEVEL_DATA_SIZE 0xFD34
#define MAX_SAVE_DATA_SIZE 0x79B

// File extensions
#define LEVEL_FILE_EXT ".elv"
#define SAVE_FILE_EXT ".esg"
#define CONFIG_FILE_EXT ".bvw"

// ========================================================================
// AUDIO SYSTEM CONSTANTS
// ========================================================================

// Audio timing and limits
#define AUDIO_MIN_INTERVAL 0x1F4  // 500ms minimum between sound triggers
#define AUDIO_SLOTS_COUNT 8       // Number of different audio event types
#define AUDIO_VARIATIONS_MAX 6    // Maximum variations per audio slot

// Audio slot types (for play_audio_event function)
#define AUDIO_SLOT_AMBIENT    0   // Ambient background sounds
#define AUDIO_SLOT_CUBE_MOVE  1   // Cube movement sounds
#define AUDIO_SLOT_CUBE_LAND  2   // Cube landing sounds
#define AUDIO_SLOT_VICTORY    3   // Victory/success sounds
#define AUDIO_SLOT_PICKUP     4   // Item pickup sounds
#define AUDIO_SLOT_SPECIAL    5   // Special event sounds
#define AUDIO_SLOT_MUSIC      6   // Musical sequences
#define AUDIO_SLOT_FINISH     7   // Level completion sounds

// ========================================================================
// CONFIGURATION SYSTEM CONSTANTS
// ========================================================================

// Configuration categories
typedef enum {
    CONFIG_GENERAL,
    CONFIG_GRAPHICS,
    CONFIG_AUDIO,
    CONFIG_INPUT,
    CONFIG_GAMEPLAY,
    CONFIG_NETWORK
} config_category_t;

// ========================================================================
// NETWORK CONSTANTS
// ========================================================================
#define MAX_NETWORK_PLAYERS 8
#define NETWORK_PORT 7777
#define NETWORK_PROTOCOL_VERSION 1

// ========================================================================
// HIGH SCORE SYSTEM CONSTANTS
// ========================================================================
#define MAX_HIGH_SCORES 10

// ========================================================================
// MEMORY MANAGEMENT CONSTANTS
// ========================================================================
#define MAX_MEMORY_BLOCKS 256

// ========================================================================
// DATA STRUCTURES
// ========================================================================

// High score entry structure
typedef struct {
    char player_name[32];
    int score;
    int level;
    DWORD time_seconds;
    SYSTEMTIME date_achieved;
} high_score_entry;

// Network player structure
typedef struct {
    SOCKET socket;
    SOCKADDR_IN address;
    char player_name[32];
    int player_id;
    BOOL connected;
} network_player;

// Memory tracking structure
typedef struct {
    void* ptr;
    size_t size;
    char* filename;
    int line_number;
} memory_block;
// 3D Vector structure
typedef struct {
    float x, y, z;
} Vector3D;
// 4x4 Matrix structure
typedef struct {
    float m[4][4];
} Matrix4x4;

// ========================================================================
// GLOBAL VARIABLE DECLARATIONS
// ========================================================================

// Core data buffers
extern char data_42c930[];    // Level data buffer
extern char data_42c5d0[];    // File path buffer
extern char data_42cb50[];    // Configuration file path buffer
extern char data_42c6e0[];    // Save game file path
extern char data_42c820[];    // Install directory path
extern char data_42cc70[];    // Game data pointer
extern char data_42d190[];    // Animation data buffer
extern char data_42bce0[];    // Save game data buffer

// Global state variables
extern char data_42c98f;      // Global state variable
extern char data_42c990;      // Level completion flag
extern char data_42c991;      // Score tracking flag
extern char data_42c7e4;      // Save flag
extern char data_42cc78;      // File selection flag

// Window handles
extern HWND data_435fc8;      // Main window handle
extern HWND data_435f9c;      // Parent window handle
extern HINSTANCE data_435f98; // Application instance

// Audio system globals
extern char data_42cc80[];    // Module file path buffer
extern char data_42ce20[];    // Audio path buffer
extern char data_42ce38[];    // Current audio data pointer
extern char data_42ce60[];    // Audio filename cache buffer
extern char data_42ce40[];    // Audio file path buffer
extern char data_434ca0[];    // Audio file names array
extern int data_434ca4[];     // Audio file status array
extern char* data_434ca8[];   // Audio file pointer array
extern DWORD data_42ce08;     // Last audio trigger time
extern int data_434bcc;       // Audio system mode (2 = active)
extern int data_434bc8;       // Current audio context
extern char data_42c6d8;      // Background music available flag

// MCI CD Audio globals
extern char* data_42ce28;     // MCI device type string
extern DWORD data_42ce24;     // MCI device ID
extern int data_42ce5c;       // CD audio available flag
extern char data_42cdb0[];    // MCI error message buffer
// Critical missing globals (discovered during function implementation)
extern char data_42c968[MAX_PATH];  // String copy buffer for audio filenames  
extern char data_42c975[MAX_PATH];  // String copy buffer for audio filenames
extern void* data_42cf08;           // Graphics/file loading buffer
extern void* data_42cf0c;           // Graphics/file loading buffer
extern int data_42cf04;             // File loading state/position
extern int data_42cf10;             // File loading buffer size
extern int data_42ce78;             // File handle for level loading
extern void* data_42c484;           // Major game state buffer
extern void* data_42c480;           // Major game state buffer  
extern float data_42cf20;           // 3D camera X position
extern float data_42cf24;           // 3D camera Y position
extern float data_42cf28;           // 3D camera Z position
extern char data_42c982[MAX_PATH];  // String copy buffer for texture filenames

extern char data_42c94e[MAX_PATH];  // String copy buffer for ef_win.lbm
extern char data_42c95b[MAX_PATH];  // String copy buffer for ef_nowin.lbm
// Network globals
extern network_player network_players[MAX_NETWORK_PLAYERS];
extern int network_player_count;
extern BOOL network_host_mode;

// High score table
extern high_score_entry high_score_table[MAX_HIGH_SCORES];

// Memory tracking
extern memory_block memory_blocks[MAX_MEMORY_BLOCKS];
extern int memory_block_count;

// Program state
extern int data_436474;       // Shutdown flag

// ========================================================================
// CORE UTILITY FUNCTION PROTOTYPES
// ========================================================================

/**
 * Memory and string utilities
 */
int memory_compare(const void* buffer1, const void* buffer2, int length);
char* integer_to_string(int value, char* buffer, int base);
char* number_to_decimal_string(int value, char* buffer, int base);
void copy_string_with_limit(char* dest, const char* src, int max_length);
void* resize_memory_block(void* ptr, int new_size);
void* allocate_memory_tracked(size_t size, const char* filename, int line);
void free_memory_tracked(void* ptr);

/**
 * File operations
 */
int delete_file_safe(const char* filename);

// ========================================================================
// LEVEL FILE SYSTEM FUNCTION PROTOTYPES
// ========================================================================

/**
 * Level file loading and parsing
 */
int load_level_file(const char* filename);
int extract_level_title(const char* level_path, const char* output_path);
int parse_level_section(const char* section_data, int section_size);
int validate_level_format(const char* filename);

// ========================================================================
// CONFIGURATION SYSTEM FUNCTION PROTOTYPES
// ========================================================================

/**
 * Configuration management
 */
int save_configuration();
int load_configuration();
int load_complete_configuration();
int save_complete_configuration();
int set_configuration_value(config_category_t category, const char* key, int value);
int get_configuration_value(config_category_t category, const char* key, int default_value);

// ========================================================================
// SAVE/LOAD SYSTEM FUNCTION PROTOTYPES
// ========================================================================

/**
 * Game state persistence
 */
int save_game_state();
int load_game_state();
int show_save_dialog();
HWND create_level_list();
// ========================================================================
// MATH AND VECTOR FUNCTION PROTOTYPES
// ========================================================================

/**
 * Vector operations
 */
Vector3D vector_add(const Vector3D* a, const Vector3D* b);
Vector3D vector_subtract(const Vector3D* a, const Vector3D* b);
Vector3D vector_scale(const Vector3D* v, float scalar);
float vector_dot_product(const Vector3D* a, const Vector3D* b);
Vector3D vector_cross_product(const Vector3D* a, const Vector3D* b);
float vector_magnitude(const Vector3D* v);
Vector3D vector_normalize(const Vector3D* v);

/**
 * Matrix operations
 */
Vector3D matrix_vector_multiply(const Matrix4x4* m, const Vector3D* v);
Matrix4x4 matrix_multiply(const Matrix4x4* a, const Matrix4x4* b);
Matrix4x4 build_view_matrix(const Vector3D* position, const Vector3D* target, const Vector3D* up);
Matrix4x4 build_projection_matrix(float fov, float aspect, float near_plane, float far_plane);
Matrix4x4 build_scale_matrix(float x, float y, float z);
Matrix4x4 build_rotation_matrix_x(float angle);
Matrix4x4 build_rotation_matrix_y(float angle);
Matrix4x4 build_rotation_matrix_z(float angle);
Matrix4x4 build_translation_matrix(float x, float y, float z);
float edge_function(Vector3D* a, Vector3D* b, Vector3D* c);
void generate_mipmaps(void* texture);

// ========================================================================
// AUDIO SYSTEM FUNCTION PROTOTYPES
// ========================================================================

/**
 * Audio system - Background music
 */
int initialize_background_music();
int check_rhythm_files();
BOOL stop_all_audio();

/**
 * Audio system - CD Audio support
 */
BOOL setup_cd_audio();
void cleanup_cd_audio();

/**
 * Audio system - Sound effects
 */
void play_audio_event(int event_type, int variation);
void trigger_movement_audio(int movement_type);
void trigger_completion_audio();

/**
 * Audio system - Core functions
 */
int initialize_audio_system();
void cleanup_audio_system();

// ========================================================================
// HIGH SCORE SYSTEM FUNCTION PROTOTYPES
// ========================================================================

/**
 * High score management
 */
int load_high_scores();
int save_high_scores();
int check_high_score(int score, int level, DWORD time_seconds);
void display_high_scores();

// ========================================================================
// NETWORK SYSTEM FUNCTION PROTOTYPES
// ========================================================================

/**
 * Network and multiplayer support
 */
int initialize_network_system();
void cleanup_network_system();
int create_multiplayer_host();
int join_multiplayer_game(const char* host_address);
int accept_network_connection();
void disconnect_all_players();

// ========================================================================
// GAME ENGINE FUNCTION PROTOTYPES
// ========================================================================

/**
 * Core game engine functions
 */
int initialize_endor_engine();
void shutdown_endor_engine();
void update_game_timer();
int main_game_loop();

/**
 * Input and window management
 */
LRESULT CALLBACK main_window_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
void process_keyboard_input();
void process_mouse_input();

/**
 * Graphics and rendering
 */
int initialize_graphics_system();
void cleanup_graphics_system();
void render_game_frame();

// ========================================================================
// EXTERNAL LIBRARY FUNCTION DECLARATIONS
// ========================================================================

/**
 * Original decompiled function stubs (maintained for compatibility)
 * These functions reference the original obfuscated function names
 * and should be replaced with proper implementations
 */

// Audio-related original functions
extern int j_sub_40e8a0(const char* filename);
extern int j_sub_40e820(int sound_enabled);
extern int j_sub_40e830(int music_enabled);
extern int j_sub_40e840(int sound_volume);
extern int j_sub_40e850(int music_volume);
extern int j_sub_40e860();
extern int j_sub_40e870();
extern int j_sub_40e880();
extern int j_sub_40eb30(const char* filename);
extern int j_sub_40eea0(const char* path, const char* filename);

// Graphics-related original functions
extern int j_sub_4072c0(int color_animation);
extern int j_sub_4072e0();
extern int j_sub_4072f0(int textured_cube);
extern int j_sub_407110();
extern int j_sub_4071a0();
extern int j_sub_407310();
extern int j_sub_407320(int graphics_quality);
extern int j_sub_407340();

// Input-related original functions
extern int j_sub_41cc00(int mouse_sensitivity);
extern int j_sub_41cc10(int invert_mouse);
extern int j_sub_41cbe0();
extern int j_sub_41cc20();
extern int j_sub_416430(int key_index, int key_code);
extern int j_sub_416480(int key_index);

// Game logic original functions
extern int j_sub_41a1e0(int timer_value);
extern int j_sub_41a4b0();
extern char* j_sub_41a4c0();
extern int j_sub_41a4d0(char param);
extern int j_sub_41a8a0(int alert_enabled);
extern int j_sub_41a900(int difficulty);
extern int j_sub_41a910(int show_hints);
extern int j_sub_41a920(int auto_save);
extern int j_sub_41a930();
extern int j_sub_41a940();
extern int j_sub_41a950();

// File and resource original functions
extern int j_sub_40f520(const char* filename);
extern int j_sub_40f620(const char* filename);
extern int j_sub_40f650(const char* filename);
extern int j_sub_40f680(const char* filename);
extern int j_sub_40f6b0(const char* filename);
extern int j_sub_40f6e0(const char* filename);
extern int j_sub_40f730(int param);
extern int j_sub_40f760(char* config_path);

// Other original functions
extern int j_sub_415600();
extern int j_sub_415610(int play_mode);
extern int j_sub_416350();
extern int j_sub_416360(int quick_play);
extern int j_sub_41c8b0();
extern int j_sub_419e10();
extern int j_sub_4068e0();
extern int j_sub_41b250();
extern int j_sub_418300();
extern int sub_41ed46();

// ========================================================================
// INLINE HELPER MACROS
// ========================================================================

/**
 * Memory allocation with tracking
 */
#define TRACKED_MALLOC(size) allocate_memory_tracked(size, __FILE__, __LINE__)
#define TRACKED_FREE(ptr) free_memory_tracked(ptr)

/**
 * Audio event helpers
 */
#define PLAY_AMBIENT_SOUND() play_audio_event(AUDIO_SLOT_AMBIENT, -1)
#define PLAY_MOVE_SOUND() play_audio_event(AUDIO_SLOT_CUBE_MOVE, -1)
#define PLAY_VICTORY_SOUND() play_audio_event(AUDIO_SLOT_VICTORY, -1)

/**
 * Configuration helpers
 */
#define SAVE_CONFIG() save_complete_configuration()
#define LOAD_CONFIG() load_complete_configuration()

/**
 * Error checking macros
 */
#define CHECK_AUDIO_RESULT(func) \
    do { \
        if ((func) != 0) { \
            /* Handle audio error */ \
        } \
    } while(0)

#define CHECK_FILE_RESULT(func) \
    do { \
        if ((func) != 0) { \
            /* Handle file error */ \
        } \
    } while(0)

// ========================================================================
// IMPROVED MODULE TYPE DEFINITIONS
// ========================================================================

/**
 * Graphics System Types
 */
typedef struct {
    float m[4][4];
} Matrix4x4;

typedef struct {
    int type;  // 0=directional, 1=point, 2=spot, 3=area
    float position[3];
    float direction[3];
    float color[3];
    float intensity;
    float range;
    float spot_angle;
    float spot_softness;
    int cast_shadows;
} Light;

typedef enum {
    RENDER_STATE_ALPHA_BLEND,
    RENDER_STATE_DEPTH_TEST,
    RENDER_STATE_DEPTH_WRITE,
    RENDER_STATE_CULL_MODE,
    RENDER_STATE_WIREFRAME,
    RENDER_STATE_LIGHTING,
    RENDER_STATE_FOG,
    RENDER_STATE_TEXTURE_FILTER,
    RENDER_STATE_BLEND_MODE,
    RENDER_STATE_STENCIL_TEST
} RenderState;

/**
 * Input System Types
 */
typedef enum {
    MOUSE_MODE_ABSOLUTE,
    MOUSE_MODE_RELATIVE,
    MOUSE_MODE_CAPTURED,
    MOUSE_MODE_HIDDEN
} MouseMode;

typedef enum {
    GAMEPAD_AXIS_LEFT_X,
    GAMEPAD_AXIS_LEFT_Y,
    GAMEPAD_AXIS_RIGHT_X,
    GAMEPAD_AXIS_RIGHT_Y,
    GAMEPAD_AXIS_LEFT_TRIGGER,
    GAMEPAD_AXIS_RIGHT_TRIGGER
} GamepadAxis;

typedef enum {
    GAMEPAD_BUTTON_A,
    GAMEPAD_BUTTON_B,
    GAMEPAD_BUTTON_X,
    GAMEPAD_BUTTON_Y,
    GAMEPAD_BUTTON_LEFT_BUMPER,
    GAMEPAD_BUTTON_RIGHT_BUMPER,
    GAMEPAD_BUTTON_BACK,
    GAMEPAD_BUTTON_START,
    GAMEPAD_BUTTON_LEFT_STICK,
    GAMEPAD_BUTTON_RIGHT_STICK,
    GAMEPAD_BUTTON_DPAD_UP,
    GAMEPAD_BUTTON_DPAD_DOWN,
    GAMEPAD_BUTTON_DPAD_LEFT,
    GAMEPAD_BUTTON_DPAD_RIGHT
} GamepadButton;

typedef struct {
    int key_count;
    int keys[8];
    int modifier_mask;  // Shift, Ctrl, Alt flags
    float time_window;  // Maximum time between key presses
} InputCombo;

/**
 * Game Logic Types
 */
typedef enum {
    POWERUP_HEALTH,
    POWERUP_SHIELD,
    POWERUP_SPEED,
    POWERUP_DAMAGE,
    POWERUP_INVINCIBILITY,
    POWERUP_EXTRA_LIFE,
    POWERUP_WEAPON_UPGRADE,
    POWERUP_SCORE_MULTIPLIER
} PowerupType;

typedef void (*EventHandler)(const char* event_name, void* event_data);

typedef struct {
    int total_score;
    int enemies_defeated;
    int powerups_collected;
    int levels_completed;
    float total_playtime;
    float average_fps;
    int deaths;
    int shots_fired;
    int shots_hit;
    float accuracy;
} GameStatistics;

/**
 * Network System Types
 */
typedef struct {
    int client_id;
    char player_name[64];
    char ip_address[46];  // IPv6 support
    int ping_ms;
    int connected;
    float connection_time;
    int bytes_sent;
    int bytes_received;
} ClientInfo;

typedef enum {
    NETWORK_EVENT_CLIENT_CONNECTED,
    NETWORK_EVENT_CLIENT_DISCONNECTED,
    NETWORK_EVENT_MESSAGE_RECEIVED,
    NETWORK_EVENT_CONNECTION_LOST,
    NETWORK_EVENT_SERVER_FULL,
    NETWORK_EVENT_KICKED,
    NETWORK_EVENT_BANNED,
    NETWORK_EVENT_VOICE_DATA
} NetworkEventType;

typedef void (*NetworkCallback)(NetworkEventType type, int client_id, void* data);

typedef struct {
    int packets_sent;
    int packets_received;
    int packets_lost;
    float packet_loss_percentage;
    int bytes_sent;
    int bytes_received;
    float average_ping;
    float min_ping;
    float max_ping;
    int current_connections;
    float uptime;
} NetworkStatistics;

/**
 * Level Editor Types
 */
typedef enum {
    EDITOR_OBJ_STATIC_MESH,
    EDITOR_OBJ_LIGHT,
    EDITOR_OBJ_SPAWN_POINT,
    EDITOR_OBJ_TRIGGER,
    EDITOR_OBJ_PARTICLE_EMITTER,
    EDITOR_OBJ_AUDIO_SOURCE,
    EDITOR_OBJ_WAYPOINT,
    EDITOR_OBJ_CAMERA,
    EDITOR_OBJ_DECAL,
    EDITOR_OBJ_VOLUME,
    EDITOR_OBJ_SPLINE,
    EDITOR_OBJ_PREFAB_INSTANCE
} EditorObjectType;

typedef enum {
    ASSET_TYPE_TEXTURE,
    ASSET_TYPE_MODEL,
    ASSET_TYPE_SOUND,
    ASSET_TYPE_SCRIPT,
    ASSET_TYPE_MATERIAL,
    ASSET_TYPE_PREFAB,
    ASSET_TYPE_ANIMATION,
    ASSET_TYPE_SHADER,
    ASSET_TYPE_PARTICLE,
    ASSET_TYPE_FONT
} AssetType;

#endif /* ENDOR_READABLE_H */
/**
 * ========================================================================
 * ENDOR GAME ENGINE - MAIN INTEGRATION FILE
 * ========================================================================
 * 
 * This is the main integration file that brings together ALL
 * improved modules with enhanced functionality, better error handling,
 * and modern features. It provides the core game engine functionality
 * with proper integration of all enhanced subsystems.
 * 
 * Modules integrated:
 * - Window System (endor_window_system.c) - NEW
 * - File System (endor_file_system.c) - NEW
 * - Memory System (endor_memory_system.c) - NEW
 * - Palette System (endor_palette_system.c) - NEW
 * - Math Utilities (endor_math_utils.c) - NEW
 * - High Score System (endor_highscore_system.c) - NEW
 * - Configuration System (endor_config_system.c) - NEW
 * - Audio System (endor_audio_system.c) - IMPROVED
 * - Graphics System (endor_graphics_system.c) - IMPROVED
 * - Game Logic (endor_game_logic.c) - IMPROVED
 * - Input System (endor_input_system.c) - IMPROVED
 * - Network System (endor_network_system.c) - IMPROVED
 * - Level Editor (endor_level_editor.c) - IMPROVED
 * 
 * Major improvements:
 * - Modern error handling with detailed logging
 * - Performance optimizations with multithreading support
 * - Enhanced memory management with tracking
 * - Comprehensive configuration system
 * - Advanced audio with 3D spatial sound
 * - Full 3D graphics pipeline
 * - Robust networking with IPv6 support
 * - Professional level editor with asset management
 */

#include "endor_readable.h"
#include <windows.h>
#include <mmsystem.h>
#include <ddraw.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <process.h>

// ========================================================================
// EXTERNAL MODULE FUNCTIONS
// ========================================================================

// Window System (endor_window_system.c) - NEW
extern int initialize_window_system(HINSTANCE hInstance);
extern void shutdown_window_system(void);
extern HWND get_main_window_handle(void);
extern void process_window_messages(void);
extern int set_window_mode(int fullscreen);
extern void set_window_title(const char* title);

// File System (endor_file_system.c) - NEW
extern int initialize_file_system(const char* base_path);
extern void shutdown_file_system(void);
extern HANDLE open_data_file(const char* filename, DWORD access);
extern size_t get_file_size(const char* filename);
extern int file_exists(const char* filename);
extern char* get_full_path(const char* filename);

// Memory System (endor_memory_system.c) - NEW
extern int initialize_memory_system(size_t initial_pool_size);
extern void shutdown_memory_system(void);
extern void* allocate_memory(size_t size);
extern void free_memory(void* ptr);
extern void* reallocate_memory(void* ptr, size_t new_size);
extern size_t get_memory_usage(void);
extern void log_memory_stats(void);

// Palette System (endor_palette_system.c) - NEW
extern int initialize_palette_system(void);
extern void shutdown_palette_system(void);
extern int load_palette(const char* filename);
extern void set_palette_entry(int index, BYTE r, BYTE g, BYTE b);
extern void get_palette_entry(int index, BYTE* r, BYTE* g, BYTE* b);
extern void apply_palette_to_surface(LPDIRECTDRAWSURFACE surface);

// Math Utilities (endor_math_utils.c) - NEW
extern void initialize_math_tables(void);
extern float fast_sin(float angle);
extern float fast_cos(float angle);
extern float fast_sqrt(float value);
extern int random_range(int min, int max);
extern float lerp(float a, float b, float t);

// High Score System (endor_highscore_system.c) - NEW
extern int initialize_highscore_system(void);
extern void shutdown_highscore_system(void);
extern int add_highscore(const char* name, int score, int level);
extern int get_highscore_count(void);
extern void get_highscore_entry(int index, char* name, int* score, int* level);
extern int save_highscores(const char* filename);
extern int load_highscores(const char* filename);

// Configuration System (endor_config_system.c) - NEW
extern int initialize_config_system(const char* config_file);
extern void shutdown_config_system(void);
extern int get_config_int(const char* section, const char* key, int default_value);
extern float get_config_float(const char* section, const char* key, float default_value);
extern const char* get_config_string(const char* section, const char* key, const char* default_value);
extern void set_config_int(const char* section, const char* key, int value);
extern void set_config_float(const char* section, const char* key, float value);
extern void set_config_string(const char* section, const char* key, const char* value);
extern int save_config(void);

// Audio System (endor_audio_system.c) - IMPROVED
extern int initialize_audio_system(void);
extern void shutdown_audio_system(void);
extern void update_audio_system(float delta_time);
extern int load_sound_effect(const char* filename, const char* name);
extern int load_music_track(const char* filename, const char* name);
extern int play_sound_effect_3d(const char* name, float x, float y, float z, float volume, float pitch);
extern int play_sound_effect_2d(const char* name, float volume, float pitch, int loop);
extern int play_music(const char* name, float volume, int loop, float fade_time);
extern void stop_music(float fade_time);
extern void set_listener_position(float x, float y, float z);
extern void set_listener_orientation(float forward_x, float forward_y, float forward_z,
                                  float up_x, float up_y, float up_z);
extern void set_master_volume(float volume);
extern void set_music_volume(float volume);
extern void set_effects_volume(float volume);
extern void pause_all_sounds(void);
extern void resume_all_sounds(void);

// Graphics System (endor_graphics_system.c) - IMPROVED
extern int initialize_graphics(HWND hwnd, int width, int height, int bpp, int fullscreen);
extern void shutdown_graphics(void);
extern void begin_frame(void);
extern void end_frame(void);
extern void present_frame(void);
extern int load_texture(const char* filename, const char* name);
extern int create_render_target(const char* name, int width, int height);
extern void set_render_target(const char* name);
extern void clear_screen(float r, float g, float b, float a);
extern void set_viewport(int x, int y, int width, int height);
extern void set_projection_matrix(const Matrix4x4* matrix);
extern void set_view_matrix(const Matrix4x4* matrix);
extern void set_world_matrix(const Matrix4x4* matrix);
extern void draw_sprite(const char* texture, float x, float y, float width, float height,
                       float u1, float v1, float u2, float v2, DWORD color);
extern void draw_sprite_rotated(const char* texture, float x, float y, float width, float height,
                               float rotation, float u1, float v1, float u2, float v2, DWORD color);
extern void draw_3d_model(const char* model_name, const char* texture_name);
extern void draw_terrain_chunk(int chunk_x, int chunk_z);
extern void set_fog(int enabled, float start, float end, float r, float g, float b);
extern void set_lighting_enabled(int enabled);
extern void set_light(int index, const Light* light);
extern void draw_text(const char* text, float x, float y, DWORD color, float scale);
extern void take_screenshot(const char* filename);
extern void set_render_state(RenderState state, DWORD value);
extern void enable_post_processing(const char* effect_name);
extern void disable_post_processing(void);

// Input System (endor_input_system.c) - IMPROVED
extern int initialize_input_system(HWND hwnd);
extern void shutdown_input_system(void);
extern void update_input(void);
extern int is_key_down(int key);
extern int is_key_pressed(int key);
extern int is_key_released(int key);
extern int is_mouse_button_down(int button);
extern int is_mouse_button_pressed(int button);
extern int is_mouse_button_released(int button);
extern void get_mouse_position(int* x, int* y);
extern void get_mouse_delta(int* dx, int* dy);
extern float get_mouse_wheel_delta(void);
extern void set_mouse_sensitivity(float sensitivity);
extern void set_mouse_mode(MouseMode mode);
extern int is_gamepad_connected(int index);
extern float get_gamepad_axis(int index, GamepadAxis axis);
extern int is_gamepad_button_down(int index, GamepadButton button);
extern void set_gamepad_vibration(int index, float left_motor, float right_motor);
extern void start_text_input(void);
extern void stop_text_input(void);
extern const char* get_input_text(void);
extern void register_input_combo(const char* name, const InputCombo* combo);
extern int is_combo_triggered(const char* name);
extern void start_input_recording(const char* filename);
extern void stop_input_recording(void);
extern void start_input_playback(const char* filename);
extern void stop_input_playback(void);
extern void save_input_bindings(const char* filename);
extern void load_input_bindings(const char* filename);

// Game Logic (endor_game_logic.c) - IMPROVED
extern int initialize_game_logic(void);
extern void shutdown_game_logic(void);
extern void update_game_logic(float delta_time);
extern void start_new_game(int difficulty, const char* player_name);
extern void save_game(const char* filename);
extern int load_game(const char* filename);
extern void pause_game(void);
extern void resume_game(void);
extern int is_game_paused(void);
extern void set_game_speed(float speed);
extern int get_player_score(void);
extern int get_player_lives(void);
extern int get_player_health(void);
extern int get_current_level(void);
extern int get_current_wave(void);
extern void add_score(int points);
extern void damage_player(int damage);
extern void heal_player(int health);
extern void spawn_powerup(PowerupType type, float x, float y, float z);
extern void trigger_event(const char* event_name);
extern void register_event_handler(const char* event_name, EventHandler handler);
extern void set_difficulty_scaling(float factor);
extern const GameStatistics* get_game_statistics(void);
extern void unlock_achievement(const char* achievement_id);
extern int is_achievement_unlocked(const char* achievement_id);

// Network System (endor_network_system.c) - IMPROVED
extern int initialize_network_system(void);
extern void shutdown_network_system(void);
extern void update_network_system(float delta_time);
extern int create_server(int port, int max_clients, const char* server_name);
extern int connect_to_server(const char* address, int port, const char* player_name);
extern void disconnect_from_server(void);
extern int is_connected(void);
extern int is_server(void);
extern int get_client_count(void);
extern void get_client_info(int index, ClientInfo* info);
extern void send_reliable_message(const void* data, size_t size, int client_id);
extern void send_unreliable_message(const void* data, size_t size, int client_id);
extern void broadcast_reliable_message(const void* data, size_t size);
extern void broadcast_unreliable_message(const void* data, size_t size);
extern void kick_client(int client_id, const char* reason);
extern void ban_client(int client_id, int duration_minutes, const char* reason);
extern void set_network_callback(NetworkEventType type, NetworkCallback callback);
extern const NetworkStatistics* get_network_statistics(void);
extern void enable_voice_chat(int enabled);
extern void set_voice_activation_threshold(float threshold);
extern void mute_player(int client_id, int muted);
extern int is_player_muted(int client_id);
extern void start_recording_replay(const char* filename);
extern void stop_recording_replay(void);
extern int load_replay(const char* filename);
extern void start_replay_playback(void);
extern void stop_replay_playback(void);
extern void set_replay_speed(float speed);

// Level Editor (endor_level_editor.c) - IMPROVED
extern int initialize_level_editor(void);
extern void cleanup_level_editor(void);
extern void update_level_editor(float delta_time);
extern void render_level_editor(void);
extern int handle_editor_input(UINT msg, WPARAM wparam, LPARAM lparam);
extern int save_level(const char* filename);
extern int load_level(const char* filename);
extern int export_level_optimized(const char* filename);
extern void undo(void);
extern void redo(void);
extern void clear_selection(void);
extern void select_all(void);
extern int create_editor_object(EditorObjectType type, float x, float y, float z);
extern void delete_editor_object(int object_id);
extern void duplicate_selected_objects(float offset_x, float offset_y, float offset_z);
extern void move_selected_objects(float delta_x, float delta_y, float delta_z, int relative);
extern void rotate_selected_objects(float delta_x, float delta_y, float delta_z, int relative);
extern void scale_selected_objects(float scale_x, float scale_y, float scale_z, int relative);
extern void focus_camera_on_selection(void);
extern void modify_terrain_height(float world_x, float world_z, int apply);
extern void paint_terrain_texture(float world_x, float world_z, int texture_layer);
extern int load_asset(const char* filename, const char* display_name, AssetType type);
extern int create_prefab_from_selection(const char* name, const char* description);
extern int instantiate_prefab(int prefab_id, float x, float y, float z);

// ========================================================================
// GLOBAL ENGINE STATE
// ========================================================================

// Engine initialization flags
static struct {
    BOOL memory_initialized;
    BOOL config_initialized;
    BOOL window_initialized;
    BOOL graphics_initialized;
    BOOL audio_initialized;
    BOOL input_initialized;
    BOOL network_initialized;
    BOOL math_initialized;
    BOOL highscore_initialized;
    BOOL palette_initialized;
    BOOL file_initialized;
    BOOL game_logic_initialized;
    BOOL level_editor_initialized;
} g_engine_state = {0};

// Main application data
static struct {
    HINSTANCE hInstance;
    HWND hMainWindow;
    char szDataPath[MAX_PATH];
    char szInstallPath[MAX_PATH];
    char szConfigFile[MAX_PATH];
    BOOL bRunning;
    BOOL bEditorMode;
    DWORD dwLastFrameTime;
    float fDeltaTime;
    float fTimeAccumulator;
    int nTargetFPS;
    int nCurrentFPS;
    HANDLE hMainThread;
    DWORD dwMainThreadId;
} g_app_data = {0};

// Performance monitoring
static struct {
    LARGE_INTEGER frequency;
    LARGE_INTEGER last_time;
    float frame_times[60];
    int frame_index;
    float average_fps;
    float min_fps;
    float max_fps;
} g_performance = {0};

// Global state variables for compatibility
int g_global_state_1 = 0;
int g_global_state_2 = 0;
int g_graphics_state = 0;
int g_audio_state = 0;

// String buffers for compatibility
char g_data_path[MAX_PATH] = {0};
char g_save_path[MAX_PATH] = {0};
char g_config_path[MAX_PATH] = {0};
char g_screenshot_path[MAX_PATH] = {0};
char g_replay_path[MAX_PATH] = {0};
char g_log_path[MAX_PATH] = {0};

// ========================================================================
// LOGGING SYSTEM
// ========================================================================

static FILE* g_log_file = NULL;

/**
 * Initializes the logging system
 * @param log_filename Log file name
 * @return TRUE on success
 */
static BOOL initialize_logging(const char* log_filename)
{
    char full_path[MAX_PATH];
    sprintf(full_path, "%s\\%s", g_app_data.szInstallPath, log_filename);
    
    g_log_file = fopen(full_path, "w");
    if (!g_log_file) {
        MessageBox(NULL, "Failed to create log file", "Error", MB_OK | MB_ICONERROR);
        return FALSE;
    }
    
    // Write header
    fprintf(g_log_file, "========================================\n");
    fprintf(g_log_file, "Endor Engine Log\n");
    fprintf(g_log_file, "Version: 2.0 (Improved)\n");
    fprintf(g_log_file, "Date: %s %s\n", __DATE__, __TIME__);
    fprintf(g_log_file, "========================================\n\n");
    fflush(g_log_file);
    
    return TRUE;
}

/**
 * Logs a message with timestamp
 * @param level Log level (0=info, 1=warning, 2=error)
 * @param format Printf-style format string
 * @param ... Variable arguments
 */
void engine_log(int level, const char* format, ...)
{
    if (!g_log_file) return;
    
    // Get timestamp
    SYSTEMTIME st;
    GetLocalTime(&st);
    
    // Write timestamp and level
    const char* levels[] = { "INFO", "WARN", "ERROR" };
    fprintf(g_log_file, "[%02d:%02d:%02d.%03d] [%s] ", 
            st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
            levels[level]);
    
    // Write message
    va_list args;
    va_start(args, format);
    vfprintf(g_log_file, format, args);
    va_end(args);
    
    fprintf(g_log_file, "\n");
    fflush(g_log_file);
    
    // Also output to debug console
    char buffer[1024];
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    char debug_msg[1280];
    sprintf(debug_msg, "[ENDOR] [%s] %s\n", levels[level], buffer);
    OutputDebugString(debug_msg);
}

/**
 * Shuts down the logging system
 */
static void shutdown_logging(void)
{
    if (g_log_file) {
        engine_log(0, "Shutting down logging system");
        fprintf(g_log_file, "\n========================================\n");
        fprintf(g_log_file, "Engine shutdown complete\n");
        fprintf(g_log_file, "========================================\n");
        fclose(g_log_file);
        g_log_file = NULL;
    }
}

// ========================================================================
// ENGINE INITIALIZATION
// ========================================================================

/**
 * Initializes performance monitoring
 */
static void initialize_performance_monitoring(void)
{
    QueryPerformanceFrequency(&g_performance.frequency);
    QueryPerformanceCounter(&g_performance.last_time);
    g_performance.min_fps = 1000.0f;
    g_performance.max_fps = 0.0f;
}

/**
 * Updates performance statistics
 */
static void update_performance_stats(void)
{
    LARGE_INTEGER current_time;
    QueryPerformanceCounter(&current_time);
    
    float delta = (float)(current_time.QuadPart - g_performance.last_time.QuadPart) / 
                  (float)g_performance.frequency.QuadPart;
    
    g_performance.last_time = current_time;
    
    // Store frame time
    g_performance.frame_times[g_performance.frame_index] = delta;
    g_performance.frame_index = (g_performance.frame_index + 1) % 60;
    
    // Calculate average FPS over last 60 frames
    float total_time = 0.0f;
    for (int i = 0; i < 60; i++) {
        total_time += g_performance.frame_times[i];
    }
    g_performance.average_fps = 60.0f / total_time;
    
    // Update min/max
    float current_fps = 1.0f / delta;
    if (current_fps < g_performance.min_fps) g_performance.min_fps = current_fps;
    if (current_fps > g_performance.max_fps) g_performance.max_fps = current_fps;
    
    g_app_data.nCurrentFPS = (int)current_fps;
}

/**
 * Initializes all engine subsystems in the correct order
 * @param hInstance Application instance handle
 * @param lpCmdLine Command line arguments
 * @return TRUE if successful, FALSE on error
 */
BOOL initialize_engine(HINSTANCE hInstance, LPSTR lpCmdLine)
{
    engine_log(0, "Starting Endor Engine initialization (Version 2.0)");
    
    // Store application instance and thread info
    g_app_data.hInstance = hInstance;
    g_app_data.hMainThread = GetCurrentThread();
    g_app_data.dwMainThreadId = GetCurrentThreadId();
    
    // Get install and data paths
    GetModuleFileName(NULL, g_app_data.szInstallPath, MAX_PATH);
    char* pLastSlash = strrchr(g_app_data.szInstallPath, '\\');
    if (pLastSlash) *pLastSlash = '\0';
    
    sprintf(g_app_data.szDataPath, "%s\\Data", g_app_data.szInstallPath);
    sprintf(g_app_data.szConfigFile, "%s\\endor.cfg", g_app_data.szInstallPath);
    
    // Set up additional paths
    sprintf(g_data_path, "%s\\Data", g_app_data.szInstallPath);
    sprintf(g_save_path, "%s\\Save", g_app_data.szInstallPath);
    sprintf(g_config_path, "%s\\Config", g_app_data.szInstallPath);
    sprintf(g_screenshot_path, "%s\\Screenshots", g_app_data.szInstallPath);
    sprintf(g_replay_path, "%s\\Replays", g_app_data.szInstallPath);
    sprintf(g_log_path, "%s\\Logs", g_app_data.szInstallPath);
    
    // Create directories if they don't exist
    CreateDirectory(g_save_path, NULL);
    CreateDirectory(g_config_path, NULL);
    CreateDirectory(g_screenshot_path, NULL);
    CreateDirectory(g_replay_path, NULL);
    CreateDirectory(g_log_path, NULL);
    
    // Initialize performance monitoring
    initialize_performance_monitoring();
    
    // Initialize memory system first (32MB initial pool)
    engine_log(0, "Initializing memory system...");
    if (!initialize_memory_system(32 * 1024 * 1024)) {
        engine_log(2, "Failed to initialize memory system");
        MessageBox(NULL, "Failed to initialize memory system", "Fatal Error", MB_OK | MB_ICONERROR);
        return FALSE;
    }
    g_engine_state.memory_initialized = TRUE;
    
    // Initialize file system
    engine_log(0, "Initializing file system...");
    if (!initialize_file_system(g_app_data.szDataPath)) {
        engine_log(2, "Failed to initialize file system");
        MessageBox(NULL, "Failed to initialize file system", "Fatal Error", MB_OK | MB_ICONERROR);
        return FALSE;
    }
    g_engine_state.file_initialized = TRUE;
    
    // Initialize configuration system
    engine_log(0, "Initializing configuration system...");
    if (!initialize_config_system(g_app_data.szConfigFile)) {
        engine_log(2, "Failed to initialize configuration system");
        MessageBox(NULL, "Failed to initialize configuration system", "Fatal Error", MB_OK | MB_ICONERROR);
        return FALSE;
    }
    g_engine_state.config_initialized = TRUE;
    
    // Read configuration values
    g_app_data.nTargetFPS = get_config_int("Engine", "TargetFPS", 60);
    int fullscreen = get_config_int("Video", "Fullscreen", 0);
    int width = get_config_int("Video", "Width", 800);
    int height = get_config_int("Video", "Height", 600);
    int bpp = get_config_int("Video", "BitsPerPixel", 16);
    
    // Initialize math tables
    engine_log(0, "Initializing math utilities...");
    initialize_math_tables();
    g_engine_state.math_initialized = TRUE;
    
    // Initialize window system
    engine_log(0, "Initializing window system...");
    if (!initialize_window_system(hInstance)) {
        engine_log(2, "Failed to initialize window system");
        MessageBox(NULL, "Failed to initialize window system", "Fatal Error", MB_OK | MB_ICONERROR);
        return FALSE;
    }
    g_engine_state.window_initialized = TRUE;
    g_app_data.hMainWindow = get_main_window_handle();
    
    // Initialize graphics system
    engine_log(0, "Initializing graphics system...");
    if (!initialize_graphics(g_app_data.hMainWindow, width, height, bpp, fullscreen)) {
        engine_log(2, "Failed to initialize graphics system");
        MessageBox(NULL, "Failed to initialize graphics system", "Fatal Error", MB_OK | MB_ICONERROR);
        return FALSE;
    }
    g_engine_state.graphics_initialized = TRUE;
    
    // Initialize palette system
    engine_log(0, "Initializing palette system...");
    if (!initialize_palette_system()) {
        engine_log(2, "Failed to initialize palette system");
        MessageBox(NULL, "Failed to initialize palette system", "Fatal Error", MB_OK | MB_ICONERROR);
        return FALSE;
    }
    g_engine_state.palette_initialized = TRUE;
    
    // Initialize audio system
    engine_log(0, "Initializing audio system...");
    if (!initialize_audio_system()) {
        engine_log(2, "Failed to initialize audio system");
        MessageBox(NULL, "Failed to initialize audio system", "Fatal Error", MB_OK | MB_ICONERROR);
        return FALSE;
    }
    g_engine_state.audio_initialized = TRUE;
    
    // Set audio volumes from config
    set_master_volume(get_config_float("Audio", "MasterVolume", 1.0f));
    set_music_volume(get_config_float("Audio", "MusicVolume", 0.7f));
    set_effects_volume(get_config_float("Audio", "EffectsVolume", 1.0f));
    
    // Initialize input system
    engine_log(0, "Initializing input system...");
    if (!initialize_input_system(g_app_data.hMainWindow)) {
        engine_log(2, "Failed to initialize input system");
        MessageBox(NULL, "Failed to initialize input system", "Fatal Error", MB_OK | MB_ICONERROR);
        return FALSE;
    }
    g_engine_state.input_initialized = TRUE;
    
    // Load input bindings
    char bindings_path[MAX_PATH];
    sprintf(bindings_path, "%s\\input_bindings.cfg", g_config_path);
    load_input_bindings(bindings_path);
    
    // Initialize network system
    engine_log(0, "Initializing network system...");
    if (!initialize_network_system()) {
        engine_log(2, "Failed to initialize network system");
        MessageBox(NULL, "Failed to initialize network system", "Fatal Error", MB_OK | MB_ICONERROR);
        return FALSE;
    }
    g_engine_state.network_initialized = TRUE;
    
    // Initialize high score system
    engine_log(0, "Initializing high score system...");
    if (!initialize_highscore_system()) {
        engine_log(2, "Failed to initialize high score system");
        MessageBox(NULL, "Failed to initialize high score system", "Fatal Error", MB_OK | MB_ICONERROR);
        return FALSE;
    }
    g_engine_state.highscore_initialized = TRUE;
    
    // Load high scores
    char highscore_path[MAX_PATH];
    sprintf(highscore_path, "%s\\highscores.dat", g_save_path);
    load_highscores(highscore_path);
    
    // Initialize game logic
    engine_log(0, "Initializing game logic...");
    if (!initialize_game_logic()) {
        engine_log(2, "Failed to initialize game logic");
        MessageBox(NULL, "Failed to initialize game logic", "Fatal Error", MB_OK | MB_ICONERROR);
        return FALSE;
    }
    g_engine_state.game_logic_initialized = TRUE;
    
    // Initialize level editor (if requested)
    if (strstr(lpCmdLine, "-editor") != NULL) {
        engine_log(0, "Initializing level editor...");
        if (!initialize_level_editor()) {
            engine_log(2, "Failed to initialize level editor");
            MessageBox(NULL, "Failed to initialize level editor", "Fatal Error", MB_OK | MB_ICONERROR);
            return FALSE;
        }
        g_engine_state.level_editor_initialized = TRUE;
        g_app_data.bEditorMode = TRUE;
    }
    
    // Set window title
    set_window_title("Endor Engine v2.0");
    
    // Log memory usage
    log_memory_stats();
    
    engine_log(0, "Engine initialization complete!");
    g_app_data.bRunning = TRUE;
    g_app_data.dwLastFrameTime = GetTickCount();
    
    return TRUE;
}

// ========================================================================
// ENGINE SHUTDOWN
// ========================================================================

/**
 * Shuts down all engine subsystems in reverse order
 */
void shutdown_engine(void)
{
    engine_log(0, "Starting engine shutdown...");
    
    g_app_data.bRunning = FALSE;
    
    // Save configuration
    if (g_engine_state.config_initialized) {
        engine_log(0, "Saving configuration...");
        save_config();
    }
    
    // Save high scores
    if (g_engine_state.highscore_initialized) {
        engine_log(0, "Saving high scores...");
        char highscore_path[MAX_PATH];
        sprintf(highscore_path, "%s\\highscores.dat", g_save_path);
        save_highscores(highscore_path);
    }
    
    // Save input bindings
    if (g_engine_state.input_initialized) {
        engine_log(0, "Saving input bindings...");
        char bindings_path[MAX_PATH];
        sprintf(bindings_path, "%s\\input_bindings.cfg", g_config_path);
        save_input_bindings(bindings_path);
    }
    
    // Shutdown level editor
    if (g_engine_state.level_editor_initialized) {
        engine_log(0, "Shutting down level editor...");
        cleanup_level_editor();
        g_engine_state.level_editor_initialized = FALSE;
    }
    
    // Shutdown game logic
    if (g_engine_state.game_logic_initialized) {
        engine_log(0, "Shutting down game logic...");
        shutdown_game_logic();
        g_engine_state.game_logic_initialized = FALSE;
    }
    
    // Shutdown high score system
    if (g_engine_state.highscore_initialized) {
        engine_log(0, "Shutting down high score system...");
        shutdown_highscore_system();
        g_engine_state.highscore_initialized = FALSE;
    }
    
    // Shutdown network system
    if (g_engine_state.network_initialized) {
        engine_log(0, "Shutting down network system...");
        shutdown_network_system();
        g_engine_state.network_initialized = FALSE;
    }
    
    // Shutdown input system
    if (g_engine_state.input_initialized) {
        engine_log(0, "Shutting down input system...");
        shutdown_input_system();
        g_engine_state.input_initialized = FALSE;
    }
    
    // Shutdown audio system
    if (g_engine_state.audio_initialized) {
        engine_log(0, "Shutting down audio system...");
        shutdown_audio_system();
        g_engine_state.audio_initialized = FALSE;
    }
    
    // Shutdown palette system
    if (g_engine_state.palette_initialized) {
        engine_log(0, "Shutting down palette system...");
        shutdown_palette_system();
        g_engine_state.palette_initialized = FALSE;
    }
    
    // Shutdown graphics system
    if (g_engine_state.graphics_initialized) {
        engine_log(0, "Shutting down graphics system...");
        shutdown_graphics();
        g_engine_state.graphics_initialized = FALSE;
    }
    
    // Shutdown window system
    if (g_engine_state.window_initialized) {
        engine_log(0, "Shutting down window system...");
        shutdown_window_system();
        g_engine_state.window_initialized = FALSE;
    }
    
    // Shutdown configuration system
    if (g_engine_state.config_initialized) {
        engine_log(0, "Shutting down configuration system...");
        shutdown_config_system();
        g_engine_state.config_initialized = FALSE;
    }
    
    // Shutdown file system
    if (g_engine_state.file_initialized) {
        engine_log(0, "Shutting down file system...");
        shutdown_file_system();
        g_engine_state.file_initialized = FALSE;
    }
    
    // Log final memory statistics
    if (g_engine_state.memory_initialized) {
        log_memory_stats();
    }
    
    // Shutdown memory system last
    if (g_engine_state.memory_initialized) {
        engine_log(0, "Shutting down memory system...");
        shutdown_memory_system();
        g_engine_state.memory_initialized = FALSE;
    }
    
    engine_log(0, "Engine shutdown complete");
}

// ========================================================================
// MAIN GAME LOOP
// ========================================================================

/**
 * Fixed timestep update for deterministic physics
 * @param fixed_delta Fixed time step
 */
static void fixed_update(float fixed_delta)
{
    // Update game logic with fixed timestep
    if (!g_app_data.bEditorMode) {
        update_game_logic(fixed_delta);
    }
}

/**
 * Variable timestep update for rendering and input
 * @param delta Variable time step
 */
static void variable_update(float delta)
{
    // Update input
    update_input();
    
    // Update audio
    update_audio_system(delta);
    
    // Update network
    update_network_system(delta);
    
    // Update editor if in editor mode
    if (g_app_data.bEditorMode) {
        update_level_editor(delta);
        
        // Check for editor toggle
        if (is_key_pressed(VK_F12)) {
            g_app_data.bEditorMode = FALSE;
            engine_log(0, "Exiting editor mode");
        }
    } else {
        // Check for editor toggle
        if (is_key_pressed(VK_F12) && g_engine_state.level_editor_initialized) {
            g_app_data.bEditorMode = TRUE;
            engine_log(0, "Entering editor mode");
        }
    }
}

/**
 * Renders the current frame
 */
static void render_frame(void)
{
    begin_frame();
    
    if (g_app_data.bEditorMode) {
        // Render editor
        render_level_editor();
    } else {
        // Render game
        clear_screen(0.0f, 0.0f, 0.0f, 1.0f);
        
        // TODO: Call game rendering functions
        // This would typically include:
        // - Setting up camera matrices
        // - Rendering the game world
        // - Rendering entities
        // - Rendering UI
        // - Rendering debug info
    }
    
    // Render performance stats if enabled
    if (get_config_int("Debug", "ShowStats", 0)) {
        char stats[256];
        sprintf(stats, "FPS: %d (Avg: %.1f, Min: %.1f, Max: %.1f)\n"
                      "Memory: %.2f MB\n"
                      "Delta: %.3f ms",
                g_app_data.nCurrentFPS, 
                g_performance.average_fps,
                g_performance.min_fps,
                g_performance.max_fps,
                get_memory_usage() / (1024.0f * 1024.0f),
                g_app_data.fDeltaTime * 1000.0f);
        draw_text(stats, 10, 10, 0xFFFFFF00, 1.0f);
    }
    
    end_frame();
    present_frame();
}

/**
 * Main game loop
 */
void run_game_loop(void)
{
    const float FIXED_TIMESTEP = 1.0f / 60.0f;  // 60 Hz fixed update
    const float MAX_DELTA = 0.25f;  // Maximum frame time to prevent spiral of death
    
    while (g_app_data.bRunning) {
        // Process window messages
        process_window_messages();
        
        // Calculate delta time
        DWORD currentTime = GetTickCount();
        float rawDelta = (currentTime - g_app_data.dwLastFrameTime) / 1000.0f;
        g_app_data.dwLastFrameTime = currentTime;
        
        // Clamp delta time
        g_app_data.fDeltaTime = (rawDelta > MAX_DELTA) ? MAX_DELTA : rawDelta;
        
        // Fixed timestep with interpolation
        g_app_data.fTimeAccumulator += g_app_data.fDeltaTime;
        
        // Process fixed updates
        while (g_app_data.fTimeAccumulator >= FIXED_TIMESTEP) {
            fixed_update(FIXED_TIMESTEP);
            g_app_data.fTimeAccumulator -= FIXED_TIMESTEP;
        }
        
        // Variable update
        variable_update(g_app_data.fDeltaTime);
        
        // Render
        render_frame();
        
        // Update performance stats
        update_performance_stats();
        
        // Frame rate limiting
        if (g_app_data.nTargetFPS > 0) {
            float targetFrameTime = 1.0f / g_app_data.nTargetFPS;
            float currentFrameTime = (GetTickCount() - g_app_data.dwLastFrameTime) / 1000.0f;
            
            if (currentFrameTime < targetFrameTime) {
                DWORD sleepTime = (DWORD)((targetFrameTime - currentFrameTime) * 1000.0f);
                if (sleepTime > 0) {
                    Sleep(sleepTime);
                }
            }
        }
    }
}

// ========================================================================
// WINDOWS MESSAGE HANDLING
// ========================================================================

/**
 * Window procedure for handling Windows messages
 * @param hWnd Window handle
 * @param msg Message ID
 * @param wParam First parameter
 * @param lParam Second parameter
 * @return Message result
 */
LRESULT CALLBACK window_proc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    // Let the editor handle input first if in editor mode
    if (g_app_data.bEditorMode && g_engine_state.level_editor_initialized) {
        if (handle_editor_input(msg, wParam, lParam)) {
            return 0;
        }
    }
    
    switch (msg) {
        case WM_CLOSE:
            g_app_data.bRunning = FALSE;
            return 0;
            
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
            
        case WM_ACTIVATE:
            if (LOWORD(wParam) == WA_INACTIVE) {
                // Pause when losing focus
                if (!is_game_paused() && !g_app_data.bEditorMode) {
                    pause_game();
                }
                pause_all_sounds();
            } else {
                // Resume when gaining focus
                resume_all_sounds();
            }
            return 0;
            
        case WM_SIZE:
            if (wParam != SIZE_MINIMIZED) {
                int width = LOWORD(lParam);
                int height = HIWORD(lParam);
                
                // Update viewport
                if (g_engine_state.graphics_initialized) {
                    set_viewport(0, 0, width, height);
                }
                
                // Save new size to config
                if (g_engine_state.config_initialized) {
                    set_config_int("Video", "Width", width);
                    set_config_int("Video", "Height", height);
                }
            }
            return 0;
            
        case WM_SYSCOMMAND:
            // Prevent screensaver and monitor power-off
            if ((wParam & 0xFFF0) == SC_SCREENSAVE || 
                (wParam & 0xFFF0) == SC_MONITORPOWER) {
                return 0;
            }
            break;
    }
    
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

// ========================================================================
// MAIN ENTRY POINT
// ========================================================================

/**
 * Main entry point for the Endor engine
 * @param hInstance Application instance
 * @param hPrevInstance Previous instance (unused)
 * @param lpCmdLine Command line arguments
 * @param nCmdShow Show command
 * @return Exit code
 */
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    // Initialize logging first
    if (!initialize_logging("endor_engine.log")) {
        return -1;
    }
    
    engine_log(0, "========================================");
    engine_log(0, "Endor Engine v2.0");
    engine_log(0, "Build Date: %s %s", __DATE__, __TIME__);
    engine_log(0, "Command Line: %s", lpCmdLine);
    engine_log(0, "========================================");
    
    // Initialize the engine
    if (!initialize_engine(hInstance, lpCmdLine)) {
        engine_log(2, "Engine initialization failed!");
        shutdown_engine();
        shutdown_logging();
        return -1;
    }
    
    // Show main window
    ShowWindow(g_app_data.hMainWindow, nCmdShow);
    UpdateWindow(g_app_data.hMainWindow);
    
    // Run the game loop
    engine_log(0, "Entering main game loop");
    run_game_loop();
    engine_log(0, "Exited main game loop");
    
    // Shutdown the engine
    shutdown_engine();
    
    // Performance summary
    engine_log(0, "========================================");
    engine_log(0, "Performance Summary:");
    engine_log(0, "Average FPS: %.1f", g_performance.average_fps);
    engine_log(0, "Min FPS: %.1f", g_performance.min_fps);
    engine_log(0, "Max FPS: %.1f", g_performance.max_fps);
    engine_log(0, "========================================");
    
    // Shutdown logging
    shutdown_logging();
    
    return 0;
}

// ========================================================================
// COMPATIBILITY FUNCTIONS
// ========================================================================

/**
 * These functions provide compatibility with the original code
 * while routing through the improved systems
 */

// Audio compatibility
int sub_41ed50(int sound_id)
{
    char name[32];
    sprintf(name, "sound_%d", sound_id);
    return play_sound_effect_2d(name, 1.0f, 1.0f, 0);
}

int sub_41ed80(int track_id)
{
    char name[32];
    sprintf(name, "music_%d", track_id);
    return play_music(name, 0.7f, 1, 2.0f);
}

void sub_41eda0(void)
{
    stop_music(2.0f);
}

// Graphics compatibility
int sub_41e350(void)
{
    return g_engine_state.graphics_initialized ? 1 : 0;
}

void sub_41e380(void)
{
    clear_screen(0.0f, 0.0f, 0.0f, 1.0f);
}

// Input compatibility
int sub_41ec50(int key)
{
    return is_key_down(key);
}

void sub_41ec80(int* x, int* y)
{
    get_mouse_position(x, y);
}

// Network compatibility
int sub_41ee50(const char* address, int port)
{
    return connect_to_server(address, port, "Player");
}

void sub_41ee80(void)
{
    disconnect_from_server();
}

// Memory compatibility
void* sub_41e150(size_t size)
{
    return allocate_memory(size);
}

void sub_41e180(void* ptr)
{
    free_memory(ptr);
}

// File compatibility
int sub_41e250(const char* filename)
{
    return file_exists(filename);
}

// ========================================================================
// EXPORT FUNCTIONS FOR DLL COMPATIBILITY
// ========================================================================

#ifdef BUILD_DLL
__declspec(dllexport) BOOL InitializeEndorEngine(HINSTANCE hInstance, LPSTR lpCmdLine)
{
    return initialize_engine(hInstance, lpCmdLine);
}

__declspec(dllexport) void ShutdownEndorEngine(void)
{
    shutdown_engine();
}

__declspec(dllexport) void RunEndorEngine(void)
{
    run_game_loop();
}

__declspec(dllexport) void SetEndorWindowHandle(HWND hWnd)
{
    g_app_data.hMainWindow = hWnd;
}

__declspec(dllexport) BOOL IsEndorEngineRunning(void)
{
    return g_app_data.bRunning;
}
#endif
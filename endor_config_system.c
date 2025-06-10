/**
 * ========================================================================
 * ENDOR CONFIGURATION SYSTEM
 * ========================================================================
 * 
 * Configuration management for the Endor game engine. Handles loading,
 * saving, and accessing game settings including graphics, audio, input,
 * and gameplay preferences. Supports both registry and file-based storage.
 */

#include "endor_readable.h"
#include <shlwapi.h>
#pragma comment(lib, "shlwapi.lib")

// ========================================================================
// CONFIGURATION CONSTANTS
// ========================================================================

// Registry paths
#define ENDOR_REGISTRY_KEY "Software\\Endor\\Settings"
#define ENDOR_REGISTRY_GRAPHICS "Graphics"
#define ENDOR_REGISTRY_AUDIO "Audio"
#define ENDOR_REGISTRY_INPUT "Input"
#define ENDOR_REGISTRY_GAMEPLAY "Gameplay"
#define ENDOR_REGISTRY_NETWORK "Network"

// Configuration file
#define CONFIG_FILE_NAME "endor.cfg"
#define CONFIG_FILE_VERSION 2
#define CONFIG_MAGIC "ECFG"

// Default values
#define DEFAULT_RESOLUTION_WIDTH 640
#define DEFAULT_RESOLUTION_HEIGHT 480
#define DEFAULT_COLOR_DEPTH 16
#define DEFAULT_FULLSCREEN FALSE
#define DEFAULT_VSYNC TRUE
#define DEFAULT_TEXTURE_QUALITY 2
#define DEFAULT_SHADOW_QUALITY 1
#define DEFAULT_MASTER_VOLUME 80
#define DEFAULT_MUSIC_VOLUME 70
#define DEFAULT_SFX_VOLUME 90
#define DEFAULT_MOUSE_SENSITIVITY 50
#define DEFAULT_INVERT_MOUSE FALSE
#define DEFAULT_DIFFICULTY 1
#define DEFAULT_SHOW_HINTS TRUE
#define DEFAULT_AUTO_SAVE TRUE

// Configuration limits
#define MIN_RESOLUTION_WIDTH 320
#define MAX_RESOLUTION_WIDTH 1920
#define MIN_RESOLUTION_HEIGHT 240
#define MAX_RESOLUTION_HEIGHT 1080
#define MIN_VOLUME 0
#define MAX_VOLUME 100
#define MIN_SENSITIVITY 1
#define MAX_SENSITIVITY 100

// ========================================================================
// CONFIGURATION STRUCTURES
// ========================================================================

// Graphics configuration
typedef struct {
    int resolution_width;
    int resolution_height;
    int color_depth;
    BOOL fullscreen;
    BOOL vsync;
    int texture_quality;    // 0=Low, 1=Medium, 2=High, 3=Ultra
    int shadow_quality;     // 0=Off, 1=Low, 2=High
    int antialiasing;       // 0=Off, 2x, 4x, 8x
    BOOL show_fps;
    int brightness;         // 0-100
    int contrast;          // 0-100
    int gamma;             // 0-100
} GraphicsConfig;

// Audio configuration
typedef struct {
    int master_volume;      // 0-100
    int music_volume;       // 0-100
    int sfx_volume;         // 0-100
    int voice_volume;       // 0-100
    BOOL enable_3d_sound;
    BOOL enable_reverb;
    int audio_quality;      // 0=Low, 1=Medium, 2=High
    int audio_channels;     // 1=Mono, 2=Stereo, 6=5.1
    BOOL enable_music;
    BOOL enable_ambient;
} AudioConfig;

// Input configuration  
typedef struct {
    int mouse_sensitivity;  // 1-100
    BOOL invert_mouse_y;
    BOOL invert_mouse_x;
    int keyboard_layout;    // 0=QWERTY, 1=AZERTY, 2=DVORAK
    BOOL enable_gamepad;
    int gamepad_deadzone;   // 0-50
    BOOL vibration;
    // Key bindings
    int key_forward;
    int key_backward;
    int key_left;
    int key_right;
    int key_jump;
    int key_action;
    int key_menu;
    int key_quicksave;
    int key_quickload;
} InputConfig;

// Gameplay configuration
typedef struct {
    int difficulty;         // 0=Easy, 1=Normal, 2=Hard, 3=Expert
    BOOL show_hints;
    BOOL show_tutorials;
    BOOL auto_save;
    int auto_save_interval; // Minutes
    BOOL enable_gore;
    BOOL enable_subtitles;
    int ui_scale;          // 50-200%
    int language;          // 0=English, 1=French, 2=German, etc.
    BOOL skip_intro;
    BOOL show_damage_numbers;
} GameplayConfig;

// Network configuration
typedef struct {
    char player_name[32];
    int default_port;
    BOOL enable_upnp;
    int connection_timeout;
    int max_players;
    BOOL allow_spectators;
    BOOL public_game;
    char last_server_ip[64];
} NetworkConfig;

// Master configuration structure
typedef struct {
    uint32_t magic;         // "ECFG"
    uint32_t version;
    uint32_t checksum;
    GraphicsConfig graphics;
    AudioConfig audio;
    InputConfig input;
    GameplayConfig gameplay;
    NetworkConfig network;
} Configuration;

// ========================================================================
// CONFIGURATION GLOBALS
// ========================================================================

// Current configuration
static Configuration g_config;
static BOOL g_config_loaded = FALSE;
static BOOL g_config_modified = FALSE;

// Configuration file path
static char g_config_path[MAX_PATH] = "";

// Registry handles
static HKEY g_registry_key = NULL;

// ========================================================================
// UTILITY FUNCTIONS
// ========================================================================

/**
 * Calculates configuration checksum
 * @param config Configuration structure
 * @return Checksum value
 */
static uint32_t calculate_config_checksum(const Configuration* config)
{
    uint32_t checksum = 0xCAFEBABE;
    const uint8_t* data = (const uint8_t*)&config->graphics;
    size_t size = sizeof(Configuration) - offsetof(Configuration, graphics);
    
    for (size_t i = 0; i < size; i++)
    {
        checksum = ((checksum << 7) | (checksum >> 25)) ^ data[i];
    }
    
    return checksum;
}

/**
 * Validates configuration values
 * @param config Configuration to validate
 */
static void validate_config(Configuration* config)
{
    // Graphics validation
    config->graphics.resolution_width = max(MIN_RESOLUTION_WIDTH, 
        min(MAX_RESOLUTION_WIDTH, config->graphics.resolution_width));
    config->graphics.resolution_height = max(MIN_RESOLUTION_HEIGHT,
        min(MAX_RESOLUTION_HEIGHT, config->graphics.resolution_height));
    config->graphics.color_depth = (config->graphics.color_depth == 32) ? 32 : 16;
    config->graphics.texture_quality = max(0, min(3, config->graphics.texture_quality));
    config->graphics.shadow_quality = max(0, min(2, config->graphics.shadow_quality));
    config->graphics.brightness = max(0, min(100, config->graphics.brightness));
    config->graphics.contrast = max(0, min(100, config->graphics.contrast));
    config->graphics.gamma = max(0, min(100, config->graphics.gamma));
    
    // Audio validation
    config->audio.master_volume = max(MIN_VOLUME, min(MAX_VOLUME, config->audio.master_volume));
    config->audio.music_volume = max(MIN_VOLUME, min(MAX_VOLUME, config->audio.music_volume));
    config->audio.sfx_volume = max(MIN_VOLUME, min(MAX_VOLUME, config->audio.sfx_volume));
    config->audio.voice_volume = max(MIN_VOLUME, min(MAX_VOLUME, config->audio.voice_volume));
    config->audio.audio_quality = max(0, min(2, config->audio.audio_quality));
    
    // Input validation
    config->input.mouse_sensitivity = max(MIN_SENSITIVITY, 
        min(MAX_SENSITIVITY, config->input.mouse_sensitivity));
    config->input.gamepad_deadzone = max(0, min(50, config->input.gamepad_deadzone));
    
    // Gameplay validation
    config->gameplay.difficulty = max(0, min(3, config->gameplay.difficulty));
    config->gameplay.auto_save_interval = max(1, min(60, config->gameplay.auto_save_interval));
    config->gameplay.ui_scale = max(50, min(200, config->gameplay.ui_scale));
    
    // Network validation
    config->network.default_port = max(1024, min(65535, config->network.default_port));
    config->network.connection_timeout = max(5, min(300, config->network.connection_timeout));
    config->network.max_players = max(2, min(MAX_NETWORK_PLAYERS, config->network.max_players));
}

/**
 * Sets default configuration values
 * @param config Configuration structure to initialize
 */
static void set_default_config(Configuration* config)
{
    memset(config, 0, sizeof(Configuration));
    
    // Header
    memcpy(&config->magic, CONFIG_MAGIC, 4);
    config->version = CONFIG_FILE_VERSION;
    
    // Graphics defaults
    config->graphics.resolution_width = DEFAULT_RESOLUTION_WIDTH;
    config->graphics.resolution_height = DEFAULT_RESOLUTION_HEIGHT;
    config->graphics.color_depth = DEFAULT_COLOR_DEPTH;
    config->graphics.fullscreen = DEFAULT_FULLSCREEN;
    config->graphics.vsync = DEFAULT_VSYNC;
    config->graphics.texture_quality = DEFAULT_TEXTURE_QUALITY;
    config->graphics.shadow_quality = DEFAULT_SHADOW_QUALITY;
    config->graphics.antialiasing = 0;
    config->graphics.show_fps = FALSE;
    config->graphics.brightness = 50;
    config->graphics.contrast = 50;
    config->graphics.gamma = 50;
    
    // Audio defaults
    config->audio.master_volume = DEFAULT_MASTER_VOLUME;
    config->audio.music_volume = DEFAULT_MUSIC_VOLUME;
    config->audio.sfx_volume = DEFAULT_SFX_VOLUME;
    config->audio.voice_volume = 85;
    config->audio.enable_3d_sound = TRUE;
    config->audio.enable_reverb = TRUE;
    config->audio.audio_quality = 2;
    config->audio.audio_channels = 2;
    config->audio.enable_music = TRUE;
    config->audio.enable_ambient = TRUE;
    
    // Input defaults
    config->input.mouse_sensitivity = DEFAULT_MOUSE_SENSITIVITY;
    config->input.invert_mouse_y = DEFAULT_INVERT_MOUSE;
    config->input.invert_mouse_x = FALSE;
    config->input.keyboard_layout = 0;
    config->input.enable_gamepad = TRUE;
    config->input.gamepad_deadzone = 20;
    config->input.vibration = TRUE;
    
    // Default key bindings
    config->input.key_forward = 'W';
    config->input.key_backward = 'S';
    config->input.key_left = 'A';
    config->input.key_right = 'D';
    config->input.key_jump = VK_SPACE;
    config->input.key_action = 'E';
    config->input.key_menu = VK_ESCAPE;
    config->input.key_quicksave = VK_F5;
    config->input.key_quickload = VK_F9;
    
    // Gameplay defaults
    config->gameplay.difficulty = DEFAULT_DIFFICULTY;
    config->gameplay.show_hints = DEFAULT_SHOW_HINTS;
    config->gameplay.show_tutorials = TRUE;
    config->gameplay.auto_save = DEFAULT_AUTO_SAVE;
    config->gameplay.auto_save_interval = 10;
    config->gameplay.enable_gore = TRUE;
    config->gameplay.enable_subtitles = FALSE;
    config->gameplay.ui_scale = 100;
    config->gameplay.language = 0;
    config->gameplay.skip_intro = FALSE;
    config->gameplay.show_damage_numbers = TRUE;
    
    // Network defaults
    strcpy(config->network.player_name, "Player");
    config->network.default_port = NETWORK_PORT;
    config->network.enable_upnp = TRUE;
    config->network.connection_timeout = 30;
    config->network.max_players = 8;
    config->network.allow_spectators = TRUE;
    config->network.public_game = FALSE;
    config->network.last_server_ip[0] = '\0';
    
    // Calculate checksum
    config->checksum = calculate_config_checksum(config);
}

// ========================================================================
// REGISTRY FUNCTIONS
// ========================================================================

/**
 * Opens or creates the registry key for configuration
 * @return TRUE if successful
 */
static BOOL open_registry_key(void)
{
    if (g_registry_key != NULL)
        return TRUE;
    
    LONG result = RegCreateKeyEx(HKEY_CURRENT_USER, ENDOR_REGISTRY_KEY,
                                0, NULL, REG_OPTION_NON_VOLATILE,
                                KEY_READ | KEY_WRITE, NULL,
                                &g_registry_key, NULL);
    
    return result == ERROR_SUCCESS;
}

/**
 * Closes the registry key
 */
static void close_registry_key(void)
{
    if (g_registry_key != NULL)
    {
        RegCloseKey(g_registry_key);
        g_registry_key = NULL;
    }
}

/**
 * Reads an integer value from registry
 * @param subkey Subkey name
 * @param value Value name
 * @param default_value Default if not found
 * @return Value read or default
 */
static int read_registry_int(const char* subkey, const char* value, int default_value)
{
    if (!open_registry_key())
        return default_value;
    
    HKEY hSubkey;
    char full_path[256];
    sprintf(full_path, "%s\\%s", ENDOR_REGISTRY_KEY, subkey);
    
    if (RegOpenKeyEx(HKEY_CURRENT_USER, full_path, 0, KEY_READ, &hSubkey) != ERROR_SUCCESS)
        return default_value;
    
    DWORD data = 0;
    DWORD size = sizeof(DWORD);
    DWORD type;
    
    if (RegQueryValueEx(hSubkey, value, NULL, &type, (LPBYTE)&data, &size) != ERROR_SUCCESS ||
        type != REG_DWORD)
    {
        RegCloseKey(hSubkey);
        return default_value;
    }
    
    RegCloseKey(hSubkey);
    return (int)data;
}

/**
 * Writes an integer value to registry
 * @param subkey Subkey name
 * @param value Value name
 * @param data Data to write
 * @return TRUE if successful
 */
static BOOL write_registry_int(const char* subkey, const char* value, int data)
{
    if (!open_registry_key())
        return FALSE;
    
    HKEY hSubkey;
    char full_path[256];
    sprintf(full_path, "%s\\%s", ENDOR_REGISTRY_KEY, subkey);
    
    if (RegCreateKeyEx(HKEY_CURRENT_USER, full_path, 0, NULL, REG_OPTION_NON_VOLATILE,
                      KEY_WRITE, NULL, &hSubkey, NULL) != ERROR_SUCCESS)
        return FALSE;
    
    DWORD dw_data = (DWORD)data;
    LONG result = RegSetValueEx(hSubkey, value, 0, REG_DWORD, 
                               (LPBYTE)&dw_data, sizeof(DWORD));
    
    RegCloseKey(hSubkey);
    return result == ERROR_SUCCESS;
}

/**
 * Reads a string value from registry
 * @param subkey Subkey name
 * @param value Value name
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 * @param default_value Default if not found
 * @return TRUE if successful
 */
static BOOL read_registry_string(const char* subkey, const char* value, 
                               char* buffer, DWORD buffer_size, const char* default_value)
{
    if (!open_registry_key())
    {
        strcpy(buffer, default_value);
        return FALSE;
    }
    
    HKEY hSubkey;
    char full_path[256];
    sprintf(full_path, "%s\\%s", ENDOR_REGISTRY_KEY, subkey);
    
    if (RegOpenKeyEx(HKEY_CURRENT_USER, full_path, 0, KEY_READ, &hSubkey) != ERROR_SUCCESS)
    {
        strcpy(buffer, default_value);
        return FALSE;
    }
    
    DWORD type;
    if (RegQueryValueEx(hSubkey, value, NULL, &type, (LPBYTE)buffer, &buffer_size) != ERROR_SUCCESS ||
        type != REG_SZ)
    {
        strcpy(buffer, default_value);
        RegCloseKey(hSubkey);
        return FALSE;
    }
    
    RegCloseKey(hSubkey);
    return TRUE;
}

/**
 * Writes a string value to registry
 * @param subkey Subkey name
 * @param value Value name
 * @param data String to write
 * @return TRUE if successful
 */
static BOOL write_registry_string(const char* subkey, const char* value, const char* data)
{
    if (!open_registry_key())
        return FALSE;
    
    HKEY hSubkey;
    char full_path[256];
    sprintf(full_path, "%s\\%s", ENDOR_REGISTRY_KEY, subkey);
    
    if (RegCreateKeyEx(HKEY_CURRENT_USER, full_path, 0, NULL, REG_OPTION_NON_VOLATILE,
                      KEY_WRITE, NULL, &hSubkey, NULL) != ERROR_SUCCESS)
        return FALSE;
    
    LONG result = RegSetValueEx(hSubkey, value, 0, REG_SZ, 
                               (LPBYTE)data, strlen(data) + 1);
    
    RegCloseKey(hSubkey);
    return result == ERROR_SUCCESS;
}

// ========================================================================
// FILE I/O FUNCTIONS
// ========================================================================

/**
 * Loads configuration from file
 * @param filename Configuration file path
 * @return TRUE if successful
 */
static BOOL load_config_from_file(const char* filename)
{
    FILE* file = fopen(filename, "rb");
    if (!file)
        return FALSE;
    
    Configuration temp_config;
    size_t read = fread(&temp_config, 1, sizeof(Configuration), file);
    fclose(file);
    
    if (read != sizeof(Configuration))
        return FALSE;
    
    // Validate magic
    if (memcmp(&temp_config.magic, CONFIG_MAGIC, 4) != 0)
        return FALSE;
    
    // Check version compatibility
    if (temp_config.version > CONFIG_FILE_VERSION)
        return FALSE;
    
    // Verify checksum
    uint32_t calculated = calculate_config_checksum(&temp_config);
    if (calculated != temp_config.checksum)
        return FALSE;
    
    // Validate and accept configuration
    validate_config(&temp_config);
    memcpy(&g_config, &temp_config, sizeof(Configuration));
    
    return TRUE;
}

/**
 * Saves configuration to file
 * @param filename Configuration file path
 * @return TRUE if successful
 */
static BOOL save_config_to_file(const char* filename)
{
    // Update checksum
    g_config.checksum = calculate_config_checksum(&g_config);
    
    FILE* file = fopen(filename, "wb");
    if (!file)
        return FALSE;
    
    size_t written = fwrite(&g_config, 1, sizeof(Configuration), file);
    fclose(file);
    
    return written == sizeof(Configuration);
}

// ========================================================================
// CONFIGURATION MANAGEMENT
// ========================================================================

/**
 * Initializes the configuration system
 * @param data_path Path to game data directory
 * @return TRUE if successful
 */
BOOL initialize_config_system(const char* data_path)
{
    // Build configuration file path
    _snprintf(g_config_path, sizeof(g_config_path), 
              "%s\\%s", data_path, CONFIG_FILE_NAME);
    
    // Set defaults first
    set_default_config(&g_config);
    
    // Try to load from file
    if (load_config_from_file(g_config_path))
    {
        g_config_loaded = TRUE;
        g_config_modified = FALSE;
        return TRUE;
    }
    
    // File load failed, try registry
    // Note: This provides backwards compatibility with older versions
    
    // Graphics from registry
    g_config.graphics.resolution_width = read_registry_int(ENDOR_REGISTRY_GRAPHICS, 
        "ResolutionWidth", DEFAULT_RESOLUTION_WIDTH);
    g_config.graphics.resolution_height = read_registry_int(ENDOR_REGISTRY_GRAPHICS,
        "ResolutionHeight", DEFAULT_RESOLUTION_HEIGHT);
    g_config.graphics.fullscreen = read_registry_int(ENDOR_REGISTRY_GRAPHICS,
        "Fullscreen", DEFAULT_FULLSCREEN);
    
    // Audio from registry
    g_config.audio.master_volume = read_registry_int(ENDOR_REGISTRY_AUDIO,
        "MasterVolume", DEFAULT_MASTER_VOLUME);
    g_config.audio.music_volume = read_registry_int(ENDOR_REGISTRY_AUDIO,
        "MusicVolume", DEFAULT_MUSIC_VOLUME);
    
    // Input from registry
    g_config.input.mouse_sensitivity = read_registry_int(ENDOR_REGISTRY_INPUT,
        "MouseSensitivity", DEFAULT_MOUSE_SENSITIVITY);
    
    // Network from registry
    read_registry_string(ENDOR_REGISTRY_NETWORK, "PlayerName",
        g_config.network.player_name, sizeof(g_config.network.player_name), "Player");
    
    g_config_loaded = TRUE;
    g_config_modified = TRUE;  // Force save to create file
    
    return TRUE;
}

/**
 * Saves all configuration settings
 * @return TRUE if successful
 */
BOOL save_configuration(void)  // Matches existing function signature
{
    if (!g_config_modified)
        return TRUE;
    
    // Save to file
    if (!save_config_to_file(g_config_path))
    {
        OutputDebugString("Failed to save configuration file\n");
        return FALSE;
    }
    
    // Also update registry for backwards compatibility
    write_registry_int(ENDOR_REGISTRY_GRAPHICS, "ResolutionWidth", 
        g_config.graphics.resolution_width);
    write_registry_int(ENDOR_REGISTRY_GRAPHICS, "ResolutionHeight",
        g_config.graphics.resolution_height);
    write_registry_int(ENDOR_REGISTRY_GRAPHICS, "Fullscreen",
        g_config.graphics.fullscreen);
    
    write_registry_int(ENDOR_REGISTRY_AUDIO, "MasterVolume",
        g_config.audio.master_volume);
    write_registry_int(ENDOR_REGISTRY_AUDIO, "MusicVolume",
        g_config.audio.music_volume);
    
    write_registry_int(ENDOR_REGISTRY_INPUT, "MouseSensitivity",
        g_config.input.mouse_sensitivity);
    
    write_registry_string(ENDOR_REGISTRY_NETWORK, "PlayerName",
        g_config.network.player_name);
    
    g_config_modified = FALSE;
    return TRUE;
}

/**
 * Loads configuration settings
 * @return TRUE if successful
 */
BOOL load_configuration(void)  // Matches existing function signature
{
    if (g_config_loaded)
        return TRUE;
    
    return initialize_config_system(".");
}

/**
 * Loads complete configuration (extended version)
 * @return TRUE if successful
 */
BOOL load_complete_configuration(void)  // Matches existing function signature
{
    return load_configuration();
}

/**
 * Saves complete configuration (extended version)
 * @return TRUE if successful
 */
BOOL save_complete_configuration(void)  // Matches existing function signature
{
    return save_configuration();
}

// ========================================================================
// CONFIGURATION ACCESS FUNCTIONS
// ========================================================================

/**
 * Sets a configuration value
 * @param category Configuration category
 * @param key Setting key name
 * @param value Value to set
 * @return TRUE if successful
 */
BOOL set_configuration_value(config_category_t category, const char* key, int value)
{
    BOOL success = FALSE;
    
    switch (category)
    {
        case CONFIG_GRAPHICS:
            if (strcmp(key, "ResolutionWidth") == 0)
            {
                g_config.graphics.resolution_width = value;
                success = TRUE;
            }
            else if (strcmp(key, "ResolutionHeight") == 0)
            {
                g_config.graphics.resolution_height = value;
                success = TRUE;
            }
            else if (strcmp(key, "Fullscreen") == 0)
            {
                g_config.graphics.fullscreen = value != 0;
                success = TRUE;
            }
            else if (strcmp(key, "TextureQuality") == 0)
            {
                g_config.graphics.texture_quality = value;
                success = TRUE;
            }
            break;
            
        case CONFIG_AUDIO:
            if (strcmp(key, "MasterVolume") == 0)
            {
                g_config.audio.master_volume = value;
                success = TRUE;
            }
            else if (strcmp(key, "MusicVolume") == 0)
            {
                g_config.audio.music_volume = value;
                success = TRUE;
            }
            else if (strcmp(key, "SFXVolume") == 0)
            {
                g_config.audio.sfx_volume = value;
                success = TRUE;
            }
            break;
            
        case CONFIG_INPUT:
            if (strcmp(key, "MouseSensitivity") == 0)
            {
                g_config.input.mouse_sensitivity = value;
                success = TRUE;
            }
            else if (strcmp(key, "InvertMouse") == 0)
            {
                g_config.input.invert_mouse_y = value != 0;
                success = TRUE;
            }
            break;
            
        case CONFIG_GAMEPLAY:
            if (strcmp(key, "Difficulty") == 0)
            {
                g_config.gameplay.difficulty = value;
                success = TRUE;
            }
            else if (strcmp(key, "ShowHints") == 0)
            {
                g_config.gameplay.show_hints = value != 0;
                success = TRUE;
            }
            else if (strcmp(key, "AutoSave") == 0)
            {
                g_config.gameplay.auto_save = value != 0;
                success = TRUE;
            }
            break;
            
        case CONFIG_NETWORK:
            if (strcmp(key, "DefaultPort") == 0)
            {
                g_config.network.default_port = value;
                success = TRUE;
            }
            else if (strcmp(key, "MaxPlayers") == 0)
            {
                g_config.network.max_players = value;
                success = TRUE;
            }
            break;
    }
    
    if (success)
    {
        validate_config(&g_config);
        g_config_modified = TRUE;
    }
    
    return success;
}

/**
 * Gets a configuration value
 * @param category Configuration category
 * @param key Setting key name
 * @param default_value Default if not found
 * @return Configuration value
 */
int get_configuration_value(config_category_t category, const char* key, int default_value)
{
    switch (category)
    {
        case CONFIG_GRAPHICS:
            if (strcmp(key, "ResolutionWidth") == 0)
                return g_config.graphics.resolution_width;
            else if (strcmp(key, "ResolutionHeight") == 0)
                return g_config.graphics.resolution_height;
            else if (strcmp(key, "Fullscreen") == 0)
                return g_config.graphics.fullscreen ? 1 : 0;
            else if (strcmp(key, "TextureQuality") == 0)
                return g_config.graphics.texture_quality;
            break;
            
        case CONFIG_AUDIO:
            if (strcmp(key, "MasterVolume") == 0)
                return g_config.audio.master_volume;
            else if (strcmp(key, "MusicVolume") == 0)
                return g_config.audio.music_volume;
            else if (strcmp(key, "SFXVolume") == 0)
                return g_config.audio.sfx_volume;
            break;
            
        case CONFIG_INPUT:
            if (strcmp(key, "MouseSensitivity") == 0)
                return g_config.input.mouse_sensitivity;
            else if (strcmp(key, "InvertMouse") == 0)
                return g_config.input.invert_mouse_y ? 1 : 0;
            break;
            
        case CONFIG_GAMEPLAY:
            if (strcmp(key, "Difficulty") == 0)
                return g_config.gameplay.difficulty;
            else if (strcmp(key, "ShowHints") == 0)
                return g_config.gameplay.show_hints ? 1 : 0;
            else if (strcmp(key, "AutoSave") == 0)
                return g_config.gameplay.auto_save ? 1 : 0;
            break;
            
        case CONFIG_NETWORK:
            if (strcmp(key, "DefaultPort") == 0)
                return g_config.network.default_port;
            else if (strcmp(key, "MaxPlayers") == 0)
                return g_config.network.max_players;
            break;
    }
    
    return default_value;
}

// ========================================================================
// DIRECT ACCESS FUNCTIONS
// ========================================================================

/**
 * Gets the current graphics configuration
 * @return Pointer to graphics config (read-only)
 */
const GraphicsConfig* get_graphics_config(void)
{
    return &g_config.graphics;
}

/**
 * Gets the current audio configuration
 * @return Pointer to audio config (read-only)
 */
const AudioConfig* get_audio_config(void)
{
    return &g_config.audio;
}

/**
 * Gets the current input configuration
 * @return Pointer to input config (read-only)
 */
const InputConfig* get_input_config(void)
{
    return &g_config.input;
}

/**
 * Gets the current gameplay configuration
 * @return Pointer to gameplay config (read-only)
 */
const GameplayConfig* get_gameplay_config(void)
{
    return &g_config.gameplay;
}

/**
 * Gets the current network configuration
 * @return Pointer to network config (read-only)
 */
const NetworkConfig* get_network_config(void)
{
    return &g_config.network;
}

/**
 * Sets player name for network games
 * @param name Player name
 */
void set_player_name(const char* name)
{
    strncpy(g_config.network.player_name, name, 31);
    g_config.network.player_name[31] = '\0';
    g_config_modified = TRUE;
}

/**
 * Applies video settings (resolution, fullscreen, etc)
 * @param hwnd Window handle
 * @return TRUE if successful
 */
BOOL apply_video_settings(HWND hwnd)
{
    // This would typically involve DirectDraw/Direct3D calls
    // For now, just mark as successful
    
    // Save the settings
    save_configuration();
    
    return TRUE;
}

/**
 * Resets configuration to defaults
 * @param category Category to reset (CONFIG_GENERAL for all)
 */
void reset_configuration(config_category_t category)
{
    Configuration default_config;
    set_default_config(&default_config);
    
    switch (category)
    {
        case CONFIG_GENERAL:
            memcpy(&g_config, &default_config, sizeof(Configuration));
            break;
            
        case CONFIG_GRAPHICS:
            memcpy(&g_config.graphics, &default_config.graphics, sizeof(GraphicsConfig));
            break;
            
        case CONFIG_AUDIO:
            memcpy(&g_config.audio, &default_config.audio, sizeof(AudioConfig));
            break;
            
        case CONFIG_INPUT:
            memcpy(&g_config.input, &default_config.input, sizeof(InputConfig));
            break;
            
        case CONFIG_GAMEPLAY:
            memcpy(&g_config.gameplay, &default_config.gameplay, sizeof(GameplayConfig));
            break;
            
        case CONFIG_NETWORK:
            memcpy(&g_config.network, &default_config.network, sizeof(NetworkConfig));
            break;
    }
    
    g_config_modified = TRUE;
}

/**
 * Exports configuration to text file
 * @param filename Output filename
 * @return TRUE if successful
 */
BOOL export_configuration(const char* filename)
{
    FILE* file = fopen(filename, "w");
    if (!file)
        return FALSE;
    
    fprintf(file, "ENDOR CONFIGURATION\n");
    fprintf(file, "==================\n\n");
    
    fprintf(file, "[Graphics]\n");
    fprintf(file, "Resolution=%dx%d\n", g_config.graphics.resolution_width,
            g_config.graphics.resolution_height);
    fprintf(file, "ColorDepth=%d\n", g_config.graphics.color_depth);
    fprintf(file, "Fullscreen=%s\n", g_config.graphics.fullscreen ? "Yes" : "No");
    fprintf(file, "VSync=%s\n", g_config.graphics.vsync ? "Yes" : "No");
    fprintf(file, "TextureQuality=%d\n", g_config.graphics.texture_quality);
    fprintf(file, "ShadowQuality=%d\n", g_config.graphics.shadow_quality);
    fprintf(file, "\n");
    
    fprintf(file, "[Audio]\n");
    fprintf(file, "MasterVolume=%d\n", g_config.audio.master_volume);
    fprintf(file, "MusicVolume=%d\n", g_config.audio.music_volume);
    fprintf(file, "SFXVolume=%d\n", g_config.audio.sfx_volume);
    fprintf(file, "3DSound=%s\n", g_config.audio.enable_3d_sound ? "Yes" : "No");
    fprintf(file, "\n");
    
    fprintf(file, "[Input]\n");
    fprintf(file, "MouseSensitivity=%d\n", g_config.input.mouse_sensitivity);
    fprintf(file, "InvertMouseY=%s\n", g_config.input.invert_mouse_y ? "Yes" : "No");
    fprintf(file, "Forward=%c\n", g_config.input.key_forward);
    fprintf(file, "Backward=%c\n", g_config.input.key_backward);
    fprintf(file, "Left=%c\n", g_config.input.key_left);
    fprintf(file, "Right=%c\n", g_config.input.key_right);
    fprintf(file, "\n");
    
    fprintf(file, "[Gameplay]\n");
    fprintf(file, "Difficulty=%d\n", g_config.gameplay.difficulty);
    fprintf(file, "ShowHints=%s\n", g_config.gameplay.show_hints ? "Yes" : "No");
    fprintf(file, "AutoSave=%s\n", g_config.gameplay.auto_save ? "Yes" : "No");
    fprintf(file, "Language=%d\n", g_config.gameplay.language);
    fprintf(file, "\n");
    
    fprintf(file, "[Network]\n");
    fprintf(file, "PlayerName=%s\n", g_config.network.player_name);
    fprintf(file, "DefaultPort=%d\n", g_config.network.default_port);
    fprintf(file, "MaxPlayers=%d\n", g_config.network.max_players);
    
    fclose(file);
    return TRUE;
}

/**
 * Shutdown configuration system
 */
void shutdown_config_system(void)
{
    // Save any pending changes
    if (g_config_modified)
        save_configuration();
    
    // Close registry
    close_registry_key();
    
    // Clear sensitive data
    memset(&g_config, 0, sizeof(Configuration));
    g_config_loaded = FALSE;
    g_config_modified = FALSE;
}
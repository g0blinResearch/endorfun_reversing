/**
 * ========================================================================
 * ENDOR FILE SYSTEM AND RESOURCE MANAGEMENT
 * ========================================================================
 * 
 * Handles all file I/O operations, resource loading, and data persistence
 * for the Endor game engine. Includes level file parsing, save game
 * management, configuration files, and resource caching.
 */

#include "endor_readable.h"
#include <stdio.h>
#include <io.h>
#include <fcntl.h>
#include <sys/stat.h>

// ========================================================================
// FILE SYSTEM CONSTANTS
// ========================================================================

// File type signatures
#define FILE_SIG_LEVEL     "ELVD"    // Level data file
#define FILE_SIG_SAVE      "ESVG"    // Save game file
#define FILE_SIG_CONFIG    "ECFG"    // Configuration file
#define FILE_SIG_HIGHSCORE "EHSC"    // High score file

// File size limits
#define MAX_FILE_PATH      260
#define MAX_LEVEL_SIZE     0xFD34
#define MAX_SAVE_SIZE      0x79B
#define MAX_CONFIG_SIZE    0x1000

// File access modes
#define FILE_MODE_READ     0x01
#define FILE_MODE_WRITE    0x02
#define FILE_MODE_CREATE   0x04
#define FILE_MODE_BINARY   0x08

// ========================================================================
// FILE SYSTEM STRUCTURES
// ========================================================================

// Level file header structure
typedef struct {
    char signature[4];      // "ELVD"
    uint32_t version;
    uint32_t data_size;
    uint32_t checksum;
    char level_name[64];
    char author[32];
    uint32_t flags;
    uint32_t reserved[8];
} LevelFileHeader;

// Save game header structure
typedef struct {
    char signature[4];      // "ESVG"
    uint32_t version;
    uint32_t save_time;
    uint32_t play_time;
    char player_name[32];
    int32_t current_level;
    int32_t score;
    int32_t lives;
    uint32_t checksum;
} SaveGameHeader;

// File handle wrapper
typedef struct {
    FILE* file;
    int mode;
    char path[MAX_FILE_PATH];
    size_t size;
    size_t position;
} FileHandle;

// ========================================================================
// FILE SYSTEM GLOBALS
// ========================================================================

// File paths
static char g_game_directory[MAX_FILE_PATH] = "";     // was data_42c820
static char g_level_directory[MAX_FILE_PATH] = "";
static char g_save_directory[MAX_FILE_PATH] = "";
static char g_config_file_path[MAX_FILE_PATH] = "";   // was data_42cb50

// File caching
static FileHandle* g_open_files[16] = {NULL};
static int g_num_open_files = 0;

// Level data buffer
static char g_level_data_buffer[MAX_LEVEL_SIZE];      // was data_42c930
// Save game data buffer
static char g_save_game_data[MAX_SAVE_SIZE];

// ========================================================================
// PATH MANAGEMENT
// ========================================================================

/**
 * Initializes the file system and sets up directory paths
 * @param game_path The root game directory path
 * @return TRUE on success, FALSE on failure
 */
BOOL initialize_file_system(const char* game_path)
{
    // Set game directory
    strncpy(g_game_directory, game_path, MAX_FILE_PATH - 1);
    g_game_directory[MAX_FILE_PATH - 1] = '\0';
    
    // Ensure path ends with backslash
    size_t len = strlen(g_game_directory);
    if (len > 0 && g_game_directory[len - 1] != '\\')
    {
        if (len < MAX_FILE_PATH - 1)
        {
            g_game_directory[len] = '\\';
            g_game_directory[len + 1] = '\0';
        }
    }
    
    // Set up subdirectories
    sprintf(g_level_directory, "%slevels\\", g_game_directory);
    sprintf(g_save_directory, "%ssaves\\", g_game_directory);
    sprintf(g_config_file_path, "%sendor.cfg", g_game_directory);
    
    // Create directories if they don't exist
    CreateDirectoryA(g_level_directory, NULL);
    CreateDirectoryA(g_save_directory, NULL);
    
    return TRUE;
}

/**
 * Constructs a full file path from a relative path
 * @param relative_path The relative file path
 * @param file_type Type of file (for determining subdirectory)
 * @param out_path Buffer to receive the full path
 * @return Pointer to out_path
 */
char* build_file_path(const char* relative_path, int file_type, char* out_path)
{
    switch (file_type)
    {
        case 0:  // Level file
            sprintf(out_path, "%s%s", g_level_directory, relative_path);
            break;
        case 1:  // Save file
            sprintf(out_path, "%s%s", g_save_directory, relative_path);
            break;
        case 2:  // Config file
            strcpy(out_path, g_config_file_path);
            break;
        default:  // General file
            sprintf(out_path, "%s%s", g_game_directory, relative_path);
            break;
    }
    
    return out_path;
}

// ========================================================================
// FILE OPERATIONS
// ========================================================================

/**
 * Opens a file for reading or writing
 * @param path File path
 * @param mode File access mode flags
 * @return File handle on success, NULL on failure
 */
FileHandle* open_file(const char* path, int mode)
{
    if (g_num_open_files >= 16)
        return NULL;  // Too many open files
    
    FileHandle* handle = (FileHandle*)malloc(sizeof(FileHandle));
    if (!handle)
        return NULL;
    
    // Determine fopen mode string
    char mode_str[8];
    if (mode & FILE_MODE_WRITE)
    {
        if (mode & FILE_MODE_CREATE)
            strcpy(mode_str, "w");
        else
            strcpy(mode_str, "r+");
    }
    else
    {
        strcpy(mode_str, "r");
    }
    
    if (mode & FILE_MODE_BINARY)
        strcat(mode_str, "b");
    
    // Open the file
    handle->file = fopen(path, mode_str);
    if (!handle->file)
    {
        free(handle);
        return NULL;
    }
    
    // Initialize handle
    handle->mode = mode;
    strncpy(handle->path, path, MAX_FILE_PATH - 1);
    handle->path[MAX_FILE_PATH - 1] = '\0';
    handle->position = 0;
    
    // Get file size
    fseek(handle->file, 0, SEEK_END);
    handle->size = ftell(handle->file);
    fseek(handle->file, 0, SEEK_SET);
    
    // Add to open files list
    g_open_files[g_num_open_files++] = handle;
    
    return handle;
}

/**
 * Closes an open file
 * @param handle File handle to close
 */
void close_file(FileHandle* handle)
{
    if (!handle)
        return;
    
    if (handle->file)
        fclose(handle->file);
    
    // Remove from open files list
    for (int i = 0; i < g_num_open_files; i++)
    {
        if (g_open_files[i] == handle)
        {
            // Shift remaining handles
            for (int j = i; j < g_num_open_files - 1; j++)
                g_open_files[j] = g_open_files[j + 1];
            g_num_open_files--;
            break;
        }
    }
    
    free(handle);
}

/**
 * Reads data from a file
 * @param handle File handle
 * @param buffer Buffer to receive data
 * @param size Number of bytes to read
 * @return Number of bytes actually read
 */
size_t read_file_data(FileHandle* handle, void* buffer, size_t size)
{
    if (!handle || !handle->file || !buffer)
        return 0;
    
    size_t bytes_read = fread(buffer, 1, size, handle->file);
    handle->position += bytes_read;
    
    return bytes_read;
}

/**
 * Writes data to a file
 * @param handle File handle
 * @param buffer Data to write
 * @param size Number of bytes to write
 * @return Number of bytes actually written
 */
size_t write_file_data(FileHandle* handle, const void* buffer, size_t size)
{
    if (!handle || !handle->file || !buffer)
        return 0;
    
    if (!(handle->mode & FILE_MODE_WRITE))
        return 0;  // File not opened for writing
    
    size_t bytes_written = fwrite(buffer, 1, size, handle->file);
    handle->position += bytes_written;
    
    if (handle->position > handle->size)
        handle->size = handle->position;
    
    return bytes_written;
}

// ========================================================================
// LEVEL FILE OPERATIONS
// ========================================================================

/**
 * Loads a level file into memory
 * @param level_name Name of the level file (without extension)
 * @return TRUE on success, FALSE on failure
 */
BOOL load_level_file(const char* level_name)  // Improved from various sub_ functions
{
    char file_path[MAX_FILE_PATH];
    sprintf(file_path, "%s.elv", level_name);
    build_file_path(file_path, 0, file_path);
    
    FileHandle* handle = open_file(file_path, FILE_MODE_READ | FILE_MODE_BINARY);
    if (!handle)
        return FALSE;
    
    // Read and validate header
    LevelFileHeader header;
    if (read_file_data(handle, &header, sizeof(header)) != sizeof(header))
    {
        close_file(handle);
        return FALSE;
    }
    
    // Check signature
    if (memcmp(header.signature, FILE_SIG_LEVEL, 4) != 0)
    {
        close_file(handle);
        return FALSE;
    }
    
    // Check data size
    if (header.data_size > MAX_LEVEL_SIZE)
    {
        close_file(handle);
        return FALSE;
    }
    
    // Read level data
    size_t bytes_read = read_file_data(handle, g_level_data_buffer, header.data_size);
    close_file(handle);
    
    if (bytes_read != header.data_size)
        return FALSE;
    
    // Verify checksum
    uint32_t checksum = calculate_checksum(g_level_data_buffer, header.data_size);
    if (checksum != header.checksum)
        return FALSE;
    
    return TRUE;
}

/**
 * Saves the current level to a file
 * @param level_name Name for the level file
 * @param author Author name
 * @return TRUE on success, FALSE on failure
 */
BOOL save_level_file(const char* level_name, const char* author)
{
    char file_path[MAX_FILE_PATH];
    sprintf(file_path, "%s.elv", level_name);
    build_file_path(file_path, 0, file_path);
    
    FileHandle* handle = open_file(file_path, 
                                  FILE_MODE_WRITE | FILE_MODE_CREATE | FILE_MODE_BINARY);
    if (!handle)
        return FALSE;
    
    // Prepare header
    LevelFileHeader header = {0};
    memcpy(header.signature, FILE_SIG_LEVEL, 4);
    header.version = 1;
    header.data_size = get_current_level_size();
    strncpy(header.level_name, level_name, 63);
    strncpy(header.author, author, 31);
    header.checksum = calculate_checksum(g_level_data_buffer, header.data_size);
    
    // Write header and data
    BOOL success = TRUE;
    if (write_file_data(handle, &header, sizeof(header)) != sizeof(header))
        success = FALSE;
    else if (write_file_data(handle, g_level_data_buffer, header.data_size) != header.data_size)
        success = FALSE;
    
    close_file(handle);
    return success;
}

// ========================================================================
// SAVE GAME OPERATIONS
// ========================================================================

/**
 * Saves the current game state
 * @param slot Save game slot number (0-9)
 * @param player_name Player's name
 * @return TRUE on success, FALSE on failure
 */
BOOL save_game_state(int slot, const char* player_name)
{
    if (slot < 0 || slot > 9)
        return FALSE;
    
    char file_path[MAX_FILE_PATH];
    sprintf(file_path, "save%d.esg", slot);
    build_file_path(file_path, 1, file_path);
    
    FileHandle* handle = open_file(file_path,
                                  FILE_MODE_WRITE | FILE_MODE_CREATE | FILE_MODE_BINARY);
    if (!handle)
        return FALSE;
    
    // Prepare header
    SaveGameHeader header = {0};
    memcpy(header.signature, FILE_SIG_SAVE, 4);
    header.version = 1;
    header.save_time = (uint32_t)time(NULL);
    header.play_time = get_current_play_time();
    strncpy(header.player_name, player_name, 31);
    header.current_level = get_current_level();
    header.score = get_current_score();
    header.lives = get_player_lives();
    
    // Calculate checksum of game state data
    header.checksum = calculate_checksum(&g_save_game_data, sizeof(g_save_game_data));
    
    // Write header and game state
    BOOL success = TRUE;
    if (write_file_data(handle, &header, sizeof(header)) != sizeof(header))
        success = FALSE;
    else if (write_file_data(handle, &g_save_game_data, sizeof(g_save_game_data)) != 
             sizeof(g_save_game_data))
        success = FALSE;
    
    close_file(handle);
    return success;
}

/**
 * Loads a saved game state
 * @param slot Save game slot number (0-9)
 * @return TRUE on success, FALSE on failure
 */
BOOL load_game_state(int slot)
{
    if (slot < 0 || slot > 9)
        return FALSE;
    
    char file_path[MAX_FILE_PATH];
    sprintf(file_path, "save%d.esg", slot);
    build_file_path(file_path, 1, file_path);
    
    FileHandle* handle = open_file(file_path, FILE_MODE_READ | FILE_MODE_BINARY);
    if (!handle)
        return FALSE;
    
    // Read and validate header
    SaveGameHeader header;
    if (read_file_data(handle, &header, sizeof(header)) != sizeof(header))
    {
        close_file(handle);
        return FALSE;
    }
    
    // Check signature
    if (memcmp(header.signature, FILE_SIG_SAVE, 4) != 0)
    {
        close_file(handle);
        return FALSE;
    }
    
    // Read game state data
    if (read_file_data(handle, &g_save_game_data, sizeof(g_save_game_data)) != 
        sizeof(g_save_game_data))
    {
        close_file(handle);
        return FALSE;
    }
    
    close_file(handle);
    
    // Verify checksum
    uint32_t checksum = calculate_checksum(&g_save_game_data, sizeof(g_save_game_data));
    if (checksum != header.checksum)
        return FALSE;
    
    // Restore game state
    set_current_level(header.current_level);
    set_current_score(header.score);
    set_player_lives(header.lives);
    
    return TRUE;
}

// ========================================================================
// UTILITY FUNCTIONS
// ========================================================================

/**
 * Calculates a simple checksum for data integrity
 * @param data Data buffer
 * @param size Size of data
 * @return Checksum value
 */
uint32_t calculate_checksum(const void* data, size_t size)
{
    const uint8_t* bytes = (const uint8_t*)data;
    uint32_t checksum = 0;
    
    for (size_t i = 0; i < size; i++)
    {
        checksum = ((checksum << 1) | (checksum >> 31)) ^ bytes[i];
    }
    
    return checksum;
}

/**
 * Checks if a file exists
 * @param path File path to check
 * @return TRUE if file exists, FALSE otherwise
 */
BOOL file_exists(const char* path)
{
    return (_access(path, 0) != -1);
}

/**
 * Gets the size of a file
 * @param path File path
 * @return File size in bytes, or -1 on error
 */
long get_file_size(const char* path)
{
    struct _stat st;
    if (_stat(path, &st) == 0)
        return st.st_size;
    return -1;
}

/**
 * Lists all files in a directory matching a pattern
 * @param directory Directory path
 * @param pattern File pattern (e.g., "*.elv")
 * @param callback Callback function for each file found
 * @return Number of files found
 */
int enumerate_files(const char* directory, const char* pattern, 
                   void (*callback)(const char* filename))
{
    WIN32_FIND_DATAA find_data;
    char search_path[MAX_FILE_PATH];
    int count = 0;
    
    sprintf(search_path, "%s\\%s", directory, pattern);
    
    HANDLE find_handle = FindFirstFileA(search_path, &find_data);
    if (find_handle == INVALID_HANDLE_VALUE)
        return 0;
    
    do
    {
        if (!(find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
        {
            if (callback)
                callback(find_data.cFileName);
            count++;
        }
    } while (FindNextFileA(find_handle, &find_data));
    
    FindClose(find_handle);
    return count;
}

/**
 * Cleans up the file system and closes all open files
 */
void shutdown_file_system()
{
    // Close any remaining open files
    while (g_num_open_files > 0)
    {
        close_file(g_open_files[0]);
    }
}

// Placeholder functions - these would need to be implemented or linked
static size_t get_current_level_size() { return 0; }
static uint32_t get_current_play_time() { return 0; }
static int get_current_level() { return 0; }
static int get_current_score() { return 0; }
static int get_player_lives() { return 0; }
static void set_current_level(int level) { }
static void set_current_score(int score) { }
static void set_player_lives(int lives) { }
static char g_save_game_data[MAX_SAVE_SIZE];
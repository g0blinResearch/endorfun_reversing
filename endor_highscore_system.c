/**
 * ========================================================================
 * ENDOR HIGH SCORE SYSTEM
 * ========================================================================
 * 
 * High score tracking and management for the Endor game engine.
 * Handles loading, saving, displaying, and updating high scores.
 * Supports multiple difficulty levels and player name entry.
 */

#include "endor_readable.h"
#include <time.h>

// ========================================================================
// HIGH SCORE CONSTANTS
// ========================================================================

#define HIGH_SCORE_FILE "endor.hsc"
#define MAX_HIGH_SCORES 10
#define MAX_PLAYER_NAME 32
#define HIGH_SCORE_VERSION 1
#define DEFAULT_HIGH_SCORE 1000

// High score file format markers
#define HSC_MAGIC "EHSC"
#define HSC_DATA "HISC"

// Dialog IDs for high score display
#define IDD_HIGH_SCORES 3000
#define IDC_HIGH_SCORE_LIST 3001
#define IDC_PLAYER_NAME 3002
#define IDC_NEW_SCORE 3003

// ========================================================================
// HIGH SCORE STRUCTURES
// ========================================================================

// High score entry
typedef struct {
    char player_name[MAX_PLAYER_NAME];
    uint32_t score;
    uint32_t level;
    uint32_t time_seconds;
    SYSTEMTIME date_achieved;
    uint32_t checksum;
} HighScoreEntry;

// High score table
typedef struct {
    char magic[4];                          // "EHSC"
    uint32_t version;
    uint32_t entry_count;
    HighScoreEntry entries[MAX_HIGH_SCORES];
    uint32_t table_checksum;
} HighScoreTable;

// ========================================================================
// HIGH SCORE GLOBALS
// ========================================================================

// Current high score table
static HighScoreTable g_high_score_table;
static BOOL g_high_scores_loaded = FALSE;
static BOOL g_high_scores_modified = FALSE;

// New score entry data
static uint32_t g_pending_score = 0;
static uint32_t g_pending_level = 0;
static uint32_t g_pending_time = 0;
static char g_pending_name[MAX_PLAYER_NAME] = "";

// High score file path
static char g_high_score_path[MAX_PATH] = "";

// ========================================================================
// UTILITY FUNCTIONS
// ========================================================================

/**
 * Calculates checksum for a high score entry
 * @param entry High score entry
 * @return Checksum value
 */
static uint32_t calculate_entry_checksum(const HighScoreEntry* entry)
{
    uint32_t checksum = 0x12345678;
    const uint8_t* data = (const uint8_t*)entry;
    
    // Calculate checksum over all fields except the checksum itself
    for (size_t i = 0; i < offsetof(HighScoreEntry, checksum); i++)
    {
        checksum = (checksum << 1) ^ data[i];
        if (checksum & 0x80000000)
            checksum ^= 0x04C11DB7;  // CRC polynomial
    }
    
    return checksum;
}

/**
 * Calculates checksum for the entire high score table
 * @param table High score table
 * @return Checksum value
 */
static uint32_t calculate_table_checksum(const HighScoreTable* table)
{
    uint32_t checksum = 0x87654321;
    const uint8_t* data = (const uint8_t*)table;
    
    // Calculate checksum over all fields except the table checksum
    for (size_t i = 0; i < offsetof(HighScoreTable, table_checksum); i++)
    {
        checksum = (checksum >> 1) ^ data[i];
        if (checksum & 1)
            checksum ^= 0xEDB88320;  // CRC polynomial (reversed)
    }
    
    return checksum;
}

/**
 * Validates a high score entry
 * @param entry High score entry to validate
 * @return TRUE if valid, FALSE otherwise
 */
static BOOL validate_entry(const HighScoreEntry* entry)
{
    // Check name is null-terminated
    BOOL name_valid = FALSE;
    for (int i = 0; i < MAX_PLAYER_NAME; i++)
    {
        if (entry->player_name[i] == '\0')
        {
            name_valid = TRUE;
            break;
        }
    }
    
    if (!name_valid)
        return FALSE;
    
    // Verify checksum
    uint32_t calculated = calculate_entry_checksum(entry);
    return calculated == entry->checksum;
}

/**
 * Formats a high score entry for display
 * @param entry High score entry
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 */
static void format_high_score_entry(const HighScoreEntry* entry, char* buffer, size_t buffer_size)
{
    char date_str[32];
    sprintf(date_str, "%02d/%02d/%04d", 
            entry->date_achieved.wMonth,
            entry->date_achieved.wDay,
            entry->date_achieved.wYear);
    
    uint32_t minutes = entry->time_seconds / 60;
    uint32_t seconds = entry->time_seconds % 60;
    
    _snprintf(buffer, buffer_size, "%-20s %8u  Level %2u  %02u:%02u  %s",
              entry->player_name,
              entry->score,
              entry->level,
              minutes, seconds,
              date_str);
}

// ========================================================================
// HIGH SCORE TABLE MANAGEMENT
// ========================================================================

/**
 * Initializes the high score system
 * @param data_path Path to game data directory
 * @return TRUE if successful
 */
BOOL initialize_high_score_system(const char* data_path)
{
    // Build high score file path
    _snprintf(g_high_score_path, sizeof(g_high_score_path), 
              "%s\\%s", data_path, HIGH_SCORE_FILE);
    
    // Initialize default table
    memset(&g_high_score_table, 0, sizeof(g_high_score_table));
    memcpy(g_high_score_table.magic, HSC_MAGIC, 4);
    g_high_score_table.version = HIGH_SCORE_VERSION;
    g_high_score_table.entry_count = 0;
    
    // Create default entries
    for (int i = 0; i < MAX_HIGH_SCORES; i++)
    {
        HighScoreEntry* entry = &g_high_score_table.entries[i];
        strcpy(entry->player_name, "Anonymous");
        entry->score = DEFAULT_HIGH_SCORE - (i * 100);
        entry->level = 1;
        entry->time_seconds = 300 + (i * 60);  // 5-15 minutes
        GetLocalTime(&entry->date_achieved);
        entry->checksum = calculate_entry_checksum(entry);
    }
    
    g_high_score_table.entry_count = MAX_HIGH_SCORES;
    g_high_score_table.table_checksum = calculate_table_checksum(&g_high_score_table);
    
    return TRUE;
}

/**
 * Loads high scores from file
 * @return TRUE if successful
 */
BOOL load_high_scores(void)  // Improved from BLD_ShowHighScoresDlgProc reference
{
    FILE* file = fopen(g_high_score_path, "rb");
    if (!file)
    {
        // File doesn't exist, use defaults
        g_high_scores_loaded = TRUE;
        return TRUE;
    }
    
    // Read table
    HighScoreTable temp_table;
    size_t read = fread(&temp_table, 1, sizeof(temp_table), file);
    fclose(file);
    
    if (read != sizeof(temp_table))
    {
        OutputDebugString("High score file corrupted - wrong size\n");
        return FALSE;
    }
    
    // Validate magic
    if (memcmp(temp_table.magic, HSC_MAGIC, 4) != 0)
    {
        OutputDebugString("High score file corrupted - invalid magic\n");
        return FALSE;
    }
    
    // Validate version
    if (temp_table.version != HIGH_SCORE_VERSION)
    {
        OutputDebugString("High score file version mismatch\n");
        return FALSE;
    }
    
    // Validate table checksum
    uint32_t calculated_checksum = calculate_table_checksum(&temp_table);
    if (calculated_checksum != temp_table.table_checksum)
    {
        OutputDebugString("High score table checksum failed\n");
        return FALSE;
    }
    
    // Validate individual entries
    for (uint32_t i = 0; i < temp_table.entry_count && i < MAX_HIGH_SCORES; i++)
    {
        if (!validate_entry(&temp_table.entries[i]))
        {
            char msg[128];
            sprintf(msg, "High score entry %u validation failed\n", i);
            OutputDebugString(msg);
            return FALSE;
        }
    }
    
    // All validations passed, accept the table
    memcpy(&g_high_score_table, &temp_table, sizeof(g_high_score_table));
    g_high_scores_loaded = TRUE;
    g_high_scores_modified = FALSE;
    
    return TRUE;
}

/**
 * Saves high scores to file
 * @return TRUE if successful
 */
BOOL save_high_scores(void)
{
    if (!g_high_scores_modified)
        return TRUE;  // Nothing to save
    
    // Update checksums
    for (uint32_t i = 0; i < g_high_score_table.entry_count; i++)
    {
        g_high_score_table.entries[i].checksum = 
            calculate_entry_checksum(&g_high_score_table.entries[i]);
    }
    g_high_score_table.table_checksum = calculate_table_checksum(&g_high_score_table);
    
    // Write to file
    FILE* file = fopen(g_high_score_path, "wb");
    if (!file)
    {
        OutputDebugString("Failed to open high score file for writing\n");
        return FALSE;
    }
    
    size_t written = fwrite(&g_high_score_table, 1, sizeof(g_high_score_table), file);
    fclose(file);
    
    if (written != sizeof(g_high_score_table))
    {
        OutputDebugString("Failed to write complete high score table\n");
        return FALSE;
    }
    
    g_high_scores_modified = FALSE;
    return TRUE;
}

// ========================================================================
// HIGH SCORE OPERATIONS
// ========================================================================

/**
 * Checks if a score qualifies for the high score table
 * @param score Score to check
 * @return Position in table (0-based) or -1 if not qualified
 */
int check_high_score(uint32_t score)
{
    if (!g_high_scores_loaded)
        load_high_scores();
    
    // Find insertion position
    for (int i = 0; i < MAX_HIGH_SCORES; i++)
    {
        if (i >= (int)g_high_score_table.entry_count || 
            score > g_high_score_table.entries[i].score)
        {
            return i;
        }
    }
    
    return -1;  // Score too low
}

/**
 * Adds a new high score to the table
 * @param name Player name
 * @param score Score achieved
 * @param level Level reached
 * @param time_seconds Time taken
 * @return Position in table or -1 if not added
 */
int add_high_score(const char* name, uint32_t score, uint32_t level, uint32_t time_seconds)
{
    int position = check_high_score(score);
    if (position < 0)
        return -1;
    
    // Shift lower scores down
    for (int i = MAX_HIGH_SCORES - 1; i > position; i--)
    {
        memcpy(&g_high_score_table.entries[i], 
               &g_high_score_table.entries[i - 1],
               sizeof(HighScoreEntry));
    }
    
    // Insert new entry
    HighScoreEntry* entry = &g_high_score_table.entries[position];
    memset(entry, 0, sizeof(HighScoreEntry));
    
    strncpy(entry->player_name, name, MAX_PLAYER_NAME - 1);
    entry->player_name[MAX_PLAYER_NAME - 1] = '\0';
    entry->score = score;
    entry->level = level;
    entry->time_seconds = time_seconds;
    GetLocalTime(&entry->date_achieved);
    entry->checksum = calculate_entry_checksum(entry);
    
    // Update table info
    if (g_high_score_table.entry_count < MAX_HIGH_SCORES)
        g_high_score_table.entry_count++;
    
    g_high_scores_modified = TRUE;
    save_high_scores();
    
    return position;
}

/**
 * Gets a high score entry
 * @param index Entry index (0-based)
 * @param entry Output entry structure
 * @return TRUE if successful
 */
BOOL get_high_score_entry(int index, HighScoreEntry* entry)
{
    if (!g_high_scores_loaded)
        load_high_scores();
    
    if (index < 0 || index >= (int)g_high_score_table.entry_count)
        return FALSE;
    
    memcpy(entry, &g_high_score_table.entries[index], sizeof(HighScoreEntry));
    return TRUE;
}

/**
 * Gets the number of high score entries
 * @return Number of entries
 */
int get_high_score_count(void)
{
    if (!g_high_scores_loaded)
        load_high_scores();
    
    return g_high_score_table.entry_count;
}

/**
 * Clears all high scores
 */
void clear_high_scores(void)
{
    g_high_score_table.entry_count = 0;
    g_high_scores_modified = TRUE;
    save_high_scores();
}

// ========================================================================
// HIGH SCORE DIALOG
// ========================================================================

/**
 * High score dialog procedure
 * @param hDlg Dialog handle
 * @param message Message ID
 * @param wParam Message parameter
 * @param lParam Message parameter
 * @return TRUE if handled
 */
INT_PTR CALLBACK high_score_dialog_proc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
        case WM_INITDIALOG:
        {
            // Center dialog
            RECT rcDlg, rcDesktop;
            GetWindowRect(hDlg, &rcDlg);
            GetWindowRect(GetDesktopWindow(), &rcDesktop);
            
            int x = (rcDesktop.right - rcDlg.right + rcDlg.left) / 2;
            int y = (rcDesktop.bottom - rcDlg.bottom + rcDlg.top) / 2;
            SetWindowPos(hDlg, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
            
            // Set fixed-width font for list
            HFONT hFont = CreateFont(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                   ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                   DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN, "Courier New");
            
            HWND hList = GetDlgItem(hDlg, IDC_HIGH_SCORE_LIST);
            SendMessage(hList, WM_SETFONT, (WPARAM)hFont, TRUE);
            
            // Load and display high scores
            if (!g_high_scores_loaded)
                load_high_scores();
            
            // Add header
            SendMessage(hList, LB_ADDSTRING, 0, 
                       (LPARAM)"Name                    Score  Level  Time    Date");
            SendMessage(hList, LB_ADDSTRING, 0, 
                       (LPARAM)"----                    -----  -----  ----    ----");
            
            // Add entries
            for (int i = 0; i < get_high_score_count(); i++)
            {
                HighScoreEntry entry;
                if (get_high_score_entry(i, &entry))
                {
                    char buffer[256];
                    format_high_score_entry(&entry, buffer, sizeof(buffer));
                    SendMessage(hList, LB_ADDSTRING, 0, (LPARAM)buffer);
                }
            }
            
            // Check if we have a pending score to add
            if (g_pending_score > 0)
            {
                // Show name entry controls
                ShowWindow(GetDlgItem(hDlg, IDC_PLAYER_NAME), SW_SHOW);
                ShowWindow(GetDlgItem(hDlg, IDC_NEW_SCORE), SW_SHOW);
                
                // Display pending score info
                char score_text[128];
                sprintf(score_text, "New High Score: %u (Level %u)", 
                        g_pending_score, g_pending_level);
                SetDlgItemText(hDlg, IDC_NEW_SCORE, score_text);
                
                // Focus on name entry
                SetFocus(GetDlgItem(hDlg, IDC_PLAYER_NAME));
            }
            else
            {
                // Hide name entry controls
                ShowWindow(GetDlgItem(hDlg, IDC_PLAYER_NAME), SW_HIDE);
                ShowWindow(GetDlgItem(hDlg, IDC_NEW_SCORE), SW_HIDE);
            }
            
            return TRUE;
        }
        
        case WM_COMMAND:
        {
            switch (LOWORD(wParam))
            {
                case IDOK:
                    // If entering a new score, save it first
                    if (g_pending_score > 0)
                    {
                        GetDlgItemText(hDlg, IDC_PLAYER_NAME, g_pending_name, MAX_PLAYER_NAME);
                        if (strlen(g_pending_name) == 0)
                            strcpy(g_pending_name, "Anonymous");
                        
                        add_high_score(g_pending_name, g_pending_score, 
                                     g_pending_level, g_pending_time);
                        
                        // Clear pending data
                        g_pending_score = 0;
                        g_pending_level = 0;
                        g_pending_time = 0;
                        g_pending_name[0] = '\0';
                    }
                    
                    // Clean up font
                    HWND hList = GetDlgItem(hDlg, IDC_HIGH_SCORE_LIST);
                    HFONT hFont = (HFONT)SendMessage(hList, WM_GETFONT, 0, 0);
                    if (hFont)
                        DeleteObject(hFont);
                    
                    EndDialog(hDlg, IDOK);
                    return TRUE;
                    
                case IDCANCEL:
                    // Clean up font
                    hList = GetDlgItem(hDlg, IDC_HIGH_SCORE_LIST);
                    hFont = (HFONT)SendMessage(hList, WM_GETFONT, 0, 0);
                    if (hFont)
                        DeleteObject(hFont);
                    
                    EndDialog(hDlg, IDCANCEL);
                    return TRUE;
            }
            break;
        }
    }
    
    return FALSE;
}

/**
 * Shows the high score dialog
 * @param hWndParent Parent window
 * @param score New score to add (0 for view only)
 * @param level Level reached
 * @param time_seconds Time taken
 * @return Dialog result
 */
int show_high_score_dialog(HWND hWndParent, uint32_t score, uint32_t level, uint32_t time_seconds)
{
    // Set pending score data if provided
    g_pending_score = score;
    g_pending_level = level;
    g_pending_time = time_seconds;
    g_pending_name[0] = '\0';
    
    // Show dialog
    return DialogBox(GetModuleHandle(NULL), 
                    MAKEINTRESOURCE(IDD_HIGH_SCORES),
                    hWndParent,
                    high_score_dialog_proc);
}

// ========================================================================
// EXPORTED FUNCTIONS
// ========================================================================

/**
 * Checks and potentially shows high score dialog for a new score
 * @param hWndParent Parent window
 * @param score Score achieved
 * @param level Level reached  
 * @param time_seconds Time taken
 * @return TRUE if score was added to high score table
 */
BOOL process_game_score(HWND hWndParent, uint32_t score, uint32_t level, uint32_t time_seconds)
{
    int position = check_high_score(score);
    if (position >= 0)
    {
        // Score qualifies, show dialog for name entry
        show_high_score_dialog(hWndParent, score, level, time_seconds);
        return TRUE;
    }
    
    return FALSE;
}

/**
 * Shows high score table (view only)
 * @param hWndParent Parent window
 */
void view_high_scores(HWND hWndParent)
{
    show_high_score_dialog(hWndParent, 0, 0, 0);
}

/**
 * Exports high scores to text file
 * @param filename Output filename
 * @return TRUE if successful
 */
BOOL export_high_scores(const char* filename)
{
    if (!g_high_scores_loaded)
        load_high_scores();
    
    FILE* file = fopen(filename, "w");
    if (!file)
        return FALSE;
    
    fprintf(file, "ENDOR HIGH SCORES\n");
    fprintf(file, "=================\n\n");
    fprintf(file, "%-4s %-20s %8s %6s %8s %12s\n",
            "Rank", "Player", "Score", "Level", "Time", "Date");
    fprintf(file, "---- -------------------- -------- ------ -------- ------------\n");
    
    for (int i = 0; i < get_high_score_count(); i++)
    {
        HighScoreEntry entry;
        if (get_high_score_entry(i, &entry))
        {
            char date_str[32];
            sprintf(date_str, "%02d/%02d/%04d", 
                    entry.date_achieved.wMonth,
                    entry.date_achieved.wDay,
                    entry.date_achieved.wYear);
            
            uint32_t minutes = entry.time_seconds / 60;
            uint32_t seconds = entry.time_seconds % 60;
            
            fprintf(file, "%-4d %-20s %8u %6u %02u:%02u:00 %s\n",
                    i + 1,
                    entry.player_name,
                    entry.score,
                    entry.level,
                    minutes, seconds,
                    date_str);
        }
    }
    
    fclose(file);
    return TRUE;
}

/**
 * Shutdown high score system
 */
void shutdown_high_score_system(void)
{
    // Save any pending changes
    if (g_high_scores_modified)
        save_high_scores();
    
    // Clear data
    memset(&g_high_score_table, 0, sizeof(g_high_score_table));
    g_high_scores_loaded = FALSE;
    g_high_scores_modified = FALSE;
}
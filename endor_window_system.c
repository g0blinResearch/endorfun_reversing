/**
 * ========================================================================
 * ENDOR WINDOW AND DIALOG MANAGEMENT SYSTEM
 * ========================================================================
 * 
 * Handles all window creation, message processing, dialog management,
 * and UI-related functionality for the Endor game engine.
 * Includes main window handling, child windows, custom controls,
 * and dialog procedures.
 */

#include "endor_readable.h"
#include <commctrl.h>
#include <richedit.h>

// ========================================================================
// WINDOW SYSTEM CONSTANTS
// ========================================================================

#define MAIN_WINDOW_CLASS "EndorMainWindow"
#define CHILD_WINDOW_CLASS "EndorChildWindow"
#define CUSTOM_CONTROL_CLASS "EndorCustomControl"

// Custom window messages
#define MSG_QUERY_STATE 0x229
#define MSG_SET_STATE 0x221
#define MSG_UPDATE_DISPLAY 0x230
#define MSG_GAME_EVENT 0x240

// Window properties
#define PROP_WINDOW_DATA "ENDORDATA"
#define PROP_BRUSH "BLDPROPHBRUSH"
#define PROP_FONT "BLDPROPFONT"

// Dialog IDs
#define DLG_MAIN_MENU 1000
#define DLG_OPTIONS 1001
#define DLG_HIGH_SCORES 1002
#define DLG_ABOUT 1003
#define DLG_SAVE_GAME 1004
#define DLG_LOAD_GAME 1005
#define DLG_MOUSE_SENSITIVITY 1006
// Dialog control IDs
#define IDC_SENSITIVITY_SLIDER 2001
#define IDC_SENSITIVITY_TEXT 2002
#define IDC_HIGHSCORE_LIST 2003
#define IDC_CLEAR_SCORES 2004

// ========================================================================
// WINDOW SYSTEM GLOBALS
// ========================================================================

// Main application window
static HWND g_main_window = NULL;          // was data_435fc8
static HWND g_parent_window = NULL;        // was data_435f9c
static HINSTANCE g_app_instance = NULL;    // was data_435f98

// Window state
static BOOL g_window_active = FALSE;
static BOOL g_fullscreen_mode = FALSE;
static int g_window_width = 800;
static int g_window_height = 600;

// Dialog state
static HWND g_active_dialog = NULL;
static int g_dialog_result = 0;

// ========================================================================
// WINDOW MESSAGE HANDLING
// ========================================================================

/**
 * Sends a message to the main game window
 * @param message_id The Windows message ID to send
 * @return 1 on success, 0 if no window exists
 * 
 * Special handling for message 0x229 (query state message)
 * which triggers a follow-up message 0x221 with the query result
 */
int32_t send_game_window_message(uint32_t message_id)  // was sub_4012d0
{
    HWND main_window = g_main_window;
    
    if (!main_window)
        return 0;
    
    // Special handling for query message
    if (message_id != MSG_QUERY_STATE)
    {
        SendMessageA(main_window, message_id, 0, 0);
        return 1;
    }
    
    // Query state and send result back
    LRESULT query_result = SendMessageA(main_window, MSG_QUERY_STATE, 0, 0);
    
    if (query_result)
        SendMessageA(main_window, MSG_SET_STATE, query_result, 0);
    
    return 1;
}

// ========================================================================
// WINDOW PROPERTY MANAGEMENT
// ========================================================================

/**
 * Creates and sets a pattern brush for a window from a bitmap resource
 * @param window Handle to the window
 * @param bitmap_resource Resource name of the bitmap
 * @return 1 on success, 0 on failure
 */
int32_t set_window_pattern_brush(HWND window, PSTR bitmap_resource)  // was sub_401429
{
    if (!window)
        return 0;
    
    // Load the bitmap from resources
    HBITMAP bitmap = LoadBitmapA(g_app_instance, bitmap_resource);  // was j_sub_40d070
    
    if (bitmap)
    {
        // Create pattern brush from bitmap
        HBRUSH pattern_brush = CreatePatternBrush(bitmap);
        DeleteObject(bitmap);
        
        if (pattern_brush)
        {
            // Store brush as window property
            SetPropA(window, PROP_BRUSH, pattern_brush);
            return 1;
        }
    }
    
    return 0;
}

// ========================================================================
// DIALOG PROCEDURES
// ========================================================================

/**
 * Dialog procedure for mouse sensitivity settings
 * Allows user to adjust mouse sensitivity for gameplay
 */
LRESULT CALLBACK mouse_sensitivity_dialog_proc(HWND dialog, UINT message, 
                                               WPARAM wParam, LPARAM lParam)  // was BLD_SetMouseSensitiviDlgProc
{
    switch (message)
    {
        case WM_INITDIALOG:
        {
            // Center dialog on parent window
            RECT parent_rect, dialog_rect;
            GetWindowRect(GetParent(dialog), &parent_rect);
            GetWindowRect(dialog, &dialog_rect);
            
            int x = parent_rect.left + (parent_rect.right - parent_rect.left - 
                    (dialog_rect.right - dialog_rect.left)) / 2;
            int y = parent_rect.top + (parent_rect.bottom - parent_rect.top - 
                    (dialog_rect.bottom - dialog_rect.top)) / 2;
            
            SetWindowPos(dialog, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
            
            // Initialize slider control
            HWND slider = GetDlgItem(dialog, IDC_SENSITIVITY_SLIDER);
            SendMessage(slider, TBM_SETRANGE, TRUE, MAKELONG(1, 100));
            SendMessage(slider, TBM_SETPOS, TRUE, 50);  // Default to middle
            
            return TRUE;
        }
        
        case WM_COMMAND:
        {
            switch (LOWORD(wParam))
            {
                case IDOK:
                {
                    // Save sensitivity setting
                    HWND slider = GetDlgItem(dialog, IDC_SENSITIVITY_SLIDER);
                    int sensitivity = SendMessage(slider, TBM_GETPOS, 0, 0);
                    // Save to configuration
                    EndDialog(dialog, sensitivity);
                    return TRUE;
                }
                
                case IDCANCEL:
                    EndDialog(dialog, -1);
                    return TRUE;
            }
            break;
        }
        
        case WM_HSCROLL:
        {
            // Update preview text as slider moves
            if ((HWND)lParam == GetDlgItem(dialog, IDC_SENSITIVITY_SLIDER))
            {
                int pos = SendMessage((HWND)lParam, TBM_GETPOS, 0, 0);
                char buffer[32];
                sprintf(buffer, "Sensitivity: %d%%", pos);
                SetDlgItemTextA(dialog, IDC_SENSITIVITY_TEXT, buffer);
            }
            break;
        }
    }
    
    return FALSE;
}

/**
 * Dialog procedure for high scores display
 * Shows the top scores achieved by players
 */
LRESULT CALLBACK high_scores_dialog_proc(HWND dialog, UINT message,
                                         WPARAM wParam, LPARAM lParam)  // was BLD_ShowHighScoresDlgProc
{
    switch (message)
    {
        case WM_INITDIALOG:
        {
            // Load and display high scores
            HWND list = GetDlgItem(dialog, IDC_HIGHSCORE_LIST);
            
            // Add column headers
            LVCOLUMN col = {0};
            col.mask = LVCF_TEXT | LVCF_WIDTH;
            
            col.pszText = "Rank";
            col.cx = 50;
            ListView_InsertColumn(list, 0, &col);
            
            col.pszText = "Name";
            col.cx = 150;
            ListView_InsertColumn(list, 1, &col);
            
            col.pszText = "Score";
            col.cx = 100;
            ListView_InsertColumn(list, 2, &col);
            
            col.pszText = "Level";
            col.cx = 80;
            ListView_InsertColumn(list, 3, &col);
            
            col.pszText = "Date";
            col.cx = 120;
            ListView_InsertColumn(list, 4, &col);
            
            // Load scores from file/registry
            // populate_high_scores_list(list);
            
            return TRUE;
        }
        
        case WM_COMMAND:
        {
            switch (LOWORD(wParam))
            {
                case IDOK:
                case IDCANCEL:
                    EndDialog(dialog, 0);
                    return TRUE;
                    
                case IDC_CLEAR_SCORES:
                    if (MessageBoxA(dialog, 
                                   "Are you sure you want to clear all high scores?",
                                   "Confirm Clear", 
                                   MB_YESNO | MB_ICONQUESTION) == IDYES)
                    {
                        // Clear high scores
                        // clear_high_scores();
                        // Refresh list
                    }
                    return TRUE;
            }
            break;
        }
    }
    
    return FALSE;
}

// ========================================================================
// MAIN WINDOW PROCEDURES
// ========================================================================

/**
 * Main window procedure for the game overlay/score display
 */
LRESULT CALLBACK overlay_window_proc(HWND window, UINT message,
                                    WPARAM wParam, LPARAM lParam)  // was BLD_overcoatWndProc
{
    switch (message)
    {
        case WM_CREATE:
        {
            // Initialize overlay window
            SetWindowLongPtr(window, GWL_EXSTYLE, 
                           GetWindowLongPtr(window, GWL_EXSTYLE) | WS_EX_LAYERED);
            SetLayeredWindowAttributes(window, 0, 200, LWA_ALPHA);
            return 0;
        }
        
        case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(window, &ps);
            
            // Draw overlay content (scores, status, etc.)
            RECT rect;
            GetClientRect(window, &rect);
            
            // Use transparent background
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, RGB(255, 255, 0));
            
            // Draw game status
            char status[256];
            sprintf(status, "Score: %d  Lives: %d  Level: %d", 
                   0, 0, 0);  // Get actual values from game state
            DrawTextA(hdc, status, -1, &rect, DT_TOP | DT_RIGHT);
            
            EndPaint(window, &ps);
            return 0;
        }
        
        case WM_ERASEBKGND:
            // Don't erase background for transparency
            return 1;
            
        case MSG_UPDATE_DISPLAY:
            // Force redraw when game state changes
            InvalidateRect(window, NULL, FALSE);
            return 0;
    }
    
    return DefWindowProc(window, message, wParam, lParam);
}

/**
 * Score window procedure for displaying game scores
 */
LRESULT CALLBACK score_window_proc(HWND window, UINT message,
                                  WPARAM wParam, LPARAM lParam)  // was BLD_ef_scoreWndProc
{
    switch (message)
    {
        case WM_CREATE:
        {
            // Create score display controls
            CreateWindowA("STATIC", "SCORE", 
                         WS_CHILD | WS_VISIBLE | SS_CENTER,
                         10, 10, 100, 20,
                         window, (HMENU)1001, g_app_instance, NULL);
                         
            CreateWindowA("STATIC", "0", 
                         WS_CHILD | WS_VISIBLE | SS_CENTER | SS_SUNKEN,
                         10, 35, 100, 30,
                         window, (HMENU)1002, g_app_instance, NULL);
            return 0;
        }
        
        case WM_COMMAND:
        {
            // Handle score window commands
            break;
        }
        
        case MSG_UPDATE_DISPLAY:
        {
            // Update score display
            char score_text[32];
            sprintf(score_text, "%d", 0);  // Get actual score
            SetDlgItemTextA(window, 1002, score_text);
            return 0;
        }
    }
    
    return DefWindowProc(window, message, wParam, lParam);
}

// ========================================================================
// WINDOW CREATION AND MANAGEMENT
// ========================================================================

/**
 * Initializes the window system and registers window classes
 * @return TRUE on success, FALSE on failure
 */
BOOL initialize_window_system(HINSTANCE instance)
{
    g_app_instance = instance;
    
    // Register main window class
    WNDCLASSA wc = {0};
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wc.lpfnWndProc = DefWindowProc;  // Will be subclassed
    wc.hInstance = instance;
    wc.hIcon = LoadIcon(instance, MAKEINTRESOURCE(IDI_APPLICATION));
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = MAIN_WINDOW_CLASS;
    
    if (!RegisterClassA(&wc))
        return FALSE;
    
    // Register overlay window class
    wc.lpfnWndProc = overlay_window_proc;
    wc.lpszClassName = "EndorOverlay";
    wc.hbrBackground = NULL;  // Transparent
    
    if (!RegisterClassA(&wc))
        return FALSE;
    
    // Register score window class
    wc.lpfnWndProc = score_window_proc;
    wc.lpszClassName = "EndorScore";
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    
    if (!RegisterClassA(&wc))
        return FALSE;
    
    return TRUE;
}

/**
 * Creates the main game window
 * @param title Window title
 * @param width Window width
 * @param height Window height
 * @param fullscreen Whether to create in fullscreen mode
 * @return Window handle on success, NULL on failure
 */
HWND create_main_window(const char* title, int width, int height, BOOL fullscreen)
{
    DWORD style = WS_OVERLAPPEDWINDOW;
    DWORD ex_style = 0;
    
    if (fullscreen)
    {
        style = WS_POPUP;
        ex_style = WS_EX_TOPMOST;
        width = GetSystemMetrics(SM_CXSCREEN);
        height = GetSystemMetrics(SM_CYSCREEN);
    }
    
    // Adjust window rect for desired client area
    RECT rect = {0, 0, width, height};
    AdjustWindowRect(&rect, style, FALSE);
    
    g_main_window = CreateWindowExA(
        ex_style,
        MAIN_WINDOW_CLASS,
        title,
        style,
        CW_USEDEFAULT, CW_USEDEFAULT,
        rect.right - rect.left,
        rect.bottom - rect.top,
        NULL,
        NULL,
        g_app_instance,
        NULL
    );
    
    if (g_main_window)
    {
        g_window_width = width;
        g_window_height = height;
        g_fullscreen_mode = fullscreen;
        
        ShowWindow(g_main_window, SW_SHOW);
        UpdateWindow(g_main_window);
    }
    
    return g_main_window;
}

/**
 * Shuts down the window system and cleans up resources
 */
void shutdown_window_system()
{
    if (g_main_window)
    {
        DestroyWindow(g_main_window);
        g_main_window = NULL;
    }
    
    UnregisterClassA(MAIN_WINDOW_CLASS, g_app_instance);
    UnregisterClassA("EndorOverlay", g_app_instance);
    UnregisterClassA("EndorScore", g_app_instance);
}

// ========================================================================
// UTILITY FUNCTIONS
// ========================================================================

/**
 * Centers a window on the screen or parent window
 * @param window Window to center
 * @param parent Parent window (NULL for screen)
 */
void center_window(HWND window, HWND parent)
{
    RECT window_rect, parent_rect;
    GetWindowRect(window, &window_rect);
    
    if (parent)
    {
        GetWindowRect(parent, &parent_rect);
    }
    else
    {
        // Center on screen
        parent_rect.left = 0;
        parent_rect.top = 0;
        parent_rect.right = GetSystemMetrics(SM_CXSCREEN);
        parent_rect.bottom = GetSystemMetrics(SM_CYSCREEN);
    }
    
    int width = window_rect.right - window_rect.left;
    int height = window_rect.bottom - window_rect.top;
    int x = parent_rect.left + (parent_rect.right - parent_rect.left - width) / 2;
    int y = parent_rect.top + (parent_rect.bottom - parent_rect.top - height) / 2;
    
    SetWindowPos(window, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
}

/**
 * Shows a modal dialog and returns the result
 * @param dialog_id Dialog resource ID
 * @param parent Parent window
 * @return Dialog result
 */
int show_modal_dialog(int dialog_id, HWND parent)
{
    DLGPROC proc = NULL;
    
    switch (dialog_id)
    {
        case DLG_HIGH_SCORES:
            proc = (DLGPROC)high_scores_dialog_proc;
            break;
        case DLG_MOUSE_SENSITIVITY:
            proc = (DLGPROC)mouse_sensitivity_dialog_proc;
            break;
        // Add other dialog procedures as needed
    }
    
    if (proc)
    {
        return DialogBoxParamA(g_app_instance, 
                              MAKEINTRESOURCE(dialog_id),
                              parent,
                              proc,
                              0);
    }
    
    return -1;
}
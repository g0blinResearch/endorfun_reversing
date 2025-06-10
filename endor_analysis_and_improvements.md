# Endor Project Analysis and Improvement Plan

## Current State Analysis

### Original endor.c File
The original decompiled endor.c file contains approximately 30,000+ lines with:
- **Wrapper Functions**: Many `j_sub_*` functions that are just tailcalls to actual implementations
- **Implementation Functions**: `sub_*` functions with actual logic
- **Named Functions**: Some functions with meaningful names like `BLD_SetMouseSensitiviDlgProc`, `BLD_ShowHighScoresDlgProc`, etc.

### Extracted Files Status

1. **endor_audio_system.c** - Basic audio system implementation
2. **endor_game_logic.c** - Game mechanics, AI, and state management
3. **endor_graphics_system.c** - 3D rendering and graphics pipeline
4. **endor_input_system.c** - Input handling system
5. **endor_level_editor.c** - Level editor functionality
6. **endor_network_system.c** - Networking and multiplayer
7. **endor_readable.h** - Common header with type definitions

## Missing Functionality Analysis

From the original endor.c, the following categories of functions need to be extracted:

### 1. Window Management Functions
- Window creation and message handling
- Dialog procedures
- Window property management
- Functions like `sub_4012d0` (message sending), `sub_401429` (brush creation)

### 2. File I/O and Resource Management
- Level file loading/saving
- Configuration file handling
- Resource loading (bitmaps, sounds, etc.)
- Functions dealing with `.elv`, `.esg`, `.bvw` files

### 3. Memory Management
- Custom memory allocation/deallocation
- Memory tracking and debugging
- Functions like `sub_4026b0`, `sub_402700` (memory initialization)

### 4. Palette and Color Management
- Palette manipulation functions
- Color conversion routines
- Functions like `sub_404510` (system palette handling)

### 5. Mathematical and Utility Functions
- Matrix operations
- Vector calculations
- String manipulation
- Data conversion utilities

### 6. High Score System
- Score tracking and management
- High score dialog handling
- Persistence of score data

### 7. Configuration System
- Settings management
- Registry access
- User preferences

## Improvement Plan

### Phase 1: Extract Missing Functionality

Create new files for uncategorized functions:

1. **endor_window_system.c** - Window and dialog management
2. **endor_file_system.c** - File I/O and resource loading
3. **endor_memory_system.c** - Memory management utilities
4. **endor_palette_system.c** - Color and palette handling
5. **endor_math_utils.c** - Mathematical utilities
6. **endor_highscore_system.c** - High score management
7. **endor_config_system.c** - Configuration and settings

### Phase 2: Improve Readability

For each extracted file:

1. **Rename Functions**: Convert `sub_XXXXXX` to meaningful names based on functionality
2. **Rename Variables**: 
   - `data_XXXXXX` → meaningful global names
   - Local variables to descriptive names
   - Parameters to indicate purpose

3. **Add Documentation**:
   - Function headers with purpose, parameters, return values
   - Inline comments for complex logic
   - File headers with subsystem overview

4. **Improve Structure**:
   - Group related functions together
   - Add section headers
   - Define constants for magic numbers

### Phase 3: Cross-Reference and Validation

1. **Ensure Complete Coverage**: Every function from original endor.c is accounted for
2. **Update Headers**: Ensure endor_readable.h has all necessary declarations
3. **Remove Duplicates**: Eliminate wrapper functions where possible
4. **Test Compilation**: Ensure all files compile together

## Example Improvements

### Before (from endor.c):
```c
int32_t sub_4012d0(uint32_t arg1)
{
    HWND hWnd = data_435fc8;
    
    if (!hWnd)
        return 0;
    
    if (arg1 != 0x229)
    {
        SendMessageA(hWnd, arg1, 0, 0);
        return 1;
    }
    
    LRESULT wParam = SendMessageA(hWnd, 0x229, 0, 0);
    
    if (wParam)
        SendMessageA(data_435fc8, 0x221, wParam, 0);
    
    return 1;
}
```

### After (in endor_window_system.c):
```c
/**
 * Sends a message to the main game window
 * @param message_id The Windows message ID to send
 * @return 1 on success, 0 if no window exists
 * 
 * Special handling for message 0x229 (possibly a custom query message)
 * which triggers a follow-up message 0x221 with the query result
 */
int32_t send_game_window_message(uint32_t message_id)
{
    HWND main_window = g_main_game_window;  // was data_435fc8
    
    if (!main_window)
        return 0;
    
    // Special handling for query message
    if (message_id != MSG_QUERY_STATE)  // 0x229
    {
        SendMessageA(main_window, message_id, 0, 0);
        return 1;
    }
    
    // Query state and send result back
    LRESULT query_result = SendMessageA(main_window, MSG_QUERY_STATE, 0, 0);
    
    if (query_result)
        SendMessageA(main_window, MSG_SET_STATE, query_result, 0);  // 0x221
    
    return 1;
}
```

## Next Steps

1. Create the new system files
2. Move functions from endor.c to appropriate files
3. Rename and document each function
4. Update endor_readable.h with new declarations
5. Create a mapping document showing old → new function names
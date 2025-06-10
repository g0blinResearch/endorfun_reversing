/**
 * ========================================================================
 * ENDOR PALETTE AND COLOR MANAGEMENT SYSTEM
 * ========================================================================
 * 
 * Handles color palette operations, palette animation, color conversion,
 * and system palette management for the Endor game engine.
 * Supports 256-color palettes with smooth transitions and effects.
 */

#include "endor_readable.h"
#include <math.h>

// ========================================================================
// PALETTE CONSTANTS
// ========================================================================

#define MAX_PALETTE_ENTRIES 256
#define MAX_PALETTES 16
#define PALETTE_FADE_STEPS 32
#define COLOR_COMPONENT_MAX 255

// Palette flags
#define PAL_FLAG_SYSTEM 0x01      // System palette (don't modify)
#define PAL_FLAG_ANIMATED 0x02    // Animated palette
#define PAL_FLAG_LOCKED 0x04      // Locked entries
#define PAL_FLAG_DIRTY 0x08       // Needs update

// ========================================================================
// PALETTE STRUCTURES
// ========================================================================

// Color entry structure
typedef struct {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
    uint8_t flags;
} ColorEntry;

// Palette structure
typedef struct {
    ColorEntry entries[MAX_PALETTE_ENTRIES];
    char name[32];
    uint32_t flags;
    int reference_count;
} Palette;

// Palette animation data
typedef struct {
    int start_index;
    int end_index;
    int current_offset;
    int speed;
    int direction;
    BOOL active;
} PaletteAnimation;

// ========================================================================
// PALETTE GLOBALS
// ========================================================================

// Palette storage
static Palette g_palettes[MAX_PALETTES];
static int g_current_palette = 0;
static int g_num_palettes = 0;

// System palette cache
static PALETTEENTRY g_system_palette[MAX_PALETTE_ENTRIES];
static BOOL g_system_palette_saved = FALSE;

// Palette animation
static PaletteAnimation g_animations[8];
static int g_num_animations = 0;

// Hardware palette
static HPALETTE g_hardware_palette = NULL;
static HDC g_palette_dc = NULL;

// Fade state
static BOOL g_fade_active = FALSE;
static Palette g_fade_target;
static int g_fade_step = 0;
static int g_fade_speed = 1;

// ========================================================================
// PALETTE MANAGEMENT
// ========================================================================

/**
 * Initializes the palette system
 * @return TRUE on success, FALSE on failure
 */
BOOL initialize_palette_system()
{
    // Clear all palettes
    memset(g_palettes, 0, sizeof(g_palettes));
    g_num_palettes = 0;
    g_current_palette = -1;
    
    // Clear animations
    memset(g_animations, 0, sizeof(g_animations));
    g_num_animations = 0;
    
    // Create default palette (grayscale)
    create_default_palette();
    
    return TRUE;
}

/**
 * Shuts down the palette system
 */
void shutdown_palette_system()
{
    // Restore system palette if saved
    if (g_system_palette_saved && g_palette_dc)
    {
        SetSystemPaletteUse(g_palette_dc, SYSPAL_STATIC);
    }
    
    // Delete hardware palette
    if (g_hardware_palette)
    {
        DeleteObject(g_hardware_palette);
        g_hardware_palette = NULL;
    }
    
    // Release DC
    if (g_palette_dc)
    {
        ReleaseDC(NULL, g_palette_dc);
        g_palette_dc = NULL;
    }
}

/**
 * Creates a default grayscale palette
 */
void create_default_palette()
{
    Palette* pal = &g_palettes[0];
    strcpy(pal->name, "Default");
    
    // Create grayscale ramp
    for (int i = 0; i < 256; i++)
    {
        pal->entries[i].red = i;
        pal->entries[i].green = i;
        pal->entries[i].blue = i;
        pal->entries[i].flags = 0;
    }
    
    // Reserve system colors
    for (int i = 0; i < 10; i++)
    {
        pal->entries[i].flags |= PAL_FLAG_SYSTEM;
        pal->entries[246 + i].flags |= PAL_FLAG_SYSTEM;
    }
    
    g_num_palettes = 1;
    g_current_palette = 0;
}

// ========================================================================
// PALETTE OPERATIONS
// ========================================================================

/**
 * Sets up the system palette for exclusive use
 * @param window Window requesting palette control
 * @param palette Palette to realize
 * @return Number of entries realized
 *
 * This function is based on the original sub_404510
 */
int setup_system_palette(HWND window, HPALETTE palette)
{
    if (!window || !palette)
        return 0;
    
    // Get window DC
    HDC dc = GetDC(window);
    if (!dc)
        return 0;
    
    // Check if we can use palette
    int raster_caps = GetDeviceCaps(dc, RASTERCAPS);
    int color_depth = GetDeviceCaps(dc, BITSPIXEL);
    int num_colors = GetDeviceCaps(dc, SIZEPALETTE);
    
    if (!(raster_caps & RC_PALETTE) || num_colors > 256)
    {
        ReleaseDC(window, dc);
        return 0;
    }
    
    // Save system palette
    if (!g_system_palette_saved)
    {
        GetSystemPaletteEntries(dc, 0, num_colors, g_system_palette);
        g_system_palette_saved = TRUE;
    }
    
    // Set palette use to no static colors
    SetSystemPaletteUse(dc, SYSPAL_NOSTATIC);
    
    // Select and realize palette
    HPALETTE old_pal = SelectPalette(dc, palette, FALSE);
    int realized = RealizePalette(dc);
    
    // Restore old palette
    SelectPalette(dc, old_pal, FALSE);
    
    // Set back to static colors
    SetSystemPaletteUse(dc, SYSPAL_STATIC);
    
    ReleaseDC(window, dc);
    g_palette_dc = dc;
    
    return realized;
}

/**
 * Creates a hardware palette from the current software palette
 * @return Handle to the created palette
 */
HPALETTE create_hardware_palette()
{
    if (g_current_palette < 0)
        return NULL;
    
    // Allocate logical palette structure
    LOGPALETTE* log_pal = (LOGPALETTE*)malloc(
        sizeof(LOGPALETTE) + (MAX_PALETTE_ENTRIES - 1) * sizeof(PALETTEENTRY));
    
    if (!log_pal)
        return NULL;
    
    // Fill palette structure
    log_pal->palVersion = 0x300;
    log_pal->palNumEntries = MAX_PALETTE_ENTRIES;
    
    Palette* pal = &g_palettes[g_current_palette];
    for (int i = 0; i < MAX_PALETTE_ENTRIES; i++)
    {
        log_pal->palPalEntry[i].peRed = pal->entries[i].red;
        log_pal->palPalEntry[i].peGreen = pal->entries[i].green;
        log_pal->palPalEntry[i].peBlue = pal->entries[i].blue;
        log_pal->palPalEntry[i].peFlags = 
            (pal->entries[i].flags & PAL_FLAG_SYSTEM) ? PC_NOCOLLAPSE : 0;
    }
    
    // Create palette
    HPALETTE hpal = CreatePalette(log_pal);
    free(log_pal);
    
    // Update hardware palette
    if (hpal && g_hardware_palette)
    {
        DeleteObject(g_hardware_palette);
    }
    g_hardware_palette = hpal;
    
    return hpal;
}

/**
 * Loads a palette from a file
 * @param filename Palette file to load
 * @return Palette index on success, -1 on failure
 */
int load_palette_from_file(const char* filename)
{
    if (g_num_palettes >= MAX_PALETTES)
        return -1;
    
    FILE* file = fopen(filename, "rb");
    if (!file)
        return -1;
    
    // Read palette data (assuming simple RGB triplets)
    Palette* pal = &g_palettes[g_num_palettes];
    
    // Read header if any
    char header[4];
    fread(header, 1, 4, file);
    
    if (memcmp(header, "RIFF", 4) == 0)
    {
        // Microsoft RIFF palette
        fseek(file, 20, SEEK_SET);  // Skip to data
    }
    else if (memcmp(header, "JASC", 4) == 0)
    {
        // JASC palette - skip header lines
        char line[256];
        fgets(line, sizeof(line), file);  // JASC-PAL
        fgets(line, sizeof(line), file);  // Version
        fgets(line, sizeof(line), file);  // Number of colors
    }
    else
    {
        // Raw RGB data
        fseek(file, 0, SEEK_SET);
    }
    
    // Read color entries
    for (int i = 0; i < MAX_PALETTE_ENTRIES; i++)
    {
        uint8_t rgb[3];
        if (fread(rgb, 1, 3, file) != 3)
            break;
        
        pal->entries[i].red = rgb[0];
        pal->entries[i].green = rgb[1];
        pal->entries[i].blue = rgb[2];
        pal->entries[i].flags = 0;
    }
    
    fclose(file);
    
    // Set palette name from filename
    const char* name = strrchr(filename, '\\');
    if (!name) name = filename; else name++;
    strncpy(pal->name, name, 31);
    pal->name[31] = '\0';
    
    return g_num_palettes++;
}

// ========================================================================
// COLOR OPERATIONS
// ========================================================================

/**
 * Converts RGB color to palette index
 * @param red Red component (0-255)
 * @param green Green component (0-255)
 * @param blue Blue component (0-255)
 * @return Closest matching palette index
 */
int rgb_to_palette_index(uint8_t red, uint8_t green, uint8_t blue)
{
    if (g_current_palette < 0)
        return 0;
    
    Palette* pal = &g_palettes[g_current_palette];
    int best_index = 0;
    int best_distance = INT_MAX;
    
    // Find closest color using color distance formula
    for (int i = 0; i < MAX_PALETTE_ENTRIES; i++)
    {
        int dr = (int)pal->entries[i].red - red;
        int dg = (int)pal->entries[i].green - green;
        int db = (int)pal->entries[i].blue - blue;
        
        // Weighted distance (eye is more sensitive to green)
        int distance = dr * dr * 30 + dg * dg * 59 + db * db * 11;
        
        if (distance < best_distance)
        {
            best_distance = distance;
            best_index = i;
        }
    }
    
    return best_index;
}

/**
 * Gets RGB values for a palette index
 * @param index Palette index
 * @param red Output for red component
 * @param green Output for green component
 * @param blue Output for blue component
 * @return TRUE on success, FALSE on invalid index
 */
BOOL palette_index_to_rgb(int index, uint8_t* red, uint8_t* green, uint8_t* blue)
{
    if (g_current_palette < 0 || index < 0 || index >= MAX_PALETTE_ENTRIES)
        return FALSE;
    
    Palette* pal = &g_palettes[g_current_palette];
    *red = pal->entries[index].red;
    *green = pal->entries[index].green;
    *blue = pal->entries[index].blue;
    
    return TRUE;
}

// ========================================================================
// PALETTE EFFECTS
// ========================================================================

/**
 * Fades the palette to black
 * @param steps Number of fade steps
 */
void fade_palette_to_black(int steps)
{
    if (g_current_palette < 0)
        return;
    
    // Set up fade target (all black)
    memset(&g_fade_target, 0, sizeof(Palette));
    strcpy(g_fade_target.name, "Black");
    
    // Initialize fade
    g_fade_active = TRUE;
    g_fade_step = 0;
    g_fade_speed = PALETTE_FADE_STEPS / steps;
}

/**
 * Fades from black to the current palette
 * @param steps Number of fade steps
 */
void fade_palette_from_black(int steps)
{
    if (g_current_palette < 0)
        return;
    
    // Save current palette as target
    memcpy(&g_fade_target, &g_palettes[g_current_palette], sizeof(Palette));
    
    // Set current to black
    Palette* pal = &g_palettes[g_current_palette];
    for (int i = 0; i < MAX_PALETTE_ENTRIES; i++)
    {
        if (!(pal->entries[i].flags & PAL_FLAG_SYSTEM))
        {
            pal->entries[i].red = 0;
            pal->entries[i].green = 0;
            pal->entries[i].blue = 0;
        }
    }
    
    // Initialize fade
    g_fade_active = TRUE;
    g_fade_step = 0;
    g_fade_speed = PALETTE_FADE_STEPS / steps;
}

/**
 * Updates palette fading
 * @return TRUE if fade is complete
 */
BOOL update_palette_fade()
{
    if (!g_fade_active || g_current_palette < 0)
        return TRUE;
    
    g_fade_step += g_fade_speed;
    if (g_fade_step >= PALETTE_FADE_STEPS)
    {
        // Fade complete
        memcpy(&g_palettes[g_current_palette], &g_fade_target, sizeof(Palette));
        g_fade_active = FALSE;
        return TRUE;
    }
    
    // Interpolate colors
    Palette* current = &g_palettes[g_current_palette];
    float t = (float)g_fade_step / PALETTE_FADE_STEPS;
    
    for (int i = 0; i < MAX_PALETTE_ENTRIES; i++)
    {
        if (!(current->entries[i].flags & PAL_FLAG_SYSTEM))
        {
            current->entries[i].red = (uint8_t)(
                current->entries[i].red + 
                (g_fade_target.entries[i].red - current->entries[i].red) * t);
            current->entries[i].green = (uint8_t)(
                current->entries[i].green + 
                (g_fade_target.entries[i].green - current->entries[i].green) * t);
            current->entries[i].blue = (uint8_t)(
                current->entries[i].blue + 
                (g_fade_target.entries[i].blue - current->entries[i].blue) * t);
        }
    }
    
    current->flags |= PAL_FLAG_DIRTY;
    return FALSE;
}

/**
 * Starts a palette rotation animation
 * @param start Start index for rotation
 * @param end End index for rotation
 * @param speed Rotation speed (frames per step)
 * @return Animation slot, or -1 on failure
 */
int start_palette_rotation(int start, int end, int speed)
{
    if (g_num_animations >= 8)
        return -1;
    
    PaletteAnimation* anim = &g_animations[g_num_animations];
    anim->start_index = start;
    anim->end_index = end;
    anim->current_offset = 0;
    anim->speed = speed;
    anim->direction = 1;
    anim->active = TRUE;
    
    return g_num_animations++;
}

/**
 * Updates all active palette animations
 */
void update_palette_animations()
{
    if (g_current_palette < 0)
        return;
    
    Palette* pal = &g_palettes[g_current_palette];
    BOOL dirty = FALSE;
    
    for (int i = 0; i < g_num_animations; i++)
    {
        PaletteAnimation* anim = &g_animations[i];
        if (!anim->active)
            continue;
        
        // Update animation
        static int frame_counter = 0;
        frame_counter++;
        
        if (frame_counter % anim->speed == 0)
        {
            // Rotate colors
            int range = anim->end_index - anim->start_index + 1;
            ColorEntry temp = pal->entries[anim->start_index];
            
            if (anim->direction > 0)
            {
                // Forward rotation
                for (int j = anim->start_index; j < anim->end_index; j++)
                {
                    pal->entries[j] = pal->entries[j + 1];
                }
                pal->entries[anim->end_index] = temp;
            }
            else
            {
                // Backward rotation
                for (int j = anim->end_index; j > anim->start_index; j--)
                {
                    pal->entries[j] = pal->entries[j - 1];
                }
                pal->entries[anim->start_index] = temp;
            }
            
            dirty = TRUE;
        }
    }
    
    if (dirty)
        pal->flags |= PAL_FLAG_DIRTY;
}

// ========================================================================
// PALETTE UTILITIES
// ========================================================================

/**
 * Applies gamma correction to the current palette
 * @param gamma Gamma value (1.0 = no change)
 */
void apply_gamma_correction(float gamma)
{
    if (g_current_palette < 0 || gamma <= 0.0f)
        return;
    
    Palette* pal = &g_palettes[g_current_palette];
    float inv_gamma = 1.0f / gamma;
    
    for (int i = 0; i < MAX_PALETTE_ENTRIES; i++)
    {
        if (!(pal->entries[i].flags & PAL_FLAG_SYSTEM))
        {
            pal->entries[i].red = (uint8_t)(
                pow(pal->entries[i].red / 255.0f, inv_gamma) * 255.0f);
            pal->entries[i].green = (uint8_t)(
                pow(pal->entries[i].green / 255.0f, inv_gamma) * 255.0f);
            pal->entries[i].blue = (uint8_t)(
                pow(pal->entries[i].blue / 255.0f, inv_gamma) * 255.0f);
        }
    }
    
    pal->flags |= PAL_FLAG_DIRTY;
}

/**
 * Applies brightness adjustment to the current palette
 * @param brightness Brightness adjustment (-100 to 100)
 */
void adjust_palette_brightness(int brightness)
{
    if (g_current_palette < 0)
        return;
    
    Palette* pal = &g_palettes[g_current_palette];
    
    for (int i = 0; i < MAX_PALETTE_ENTRIES; i++)
    {
        if (!(pal->entries[i].flags & PAL_FLAG_SYSTEM))
        {
            int r = pal->entries[i].red + brightness;
            int g = pal->entries[i].green + brightness;
            int b = pal->entries[i].blue + brightness;
            
            // Clamp values
            pal->entries[i].red = (uint8_t)(r < 0 ? 0 : (r > 255 ? 255 : r));
            pal->entries[i].green = (uint8_t)(g < 0 ? 0 : (g > 255 ? 255 : g));
            pal->entries[i].blue = (uint8_t)(b < 0 ? 0 : (b > 255 ? 255 : b));
        }
    }
    
    pal->flags |= PAL_FLAG_DIRTY;
}

/**
 * Creates a gradient between two colors
 * @param start_index Starting palette index
 * @param end_index Ending palette index
 * @param start_color Starting RGB color
 * @param end_color Ending RGB color
 */
void create_color_gradient(int start_index, int end_index, 
                          COLORREF start_color, COLORREF end_color)
{
    if (g_current_palette < 0 || start_index >= end_index)
        return;
    
    Palette* pal = &g_palettes[g_current_palette];
    int steps = end_index - start_index;
    
    for (int i = 0; i <= steps; i++)
    {
        float t = (float)i / steps;
        int idx = start_index + i;
        
        pal->entries[idx].red = (uint8_t)(
            GetRValue(start_color) + 
            (GetRValue(end_color) - GetRValue(start_color)) * t);
        pal->entries[idx].green = (uint8_t)(
            GetGValue(start_color) + 
            (GetGValue(end_color) - GetGValue(start_color)) * t);
        pal->entries[idx].blue = (uint8_t)(
            GetBValue(start_color) + 
            (GetBValue(end_color) - GetBValue(start_color)) * t);
    }
    
    pal->flags |= PAL_FLAG_DIRTY;
}
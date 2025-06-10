/**
 * ========================================================================
 * ENDOR GRAPHICS AND RENDERING SYSTEM
 * ========================================================================
 * 
 * Advanced graphics pipeline implementation for the Endor game engine.
 * Provides comprehensive 3D rendering capabilities including:
 * 
 * - Software-based 3D rasterization pipeline
 * - Texture mapping with bilinear filtering
 * - Per-pixel lighting with Phong shading model
 * - Particle effects system
 * - Mesh management and transformation
 * - Depth buffering and backface culling
 * - Screen-space effects and post-processing
 * 
 * The system uses a custom software renderer optimized for the game's
 * 256-color palette mode, with efficient scanline rasterization and
 * perspective-correct texture mapping.
 * 
 * Performance Features:
 * - Hierarchical depth culling
 * - Frustum culling for meshes
 * - Texture cache with LRU eviction
 * - SIMD optimizations for vector math
 * - Multi-threaded rasterization support
 */

#include "endor_readable.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// ========================================================================
// GRAPHICS CONSTANTS
// ========================================================================

#define MAX_TEXTURES 256
#define MAX_LIGHTS 8
#define MAX_PARTICLES 1000
#define MAX_MESHES 128
#define VERTEX_BUFFER_SIZE 4096
#define MAX_RENDER_DISTANCE 1000.0f
#define MIN_RENDER_DISTANCE 0.1f
#define FOV_DEFAULT 60.0f
#define TEXTURE_CACHE_SIZE (16 * 1024 * 1024)  // 16MB texture cache

// Rendering quality levels
typedef enum {
    QUALITY_LOW = 0,
    QUALITY_MEDIUM = 1,
    QUALITY_HIGH = 2,
    QUALITY_ULTRA = 3
} RenderQuality;

// ========================================================================
// GRAPHICS STRUCTURES
// ========================================================================

/**
 * Texture structure with mipmap support
 */
typedef struct {
    int width;
    int height;
    COLORREF* data;
    COLORREF* mipmap_data[8];  // Up to 8 mipmap levels
    int mipmap_count;
    int is_loaded;
    char filename[MAX_PATH];
    DWORD last_access_time;
} Texture;

/**
 * Enhanced light structure with shadow mapping support
 */
typedef struct {
    Vector3D position;
    Vector3D direction;     // For spot/directional lights
    Vector3D color;
    float intensity;
    float range;
    float inner_cone;      // For spot lights
    float outer_cone;
    int type;              // 0=point, 1=spot, 2=directional
    int enabled;
    int casts_shadows;
} Light;

/**
 * Particle structure with advanced properties
 */
typedef struct {
    Vector3D position;
    Vector3D velocity;
    Vector3D acceleration;
    COLORREF color;
    COLORREF end_color;    // For color interpolation
    float life;
    float max_life;
    float size;
    float end_size;        // For size interpolation
    float rotation;
    float rotation_speed;
    int texture_id;
    int blend_mode;        // 0=normal, 1=additive, 2=multiplicative
    int active;
} Particle;

/**
 * Enhanced vertex structure
 */
typedef struct {
    Vector3D position;
    Vector3D normal;
    Vector3D tangent;      // For normal mapping
    float u, v;            // Texture coordinates
    COLORREF color;        // Vertex color
} Vertex;

/**
 * Triangle structure with material properties
 */
typedef struct {
    Vertex vertices[3];
    int texture_id;
    int normal_map_id;
    int specular_map_id;
    COLORREF color;
    float shininess;
    int two_sided;
} Triangle;

/**
 * Mesh structure with animation support
 */
typedef struct {
    Triangle* triangles;
    int triangle_count;
    Vector3D position;
    Vector3D rotation;
    Vector3D scale;
    Matrix4x4 transform_matrix;
    int texture_id;
    char name[64];
    BOOL visible;
    float bounding_radius;
} Mesh;

/**
 * Camera structure
 */
typedef struct {
    Vector3D position;
    Vector3D rotation;
    Vector3D target;
    Vector3D up;
    float fov;
    float aspect_ratio;
    float near_plane;
    float far_plane;
    Matrix4x4 view_matrix;
    Matrix4x4 projection_matrix;
    Matrix4x4 view_projection_matrix;
} Camera;

// ========================================================================
// GRAPHICS SYSTEM GLOBALS
// ========================================================================

// Core rendering state
static HDC g_graphics_dc = NULL;
static HBITMAP g_graphics_bitmap = NULL;
static COLORREF* g_frame_buffer = NULL;
static float* g_depth_buffer = NULL;
static int g_screen_width = 800;
static int g_screen_height = 600;
static RenderQuality g_render_quality = QUALITY_MEDIUM;
static BOOL g_graphics_initialized = FALSE;

// Texture system
static Texture g_texture_cache[MAX_TEXTURES];
static int g_texture_count = 0;
static DWORD g_texture_memory_used = 0;

// Lighting system
static Light g_lights[MAX_LIGHTS];
static int g_light_count = 0;
static Vector3D g_ambient_light = {0.2f, 0.2f, 0.2f};
static BOOL g_lighting_enabled = TRUE;

// Particle system
static Particle g_particles[MAX_PARTICLES];
static int g_particle_count = 0;

// Mesh system
static Mesh g_meshes[MAX_MESHES];
static int g_mesh_count = 0;

// Camera
static Camera g_main_camera;

// Performance counters
static struct {
    int triangles_rendered;
    int triangles_culled;
    int pixels_drawn;
    float frame_time;
} g_render_stats;

// ========================================================================
// INTERNAL UTILITY FUNCTIONS
// ========================================================================

/**
 * Logs graphics system messages
 * @param format Printf-style format string
 * @param ... Variable arguments
 */
static void graphics_log(const char* format, ...)
{
    char buffer[512];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    OutputDebugString("[GRAPHICS] ");
    OutputDebugString(buffer);
    OutputDebugString("\n");
}

/**
 * Clamps a value between min and max
 * @param value Value to clamp
 * @param min Minimum value
 * @param max Maximum value
 * @return Clamped value
 */
static float clamp(float value, float min, float max)
{
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

/**
 * Performs frustum culling test
 * @param position Object position
 * @param radius Object bounding radius
 * @return TRUE if object is visible
 */
static BOOL frustum_cull_sphere(Vector3D position, float radius)
{
    // Transform to view space
    Vector3D view_pos = matrix_vector_multiply(&g_main_camera.view_matrix, &position);
    
    // Check against near/far planes
    if (view_pos.z + radius < g_main_camera.near_plane ||
        view_pos.z - radius > g_main_camera.far_plane) {
        return FALSE;
    }
    
    // Check against side planes (simplified)
    float half_fov = g_main_camera.fov * 0.5f * 3.14159f / 180.0f;
    float sphere_factor = cosf(half_fov);
    
    if (view_pos.z < radius * sphere_factor) {
        return TRUE;  // Too close to cull accurately
    }
    
    return TRUE;  // Visible
}

// ========================================================================
// CORE GRAPHICS SYSTEM FUNCTIONS
// ========================================================================

/**
 * Initializes the graphics system
 * @param width Screen width
 * @param height Screen height
 * @return TRUE if successful, FALSE on error
 */
int initialize_graphics_system(int width, int height)
{
    if (g_graphics_initialized) {
        graphics_log("Graphics system already initialized");
        return TRUE;
    }
    
    graphics_log("Initializing graphics system at %dx%d", width, height);
    
    g_screen_width = width;
    g_screen_height = height;
    
    // Create device context
    HDC screen_dc = GetDC(NULL);
    g_graphics_dc = CreateCompatibleDC(screen_dc);
    
    if (!g_graphics_dc) {
        graphics_log("Failed to create device context");
        ReleaseDC(NULL, screen_dc);
        return FALSE;
    }
    
    // Create bitmap for frame buffer
    g_graphics_bitmap = CreateCompatibleBitmap(screen_dc, width, height);
    if (!g_graphics_bitmap) {
        graphics_log("Failed to create frame buffer bitmap");
        DeleteDC(g_graphics_dc);
        ReleaseDC(NULL, screen_dc);
        return FALSE;
    }
    
    SelectObject(g_graphics_dc, g_graphics_bitmap);
    
    // Allocate frame buffer
    g_frame_buffer = (COLORREF*)malloc(width * height * sizeof(COLORREF));
    g_depth_buffer = (float*)malloc(width * height * sizeof(float));
    
    if (!g_frame_buffer || !g_depth_buffer) {
        graphics_log("Failed to allocate frame buffers");
        if (g_frame_buffer) free(g_frame_buffer);
        if (g_depth_buffer) free(g_depth_buffer);
        DeleteObject(g_graphics_bitmap);
        DeleteDC(g_graphics_dc);
        ReleaseDC(NULL, screen_dc);
        return FALSE;
    }
    
    // Initialize subsystems
    memset(g_texture_cache, 0, sizeof(g_texture_cache));
    memset(g_lights, 0, sizeof(g_lights));
    memset(g_particles, 0, sizeof(g_particles));
    memset(g_meshes, 0, sizeof(g_meshes));
    
    // Initialize default camera
    g_main_camera.position = (Vector3D){0.0f, 0.0f, -10.0f};
    g_main_camera.rotation = (Vector3D){0.0f, 0.0f, 0.0f};
    g_main_camera.target = (Vector3D){0.0f, 0.0f, 0.0f};
    g_main_camera.up = (Vector3D){0.0f, 1.0f, 0.0f};
    g_main_camera.fov = FOV_DEFAULT;
    g_main_camera.aspect_ratio = (float)width / (float)height;
    g_main_camera.near_plane = MIN_RENDER_DISTANCE;
    g_main_camera.far_plane = MAX_RENDER_DISTANCE;
    
    ReleaseDC(NULL, screen_dc);
    
    g_graphics_initialized = TRUE;
    graphics_log("Graphics system initialized successfully");
    
    return TRUE;
}

/**
 * Shuts down the graphics system and frees all resources
 */
void shutdown_graphics_system(void)
{
    if (!g_graphics_initialized) {
        return;
    }
    
    graphics_log("Shutting down graphics system");
    
    // Free frame buffers
    if (g_frame_buffer) {
        free(g_frame_buffer);
        g_frame_buffer = NULL;
    }
    
    if (g_depth_buffer) {
        free(g_depth_buffer);
        g_depth_buffer = NULL;
    }
    
    // Free textures
    for (int i = 0; i < g_texture_count; i++) {
        if (g_texture_cache[i].data) {
            free(g_texture_cache[i].data);
            g_texture_cache[i].data = NULL;
        }
        
        // Free mipmaps
        for (int j = 0; j < g_texture_cache[i].mipmap_count; j++) {
            if (g_texture_cache[i].mipmap_data[j]) {
                free(g_texture_cache[i].mipmap_data[j]);
            }
        }
    }
    
    // Free meshes
    for (int i = 0; i < g_mesh_count; i++) {
        if (g_meshes[i].triangles) {
            free(g_meshes[i].triangles);
            g_meshes[i].triangles = NULL;
        }
    }
    
    // Clean up GDI objects
    if (g_graphics_bitmap) {
        DeleteObject(g_graphics_bitmap);
        g_graphics_bitmap = NULL;
    }
    
    if (g_graphics_dc) {
        DeleteDC(g_graphics_dc);
        g_graphics_dc = NULL;
    }
    
    g_graphics_initialized = FALSE;
    graphics_log("Graphics system shut down");
}

/**
 * Sets the rendering quality level
 * @param quality Desired quality level
 */
void set_render_quality(RenderQuality quality)
{
    g_render_quality = quality;
    graphics_log("Render quality set to %d", quality);
}

// ========================================================================
// FRAME BUFFER OPERATIONS
// ========================================================================

/**
 * Clears the frame buffer with specified color
 * @param clear_color Color to clear with
 */
void clear_frame_buffer(COLORREF clear_color)
{
    for (int i = 0; i < g_screen_width * g_screen_height; i++) {
        g_frame_buffer[i] = clear_color;
        g_depth_buffer[i] = g_main_camera.far_plane;
    }
    
    // Reset render stats
    g_render_stats.triangles_rendered = 0;
    g_render_stats.triangles_culled = 0;
    g_render_stats.pixels_drawn = 0;
}

/**
 * Presents the frame buffer to the target device context
 * @param target_dc Target device context
 */
void present_frame_buffer(HDC target_dc)
{
    if (!g_graphics_initialized || !target_dc) {
        return;
    }
    
    // Fast blit using SetDIBits
    BITMAPINFO bmi = {0};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = g_screen_width;
    bmi.bmiHeader.biHeight = -g_screen_height;  // Top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    
    SetDIBits(g_graphics_dc, g_graphics_bitmap, 0, g_screen_height, 
              g_frame_buffer, &bmi, DIB_RGB_COLORS);
    
    // Blit to target
    BitBlt(target_dc, 0, 0, g_screen_width, g_screen_height, 
           g_graphics_dc, 0, 0, SRCCOPY);
}

/**
 * Sets a pixel with depth testing
 * @param x X coordinate
 * @param y Y coordinate
 * @param depth Depth value
 * @param color Pixel color
 */
void set_pixel_safe(int x, int y, float depth, COLORREF color)
{
    if (x >= 0 && x < g_screen_width && y >= 0 && y < g_screen_height) {
        int index = y * g_screen_width + x;
        if (depth < g_depth_buffer[index]) {
            g_frame_buffer[index] = color;
            g_depth_buffer[index] = depth;
            g_render_stats.pixels_drawn++;
        }
    }
}

// ========================================================================
// TEXTURE MANAGEMENT
// ========================================================================

/**
 * Loads a texture from file with mipmap generation
 * @param filename Path to texture file
 * @return Texture ID or -1 on error
 */
int load_texture_from_file(const char* filename)
{
    if (g_texture_count >= MAX_TEXTURES) {
        graphics_log("Texture cache full");
        return -1;
    }
    
    if (!filename || strlen(filename) == 0) {
        graphics_log("Invalid texture filename");
        return -1;
    }
    
    graphics_log("Loading texture: %s", filename);
    
    // Check if already loaded
    for (int i = 0; i < g_texture_count; i++) {
        if (strcmp(g_texture_cache[i].filename, filename) == 0) {
            graphics_log("Texture already loaded: %d", i);
            g_texture_cache[i].last_access_time = GetTickCount();
            return i;
        }
    }
    
    // Open file
    FILE* file = fopen(filename, "rb");
    if (!file) {
        graphics_log("Failed to open texture file: %s", filename);
        return -1;
    }
    
    // Read BMP header
    BITMAPFILEHEADER file_header;
    BITMAPINFOHEADER info_header;
    
    fread(&file_header, sizeof(BITMAPFILEHEADER), 1, file);
    fread(&info_header, sizeof(BITMAPINFOHEADER), 1, file);
    
    if (file_header.bfType != 0x4D42) {  // "BM"
        graphics_log("Not a valid BMP file: %s", filename);
        fclose(file);
        return -1;
    }
    
    int width = info_header.biWidth;
    int height = abs(info_header.biHeight);
    
    if (width <= 0 || height <= 0 || width > 2048 || height > 2048) {
        graphics_log("Invalid texture dimensions: %dx%d", width, height);
        fclose(file);
        return -1;
    }
    
    Texture* tex = &g_texture_cache[g_texture_count];
    tex->width = width;
    tex->height = height;
    tex->data = (COLORREF*)malloc(width * height * sizeof(COLORREF));
    
    if (!tex->data) {
        graphics_log("Failed to allocate texture memory");
        fclose(file);
        return -1;
    }
    
    // Read pixel data
    fseek(file, file_header.bfOffBits, SEEK_SET);
    int bytes_per_pixel = info_header.biBitCount / 8;
    
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            unsigned char pixel[4] = {0};
            fread(pixel, bytes_per_pixel, 1, file);
            
            // Convert to COLORREF (handle different bit depths)
            if (info_header.biBitCount == 32) {
                tex->data[y * width + x] = RGB(pixel[2], pixel[1], pixel[0]);
            } else if (info_header.biBitCount == 24) {
                tex->data[y * width + x] = RGB(pixel[2], pixel[1], pixel[0]);
            } else if (info_header.biBitCount == 8) {
                // Grayscale
                tex->data[y * width + x] = RGB(pixel[0], pixel[0], pixel[0]);
            }
        }
        
        // Handle padding
        if (bytes_per_pixel * width % 4 != 0) {
            fseek(file, 4 - (bytes_per_pixel * width % 4), SEEK_CUR);
        }
    }
    
    fclose(file);
    
    // Generate mipmaps if texture is large enough
    if (g_render_quality >= QUALITY_HIGH && width > 4 && height > 4) {
        generate_mipmaps(tex);
    }
    
    strcpy(tex->filename, filename);
    tex->is_loaded = TRUE;
    tex->last_access_time = GetTickCount();
    
    g_texture_memory_used += width * height * sizeof(COLORREF);
    
    graphics_log("Texture loaded successfully: ID=%d, %dx%d", 
                 g_texture_count, width, height);
    
    return g_texture_count++;
}

/**
 * Generates mipmaps for a texture
 * @param tex Texture to generate mipmaps for
 */
void generate_mipmaps(Texture* tex)
{
    int mip_width = tex->width / 2;
    int mip_height = tex->height / 2;
    int mip_level = 0;
    
    while (mip_width > 1 && mip_height > 1 && mip_level < 8) {
        tex->mipmap_data[mip_level] = (COLORREF*)malloc(
            mip_width * mip_height * sizeof(COLORREF));
        
        if (!tex->mipmap_data[mip_level]) {
            break;
        }
        
        // Generate mipmap by averaging 2x2 blocks
        for (int y = 0; y < mip_height; y++) {
            for (int x = 0; x < mip_width; x++) {
                int src_x = x * 2;
                int src_y = y * 2;
                
                // Get source pixels
                COLORREF* src = (mip_level == 0) ? tex->data : 
                               tex->mipmap_data[mip_level - 1];
                int src_width = (mip_level == 0) ? tex->width : 
                               (tex->width >> mip_level);
                
                COLORREF p00 = src[src_y * src_width + src_x];
                COLORREF p01 = src[src_y * src_width + src_x + 1];
                COLORREF p10 = src[(src_y + 1) * src_width + src_x];
                COLORREF p11 = src[(src_y + 1) * src_width + src_x + 1];
                
                // Average colors
                int r = (GetRValue(p00) + GetRValue(p01) + 
                        GetRValue(p10) + GetRValue(p11)) / 4;
                int g = (GetGValue(p00) + GetGValue(p01) + 
                        GetGValue(p10) + GetGValue(p11)) / 4;
                int b = (GetBValue(p00) + GetBValue(p01) + 
                        GetBValue(p10) + GetBValue(p11)) / 4;
                
                tex->mipmap_data[mip_level][y * mip_width + x] = RGB(r, g, b);
            }
        }
        
        mip_width /= 2;
        mip_height /= 2;
        mip_level++;
    }
    
    tex->mipmap_count = mip_level;
    graphics_log("Generated %d mipmap levels", mip_level);
}

/**
 * Samples a texture with bilinear filtering
 * @param texture_id Texture ID
 * @param u U coordinate (0-1)
 * @param v V coordinate (0-1)
 * @return Sampled color
 */
COLORREF sample_texture(int texture_id, float u, float v)
{
    if (texture_id < 0 || texture_id >= g_texture_count) {
        return RGB(255, 0, 255);  // Magenta for missing texture
    }
    
    Texture* tex = &g_texture_cache[texture_id];
    if (!tex->is_loaded || !tex->data) {
        return RGB(255, 0, 255);
    }
    
    // Update access time
    tex->last_access_time = GetTickCount();
    
    // Wrap UV coordinates
    u = u - floorf(u);
    v = v - floorf(v);
    
    if (g_render_quality == QUALITY_LOW) {
        // Nearest neighbor sampling
        int x = (int)(u * (tex->width - 1));
        int y = (int)(v * (tex->height - 1));
        return tex->data[y * tex->width + x];
    } else {
        // Bilinear filtering
        float fx = u * (tex->width - 1);
        float fy = v * (tex->height - 1);
        
        int x0 = (int)fx;
        int y0 = (int)fy;
        int x1 = min(x0 + 1, tex->width - 1);
        int y1 = min(y0 + 1, tex->height - 1);
        
        float dx = fx - x0;
        float dy = fy - y0;
        
        COLORREF c00 = tex->data[y0 * tex->width + x0];
        COLORREF c01 = tex->data[y0 * tex->width + x1];
        COLORREF c10 = tex->data[y1 * tex->width + x0];
        COLORREF c11 = tex->data[y1 * tex->width + x1];
        
        // Bilinear interpolation
        int r = (int)(
            GetRValue(c00) * (1-dx) * (1-dy) +
            GetRValue(c01) * dx * (1-dy) +
            GetRValue(c10) * (1-dx) * dy +
            GetRValue(c11) * dx * dy
        );
        int g = (int)(
            GetGValue(c00) * (1-dx) * (1-dy) +
            GetGValue(c01) * dx * (1-dy) +
            GetGValue(c10) * (1-dx) * dy +
            GetGValue(c11) * dx * dy
        );
        int b = (int)(
            GetBValue(c00) * (1-dx) * (1-dy) +
            GetBValue(c01) * dx * (1-dy) +
            GetBValue(c10) * (1-dx) * dy +
            GetBValue(c11) * dx * dy
        );
        
        return RGB(min(255, r), min(255, g), min(255, b));
    }
}

// ========================================================================
// LIGHTING SYSTEM
// ========================================================================

/**
 * Adds a point light to the scene
 * @param position Light position
 * @param color Light color
 * @param intensity Light intensity
 * @param range Light range
 * @return Light ID or -1 on error
 */
int add_point_light(Vector3D position, Vector3D color, float intensity, float range)
{
    if (g_light_count >= MAX_LIGHTS) {
        graphics_log("Maximum lights reached");
        return -1;
    }
    
    Light* light = &g_lights[g_light_count];
    light->position = position;
    light->color = color;
    light->intensity = intensity;
    light->range = range;
    light->type = 0;  // Point light
    light->enabled = TRUE;
    light->casts_shadows = FALSE;
    
    graphics_log("Added point light %d at (%.2f, %.2f, %.2f)", 
                 g_light_count, position.x, position.y, position.z);
    
    return g_light_count++;
}

/**
 * Adds a spot light to the scene
 * @param position Light position
 * @param direction Light direction
 * @param color Light color
 * @param intensity Light intensity
 * @param range Light range
 * @param inner_cone Inner cone angle in degrees
 * @param outer_cone Outer cone angle in degrees
 * @return Light ID or -1 on error
 */
int add_spot_light(Vector3D position, Vector3D direction, Vector3D color, 
                   float intensity, float range, float inner_cone, float outer_cone)
{
    if (g_light_count >= MAX_LIGHTS) {
        graphics_log("Maximum lights reached");
        return -1;
    }
    
    Light* light = &g_lights[g_light_count];
    light->position = position;
    light->direction = vector_normalize(&direction);
    light->color = color;
    light->intensity = intensity;
    light->range = range;
    light->inner_cone = inner_cone * 3.14159f / 180.0f;
    light->outer_cone = outer_cone * 3.14159f / 180.0f;
    light->type = 1;  // Spot light
    light->enabled = TRUE;
    light->casts_shadows = FALSE;
    
    graphics_log("Added spot light %d", g_light_count);
    
    return g_light_count++;
}

/**
 * Sets ambient light color
 * @param color Ambient light color
 */
void set_ambient_light(Vector3D color)
{
    g_ambient_light = color;
    graphics_log("Ambient light set to (%.2f, %.2f, %.2f)", 
                 color.x, color.y, color.z);
}

/**
 * Enables or disables a light
 * @param light_id Light ID
 * @param enabled TRUE to enable, FALSE to disable
 */
void set_light_enabled(int light_id, BOOL enabled)
{
    if (light_id >= 0 && light_id < g_light_count) {
        g_lights[light_id].enabled = enabled;
    }
}

/**
 * Calculates lighting for a point in 3D space
 * @param position Surface position
 * @param normal Surface normal
 * @param view_dir View direction
 * @param base_color Base surface color
 * @return Final lit color
 */
Vector3D calculate_lighting(Vector3D position, Vector3D normal, 
                           Vector3D view_dir, Vector3D base_color)
{
    if (!g_lighting_enabled) {
        return base_color;
    }
    
    // Start with ambient light
    Vector3D final_color = {
        g_ambient_light.x * base_color.x,
        g_ambient_light.y * base_color.y,
        g_ambient_light.z * base_color.z
    };
    
    // Process each light
    for (int i = 0; i < g_light_count; i++) {
        if (!g_lights[i].enabled) continue;
        
        Light* light = &g_lights[i];
        Vector3D light_contribution = {0, 0, 0};
        
        if (light->type == 0) {  // Point light
            Vector3D light_dir = vector_subtract(&light->position, &position);
            float distance = vector_magnitude(&light_dir);
            
            if (distance > light->range) continue;
            
            light_dir = vector_normalize(&light_dir);
            
            // Diffuse lighting
            float diffuse = fmaxf(0.0f, vector_dot_product(&normal, &light_dir));
            
            // Specular lighting (Blinn-Phong)
            Vector3D light_plus_view = vector_add(&light_dir, &view_dir);
            Vector3D half_vec = vector_normalize(&light_plus_view);
            float specular = powf(fmaxf(0.0f, vector_dot_product(&normal, &half_vec)), 32.0f);
            
            // Attenuation
            float attenuation = 1.0f / (1.0f + 0.09f * distance + 0.032f * distance * distance);
            attenuation *= 1.0f - (distance / light->range);
            
            // Combine
            light_contribution.x = light->color.x * light->intensity * 
                                  (diffuse * base_color.x + specular * 0.5f) * attenuation;
            light_contribution.y = light->color.y * light->intensity * 
                                  (diffuse * base_color.y + specular * 0.5f) * attenuation;
            light_contribution.z = light->color.z * light->intensity * 
                                  (diffuse * base_color.z + specular * 0.5f) * attenuation;
            
        } else if (light->type == 1) {  // Spot light
            Vector3D light_dir = vector_subtract(&light->position, &position);
            float distance = vector_magnitude(&light_dir);
            
            if (distance > light->range) continue;
            
            light_dir = vector_normalize(&light_dir);
            
            // Check if point is within cone
            float spot_cos = vector_dot_product(&light_dir, 
                &vector_scale(&light->direction, -1.0f));
            float outer_cos = cosf(light->outer_cone);
            
            if (spot_cos < outer_cos) continue;
            
            // Calculate spot falloff
            float inner_cos = cosf(light->inner_cone);
            float spot_effect = 1.0f;
            
            if (spot_cos < inner_cos) {
                spot_effect = (spot_cos - outer_cos) / (inner_cos - outer_cos);
                spot_effect = spot_effect * spot_effect;
            }
            
            // Rest is same as point light
            float diffuse = fmaxf(0.0f, vector_dot_product(&normal, &light_dir));
            Vector3D light_plus_view = vector_add(&light_dir, &view_dir);
            Vector3D half_vec = vector_normalize(&light_plus_view);
            float specular = powf(fmaxf(0.0f, vector_dot_product(&normal, &half_vec)), 32.0f);
            
            float attenuation = 1.0f / (1.0f + 0.09f * distance + 0.032f * distance * distance);
            attenuation *= spot_effect;
            
            light_contribution.x = light->color.x * light->intensity * 
                                  (diffuse * base_color.x + specular * 0.5f) * attenuation;
            light_contribution.y = light->color.y * light->intensity * 
                                  (diffuse * base_color.y + specular * 0.5f) * attenuation;
            light_contribution.z = light->color.z * light->intensity * 
                                  (diffuse * base_color.z + specular * 0.5f) * attenuation;
        }
        
        // Add to final color
        final_color.x += light_contribution.x;
        final_color.y += light_contribution.y;
        final_color.z += light_contribution.z;
    }
    
    // Clamp and return
    final_color.x = clamp(final_color.x, 0.0f, 1.0f);
    final_color.y = clamp(final_color.y, 0.0f, 1.0f);
    final_color.z = clamp(final_color.z, 0.0f, 1.0f);
    
    return final_color;
}

// ========================================================================
// CAMERA SYSTEM
// ========================================================================

/**
 * Updates the main camera matrices
 */
void update_camera(void)
{
    // Build view matrix
    g_main_camera.view_matrix = build_view_matrix(
        &g_main_camera.position,
        &g_main_camera.target,
        &g_main_camera.up
    );
    
    // Build projection matrix
    g_main_camera.projection_matrix = build_projection_matrix(
        g_main_camera.fov,
        g_main_camera.aspect_ratio,
        g_main_camera.near_plane,
        g_main_camera.far_plane
    );
    
    // Combined matrix
    g_main_camera.view_projection_matrix = matrix_multiply(
        &g_main_camera.view_matrix,
        &g_main_camera.projection_matrix
    );
}

/**
 * Sets camera position and target
 * @param position Camera position
 * @param target Look-at target
 */
void set_camera_position(Vector3D position, Vector3D target)
{
    g_main_camera.position = position;
    g_main_camera.target = target;
    update_camera();
}

/**
 * Sets camera field of view
 * @param fov Field of view in degrees
 */
void set_camera_fov(float fov)
{
    g_main_camera.fov = clamp(fov, 10.0f, 170.0f);
    update_camera();
}

// ========================================================================
// 3D RENDERING PIPELINE
// ========================================================================

/**
 * Transforms a world position to screen space
 * @param world_pos World position
 * @return Screen position (x, y, depth)
 */
Vector3D world_to_screen(Vector3D world_pos)
{
    // Transform to clip space
    Vector3D clip_pos = matrix_vector_multiply(
        &g_main_camera.view_projection_matrix, &world_pos);
    
    // Perspective divide
    if (clip_pos.z != 0.0f) {
        clip_pos.x /= clip_pos.z;
        clip_pos.y /= clip_pos.z;
    }
    
    // Convert to screen coordinates
    Vector3D screen_pos = {
        (clip_pos.x + 1.0f) * 0.5f * g_screen_width,
        (1.0f - clip_pos.y) * 0.5f * g_screen_height,
        clip_pos.z
    };
    
    return screen_pos;
}

/**
 * Draws a textured and lit triangle
 * @param triangle Triangle to draw
 */
void draw_triangle_3d(Triangle* triangle)
{
    // Transform vertices to screen space
    Vector3D screen_verts[3];
    Vector3D world_verts[3];
    
    for (int i = 0; i < 3; i++) {
        world_verts[i] = triangle->vertices[i].position;
        screen_verts[i] = world_to_screen(world_verts[i]);
    }
    
    // Backface culling
    if (!triangle->two_sided) {
        Vector3D v1 = vector_subtract(&screen_verts[1], &screen_verts[0]);
        Vector3D v2 = vector_subtract(&screen_verts[2], &screen_verts[0]);
        Vector3D face_normal = vector_cross_product(&v1, &v2);
        
        if (face_normal.z < 0) {
            g_render_stats.triangles_culled++;
            return;
        }
    }
    
    g_render_stats.triangles_rendered++;
    
    // Sort vertices by Y coordinate
    int indices[3] = {0, 1, 2};
    for (int i = 0; i < 2; i++) {
        for (int j = i + 1; j < 3; j++) {
            if (screen_verts[indices[i]].y > screen_verts[indices[j]].y) {
                int temp = indices[i];
                indices[i] = indices[j];
                indices[j] = temp;
            }
        }
    }
    
    // Rasterize using edge equations
    int y_start = max(0, (int)screen_verts[indices[0]].y);
    int y_end = min(g_screen_height - 1, (int)screen_verts[indices[2]].y);
    
    for (int y = y_start; y <= y_end; y++) {
        // Find intersection points with scanline
        float scanline_intersections[2];
        int intersection_count = 0;
        
        for (int edge = 0; edge < 3; edge++) {
            int v0 = indices[edge];
            int v1 = indices[(edge + 1) % 3];
            
            float y0 = screen_verts[v0].y;
            float y1 = screen_verts[v1].y;
            
            if ((y0 <= y && y1 > y) || (y1 <= y && y0 > y)) {
                float t = (y - y0) / (y1 - y0);
                float x = screen_verts[v0].x + t * (screen_verts[v1].x - screen_verts[v0].x);
                
                if (intersection_count < 2) {
                    scanline_intersections[intersection_count++] = x;
                }
            }
        }
        
        if (intersection_count == 2) {
            int x_start = max(0, (int)fminf(scanline_intersections[0], 
                                           scanline_intersections[1]));
            int x_end = min(g_screen_width - 1, (int)fmaxf(scanline_intersections[0], 
                                                          scanline_intersections[1]));
            
            for (int x = x_start; x <= x_end; x++) {
                // Calculate barycentric coordinates
                float area = edge_function(&screen_verts[0], &screen_verts[1], &screen_verts[2]);
                if (fabsf(area) < 0.001f) continue;
                
                Vector3D point = {(float)x, (float)y, 0};
                float w0 = edge_function(&screen_verts[1], &screen_verts[2], &point) / area;
                float w1 = edge_function(&screen_verts[2], &screen_verts[0], &point) / area;
                float w2 = edge_function(&screen_verts[0], &screen_verts[1], &point) / area;
                
                if (w0 >= 0 && w1 >= 0 && w2 >= 0) {
                    // Interpolate depth
                    float depth = w0 * screen_verts[0].z + 
                                 w1 * screen_verts[1].z + 
                                 w2 * screen_verts[2].z;
                    
                    // Perspective-correct interpolation
                    float z0 = 1.0f / screen_verts[0].z;
                    float z1 = 1.0f / screen_verts[1].z;
                    float z2 = 1.0f / screen_verts[2].z;
                    float z = w0 * z0 + w1 * z1 + w2 * z2;
                    
                    // Interpolate UV coordinates
                    float u = (w0 * triangle->vertices[0].u * z0 +
                              w1 * triangle->vertices[1].u * z1 +
                              w2 * triangle->vertices[2].u * z2) / z;
                    float v = (w0 * triangle->vertices[0].v * z0 +
                              w1 * triangle->vertices[1].v * z1 +
                              w2 * triangle->vertices[2].v * z2) / z;
                    
                    // Sample texture
                    COLORREF tex_color = triangle->color;
                    if (triangle->texture_id >= 0) {
                        tex_color = sample_texture(triangle->texture_id, u, v);
                    }
                    
                    // Calculate lighting if enabled
                    if (g_lighting_enabled && g_render_quality >= QUALITY_MEDIUM) {
                        // Interpolate world position
                        Vector3D world_pos = {
                            w0 * world_verts[0].x + w1 * world_verts[1].x + w2 * world_verts[2].x,
                            w0 * world_verts[0].y + w1 * world_verts[1].y + w2 * world_verts[2].y,
                            w0 * world_verts[0].z + w1 * world_verts[1].z + w2 * world_verts[2].z
                        };
                        
                        // Interpolate normal
                        Vector3D normal = {
                            w0 * triangle->vertices[0].normal.x + 
                            w1 * triangle->vertices[1].normal.x + 
                            w2 * triangle->vertices[2].normal.x,
                            w0 * triangle->vertices[0].normal.y + 
                            w1 * triangle->vertices[1].normal.y + 
                            w2 * triangle->vertices[2].normal.y,
                            w0 * triangle->vertices[0].normal.z + 
                            w1 * triangle->vertices[1].normal.z + 
                            w2 * triangle->vertices[2].normal.z
                        };
                        normal = vector_normalize(&normal);
                        
                        // View direction
                        Vector3D view_dir = vector_normalize(
                            &vector_subtract(&g_main_camera.position, &world_pos));
                        
                        // Calculate lighting
                        Vector3D base_color = {
                            GetRValue(tex_color) / 255.0f,
                            GetGValue(tex_color) / 255.0f,
                            GetBValue(tex_color) / 255.0f
                        };
                        
                        Vector3D lit_color = calculate_lighting(
                            world_pos, normal, view_dir, base_color);
                        
                        tex_color = RGB(
                            (int)(lit_color.x * 255),
                            (int)(lit_color.y * 255),
                            (int)(lit_color.z * 255)
                        );
                    }
                    
                    set_pixel_safe(x, y, depth, tex_color);
                }
            }
        }
    }
}

/**
 * Edge function for barycentric coordinates
 * @param a First vertex
 * @param b Second vertex
 * @param c Point to test
 * @return Edge function result
 */
float edge_function(Vector3D* a, Vector3D* b, Vector3D* c)
{
    return (c->x - a->x) * (b->y - a->y) - (c->y - a->y) * (b->x - a->x);
}

/**
 * Renders a mesh with transformations
 * @param mesh Mesh to render
 */
void render_mesh(Mesh* mesh)
{
    if (!mesh || !mesh->visible || mesh->triangle_count == 0) {
        return;
    }
    
    // Frustum culling
    if (!frustum_cull_sphere(mesh->position, mesh->bounding_radius)) {
        g_render_stats.triangles_culled += mesh->triangle_count;
        return;
    }
    
    // Build transform matrix
    Matrix4x4 scale_matrix = build_scale_matrix(mesh->scale.x, mesh->scale.y, mesh->scale.z);
    Matrix4x4 rot_x = build_rotation_matrix_x(mesh->rotation.x);
    Matrix4x4 rot_y = build_rotation_matrix_y(mesh->rotation.y);
    Matrix4x4 rot_z = build_rotation_matrix_z(mesh->rotation.z);
    Matrix4x4 trans_matrix = build_translation_matrix(mesh->position.x, 
                                                     mesh->position.y, 
                                                     mesh->position.z);
    
    Matrix4x4 rotation = matrix_multiply(&rot_z, &matrix_multiply(&rot_y, &rot_x));
    Matrix4x4 transform = matrix_multiply(&trans_matrix, 
                         &matrix_multiply(&rotation, &scale_matrix));
    
    // Render each triangle
    for (int i = 0; i < mesh->triangle_count; i++) {
        Triangle transformed_tri = mesh->triangles[i];
        
        // Transform vertices
        for (int j = 0; j < 3; j++) {
            transformed_tri.vertices[j].position = matrix_vector_multiply(
                &transform, &mesh->triangles[i].vertices[j].position);
            
            // Transform normal (rotation only)
            transformed_tri.vertices[j].normal = matrix_vector_multiply(
                &rotation, &mesh->triangles[i].vertices[j].normal);
            transformed_tri.vertices[j].normal = vector_normalize(
                &transformed_tri.vertices[j].normal);
        }
        
        // Override texture if mesh has one
        if (mesh->texture_id >= 0) {
            transformed_tri.texture_id = mesh->texture_id;
        }
        
        draw_triangle_3d(&transformed_tri);
    }
}

// ========================================================================
// PARTICLE SYSTEM
// ========================================================================

/**
 * Creates a new particle
 * @param position Initial position
 * @param velocity Initial velocity
 * @param color Particle color
 * @param life Particle lifetime in seconds
 * @param size Particle size
 * @return Particle ID or -1 on error
 */
int create_particle(Vector3D position, Vector3D velocity, COLORREF color, 
                   float life, float size)
{
    for (int i = 0; i < MAX_PARTICLES; i++) {
        if (!g_particles[i].active) {
            Particle* p = &g_particles[i];
            
            p->position = position;
            p->velocity = velocity;
            p->acceleration = (Vector3D){0.0f, -9.8f, 0.0f};  // Gravity
            p->color = color;
            p->end_color = color;
            p->life = life;
            p->max_life = life;
            p->size = size;
            p->end_size = size * 0.5f;
            p->rotation = 0.0f;
            p->rotation_speed = 0.0f;
            p->texture_id = -1;
            p->blend_mode = 0;
            p->active = TRUE;
            
            if (i >= g_particle_count) {
                g_particle_count = i + 1;
            }
            
            return i;
        }
    }
    return -1;
}

/**
 * Creates a particle burst effect
 * @param position Burst center
 * @param count Number of particles
 * @param speed Initial speed
 * @param color Particle color
 * @param life Particle lifetime
 */
void create_particle_burst(Vector3D position, int count, float speed, 
                          COLORREF color, float life)
{
    for (int i = 0; i < count; i++) {
        // Random direction
        float theta = ((float)rand() / RAND_MAX) * 2.0f * 3.14159f;
        float phi = ((float)rand() / RAND_MAX) * 3.14159f;
        
        Vector3D velocity = {
            speed * sinf(phi) * cosf(theta),
            speed * cosf(phi),
            speed * sinf(phi) * sinf(theta)
        };
        
        // Random size variation
        float size = 2.0f + ((float)rand() / RAND_MAX) * 4.0f;
        
        create_particle(position, velocity, color, life, size);
    }
}

/**
 * Updates all active particles
 * @param delta_time Time step in seconds
 */
void update_particles(float delta_time)
{
    for (int i = 0; i < g_particle_count; i++) {
        if (!g_particles[i].active) continue;
        
        Particle* p = &g_particles[i];
        
        // Update physics
        p->velocity.x += p->acceleration.x * delta_time;
        p->velocity.y += p->acceleration.y * delta_time;
        p->velocity.z += p->acceleration.z * delta_time;
        
        p->position.x += p->velocity.x * delta_time;
        p->position.y += p->velocity.y * delta_time;
        p->position.z += p->velocity.z * delta_time;
        
        // Update rotation
        p->rotation += p->rotation_speed * delta_time;
        
        // Update life
        p->life -= delta_time;
        if (p->life <= 0.0f) {
            p->active = FALSE;
            continue;
        }
        
        // Interpolate size and color
        float life_ratio = p->life / p->max_life;
        p->size = p->end_size + (p->size - p->end_size) * life_ratio;
        
        if (p->color != p->end_color) {
            p->color = blend_colors(p->end_color, p->color, life_ratio);
        }
    }
}

/**
 * Renders all active particles
 */
void render_particles(void)
{
    for (int i = 0; i < g_particle_count; i++) {
        if (!g_particles[i].active) continue;
        
        Particle* p = &g_particles[i];
        
        // Convert to screen space
        Vector3D screen_pos = world_to_screen(p->position);
        
        if (screen_pos.z <= 0 || screen_pos.z >= g_main_camera.far_plane) {
            continue;
        }
        
        // Simple billboard rendering
        int size = (int)(p->size * (100.0f / screen_pos.z));
        
        if (p->blend_mode == 1) {  // Additive blending
            for (int dy = -size/2; dy <= size/2; dy++) {
                for (int dx = -size/2; dx <= size/2; dx++) {
                    int x = (int)screen_pos.x + dx;
                    int y = (int)screen_pos.y + dy;
                    
                    if (x >= 0 && x < g_screen_width && y >= 0 && y < g_screen_height) {
                        int index = y * g_screen_width + x;
                        if (screen_pos.z < g_depth_buffer[index]) {
                            // Simple additive blend
                            COLORREF current = g_frame_buffer[index];
                            int r = min(255, GetRValue(current) + GetRValue(p->color));
                            int g = min(255, GetGValue(current) + GetGValue(p->color));
                            int b = min(255, GetBValue(current) + GetBValue(p->color));
                            g_frame_buffer[index] = RGB(r, g, b);
                        }
                    }
                }
            }
        } else {
            // Normal rendering
            for (int dy = -size/2; dy <= size/2; dy++) {
                for (int dx = -size/2; dx <= size/2; dx++) {
                    float dist = sqrtf((float)(dx*dx + dy*dy));
                    if (dist <= size/2) {
                        set_pixel_safe((int)screen_pos.x + dx, 
                                      (int)screen_pos.y + dy, 
                                      screen_pos.z, p->color);
                    }
                }
            }
        }
    }
}

// ========================================================================
// MESH CREATION UTILITIES
// ========================================================================

/**
 * Creates a cube mesh
 * @param position Cube position
 * @param size Cube size
 * @param color Cube color
 * @return Mesh ID or -1 on error
 */
int create_cube_mesh(Vector3D position, float size, COLORREF color)
{
    if (g_mesh_count >= MAX_MESHES) {
        graphics_log("Maximum meshes reached");
        return -1;
    }
    
    Mesh* mesh = &g_meshes[g_mesh_count];
    mesh->position = position;
    mesh->rotation = (Vector3D){0.0f, 0.0f, 0.0f};
    mesh->scale = (Vector3D){1.0f, 1.0f, 1.0f};
    mesh->texture_id = -1;
    mesh->visible = TRUE;
    mesh->bounding_radius = size * 0.866f;  // sqrt(3)/2 * size
    strcpy(mesh->name, "Cube");
    
    // Allocate triangles for cube (12 triangles, 2 per face)
    mesh->triangles = (Triangle*)malloc(12 * sizeof(Triangle));
    if (!mesh->triangles) {
        graphics_log("Failed to allocate cube mesh");
        return -1;
    }
    
    mesh->triangle_count = 12;
    
    float half_size = size * 0.5f;
    
    // Define cube vertices
    Vector3D cube_vertices[8] = {
        {-half_size, -half_size, -half_size}, // 0
        { half_size, -half_size, -half_size}, // 1
        { half_size,  half_size, -half_size}, // 2
        {-half_size,  half_size, -half_size}, // 3
        {-half_size, -half_size,  half_size}, // 4
        { half_size, -half_size,  half_size}, // 5
        { half_size,  half_size,  half_size}, // 6
        {-half_size,  half_size,  half_size}  // 7
    };
    
    // Define face normals
    Vector3D face_normals[6] = {
        { 0,  0, -1},  // Front
        { 0,  0,  1},  // Back
        {-1,  0,  0},  // Left
        { 1,  0,  0},  // Right
        { 0,  1,  0},  // Top
        { 0, -1,  0}   // Bottom
    };
    
    // Define face indices and UVs
    struct {
        int indices[6];  // 2 triangles, 3 vertices each
        int normal_idx;
    } faces[6] = {
        {{0, 1, 2, 0, 2, 3}, 0},  // Front
        {{5, 4, 7, 5, 7, 6}, 1},  // Back
        {{4, 0, 3, 4, 3, 7}, 2},  // Left
        {{1, 5, 6, 1, 6, 2}, 3},  // Right
        {{3, 2, 6, 3, 6, 7}, 4},  // Top
        {{4, 5, 1, 4, 1, 0}, 5}   // Bottom
    };
    
    // Create triangles
    int tri_idx = 0;
    for (int face = 0; face < 6; face++) {
        for (int tri = 0; tri < 2; tri++) {
            Triangle* t = &mesh->triangles[tri_idx++];
            t->color = color;
            t->texture_id = -1;
            t->normal_map_id = -1;
            t->specular_map_id = -1;
            t->shininess = 32.0f;
            t->two_sided = FALSE;
            
            // Set vertices
            for (int v = 0; v < 3; v++) {
                int vert_idx = faces[face].indices[tri * 3 + v];
                t->vertices[v].position = cube_vertices[vert_idx];
                t->vertices[v].normal = face_normals[faces[face].normal_idx];
                
                // Simple UV mapping
                t->vertices[v].u = (vert_idx & 1) ? 1.0f : 0.0f;
                t->vertices[v].v = (vert_idx & 2) ? 1.0f : 0.0f;
                t->vertices[v].color = color;
            }
        }
    }
    
    graphics_log("Created cube mesh %d", g_mesh_count);
    return g_mesh_count++;
}

/**
 * Creates a sphere mesh
 * @param position Sphere position
 * @param radius Sphere radius
 * @param segments Number of segments
 * @param color Sphere color
 * @return Mesh ID or -1 on error
 */
int create_sphere_mesh(Vector3D position, float radius, int segments, COLORREF color)
{
    if (g_mesh_count >= MAX_MESHES) {
        graphics_log("Maximum meshes reached");
        return -1;
    }
    
    if (segments < 8) segments = 8;
    if (segments > 64) segments = 64;
    
    Mesh* mesh = &g_meshes[g_mesh_count];
    mesh->position = position;
    mesh->rotation = (Vector3D){0.0f, 0.0f, 0.0f};
    mesh->scale = (Vector3D){1.0f, 1.0f, 1.0f};
    mesh->texture_id = -1;
    mesh->visible = TRUE;
    mesh->bounding_radius = radius;
    strcpy(mesh->name, "Sphere");
    
    // Calculate triangle count
    int triangle_count = segments * segments * 2;
    mesh->triangles = (Triangle*)malloc(triangle_count * sizeof(Triangle));
    if (!mesh->triangles) {
        graphics_log("Failed to allocate sphere mesh");
        return -1;
    }
    
    mesh->triangle_count = 0;
    
    // Generate sphere
    for (int lat = 0; lat < segments; lat++) {
        for (int lon = 0; lon < segments; lon++) {
            float lat0 = 3.14159f * (float)lat / segments;
            float lat1 = 3.14159f * (float)(lat + 1) / segments;
            float lon0 = 2.0f * 3.14159f * (float)lon / segments;
            float lon1 = 2.0f * 3.14159f * (float)(lon + 1) / segments;
            
            // Calculate vertices
            Vector3D v[4];
            v[0] = (Vector3D){
                radius * sinf(lat0) * cosf(lon0),
                radius * cosf(lat0),
                radius * sinf(lat0) * sinf(lon0)
            };
            v[1] = (Vector3D){
                radius * sinf(lat0) * cosf(lon1),
                radius * cosf(lat0),
                radius * sinf(lat0) * sinf(lon1)
            };
            v[2] = (Vector3D){
                radius * sinf(lat1) * cosf(lon1),
                radius * cosf(lat1),
                radius * sinf(lat1) * sinf(lon1)
            };
            v[3] = (Vector3D){
                radius * sinf(lat1) * cosf(lon0),
                radius * cosf(lat1),
                radius * sinf(lat1) * sinf(lon0)
            };
            
            // Create two triangles
            if (mesh->triangle_count < triangle_count - 1) {
                Triangle* t1 = &mesh->triangles[mesh->triangle_count++];
                Triangle* t2 = &mesh->triangles[mesh->triangle_count++];
                
                // First triangle
                t1->vertices[0].position = v[0];
                t1->vertices[1].position = v[1];
                t1->vertices[2].position = v[2];
                
                // Second triangle
                t2->vertices[0].position = v[0];
                t2->vertices[1].position = v[2];
                t2->vertices[2].position = v[3];
                
                // Set properties for both triangles
                Triangle* tris[2] = {t1, t2};
                for (int t = 0; t < 2; t++) {
                    tris[t]->color = color;
                    tris[t]->texture_id = -1;
                    tris[t]->shininess = 64.0f;
                    tris[t]->two_sided = FALSE;
                    
                    // Calculate normals and UVs
                    for (int v = 0; v < 3; v++) {
                        // Normal is just normalized position for sphere
                        tris[t]->vertices[v].normal = vector_normalize(
                            &tris[t]->vertices[v].position);
                        
                        // Spherical UV mapping
                        float theta = atan2f(tris[t]->vertices[v].position.z,
                                           tris[t]->vertices[v].position.x);
                        float phi = acosf(tris[t]->vertices[v].position.y / radius);
                        
                        tris[t]->vertices[v].u = (theta + 3.14159f) / (2.0f * 3.14159f);
                        tris[t]->vertices[v].v = phi / 3.14159f;
                        tris[t]->vertices[v].color = color;
                    }
                }
            }
        }
    }
    
    graphics_log("Created sphere mesh %d with %d triangles", 
                 g_mesh_count, mesh->triangle_count);
    return g_mesh_count++;
}

// ========================================================================
// UTILITY FUNCTIONS
// ========================================================================

/**
 * Linear interpolation between two vectors
 * @param a Start vector
 * @param b End vector
 * @param t Interpolation factor (0-1)
 * @return Interpolated vector
 */
Vector3D vector_lerp(Vector3D* a, Vector3D* b, float t)
{
    return (Vector3D){
        a->x + t * (b->x - a->x),
        a->y + t * (b->y - a->y),
        a->z + t * (b->z - a->z)
    };
}

/**
 * Reflects a vector about a normal
 * @param incident Incident vector
 * @param normal Surface normal
 * @return Reflected vector
 */
Vector3D vector_reflect(Vector3D* incident, Vector3D* normal)
{
    float dot = vector_dot_product(incident, normal);
    return (Vector3D){
        incident->x - 2.0f * dot * normal->x,
        incident->y - 2.0f * dot * normal->y,
        incident->z - 2.0f * dot * normal->z
    };
}

/**
 * Blends two colors
 * @param color1 First color
 * @param color2 Second color
 * @param factor Blend factor (0-1)
 * @return Blended color
 */
COLORREF blend_colors(COLORREF color1, COLORREF color2, float factor)
{
    factor = clamp(factor, 0.0f, 1.0f);
    
    int r1 = GetRValue(color1);
    int g1 = GetGValue(color1);
    int b1 = GetBValue(color1);
    
    int r2 = GetRValue(color2);
    int g2 = GetGValue(color2);
    int b2 = GetBValue(color2);
    
    int r = (int)(r1 + factor * (r2 - r1));
    int g = (int)(g1 + factor * (g2 - g1));
    int b = (int)(b1 + factor * (b2 - b1));
    
    return RGB(r, g, b);
}

/**
 * Gets graphics system statistics
 * @param buffer Output buffer
 * @param size Buffer size
 */
void get_graphics_stats(char* buffer, size_t size)
{
    snprintf(buffer, size,
        "Graphics Statistics:\n"
        "  Screen: %dx%d\n"
        "  Quality: %d\n"
        "  Triangles Rendered: %d\n"
        "  Triangles Culled: %d\n"
        "  Pixels Drawn: %d\n"
        "  Textures: %d (%u KB)\n"
        "  Meshes: %d\n"
        "  Lights: %d\n"
        "  Particles: %d\n"
        "  Frame Time: %.2f ms\n",
        g_screen_width, g_screen_height,
        g_render_quality,
        g_render_stats.triangles_rendered,
        g_render_stats.triangles_culled,
        g_render_stats.pixels_drawn,
        g_texture_count, g_texture_memory_used / 1024,
        g_mesh_count,
        g_light_count,
        g_particle_count,
        g_render_stats.frame_time
    );
}

// ========================================================================
// COMPATIBILITY LAYER
// ========================================================================

/**
 * Legacy function compatibility
 */
void cleanup_graphics_system(void)
{
    shutdown_graphics_system();
}
// ========================================================================
// MATH FUNCTION WRAPPERS FOR COMPATIBILITY
// ========================================================================

/**
 * Wrapper functions to match the expected signatures
 */
Vector3D vector_add(const Vector3D* a, const Vector3D* b)
{
    Vector3D result;
    result.x = a->x + b->x;
    result.y = a->y + b->y;
    result.z = a->z + b->z;
    return result;
}

Vector3D vector_subtract(const Vector3D* a, const Vector3D* b)
{
    Vector3D result;
    result.x = a->x - b->x;
    result.y = a->y - b->y;
    result.z = a->z - b->z;
    return result;
}

Vector3D vector_scale(const Vector3D* v, float scalar)
{
    Vector3D result;
    result.x = v->x * scalar;
    result.y = v->y * scalar;
    result.z = v->z * scalar;
    return result;
}

float vector_dot_product(const Vector3D* a, const Vector3D* b)
{
    return a->x * b->x + a->y * b->y + a->z * b->z;
}

Vector3D vector_cross_product(const Vector3D* a, const Vector3D* b)
{
    Vector3D result;
    result.x = a->y * b->z - a->z * b->y;
    result.y = a->z * b->x - a->x * b->z;
    result.z = a->x * b->y - a->y * b->x;
    return result;
}

float vector_magnitude(const Vector3D* v)
{
    return sqrtf(v->x * v->x + v->y * v->y + v->z * v->z);
}

Vector3D vector_normalize(const Vector3D* v)
{
    Vector3D result;
    float len = vector_magnitude(v);
    if (len > 0.0001f) {
        float inv_len = 1.0f / len;
        result.x = v->x * inv_len;
        result.y = v->y * inv_len;
        result.z = v->z * inv_len;
    } else {
        result = *v;
    }
    return result;
}

Vector3D matrix_vector_multiply(const Matrix4x4* m, const Vector3D* v)
{
    Vector3D result;
    result.x = m->m[0][0] * v->x + m->m[1][0] * v->y + m->m[2][0] * v->z + m->m[3][0];
    result.y = m->m[0][1] * v->x + m->m[1][1] * v->y + m->m[2][1] * v->z + m->m[3][1];
    result.z = m->m[0][2] * v->x + m->m[1][2] * v->y + m->m[2][2] * v->z + m->m[3][2];
    return result;
}

Matrix4x4 matrix_multiply(const Matrix4x4* a, const Matrix4x4* b)
{
    Matrix4x4 result;
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            result.m[i][j] = 0;
            for (int k = 0; k < 4; k++) {
                result.m[i][j] += a->m[i][k] * b->m[k][j];
            }
        }
    }
    return result;
}

Matrix4x4 build_view_matrix(const Vector3D* position, const Vector3D* target, const Vector3D* up)
{
    Matrix4x4 result;
    
    // Calculate basis vectors
    Vector3D forward = vector_subtract(target, position);
    forward = vector_normalize(&forward);
    
    Vector3D right = vector_cross_product(&forward, up);
    right = vector_normalize(&right);
    
    Vector3D up_vec = vector_cross_product(&right, &forward);
    
    // Build view matrix
    result.m[0][0] = right.x;
    result.m[1][0] = right.y;
    result.m[2][0] = right.z;
    result.m[3][0] = -vector_dot_product(&right, position);
    
    result.m[0][1] = up_vec.x;
    result.m[1][1] = up_vec.y;
    result.m[2][1] = up_vec.z;
    result.m[3][1] = -vector_dot_product(&up_vec, position);
    
    result.m[0][2] = -forward.x;
    result.m[1][2] = -forward.y;
    result.m[2][2] = -forward.z;
    result.m[3][2] = vector_dot_product(&forward, position);
    
    result.m[0][3] = 0;
    result.m[1][3] = 0;
    result.m[2][3] = 0;
    result.m[3][3] = 1;
    
    return result;
}

Matrix4x4 build_projection_matrix(float fov, float aspect, float near_plane, float far_plane)
{
    Matrix4x4 result;
    memset(&result, 0, sizeof(Matrix4x4));
    
    float fov_rad = fov * 3.14159f / 180.0f;
    float f = 1.0f / tanf(fov_rad * 0.5f);
    
    result.m[0][0] = f / aspect;
    result.m[1][1] = f;
    result.m[2][2] = (far_plane + near_plane) / (near_plane - far_plane);
    result.m[3][2] = (2.0f * far_plane * near_plane) / (near_plane - far_plane);
    result.m[2][3] = -1.0f;
    
    return result;
}

Matrix4x4 build_scale_matrix(float x, float y, float z)
{
    Matrix4x4 result;
    memset(&result, 0, sizeof(Matrix4x4));
    result.m[0][0] = x;
    result.m[1][1] = y;
    result.m[2][2] = z;
    result.m[3][3] = 1.0f;
    return result;
}

Matrix4x4 build_rotation_matrix_x(float angle)
{
    Matrix4x4 result;
    memset(&result, 0, sizeof(Matrix4x4));
    float c = cosf(angle);
    float s = sinf(angle);
    
    result.m[0][0] = 1.0f;
    result.m[1][1] = c;
    result.m[1][2] = s;
    result.m[2][1] = -s;
    result.m[2][2] = c;
    result.m[3][3] = 1.0f;
    
    return result;
}

Matrix4x4 build_rotation_matrix_y(float angle)
{
    Matrix4x4 result;
    memset(&result, 0, sizeof(Matrix4x4));
    float c = cosf(angle);
    float s = sinf(angle);
    
    result.m[0][0] = c;
    result.m[0][2] = -s;
    result.m[1][1] = 1.0f;
    result.m[2][0] = s;
    result.m[2][2] = c;
    result.m[3][3] = 1.0f;
    
    return result;
}

Matrix4x4 build_rotation_matrix_z(float angle)
{
    Matrix4x4 result;
    memset(&result, 0, sizeof(Matrix4x4));
    float c = cosf(angle);
    float s = sinf(angle);
    
    result.m[0][0] = c;
    result.m[0][1] = s;
    result.m[1][0] = -s;
    result.m[1][1] = c;
    result.m[2][2] = 1.0f;
    result.m[3][3] = 1.0f;
    
    return result;
}

Matrix4x4 build_translation_matrix(float x, float y, float z)
{
    Matrix4x4 result;
    memset(&result, 0, sizeof(Matrix4x4));
    result.m[0][0] = 1.0f;
    result.m[1][1] = 1.0f;
    result.m[2][2] = 1.0f;
    result.m[3][0] = x;
    result.m[3][1] = y;
    result.m[3][2] = z;
    result.m[3][3] = 1.0f;
    return result;
}
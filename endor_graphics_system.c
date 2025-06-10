/**
 * ========================================================================
 * ENDOR GRAPHICS AND RENDERING SYSTEM
 * ========================================================================
 * 
 * Advanced graphics pipeline implementation for the Endor game engine.
 * Includes 3D rendering, texture management, lighting, and visual effects.
 */

#include "endor_readable.h"
#include <math.h>

// ========================================================================
// GRAPHICS CONSTANTS AND GLOBALS
// ========================================================================

#define MAX_TEXTURES 256
#define MAX_LIGHTS 8
#define MAX_PARTICLES 1000
#define MAX_MESHES 128
#define VERTEX_BUFFER_SIZE 4096

// Graphics state
static HDC graphics_dc = NULL;
static HBITMAP graphics_bitmap = NULL;
static COLORREF* frame_buffer = NULL;
static float* depth_buffer = NULL;
static int screen_width = 800;
static int screen_height = 600;

// Texture system
typedef struct {
    int width;
    int height;
    COLORREF* data;
    int is_loaded;
} Texture;

static Texture texture_cache[MAX_TEXTURES];
static int texture_count = 0;

// Lighting system
typedef struct {
    Vector3D position;
    Vector3D color;
    float intensity;
    float range;
    int enabled;
} Light;

static Light lights[MAX_LIGHTS];
static int light_count = 0;
static Vector3D ambient_light = {0.2f, 0.2f, 0.2f};

// Particle system
typedef struct {
    Vector3D position;
    Vector3D velocity;
    Vector3D acceleration;
    COLORREF color;
    float life;
    float size;
    int active;
} Particle;

static Particle particles[MAX_PARTICLES];
static int particle_count = 0;

// Mesh system
typedef struct {
    Vector3D position;
    Vector3D normal;
    float u, v; // Texture coordinates
} Vertex;

typedef struct {
    Vertex vertices[3];
    int texture_id;
    COLORREF color;
} Triangle;

typedef struct {
    Triangle* triangles;
    int triangle_count;
    Vector3D position;
    Vector3D rotation;
    Vector3D scale;
    int texture_id;
} Mesh;

static Mesh meshes[MAX_MESHES];
static int mesh_count = 0;

// ========================================================================
// CORE GRAPHICS FUNCTIONS
// ========================================================================

int initialize_graphics_system(int width, int height) {
    screen_width = width;
    screen_height = height;
    
    // Create device context
    HDC screen_dc = GetDC(NULL);
    graphics_dc = CreateCompatibleDC(screen_dc);
    
    // Create bitmap for frame buffer
    graphics_bitmap = CreateCompatibleBitmap(screen_dc, width, height);
    SelectObject(graphics_dc, graphics_bitmap);
    
    // Allocate frame buffer
    frame_buffer = (COLORREF*)malloc(width * height * sizeof(COLORREF));
    depth_buffer = (float*)malloc(width * height * sizeof(float));
    
    if (!frame_buffer || !depth_buffer) {
        return 0;
    }
    
    // Initialize texture cache
    for (int i = 0; i < MAX_TEXTURES; i++) {
        texture_cache[i].is_loaded = 0;
        texture_cache[i].data = NULL;
    }
    
    // Initialize lighting
    for (int i = 0; i < MAX_LIGHTS; i++) {
        lights[i].enabled = 0;
    }
    
    // Initialize particles
    for (int i = 0; i < MAX_PARTICLES; i++) {
        particles[i].active = 0;
    }
    
    ReleaseDC(NULL, screen_dc);
    return 1;
}

void clear_frame_buffer(COLORREF clear_color) {
    for (int i = 0; i < screen_width * screen_height; i++) {
        frame_buffer[i] = clear_color;
        depth_buffer[i] = 1000000.0f; // Far plane
    }
}

void present_frame_buffer(HDC target_dc) {
    // Copy frame buffer to bitmap
    for (int y = 0; y < screen_height; y++) {
        for (int x = 0; x < screen_width; x++) {
            SetPixel(graphics_dc, x, y, frame_buffer[y * screen_width + x]);
        }
    }
    
    // Blit to target
    BitBlt(target_dc, 0, 0, screen_width, screen_height, graphics_dc, 0, 0, SRCCOPY);
}

void set_pixel_safe(int x, int y, float depth, COLORREF color) {
    if (x >= 0 && x < screen_width && y >= 0 && y < screen_height) {
        int index = y * screen_width + x;
        if (depth < depth_buffer[index]) {
            frame_buffer[index] = color;
            depth_buffer[index] = depth;
        }
    }
}

// ========================================================================
// TEXTURE MANAGEMENT
// ========================================================================

int load_texture_from_file(const char* filename) {
    if (texture_count >= MAX_TEXTURES) {
        return -1;
    }
    
    // Simple bitmap loader (placeholder implementation)
    FILE* file = fopen(filename, "rb");
    if (!file) {
        return -1;
    }
    
    // Read basic bitmap header (simplified)
    fseek(file, 18, SEEK_SET);
    int width, height;
    fread(&width, sizeof(int), 1, file);
    fread(&height, sizeof(int), 1, file);
    
    if (width <= 0 || height <= 0 || width > 1024 || height > 1024) {
        fclose(file);
        return -1;
    }
    
    Texture* tex = &texture_cache[texture_count];
    tex->width = width;
    tex->height = height;
    tex->data = (COLORREF*)malloc(width * height * sizeof(COLORREF));
    
    if (!tex->data) {
        fclose(file);
        return -1;
    }
    
    // Read pixel data (simplified RGB conversion)
    fseek(file, 54, SEEK_SET); // Skip headers
    for (int i = 0; i < width * height; i++) {
        unsigned char rgb[3];
        fread(rgb, 3, 1, file);
        tex->data[i] = RGB(rgb[2], rgb[1], rgb[0]); // BGR to RGB
    }
    
    fclose(file);
    tex->is_loaded = 1;
    
    return texture_count++;
}

COLORREF sample_texture(int texture_id, float u, float v) {
    if (texture_id < 0 || texture_id >= texture_count) {
        return RGB(255, 0, 255); // Magenta for missing texture
    }
    
    Texture* tex = &texture_cache[texture_id];
    if (!tex->is_loaded) {
        return RGB(255, 0, 255);
    }
    
    // Clamp UV coordinates
    u = fmax(0.0f, fmin(1.0f, u));
    v = fmax(0.0f, fmin(1.0f, v));
    
    int x = (int)(u * (tex->width - 1));
    int y = (int)(v * (tex->height - 1));
    
    return tex->data[y * tex->width + x];
}

// ========================================================================
// LIGHTING CALCULATIONS
// ========================================================================

int add_light(Vector3D position, Vector3D color, float intensity, float range) {
    if (light_count >= MAX_LIGHTS) {
        return -1;
    }
    
    Light* light = &lights[light_count];
    light->position = position;
    light->color = color;
    light->intensity = intensity;
    light->range = range;
    light->enabled = 1;
    
    return light_count++;
}

Vector3D calculate_lighting(Vector3D position, Vector3D normal, Vector3D view_dir) {
    Vector3D final_color = ambient_light;
    
    for (int i = 0; i < light_count; i++) {
        if (!lights[i].enabled) continue;
        
        Light* light = &lights[i];
        Vector3D light_dir = vector_subtract(&light->position, &position);
        float distance = vector_magnitude(&light_dir);
        
        if (distance > light->range) continue;
        
        light_dir = vector_normalize(&light_dir);
        
        // Diffuse lighting
        float diffuse = fmax(0.0f, vector_dot_product(&normal, &light_dir));
        
        // Specular lighting (Phong model)
        Vector3D reflect_dir = vector_reflect(&light_dir, &normal);
        float specular = pow(fmax(0.0f, vector_dot_product(&view_dir, &reflect_dir)), 32.0f);
        
        // Attenuation
        float attenuation = 1.0f / (1.0f + 0.09f * distance + 0.032f * distance * distance);
        
        // Combine lighting components
        Vector3D light_contribution = {
            light->color.x * light->intensity * (diffuse + specular * 0.5f) * attenuation,
            light->color.y * light->intensity * (diffuse + specular * 0.5f) * attenuation,
            light->color.z * light->intensity * (diffuse + specular * 0.5f) * attenuation
        };
        
        final_color.x += light_contribution.x;
        final_color.y += light_contribution.y;
        final_color.z += light_contribution.z;
    }
    
    // Clamp values
    final_color.x = fmin(1.0f, final_color.x);
    final_color.y = fmin(1.0f, final_color.y);
    final_color.z = fmin(1.0f, final_color.z);
    
    return final_color;
}

// ========================================================================
// 3D RENDERING PIPELINE
// ========================================================================

Vector3D world_to_screen(Vector3D world_pos, Matrix4x4* view_matrix, Matrix4x4* proj_matrix) {
    // Transform to view space
    Vector3D view_pos = matrix_vector_multiply(view_matrix, &world_pos);
    
    // Transform to clip space
    Vector3D clip_pos = matrix_vector_multiply(proj_matrix, &view_pos);
    
    // Perspective divide
    if (clip_pos.z != 0.0f) {
        clip_pos.x /= clip_pos.z;
        clip_pos.y /= clip_pos.z;
    }
    
    // Convert to screen coordinates
    Vector3D screen_pos = {
        (clip_pos.x + 1.0f) * 0.5f * screen_width,
        (1.0f - clip_pos.y) * 0.5f * screen_height,
        clip_pos.z
    };
    
    return screen_pos;
}

void draw_triangle_3d(Triangle* triangle, Matrix4x4* view_matrix, Matrix4x4* proj_matrix, Vector3D camera_pos) {
    // Transform vertices to screen space
    Vector3D screen_verts[3];
    for (int i = 0; i < 3; i++) {
        screen_verts[i] = world_to_screen(triangle->vertices[i].position, view_matrix, proj_matrix);
    }
    
    // Backface culling
    Vector3D v1 = vector_subtract(&screen_verts[1], &screen_verts[0]);
    Vector3D v2 = vector_subtract(&screen_verts[2], &screen_verts[0]);
    Vector3D face_normal = vector_cross_product(&v1, &v2);
    
    if (face_normal.z < 0) return; // Back-facing
    
    // Rasterization using scanline algorithm
    // Sort vertices by Y coordinate
    int indices[3] = {0, 1, 2};
    if (screen_verts[indices[0]].y > screen_verts[indices[1]].y) {
        int temp = indices[0]; indices[0] = indices[1]; indices[1] = temp;
    }
    if (screen_verts[indices[1]].y > screen_verts[indices[2]].y) {
        int temp = indices[1]; indices[1] = indices[2]; indices[2] = temp;
    }
    if (screen_verts[indices[0]].y > screen_verts[indices[1]].y) {
        int temp = indices[0]; indices[0] = indices[1]; indices[1] = temp;
    }
    
    // Rasterize triangle
    int y_start = (int)screen_verts[indices[0]].y;
    int y_end = (int)screen_verts[indices[2]].y;
    
    for (int y = y_start; y <= y_end; y++) {
        if (y < 0 || y >= screen_height) continue;
        
        float t = (float)(y - y_start) / (float)(y_end - y_start);
        
        // Find left and right edges
        Vector3D left_edge, right_edge;
        
        if (y <= screen_verts[indices[1]].y) {
            // Top half
            float t1 = (float)(y - screen_verts[indices[0]].y) / 
                      (screen_verts[indices[1]].y - screen_verts[indices[0]].y);
            left_edge = vector_lerp(&screen_verts[indices[0]], &screen_verts[indices[1]], t1);
        } else {
            // Bottom half
            float t1 = (float)(y - screen_verts[indices[1]].y) / 
                      (screen_verts[indices[2]].y - screen_verts[indices[1]].y);
            left_edge = vector_lerp(&screen_verts[indices[1]], &screen_verts[indices[2]], t1);
        }
        
        right_edge = vector_lerp(&screen_verts[indices[0]], &screen_verts[indices[2]], t);
        
        if (left_edge.x > right_edge.x) {
            Vector3D temp = left_edge;
            left_edge = right_edge;
            right_edge = temp;
        }
        
        // Rasterize scanline
        int x_start = (int)left_edge.x;
        int x_end = (int)right_edge.x;
        
        for (int x = x_start; x <= x_end; x++) {
            if (x < 0 || x >= screen_width) continue;
            
            // Interpolate depth
            float depth_t = (float)(x - x_start) / (float)(x_end - x_start);
            float depth = left_edge.z + depth_t * (right_edge.z - left_edge.z);
            
            // Simple flat shading for now
            COLORREF pixel_color = triangle->color;
            
            set_pixel_safe(x, y, depth, pixel_color);
        }
    }
}

void render_mesh(Mesh* mesh, Matrix4x4* view_matrix, Matrix4x4* proj_matrix, Vector3D camera_pos) {
    for (int i = 0; i < mesh->triangle_count; i++) {
        Triangle* triangle = &mesh->triangles[i];
        
        // Transform triangle vertices by mesh transform
        Triangle transformed_triangle = *triangle;
        
        for (int j = 0; j < 3; j++) {
            Vector3D* vertex = &transformed_triangle.vertices[j].position;
            
            // Apply scale
            vertex->x *= mesh->scale.x;
            vertex->y *= mesh->scale.y;
            vertex->z *= mesh->scale.z;
            
            // Apply rotation (simplified - just Y rotation for now)
            float cos_y = cos(mesh->rotation.y);
            float sin_y = sin(mesh->rotation.y);
            float old_x = vertex->x;
            vertex->x = old_x * cos_y - vertex->z * sin_y;
            vertex->z = old_x * sin_y + vertex->z * cos_y;
            
            // Apply translation
            vertex->x += mesh->position.x;
            vertex->y += mesh->position.y;
            vertex->z += mesh->position.z;
        }
        
        draw_triangle_3d(&transformed_triangle, view_matrix, proj_matrix, camera_pos);
    }
}

// ========================================================================
// PARTICLE SYSTEM
// ========================================================================

int create_particle(Vector3D position, Vector3D velocity, COLORREF color, float life, float size) {
    for (int i = 0; i < MAX_PARTICLES; i++) {
        if (!particles[i].active) {
            particles[i].position = position;
            particles[i].velocity = velocity;
            particles[i].acceleration = (Vector3D){0.0f, -9.8f, 0.0f}; // Gravity
            particles[i].color = color;
            particles[i].life = life;
            particles[i].size = size;
            particles[i].active = 1;
            
            if (i >= particle_count) {
                particle_count = i + 1;
            }
            
            return i;
        }
    }
    return -1;
}

void update_particles(float delta_time) {
    for (int i = 0; i < particle_count; i++) {
        if (!particles[i].active) continue;
        
        Particle* p = &particles[i];
        
        // Update physics
        p->velocity.x += p->acceleration.x * delta_time;
        p->velocity.y += p->acceleration.y * delta_time;
        p->velocity.z += p->acceleration.z * delta_time;
        
        p->position.x += p->velocity.x * delta_time;
        p->position.y += p->velocity.y * delta_time;
        p->position.z += p->velocity.z * delta_time;
        
        // Update life
        p->life -= delta_time;
        if (p->life <= 0.0f) {
            p->active = 0;
        }
    }
}

void render_particles(Matrix4x4* view_matrix, Matrix4x4* proj_matrix) {
    for (int i = 0; i < particle_count; i++) {
        if (!particles[i].active) continue;
        
        Particle* p = &particles[i];
        
        // Convert to screen space
        Vector3D screen_pos = world_to_screen(p->position, view_matrix, proj_matrix);
        
        // Render as small rectangle
        int size = (int)p->size;
        for (int dy = -size/2; dy <= size/2; dy++) {
            for (int dx = -size/2; dx <= size/2; dx++) {
                set_pixel_safe((int)screen_pos.x + dx, (int)screen_pos.y + dy, 
                              screen_pos.z, p->color);
            }
        }
    }
}

// ========================================================================
// MESH CREATION UTILITIES
// ========================================================================

int create_cube_mesh(Vector3D position, float size, COLORREF color) {
    if (mesh_count >= MAX_MESHES) return -1;
    
    Mesh* mesh = &meshes[mesh_count];
    mesh->position = position;
    mesh->rotation = (Vector3D){0.0f, 0.0f, 0.0f};
    mesh->scale = (Vector3D){1.0f, 1.0f, 1.0f};
    mesh->texture_id = -1;
    
    // Allocate triangles for cube (12 triangles, 2 per face)
    mesh->triangles = (Triangle*)malloc(12 * sizeof(Triangle));
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
    
    // Define face indices (counter-clockwise)
    int face_indices[12][3] = {
        // Front face
        {0, 1, 2}, {0, 2, 3},
        // Back face  
        {5, 4, 7}, {5, 7, 6},
        // Left face
        {4, 0, 3}, {4, 3, 7},
        // Right face
        {1, 5, 6}, {1, 6, 2},
        // Top face
        {3, 2, 6}, {3, 6, 7},
        // Bottom face
        {4, 5, 1}, {4, 1, 0}
    };
    
    // Create triangles
    for (int i = 0; i < 12; i++) {
        Triangle* tri = &mesh->triangles[i];
        tri->color = color;
        tri->texture_id = -1;
        
        for (int j = 0; j < 3; j++) {
            tri->vertices[j].position = cube_vertices[face_indices[i][j]];
            tri->vertices[j].normal = (Vector3D){0.0f, 0.0f, 1.0f}; // Simplified
            tri->vertices[j].u = (float)(j % 2);
            tri->vertices[j].v = (float)(j / 2);
        }
    }
    
    return mesh_count++;
}

// ========================================================================
// GRAPHICS UTILITY FUNCTIONS
// ========================================================================

void cleanup_graphics_system() {
    // Free frame buffers
    if (frame_buffer) {
        free(frame_buffer);
        frame_buffer = NULL;
    }
    
    if (depth_buffer) {
        free(depth_buffer);
        depth_buffer = NULL;
    }
    
    // Free textures
    for (int i = 0; i < texture_count; i++) {
        if (texture_cache[i].data) {
            free(texture_cache[i].data);
        }
    }
    
    // Free meshes
    for (int i = 0; i < mesh_count; i++) {
        if (meshes[i].triangles) {
            free(meshes[i].triangles);
        }
    }
    
    // Clean up GDI objects
    if (graphics_bitmap) {
        DeleteObject(graphics_bitmap);
    }
    
    if (graphics_dc) {
        DeleteDC(graphics_dc);
    }
}

Vector3D vector_lerp(Vector3D* a, Vector3D* b, float t) {
    return (Vector3D){
        a->x + t * (b->x - a->x),
        a->y + t * (b->y - a->y),
        a->z + t * (b->z - a->z)
    };
}

Vector3D vector_reflect(Vector3D* incident, Vector3D* normal) {
    float dot = vector_dot_product(incident, normal);
    return (Vector3D){
        incident->x - 2.0f * dot * normal->x,
        incident->y - 2.0f * dot * normal->y,
        incident->z - 2.0f * dot * normal->z
    };
}

COLORREF blend_colors(COLORREF color1, COLORREF color2, float factor) {
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
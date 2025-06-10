/**
 * ========================================================================
 * ENDOR LEVEL EDITOR AND ASSET MANAGEMENT SYSTEM
 * ========================================================================
 * 
 * Advanced level editor with real-time editing capabilities, asset browser,
 * terrain editing, entity placement, lighting setup, and export functionality.
 * Includes comprehensive asset management with texture loading, model import,
 * and resource optimization.
 * 
 * Features:
 * - Multi-selection with box/lasso selection
 * - Gizmo-based transformation tools
 * - Advanced terrain editing with erosion simulation
 * - Prefab system for reusable object groups
 * - Layer-based organization
 * - Material editor with PBR support
 * - Lightmap baking
 * - Asset hot-reloading
 * - Performance LOD system
 * - Collaborative editing support
 * 
 * Improvements in this version:
 * - Enhanced error handling and logging
 * - Better asset management with dependencies
 * - More terrain editing tools (erosion, noise, etc.)
 * - Advanced selection tools
 * - Prefab and instance system
 * - Layer management
 * - Better undo/redo with serialization
 * - Asset preview generation
 * - Performance optimizations
 * - Plugin system support
 */

#include "endor_readable.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>  // For file stats
#include <float.h>     // For FLT_MAX
#include <stdarg.h>    // For va_list

// Define M_PI if not defined
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#include <direct.h>  // For directory operations on Windows

// ========================================================================
// EDITOR CONSTANTS AND STRUCTURES
// ========================================================================

#define MAX_EDITOR_OBJECTS 4096
#define MAX_TERRAIN_SIZE 256
#define MAX_ASSET_CACHE 2048
#define MAX_UNDO_STACK 500
#define MAX_SELECTION 256
#define MAX_LAYERS 32
#define MAX_PREFABS 256
#define GRID_SIZE 1.0f
#define SNAP_THRESHOLD 0.5f
#define TERRAIN_CHUNK_SIZE 32
#define MAX_LOD_LEVELS 4
#define AUTOSAVE_INTERVAL 300000  // 5 minutes in milliseconds

// Editor modes
typedef enum {
    EDITOR_MODE_SELECT,
    EDITOR_MODE_MOVE,
    EDITOR_MODE_ROTATE,
    EDITOR_MODE_SCALE,
    EDITOR_MODE_TERRAIN,
    EDITOR_MODE_PAINT,
    EDITOR_MODE_LIGHTING,
    EDITOR_MODE_CAMERA,
    EDITOR_MODE_PREFAB,
    EDITOR_MODE_VERTEX,
    EDITOR_MODE_SPLINE
} EditorMode;

// Asset types
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

// Editor object types
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

// Terrain brush types
typedef enum {
    TERRAIN_BRUSH_RAISE,
    TERRAIN_BRUSH_LOWER,
    TERRAIN_BRUSH_SMOOTH,
    TERRAIN_BRUSH_FLATTEN,
    TERRAIN_BRUSH_NOISE,
    TERRAIN_BRUSH_EROSION,
    TERRAIN_BRUSH_RAMP,
    TERRAIN_BRUSH_TEXTURE
} TerrainBrushType;

// Selection modes
typedef enum {
    SELECTION_MODE_SINGLE,
    SELECTION_MODE_BOX,
    SELECTION_MODE_LASSO,
    SELECTION_MODE_PAINT,
    SELECTION_MODE_SIMILAR
} SelectionMode;

// Asset dependency
typedef struct AssetDependency {
    int asset_id;
    struct AssetDependency* next;
} AssetDependency;

// Asset structure
typedef struct {
    char filename[512];
    char display_name[128];
    char tags[256];
    AssetType type;
    int loaded;
    void* data;
    size_t data_size;
    int reference_count;
    time_t last_modified;
    time_t last_checked;
    char thumbnail_path[512];
    unsigned char* thumbnail_data;
    int thumbnail_size;
    AssetDependency* dependencies;
    int import_settings;
    float import_scale;
    int mipmap_count;
    int compression_type;
    char guid[64];  // Unique identifier
} Asset;

// Material properties
typedef struct {
    int diffuse_texture_id;
    int normal_texture_id;
    int specular_texture_id;
    int emissive_texture_id;
    float diffuse_color[4];
    float specular_color[4];
    float emissive_color[4];
    float roughness;
    float metallic;
    float opacity;
    float normal_strength;
    int blend_mode;
    int two_sided;
    int cast_shadows;
    char shader_name[128];
} Material;

// Editor layer
typedef struct {
    char name[64];
    int visible;
    int locked;
    int selectable;
    float color[4];
    int object_count;
    int parent_layer;
} EditorLayer;

// Prefab structure
typedef struct {
    char name[128];
    char description[256];
    int object_ids[MAX_SELECTION];
    int object_count;
    float bounds_min[3];
    float bounds_max[3];
    char thumbnail_path[512];
    time_t creation_date;
    char author[64];
    char tags[256];
} Prefab;

// Editor object structure
typedef struct {
    int id;
    EditorObjectType type;
    float position[3];
    float rotation[3];
    float scale[3];
    float pivot[3];
    char name[128];
    int asset_id;
    int material_id;
    int layer_id;
    int parent_id;
    int selected;
    int visible;
    int locked;
    int cast_shadows;
    int receive_shadows;
    int static_object;
    int lod_bias;
    float bounds_min[3];
    float bounds_max[3];
    
    // Type-specific properties
    union {
        struct {  // Static mesh
            int collision_enabled;
            int collision_type;
            float lod_distances[MAX_LOD_LEVELS];
            int lod_models[MAX_LOD_LEVELS];
            int lightmap_resolution;
            float lightmap_scale;
        } mesh;
        
        struct {  // Light
            float color[3];
            float intensity;
            float range;
            int light_type;  // 0=directional, 1=point, 2=spot, 3=area
            float spot_angle;
            float spot_softness;
            int cast_shadows;
            int shadow_resolution;
            float shadow_bias;
            int volumetric;
            float volumetric_intensity;
            float temperature;
            int use_ies_profile;
            char ies_filename[256];
        } light;
        
        struct {  // Spawn point
            int team;
            int spawn_type;
            float spawn_radius;
            int priority;
        } spawn;
        
        struct {  // Trigger
            float bounds[6];  // min_x, min_y, min_z, max_x, max_y, max_z
            char script[512];
            int trigger_once;
            int trigger_delay;
            char tag_filter[128];
            int shape_type;  // 0=box, 1=sphere, 2=capsule
        } trigger;
        
        struct {  // Particle emitter
            int particle_system_id;
            float emission_rate;
            float lifetime;
            int max_particles;
            int auto_play;
            int looping;
        } particle;
        
        struct {  // Audio source
            int sound_id;
            float volume;
            float pitch;
            float min_distance;
            float max_distance;
            int spatial;
            int looping;
            int auto_play;
            float doppler_level;
        } audio;
        
        struct {  // Volume
            float bounds[6];
            int volume_type;  // 0=fog, 1=water, 2=post-process
            float density;
            float color[3];
            int priority;
        } volume;
        
        struct {  // Spline
            float* control_points;
            int point_count;
            int closed;
            float tension;
            int segments_per_curve;
        } spline;
        
        struct {  // Prefab instance
            int prefab_id;
            float instance_seed;
            int override_materials;
        } prefab;
    } properties;
    
    // Custom properties
    char* custom_properties;
    int custom_properties_size;
} EditorObject;

// Terrain chunk for optimization
typedef struct {
    float heights[TERRAIN_CHUNK_SIZE][TERRAIN_CHUNK_SIZE];
    float normals[TERRAIN_CHUNK_SIZE][TERRAIN_CHUNK_SIZE][3];
    int dirty;
    float bounds_min[3];
    float bounds_max[3];
} TerrainChunk;

// Terrain structure
typedef struct {
    float* heights;  // Dynamic allocation for variable size
    float* normals;  // Computed normals
    int texture_ids[8];  // Up to 8 blended textures
    float texture_scales[8];
    float* blend_weights;  // Per-vertex blend weights
    float* vegetation_map;  // Vegetation density
    float* water_map;  // Water level map
    int size_x, size_z;
    float scale;
    float vertical_scale;
    TerrainChunk* chunks;
    int chunk_count_x, chunk_count_z;
    int detail_texture_id;
    float detail_scale;
    float tessellation_factor;
} TerrainData;

// Terrain brush settings
typedef struct {
    TerrainBrushType type;
    float radius;
    float strength;
    float falloff;
    int smooth_edges;
    float target_height;
    float noise_scale;
    float noise_octaves;
    int texture_layer;
    float flow_rate;  // For erosion
    float deposition_rate;  // For erosion
} TerrainBrush;

// Undo/Redo system
typedef enum {
    UNDO_TYPE_OBJECT_CREATE,
    UNDO_TYPE_OBJECT_DELETE,
    UNDO_TYPE_OBJECT_MODIFY,
    UNDO_TYPE_TERRAIN_MODIFY,
    UNDO_TYPE_SELECTION_CHANGE,
    UNDO_TYPE_LAYER_CHANGE,
    UNDO_TYPE_BATCH
} UndoType;

typedef struct UndoAction {
    UndoType type;
    char description[128];
    time_t timestamp;
    
    // Object-related undo data
    int* object_ids;
    int object_count;
    EditorObject* old_states;
    EditorObject* new_states;
    
    // Terrain-related undo data
    float* terrain_backup;
    int terrain_region[4];  // x1, z1, x2, z2
    
    // Batch undo support
    struct UndoAction* sub_actions;
    int sub_action_count;
} UndoAction;

// Editor camera
typedef struct {
    float position[3];
    float target[3];
    float rotation[2];  // yaw, pitch
    float distance;
    float fov;
    float near_plane;
    float far_plane;
    float speed;
    float smoothing;
    int free_camera;
    int orthographic;
    float ortho_size;
    float movement_speed;
    float rotation_speed;
    
    // Camera bookmarks
    float bookmarks[10][7];  // position + rotation for 10 bookmarks
    int bookmark_count;
} EditorCamera;

// Selection system
typedef struct {
    int* object_ids;  // Dynamic array
    int capacity;
    int count;
    float bounds_min[3];
    float bounds_max[3];
    float center[3];
    float last_transform[16];  // Last transformation matrix
    SelectionMode mode;
    
    // Lasso selection
    float* lasso_points;
    int lasso_point_count;
    int lasso_capacity;
} Selection;

// Editor statistics
typedef struct {
    int total_objects;
    int visible_objects;
    int selected_objects;
    int total_triangles;
    int visible_triangles;
    float terrain_memory_mb;
    float asset_memory_mb;
    float last_frame_time;
    int undo_memory_mb;
    time_t session_start;
    int changes_since_save;
} EditorStats;

// Editor configuration
typedef struct {
    int autosave_enabled;
    int autosave_interval;
    int max_undo_steps;
    float camera_speed_multiplier;
    int show_grid;
    int show_gizmos;
    int show_wireframe;
    int show_bounds;
    int show_normals;
    int show_statistics;
    float grid_size;
    int grid_subdivisions;
    int snap_enabled;
    float snap_angle;
    int viewport_shading;  // 0=solid, 1=wireframe, 2=textured, 3=lit
    float selection_color[4];
    float grid_color[4];
    int multithread_enabled;
    int gpu_acceleration;
} EditorConfig;

// ========================================================================
// GLOBAL EDITOR STATE
// ========================================================================

static EditorObject* g_editor_objects = NULL;
static int g_object_capacity = MAX_EDITOR_OBJECTS;
static int g_object_count = 0;
static int g_next_object_id = 1;

static Asset* g_asset_cache = NULL;
static int g_asset_capacity = MAX_ASSET_CACHE;
static int g_asset_count = 0;

static Material* g_materials = NULL;
static int g_material_capacity = 256;
static int g_material_count = 0;

static EditorLayer g_layers[MAX_LAYERS];
static int g_layer_count = 1;  // Default layer always exists
static int g_active_layer = 0;

static Prefab g_prefabs[MAX_PREFABS];
static int g_prefab_count = 0;

static TerrainData g_terrain;
static TerrainBrush g_terrain_brush;

static EditorCamera g_camera;
static Selection g_selection;
static EditorStats g_stats;
static EditorConfig g_config;

static UndoAction* g_undo_stack = NULL;
static int g_undo_capacity = MAX_UNDO_STACK;
static int g_undo_head = 0;
static int g_undo_tail = 0;
static int g_undo_count = 0;
static int g_undo_group_depth = 0;

static EditorMode g_current_mode = EDITOR_MODE_SELECT;
static int g_editor_initialized = 0;
static time_t g_last_autosave = 0;
static char g_current_level_file[512] = "";

// Tool state
static int g_dragging = 0;
static float g_drag_start[3];
static float g_drag_offset[3];
static float g_gizmo_size = 1.0f;
static int g_active_axis = -1;  // -1=none, 0=X, 1=Y, 2=Z, 3=XY, 4=YZ, 5=XZ, 6=XYZ
static int g_transform_space = 0;  // 0=world, 1=local
static float g_transform_pivot[3];

// Performance optimization
static int* g_visible_objects = NULL;
static int g_visible_object_count = 0;
static int g_frustum_cull_enabled = 1;
static float g_lod_bias = 1.0f;

// ========================================================================
// FORWARD DECLARATIONS
// ========================================================================

// Core functions
int initialize_level_editor(void);
void cleanup_level_editor(void);
void update_editor_camera(float delta_time);
void update_visible_objects(void);

// Asset management
int load_asset(const char* filename, const char* display_name, AssetType type);
int load_asset_data(Asset* asset);
void unload_asset(int asset_id);
int check_asset_hot_reload(int asset_id);
void generate_texture_thumbnail(Asset* asset, int width, int height);
void generate_model_thumbnail(Asset* asset);
void save_thumbnail_bmp(const char* filename, unsigned char* data, int width, int height);
const char* get_asset_type_string(AssetType type);

// Asset processing
int process_texture_asset(Asset* asset);
int process_model_asset(Asset* asset);
int process_sound_asset(Asset* asset);
int process_material_asset(Asset* asset);
int process_prefab_asset(Asset* asset);

// Object management
int create_editor_object(EditorObjectType type, float x, float y, float z);
void delete_editor_object(int object_id);
EditorObject* find_editor_object(int object_id);
void duplicate_selected_objects(float offset_x, float offset_y, float offset_z);
void group_selected_objects(void);
void calculate_object_bounds(EditorObject* obj);

// Selection system
void clear_selection(void);
void add_to_selection(int object_id);
void remove_from_selection(int object_id);
void toggle_selection(int object_id);
void select_all(void);
void select_by_type(EditorObjectType type);
void select_by_asset(int asset_id);
void invert_selection(void);
void update_selection_bounds(void);
void box_select(float min_x, float min_y, float max_x, float max_y, int additive);
void begin_lasso_selection(void);
void add_lasso_point(float x, float y);
void end_lasso_selection(int additive);

// Transformation
void move_selected_objects(float delta_x, float delta_y, float delta_z, int relative);
void rotate_selected_objects(float delta_x, float delta_y, float delta_z, int relative);
void scale_selected_objects(float scale_x, float scale_y, float scale_z, int relative);
void align_selected_objects(int axis, int alignment);
void distribute_selected_objects(int axis);
float snap_to_grid(float value);
float snap_to_angle(float angle);

// Camera system
void camera_mouse_input(int dx, int dy, int button, int wheel);
void focus_camera_on_selection(void);
void save_camera_bookmark(int slot);
void load_camera_bookmark(int slot);

// Terrain editing
void modify_terrain_height(float world_x, float world_z, int apply);
void paint_terrain_texture(float world_x, float world_z, int texture_layer);
void update_terrain_normals(int min_x, int max_x, int min_z, int max_z);
void mark_terrain_chunks_dirty(int min_x, int max_x, int min_z, int max_z);
int load_terrain_heightmap(const char* filename);
int export_terrain_heightmap(const char* filename);
void resize_terrain(int new_width, int new_height);

// Terrain brush functions
void apply_terrain_brush_height(int center_x, int center_z, int min_x, int max_x, int min_z, int max_z, int apply);
void apply_terrain_brush_smooth(int center_x, int center_z, int min_x, int max_x, int min_z, int max_z, int apply);
void apply_terrain_brush_flatten(int center_x, int center_z, int min_x, int max_x, int min_z, int max_z, int apply);
void apply_terrain_brush_noise(int center_x, int center_z, int min_x, int max_x, int min_z, int max_z, int apply);
void apply_terrain_brush_erosion(int center_x, int center_z, int min_x, int max_x, int min_z, int max_z, int apply);

// Layer system
int create_layer(const char* name, int parent_id);
void delete_layer(int layer_id);
void set_layer_visibility(int layer_id, int visible);
void move_objects_to_layer(int* object_ids, int count, int target_layer);

// Prefab system
int create_prefab_from_selection(const char* name, const char* description);
int instantiate_prefab(int prefab_id, float x, float y, float z);

// Undo/Redo system
void begin_undo_group(const char* description);
void end_undo_group(void);
void add_undo_action(UndoType type, int object_id, EditorObject* old_state, EditorObject* new_state);
void add_undo_action_data(UndoAction* action);
void undo(void);
void redo(void);
void apply_undo_action(UndoAction* action, int is_undo);
void free_undo_action(UndoAction* action);

// File I/O
int save_level(const char* filename);
int load_level(const char* filename);
void check_autosave(void);
int export_level_optimized(const char* filename);
void save_terrain_data(FILE* file);
void save_object_data(FILE* file);
void save_object_properties(FILE* file, EditorObject* obj);
void load_terrain_property(const char* key, const char* value);
void load_object_property(EditorObject* obj, const char* key, const char* value);
void load_object_type_property(EditorObject* obj, const char* key, const char* value);

// Material system
void create_default_materials(void);
int create_material(const char* name, int diffuse_tex, int normal_tex, int specular_tex, int emissive_tex);

// Utility functions
void editor_log(int level, const char* format, ...);
void generate_guid(char* guid);
const char* get_timestamp(void);
const char* get_filename_without_path(const char* path);
int is_key_pressed(int key);
void load_default_assets(void);
void create_default_prefabs(void);
void load_editor_config(const char* filename);
void save_editor_config(const char* filename);

// ========================================================================
// INTERNAL UTILITY FUNCTIONS
// ========================================================================

/**
 * Logs editor messages
 * @param level Log level (0=info, 1=warning, 2=error)
 * @param format Printf-style format string
 * @param ... Variable arguments
 */
static void editor_log(int level, const char* format, ...)
{
    char buffer[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    const char* level_str[] = { "INFO", "WARN", "ERROR" };
    
    // Add timestamp
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_info);
    
    // Output to debug console
    char output[1280];
    sprintf(output, "[EDITOR %s] %s: %s\n", timestamp, level_str[level], buffer);
    OutputDebugString(output);
    
    // Also write to log file
    FILE* log_file = fopen("editor_log.txt", "a");
    if (log_file) {
        fprintf(log_file, "%s", output);
        fclose(log_file);
    }
}

/**
 * Generates a unique GUID for assets
 * @param guid Output buffer (must be at least 64 bytes)
 */
static void generate_guid(char* guid)
{
    sprintf(guid, "%08X-%04X-%04X-%04X-%012X",
            rand(), rand() & 0xFFFF, rand() & 0xFFFF,
            rand() & 0xFFFF, rand());
}

/**
 * Calculates bounding box for an object
 * @param obj Object to calculate bounds for
 */
static void calculate_object_bounds(EditorObject* obj)
{
    // Default 1x1x1 bounds
    obj->bounds_min[0] = obj->position[0] - 0.5f * obj->scale[0];
    obj->bounds_min[1] = obj->position[1] - 0.5f * obj->scale[1];
    obj->bounds_min[2] = obj->position[2] - 0.5f * obj->scale[2];
    obj->bounds_max[0] = obj->position[0] + 0.5f * obj->scale[0];
    obj->bounds_max[1] = obj->position[1] + 0.5f * obj->scale[1];
    obj->bounds_max[2] = obj->position[2] + 0.5f * obj->scale[2];
    
    // TODO: Calculate actual bounds from mesh data if available
}

// ========================================================================
// CORE EDITOR FUNCTIONS
// ========================================================================

/**
 * Initializes the level editor
 * @return 1 on success, 0 on failure
 */
int initialize_level_editor(void)
{
    if (g_editor_initialized) {
        editor_log(1, "Editor already initialized");
        return 1;
    }
    
    editor_log(0, "Initializing level editor");
    
    // Seed random number generator
    srand((unsigned int)time(NULL));
    
    // Allocate dynamic arrays
    g_editor_objects = (EditorObject*)calloc(g_object_capacity, sizeof(EditorObject));
    if (!g_editor_objects) {
        editor_log(2, "Failed to allocate object array");
        return 0;
    }
    
    g_asset_cache = (Asset*)calloc(g_asset_capacity, sizeof(Asset));
    if (!g_asset_cache) {
        editor_log(2, "Failed to allocate asset cache");
        free(g_editor_objects);
        return 0;
    }
    
    g_materials = (Material*)calloc(g_material_capacity, sizeof(Material));
    if (!g_materials) {
        editor_log(2, "Failed to allocate material array");
        free(g_editor_objects);
        free(g_asset_cache);
        return 0;
    }
    
    g_undo_stack = (UndoAction*)calloc(g_undo_capacity, sizeof(UndoAction));
    if (!g_undo_stack) {
        editor_log(2, "Failed to allocate undo stack");
        free(g_editor_objects);
        free(g_asset_cache);
        free(g_materials);
        return 0;
    }
    
    g_visible_objects = (int*)calloc(g_object_capacity, sizeof(int));
    if (!g_visible_objects) {
        editor_log(2, "Failed to allocate visible object array");
        free(g_editor_objects);
        free(g_asset_cache);
        free(g_materials);
        free(g_undo_stack);
        return 0;
    }
    
    // Initialize selection
    g_selection.capacity = MAX_SELECTION;
    g_selection.object_ids = (int*)calloc(g_selection.capacity, sizeof(int));
    g_selection.lasso_capacity = 256;
    g_selection.lasso_points = (float*)calloc(g_selection.lasso_capacity * 2, sizeof(float));
    
    // Initialize default layer
    strcpy(g_layers[0].name, "Default");
    g_layers[0].visible = 1;
    g_layers[0].locked = 0;
    g_layers[0].selectable = 1;
    g_layers[0].color[0] = 0.7f;
    g_layers[0].color[1] = 0.7f;
    g_layers[0].color[2] = 0.7f;
    g_layers[0].color[3] = 1.0f;
    g_layers[0].parent_layer = -1;
    g_layer_count = 1;
    
    // Initialize terrain
    memset(&g_terrain, 0, sizeof(TerrainData));
    g_terrain.size_x = 128;
    g_terrain.size_z = 128;
    g_terrain.scale = 1.0f;
    g_terrain.vertical_scale = 1.0f;
    
    // Allocate terrain data
    int terrain_size = g_terrain.size_x * g_terrain.size_z;
    g_terrain.heights = (float*)calloc(terrain_size, sizeof(float));
    g_terrain.normals = (float*)calloc(terrain_size * 3, sizeof(float));
    g_terrain.blend_weights = (float*)calloc(terrain_size * 8, sizeof(float));
    g_terrain.vegetation_map = (float*)calloc(terrain_size, sizeof(float));
    g_terrain.water_map = (float*)calloc(terrain_size, sizeof(float));
    
    // Initialize terrain chunks
    g_terrain.chunk_count_x = (g_terrain.size_x + TERRAIN_CHUNK_SIZE - 1) / TERRAIN_CHUNK_SIZE;
    g_terrain.chunk_count_z = (g_terrain.size_z + TERRAIN_CHUNK_SIZE - 1) / TERRAIN_CHUNK_SIZE;
    g_terrain.chunks = (TerrainChunk*)calloc(g_terrain.chunk_count_x * g_terrain.chunk_count_z, 
                                             sizeof(TerrainChunk));
    
    // Initialize terrain textures
    for (int i = 0; i < 8; i++) {
        g_terrain.texture_ids[i] = -1;
        g_terrain.texture_scales[i] = 10.0f;
    }
    
    // Initialize terrain brush
    g_terrain_brush.type = TERRAIN_BRUSH_RAISE;
    g_terrain_brush.radius = 5.0f;
    g_terrain_brush.strength = 0.5f;
    g_terrain_brush.falloff = 0.5f;
    g_terrain_brush.smooth_edges = 1;
    g_terrain_brush.target_height = 0.0f;
    g_terrain_brush.noise_scale = 1.0f;
    g_terrain_brush.noise_octaves = 4.0f;
    g_terrain_brush.texture_layer = 0;
    g_terrain_brush.flow_rate = 0.1f;
    g_terrain_brush.deposition_rate = 0.05f;
    
    // Initialize camera
    g_camera.position[0] = 10.0f;
    g_camera.position[1] = 20.0f;
    g_camera.position[2] = -10.0f;
    g_camera.target[0] = 0.0f;
    g_camera.target[1] = 0.0f;
    g_camera.target[2] = 0.0f;
    g_camera.rotation[0] = -45.0f;  // yaw
    g_camera.rotation[1] = -30.0f;  // pitch
    g_camera.distance = 30.0f;
    g_camera.fov = 60.0f;
    g_camera.near_plane = 0.1f;
    g_camera.far_plane = 1000.0f;
    g_camera.speed = 20.0f;
    g_camera.smoothing = 0.1f;
    g_camera.free_camera = 0;
    g_camera.orthographic = 0;
    g_camera.ortho_size = 50.0f;
    g_camera.movement_speed = 1.0f;
    g_camera.rotation_speed = 1.0f;
    g_camera.bookmark_count = 0;
    
    // Initialize configuration
    g_config.autosave_enabled = 1;
    g_config.autosave_interval = AUTOSAVE_INTERVAL;
    g_config.max_undo_steps = MAX_UNDO_STACK;
    g_config.camera_speed_multiplier = 1.0f;
    g_config.show_grid = 1;
    g_config.show_gizmos = 1;
    g_config.show_wireframe = 0;
    g_config.show_bounds = 0;
    g_config.show_normals = 0;
    g_config.show_statistics = 1;
    g_config.grid_size = GRID_SIZE;
    g_config.grid_subdivisions = 10;
    g_config.snap_enabled = 1;
    g_config.snap_angle = 15.0f;
    g_config.viewport_shading = 3;  // Lit by default
    g_config.selection_color[0] = 1.0f;
    g_config.selection_color[1] = 0.5f;
    g_config.selection_color[2] = 0.0f;
    g_config.selection_color[3] = 1.0f;
    g_config.grid_color[0] = 0.5f;
    g_config.grid_color[1] = 0.5f;
    g_config.grid_color[2] = 0.5f;
    g_config.grid_color[3] = 0.5f;
    g_config.multithread_enabled = 1;
    g_config.gpu_acceleration = 1;
    
    // Clear selection
    memset(&g_selection, 0, sizeof(Selection));
    g_selection.mode = SELECTION_MODE_SINGLE;
    
    // Initialize statistics
    memset(&g_stats, 0, sizeof(EditorStats));
    g_stats.session_start = time(NULL);
    
    // Load default assets
    load_default_assets();
    
    // Create default materials
    create_default_materials();
    
    // Create default prefabs
    create_default_prefabs();
    
    // Load editor configuration
    load_editor_config("editor_config.ini");
    
    g_editor_initialized = 1;
    g_last_autosave = time(NULL);
    
    editor_log(0, "Level editor initialized successfully");
    return 1;
}

/**
 * Loads default assets into the editor
 */
void load_default_assets(void)
{
    editor_log(0, "Loading default assets");
    
    // Create asset directories if they don't exist
    _mkdir("assets");
    _mkdir("assets/models");
    _mkdir("assets/textures");
    _mkdir("assets/materials");
    _mkdir("assets/sounds");
    _mkdir("assets/scripts");
    _mkdir("assets/prefabs");
    _mkdir("thumbnails");
    
    // Load primitive models
    load_asset("assets/models/primitives/cube.obj", "Cube", ASSET_TYPE_MODEL);
    load_asset("assets/models/primitives/sphere.obj", "Sphere", ASSET_TYPE_MODEL);
    load_asset("assets/models/primitives/cylinder.obj", "Cylinder", ASSET_TYPE_MODEL);
    load_asset("assets/models/primitives/cone.obj", "Cone", ASSET_TYPE_MODEL);
    load_asset("assets/models/primitives/torus.obj", "Torus", ASSET_TYPE_MODEL);
    load_asset("assets/models/primitives/plane.obj", "Plane", ASSET_TYPE_MODEL);
    
    // Load default textures
    load_asset("assets/textures/default/white.bmp", "White", ASSET_TYPE_TEXTURE);
    load_asset("assets/textures/default/checker.bmp", "Checker", ASSET_TYPE_TEXTURE);
    load_asset("assets/textures/default/grid.bmp", "Grid", ASSET_TYPE_TEXTURE);
    load_asset("assets/textures/default/normal_flat.bmp", "Flat Normal", ASSET_TYPE_TEXTURE);
    
    // Load terrain textures
    load_asset("assets/textures/terrain/grass.bmp", "Grass", ASSET_TYPE_TEXTURE);
    load_asset("assets/textures/terrain/dirt.bmp", "Dirt", ASSET_TYPE_TEXTURE);
    load_asset("assets/textures/terrain/rock.bmp", "Rock", ASSET_TYPE_TEXTURE);
    load_asset("assets/textures/terrain/sand.bmp", "Sand", ASSET_TYPE_TEXTURE);
    load_asset("assets/textures/terrain/snow.bmp", "Snow", ASSET_TYPE_TEXTURE);
    
    // Load editor icons/gizmos
    load_asset("assets/textures/editor/move_gizmo.bmp", "Move Gizmo", ASSET_TYPE_TEXTURE);
    load_asset("assets/textures/editor/rotate_gizmo.bmp", "Rotate Gizmo", ASSET_TYPE_TEXTURE);
    load_asset("assets/textures/editor/scale_gizmo.bmp", "Scale Gizmo", ASSET_TYPE_TEXTURE);
    load_asset("assets/textures/editor/light_icon.bmp", "Light Icon", ASSET_TYPE_TEXTURE);
    load_asset("assets/textures/editor/camera_icon.bmp", "Camera Icon", ASSET_TYPE_TEXTURE);
    
    editor_log(0, "Default assets loaded");
}

/**
 * Loads an asset into the cache
 * @param filename Asset file path
 * @param display_name Display name for the asset
 * @param type Asset type
 * @return Asset ID or -1 on failure
 */
int load_asset(const char* filename, const char* display_name, AssetType type)
{
    // Check if already loaded
    for (int i = 0; i < g_asset_count; i++) {
        if (strcmp(g_asset_cache[i].filename, filename) == 0) {
            g_asset_cache[i].reference_count++;
            editor_log(0, "Asset already loaded: %s (ref count: %d)", 
                      filename, g_asset_cache[i].reference_count);
            return i;
        }
    }
    
    // Check capacity
    if (g_asset_count >= g_asset_capacity) {
        // Expand array
        int new_capacity = g_asset_capacity * 2;
        Asset* new_cache = (Asset*)realloc(g_asset_cache, new_capacity * sizeof(Asset));
        if (!new_cache) {
            editor_log(2, "Failed to expand asset cache");
            return -1;
        }
        g_asset_cache = new_cache;
        g_asset_capacity = new_capacity;
        editor_log(0, "Expanded asset cache to %d entries", new_capacity);
    }
    
    Asset* asset = &g_asset_cache[g_asset_count];
    memset(asset, 0, sizeof(Asset));
    
    strncpy(asset->filename, filename, sizeof(asset->filename) - 1);
    strncpy(asset->display_name, display_name, sizeof(asset->display_name) - 1);
    asset->type = type;
    asset->loaded = 0;
    asset->reference_count = 1;
    generate_guid(asset->guid);
    
    // Set default import settings based on type
    switch (type) {
        case ASSET_TYPE_MODEL:
            asset->import_scale = 1.0f;
            break;
        case ASSET_TYPE_TEXTURE:
            asset->mipmap_count = -1;  // Auto-generate
            asset->compression_type = 1;  // DXT1
            break;
    }
    
    // Generate thumbnail path
    sprintf(asset->thumbnail_path, "thumbnails/%s_%s.bmp", 
            get_asset_type_string(type), asset->guid);
    
    // Load the actual asset data
    if (load_asset_data(asset)) {
        asset->loaded = 1;
        editor_log(0, "Loaded asset: %s (%s)", display_name, filename);
        return g_asset_count++;
    }
    
    editor_log(2, "Failed to load asset: %s", filename);
    return -1;
}

/**
 * Loads asset data from file
 * @param asset Asset to load data for
 * @return 1 on success, 0 on failure
 */
int load_asset_data(Asset* asset)
{
    // Check if file exists
    FILE* file = fopen(asset->filename, "rb");
    if (!file) {
        editor_log(2, "Asset file not found: %s", asset->filename);
        return 0;
    }
    
    // Get file info
    fseek(file, 0, SEEK_END);
    size_t file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    // Get last modified time
    struct stat file_stat;
    if (stat(asset->filename, &file_stat) == 0) {
        asset->last_modified = file_stat.st_mtime;
        asset->last_checked = time(NULL);
    }
    
    // Allocate memory
    asset->data = malloc(file_size);
    if (!asset->data) {
        editor_log(2, "Failed to allocate memory for asset: %s", asset->filename);
        fclose(file);
        return 0;
    }
    
    // Read file
    size_t bytes_read = fread(asset->data, 1, file_size, file);
    fclose(file);
    
    if (bytes_read != file_size) {
        editor_log(2, "Failed to read asset file: %s", asset->filename);
        free(asset->data);
        asset->data = NULL;
        return 0;
    }
    
    asset->data_size = file_size;
    
    // Process asset based on type
    int success = 0;
    switch (asset->type) {
        case ASSET_TYPE_TEXTURE:
            success = process_texture_asset(asset);
            break;
            
        case ASSET_TYPE_MODEL:
            success = process_model_asset(asset);
            break;
            
        case ASSET_TYPE_SOUND:
            success = process_sound_asset(asset);
            break;
            
        case ASSET_TYPE_MATERIAL:
            success = process_material_asset(asset);
            break;
            
        case ASSET_TYPE_PREFAB:
            success = process_prefab_asset(asset);
            break;
            
        default:
            success = 1;  // Basic file loading successful
            break;
    }
    
    if (!success) {
        free(asset->data);
        asset->data = NULL;
        asset->data_size = 0;
        return 0;
    }
    
    // Update statistics
    g_stats.asset_memory_mb += asset->data_size / (1024.0f * 1024.0f);
    
    return 1;
}

/**
 * Processes a texture asset
 * @param asset Texture asset to process
 * @return 1 on success, 0 on failure
 */
int process_texture_asset(Asset* asset)
{
    if (asset->data_size < 54) {
        editor_log(2, "Texture file too small: %s", asset->filename);
        return 0;
    }
    
    unsigned char* data = (unsigned char*)asset->data;
    
    // Check BMP signature
    if (data[0] != 'B' || data[1] != 'M') {
        editor_log(2, "Not a valid BMP file: %s", asset->filename);
        return 0;
    }
    
    // Extract dimensions
    int width = *(int*)(data + 18);
    int height = *(int*)(data + 22);
    int bpp = *(short*)(data + 28);
    
    if (width <= 0 || height <= 0 || width > 4096 || height > 4096) {
        editor_log(2, "Invalid texture dimensions: %dx%d", width, height);
        return 0;
    }
    
    if (bpp != 24 && bpp != 32) {
        editor_log(2, "Unsupported bit depth: %d", bpp);
        return 0;
    }
    
    // Add texture info to tags
    sprintf(asset->tags, "width:%d,height:%d,bpp:%d", width, height, bpp);
    
    // Generate thumbnail
    generate_texture_thumbnail(asset, width, height);
    
    // Generate mipmaps if requested
    if (asset->mipmap_count != 0) {
        // TODO: Generate mipmaps
    }
    
    editor_log(0, "Processed texture: %dx%d, %d bpp", width, height, bpp);
    return 1;
}

/**
 * Processes a model asset
 * @param asset Model asset to process
 * @return 1 on success, 0 on failure
 */
int process_model_asset(Asset* asset)
{
    char* data = (char*)asset->data;
    
    // Simple OBJ validation
    if (!strstr(data, "v ") && !strstr(data, "f ")) {
        editor_log(2, "Not a valid OBJ file: %s", asset->filename);
        return 0;
    }
    
    // Count vertices, normals, UVs, and faces
    int vertex_count = 0;
    int normal_count = 0;
    int uv_count = 0;
    int face_count = 0;
    float bounds_min[3] = { FLT_MAX, FLT_MAX, FLT_MAX };
    float bounds_max[3] = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
    
    // Make a copy for parsing
    char* data_copy = (char*)malloc(asset->data_size + 1);
    memcpy(data_copy, data, asset->data_size);
    data_copy[asset->data_size] = '\0';
    
    char* line = strtok(data_copy, "\n");
    while (line != NULL) {
        if (strncmp(line, "v ", 2) == 0) {
            vertex_count++;
            
            // Parse vertex for bounds calculation
            float x, y, z;
            if (sscanf(line + 2, "%f %f %f", &x, &y, &z) == 3) {
                bounds_min[0] = fminf(bounds_min[0], x);
                bounds_min[1] = fminf(bounds_min[1], y);
                bounds_min[2] = fminf(bounds_min[2], z);
                bounds_max[0] = fmaxf(bounds_max[0], x);
                bounds_max[1] = fmaxf(bounds_max[1], y);
                bounds_max[2] = fmaxf(bounds_max[2], z);
            }
        } else if (strncmp(line, "vn ", 3) == 0) {
            normal_count++;
        } else if (strncmp(line, "vt ", 3) == 0) {
            uv_count++;
        } else if (strncmp(line, "f ", 2) == 0) {
            face_count++;
        }
        line = strtok(NULL, "\n");
    }
    
    free(data_copy);
    
    if (vertex_count == 0 || face_count == 0) {
        editor_log(2, "Invalid model data: %d vertices, %d faces", vertex_count, face_count);
        return 0;
    }
    
    // Calculate model size
    float size_x = bounds_max[0] - bounds_min[0];
    float size_y = bounds_max[1] - bounds_min[1];
    float size_z = bounds_max[2] - bounds_min[2];
    
    // Add model info to tags
    sprintf(asset->tags, "vertices:%d,faces:%d,size:%.2fx%.2fx%.2f", 
            vertex_count, face_count, size_x, size_y, size_z);
    
    // Check for dependencies (materials)
    if (strstr(data, "mtllib ")) {
        // TODO: Parse material dependencies
    }
    
    // Generate thumbnail
    generate_model_thumbnail(asset);
    
    editor_log(0, "Processed model: %d vertices, %d faces, size: %.2fx%.2fx%.2f",
              vertex_count, face_count, size_x, size_y, size_z);
    
    return 1;
}

/**
 * Processes a sound asset
 * @param asset Sound asset to process
 * @return 1 on success, 0 on failure
 */
int process_sound_asset(Asset* asset)
{
    if (asset->data_size < 44) {
        editor_log(2, "Sound file too small: %s", asset->filename);
        return 0;
    }
    
    unsigned char* data = (unsigned char*)asset->data;
    
    // Check WAV signature
    if (memcmp(data, "RIFF", 4) != 0 || memcmp(data + 8, "WAVE", 4) != 0) {
        editor_log(2, "Not a valid WAV file: %s", asset->filename);
        return 0;
    }
    
    // Find fmt chunk
    int pos = 12;
    while (pos < asset->data_size - 8) {
        if (memcmp(data + pos, "fmt ", 4) == 0) {
            // Parse format info
            int chunk_size = *(int*)(data + pos + 4);
            short format = *(short*)(data + pos + 8);
            short channels = *(short*)(data + pos + 10);
            int sample_rate = *(int*)(data + pos + 12);
            short bits_per_sample = *(short*)(data + pos + 22);
            
            // Add sound info to tags
            sprintf(asset->tags, "format:%d,channels:%d,rate:%d,bits:%d",
                    format, channels, sample_rate, bits_per_sample);
            
            editor_log(0, "Processed sound: %d Hz, %d channels, %d bits",
                      sample_rate, channels, bits_per_sample);
            return 1;
        }
        pos += 8 + *(int*)(data + pos + 4);
    }
    
    editor_log(2, "Invalid WAV format: %s", asset->filename);
    return 0;
}

/**
 * Processes a material asset
 * @param asset Material asset to process
 * @return 1 on success, 0 on failure
 */
int process_material_asset(Asset* asset)
{
    // Parse material definition file
    // TODO: Implement material parsing
    return 1;
}

/**
 * Processes a prefab asset
 * @param asset Prefab asset to process
 * @return 1 on success, 0 on failure
 */
int process_prefab_asset(Asset* asset)
{
    // Parse prefab definition file
    // TODO: Implement prefab parsing
    return 1;
}

/**
 * Checks if asset file has been modified and reloads if necessary
 * @param asset_id Asset to check
 * @return 1 if reloaded, 0 otherwise
 */
int check_asset_hot_reload(int asset_id)
{
    if (asset_id < 0 || asset_id >= g_asset_count) {
        return 0;
    }
    
    Asset* asset = &g_asset_cache[asset_id];
    
    // Check every 5 seconds
    time_t now = time(NULL);
    if (now - asset->last_checked < 5) {
        return 0;
    }
    
    asset->last_checked = now;
    
    // Check file modification time
    struct stat file_stat;
    if (stat(asset->filename, &file_stat) != 0) {
        return 0;
    }
    
    if (file_stat.st_mtime > asset->last_modified) {
        editor_log(0, "Hot-reloading asset: %s", asset->display_name);
        
        // Free old data
        if (asset->data) {
            free(asset->data);
            g_stats.asset_memory_mb -= asset->data_size / (1024.0f * 1024.0f);
        }
        
        // Reload
        if (load_asset_data(asset)) {
            asset->last_modified = file_stat.st_mtime;
            
            // Notify objects using this asset
            for (int i = 0; i < g_object_count; i++) {
                if (g_editor_objects[i].asset_id == asset_id) {
                    // TODO: Update object with new asset data
                }
            }
            
            return 1;
        }
    }
    
    return 0;
}

/**
 * Generates a thumbnail for a texture asset
 * @param asset Texture asset
 * @param width Texture width
 * @param height Texture height
 */
void generate_texture_thumbnail(Asset* asset, int width, int height)
{
    const int thumb_size = 128;
    unsigned char* thumb_data = (unsigned char*)malloc(thumb_size * thumb_size * 3);
    
    if (!thumb_data) {
        editor_log(2, "Failed to allocate thumbnail memory");
        return;
    }
    
    unsigned char* src_data = (unsigned char*)asset->data + 54;  // Skip BMP header
    
    // Bilinear sampling for better quality
    for (int y = 0; y < thumb_size; y++) {
        for (int x = 0; x < thumb_size; x++) {
            float src_x = (x * (width - 1)) / (float)(thumb_size - 1);
            float src_y = (y * (height - 1)) / (float)(thumb_size - 1);
            
            int x0 = (int)src_x;
            int y0 = (int)src_y;
            int x1 = fminf(x0 + 1, width - 1);
            int y1 = fminf(y0 + 1, height - 1);
            
            float fx = src_x - x0;
            float fy = src_y - y0;
            
            // Sample four pixels
            int idx00 = (y0 * width + x0) * 3;
            int idx10 = (y0 * width + x1) * 3;
            int idx01 = (y1 * width + x0) * 3;
            int idx11 = (y1 * width + x1) * 3;
            
            int thumb_index = (y * thumb_size + x) * 3;
            
            // Bilinear interpolation
            for (int c = 0; c < 3; c++) {
                float v00 = src_data[idx00 + c];
                float v10 = src_data[idx10 + c];
                float v01 = src_data[idx01 + c];
                float v11 = src_data[idx11 + c];
                
                float v0 = v00 * (1 - fx) + v10 * fx;
                float v1 = v01 * (1 - fx) + v11 * fx;
                float v = v0 * (1 - fy) + v1 * fy;
                
                thumb_data[thumb_index + c] = (unsigned char)v;
            }
        }
    }
    
    // Save thumbnail
    save_thumbnail_bmp(asset->thumbnail_path, thumb_data, thumb_size, thumb_size);
    
    // Keep thumbnail in memory for UI
    asset->thumbnail_data = thumb_data;
    asset->thumbnail_size = thumb_size;
}

/**
 * Generates a thumbnail for a model asset
 * @param asset Model asset
 */
void generate_model_thumbnail(Asset* asset)
{
    const int thumb_size = 128;
    unsigned char* thumb_data = (unsigned char*)calloc(thumb_size * thumb_size * 3, 1);
    
    if (!thumb_data) {
        editor_log(2, "Failed to allocate thumbnail memory");
        return;
    }
    
    // TODO: Render model to thumbnail
    // For now, create a placeholder with wireframe pattern
    for (int y = 0; y < thumb_size; y++) {
        for (int x = 0; x < thumb_size; x++) {
            int idx = (y * thumb_size + x) * 3;
            
            // Create grid pattern
            if ((x % 16) == 0 || (y % 16) == 0) {
                thumb_data[idx] = 100;
                thumb_data[idx + 1] = 100;
                thumb_data[idx + 2] = 150;
            } else {
                thumb_data[idx] = 50;
                thumb_data[idx + 1] = 50;
                thumb_data[idx + 2] = 75;
            }
        }
    }
    
    save_thumbnail_bmp(asset->thumbnail_path, thumb_data, thumb_size, thumb_size);
    
    asset->thumbnail_data = thumb_data;
    asset->thumbnail_size = thumb_size;
}

/**
 * Saves a thumbnail as BMP file
 * @param filename Output filename
 * @param data Image data (RGB)
 * @param width Image width
 * @param height Image height
 */
void save_thumbnail_bmp(const char* filename, unsigned char* data, int width, int height)
{
    FILE* file = fopen(filename, "wb");
    if (!file) {
        editor_log(2, "Failed to create thumbnail file: %s", filename);
        return;
    }
    
    // BMP header
    unsigned char header[54] = {
        'B', 'M', 0, 0, 0, 0, 0, 0, 0, 0, 54, 0, 0, 0, 40, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 24, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    };
    
    int row_size = (width * 3 + 3) & ~3;  // Align to 4 bytes
    int image_size = row_size * height;
    int file_size = 54 + image_size;
    
    // Update header
    *(int*)(header + 2) = file_size;
    *(int*)(header + 18) = width;
    *(int*)(header + 22) = height;
    *(int*)(header + 34) = image_size;
    
    fwrite(header, 1, 54, file);
    
    // Write image data (bottom-up)
    unsigned char* row_buffer = (unsigned char*)calloc(row_size, 1);
    for (int y = height - 1; y >= 0; y--) {
        memcpy(row_buffer, data + y * width * 3, width * 3);
        fwrite(row_buffer, 1, row_size, file);
    }
    free(row_buffer);
    
    fclose(file);
}

/**
 * Gets asset type as string
 * @param type Asset type
 * @return Type string
 */
const char* get_asset_type_string(AssetType type)
{
    static const char* type_strings[] = {
        "texture", "model", "sound", "script", "material",
        "prefab", "animation", "shader", "particle", "font"
    };
    
    if (type >= 0 && type < sizeof(type_strings) / sizeof(type_strings[0])) {
        return type_strings[type];
    }
    return "unknown";
}

// ========================================================================
// OBJECT MANAGEMENT
// ========================================================================

/**
 * Creates a new editor object
 * @param type Object type
 * @param x X position
 * @param y Y position
 * @param z Z position
 * @return Object ID or -1 on failure
 */
int create_editor_object(EditorObjectType type, float x, float y, float z)
{
    if (g_object_count >= g_object_capacity) {
        // Expand array
        int new_capacity = g_object_capacity * 2;
        EditorObject* new_objects = (EditorObject*)realloc(g_editor_objects, 
                                                           new_capacity * sizeof(EditorObject));
        if (!new_objects) {
            editor_log(2, "Failed to expand object array");
            return -1;
        }
        g_editor_objects = new_objects;
        
        // Also expand visible objects array
        int* new_visible = (int*)realloc(g_visible_objects, new_capacity * sizeof(int));
        if (!new_visible) {
            editor_log(2, "Failed to expand visible object array");
            return -1;
        }
        g_visible_objects = new_visible;
        
        g_object_capacity = new_capacity;
        editor_log(0, "Expanded object array to %d entries", new_capacity);
    }
    
    EditorObject* obj = &g_editor_objects[g_object_count];
    memset(obj, 0, sizeof(EditorObject));
    
    obj->id = g_next_object_id++;
    obj->type = type;
    obj->position[0] = snap_to_grid(x);
    obj->position[1] = snap_to_grid(y);
    obj->position[2] = snap_to_grid(z);
    obj->rotation[0] = 0.0f;
    obj->rotation[1] = 0.0f;
    obj->rotation[2] = 0.0f;
    obj->scale[0] = 1.0f;
    obj->scale[1] = 1.0f;
    obj->scale[2] = 1.0f;
    obj->selected = 0;
    obj->visible = 1;
    obj->locked = 0;
    obj->asset_id = -1;
    obj->material_id = -1;
    obj->layer_id = g_active_layer;
    obj->parent_id = -1;
    obj->cast_shadows = 1;
    obj->receive_shadows = 1;
    obj->static_object = (type == EDITOR_OBJ_STATIC_MESH);
    obj->lod_bias = 0;
    
    // Set default name and properties based on type
    switch (type) {
        case EDITOR_OBJ_STATIC_MESH:
            sprintf(obj->name, "StaticMesh_%d", obj->id);
            obj->properties.mesh.collision_enabled = 1;
            obj->properties.mesh.collision_type = 0;  // Box
            obj->properties.mesh.lightmap_resolution = 32;
            obj->properties.mesh.lightmap_scale = 1.0f;
            for (int i = 0; i < MAX_LOD_LEVELS; i++) {
                obj->properties.mesh.lod_distances[i] = 50.0f * (i + 1);
                obj->properties.mesh.lod_models[i] = -1;
            }
            break;
            
        case EDITOR_OBJ_LIGHT:
            sprintf(obj->name, "Light_%d", obj->id);
            obj->properties.light.color[0] = 1.0f;
            obj->properties.light.color[1] = 1.0f;
            obj->properties.light.color[2] = 1.0f;
            obj->properties.light.intensity = 1.0f;
            obj->properties.light.range = 10.0f;
            obj->properties.light.light_type = 1;  // Point light
            obj->properties.light.spot_angle = 45.0f;
            obj->properties.light.spot_softness = 0.1f;
            obj->properties.light.cast_shadows = 1;
            obj->properties.light.shadow_resolution = 512;
            obj->properties.light.shadow_bias = 0.001f;
            obj->properties.light.temperature = 6500.0f;
            break;
            
        case EDITOR_OBJ_SPAWN_POINT:
            sprintf(obj->name, "SpawnPoint_%d", obj->id);
            obj->properties.spawn.team = 0;
            obj->properties.spawn.spawn_type = 0;
            obj->properties.spawn.spawn_radius = 1.0f;
            obj->properties.spawn.priority = 0;
            break;
            
        case EDITOR_OBJ_TRIGGER:
            sprintf(obj->name, "Trigger_%d", obj->id);
            obj->properties.trigger.bounds[0] = x - 2.0f;
            obj->properties.trigger.bounds[1] = y - 2.0f;
            obj->properties.trigger.bounds[2] = z - 2.0f;
            obj->properties.trigger.bounds[3] = x + 2.0f;
            obj->properties.trigger.bounds[4] = y + 2.0f;
            obj->properties.trigger.bounds[5] = z + 2.0f;
            obj->properties.trigger.trigger_once = 0;
            obj->properties.trigger.trigger_delay = 0;
            obj->properties.trigger.shape_type = 0;  // Box
            break;
            
        case EDITOR_OBJ_PARTICLE_EMITTER:
            sprintf(obj->name, "ParticleEmitter_%d", obj->id);
            obj->properties.particle.particle_system_id = -1;
            obj->properties.particle.emission_rate = 10.0f;
            obj->properties.particle.lifetime = 5.0f;
            obj->properties.particle.max_particles = 100;
            obj->properties.particle.auto_play = 1;
            obj->properties.particle.looping = 1;
            break;
            
        case EDITOR_OBJ_AUDIO_SOURCE:
            sprintf(obj->name, "AudioSource_%d", obj->id);
            obj->properties.audio.sound_id = -1;
            obj->properties.audio.volume = 1.0f;
            obj->properties.audio.pitch = 1.0f;
            obj->properties.audio.min_distance = 1.0f;
            obj->properties.audio.max_distance = 100.0f;
            obj->properties.audio.spatial = 1;
            obj->properties.audio.looping = 0;
            obj->properties.audio.auto_play = 0;
            obj->properties.audio.doppler_level = 1.0f;
            break;
            
        case EDITOR_OBJ_VOLUME:
            sprintf(obj->name, "Volume_%d", obj->id);
            obj->properties.volume.bounds[0] = x - 5.0f;
            obj->properties.volume.bounds[1] = y - 5.0f;
            obj->properties.volume.bounds[2] = z - 5.0f;
            obj->properties.volume.bounds[3] = x + 5.0f;
            obj->properties.volume.bounds[4] = y + 5.0f;
            obj->properties.volume.bounds[5] = z + 5.0f;
            obj->properties.volume.volume_type = 0;  // Fog
            obj->properties.volume.density = 0.1f;
            obj->properties.volume.color[0] = 0.5f;
            obj->properties.volume.color[1] = 0.5f;
            obj->properties.volume.color[2] = 0.5f;
            obj->properties.volume.priority = 0;
            break;
            
        case EDITOR_OBJ_SPLINE:
            sprintf(obj->name, "Spline_%d", obj->id);
            obj->properties.spline.control_points = (float*)calloc(12, sizeof(float));  // 4 initial points
            obj->properties.spline.point_count = 4;
            obj->properties.spline.closed = 0;
            obj->properties.spline.tension = 0.5f;
            obj->properties.spline.segments_per_curve = 10;
            // Set initial control points
            for (int i = 0; i < 4; i++) {
                obj->properties.spline.control_points[i * 3] = x + i * 2.0f;
                obj->properties.spline.control_points[i * 3 + 1] = y;
                obj->properties.spline.control_points[i * 3 + 2] = z;
            }
            break;
            
        default:
            sprintf(obj->name, "Object_%d", obj->id);
            break;
    }
    
    // Calculate initial bounds
    calculate_object_bounds(obj);
    
    // Add to undo stack
    begin_undo_group("Create Object");
    add_undo_action(UNDO_TYPE_OBJECT_CREATE, obj->id, NULL, obj);
    end_undo_group();
    
    // Update statistics
    g_stats.total_objects++;
    g_stats.changes_since_save++;
    
    editor_log(0, "Created %s at (%.2f, %.2f, %.2f)", obj->name, x, y, z);
    
    return g_object_count++;
}

/**
 * Deletes an editor object
 * @param object_id Object ID to delete
 */
void delete_editor_object(int object_id)
{
    for (int i = 0; i < g_object_count; i++) {
        if (g_editor_objects[i].id == object_id) {
            EditorObject* obj = &g_editor_objects[i];
            
            // Add to undo stack
            begin_undo_group("Delete Object");
            add_undo_action(UNDO_TYPE_OBJECT_DELETE, object_id, obj, NULL);
            end_undo_group();
            
            // Remove from selection if selected
            remove_from_selection(object_id);
            
            // Free type-specific data
            if (obj->type == EDITOR_OBJ_SPLINE && obj->properties.spline.control_points) {
                free(obj->properties.spline.control_points);
            }
            
            // Free custom properties
            if (obj->custom_properties) {
                free(obj->custom_properties);
            }
            
            editor_log(0, "Deleted object: %s", obj->name);
            
            // Shift objects down
            for (int j = i; j < g_object_count - 1; j++) {
                g_editor_objects[j] = g_editor_objects[j + 1];
            }
            g_object_count--;
            
            // Update statistics
            g_stats.total_objects--;
            g_stats.changes_since_save++;
            
            break;
        }
    }
}

/**
 * Finds an editor object by ID
 * @param object_id Object ID to find
 * @return Object pointer or NULL
 */
EditorObject* find_editor_object(int object_id)
{
    for (int i = 0; i < g_object_count; i++) {
        if (g_editor_objects[i].id == object_id) {
            return &g_editor_objects[i];
        }
    }
    return NULL;
}

/**
 * Duplicates selected objects
 * @param offset_x X offset for duplicates
 * @param offset_y Y offset for duplicates
 * @param offset_z Z offset for duplicates
 */
void duplicate_selected_objects(float offset_x, float offset_y, float offset_z)
{
    if (g_selection.count == 0) {
        return;
    }
    
    // Store original selection
    int* original_ids = (int*)malloc(g_selection.count * sizeof(int));
    int original_count = g_selection.count;
    memcpy(original_ids, g_selection.object_ids, original_count * sizeof(int));
    
    // Clear selection to select duplicates
    clear_selection();
    
    begin_undo_group("Duplicate Objects");
    
    // Duplicate each object
    for (int i = 0; i < original_count; i++) {
        EditorObject* original = find_editor_object(original_ids[i]);
        if (!original || original->locked) {
            continue;
        }
        
        int new_id = create_editor_object(original->type,
                                         original->position[0] + offset_x,
                                         original->position[1] + offset_y,
                                         original->position[2] + offset_z);
        
        if (new_id >= 0) {
            EditorObject* duplicate = &g_editor_objects[new_id - 1];  // Last created
            
            // Copy properties
            memcpy(duplicate->rotation, original->rotation, sizeof(float) * 3);
            memcpy(duplicate->scale, original->scale, sizeof(float) * 3);
            duplicate->asset_id = original->asset_id;
            duplicate->material_id = original->material_id;
            duplicate->layer_id = original->layer_id;
            duplicate->cast_shadows = original->cast_shadows;
            duplicate->receive_shadows = original->receive_shadows;
            duplicate->static_object = original->static_object;
            
            // Copy type-specific properties
            switch (original->type) {
                case EDITOR_OBJ_SPLINE:
                    // Deep copy spline points
                    if (original->properties.spline.control_points) {
                        int point_size = original->properties.spline.point_count * 3 * sizeof(float);
                        duplicate->properties.spline.control_points = (float*)malloc(point_size);
                        memcpy(duplicate->properties.spline.control_points,
                               original->properties.spline.control_points, point_size);
                    }
                    break;
                default:
                    duplicate->properties = original->properties;
                    break;
            }
            
            // Copy custom properties
            if (original->custom_properties) {
                duplicate->custom_properties = (char*)malloc(original->custom_properties_size);
                memcpy(duplicate->custom_properties, original->custom_properties,
                       original->custom_properties_size);
                duplicate->custom_properties_size = original->custom_properties_size;
            }
            
            // Update name
            sprintf(duplicate->name, "%s_Copy", original->name);
            
            // Select the duplicate
            add_to_selection(duplicate->id);
            
            // Update asset reference count
            if (duplicate->asset_id >= 0) {
                g_asset_cache[duplicate->asset_id].reference_count++;
            }
        }
    }
    
    end_undo_group();
    
    free(original_ids);
    
    editor_log(0, "Duplicated %d objects", original_count);
}

/**
 * Groups selected objects under a parent
 */
void group_selected_objects(void)
{
    if (g_selection.count < 2) {
        return;
    }
    
    // Create empty parent object at selection center
    int parent_id = create_editor_object(EDITOR_OBJ_STATIC_MESH,
                                        g_selection.center[0],
                                        g_selection.center[1],
                                        g_selection.center[2]);
    
    if (parent_id < 0) {
        return;
    }
    
    EditorObject* parent = &g_editor_objects[parent_id - 1];
    sprintf(parent->name, "Group_%d", parent->id);
    parent->asset_id = -1;  // Empty object
    
    begin_undo_group("Group Objects");
    
    // Parent all selected objects
    for (int i = 0; i < g_selection.count; i++) {
        EditorObject* child = find_editor_object(g_selection.object_ids[i]);
        if (child && child->id != parent->id) {
            // Store old state for undo
            EditorObject old_state = *child;
            
            // Convert to local space
            child->position[0] -= parent->position[0];
            child->position[1] -= parent->position[1];
            child->position[2] -= parent->position[2];
            child->parent_id = parent->id;
            
            add_undo_action(UNDO_TYPE_OBJECT_MODIFY, child->id, &old_state, child);
        }
    }
    
    end_undo_group();
    
    // Select the parent
    clear_selection();
    add_to_selection(parent->id);
    
    editor_log(0, "Grouped %d objects", g_selection.count);
}

// ========================================================================
// SELECTION SYSTEM
// ========================================================================

/**
 * Clears the current selection
 */
void clear_selection(void)
{
    for (int i = 0; i < g_object_count; i++) {
        g_editor_objects[i].selected = 0;
    }
    g_selection.count = 0;
    update_selection_bounds();
    
    editor_log(0, "Selection cleared");
}

/**
 * Adds an object to the selection
 * @param object_id Object ID to add
 */
void add_to_selection(int object_id)
{
    EditorObject* obj = find_editor_object(object_id);
    if (!obj || obj->selected || obj->locked) {
        return;
    }
    
    // Check layer visibility
    if (!g_layers[obj->layer_id].visible || !g_layers[obj->layer_id].selectable) {
        return;
    }
    
    // Expand selection array if needed
    if (g_selection.count >= g_selection.capacity) {
        int new_capacity = g_selection.capacity * 2;
        int* new_ids = (int*)realloc(g_selection.object_ids, new_capacity * sizeof(int));
        if (!new_ids) {
            editor_log(2, "Failed to expand selection array");
            return;
        }
        g_selection.object_ids = new_ids;
        g_selection.capacity = new_capacity;
    }
    
    obj->selected = 1;
    g_selection.object_ids[g_selection.count++] = object_id;
    update_selection_bounds();
    
    g_stats.selected_objects = g_selection.count;
}

/**
 * Removes an object from the selection
 * @param object_id Object ID to remove
 */
void remove_from_selection(int object_id)
{
    EditorObject* obj = find_editor_object(object_id);
    if (!obj || !obj->selected) {
        return;
    }
    
    obj->selected = 0;
    
    // Remove from selection array
    for (int i = 0; i < g_selection.count; i++) {
        if (g_selection.object_ids[i] == object_id) {
            // Shift remaining items
            for (int j = i; j < g_selection.count - 1; j++) {
                g_selection.object_ids[j] = g_selection.object_ids[j + 1];
            }
            g_selection.count--;
            break;
        }
    }
    
    update_selection_bounds();
    g_stats.selected_objects = g_selection.count;
}

/**
 * Toggles object selection
 * @param object_id Object ID to toggle
 */
void toggle_selection(int object_id)
{
    EditorObject* obj = find_editor_object(object_id);
    if (obj) {
        if (obj->selected) {
            remove_from_selection(object_id);
        } else {
            add_to_selection(object_id);
        }
    }
}

/**
 * Selects all visible, unlocked objects
 */
void select_all(void)
{
    clear_selection();
    
    for (int i = 0; i < g_object_count; i++) {
        EditorObject* obj = &g_editor_objects[i];
        if (obj->visible && !obj->locked && 
            g_layers[obj->layer_id].visible && 
            g_layers[obj->layer_id].selectable) {
            add_to_selection(obj->id);
        }
    }
    
    editor_log(0, "Selected all objects: %d", g_selection.count);
}

/**
 * Selects objects by type
 * @param type Object type to select
 */
void select_by_type(EditorObjectType type)
{
    clear_selection();
    
    for (int i = 0; i < g_object_count; i++) {
        EditorObject* obj = &g_editor_objects[i];
        if (obj->type == type && obj->visible && !obj->locked &&
            g_layers[obj->layer_id].visible && 
            g_layers[obj->layer_id].selectable) {
            add_to_selection(obj->id);
        }
    }
    
    editor_log(0, "Selected %d objects of type %d", g_selection.count, type);
}

/**
 * Selects objects using the same asset
 * @param asset_id Asset ID to match
 */
void select_by_asset(int asset_id)
{
    clear_selection();
    
    for (int i = 0; i < g_object_count; i++) {
        EditorObject* obj = &g_editor_objects[i];
        if (obj->asset_id == asset_id && obj->visible && !obj->locked &&
            g_layers[obj->layer_id].visible && 
            g_layers[obj->layer_id].selectable) {
            add_to_selection(obj->id);
        }
    }
    
    editor_log(0, "Selected %d objects using asset %d", g_selection.count, asset_id);
}

/**
 * Inverts the current selection
 */
void invert_selection(void)
{
    for (int i = 0; i < g_object_count; i++) {
        EditorObject* obj = &g_editor_objects[i];
        if (obj->visible && !obj->locked &&
            g_layers[obj->layer_id].visible && 
            g_layers[obj->layer_id].selectable) {
            toggle_selection(obj->id);
        }
    }
    
    editor_log(0, "Inverted selection: %d objects", g_selection.count);
}

/**
 * Updates selection bounds and center
 */
void update_selection_bounds(void)
{
    if (g_selection.count == 0) {
        memset(g_selection.bounds_min, 0, sizeof(float) * 3);
        memset(g_selection.bounds_max, 0, sizeof(float) * 3);
        memset(g_selection.center, 0, sizeof(float) * 3);
        return;
    }
    
    // Initialize with first object
    EditorObject* first = find_editor_object(g_selection.object_ids[0]);
    if (!first) return;
    
    memcpy(g_selection.bounds_min, first->bounds_min, sizeof(float) * 3);
    memcpy(g_selection.bounds_max, first->bounds_max, sizeof(float) * 3);
    
    // Expand to include all selected objects
    for (int i = 1; i < g_selection.count; i++) {
        EditorObject* obj = find_editor_object(g_selection.object_ids[i]);
        if (obj) {
            for (int j = 0; j < 3; j++) {
                g_selection.bounds_min[j] = fminf(g_selection.bounds_min[j], obj->bounds_min[j]);
                g_selection.bounds_max[j] = fmaxf(g_selection.bounds_max[j], obj->bounds_max[j]);
            }
        }
    }
    
    // Calculate center
    for (int i = 0; i < 3; i++) {
        g_selection.center[i] = (g_selection.bounds_min[i] + g_selection.bounds_max[i]) * 0.5f;
    }
    
    // Update transform pivot
    memcpy(g_transform_pivot, g_selection.center, sizeof(float) * 3);
}

/**
 * Performs box selection
 * @param min_x Minimum X in screen space
 * @param min_y Minimum Y in screen space
 * @param max_x Maximum X in screen space
 * @param max_y Maximum Y in screen space
 * @param additive Whether to add to existing selection
 */
void box_select(float min_x, float min_y, float max_x, float max_y, int additive)
{
    if (!additive) {
        clear_selection();
    }
    
    // TODO: Project objects to screen space and check bounds
    
    editor_log(0, "Box selection: %d objects", g_selection.count);
}

/**
 * Starts lasso selection
 */
void begin_lasso_selection(void)
{
    g_selection.mode = SELECTION_MODE_LASSO;
    g_selection.lasso_point_count = 0;
}

/**
 * Adds a point to lasso selection
 * @param x Screen X coordinate
 * @param y Screen Y coordinate
 */
void add_lasso_point(float x, float y)
{
    if (g_selection.lasso_point_count >= g_selection.lasso_capacity / 2) {
        // Expand array
        int new_capacity = g_selection.lasso_capacity * 2;
        float* new_points = (float*)realloc(g_selection.lasso_points, 
                                           new_capacity * 2 * sizeof(float));
        if (!new_points) {
            return;
        }
        g_selection.lasso_points = new_points;
        g_selection.lasso_capacity = new_capacity;
    }
    
    g_selection.lasso_points[g_selection.lasso_point_count * 2] = x;
    g_selection.lasso_points[g_selection.lasso_point_count * 2 + 1] = y;
    g_selection.lasso_point_count++;
}

/**
 * Completes lasso selection
 * @param additive Whether to add to existing selection
 */
void end_lasso_selection(int additive)
{
    if (g_selection.lasso_point_count < 3) {
        g_selection.mode = SELECTION_MODE_SINGLE;
        return;
    }
    
    if (!additive) {
        clear_selection();
    }
    
    // TODO: Project objects to screen space and check if inside lasso polygon
    
    g_selection.mode = SELECTION_MODE_SINGLE;
    g_selection.lasso_point_count = 0;
    
    editor_log(0, "Lasso selection: %d objects", g_selection.count);
}

// ========================================================================
// TRANSFORMATION FUNCTIONS
// ========================================================================

/**
 * Moves selected objects
 * @param delta_x X movement
 * @param delta_y Y movement
 * @param delta_z Z movement
 * @param relative Whether movement is relative to current position
 */
void move_selected_objects(float delta_x, float delta_y, float delta_z, int relative)
{
    if (g_selection.count == 0) {
        return;
    }
    
    begin_undo_group("Move Objects");
    
    for (int i = 0; i < g_selection.count; i++) {
        EditorObject* obj = find_editor_object(g_selection.object_ids[i]);
        if (obj && !obj->locked) {
            EditorObject old_state = *obj;
            
            if (relative) {
                obj->position[0] += delta_x;
                obj->position[1] += delta_y;
                obj->position[2] += delta_z;
            } else {
                obj->position[0] = delta_x;
                obj->position[1] = delta_y;
                obj->position[2] = delta_z;
            }
            
            // Apply grid snapping
            if (g_config.snap_enabled) {
                obj->position[0] = snap_to_grid(obj->position[0]);
                obj->position[1] = snap_to_grid(obj->position[1]);
                obj->position[2] = snap_to_grid(obj->position[2]);
            }
            
            // Update bounds
            calculate_object_bounds(obj);
            
            add_undo_action(UNDO_TYPE_OBJECT_MODIFY, obj->id, &old_state, obj);
        }
    }
    
    end_undo_group();
    update_selection_bounds();
    
    g_stats.changes_since_save++;
}

/**
 * Rotates selected objects
 * @param delta_x X rotation (degrees)
 * @param delta_y Y rotation (degrees)
 * @param delta_z Z rotation (degrees)
 * @param relative Whether rotation is relative
 */
void rotate_selected_objects(float delta_x, float delta_y, float delta_z, int relative)
{
    if (g_selection.count == 0) {
        return;
    }
    
    begin_undo_group("Rotate Objects");
    
    for (int i = 0; i < g_selection.count; i++) {
        EditorObject* obj = find_editor_object(g_selection.object_ids[i]);
        if (obj && !obj->locked) {
            EditorObject old_state = *obj;
            
            if (relative) {
                obj->rotation[0] += delta_x;
                obj->rotation[1] += delta_y;
                obj->rotation[2] += delta_z;
            } else {
                obj->rotation[0] = delta_x;
                obj->rotation[1] = delta_y;
                obj->rotation[2] = delta_z;
            }
            
            // Apply angle snapping
            if (g_config.snap_enabled) {
                obj->rotation[0] = snap_to_angle(obj->rotation[0]);
                obj->rotation[1] = snap_to_angle(obj->rotation[1]);
                obj->rotation[2] = snap_to_angle(obj->rotation[2]);
            }
            
            // Normalize angles
            for (int j = 0; j < 3; j++) {
                while (obj->rotation[j] < 0) obj->rotation[j] += 360.0f;
                while (obj->rotation[j] >= 360.0f) obj->rotation[j] -= 360.0f;
            }
            
            // Update bounds
            calculate_object_bounds(obj);
            
            add_undo_action(UNDO_TYPE_OBJECT_MODIFY, obj->id, &old_state, obj);
        }
    }
    
    end_undo_group();
    update_selection_bounds();
    
    g_stats.changes_since_save++;
}

/**
 * Scales selected objects
 * @param scale_x X scale factor
 * @param scale_y Y scale factor
 * @param scale_z Z scale factor
 * @param relative Whether scale is relative
 */
void scale_selected_objects(float scale_x, float scale_y, float scale_z, int relative)
{
    if (g_selection.count == 0) {
        return;
    }
    
    begin_undo_group("Scale Objects");
    
    for (int i = 0; i < g_selection.count; i++) {
        EditorObject* obj = find_editor_object(g_selection.object_ids[i]);
        if (obj && !obj->locked) {
            EditorObject old_state = *obj;
            
            if (relative) {
                obj->scale[0] *= scale_x;
                obj->scale[1] *= scale_y;
                obj->scale[2] *= scale_z;
            } else {
                obj->scale[0] = scale_x;
                obj->scale[1] = scale_y;
                obj->scale[2] = scale_z;
            }
            
            // Clamp scale
            for (int j = 0; j < 3; j++) {
                obj->scale[j] = fmaxf(0.01f, fminf(1000.0f, obj->scale[j]));
            }
            
            // Update bounds
            calculate_object_bounds(obj);
            
            add_undo_action(UNDO_TYPE_OBJECT_MODIFY, obj->id, &old_state, obj);
        }
    }
    
    end_undo_group();
    update_selection_bounds();
    
    g_stats.changes_since_save++;
}

/**
 * Aligns selected objects
 * @param axis Axis to align (0=X, 1=Y, 2=Z)
 * @param alignment Alignment type (0=min, 1=center, 2=max)
 */
void align_selected_objects(int axis, int alignment)
{
    if (g_selection.count < 2 || axis < 0 || axis > 2) {
        return;
    }
    
    float target_value;
    
    switch (alignment) {
        case 0:  // Min
            target_value = g_selection.bounds_min[axis];
            break;
        case 1:  // Center
            target_value = g_selection.center[axis];
            break;
        case 2:  // Max
            target_value = g_selection.bounds_max[axis];
            break;
        default:
            return;
    }
    
    begin_undo_group("Align Objects");
    
    for (int i = 0; i < g_selection.count; i++) {
        EditorObject* obj = find_editor_object(g_selection.object_ids[i]);
        if (obj && !obj->locked) {
            EditorObject old_state = *obj;
            
            // Adjust position based on object bounds
            float obj_center = (obj->bounds_min[axis] + obj->bounds_max[axis]) * 0.5f;
            float offset = obj->position[axis] - obj_center;
            
            switch (alignment) {
                case 0:  // Align to min
                    obj->position[axis] = target_value + (obj->position[axis] - obj->bounds_min[axis]);
                    break;
                case 1:  // Align to center
                    obj->position[axis] = target_value + offset;
                    break;
                case 2:  // Align to max
                    obj->position[axis] = target_value - (obj->bounds_max[axis] - obj->position[axis]);
                    break;
            }
            
            // Update bounds
            calculate_object_bounds(obj);
            
            add_undo_action(UNDO_TYPE_OBJECT_MODIFY, obj->id, &old_state, obj);
        }
    }
    
    end_undo_group();
    update_selection_bounds();
    
    editor_log(0, "Aligned %d objects on axis %d", g_selection.count, axis);
}

/**
 * Distributes selected objects evenly
 * @param axis Axis to distribute along (0=X, 1=Y, 2=Z)
 */
void distribute_selected_objects(int axis)
{
    if (g_selection.count < 3 || axis < 0 || axis > 2) {
        return;
    }
    
    // Sort objects by position on axis
    typedef struct {
        int id;
        float pos;
    } SortItem;
    
    SortItem* items = (SortItem*)malloc(g_selection.count * sizeof(SortItem));
    
    for (int i = 0; i < g_selection.count; i++) {
        EditorObject* obj = find_editor_object(g_selection.object_ids[i]);
        items[i].id = g_selection.object_ids[i];
        items[i].pos = obj ? obj->position[axis] : 0.0f;
    }
    
    // Simple bubble sort
    for (int i = 0; i < g_selection.count - 1; i++) {
        for (int j = 0; j < g_selection.count - i - 1; j++) {
            if (items[j].pos > items[j + 1].pos) {
                SortItem temp = items[j];
                items[j] = items[j + 1];
                items[j + 1] = temp;
            }
        }
    }
    
    // Calculate spacing
    float start_pos = items[0].pos;
    float end_pos = items[g_selection.count - 1].pos;
    float spacing = (end_pos - start_pos) / (g_selection.count - 1);
    
    begin_undo_group("Distribute Objects");
    
    // Apply new positions
    for (int i = 1; i < g_selection.count - 1; i++) {
        EditorObject* obj = find_editor_object(items[i].id);
        if (obj && !obj->locked) {
            EditorObject old_state = *obj;
            
            obj->position[axis] = start_pos + spacing * i;
            
            // Update bounds
            calculate_object_bounds(obj);
            
            add_undo_action(UNDO_TYPE_OBJECT_MODIFY, obj->id, &old_state, obj);
        }
    }
    
    end_undo_group();
    update_selection_bounds();
    
    free(items);
    
    editor_log(0, "Distributed %d objects on axis %d", g_selection.count, axis);
}

/**
 * Snaps value to grid
 * @param value Value to snap
 * @return Snapped value
 */
float snap_to_grid(float value)
{
    if (!g_config.snap_enabled) {
        return value;
    }
    return roundf(value / g_config.grid_size) * g_config.grid_size;
}

/**
 * Snaps angle to increment
 * @param angle Angle to snap
 * @return Snapped angle
 */
float snap_to_angle(float angle)
{
    if (!g_config.snap_enabled || g_config.snap_angle <= 0) {
        return angle;
    }
    return roundf(angle / g_config.snap_angle) * g_config.snap_angle;
}

// ========================================================================
// CAMERA SYSTEM
// ========================================================================

/**
 * Updates editor camera
 * @param delta_time Frame delta time
 */
void update_editor_camera(float delta_time)
{
    float speed = g_camera.speed * g_camera.movement_speed * 
                  g_config.camera_speed_multiplier * delta_time;
    
    if (g_camera.free_camera) {
        // Free camera movement
        float yaw_rad = g_camera.rotation[0] * M_PI / 180.0f;
        float pitch_rad = g_camera.rotation[1] * M_PI / 180.0f;
        
        // Calculate movement vectors
        float forward_x = sinf(yaw_rad) * cosf(pitch_rad);
        float forward_y = -sinf(pitch_rad);
        float forward_z = cosf(yaw_rad) * cosf(pitch_rad);
        
        float right_x = sinf(yaw_rad + M_PI * 0.5f);
        float right_z = cosf(yaw_rad + M_PI * 0.5f);
        
        // Apply movement
        if (is_key_pressed('W')) {
            g_camera.position[0] += forward_x * speed;
            g_camera.position[1] += forward_y * speed;
            g_camera.position[2] += forward_z * speed;
        }
        if (is_key_pressed('S')) {
            g_camera.position[0] -= forward_x * speed;
            g_camera.position[1] -= forward_y * speed;
            g_camera.position[2] -= forward_z * speed;
        }
        if (is_key_pressed('A')) {
            g_camera.position[0] -= right_x * speed;
            g_camera.position[2] -= right_z * speed;
        }
        if (is_key_pressed('D')) {
            g_camera.position[0] += right_x * speed;
            g_camera.position[2] += right_z * speed;
        }
        if (is_key_pressed('Q')) {
            g_camera.position[1] -= speed;
        }
        if (is_key_pressed('E')) {
            g_camera.position[1] += speed;
        }
        
        // Speed modifier
        if (is_key_pressed(VK_LSHIFT)) {
            g_camera.movement_speed = 2.0f;
        } else {
            g_camera.movement_speed = 1.0f;
        }
        
        // Update target based on rotation
        g_camera.target[0] = g_camera.position[0] + forward_x * 10.0f;
        g_camera.target[1] = g_camera.position[1] + forward_y * 10.0f;
        g_camera.target[2] = g_camera.position[2] + forward_z * 10.0f;
    } else {
        // Orbit camera
        float yaw_rad = g_camera.rotation[0] * M_PI / 180.0f;
        float pitch_rad = g_camera.rotation[1] * M_PI / 180.0f;
        
        // Clamp pitch
        pitch_rad = fmaxf(-M_PI * 0.49f, fminf(M_PI * 0.49f, pitch_rad));
        g_camera.rotation[1] = pitch_rad * 180.0f / M_PI;
        
        // Calculate camera position from target
        float orbit_x = sinf(yaw_rad) * cosf(pitch_rad);
        float orbit_y = sinf(pitch_rad);
        float orbit_z = cosf(yaw_rad) * cosf(pitch_rad);
        
        g_camera.position[0] = g_camera.target[0] - orbit_x * g_camera.distance;
        g_camera.position[1] = g_camera.target[1] - orbit_y * g_camera.distance;
        g_camera.position[2] = g_camera.target[2] - orbit_z * g_camera.distance;
    }
}

/**
 * Handles mouse input for camera
 * @param dx Mouse X delta
 * @param dy Mouse Y delta
 * @param button Mouse button state
 * @param wheel Mouse wheel delta
 */
void camera_mouse_input(int dx, int dy, int button, int wheel)
{
    const float sensitivity = 0.3f * g_camera.rotation_speed;
    
    if (button & 2) {  // Right mouse button
        // Rotate camera
        g_camera.rotation[0] -= dx * sensitivity;
        g_camera.rotation[1] += dy * sensitivity;
        
        // Wrap yaw
        while (g_camera.rotation[0] < 0) g_camera.rotation[0] += 360.0f;
        while (g_camera.rotation[0] >= 360.0f) g_camera.rotation[0] -= 360.0f;
        
        // Clamp pitch
        g_camera.rotation[1] = fmaxf(-89.0f, fminf(89.0f, g_camera.rotation[1]));
    }
    
    if (button & 4) {  // Middle mouse button
        // Pan camera
        float pan_speed = g_camera.distance * 0.001f * g_camera.movement_speed;
        
        float yaw_rad = g_camera.rotation[0] * M_PI / 180.0f;
        float right_x = sinf(yaw_rad + M_PI * 0.5f);
        float right_z = cosf(yaw_rad + M_PI * 0.5f);
        
        g_camera.target[0] -= right_x * dx * pan_speed;
        g_camera.target[2] -= right_z * dx * pan_speed;
        g_camera.target[1] += dy * pan_speed;
    }
    
    // Zoom with mouse wheel
    if (wheel != 0) {
        if (g_camera.orthographic) {
            g_camera.ortho_size *= (wheel > 0) ? 0.9f : 1.1f;
            g_camera.ortho_size = fmaxf(1.0f, fminf(1000.0f, g_camera.ortho_size));
        } else {
            g_camera.distance *= (wheel > 0) ? 0.9f : 1.1f;
            g_camera.distance = fmaxf(1.0f, fminf(1000.0f, g_camera.distance));
        }
    }
}

/**
 * Focuses camera on selection
 */
void focus_camera_on_selection(void)
{
    if (g_selection.count == 0) {
        return;
    }
    
    // Set target to selection center
    memcpy(g_camera.target, g_selection.center, sizeof(float) * 3);
    
    // Calculate appropriate distance
    float size_x = g_selection.bounds_max[0] - g_selection.bounds_min[0];
    float size_y = g_selection.bounds_max[1] - g_selection.bounds_min[1];
    float size_z = g_selection.bounds_max[2] - g_selection.bounds_min[2];
    float max_size = fmaxf(size_x, fmaxf(size_y, size_z));
    
    // Account for FOV
    float half_fov = g_camera.fov * 0.5f * M_PI / 180.0f;
    g_camera.distance = (max_size * 0.5f) / tanf(half_fov) * 1.5f;
    g_camera.distance = fmaxf(5.0f, g_camera.distance);
    
    if (g_camera.orthographic) {
        g_camera.ortho_size = max_size * 1.5f;
    }
    
    editor_log(0, "Focused camera on selection");
}

/**
 * Saves camera bookmark
 * @param slot Bookmark slot (0-9)
 */
void save_camera_bookmark(int slot)
{
    if (slot < 0 || slot >= 10) {
        return;
    }
    
    g_camera.bookmarks[slot][0] = g_camera.position[0];
    g_camera.bookmarks[slot][1] = g_camera.position[1];
    g_camera.bookmarks[slot][2] = g_camera.position[2];
    g_camera.bookmarks[slot][3] = g_camera.target[0];
    g_camera.bookmarks[slot][4] = g_camera.target[1];
    g_camera.bookmarks[slot][5] = g_camera.target[2];
    g_camera.bookmarks[slot][6] = g_camera.distance;
    
    if (slot >= g_camera.bookmark_count) {
        g_camera.bookmark_count = slot + 1;
    }
    
    editor_log(0, "Saved camera bookmark %d", slot);
}

/**
 * Loads camera bookmark
 * @param slot Bookmark slot (0-9)
 */
void load_camera_bookmark(int slot)
{
    if (slot < 0 || slot >= g_camera.bookmark_count) {
        return;
    }
    
    g_camera.position[0] = g_camera.bookmarks[slot][0];
    g_camera.position[1] = g_camera.bookmarks[slot][1];
    g_camera.position[2] = g_camera.bookmarks[slot][2];
    g_camera.target[0] = g_camera.bookmarks[slot][3];
    g_camera.target[1] = g_camera.bookmarks[slot][4];
    g_camera.target[2] = g_camera.bookmarks[slot][5];
    g_camera.distance = g_camera.bookmarks[slot][6];
    
    // Recalculate rotation from position and target
    float dx = g_camera.target[0] - g_camera.position[0];
    float dy = g_camera.target[1] - g_camera.position[1];
    float dz = g_camera.target[2] - g_camera.position[2];
    
    g_camera.rotation[0] = atan2f(dx, dz) * 180.0f / M_PI;
    g_camera.rotation[1] = atan2f(-dy, sqrtf(dx * dx + dz * dz)) * 180.0f / M_PI;
    
    editor_log(0, "Loaded camera bookmark %d", slot);
}

// ========================================================================
// TERRAIN EDITING
// ========================================================================

/**
 * Modifies terrain height
 * @param world_x World X coordinate
 * @param world_z World Z coordinate
 * @param apply Whether to apply the brush (vs preview)
 */
void modify_terrain_height(float world_x, float world_z, int apply)
{
    // Convert world to terrain coordinates
    int center_x = (int)((world_x / g_terrain.scale) + g_terrain.size_x * 0.5f);
    int center_z = (int)((world_z / g_terrain.scale) + g_terrain.size_z * 0.5f);
    
    if (center_x < 0 || center_x >= g_terrain.size_x || 
        center_z < 0 || center_z >= g_terrain.size_z) {
        return;
    }
    
    // Calculate affected region
    int radius_cells = (int)(g_terrain_brush.radius / g_terrain.scale) + 1;
    int min_x = fmaxf(0, center_x - radius_cells);
    int max_x = fminf(g_terrain.size_x - 1, center_x + radius_cells);
    int min_z = fmaxf(0, center_z - radius_cells);
    int max_z = fminf(g_terrain.size_z - 1, center_z + radius_cells);
    
    if (apply) {
        // Create undo action
        UndoAction* undo = (UndoAction*)calloc(1, sizeof(UndoAction));
        undo->type = UNDO_TYPE_TERRAIN_MODIFY;
        strcpy(undo->description, "Terrain Modification");
        undo->timestamp = time(NULL);
        undo->terrain_region[0] = min_x;
        undo->terrain_region[1] = min_z;
        undo->terrain_region[2] = max_x;
        undo->terrain_region[3] = max_z;
        
        // Backup affected region
        int region_width = max_x - min_x + 1;
        int region_height = max_z - min_z + 1;
        undo->terrain_backup = (float*)malloc(region_width * region_height * sizeof(float));
        
        for (int z = min_z; z <= max_z; z++) {
            for (int x = min_x; x <= max_x; x++) {
                int backup_idx = (z - min_z) * region_width + (x - min_x);
                int terrain_idx = z * g_terrain.size_x + x;
                undo->terrain_backup[backup_idx] = g_terrain.heights[terrain_idx];
            }
        }
        
        add_undo_action_data(undo);
    }
    
    // Apply brush based on type
    switch (g_terrain_brush.type) {
        case TERRAIN_BRUSH_RAISE:
        case TERRAIN_BRUSH_LOWER:
            apply_terrain_brush_height(center_x, center_z, min_x, max_x, min_z, max_z, apply);
            break;
            
        case TERRAIN_BRUSH_SMOOTH:
            apply_terrain_brush_smooth(center_x, center_z, min_x, max_x, min_z, max_z, apply);
            break;
            
        case TERRAIN_BRUSH_FLATTEN:
            apply_terrain_brush_flatten(center_x, center_z, min_x, max_x, min_z, max_z, apply);
            break;
            
        case TERRAIN_BRUSH_NOISE:
            apply_terrain_brush_noise(center_x, center_z, min_x, max_x, min_z, max_z, apply);
            break;
            
        case TERRAIN_BRUSH_EROSION:
            apply_terrain_brush_erosion(center_x, center_z, min_x, max_x, min_z, max_z, apply);
            break;
            
        case TERRAIN_BRUSH_RAMP:
            // TODO: Implement ramp brush
            break;
    }
    
    if (apply) {
        // Update terrain normals
        update_terrain_normals(min_x, max_x, min_z, max_z);
        
        // Mark chunks as dirty
        mark_terrain_chunks_dirty(min_x, max_x, min_z, max_z);
        
        g_stats.changes_since_save++;
    }
}

/**
 * Applies height modification brush
 */
void apply_terrain_brush_height(int center_x, int center_z, 
                               int min_x, int max_x, int min_z, int max_z, int apply)
{
    float sign = (g_terrain_brush.type == TERRAIN_BRUSH_RAISE) ? 1.0f : -1.0f;
    float strength = g_terrain_brush.strength * sign;
    
    for (int z = min_z; z <= max_z; z++) {
        for (int x = min_x; x <= max_x; x++) {
            float dx = (x - center_x) * g_terrain.scale;
            float dz = (z - center_z) * g_terrain.scale;
            float distance = sqrtf(dx * dx + dz * dz);
            
            if (distance <= g_terrain_brush.radius) {
                float influence = 1.0f - (distance / g_terrain_brush.radius);
                
                // Apply falloff
                if (g_terrain_brush.smooth_edges) {
                    influence = powf(influence, g_terrain_brush.falloff);
                }
                
                if (apply) {
                    int idx = z * g_terrain.size_x + x;
                    g_terrain.heights[idx] += strength * influence;
                    
                    // Clamp height
                    g_terrain.heights[idx] = fmaxf(-100.0f, fminf(100.0f, g_terrain.heights[idx]));
                }
            }
        }
    }
}

/**
 * Applies smoothing brush
 */
void apply_terrain_brush_smooth(int center_x, int center_z, 
                               int min_x, int max_x, int min_z, int max_z, int apply)
{
    if (!apply) return;
    
    // Simple erosion simulation
    float* water_flow = (float*)calloc((max_x - min_x + 1) * (max_z - min_z + 1), sizeof(float));
    float* sediment = (float*)calloc((max_x - min_x + 1) * (max_z - min_z + 1), sizeof(float));
    
    // Simulate water flow
    for (int iteration = 0; iteration < 10; iteration++) {
        for (int z = min_z; z <= max_z; z++) {
            for (int x = min_x; x <= max_x; x++) {
                float dx = (x - center_x) * g_terrain.scale;
                float dz = (z - center_z) * g_terrain.scale;
                float distance = sqrtf(dx * dx + dz * dz);
                
                if (distance <= g_terrain_brush.radius) {
                    int local_idx = (z - min_z) * (max_x - min_x + 1) + (x - min_x);
                    int terrain_idx = z * g_terrain.size_x + x;
                    
                    // Calculate gradient
                    float gradient_x = 0.0f;
                    float gradient_z = 0.0f;
                    
                    if (x > 0 && x < g_terrain.size_x - 1) {
                        gradient_x = g_terrain.heights[terrain_idx + 1] - 
                                    g_terrain.heights[terrain_idx - 1];
                    }
                    if (z > 0 && z < g_terrain.size_z - 1) {
                        gradient_z = g_terrain.heights[(z + 1) * g_terrain.size_x + x] - 
                                    g_terrain.heights[(z - 1) * g_terrain.size_x + x];
                    }
                    
                    // Water flows downhill
                    float flow_strength = sqrtf(gradient_x * gradient_x + gradient_z * gradient_z);
                    water_flow[local_idx] += flow_strength * g_terrain_brush.flow_rate;
                    
                    // Erosion
                    float erosion = water_flow[local_idx] * g_terrain_brush.strength * 0.1f;
                    g_terrain.heights[terrain_idx] -= erosion;
                    sediment[local_idx] += erosion;
                    
                    // Deposition
                    float deposition = sediment[local_idx] * g_terrain_brush.deposition_rate;
                    g_terrain.heights[terrain_idx] += deposition;
                    sediment[local_idx] -= deposition;
                }
            }
        }
    }
    
    free(water_flow);
    free(sediment);
}

/**
 * Paints terrain texture
 * @param world_x World X coordinate
 * @param world_z World Z coordinate
 * @param texture_layer Texture layer to paint
 */
void paint_terrain_texture(float world_x, float world_z, int texture_layer)
{
    if (texture_layer < 0 || texture_layer >= 8) {
        return;
    }
    
    // Convert world to terrain coordinates
    int center_x = (int)((world_x / g_terrain.scale) + g_terrain.size_x * 0.5f);
    int center_z = (int)((world_z / g_terrain.scale) + g_terrain.size_z * 0.5f);
    
    if (center_x < 0 || center_x >= g_terrain.size_x || 
        center_z < 0 || center_z >= g_terrain.size_z) {
        return;
    }
    
    // Calculate affected region
    int radius_cells = (int)(g_terrain_brush.radius / g_terrain.scale) + 1;
    int min_x = fmaxf(0, center_x - radius_cells);
    int max_x = fminf(g_terrain.size_x - 1, center_x + radius_cells);
    int min_z = fmaxf(0, center_z - radius_cells);
    int max_z = fminf(g_terrain.size_z - 1, center_z + radius_cells);
    
    for (int z = min_z; z <= max_z; z++) {
        for (int x = min_x; x <= max_x; x++) {
            float dx = (x - center_x) * g_terrain.scale;
            float dz = (z - center_z) * g_terrain.scale;
            float distance = sqrtf(dx * dx + dz * dz);
            
            if (distance <= g_terrain_brush.radius) {
                float influence = 1.0f - (distance / g_terrain_brush.radius);
                influence = powf(influence, g_terrain_brush.falloff) * g_terrain_brush.strength;
                
                int idx = z * g_terrain.size_x + x;
                
                // Update blend weights
                float* weights = &g_terrain.blend_weights[idx * 8];
                
                // Increase target layer
                weights[texture_layer] += influence;
                weights[texture_layer] = fminf(1.0f, weights[texture_layer]);
                
                // Normalize weights
                float total = 0.0f;
                for (int i = 0; i < 8; i++) {
                    total += weights[i];
                }
                
                if (total > 0.0f) {
                    for (int i = 0; i < 8; i++) {
                        weights[i] /= total;
                    }
                }
            }
        }
    }
    
    // Mark chunks as dirty
    mark_terrain_chunks_dirty(min_x, max_x, min_z, max_z);
}

/**
 * Updates terrain normals for a region
 * @param min_x Minimum X coordinate
 * @param max_x Maximum X coordinate
 * @param min_z Minimum Z coordinate
 * @param max_z Maximum Z coordinate
 */
void update_terrain_normals(int min_x, int max_x, int min_z, int max_z)
{
    // Expand region by 1 to include neighbors
    min_x = fmaxf(1, min_x - 1);
    max_x = fminf(g_terrain.size_x - 2, max_x + 1);
    min_z = fmaxf(1, min_z - 1);
    max_z = fminf(g_terrain.size_z - 2, max_z + 1);
    
    for (int z = min_z; z <= max_z; z++) {
        for (int x = min_x; x <= max_x; x++) {
            int idx = z * g_terrain.size_x + x;
            
            // Calculate normal from neighboring heights
            float h_left = g_terrain.heights[idx - 1];
            float h_right = g_terrain.heights[idx + 1];
            float h_down = g_terrain.heights[(z - 1) * g_terrain.size_x + x];
            float h_up = g_terrain.heights[(z + 1) * g_terrain.size_x + x];
            
            // Cross product of tangent vectors
            float dx = 2.0f * g_terrain.scale;
            float dy_x = h_right - h_left;
            float dy_z = h_up - h_down;
            
            // Normal = (dx, dy_x, 0) x (0, dy_z, dx)
            float nx = -dy_x * g_terrain.vertical_scale;
            float ny = dx * g_terrain.vertical_scale;
            float nz = -dy_z * g_terrain.vertical_scale;
            
            // Normalize
            float length = sqrtf(nx * nx + ny * ny + nz * nz);
            if (length > 0.0f) {
                g_terrain.normals[idx * 3] = nx / length;
                g_terrain.normals[idx * 3 + 1] = ny / length;
                g_terrain.normals[idx * 3 + 2] = nz / length;
            } else {
                g_terrain.normals[idx * 3] = 0.0f;
                g_terrain.normals[idx * 3 + 1] = 1.0f;
                g_terrain.normals[idx * 3 + 2] = 0.0f;
            }
        }
    }
}

/**
 * Marks terrain chunks as dirty
 * @param min_x Minimum X coordinate
 * @param max_x Maximum X coordinate
 * @param min_z Minimum Z coordinate
 * @param max_z Maximum Z coordinate
 */
void mark_terrain_chunks_dirty(int min_x, int max_x, int min_z, int max_z)
{
    int chunk_min_x = min_x / TERRAIN_CHUNK_SIZE;
    int chunk_max_x = max_x / TERRAIN_CHUNK_SIZE;
    int chunk_min_z = min_z / TERRAIN_CHUNK_SIZE;
    int chunk_max_z = max_z / TERRAIN_CHUNK_SIZE;
    
    for (int cz = chunk_min_z; cz <= chunk_max_z; cz++) {
        for (int cx = chunk_min_x; cx <= chunk_max_x; cx++) {
            if (cx >= 0 && cx < g_terrain.chunk_count_x &&
                cz >= 0 && cz < g_terrain.chunk_count_z) {
                g_terrain.chunks[cz * g_terrain.chunk_count_x + cx].dirty = 1;
            }
        }
    }
}

/**
 * Loads terrain heightmap from file
 * @param filename Heightmap filename
 * @return 1 on success, 0 on failure
 */
int load_terrain_heightmap(const char* filename)
{
    FILE* file = fopen(filename, "rb");
    if (!file) {
        editor_log(2, "Failed to open heightmap: %s", filename);
        return 0;
    }
    
    // Read header (assuming raw float format)
    int width, height;
    fread(&width, sizeof(int), 1, file);
    fread(&height, sizeof(int), 1, file);
    
    if (width != g_terrain.size_x || height != g_terrain.size_z) {
        editor_log(1, "Heightmap size mismatch: %dx%d vs %dx%d", 
                  width, height, g_terrain.size_x, g_terrain.size_z);
        fclose(file);
        return 0;
    }
    
    // Read height data
    fread(g_terrain.heights, sizeof(float), width * height, file);
    fclose(file);
    
    // Update normals for entire terrain
    update_terrain_normals(0, g_terrain.size_x - 1, 0, g_terrain.size_z - 1);
    
    // Mark all chunks as dirty
    for (int i = 0; i < g_terrain.chunk_count_x * g_terrain.chunk_count_z; i++) {
        g_terrain.chunks[i].dirty = 1;
    }
    
    editor_log(0, "Loaded heightmap: %s", filename);
    return 1;
}

/**
 * Exports terrain heightmap to file
 * @param filename Output filename
 * @return 1 on success, 0 on failure
 */
int export_terrain_heightmap(const char* filename)
{
    FILE* file = fopen(filename, "wb");
    if (!file) {
        editor_log(2, "Failed to create heightmap file: %s", filename);
        return 0;
    }
    
    // Write header
    fwrite(&g_terrain.size_x, sizeof(int), 1, file);
    fwrite(&g_terrain.size_z, sizeof(int), 1, file);
    
    // Write height data
    fwrite(g_terrain.heights, sizeof(float), 
           g_terrain.size_x * g_terrain.size_z, file);
    
    fclose(file);
    
    editor_log(0, "Exported heightmap: %s", filename);
    return 1;
}

/**
 * Resizes terrain
 * @param new_width New terrain width
 * @param new_height New terrain height
 */
void resize_terrain(int new_width, int new_height)
{
    if (new_width == g_terrain.size_x && new_height == g_terrain.size_z) {
        return;
    }
    
    // Allocate new arrays
    int new_size = new_width * new_height;
    float* new_heights = (float*)calloc(new_size, sizeof(float));
    float* new_normals = (float*)calloc(new_size * 3, sizeof(float));
    float* new_blend_weights = (float*)calloc(new_size * 8, sizeof(float));
    
    // Copy existing data (with bilinear sampling for scaling)
    for (int z = 0; z < new_height; z++) {
        for (int x = 0; x < new_width; x++) {
            float src_x = (x * (g_terrain.size_x - 1)) / (float)(new_width - 1);
            float src_z = (z * (g_terrain.size_z - 1)) / (float)(new_height - 1);
            
            int x0 = (int)src_x;
            int z0 = (int)src_z;
            int x1 = fminf(x0 + 1, g_terrain.size_x - 1);
            int z1 = fminf(z0 + 1, g_terrain.size_z - 1);
            
            float fx = src_x - x0;
            float fz = src_z - z0;
            
            // Bilinear interpolation
            float h00 = g_terrain.heights[z0 * g_terrain.size_x + x0];
            float h10 = g_terrain.heights[z0 * g_terrain.size_x + x1];
            float h01 = g_terrain.heights[z1 * g_terrain.size_x + x0];
            float h11 = g_terrain.heights[z1 * g_terrain.size_x + x1];
            
            float h0 = h00 * (1 - fx) + h10 * fx;
            float h1 = h01 * (1 - fx) + h11 * fx;
            float h = h0 * (1 - fz) + h1 * fz;
            
            new_heights[z * new_width + x] = h;
            
            // Copy blend weights
            for (int i = 0; i < 8; i++) {
                new_blend_weights[(z * new_width + x) * 8 + i] = 
                    g_terrain.blend_weights[(z0 * g_terrain.size_x + x0) * 8 + i];
            }
        }
    }
    
    // Free old arrays
    free(g_terrain.heights);
    free(g_terrain.normals);
    free(g_terrain.blend_weights);
    free(g_terrain.vegetation_map);
    free(g_terrain.water_map);
    free(g_terrain.chunks);
    
    // Update terrain data
    g_terrain.size_x = new_width;
    g_terrain.size_z = new_height;
    g_terrain.heights = new_heights;
    g_terrain.normals = new_normals;
    g_terrain.blend_weights = new_blend_weights;
    g_terrain.vegetation_map = (float*)calloc(new_size, sizeof(float));
    g_terrain.water_map = (float*)calloc(new_size, sizeof(float));
    
    // Recreate chunks
    g_terrain.chunk_count_x = (new_width + TERRAIN_CHUNK_SIZE - 1) / TERRAIN_CHUNK_SIZE;
    g_terrain.chunk_count_z = (new_height + TERRAIN_CHUNK_SIZE - 1) / TERRAIN_CHUNK_SIZE;
    g_terrain.chunks = (TerrainChunk*)calloc(g_terrain.chunk_count_x * g_terrain.chunk_count_z, 
                                             sizeof(TerrainChunk));
    
    // Update normals
    update_terrain_normals(0, new_width - 1, 0, new_height - 1);
    
    editor_log(0, "Resized terrain to %dx%d", new_width, new_height);
}

// ========================================================================
// LAYER SYSTEM
// ========================================================================

/**
 * Creates a new layer
 * @param name Layer name
 * @param parent_id Parent layer ID (-1 for root)
 * @return Layer ID or -1 on failure
 */
int create_layer(const char* name, int parent_id)
{
    if (g_layer_count >= MAX_LAYERS) {
        editor_log(2, "Maximum layer count reached");
        return -1;
    }
    
    EditorLayer* layer = &g_layers[g_layer_count];
    strncpy(layer->name, name, sizeof(layer->name) - 1);
    layer->visible = 1;
    layer->locked = 0;
    layer->selectable = 1;
    layer->color[0] = 0.5f + (rand() % 50) / 100.0f;
    layer->color[1] = 0.5f + (rand() % 50) / 100.0f;
    layer->color[2] = 0.5f + (rand() % 50) / 100.0f;
    layer->color[3] = 1.0f;
    layer->object_count = 0;
    layer->parent_layer = parent_id;
    
    editor_log(0, "Created layer: %s", name);
    return g_layer_count++;
}

/**
 * Deletes a layer
 * @param layer_id Layer ID to delete
 */
void delete_layer(int layer_id)
{
    if (layer_id <= 0 || layer_id >= g_layer_count) {
        return;  // Can't delete default layer
    }
    
    // Move all objects to default layer
    for (int i = 0; i < g_object_count; i++) {
        if (g_editor_objects[i].layer_id == layer_id) {
            g_editor_objects[i].layer_id = 0;
        }
    }
    
    // Update child layers
    for (int i = 0; i < g_layer_count; i++) {
        if (g_layers[i].parent_layer == layer_id) {
            g_layers[i].parent_layer = -1;
        }
    }
    
    editor_log(0, "Deleted layer: %s", g_layers[layer_id].name);
    
    // Shift layers down
    for (int i = layer_id; i < g_layer_count - 1; i++) {
        g_layers[i] = g_layers[i + 1];
    }
    g_layer_count--;
    
    // Update object layer references
    for (int i = 0; i < g_object_count; i++) {
        if (g_editor_objects[i].layer_id > layer_id) {
            g_editor_objects[i].layer_id--;
        }
    }
}

/**
 * Sets layer visibility
 * @param layer_id Layer ID
 * @param visible Visibility state
 */
void set_layer_visibility(int layer_id, int visible)
{
    if (layer_id < 0 || layer_id >= g_layer_count) {
        return;
    }
    
    g_layers[layer_id].visible = visible;
    
    // Update visible objects
    update_visible_objects();
    
    editor_log(0, "Set layer %s visibility to %d", 
              g_layers[layer_id].name, visible);
}

/**
 * Moves objects to a layer
 * @param object_ids Array of object IDs
 * @param count Number of objects
 * @param target_layer Target layer ID
 */
void move_objects_to_layer(int* object_ids, int count, int target_layer)
{
    if (target_layer < 0 || target_layer >= g_layer_count) {
        return;
    }
    
    begin_undo_group("Move Objects to Layer");
    
    for (int i = 0; i < count; i++) {
        EditorObject* obj = find_editor_object(object_ids[i]);
        if (obj) {
            EditorObject old_state = *obj;
            
            // Update layer counts
            if (obj->layer_id < g_layer_count) {
                g_layers[obj->layer_id].object_count--;
            }
            g_layers[target_layer].object_count++;
            
            obj->layer_id = target_layer;
            
            add_undo_action(UNDO_TYPE_OBJECT_MODIFY, obj->id, &old_state, obj);
        }
    }
    
    end_undo_group();
    
    editor_log(0, "Moved %d objects to layer %s", count, g_layers[target_layer].name);
}

// ========================================================================
// PREFAB SYSTEM
// ========================================================================

/**
 * Creates a prefab from selection
 * @param name Prefab name
 * @param description Prefab description
 * @return Prefab ID or -1 on failure
 */
int create_prefab_from_selection(const char* name, const char* description)
{
    if (g_selection.count == 0) {
        editor_log(1, "No objects selected for prefab creation");
        return -1;
    }
    
    if (g_prefab_count >= MAX_PREFABS) {
        editor_log(2, "Maximum prefab count reached");
        return -1;
    }
    
    Prefab* prefab = &g_prefabs[g_prefab_count];
    strncpy(prefab->name, name, sizeof(prefab->name) - 1);
    strncpy(prefab->description, description, sizeof(prefab->description) - 1);
    
    // Copy selected object IDs
    prefab->object_count = fminf(g_selection.count, MAX_SELECTION);
    for (int i = 0; i < prefab->object_count; i++) {
        prefab->object_ids[i] = g_selection.object_ids[i];
    }
    
    // Calculate bounds
    memcpy(prefab->bounds_min, g_selection.bounds_min, sizeof(float) * 3);
    memcpy(prefab->bounds_max, g_selection.bounds_max, sizeof(float) * 3);
    
    // Set metadata
    prefab->creation_date = time(NULL);
    strncpy(prefab->author, "Editor User", sizeof(prefab->author) - 1);
    sprintf(prefab->tags, "objects:%d", prefab->object_count);
    
    // Generate thumbnail path
    sprintf(prefab->thumbnail_path, "thumbnails/prefab_%d.bmp", g_prefab_count);
    
    // TODO: Generate prefab thumbnail
    
    editor_log(0, "Created prefab: %s with %d objects", name, prefab->object_count);
    return g_prefab_count++;
}

/**
 * Instantiates a prefab
 * @param prefab_id Prefab ID
 * @param x X position
 * @param y Y position
 * @param z Z position
 * @return Root object ID or -1 on failure
 */
int instantiate_prefab(int prefab_id, float x, float y, float z)
{
    if (prefab_id < 0 || prefab_id >= g_prefab_count) {
        return -1;
    }
    
    Prefab* prefab = &g_prefabs[prefab_id];
    
    // Calculate offset from prefab center
    float prefab_center[3] = {
        (prefab->bounds_min[0] + prefab->bounds_max[0]) * 0.5f,
        (prefab->bounds_min[1] + prefab->bounds_max[1]) * 0.5f,
        (prefab->bounds_min[2] + prefab->bounds_max[2]) * 0.5f
    };
    
    float offset[3] = {
        x - prefab_center[0],
        y - prefab_center[1],
        z - prefab_center[2]
    };
    
    // Clear selection
    clear_selection();
    
    begin_undo_group("Instantiate Prefab");
    
    // Create prefab instance parent
    int parent_id = create_editor_object(EDITOR_OBJ_PREFAB_INSTANCE, x, y, z);
    if (parent_id < 0) {
        end_undo_group();
        return -1;
    }
    
    EditorObject* parent = &g_editor_objects[parent_id - 1];
    sprintf(parent->name, "%s_Instance", prefab->name);
    parent->properties.prefab.prefab_id = prefab_id;
    parent->properties.prefab.instance_seed = (float)rand() / RAND_MAX;
    
    // Duplicate all prefab objects
    for (int i = 0; i < prefab->object_count; i++) {
        EditorObject* template_obj = find_editor_object(prefab->object_ids[i]);
        if (!template_obj) continue;
        
        int new_id = create_editor_object(template_obj->type,
                                         template_obj->position[0] + offset[0],
                                         template_obj->position[1] + offset[1],
                                         template_obj->position[2] + offset[2]);
        
        if (new_id >= 0) {
            EditorObject* new_obj = &g_editor_objects[new_id - 1];
            
            // Copy properties
            memcpy(new_obj->rotation, template_obj->rotation, sizeof(float) * 3);
            memcpy(new_obj->scale, template_obj->scale, sizeof(float) * 3);
            new_obj->asset_id = template_obj->asset_id;
            new_obj->material_id = template_obj->material_id;
            new_obj->parent_id = parent_id;
            
            // Update asset reference count
            if (new_obj->asset_id >= 0) {
                g_asset_cache[new_obj->asset_id].reference_count++;
            }
            
            add_to_selection(new_id);
        }
    }
    
    end_undo_group();
    
    editor_log(0, "Instantiated prefab: %s at (%.2f, %.2f, %.2f)", 
              prefab->name, x, y, z);
    
    return parent_id;
}

// ========================================================================
// UNDO/REDO SYSTEM
// ========================================================================

/**
 * Begins an undo group
 * @param description Group description
 */
void begin_undo_group(const char* description)
{
    g_undo_group_depth++;
    
    if (g_undo_group_depth == 1) {
        // Create group action
        UndoAction* group = (UndoAction*)calloc(1, sizeof(UndoAction));
        group->type = UNDO_TYPE_BATCH;
        strncpy(group->description, description, sizeof(group->description) - 1);
        group->timestamp = time(NULL);
        
        add_undo_action_data(group);
    }
}

/**
 * Ends an undo group
 */
void end_undo_group(void)
{
    g_undo_group_depth--;
    if (g_undo_group_depth < 0) {
        g_undo_group_depth = 0;
    }
}

/**
 * Adds an undo action
 * @param type Action type
 * @param object_id Object ID
 * @param old_state Old object state
 * @param new_state New object state
 */
void add_undo_action(UndoType type, int object_id, EditorObject* old_state, EditorObject* new_state)
{
    UndoAction* action = (UndoAction*)calloc(1, sizeof(UndoAction));
    action->type = type;
    action->timestamp = time(NULL);
    
    // Set description based on type
    switch (type) {
        case UNDO_TYPE_OBJECT_CREATE:
            sprintf(action->description, "Create %s", new_state ? new_state->name : "Object");
            break;
        case UNDO_TYPE_OBJECT_DELETE:
            sprintf(action->description, "Delete %s", old_state ? old_state->name : "Object");
            break;
        case UNDO_TYPE_OBJECT_MODIFY:
            sprintf(action->description, "Modify %s", old_state ? old_state->name : "Object");
            break;
        default:
            strcpy(action->description, "Unknown Action");
            break;
    }
    
    // Store object data
    action->object_count = 1;
    action->object_ids = (int*)malloc(sizeof(int));
    action->object_ids[0] = object_id;
    
    if (old_state) {
        action->old_states = (EditorObject*)malloc(sizeof(EditorObject));
        memcpy(action->old_states, old_state, sizeof(EditorObject));
    }
    
    if (new_state) {
        action->new_states = (EditorObject*)malloc(sizeof(EditorObject));
        memcpy(action->new_states, new_state, sizeof(EditorObject));
    }
    
    add_undo_action_data(action);
}

/**
 * Adds undo action data to stack
 * @param action Action to add
 */
void add_undo_action_data(UndoAction* action)
{
    // Remove any redo actions
    while (g_undo_count > g_undo_head) {
        g_undo_count--;
        int idx = (g_undo_tail + g_undo_count) % g_undo_capacity;
        free_undo_action(&g_undo_stack[idx]);
    }
    
    // Check capacity
    if (g_undo_count >= g_config.max_undo_steps) {
        // Remove oldest action
        free_undo_action(&g_undo_stack[g_undo_tail]);
        g_undo_tail = (g_undo_tail + 1) % g_undo_capacity;
        g_undo_count--;
    }
    
    // Add new action
    int idx = (g_undo_tail + g_undo_head) % g_undo_capacity;
    
    // If we're in a group, add as sub-action
    if (g_undo_group_depth > 0 && g_undo_head > 0) {
        int group_idx = (g_undo_tail + g_undo_head - 1) % g_undo_capacity;
        UndoAction* group = &g_undo_stack[group_idx];
        
        if (group->type == UNDO_TYPE_BATCH) {
            // Add as sub-action
            group->sub_action_count++;
            group->sub_actions = (UndoAction*)realloc(group->sub_actions, 
                                                      group->sub_action_count * sizeof(UndoAction));
            memcpy(&group->sub_actions[group->sub_action_count - 1], action, sizeof(UndoAction));
            free(action);
            return;
        }
    }
    
    // Add as top-level action
    memcpy(&g_undo_stack[idx], action, sizeof(UndoAction));
    free(action);
    
    g_undo_head++;
    g_undo_count++;
}

/**
 * Performs undo
 */
void undo(void)
{
    if (g_undo_head == 0) {
        editor_log(1, "Nothing to undo");
        return;
    }
    
    g_undo_head--;
    int idx = (g_undo_tail + g_undo_head) % g_undo_capacity;
    UndoAction* action = &g_undo_stack[idx];
    
    editor_log(0, "Undo: %s", action->description);
    apply_undo_action(action, 1);
    
    g_stats.changes_since_save++;
}

/**
 * Performs redo
 */
void redo(void)
{
    if (g_undo_head >= g_undo_count) {
        editor_log(1, "Nothing to redo");
        return;
    }
    
    int idx = (g_undo_tail + g_undo_head) % g_undo_capacity;
    UndoAction* action = &g_undo_stack[idx];
    
    editor_log(0, "Redo: %s", action->description);
    apply_undo_action(action, 0);
    
    g_undo_head++;
    g_stats.changes_since_save++;
}

/**
 * Applies an undo action
 * @param action Action to apply
 * @param is_undo Whether this is an undo (vs redo)
 */
void apply_undo_action(UndoAction* action, int is_undo)
{
    switch (action->type) {
        case UNDO_TYPE_OBJECT_CREATE:
            if (is_undo) {
                // Delete the created object
                delete_editor_object(action->object_ids[0]);
            } else {
                // Recreate the object
                if (action->new_states) {
                    // Find a free slot
                    if (g_object_count < g_object_capacity) {
                        memcpy(&g_editor_objects[g_object_count], 
                               action->new_states, sizeof(EditorObject));
                        g_object_count++;
                    }
                }
            }
            break;
            
        case UNDO_TYPE_OBJECT_DELETE:
            if (is_undo) {
                // Recreate the deleted object
                if (action->old_states && g_object_count < g_object_capacity) {
                    memcpy(&g_editor_objects[g_object_count], 
                           action->old_states, sizeof(EditorObject));
                    g_object_count++;
                }
            } else {
                // Delete the object again
                delete_editor_object(action->object_ids[0]);
            }
            break;
            
        case UNDO_TYPE_OBJECT_MODIFY:
            {
                EditorObject* obj = find_editor_object(action->object_ids[0]);
                if (obj) {
                    EditorObject temp = *obj;
                    if (is_undo && action->old_states) {
                        *obj = *action->old_states;
                    } else if (!is_undo && action->new_states) {
                        *obj = *action->new_states;
                    }
                }
            }
            break;
            
        case UNDO_TYPE_TERRAIN_MODIFY:
            if (action->terrain_backup) {
                // Swap terrain data
                int x1 = action->terrain_region[0];
                int z1 = action->terrain_region[1];
                int x2 = action->terrain_region[2];
                int z2 = action->terrain_region[3];
                int region_width = x2 - x1 + 1;
                
                for (int z = z1; z <= z2; z++) {
                    for (int x = x1; x <= x2; x++) {
                        int backup_idx = (z - z1) * region_width + (x - x1);
                        int terrain_idx = z * g_terrain.size_x + x;
                        
                        float temp = g_terrain.heights[terrain_idx];
                        g_terrain.heights[terrain_idx] = action->terrain_backup[backup_idx];
                        action->terrain_backup[backup_idx] = temp;
                    }
                }
                
                // Update normals and chunks
                update_terrain_normals(x1, x2, z1, z2);
                mark_terrain_chunks_dirty(x1, x2, z1, z2);
            }
            break;
            
        case UNDO_TYPE_BATCH:
            // Apply sub-actions in reverse order for undo
            if (is_undo) {
                for (int i = action->sub_action_count - 1; i >= 0; i--) {
                    apply_undo_action(&action->sub_actions[i], is_undo);
                }
            } else {
                for (int i = 0; i < action->sub_action_count; i++) {
                    apply_undo_action(&action->sub_actions[i], is_undo);
                }
            }
            break;
    }
    
    // Update selection bounds if needed
    if (action->type != UNDO_TYPE_TERRAIN_MODIFY) {
        update_selection_bounds();
    }
}

/**
 * Frees undo action memory
 * @param action Action to free
 */
void free_undo_action(UndoAction* action)
{
    if (action->object_ids) {
        free(action->object_ids);
    }
    if (action->old_states) {
        free(action->old_states);
    }
    if (action->new_states) {
        free(action->new_states);
    }
    if (action->terrain_backup) {
        free(action->terrain_backup);
    }
    if (action->sub_actions) {
        for (int i = 0; i < action->sub_action_count; i++) {
            free_undo_action(&action->sub_actions[i]);
        }
        free(action->sub_actions);
    }
}

// ========================================================================
// FILE I/O
// ========================================================================

/**
 * Saves level to file
 * @param filename Output filename
 * @return 1 on success, 0 on failure
 */
int save_level(const char* filename)
{
    FILE* file = fopen(filename, "w");
    if (!file) {
        editor_log(2, "Failed to create level file: %s", filename);
        return 0;
    }
    
    editor_log(0, "Saving level: %s", filename);
    
    // Write header
    fprintf(file, "[ENDOR_LEVEL]\n");
    fprintf(file, "version=2.0\n");
    fprintf(file, "name=Level\n");
    fprintf(file, "author=Editor User\n");
    fprintf(file, "date=%s\n", get_timestamp());
    fprintf(file, "\n");
    
    // Write terrain data
    save_terrain_data(file);
    
    // Write layers
    fprintf(file, "[LAYERS]\n");
    fprintf(file, "count=%d\n", g_layer_count);
    for (int i = 0; i < g_layer_count; i++) {
        fprintf(file, "\n[LAYER_%d]\n", i);
        fprintf(file, "name=%s\n", g_layers[i].name);
        fprintf(file, "visible=%d\n", g_layers[i].visible);
        fprintf(file, "locked=%d\n", g_layers[i].locked);
        fprintf(file, "selectable=%d\n", g_layers[i].selectable);
        fprintf(file, "color=%.3f,%.3f,%.3f,%.3f\n", 
                g_layers[i].color[0], g_layers[i].color[1], 
                g_layers[i].color[2], g_layers[i].color[3]);
        fprintf(file, "parent=%d\n", g_layers[i].parent_layer);
    }
    fprintf(file, "\n");
    
    // Write objects
    save_object_data(file);
    
    // Write camera settings
    fprintf(file, "[CAMERA]\n");
    fprintf(file, "position=%.3f,%.3f,%.3f\n", 
            g_camera.position[0], g_camera.position[1], g_camera.position[2]);
    fprintf(file, "target=%.3f,%.3f,%.3f\n", 
            g_camera.target[0], g_camera.target[1], g_camera.target[2]);
    fprintf(file, "rotation=%.3f,%.3f\n", g_camera.rotation[0], g_camera.rotation[1]);
    fprintf(file, "distance=%.3f\n", g_camera.distance);
    fprintf(file, "fov=%.3f\n", g_camera.fov);
    fprintf(file, "\n");
    
    fclose(file);
    
    // Save current filename
    strncpy(g_current_level_file, filename, sizeof(g_current_level_file) - 1);
    g_stats.changes_since_save = 0;
    
    editor_log(0, "Level saved successfully");
    return 1;
}

/**
 * Saves terrain data to file
 * @param file Output file
 */
void save_terrain_data(FILE* file)
{
    fprintf(file, "[TERRAIN]\n");
    fprintf(file, "size=%d,%d\n", g_terrain.size_x, g_terrain.size_z);
    fprintf(file, "scale=%.3f\n", g_terrain.scale);
    fprintf(file, "vertical_scale=%.3f\n", g_terrain.vertical_scale);
    fprintf(file, "detail_texture=%d\n", g_terrain.detail_texture_id);
    fprintf(file, "detail_scale=%.3f\n", g_terrain.detail_scale);
    fprintf(file, "\n");
    
    // Write texture layers
    fprintf(file, "[TERRAIN_TEXTURES]\n");
    for (int i = 0; i < 8; i++) {
        if (g_terrain.texture_ids[i] >= 0) {
            fprintf(file, "texture_%d=%d,%.3f\n", i, 
                    g_terrain.texture_ids[i], g_terrain.texture_scales[i]);
        }
    }
    fprintf(file, "\n");
    
    // Write heightmap data (compressed)
    fprintf(file, "[TERRAIN_HEIGHTS]\n");
    fprintf(file, "compression=RLE\n");
    
    // Simple RLE compression
    int count = 0;
    float last_height = g_terrain.heights[0];
    
    for (int i = 0; i < g_terrain.size_x * g_terrain.size_z; i++) {
        if (fabsf(g_terrain.heights[i] - last_height) < 0.001f && count < 255) {
            count++;
        } else {
            fprintf(file, "%d,%.3f,", count, last_height);
            last_height = g_terrain.heights[i];
            count = 1;
        }
    }
    fprintf(file, "%d,%.3f\n", count, last_height);
    fprintf(file, "\n");
}

/**
 * Saves object data to file
 * @param file Output file
 */
void save_object_data(FILE* file)
{
    fprintf(file, "[OBJECTS]\n");
    fprintf(file, "count=%d\n", g_object_count);
    fprintf(file, "\n");
    
    for (int i = 0; i < g_object_count; i++) {
        EditorObject* obj = &g_editor_objects[i];
        
        fprintf(file, "[OBJECT_%d]\n", obj->id);
        fprintf(file, "type=%d\n", obj->type);
        fprintf(file, "name=%s\n", obj->name);
        fprintf(file, "position=%.3f,%.3f,%.3f\n", 
                obj->position[0], obj->position[1], obj->position[2]);
        fprintf(file, "rotation=%.3f,%.3f,%.3f\n", 
                obj->rotation[0], obj->rotation[1], obj->rotation[2]);
        fprintf(file, "scale=%.3f,%.3f,%.3f\n", 
                obj->scale[0], obj->scale[1], obj->scale[2]);
        fprintf(file, "layer=%d\n", obj->layer_id);
        fprintf(file, "parent=%d\n", obj->parent_id);
        fprintf(file, "visible=%d\n", obj->visible);
        fprintf(file, "locked=%d\n", obj->locked);
        fprintf(file, "static=%d\n", obj->static_object);
        
        if (obj->asset_id >= 0) {
            fprintf(file, "asset=%s\n", g_asset_cache[obj->asset_id].filename);
        }
        if (obj->material_id >= 0) {
            fprintf(file, "material=%d\n", obj->material_id);
        }
        
        // Save type-specific properties
        save_object_properties(file, obj);
        
        fprintf(file, "\n");
    }
}

/**
 * Saves object type-specific properties
 * @param file Output file
 * @param obj Object to save
 */
void save_object_properties(FILE* file, EditorObject* obj)
{
    switch (obj->type) {
        case EDITOR_OBJ_LIGHT:
            fprintf(file, "light_type=%d\n", obj->properties.light.light_type);
            fprintf(file, "light_color=%.3f,%.3f,%.3f\n", 
                    obj->properties.light.color[0], 
                    obj->properties.light.color[1], 
                    obj->properties.light.color[2]);
            fprintf(file, "light_intensity=%.3f\n", obj->properties.light.intensity);
            fprintf(file, "light_range=%.3f\n", obj->properties.light.range);
            fprintf(file, "cast_shadows=%d\n", obj->properties.light.cast_shadows);
            fprintf(file, "shadow_resolution=%d\n", obj->properties.light.shadow_resolution);
            break;
            
        case EDITOR_OBJ_SPAWN_POINT:
            fprintf(file, "spawn_team=%d\n", obj->properties.spawn.team);
            fprintf(file, "spawn_type=%d\n", obj->properties.spawn.spawn_type);
            fprintf(file, "spawn_radius=%.3f\n", obj->properties.spawn.spawn_radius);
            fprintf(file, "spawn_priority=%d\n", obj->properties.spawn.priority);
            break;
            
        case EDITOR_OBJ_TRIGGER:
            fprintf(file, "trigger_bounds=%.3f,%.3f,%.3f,%.3f,%.3f,%.3f\n",
                    obj->properties.trigger.bounds[0], obj->properties.trigger.bounds[1],
                    obj->properties.trigger.bounds[2], obj->properties.trigger.bounds[3],
                    obj->properties.trigger.bounds[4], obj->properties.trigger.bounds[5]);
            fprintf(file, "trigger_script=%s\n", obj->properties.trigger.script);
            fprintf(file, "trigger_once=%d\n", obj->properties.trigger.trigger_once);
            fprintf(file, "trigger_delay=%d\n", obj->properties.trigger.trigger_delay);
            break;
            
        // Add other object types as needed
    }
}

/**
 * Loads level from file
 * @param filename Input filename
 * @return 1 on success, 0 on failure
 */
int load_level(const char* filename)
{
    FILE* file = fopen(filename, "r");
    if (!file) {
        editor_log(2, "Failed to open level file: %s", filename);
        return 0;
    }
    
    editor_log(0, "Loading level: %s", filename);
    
    // Clear existing data
    clear_selection();
    g_object_count = 0;
    g_layer_count = 1;  // Keep default layer
    
    char line[1024];
    char section[64] = "";
    int current_object_id = -1;
    EditorObject* current_object = NULL;
    
    while (fgets(line, sizeof(line), file)) {
        // Remove newline
        char* newline = strchr(line, '\n');
        if (newline) *newline = '\0';
        
        // Skip empty lines and comments
        if (line[0] == '\0' || line[0] == '#' || line[0] == ';') {
            continue;
        }
        
        // Check for section header
        if (line[0] == '[') {
            char* end = strchr(line, ']');
            if (end) {
                *end = '\0';
                strncpy(section, line + 1, sizeof(section) - 1);
                
                // Check if it's an object section
                if (strncmp(section, "OBJECT_", 7) == 0) {
                    current_object_id = atoi(section + 7);
                    // Create new object
                    if (g_object_count < g_object_capacity) {
                        current_object = &g_editor_objects[g_object_count++];
                        memset(current_object, 0, sizeof(EditorObject));
                        current_object->id = current_object_id;
                        current_object->scale[0] = current_object->scale[1] = current_object->scale[2] = 1.0f;
                        current_object->visible = 1;
                        current_object->asset_id = -1;
                        current_object->material_id = -1;
                        current_object->parent_id = -1;
                    }
                } else {
                    current_object = NULL;
                }
            }
            continue;
        }
        
        // Parse key=value
        char* equals = strchr(line, '=');
        if (!equals) continue;
        
        *equals = '\0';
        char* key = line;
        char* value = equals + 1;
        
        // Process based on section
        if (strcmp(section, "TERRAIN") == 0) {
            load_terrain_property(key, value);
        } else if (current_object != NULL) {
            load_object_property(current_object, key, value);
        }
        // Add other sections as needed
    }
    
    fclose(file);
    
    // Update terrain normals
    update_terrain_normals(0, g_terrain.size_x - 1, 0, g_terrain.size_z - 1);
    
    // Update statistics
    g_stats.total_objects = g_object_count;
    g_stats.changes_since_save = 0;
    
    // Save current filename
    strncpy(g_current_level_file, filename, sizeof(g_current_level_file) - 1);
    
    editor_log(0, "Level loaded successfully");
    return 1;
}

/**
 * Loads terrain property from file
 * @param key Property key
 * @param value Property value
 */
void load_terrain_property(const char* key, const char* value)
{
    if (strcmp(key, "size") == 0) {
        int width, height;
        if (sscanf(value, "%d,%d", &width, &height) == 2) {
            resize_terrain(width, height);
        }
    } else if (strcmp(key, "scale") == 0) {
        g_terrain.scale = atof(value);
    } else if (strcmp(key, "vertical_scale") == 0) {
        g_terrain.vertical_scale = atof(value);
    }
    // Add other terrain properties
}

/**
 * Loads object property from file
 * @param obj Object to load into
 * @param key Property key
 * @param value Property value
 */
void load_object_property(EditorObject* obj, const char* key, const char* value)
{
    if (strcmp(key, "type") == 0) {
        obj->type = atoi(value);
    } else if (strcmp(key, "name") == 0) {
        strncpy(obj->name, value, sizeof(obj->name) - 1);
    } else if (strcmp(key, "position") == 0) {
        sscanf(value, "%f,%f,%f", &obj->position[0], &obj->position[1], &obj->position[2]);
    } else if (strcmp(key, "rotation") == 0) {
        sscanf(value, "%f,%f,%f", &obj->rotation[0], &obj->rotation[1], &obj->rotation[2]);
    } else if (strcmp(key, "scale") == 0) {
        sscanf(value, "%f,%f,%f", &obj->scale[0], &obj->scale[1], &obj->scale[2]);
    } else if (strcmp(key, "layer") == 0) {
        obj->layer_id = atoi(value);
    } else if (strcmp(key, "parent") == 0) {
        obj->parent_id = atoi(value);
    } else if (strcmp(key, "visible") == 0) {
        obj->visible = atoi(value);
    } else if (strcmp(key, "locked") == 0) {
        obj->locked = atoi(value);
    } else if (strcmp(key, "static") == 0) {
        obj->static_object = atoi(value);
    } else if (strcmp(key, "asset") == 0) {
        // Find or load asset
        for (int i = 0; i < g_asset_count; i++) {
            if (strcmp(g_asset_cache[i].filename, value) == 0) {
                obj->asset_id = i;
                g_asset_cache[i].reference_count++;
                break;
            }
        }
    } else {
        // Type-specific properties
        load_object_type_property(obj, key, value);
    }
    
    // Update bounds
    calculate_object_bounds(obj);
}

/**
 * Loads object type-specific property
 * @param obj Object to load into
 * @param key Property key
 * @param value Property value
 */
void load_object_type_property(EditorObject* obj, const char* key, const char* value)
{
    switch (obj->type) {
        case EDITOR_OBJ_LIGHT:
            if (strcmp(key, "light_type") == 0) {
                obj->properties.light.light_type = atoi(value);
            } else if (strcmp(key, "light_color") == 0) {
                sscanf(value, "%f,%f,%f", 
                       &obj->properties.light.color[0],
                       &obj->properties.light.color[1],
                       &obj->properties.light.color[2]);
            } else if (strcmp(key, "light_intensity") == 0) {
                obj->properties.light.intensity = atof(value);
            } else if (strcmp(key, "light_range") == 0) {
                obj->properties.light.range = atof(value);
            } else if (strcmp(key, "cast_shadows") == 0) {
                obj->properties.light.cast_shadows = atoi(value);
            } else if (strcmp(key, "shadow_resolution") == 0) {
                obj->properties.light.shadow_resolution = atoi(value);
            }
            break;
            
        case EDITOR_OBJ_SPAWN_POINT:
            if (strcmp(key, "spawn_team") == 0) {
                obj->properties.spawn.team = atoi(value);
            } else if (strcmp(key, "spawn_type") == 0) {
                obj->properties.spawn.spawn_type = atoi(value);
            } else if (strcmp(key, "spawn_radius") == 0) {
                obj->properties.spawn.spawn_radius = atof(value);
            } else if (strcmp(key, "spawn_priority") == 0) {
                obj->properties.spawn.priority = atoi(value);
            }
            break;
            
        // Add other object types
    }
}

/**
 * Checks and performs autosave
 */
void check_autosave(void)
{
    if (!g_config.autosave_enabled || g_stats.changes_since_save == 0) {
        return;
    }
    
    time_t now = time(NULL);
    if (now - g_last_autosave >= g_config.autosave_interval / 1000) {
        char autosave_file[512];
        if (strlen(g_current_level_file) > 0) {
            // Create autosave based on current file
            char* dot = strrchr(g_current_level_file, '.');
            if (dot) {
                *dot = '\0';
                sprintf(autosave_file, "%s_autosave.lvl", g_current_level_file);
                *dot = '.';
            } else {
                sprintf(autosave_file, "%s_autosave.lvl", g_current_level_file);
            }
        } else {
            sprintf(autosave_file, "autosave_%ld.lvl", now);
        }
        
        if (save_level(autosave_file)) {
            g_last_autosave = now;
            editor_log(0, "Autosaved to: %s", autosave_file);
        }
    }
}

/**
 * Exports optimized level for game use
 * @param filename Output filename
 * @return 1 on success, 0 on failure
 */
int export_level_optimized(const char* filename)
{
    FILE* file = fopen(filename, "wb");
    if (!file) {
        editor_log(2, "Failed to create export file: %s", filename);
        return 0;
    }
    
    editor_log(0, "Exporting optimized level: %s", filename);
    
    // Write binary header
    unsigned int magic = 0x4C444E45;  // "ENDL"
    unsigned int version = 0x00020000;  // 2.0
    fwrite(&magic, sizeof(unsigned int), 1, file);
    fwrite(&version, sizeof(unsigned int), 1, file);
    
    // TODO: Write optimized binary data
    // - Packed terrain data
    // - Optimized object placement
    // - Baked lighting data
    // - Collision data
    // - Navigation mesh
    
    fclose(file);
    
    editor_log(0, "Level exported successfully");
    return 1;
}

// ========================================================================
// MATERIAL SYSTEM
// ========================================================================

/**
 * Creates default materials
 */
void create_default_materials(void)
{
    // Default material
    create_material("Default", 0, 3, -1, -1);  // White diffuse, flat normal
    
    // Basic materials
    create_material("Stone", -1, -1, -1, -1);
    create_material("Metal", -1, -1, -1, -1);
    create_material("Wood", -1, -1, -1, -1);
    create_material("Glass", -1, -1, -1, -1);
}

/**
 * Creates a new material
 * @param name Material name
 * @param diffuse_tex Diffuse texture ID
 * @param normal_tex Normal texture ID
 * @param specular_tex Specular texture ID
 * @param emissive_tex Emissive texture ID
 * @return Material ID or -1 on failure
 */
int create_material(const char* name, int diffuse_tex, int normal_tex, 
                   int specular_tex, int emissive_tex)
{
    if (g_material_count >= g_material_capacity) {
        // Expand array
        int new_capacity = g_material_capacity * 2;
        Material* new_materials = (Material*)realloc(g_materials, 
                                                    new_capacity * sizeof(Material));
        if (!new_materials) {
            editor_log(2, "Failed to expand material array");
            return -1;
        }
        g_materials = new_materials;
        g_material_capacity = new_capacity;
    }
    
    Material* mat = &g_materials[g_material_count];
    memset(mat, 0, sizeof(Material));
    
    mat->diffuse_texture_id = diffuse_tex;
    mat->normal_texture_id = normal_tex;
    mat->specular_texture_id = specular_tex;
    mat->emissive_texture_id = emissive_tex;
    
    // Default colors
    mat->diffuse_color[0] = mat->diffuse_color[1] = mat->diffuse_color[2] = 1.0f;
    mat->diffuse_color[3] = 1.0f;
    mat->specular_color[0] = mat->specular_color[1] = mat->specular_color[2] = 0.5f;
    mat->specular_color[3] = 1.0f;
    mat->emissive_color[0] = mat->emissive_color[1] = mat->emissive_color[2] = 0.0f;
    mat->emissive_color[3] = 1.0f;
    
    // Default properties
    mat->roughness = 0.5f;
    mat->metallic = 0.0f;
    mat->opacity = 1.0f;
    mat->normal_strength = 1.0f;
    mat->blend_mode = 0;  // Opaque
    mat->two_sided = 0;
    mat->cast_shadows = 1;
    
    strncpy(mat->shader_name, "standard", sizeof(mat->shader_name) - 1);
    
    editor_log(0, "Created material: %s", name);
    return g_material_count++;
}

// ========================================================================
// UTILITY FUNCTIONS
// ========================================================================

/**
 * Gets current timestamp string
 * @return Timestamp string
 */
const char* get_timestamp(void)
{
    static char timestamp[32];
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_info);
    return timestamp;
}

/**
 * Gets filename without path
 * @param path Full path
 * @return Filename portion
 */
const char* get_filename_without_path(const char* path)
{
    const char* filename = strrchr(path, '\\');
    if (!filename) {
        filename = strrchr(path, '/');
    }
    return filename ? filename + 1 : path;
}

/**
 * Checks if a key is pressed
 * @param key Virtual key code
 * @return 1 if pressed, 0 otherwise
 */
int is_key_pressed(int key)
{
    return (GetAsyncKeyState(key) & 0x8000) != 0;
}

/**
 * Creates default prefabs
 */
void create_default_prefabs(void)
{
    // TODO: Create some default prefabs
    // - Basic light setup
    // - Spawn point setup
    // - Common object arrangements
}

/**
 * Loads editor configuration
 * @param filename Configuration filename
 */
void load_editor_config(const char* filename)
{
    FILE* file = fopen(filename, "r");
    if (!file) {
        editor_log(1, "No editor config found, using defaults");
        return;
    }
    
    char line[256];
    while (fgets(line, sizeof(line), file)) {
        char* newline = strchr(line, '\n');
        if (newline) *newline = '\0';
        
        char* equals = strchr(line, '=');
        if (!equals) continue;
        
        *equals = '\0';
        char* key = line;
        char* value = equals + 1;
        
        // Parse configuration values
        if (strcmp(key, "autosave_enabled") == 0) {
            g_config.autosave_enabled = atoi(value);
        } else if (strcmp(key, "autosave_interval") == 0) {
            g_config.autosave_interval = atoi(value);
        } else if (strcmp(key, "camera_speed") == 0) {
            g_config.camera_speed_multiplier = atof(value);
        } else if (strcmp(key, "grid_size") == 0) {
            g_config.grid_size = atof(value);
        } else if (strcmp(key, "snap_angle") == 0) {
            g_config.snap_angle = atof(value);
        }
        // Add other config options
    }
    
    fclose(file);
    editor_log(0, "Loaded editor configuration");
}

/**
 * Saves editor configuration
 * @param filename Configuration filename
 */
void save_editor_config(const char* filename)
{
    FILE* file = fopen(filename, "w");
    if (!file) {
        editor_log(2, "Failed to save editor config");
        return;
    }
    
    fprintf(file, "# Endor Level Editor Configuration\n");
    fprintf(file, "autosave_enabled=%d\n", g_config.autosave_enabled);
    fprintf(file, "autosave_interval=%d\n", g_config.autosave_interval);
    fprintf(file, "camera_speed=%.3f\n", g_config.camera_speed_multiplier);
    fprintf(file, "grid_size=%.3f\n", g_config.grid_size);
    fprintf(file, "snap_angle=%.3f\n", g_config.snap_angle);
    fprintf(file, "show_grid=%d\n", g_config.show_grid);
    fprintf(file, "show_gizmos=%d\n", g_config.show_gizmos);
    fprintf(file, "show_statistics=%d\n", g_config.show_statistics);
    
    fclose(file);
    editor_log(0, "Saved editor configuration");
}

/**
 * Updates list of visible objects
 */
void update_visible_objects(void)
{
    g_visible_object_count = 0;
    
    for (int i = 0; i < g_object_count; i++) {
        EditorObject* obj = &g_editor_objects[i];
        
        // Check visibility
        if (!obj->visible || !g_layers[obj->layer_id].visible) {
            continue;
        }
        
        // TODO: Frustum culling
        if (g_frustum_cull_enabled) {
            // Check if object is in view frustum
        }
        
        // Add to visible list
        if (g_visible_object_count < g_object_capacity) {
            g_visible_objects[g_visible_object_count++] = i;
        }
    }
    
    g_stats.visible_objects = g_visible_object_count;
}

/**
 * Cleans up the level editor
 */
void cleanup_level_editor(void)
{
    if (!g_editor_initialized) {
        return;
    }
    
    editor_log(0, "Cleaning up level editor");
    
    // Save configuration
    save_editor_config("editor_config.ini");
    
    // Free dynamic memory
    if (g_editor_objects) {
        // Free type-specific data
        for (int i = 0; i < g_object_count; i++) {
            if (g_editor_objects[i].type == EDITOR_OBJ_SPLINE && 
                g_editor_objects[i].properties.spline.control_points) {
                free(g_editor_objects[i].properties.spline.control_points);
            }
            if (g_editor_objects[i].custom_properties) {
                free(g_editor_objects[i].custom_properties);
            }
        }
        free(g_editor_objects);
    }
    
    if (g_asset_cache) {
        // Free asset data
        for (int i = 0; i < g_asset_count; i++) {
            if (g_asset_cache[i].data) {
                free(g_asset_cache[i].data);
            }
            if (g_asset_cache[i].thumbnail_data) {
                free(g_asset_cache[i].thumbnail_data);
            }
            // Free dependency list
            AssetDependency* dep = g_asset_cache[i].dependencies;
            while (dep) {
                AssetDependency* next = dep->next;
                free(dep);
                dep = next;
            }
        }
        free(g_asset_cache);
    }
    
    if (g_materials) {
        free(g_materials);
    }
    
    if (g_undo_stack) {
        // Free undo actions
        for (int i = 0; i < g_undo_capacity; i++) {
            free_undo_action(&g_undo_stack[i]);
        }
        free(g_undo_stack);
    }
    
    if (g_visible_objects) {
        free(g_visible_objects);
    }
    
    if (g_selection.object_ids) {
        free(g_selection.object_ids);
    }
    
    if (g_selection.lasso_points) {
        free(g_selection.lasso_points);
    }
    
    // Free terrain data
    if (g_terrain.heights) free(g_terrain.heights);
    if (g_terrain.normals) free(g_terrain.normals);
    if (g_terrain.blend_weights) free(g_terrain.blend_weights);
    if (g_terrain.vegetation_map) free(g_terrain.vegetation_map);
    if (g_terrain.water_map) free(g_terrain.water_map);
    if (g_terrain.chunks) free(g_terrain.chunks);
    
    g_editor_initialized = 0;
    
    editor_log(0, "Level editor cleanup complete");
}

// ========================================================================
// PUBLIC API
// ========================================================================

/**
 * Main editor update function
 * @param delta_time Frame delta time in seconds
 */
void update_level_editor(float delta_time)
{
    if (!g_editor_initialized) {
        return;
    }
    
    // Update camera
    update_editor_camera(delta_time);
    
    // Update visible objects
    update_visible_objects();
    
    // Check for hot-reload
    static float hot_reload_timer = 0.0f;
    hot_reload_timer += delta_time;
    if (hot_reload_timer > 1.0f) {
        hot_reload_timer = 0.0f;
        for (int i = 0; i < g_asset_count; i++) {
            check_asset_hot_reload(i);
        }
    }
    
    // Check autosave
    check_autosave();
    
    // Update statistics
    g_stats.last_frame_time = delta_time;
}

/**
 * Renders the level editor
 */
void render_level_editor(void)
{
    if (!g_editor_initialized) {
        return;
    }
    
    // TODO: Implement rendering
    // - Render terrain
    // - Render objects
    // - Render selection
    // - Render gizmos
    // - Render grid
    // - Render UI
}

/**
 * Handles editor input
 * @param msg Windows message
 * @param wparam WPARAM
 * @param lparam LPARAM
 * @return 1 if handled, 0 otherwise
 */
int handle_editor_input(UINT msg, WPARAM wparam, LPARAM lparam)
{
    if (!g_editor_initialized) {
        return 0;
    }
    
    switch (msg) {
        case WM_KEYDOWN:
            switch (wparam) {
                case 'Z':
                    if (GetAsyncKeyState(VK_CONTROL) & 0x8000) {
                        if (GetAsyncKeyState(VK_SHIFT) & 0x8000) {
                            redo();
                        } else {
                            undo();
                        }
                        return 1;
                    }
                    break;
                    
                case 'S':
                    if (GetAsyncKeyState(VK_CONTROL) & 0x8000) {
                        if (strlen(g_current_level_file) > 0) {
                            save_level(g_current_level_file);
                        }
                        return 1;
                    }
                    break;
                    
                case VK_DELETE:
                    if (g_selection.count > 0) {
                        begin_undo_group("Delete Selection");
                        for (int i = g_selection.count - 1; i >= 0; i--) {
                            delete_editor_object(g_selection.object_ids[i]);
                        }
                        end_undo_group();
                        return 1;
                    }
                    break;
                    
                case VK_F2:
                    focus_camera_on_selection();
                    return 1;
            }
            break;
            
        case WM_LBUTTONDOWN:
        case WM_RBUTTONDOWN:
        case WM_MBUTTONDOWN:
            // Handle mouse input
            break;
            
        case WM_MOUSEWHEEL:
            {
                short wheel_delta = GET_WHEEL_DELTA_WPARAM(wparam);
                camera_mouse_input(0, 0, 0, wheel_delta / WHEEL_DELTA);
                return 1;
            }
            break;
    }
    
    return 0;
};
    
    // Create temporary buffer for smoothed values
    float* smoothed = (float*)malloc((max_x - min_x + 1) * (max_z - min_z + 1) * sizeof(float));
    
    for (int z = min_z; z <= max_z; z++) {
        for (int x = min_x; x <= max_x; x++) {
            float dx = (x - center_x) * g_terrain.scale;
            float dz = (z - center_z) * g_terrain.scale;
            float distance = sqrtf(dx * dx + dz * dz);
            
            if (distance <= g_terrain_brush.radius) {
                float influence = 1.0f - (distance / g_terrain_brush.radius);
                influence = powf(influence, g_terrain_brush.falloff) * g_terrain_brush.strength;
                
                // Average with neighbors
                float sum = 0.0f;
                int count = 0;
                
                for (int nz = z - 1; nz <= z + 1; nz++) {
                    for (int nx = x - 1; nx <= x + 1; nx++) {
                        if (nx >= 0 && nx < g_terrain.size_x && 
                            nz >= 0 && nz < g_terrain.size_z) {
                            sum += g_terrain.heights[nz * g_terrain.size_x + nx];
                            count++;
                        }
                    }
                }
                
                float avg = sum / count;
                int local_idx = (z - min_z) * (max_x - min_x + 1) + (x - min_x);
                int terrain_idx = z * g_terrain.size_x + x;
                
                smoothed[local_idx] = g_terrain.heights[terrain_idx] + 
                                     (avg - g_terrain.heights[terrain_idx]) * influence;
            }
        }
    }
    
    // Apply smoothed values
    for (int z = min_z; z <= max_z; z++) {
        for (int x = min_x; x <= max_x; x++) {
            float dx = (x - center_x) * g_terrain.scale;
            float dz = (z - center_z) * g_terrain.scale;
            float distance = sqrtf(dx * dx + dz * dz);
            
            if (distance <= g_terrain_brush.radius) {
                int local_idx = (z - min_z) * (max_x - min_x + 1) + (x - min_x);
                int terrain_idx = z * g_terrain.size_x + x;
                g_terrain.heights[terrain_idx] = smoothed[local_idx];
            }
        }
    }
    
    free(smoothed);
}

/**
 * Applies flatten brush
 */
void apply_terrain_brush_flatten(int center_x, int center_z, 
                                int min_x, int max_x, int min_z, int max_z, int apply)
{
    if (!apply) return;
    
    // Simple erosion simulation
    float* water_flow = (float*)calloc((max_x - min_x + 1) * (max_z - min_z + 1), sizeof(float));
    float* sediment = (float*)calloc((max_x - min_x + 1) * (max_z - min_z + 1), sizeof(float));
    
    // Simulate water flow
    for (int iteration = 0; iteration < 10; iteration++) {
        for (int z = min_z; z <= max_z; z++) {
            for (int x = min_x; x <= max_x; x++) {
                float dx = (x - center_x) * g_terrain.scale;
                float dz = (z - center_z) * g_terrain.scale;
                float distance = sqrtf(dx * dx + dz * dz);
                
                if (distance <= g_terrain_brush.radius) {
                    int local_idx = (z - min_z) * (max_x - min_x + 1) + (x - min_x);
                    int terrain_idx = z * g_terrain.size_x + x;
                    
                    // Calculate gradient
                    float gradient_x = 0.0f;
                    float gradient_z = 0.0f;
                    
                    if (x > 0 && x < g_terrain.size_x - 1) {
                        gradient_x = g_terrain.heights[terrain_idx + 1] - 
                                    g_terrain.heights[terrain_idx - 1];
                    }
                    if (z > 0 && z < g_terrain.size_z - 1) {
                        gradient_z = g_terrain.heights[(z + 1) * g_terrain.size_x + x] - 
                                    g_terrain.heights[(z - 1) * g_terrain.size_x + x];
                    }
                    
                    // Water flows downhill
                    float flow_strength = sqrtf(gradient_x * gradient_x + gradient_z * gradient_z);
                    water_flow[local_idx] += flow_strength * g_terrain_brush.flow_rate;
                    
                    // Erosion
                    float erosion = water_flow[local_idx] * g_terrain_brush.strength * 0.1f;
                    g_terrain.heights[terrain_idx] -= erosion;
                    sediment[local_idx] += erosion;
                    
                    // Deposition
                    float deposition = sediment[local_idx] * g_terrain_brush.deposition_rate;
                    g_terrain.heights[terrain_idx] += deposition;
                    sediment[local_idx] -= deposition;
                }
            }
        }
    }
    
    free(water_flow);
    free(sediment);
}

/**
 * Paints terrain texture
 * @param world_x World X coordinate
 * @param world_z World Z coordinate
 * @param texture_layer Texture layer to paint
 */
void paint_terrain_texture(float world_x, float world_z, int texture_layer)
{
    if (texture_layer < 0 || texture_layer >= 8) {
        return;
    }
    
    // Convert world to terrain coordinates
    int center_x = (int)((world_x / g_terrain.scale) + g_terrain.size_x * 0.5f);
    int center_z = (int)((world_z / g_terrain.scale) + g_terrain.size_z * 0.5f);
    
    if (center_x < 0 || center_x >= g_terrain.size_x || 
        center_z < 0 || center_z >= g_terrain.size_z) {
        return;
    }
    
    // Calculate affected region
    int radius_cells = (int)(g_terrain_brush.radius / g_terrain.scale) + 1;
    int min_x = fmaxf(0, center_x - radius_cells);
    int max_x = fminf(g_terrain.size_x - 1, center_x + radius_cells);
    int min_z = fmaxf(0, center_z - radius_cells);
    int max_z = fminf(g_terrain.size_z - 1, center_z + radius_cells);
    
    for (int z = min_z; z <= max_z; z++) {
        for (int x = min_x; x <= max_x; x++) {
            float dx = (x - center_x) * g_terrain.scale;
            float dz = (z - center_z) * g_terrain.scale;
            float distance = sqrtf(dx * dx + dz * dz);
            
            if (distance <= g_terrain_brush.radius) {
                float influence = 1.0f - (distance / g_terrain_brush.radius);
                influence = powf(influence, g_terrain_brush.falloff) * g_terrain_brush.strength;
                
                int idx = z * g_terrain.size_x + x;
                
                // Update blend weights
                float* weights = &g_terrain.blend_weights[idx * 8];
                
                // Increase target layer
                weights[texture_layer] += influence;
                weights[texture_layer] = fminf(1.0f, weights[texture_layer]);
                
                // Normalize weights
                float total = 0.0f;
                for (int i = 0; i < 8; i++) {
                    total += weights[i];
                }
                
                if (total > 0.0f) {
                    for (int i = 0; i < 8; i++) {
                        weights[i] /= total;
                    }
                }
            }
        }
    }
    
    // Mark chunks as dirty
    mark_terrain_chunks_dirty(min_x, max_x, min_z, max_z);
}

/**
 * Updates terrain normals for a region
 * @param min_x Minimum X coordinate
 * @param max_x Maximum X coordinate
 * @param min_z Minimum Z coordinate
 * @param max_z Maximum Z coordinate
 */
void update_terrain_normals(int min_x, int max_x, int min_z, int max_z)
{
    // Expand region by 1 to include neighbors
    min_x = fmaxf(1, min_x - 1);
    max_x = fminf(g_terrain.size_x - 2, max_x + 1);
    min_z = fmaxf(1, min_z - 1);
    max_z = fminf(g_terrain.size_z - 2, max_z + 1);
    
    for (int z = min_z; z <= max_z; z++) {
        for (int x = min_x; x <= max_x; x++) {
            int idx = z * g_terrain.size_x + x;
            
            // Calculate normal from neighboring heights
            float h_left = g_terrain.heights[idx - 1];
            float h_right = g_terrain.heights[idx + 1];
            float h_down = g_terrain.heights[(z - 1) * g_terrain.size_x + x];
            float h_up = g_terrain.heights[(z + 1) * g_terrain.size_x + x];
            
            // Cross product of tangent vectors
            float dx = 2.0f * g_terrain.scale;
            float dy_x = h_right - h_left;
            float dy_z = h_up - h_down;
            
            // Normal = (dx, dy_x, 0) x (0, dy_z, dx)
            float nx = -dy_x * g_terrain.vertical_scale;
            float ny = dx * g_terrain.vertical_scale;
            float nz = -dy_z * g_terrain.vertical_scale;
            
            // Normalize
            float length = sqrtf(nx * nx + ny * ny + nz * nz);
            if (length > 0.0f) {
                g_terrain.normals[idx * 3] = nx / length;
                g_terrain.normals[idx * 3 + 1] = ny / length;
                g_terrain.normals[idx * 3 + 2] = nz / length;
            } else {
                g_terrain.normals[idx * 3] = 0.0f;
                g_terrain.normals[idx * 3 + 1] = 1.0f;
                g_terrain.normals[idx * 3 + 2] = 0.0f;
            }
        }
    }
}

/**
 * Marks terrain chunks as dirty
 * @param min_x Minimum X coordinate
 * @param max_x Maximum X coordinate
 * @param min_z Minimum Z coordinate
 * @param max_z Maximum Z coordinate
 */
void mark_terrain_chunks_dirty(int min_x, int max_x, int min_z, int max_z)
{
    int chunk_min_x = min_x / TERRAIN_CHUNK_SIZE;
    int chunk_max_x = max_x / TERRAIN_CHUNK_SIZE;
    int chunk_min_z = min_z / TERRAIN_CHUNK_SIZE;
    int chunk_max_z = max_z / TERRAIN_CHUNK_SIZE;
    
    for (int cz = chunk_min_z; cz <= chunk_max_z; cz++) {
        for (int cx = chunk_min_x; cx <= chunk_max_x; cx++) {
            if (cx >= 0 && cx < g_terrain.chunk_count_x &&
                cz >= 0 && cz < g_terrain.chunk_count_z) {
                g_terrain.chunks[cz * g_terrain.chunk_count_x + cx].dirty = 1;
            }
        }
    }
}

/**
 * Loads terrain heightmap from file
 * @param filename Heightmap filename
 * @return 1 on success, 0 on failure
 */
int load_terrain_heightmap(const char* filename)
{
    FILE* file = fopen(filename, "rb");
    if (!file) {
        editor_log(2, "Failed to open heightmap: %s", filename);
        return 0;
    }
    
    // Read header (assuming raw float format)
    int width, height;
    fread(&width, sizeof(int), 1, file);
    fread(&height, sizeof(int), 1, file);
    
    if (width != g_terrain.size_x || height != g_terrain.size_z) {
        editor_log(1, "Heightmap size mismatch: %dx%d vs %dx%d", 
                  width, height, g_terrain.size_x, g_terrain.size_z);
        fclose(file);
        return 0;
    }
    
    // Read height data
    fread(g_terrain.heights, sizeof(float), width * height, file);
    fclose(file);
    
    // Update normals for entire terrain
    update_terrain_normals(0, g_terrain.size_x - 1, 0, g_terrain.size_z - 1);
    
    // Mark all chunks as dirty
    for (int i = 0; i < g_terrain.chunk_count_x * g_terrain.chunk_count_z; i++) {
        g_terrain.chunks[i].dirty = 1;
    }
    
    editor_log(0, "Loaded heightmap: %s", filename);
    return 1;
}

/**
 * Exports terrain heightmap to file
 * @param filename Output filename
 * @return 1 on success, 0 on failure
 */
int export_terrain_heightmap(const char* filename)
{
    FILE* file = fopen(filename, "wb");
    if (!file) {
        editor_log(2, "Failed to create heightmap file: %s", filename);
        return 0;
    }
    
    // Write header
    fwrite(&g_terrain.size_x, sizeof(int), 1, file);
    fwrite(&g_terrain.size_z, sizeof(int), 1, file);
    
    // Write height data
    fwrite(g_terrain.heights, sizeof(float), 
           g_terrain.size_x * g_terrain.size_z, file);
    
    fclose(file);
    
    editor_log(0, "Exported heightmap: %s", filename);
    return 1;
}

/**
 * Resizes terrain
 * @param new_width New terrain width
 * @param new_height New terrain height
 */
void resize_terrain(int new_width, int new_height)
{
    if (new_width == g_terrain.size_x && new_height == g_terrain.size_z) {
        return;
    }
    
    // Allocate new arrays
    int new_size = new_width * new_height;
    float* new_heights = (float*)calloc(new_size, sizeof(float));
    float* new_normals = (float*)calloc(new_size * 3, sizeof(float));
    float* new_blend_weights = (float*)calloc(new_size * 8, sizeof(float));
    
    // Copy existing data (with bilinear sampling for scaling)
    for (int z = 0; z < new_height; z++) {
        for (int x = 0; x < new_width; x++) {
            float src_x = (x * (g_terrain.size_x - 1)) / (float)(new_width - 1);
            float src_z = (z * (g_terrain.size_z - 1)) / (float)(new_height - 1);
            
            int x0 = (int)src_x;
            int z0 = (int)src_z;
            int x1 = fminf(x0 + 1, g_terrain.size_x - 1);
            int z1 = fminf(z0 + 1, g_terrain.size_z - 1);
            
            float fx = src_x - x0;
            float fz = src_z - z0;
            
            // Bilinear interpolation
            float h00 = g_terrain.heights[z0 * g_terrain.size_x + x0];
            float h10 = g_terrain.heights[z0 * g_terrain.size_x + x1];
            float h01 = g_terrain.heights[z1 * g_terrain.size_x + x0];
            float h11 = g_terrain.heights[z1 * g_terrain.size_x + x1];
            
            float h0 = h00 * (1 - fx) + h10 * fx;
            float h1 = h01 * (1 - fx) + h11 * fx;
            float h = h0 * (1 - fz) + h1 * fz;
            
            new_heights[z * new_width + x] = h;
            
            // Copy blend weights
            for (int i = 0; i < 8; i++) {
                new_blend_weights[(z * new_width + x) * 8 + i] = 
                    g_terrain.blend_weights[(z0 * g_terrain.size_x + x0) * 8 + i];
            }
        }
    }
    
    // Free old arrays
    free(g_terrain.heights);
    free(g_terrain.normals);
    free(g_terrain.blend_weights);
    free(g_terrain.vegetation_map);
    free(g_terrain.water_map);
    free(g_terrain.chunks);
    
    // Update terrain data
    g_terrain.size_x = new_width;
    g_terrain.size_z = new_height;
    g_terrain.heights = new_heights;
    g_terrain.normals = new_normals;
    g_terrain.blend_weights = new_blend_weights;
    g_terrain.vegetation_map = (float*)calloc(new_size, sizeof(float));
    g_terrain.water_map = (float*)calloc(new_size, sizeof(float));
    
    // Recreate chunks
    g_terrain.chunk_count_x = (new_width + TERRAIN_CHUNK_SIZE - 1) / TERRAIN_CHUNK_SIZE;
    g_terrain.chunk_count_z = (new_height + TERRAIN_CHUNK_SIZE - 1) / TERRAIN_CHUNK_SIZE;
    g_terrain.chunks = (TerrainChunk*)calloc(g_terrain.chunk_count_x * g_terrain.chunk_count_z, 
                                             sizeof(TerrainChunk));
    
    // Update normals
    update_terrain_normals(0, new_width - 1, 0, new_height - 1);
    
    editor_log(0, "Resized terrain to %dx%d", new_width, new_height);
}

// ========================================================================
// LAYER SYSTEM
// ========================================================================

/**
 * Creates a new layer
 * @param name Layer name
 * @param parent_id Parent layer ID (-1 for root)
 * @return Layer ID or -1 on failure
 */
int create_layer(const char* name, int parent_id)
{
    if (g_layer_count >= MAX_LAYERS) {
        editor_log(2, "Maximum layer count reached");
        return -1;
    }
    
    EditorLayer* layer = &g_layers[g_layer_count];
    strncpy(layer->name, name, sizeof(layer->name) - 1);
    layer->visible = 1;
    layer->locked = 0;
    layer->selectable = 1;
    layer->color[0] = 0.5f + (rand() % 50) / 100.0f;
    layer->color[1] = 0.5f + (rand() % 50) / 100.0f;
    layer->color[2] = 0.5f + (rand() % 50) / 100.0f;
    layer->color[3] = 1.0f;
    layer->object_count = 0;
    layer->parent_layer = parent_id;
    
    editor_log(0, "Created layer: %s", name);
    return g_layer_count++;
}

/**
 * Deletes a layer
 * @param layer_id Layer ID to delete
 */
void delete_layer(int layer_id)
{
    if (layer_id <= 0 || layer_id >= g_layer_count) {
        return;  // Can't delete default layer
    }
    
    // Move all objects to default layer
    for (int i = 0; i < g_object_count; i++) {
        if (g_editor_objects[i].layer_id == layer_id) {
            g_editor_objects[i].layer_id = 0;
        }
    }
    
    // Update child layers
    for (int i = 0; i < g_layer_count; i++) {
        if (g_layers[i].parent_layer == layer_id) {
            g_layers[i].parent_layer = -1;
        }
    }
    
    editor_log(0, "Deleted layer: %s", g_layers[layer_id].name);
    
    // Shift layers down
    for (int i = layer_id; i < g_layer_count - 1; i++) {
        g_layers[i] = g_layers[i + 1];
    }
    g_layer_count--;
    
    // Update object layer references
    for (int i = 0; i < g_object_count; i++) {
        if (g_editor_objects[i].layer_id > layer_id) {
            g_editor_objects[i].layer_id--;
        }
    }
}

/**
 * Sets layer visibility
 * @param layer_id Layer ID
 * @param visible Visibility state
 */
void set_layer_visibility(int layer_id, int visible)
{
    if (layer_id < 0 || layer_id >= g_layer_count) {
        return;
    }
    
    g_layers[layer_id].visible = visible;
    
    // Update visible objects
    update_visible_objects();
    
    editor_log(0, "Set layer %s visibility to %d", 
              g_layers[layer_id].name, visible);
}

/**
 * Moves objects to a layer
 * @param object_ids Array of object IDs
 * @param count Number of objects
 * @param target_layer Target layer ID
 */
void move_objects_to_layer(int* object_ids, int count, int target_layer)
{
    if (target_layer < 0 || target_layer >= g_layer_count) {
        return;
    }
    
    begin_undo_group("Move Objects to Layer");
    
    for (int i = 0; i < count; i++) {
        EditorObject* obj = find_editor_object(object_ids[i]);
        if (obj) {
            EditorObject old_state = *obj;
            
            // Update layer counts
            if (obj->layer_id < g_layer_count) {
                g_layers[obj->layer_id].object_count--;
            }
            g_layers[target_layer].object_count++;
            
            obj->layer_id = target_layer;
            
            add_undo_action(UNDO_TYPE_OBJECT_MODIFY, obj->id, &old_state, obj);
        }
    }
    
    end_undo_group();
    
    editor_log(0, "Moved %d objects to layer %s", count, g_layers[target_layer].name);
}

// ========================================================================
// PREFAB SYSTEM
// ========================================================================

/**
 * Creates a prefab from selection
 * @param name Prefab name
 * @param description Prefab description
 * @return Prefab ID or -1 on failure
 */
int create_prefab_from_selection(const char* name, const char* description)
{
    if (g_selection.count == 0) {
        editor_log(1, "No objects selected for prefab creation");
        return -1;
    }
    
    if (g_prefab_count >= MAX_PREFABS) {
        editor_log(2, "Maximum prefab count reached");
        return -1;
    }
    
    Prefab* prefab = &g_prefabs[g_prefab_count];
    strncpy(prefab->name, name, sizeof(prefab->name) - 1);
    strncpy(prefab->description, description, sizeof(prefab->description) - 1);
    
    // Copy selected object IDs
    prefab->object_count = fminf(g_selection.count, MAX_SELECTION);
    for (int i = 0; i < prefab->object_count; i++) {
        prefab->object_ids[i] = g_selection.object_ids[i];
    }
    
    // Calculate bounds
    memcpy(prefab->bounds_min, g_selection.bounds_min, sizeof(float) * 3);
    memcpy(prefab->bounds_max, g_selection.bounds_max, sizeof(float) * 3);
    
    // Set metadata
    prefab->creation_date = time(NULL);
    strncpy(prefab->author, "Editor User", sizeof(prefab->author) - 1);
    sprintf(prefab->tags, "objects:%d", prefab->object_count);
    
    // Generate thumbnail path
    sprintf(prefab->thumbnail_path, "thumbnails/prefab_%d.bmp", g_prefab_count);
    
    // TODO: Generate prefab thumbnail
    
    editor_log(0, "Created prefab: %s with %d objects", name, prefab->object_count);
    return g_prefab_count++;
}

/**
 * Instantiates a prefab
 * @param prefab_id Prefab ID
 * @param x X position
 * @param y Y position
 * @param z Z position
 * @return Root object ID or -1 on failure
 */
int instantiate_prefab(int prefab_id, float x, float y, float z)
{
    if (prefab_id < 0 || prefab_id >= g_prefab_count) {
        return -1;
    }
    
    Prefab* prefab = &g_prefabs[prefab_id];
    
    // Calculate offset from prefab center
    float prefab_center[3] = {
        (prefab->bounds_min[0] + prefab->bounds_max[0]) * 0.5f,
        (prefab->bounds_min[1] + prefab->bounds_max[1]) * 0.5f,
        (prefab->bounds_min[2] + prefab->bounds_max[2]) * 0.5f
    };
    
    float offset[3] = {
        x - prefab_center[0],
        y - prefab_center[1],
        z - prefab_center[2]
    };
    
    // Clear selection
    clear_selection();
    
    begin_undo_group("Instantiate Prefab");
    
    // Create prefab instance parent
    int parent_id = create_editor_object(EDITOR_OBJ_PREFAB_INSTANCE, x, y, z);
    if (parent_id < 0) {
        end_undo_group();
        return -1;
    }
    
    EditorObject* parent = &g_editor_objects[parent_id - 1];
    sprintf(parent->name, "%s_Instance", prefab->name);
    parent->properties.prefab.prefab_id = prefab_id;
    parent->properties.prefab.instance_seed = (float)rand() / RAND_MAX;
    
    // Duplicate all prefab objects
    for (int i = 0; i < prefab->object_count; i++) {
        EditorObject* template_obj = find_editor_object(prefab->object_ids[i]);
        if (!template_obj) continue;
        
        int new_id = create_editor_object(template_obj->type,
                                         template_obj->position[0] + offset[0],
                                         template_obj->position[1] + offset[1],
                                         template_obj->position[2] + offset[2]);
        
        if (new_id >= 0) {
            EditorObject* new_obj = &g_editor_objects[new_id - 1];
            
            // Copy properties
            memcpy(new_obj->rotation, template_obj->rotation, sizeof(float) * 3);
            memcpy(new_obj->scale, template_obj->scale, sizeof(float) * 3);
            new_obj->asset_id = template_obj->asset_id;
            new_obj->material_id = template_obj->material_id;
            new_obj->parent_id = parent_id;
            
            // Update asset reference count
            if (new_obj->asset_id >= 0) {
                g_asset_cache[new_obj->asset_id].reference_count++;
            }
            
            add_to_selection(new_id);
        }
    }
    
    end_undo_group();
    
    editor_log(0, "Instantiated prefab: %s at (%.2f, %.2f, %.2f)", 
              prefab->name, x, y, z);
    
    return parent_id;
}

// ========================================================================
// UNDO/REDO SYSTEM
// ========================================================================

/**
 * Begins an undo group
 * @param description Group description
 */
void begin_undo_group(const char* description)
{
    g_undo_group_depth++;
    
    if (g_undo_group_depth == 1) {
        // Create group action
        UndoAction* group = (UndoAction*)calloc(1, sizeof(UndoAction));
        group->type = UNDO_TYPE_BATCH;
        strncpy(group->description, description, sizeof(group->description) - 1);
        group->timestamp = time(NULL);
        
        add_undo_action_data(group);
    }
}

/**
 * Ends an undo group
 */
void end_undo_group(void)
{
    g_undo_group_depth--;
    if (g_undo_group_depth < 0) {
        g_undo_group_depth = 0;
    }
}

/**
 * Adds an undo action
 * @param type Action type
 * @param object_id Object ID
 * @param old_state Old object state
 * @param new_state New object state
 */
void add_undo_action(UndoType type, int object_id, EditorObject* old_state, EditorObject* new_state)
{
    UndoAction* action = (UndoAction*)calloc(1, sizeof(UndoAction));
    action->type = type;
    action->timestamp = time(NULL);
    
    // Set description based on type
    switch (type) {
        case UNDO_TYPE_OBJECT_CREATE:
            sprintf(action->description, "Create %s", new_state ? new_state->name : "Object");
            break;
        case UNDO_TYPE_OBJECT_DELETE:
            sprintf(action->description, "Delete %s", old_state ? old_state->name : "Object");
            break;
        case UNDO_TYPE_OBJECT_MODIFY:
            sprintf(action->description, "Modify %s", old_state ? old_state->name : "Object");
            break;
        default:
            strcpy(action->description, "Unknown Action");
            break;
    }
    
    // Store object data
    action->object_count = 1;
    action->object_ids = (int*)malloc(sizeof(int));
    action->object_ids[0] = object_id;
    
    if (old_state) {
        action->old_states = (EditorObject*)malloc(sizeof(EditorObject));
        memcpy(action->old_states, old_state, sizeof(EditorObject));
    }
    
    if (new_state) {
        action->new_states = (EditorObject*)malloc(sizeof(EditorObject));
        memcpy(action->new_states, new_state, sizeof(EditorObject));
    }
    
    add_undo_action_data(action);
}

/**
 * Adds undo action data to stack
 * @param action Action to add
 */
void add_undo_action_data(UndoAction* action)
{
    // Remove any redo actions
    while (g_undo_count > g_undo_head) {
        g_undo_count--;
        int idx = (g_undo_tail + g_undo_count) % g_undo_capacity;
        free_undo_action(&g_undo_stack[idx]);
    }
    
    // Check capacity
    if (g_undo_count >= g_config.max_undo_steps) {
        // Remove oldest action
        free_undo_action(&g_undo_stack[g_undo_tail]);
        g_undo_tail = (g_undo_tail + 1) % g_undo_capacity;
        g_undo_count--;
    }
    
    // Add new action
    int idx = (g_undo_tail + g_undo_head) % g_undo_capacity;
    
    // If we're in a group, add as sub-action
    if (g_undo_group_depth > 0 && g_undo_head > 0) {
        int group_idx = (g_undo_tail + g_undo_head - 1) % g_undo_capacity;
        UndoAction* group = &g_undo_stack[group_idx];
        
        if (group->type == UNDO_TYPE_BATCH) {
            // Add as sub-action
            group->sub_action_count++;
            group->sub_actions = (UndoAction*)realloc(group->sub_actions, 
                                                      group->sub_action_count * sizeof(UndoAction));
            memcpy(&group->sub_actions[group->sub_action_count - 1], action, sizeof(UndoAction));
            free(action);
            return;
        }
    }
    
    // Add as top-level action
    memcpy(&g_undo_stack[idx], action, sizeof(UndoAction));
    free(action);
    
    g_undo_head++;
    g_undo_count++;
}

/**
 * Performs undo
 */
void undo(void)
{
    if (g_undo_head == 0) {
        editor_log(1, "Nothing to undo");
        return;
    }
    
    g_undo_head--;
    int idx = (g_undo_tail + g_undo_head) % g_undo_capacity;
    UndoAction* action = &g_undo_stack[idx];
    
    editor_log(0, "Undo: %s", action->description);
    apply_undo_action(action, 1);
    
    g_stats.changes_since_save++;
}

/**
 * Performs redo
 */
void redo(void)
{
    if (g_undo_head >= g_undo_count) {
        editor_log(1, "Nothing to redo");
        return;
    }
    
    int idx = (g_undo_tail + g_undo_head) % g_undo_capacity;
    UndoAction* action = &g_undo_stack[idx];
    
    editor_log(0, "Redo: %s", action->description);
    apply_undo_action(action, 0);
    
    g_undo_head++;
    g_stats.changes_since_save++;
}

/**
 * Applies an undo action
 * @param action Action to apply
 * @param is_undo Whether this is an undo (vs redo)
 */
void apply_undo_action(UndoAction* action, int is_undo)
{
    switch (action->type) {
        case UNDO_TYPE_OBJECT_CREATE:
            if (is_undo) {
                // Delete the created object
                delete_editor_object(action->object_ids[0]);
            } else {
                // Recreate the object
                if (action->new_states) {
                    // Find a free slot
                    if (g_object_count < g_object_capacity) {
                        memcpy(&g_editor_objects[g_object_count], 
                               action->new_states, sizeof(EditorObject));
                        g_object_count++;
                    }
                }
            }
            break;
            
        case UNDO_TYPE_OBJECT_DELETE:
            if (is_undo) {
                // Recreate the deleted object
                if (action->old_states && g_object_count < g_object_capacity) {
                    memcpy(&g_editor_objects[g_object_count], 
                           action->old_states, sizeof(EditorObject));
                    g_object_count++;
                }
            } else {
                // Delete the object again
                delete_editor_object(action->object_ids[0]);
            }
            break;
            
        case UNDO_TYPE_OBJECT_MODIFY:
            {
                EditorObject* obj = find_editor_object(action->object_ids[0]);
                if (obj) {
                    EditorObject temp = *obj;
                    if (is_undo && action->old_states) {
                        *obj = *action->old_states;
                    } else if (!is_undo && action->new_states) {
                        *obj = *action->new_states;
                    }
                }
            }
            break;
            
        case UNDO_TYPE_TERRAIN_MODIFY:
            if (action->terrain_backup) {
                // Swap terrain data
                int x1 = action->terrain_region[0];
                int z1 = action->terrain_region[1];
                int x2 = action->terrain_region[2];
                int z2 = action->terrain_region[3];
                int region_width = x2 - x1 + 1;
                
                for (int z = z1; z <= z2; z++) {
                    for (int x = x1; x <= x2; x++) {
                        int backup_idx = (z - z1) * region_width + (x - x1);
                        int terrain_idx = z * g_terrain.size_x + x;
                        
                        float temp = g_terrain.heights[terrain_idx];
                        g_terrain.heights[terrain_idx] = action->terrain_backup[backup_idx];
                        action->terrain_backup[backup_idx] = temp;
                    }
                }
                
                // Update normals and chunks
                update_terrain_normals(x1, x2, z1, z2);
                mark_terrain_chunks_dirty(x1, x2, z1, z2);
            }
            break;
            
        case UNDO_TYPE_BATCH:
            // Apply sub-actions in reverse order for undo
            if (is_undo) {
                for (int i = action->sub_action_count - 1; i >= 0; i--) {
                    apply_undo_action(&action->sub_actions[i], is_undo);
                }
            } else {
                for (int i = 0; i < action->sub_action_count; i++) {
                    apply_undo_action(&action->sub_actions[i], is_undo);
                }
            }
            break;
    }
    
    // Update selection bounds if needed
    if (action->type != UNDO_TYPE_TERRAIN_MODIFY) {
        update_selection_bounds();
    }
}

/**
 * Frees undo action memory
 * @param action Action to free
 */
void free_undo_action(UndoAction* action)
{
    if (action->object_ids) {
        free(action->object_ids);
    }
    if (action->old_states) {
        free(action->old_states);
    }
    if (action->new_states) {
        free(action->new_states);
    }
    if (action->terrain_backup) {
        free(action->terrain_backup);
    }
    if (action->sub_actions) {
        for (int i = 0; i < action->sub_action_count; i++) {
            free_undo_action(&action->sub_actions[i]);
        }
        free(action->sub_actions);
    }
}

// ========================================================================
// FILE I/O
// ========================================================================

/**
 * Saves level to file
 * @param filename Output filename
 * @return 1 on success, 0 on failure
 */
int save_level(const char* filename)
{
    FILE* file = fopen(filename, "w");
    if (!file) {
        editor_log(2, "Failed to create level file: %s", filename);
        return 0;
    }
    
    editor_log(0, "Saving level: %s", filename);
    
    // Write header
    fprintf(file, "[ENDOR_LEVEL]\n");
    fprintf(file, "version=2.0\n");
    fprintf(file, "name=Level\n");
    fprintf(file, "author=Editor User\n");
    fprintf(file, "date=%s\n", get_timestamp());
    fprintf(file, "\n");
    
    // Write terrain data
    save_terrain_data(file);
    
    // Write layers
    fprintf(file, "[LAYERS]\n");
    fprintf(file, "count=%d\n", g_layer_count);
    for (int i = 0; i < g_layer_count; i++) {
        fprintf(file, "\n[LAYER_%d]\n", i);
        fprintf(file, "name=%s\n", g_layers[i].name);
        fprintf(file, "visible=%d\n", g_layers[i].visible);
        fprintf(file, "locked=%d\n", g_layers[i].locked);
        fprintf(file, "selectable=%d\n", g_layers[i].selectable);
        fprintf(file, "color=%.3f,%.3f,%.3f,%.3f\n", 
                g_layers[i].color[0], g_layers[i].color[1], 
                g_layers[i].color[2], g_layers[i].color[3]);
        fprintf(file, "parent=%d\n", g_layers[i].parent_layer);
    }
    fprintf(file, "\n");
    
    // Write objects
    save_object_data(file);
    
    // Write camera settings
    fprintf(file, "[CAMERA]\n");
    fprintf(file, "position=%.3f,%.3f,%.3f\n", 
            g_camera.position[0], g_camera.position[1], g_camera.position[2]);
    fprintf(file, "target=%.3f,%.3f,%.3f\n", 
            g_camera.target[0], g_camera.target[1], g_camera.target[2]);
    fprintf(file, "rotation=%.3f,%.3f\n", g_camera.rotation[0], g_camera.rotation[1]);
    fprintf(file, "distance=%.3f\n", g_camera.distance);
    fprintf(file, "fov=%.3f\n", g_camera.fov);
    fprintf(file, "\n");
    
    fclose(file);
    
    // Save current filename
    strncpy(g_current_level_file, filename, sizeof(g_current_level_file) - 1);
    g_stats.changes_since_save = 0;
    
    editor_log(0, "Level saved successfully");
    return 1;
}

/**
 * Saves terrain data to file
 * @param file Output file
 */
void save_terrain_data(FILE* file)
{
    fprintf(file, "[TERRAIN]\n");
    fprintf(file, "size=%d,%d\n", g_terrain.size_x, g_terrain.size_z);
    fprintf(file, "scale=%.3f\n", g_terrain.scale);
    fprintf(file, "vertical_scale=%.3f\n", g_terrain.vertical_scale);
    fprintf(file, "detail_texture=%d\n", g_terrain.detail_texture_id);
    fprintf(file, "detail_scale=%.3f\n", g_terrain.detail_scale);
    fprintf(file, "\n");
    
    // Write texture layers
    fprintf(file, "[TERRAIN_TEXTURES]\n");
    for (int i = 0; i < 8; i++) {
        if (g_terrain.texture_ids[i] >= 0) {
            fprintf(file, "texture_%d=%d,%.3f\n", i, 
                    g_terrain.texture_ids[i], g_terrain.texture_scales[i]);
        }
    }
    fprintf(file, "\n");
    
    // Write heightmap data (compressed)
    fprintf(file, "[TERRAIN_HEIGHTS]\n");
    fprintf(file, "compression=RLE\n");
    
    // Simple RLE compression
    int count = 0;
    float last_height = g_terrain.heights[0];
    
    for (int i = 0; i < g_terrain.size_x * g_terrain.size_z; i++) {
        if (fabsf(g_terrain.heights[i] - last_height) < 0.001f && count < 255) {
            count++;
        } else {
            fprintf(file, "%d,%.3f,", count, last_height);
            last_height = g_terrain.heights[i];
            count = 1;
        }
    }
    fprintf(file, "%d,%.3f\n", count, last_height);
    fprintf(file, "\n");
}

/**
 * Saves object data to file
 * @param file Output file
 */
void save_object_data(FILE* file)
{
    fprintf(file, "[OBJECTS]\n");
    fprintf(file, "count=%d\n", g_object_count);
    fprintf(file, "\n");
    
    for (int i = 0; i < g_object_count; i++) {
        EditorObject* obj = &g_editor_objects[i];
        
        fprintf(file, "[OBJECT_%d]\n", obj->id);
        fprintf(file, "type=%d\n", obj->type);
        fprintf(file, "name=%s\n", obj->name);
        fprintf(file, "position=%.3f,%.3f,%.3f\n", 
                obj->position[0], obj->position[1], obj->position[2]);
        fprintf(file, "rotation=%.3f,%.3f,%.3f\n", 
                obj->rotation[0], obj->rotation[1], obj->rotation[2]);
        fprintf(file, "scale=%.3f,%.3f,%.3f\n", 
                obj->scale[0], obj->scale[1], obj->scale[2]);
        fprintf(file, "layer=%d\n", obj->layer_id);
        fprintf(file, "parent=%d\n", obj->parent_id);
        fprintf(file, "visible=%d\n", obj->visible);
        fprintf(file, "locked=%d\n", obj->locked);
        fprintf(file, "static=%d\n", obj->static_object);
        
        if (obj->asset_id >= 0) {
            fprintf(file, "asset=%s\n", g_asset_cache[obj->asset_id].filename);
        }
        if (obj->material_id >= 0) {
            fprintf(file, "material=%d\n", obj->material_id);
        }
        
        // Save type-specific properties
        save_object_properties(file, obj);
        
        fprintf(file, "\n");
    }
}

/**
 * Saves object type-specific properties
 * @param file Output file
 * @param obj Object to save
 */
void save_object_properties(FILE* file, EditorObject* obj)
{
    switch (obj->type) {
        case EDITOR_OBJ_LIGHT:
            fprintf(file, "light_type=%d\n", obj->properties.light.light_type);
            fprintf(file, "light_color=%.3f,%.3f,%.3f\n", 
                    obj->properties.light.color[0], 
                    obj->properties.light.color[1], 
                    obj->properties.light.color[2]);
            fprintf(file, "light_intensity=%.3f\n", obj->properties.light.intensity);
            fprintf(file, "light_range=%.3f\n", obj->properties.light.range);
            fprintf(file, "cast_shadows=%d\n", obj->properties.light.cast_shadows);
            fprintf(file, "shadow_resolution=%d\n", obj->properties.light.shadow_resolution);
            break;
            
        case EDITOR_OBJ_SPAWN_POINT:
            fprintf(file, "spawn_team=%d\n", obj->properties.spawn.team);
            fprintf(file, "spawn_type=%d\n", obj->properties.spawn.spawn_type);
            fprintf(file, "spawn_radius=%.3f\n", obj->properties.spawn.spawn_radius);
            fprintf(file, "spawn_priority=%d\n", obj->properties.spawn.priority);
            break;
            
        case EDITOR_OBJ_TRIGGER:
            fprintf(file, "trigger_bounds=%.3f,%.3f,%.3f,%.3f,%.3f,%.3f\n",
                    obj->properties.trigger.bounds[0], obj->properties.trigger.bounds[1],
                    obj->properties.trigger.bounds[2], obj->properties.trigger.bounds[3],
                    obj->properties.trigger.bounds[4], obj->properties.trigger.bounds[5]);
            fprintf(file, "trigger_script=%s\n", obj->properties.trigger.script);
            fprintf(file, "trigger_once=%d\n", obj->properties.trigger.trigger_once);
            fprintf(file, "trigger_delay=%d\n", obj->properties.trigger.trigger_delay);
            break;
            
        // Add other object types as needed
    }
}

/**
 * Loads level from file
 * @param filename Input filename
 * @return 1 on success, 0 on failure
 */
int load_level(const char* filename)
{
    FILE* file = fopen(filename, "r");
    if (!file) {
        editor_log(2, "Failed to open level file: %s", filename);
        return 0;
    }
    
    editor_log(0, "Loading level: %s", filename);
    
    // Clear existing data
    clear_selection();
    g_object_count = 0;
    g_layer_count = 1;  // Keep default layer
    
    char line[1024];
    char section[64] = "";
    int current_object_id = -1;
    EditorObject* current_object = NULL;
    
    while (fgets(line, sizeof(line), file)) {
        // Remove newline
        char* newline = strchr(line, '\n');
        if (newline) *newline = '\0';
        
        // Skip empty lines and comments
        if (line[0] == '\0' || line[0] == '#' || line[0] == ';') {
            continue;
        }
        
        // Check for section header
        if (line[0] == '[') {
            char* end = strchr(line, ']');
            if (end) {
                *end = '\0';
                strncpy(section, line + 1, sizeof(section) - 1);
                
                // Check if it's an object section
                if (strncmp(section, "OBJECT_", 7) == 0) {
                    current_object_id = atoi(section + 7);
                    // Create new object
                    if (g_object_count < g_object_capacity) {
                        current_object = &g_editor_objects[g_object_count++];
                        memset(current_object, 0, sizeof(EditorObject));
                        current_object->id = current_object_id;
                        current_object->scale[0] = current_object->scale[1] = current_object->scale[2] = 1.0f;
                        current_object->visible = 1;
                        current_object->asset_id = -1;
                        current_object->material_id = -1;
                        current_object->parent_id = -1;
                    }
                } else {
                    current_object = NULL;
                }
            }
            continue;
        }
        
        // Parse key=value
        char* equals = strchr(line, '=');
        if (!equals) continue;
        
        *equals = '\0';
        char* key = line;
        char* value = equals + 1;
        
        // Process based on section
        if (strcmp(section, "TERRAIN") == 0) {
            load_terrain_property(key, value);
        } else if (current_object != NULL) {
            load_object_property(current_object, key, value);
        }
        // Add other sections as needed
    }
    
    fclose(file);
    
    // Update terrain normals
    update_terrain_normals(0, g_terrain.size_x - 1, 0, g_terrain.size_z - 1);
    
    // Update statistics
    g_stats.total_objects = g_object_count;
    g_stats.changes_since_save = 0;
    
    // Save current filename
    strncpy(g_current_level_file, filename, sizeof(g_current_level_file) - 1);
    
    editor_log(0, "Level loaded successfully");
    return 1;
}

/**
 * Loads terrain property from file
 * @param key Property key
 * @param value Property value
 */
void load_terrain_property(const char* key, const char* value)
{
    if (strcmp(key, "size") == 0) {
        int width, height;
        if (sscanf(value, "%d,%d", &width, &height) == 2) {
            resize_terrain(width, height);
        }
    } else if (strcmp(key, "scale") == 0) {
        g_terrain.scale = atof(value);
    } else if (strcmp(key, "vertical_scale") == 0) {
        g_terrain.vertical_scale = atof(value);
    }
    // Add other terrain properties
}

/**
 * Loads object property from file
 * @param obj Object to load into
 * @param key Property key
 * @param value Property value
 */
void load_object_property(EditorObject* obj, const char* key, const char* value)
{
    if (strcmp(key, "type") == 0) {
        obj->type = atoi(value);
    } else if (strcmp(key, "name") == 0) {
        strncpy(obj->name, value, sizeof(obj->name) - 1);
    } else if (strcmp(key, "position") == 0) {
        sscanf(value, "%f,%f,%f", &obj->position[0], &obj->position[1], &obj->position[2]);
    } else if (strcmp(key, "rotation") == 0) {
        sscanf(value, "%f,%f,%f", &obj->rotation[0], &obj->rotation[1], &obj->rotation[2]);
    } else if (strcmp(key, "scale") == 0) {
        sscanf(value, "%f,%f,%f", &obj->scale[0], &obj->scale[1], &obj->scale[2]);
    } else if (strcmp(key, "layer") == 0) {
        obj->layer_id = atoi(value);
    } else if (strcmp(key, "parent") == 0) {
        obj->parent_id = atoi(value);
    } else if (strcmp(key, "visible") == 0) {
        obj->visible = atoi(value);
    } else if (strcmp(key, "locked") == 0) {
        obj->locked = atoi(value);
    } else if (strcmp(key, "static") == 0) {
        obj->static_object = atoi(value);
    } else if (strcmp(key, "asset") == 0) {
        // Find or load asset
        for (int i = 0; i < g_asset_count; i++) {
            if (strcmp(g_asset_cache[i].filename, value) == 0) {
                obj->asset_id = i;
                g_asset_cache[i].reference_count++;
                break;
            }
        }
    } else {
        // Type-specific properties
        load_object_type_property(obj, key, value);
    }
    
    // Update bounds
    calculate_object_bounds(obj);
}

/**
 * Loads object type-specific property
 * @param obj Object to load into
 * @param key Property key
 * @param value Property value
 */
void load_object_type_property(EditorObject* obj, const char* key, const char* value)
{
    switch (obj->type) {
        case EDITOR_OBJ_LIGHT:
            if (strcmp(key, "light_type") == 0) {
                obj->properties.light.light_type = atoi(value);
            } else if (strcmp(key, "light_color") == 0) {
                sscanf(value, "%f,%f,%f", 
                       &obj->properties.light.color[0],
                       &obj->properties.light.color[1],
                       &obj->properties.light.color[2]);
            } else if (strcmp(key, "light_intensity") == 0) {
                obj->properties.light.intensity = atof(value);
            } else if (strcmp(key, "light_range") == 0) {
                obj->properties.light.range = atof(value);
            } else if (strcmp(key, "cast_shadows") == 0) {
                obj->properties.light.cast_shadows = atoi(value);
            } else if (strcmp(key, "shadow_resolution") == 0) {
                obj->properties.light.shadow_resolution = atoi(value);
            }
            break;
            
        case EDITOR_OBJ_SPAWN_POINT:
            if (strcmp(key, "spawn_team") == 0) {
                obj->properties.spawn.team = atoi(value);
            } else if (strcmp(key, "spawn_type") == 0) {
                obj->properties.spawn.spawn_type = atoi(value);
            } else if (strcmp(key, "spawn_radius") == 0) {
                obj->properties.spawn.spawn_radius = atof(value);
            } else if (strcmp(key, "spawn_priority") == 0) {
                obj->properties.spawn.priority = atoi(value);
            }
            break;
            
        // Add other object types
    }
}

/**
 * Checks and performs autosave
 */
void check_autosave(void)
{
    if (!g_config.autosave_enabled || g_stats.changes_since_save == 0) {
        return;
    }
    
    time_t now = time(NULL);
    if (now - g_last_autosave >= g_config.autosave_interval / 1000) {
        char autosave_file[512];
        if (strlen(g_current_level_file) > 0) {
            // Create autosave based on current file
            char* dot = strrchr(g_current_level_file, '.');
            if (dot) {
                *dot = '\0';
                sprintf(autosave_file, "%s_autosave.lvl", g_current_level_file);
                *dot = '.';
            } else {
                sprintf(autosave_file, "%s_autosave.lvl", g_current_level_file);
            }
        } else {
            sprintf(autosave_file, "autosave_%ld.lvl", now);
        }
        
        if (save_level(autosave_file)) {
            g_last_autosave = now;
            editor_log(0, "Autosaved to: %s", autosave_file);
        }
    }
}

/**
 * Exports optimized level for game use
 * @param filename Output filename
 * @return 1 on success, 0 on failure
 */
int export_level_optimized(const char* filename)
{
    FILE* file = fopen(filename, "wb");
    if (!file) {
        editor_log(2, "Failed to create export file: %s", filename);
        return 0;
    }
    
    editor_log(0, "Exporting optimized level: %s", filename);
    
    // Write binary header
    unsigned int magic = 0x4C444E45;  // "ENDL"
    unsigned int version = 0x00020000;  // 2.0
    fwrite(&magic, sizeof(unsigned int), 1, file);
    fwrite(&version, sizeof(unsigned int), 1, file);
    
    // TODO: Write optimized binary data
    // - Packed terrain data
    // - Optimized object placement
    // - Baked lighting data
    // - Collision data
    // - Navigation mesh
    
    fclose(file);
    
    editor_log(0, "Level exported successfully");
    return 1;
}

// ========================================================================
// MATERIAL SYSTEM
// ========================================================================

/**
 * Creates default materials
 */
void create_default_materials(void)
{
    // Default material
    create_material("Default", 0, 3, -1, -1);  // White diffuse, flat normal
    
    // Basic materials
    create_material("Stone", -1, -1, -1, -1);
    create_material("Metal", -1, -1, -1, -1);
    create_material("Wood", -1, -1, -1, -1);
    create_material("Glass", -1, -1, -1, -1);
}

/**
 * Creates a new material
 * @param name Material name
 * @param diffuse_tex Diffuse texture ID
 * @param normal_tex Normal texture ID
 * @param specular_tex Specular texture ID
 * @param emissive_tex Emissive texture ID
 * @return Material ID or -1 on failure
 */
int create_material(const char* name, int diffuse_tex, int normal_tex, 
                   int specular_tex, int emissive_tex)
{
    if (g_material_count >= g_material_capacity) {
        // Expand array
        int new_capacity = g_material_capacity * 2;
        Material* new_materials = (Material*)realloc(g_materials, 
                                                    new_capacity * sizeof(Material));
        if (!new_materials) {
            editor_log(2, "Failed to expand material array");
            return -1;
        }
        g_materials = new_materials;
        g_material_capacity = new_capacity;
    }
    
    Material* mat = &g_materials[g_material_count];
    memset(mat, 0, sizeof(Material));
    
    mat->diffuse_texture_id = diffuse_tex;
    mat->normal_texture_id = normal_tex;
    mat->specular_texture_id = specular_tex;
    mat->emissive_texture_id = emissive_tex;
    
    // Default colors
    mat->diffuse_color[0] = mat->diffuse_color[1] = mat->diffuse_color[2] = 1.0f;
    mat->diffuse_color[3] = 1.0f;
    mat->specular_color[0] = mat->specular_color[1] = mat->specular_color[2] = 0.5f;
    mat->specular_color[3] = 1.0f;
    mat->emissive_color[0] = mat->emissive_color[1] = mat->emissive_color[2] = 0.0f;
    mat->emissive_color[3] = 1.0f;
    
    // Default properties
    mat->roughness = 0.5f;
    mat->metallic = 0.0f;
    mat->opacity = 1.0f;
    mat->normal_strength = 1.0f;
    mat->blend_mode = 0;  // Opaque
    mat->two_sided = 0;
    mat->cast_shadows = 1;
    
    strncpy(mat->shader_name, "standard", sizeof(mat->shader_name) - 1);
    
    editor_log(0, "Created material: %s", name);
    return g_material_count++;
}

// ========================================================================
// UTILITY FUNCTIONS
// ========================================================================

/**
 * Gets current timestamp string
 * @return Timestamp string
 */
const char* get_timestamp(void)
{
    static char timestamp[32];
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_info);
    return timestamp;
}

/**
 * Gets filename without path
 * @param path Full path
 * @return Filename portion
 */
const char* get_filename_without_path(const char* path)
{
    const char* filename = strrchr(path, '\\');
    if (!filename) {
        filename = strrchr(path, '/');
    }
    return filename ? filename + 1 : path;
}

/**
 * Checks if a key is pressed
 * @param key Virtual key code
 * @return 1 if pressed, 0 otherwise
 */
int is_key_pressed(int key)
{
    return (GetAsyncKeyState(key) & 0x8000) != 0;
}

/**
 * Creates default prefabs
 */
void create_default_prefabs(void)
{
    // TODO: Create some default prefabs
    // - Basic light setup
    // - Spawn point setup
    // - Common object arrangements
}

/**
 * Loads editor configuration
 * @param filename Configuration filename
 */
void load_editor_config(const char* filename)
{
    FILE* file = fopen(filename, "r");
    if (!file) {
        editor_log(1, "No editor config found, using defaults");
        return;
    }
    
    char line[256];
    while (fgets(line, sizeof(line), file)) {
        char* newline = strchr(line, '\n');
        if (newline) *newline = '\0';
        
        char* equals = strchr(line, '=');
        if (!equals) continue;
        
        *equals = '\0';
        char* key = line;
        char* value = equals + 1;
        
        // Parse configuration values
        if (strcmp(key, "autosave_enabled") == 0) {
            g_config.autosave_enabled = atoi(value);
        } else if (strcmp(key, "autosave_interval") == 0) {
            g_config.autosave_interval = atoi(value);
        } else if (strcmp(key, "camera_speed") == 0) {
            g_config.camera_speed_multiplier = atof(value);
        } else if (strcmp(key, "grid_size") == 0) {
            g_config.grid_size = atof(value);
        } else if (strcmp(key, "snap_angle") == 0) {
            g_config.snap_angle = atof(value);
        }
        // Add other config options
    }
    
    fclose(file);
    editor_log(0, "Loaded editor configuration");
}

/**
 * Saves editor configuration
 * @param filename Configuration filename
 */
void save_editor_config(const char* filename)
{
    FILE* file = fopen(filename, "w");
    if (!file) {
        editor_log(2, "Failed to save editor config");
        return;
    }
    
    fprintf(file, "# Endor Level Editor Configuration\n");
    fprintf(file, "autosave_enabled=%d\n", g_config.autosave_enabled);
    fprintf(file, "autosave_interval=%d\n", g_config.autosave_interval);
    fprintf(file, "camera_speed=%.3f\n", g_config.camera_speed_multiplier);
    fprintf(file, "grid_size=%.3f\n", g_config.grid_size);
    fprintf(file, "snap_angle=%.3f\n", g_config.snap_angle);
    fprintf(file, "show_grid=%d\n", g_config.show_grid);
    fprintf(file, "show_gizmos=%d\n", g_config.show_gizmos);
    fprintf(file, "show_statistics=%d\n", g_config.show_statistics);
    
    fclose(file);
    editor_log(0, "Saved editor configuration");
}

/**
 * Updates list of visible objects
 */
void update_visible_objects(void)
{
    g_visible_object_count = 0;
    
    for (int i = 0; i < g_object_count; i++) {
        EditorObject* obj = &g_editor_objects[i];
        
        // Check visibility
        if (!obj->visible || !g_layers[obj->layer_id].visible) {
            continue;
        }
        
        // TODO: Frustum culling
        if (g_frustum_cull_enabled) {
            // Check if object is in view frustum
        }
        
        // Add to visible list
        if (g_visible_object_count < g_object_capacity) {
            g_visible_objects[g_visible_object_count++] = i;
        }
    }
    
    g_stats.visible_objects = g_visible_object_count;
}

/**
 * Cleans up the level editor
 */
void cleanup_level_editor(void)
{
    if (!g_editor_initialized) {
        return;
    }
    
    editor_log(0, "Cleaning up level editor");
    
    // Save configuration
    save_editor_config("editor_config.ini");
    
    // Free dynamic memory
    if (g_editor_objects) {
        // Free type-specific data
        for (int i = 0; i < g_object_count; i++) {
            if (g_editor_objects[i].type == EDITOR_OBJ_SPLINE && 
                g_editor_objects[i].properties.spline.control_points) {
                free(g_editor_objects[i].properties.spline.control_points);
            }
            if (g_editor_objects[i].custom_properties) {
                free(g_editor_objects[i].custom_properties);
            }
        }
        free(g_editor_objects);
    }
    
    if (g_asset_cache) {
        // Free asset data
        for (int i = 0; i < g_asset_count; i++) {
            if (g_asset_cache[i].data) {
                free(g_asset_cache[i].data);
            }
            if (g_asset_cache[i].thumbnail_data) {
                free(g_asset_cache[i].thumbnail_data);
            }
            // Free dependency list
            AssetDependency* dep = g_asset_cache[i].dependencies;
            while (dep) {
                AssetDependency* next = dep->next;
                free(dep);
                dep = next;
            }
        }
        free(g_asset_cache);
    }
    
    if (g_materials) {
        free(g_materials);
    }
    
    if (g_undo_stack) {
        // Free undo actions
        for (int i = 0; i < g_undo_capacity; i++) {
            free_undo_action(&g_undo_stack[i]);
        }
        free(g_undo_stack);
    }
    
    if (g_visible_objects) {
        free(g_visible_objects);
    }
    
    if (g_selection.object_ids) {
        free(g_selection.object_ids);
    }
    
    if (g_selection.lasso_points) {
        free(g_selection.lasso_points);
    }
    
    // Free terrain data
    if (g_terrain.heights) free(g_terrain.heights);
    if (g_terrain.normals) free(g_terrain.normals);
    if (g_terrain.blend_weights) free(g_terrain.blend_weights);
    if (g_terrain.vegetation_map) free(g_terrain.vegetation_map);
    if (g_terrain.water_map) free(g_terrain.water_map);
    if (g_terrain.chunks) free(g_terrain.chunks);
    
    g_editor_initialized = 0;
    
    editor_log(0, "Level editor cleanup complete");
}

// ========================================================================
// PUBLIC API
// ========================================================================

/**
 * Main editor update function
 * @param delta_time Frame delta time in seconds
 */
void update_level_editor(float delta_time)
{
    if (!g_editor_initialized) {
        return;
    }
    
    // Update camera
    update_editor_camera(delta_time);
    
    // Update visible objects
    update_visible_objects();
    
    // Check for hot-reload
    static float hot_reload_timer = 0.0f;
    hot_reload_timer += delta_time;
    if (hot_reload_timer > 1.0f) {
        hot_reload_timer = 0.0f;
        for (int i = 0; i < g_asset_count; i++) {
            check_asset_hot_reload(i);
        }
    }
    
    // Check autosave
    check_autosave();
    
    // Update statistics
    g_stats.last_frame_time = delta_time;
}

/**
 * Renders the level editor
 */
void render_level_editor(void)
{
    if (!g_editor_initialized) {
        return;
    }
    
    // TODO: Implement rendering
    // - Render terrain
    // - Render objects
    // - Render selection
    // - Render gizmos
    // - Render grid
    // - Render UI
}

/**
 * Handles editor input
 * @param msg Windows message
 * @param wparam WPARAM
 * @param lparam LPARAM
 * @return 1 if handled, 0 otherwise
 */
int handle_editor_input(UINT msg, WPARAM wparam, LPARAM lparam)
{
    if (!g_editor_initialized) {
        return 0;
    }
    
    switch (msg) {
        case WM_KEYDOWN:
            switch (wparam) {
                case 'Z':
                    if (GetAsyncKeyState(VK_CONTROL) & 0x8000) {
                        if (GetAsyncKeyState(VK_SHIFT) & 0x8000) {
                            redo();
                        } else {
                            undo();
                        }
                        return 1;
                    }
                    break;
                    
                case 'S':
                    if (GetAsyncKeyState(VK_CONTROL) & 0x8000) {
                        if (strlen(g_current_level_file) > 0) {
                            save_level(g_current_level_file);
                        }
                        return 1;
                    }
                    break;
                    
                case VK_DELETE:
                    if (g_selection.count > 0) {
                        begin_undo_group("Delete Selection");
                        for (int i = g_selection.count - 1; i >= 0; i--) {
                            delete_editor_object(g_selection.object_ids[i]);
                        }
                        end_undo_group();
                        return 1;
                    }
                    break;
                    
                case VK_F2:
                    focus_camera_on_selection();
                    return 1;
            }
            break;
            
        case WM_LBUTTONDOWN:
        case WM_RBUTTONDOWN:
        case WM_MBUTTONDOWN:
            // Handle mouse input
            break;
            
        case WM_MOUSEWHEEL:
            {
                short wheel_delta = GET_WHEEL_DELTA_WPARAM(wparam);
                camera_mouse_input(0, 0, 0, wheel_delta / WHEEL_DELTA);
                return 1;
            }
            break;
    }
    
    return 0;
};
    
    // Use target height or sample from center
    float target_height = g_terrain_brush.target_height;
    if (target_height == 0.0f) {
        target_height = g_terrain.heights[center_z * g_terrain.size_x + center_x];
    }
    
    for (int z = min_z; z <= max_z; z++) {
        for (int x = min_x; x <= max_x; x++) {
            float dx = (x - center_x) * g_terrain.scale;
            float dz = (z - center_z) * g_terrain.scale;
            float distance = sqrtf(dx * dx + dz * dz);
            
            if (distance <= g_terrain_brush.radius) {
                float influence = 1.0f - (distance / g_terrain_brush.radius);
                influence = powf(influence, g_terrain_brush.falloff) * g_terrain_brush.strength;
                
                int idx = z * g_terrain.size_x + x;
                g_terrain.heights[idx] = g_terrain.heights[idx] + 
                                        (target_height - g_terrain.heights[idx]) * influence;
            }
        }
    }
}

/**
 * Applies noise brush
 */
void apply_terrain_brush_noise(int center_x, int center_z, 
                              int min_x, int max_x, int min_z, int max_z, int apply)
{
    if (!apply) return;
    
    // Simple erosion simulation
    float* water_flow = (float*)calloc((max_x - min_x + 1) * (max_z - min_z + 1), sizeof(float));
    float* sediment = (float*)calloc((max_x - min_x + 1) * (max_z - min_z + 1), sizeof(float));
    
    // Simulate water flow
    for (int iteration = 0; iteration < 10; iteration++) {
        for (int z = min_z; z <= max_z; z++) {
            for (int x = min_x; x <= max_x; x++) {
                float dx = (x - center_x) * g_terrain.scale;
                float dz = (z - center_z) * g_terrain.scale;
                float distance = sqrtf(dx * dx + dz * dz);
                
                if (distance <= g_terrain_brush.radius) {
                    int local_idx = (z - min_z) * (max_x - min_x + 1) + (x - min_x);
                    int terrain_idx = z * g_terrain.size_x + x;
                    
                    // Calculate gradient
                    float gradient_x = 0.0f;
                    float gradient_z = 0.0f;
                    
                    if (x > 0 && x < g_terrain.size_x - 1) {
                        gradient_x = g_terrain.heights[terrain_idx + 1] - 
                                    g_terrain.heights[terrain_idx - 1];
                    }
                    if (z > 0 && z < g_terrain.size_z - 1) {
                        gradient_z = g_terrain.heights[(z + 1) * g_terrain.size_x + x] - 
                                    g_terrain.heights[(z - 1) * g_terrain.size_x + x];
                    }
                    
                    // Water flows downhill
                    float flow_strength = sqrtf(gradient_x * gradient_x + gradient_z * gradient_z);
                    water_flow[local_idx] += flow_strength * g_terrain_brush.flow_rate;
                    
                    // Erosion
                    float erosion = water_flow[local_idx] * g_terrain_brush.strength * 0.1f;
                    g_terrain.heights[terrain_idx] -= erosion;
                    sediment[local_idx] += erosion;
                    
                    // Deposition
                    float deposition = sediment[local_idx] * g_terrain_brush.deposition_rate;
                    g_terrain.heights[terrain_idx] += deposition;
                    sediment[local_idx] -= deposition;
                }
            }
        }
    }
    
    free(water_flow);
    free(sediment);
}

/**
 * Paints terrain texture
 * @param world_x World X coordinate
 * @param world_z World Z coordinate
 * @param texture_layer Texture layer to paint
 */
void paint_terrain_texture(float world_x, float world_z, int texture_layer)
{
    if (texture_layer < 0 || texture_layer >= 8) {
        return;
    }
    
    // Convert world to terrain coordinates
    int center_x = (int)((world_x / g_terrain.scale) + g_terrain.size_x * 0.5f);
    int center_z = (int)((world_z / g_terrain.scale) + g_terrain.size_z * 0.5f);
    
    if (center_x < 0 || center_x >= g_terrain.size_x || 
        center_z < 0 || center_z >= g_terrain.size_z) {
        return;
    }
    
    // Calculate affected region
    int radius_cells = (int)(g_terrain_brush.radius / g_terrain.scale) + 1;
    int min_x = fmaxf(0, center_x - radius_cells);
    int max_x = fminf(g_terrain.size_x - 1, center_x + radius_cells);
    int min_z = fmaxf(0, center_z - radius_cells);
    int max_z = fminf(g_terrain.size_z - 1, center_z + radius_cells);
    
    for (int z = min_z; z <= max_z; z++) {
        for (int x = min_x; x <= max_x; x++) {
            float dx = (x - center_x) * g_terrain.scale;
            float dz = (z - center_z) * g_terrain.scale;
            float distance = sqrtf(dx * dx + dz * dz);
            
            if (distance <= g_terrain_brush.radius) {
                float influence = 1.0f - (distance / g_terrain_brush.radius);
                influence = powf(influence, g_terrain_brush.falloff) * g_terrain_brush.strength;
                
                int idx = z * g_terrain.size_x + x;
                
                // Update blend weights
                float* weights = &g_terrain.blend_weights[idx * 8];
                
                // Increase target layer
                weights[texture_layer] += influence;
                weights[texture_layer] = fminf(1.0f, weights[texture_layer]);
                
                // Normalize weights
                float total = 0.0f;
                for (int i = 0; i < 8; i++) {
                    total += weights[i];
                }
                
                if (total > 0.0f) {
                    for (int i = 0; i < 8; i++) {
                        weights[i] /= total;
                    }
                }
            }
        }
    }
    
    // Mark chunks as dirty
    mark_terrain_chunks_dirty(min_x, max_x, min_z, max_z);
}

/**
 * Updates terrain normals for a region
 * @param min_x Minimum X coordinate
 * @param max_x Maximum X coordinate
 * @param min_z Minimum Z coordinate
 * @param max_z Maximum Z coordinate
 */
void update_terrain_normals(int min_x, int max_x, int min_z, int max_z)
{
    // Expand region by 1 to include neighbors
    min_x = fmaxf(1, min_x - 1);
    max_x = fminf(g_terrain.size_x - 2, max_x + 1);
    min_z = fmaxf(1, min_z - 1);
    max_z = fminf(g_terrain.size_z - 2, max_z + 1);
    
    for (int z = min_z; z <= max_z; z++) {
        for (int x = min_x; x <= max_x; x++) {
            int idx = z * g_terrain.size_x + x;
            
            // Calculate normal from neighboring heights
            float h_left = g_terrain.heights[idx - 1];
            float h_right = g_terrain.heights[idx + 1];
            float h_down = g_terrain.heights[(z - 1) * g_terrain.size_x + x];
            float h_up = g_terrain.heights[(z + 1) * g_terrain.size_x + x];
            
            // Cross product of tangent vectors
            float dx = 2.0f * g_terrain.scale;
            float dy_x = h_right - h_left;
            float dy_z = h_up - h_down;
            
            // Normal = (dx, dy_x, 0) x (0, dy_z, dx)
            float nx = -dy_x * g_terrain.vertical_scale;
            float ny = dx * g_terrain.vertical_scale;
            float nz = -dy_z * g_terrain.vertical_scale;
            
            // Normalize
            float length = sqrtf(nx * nx + ny * ny + nz * nz);
            if (length > 0.0f) {
                g_terrain.normals[idx * 3] = nx / length;
                g_terrain.normals[idx * 3 + 1] = ny / length;
                g_terrain.normals[idx * 3 + 2] = nz / length;
            } else {
                g_terrain.normals[idx * 3] = 0.0f;
                g_terrain.normals[idx * 3 + 1] = 1.0f;
                g_terrain.normals[idx * 3 + 2] = 0.0f;
            }
        }
    }
}

/**
 * Marks terrain chunks as dirty
 * @param min_x Minimum X coordinate
 * @param max_x Maximum X coordinate
 * @param min_z Minimum Z coordinate
 * @param max_z Maximum Z coordinate
 */
void mark_terrain_chunks_dirty(int min_x, int max_x, int min_z, int max_z)
{
    int chunk_min_x = min_x / TERRAIN_CHUNK_SIZE;
    int chunk_max_x = max_x / TERRAIN_CHUNK_SIZE;
    int chunk_min_z = min_z / TERRAIN_CHUNK_SIZE;
    int chunk_max_z = max_z / TERRAIN_CHUNK_SIZE;
    
    for (int cz = chunk_min_z; cz <= chunk_max_z; cz++) {
        for (int cx = chunk_min_x; cx <= chunk_max_x; cx++) {
            if (cx >= 0 && cx < g_terrain.chunk_count_x &&
                cz >= 0 && cz < g_terrain.chunk_count_z) {
                g_terrain.chunks[cz * g_terrain.chunk_count_x + cx].dirty = 1;
            }
        }
    }
}

/**
 * Loads terrain heightmap from file
 * @param filename Heightmap filename
 * @return 1 on success, 0 on failure
 */
int load_terrain_heightmap(const char* filename)
{
    FILE* file = fopen(filename, "rb");
    if (!file) {
        editor_log(2, "Failed to open heightmap: %s", filename);
        return 0;
    }
    
    // Read header (assuming raw float format)
    int width, height;
    fread(&width, sizeof(int), 1, file);
    fread(&height, sizeof(int), 1, file);
    
    if (width != g_terrain.size_x || height != g_terrain.size_z) {
        editor_log(1, "Heightmap size mismatch: %dx%d vs %dx%d", 
                  width, height, g_terrain.size_x, g_terrain.size_z);
        fclose(file);
        return 0;
    }
    
    // Read height data
    fread(g_terrain.heights, sizeof(float), width * height, file);
    fclose(file);
    
    // Update normals for entire terrain
    update_terrain_normals(0, g_terrain.size_x - 1, 0, g_terrain.size_z - 1);
    
    // Mark all chunks as dirty
    for (int i = 0; i < g_terrain.chunk_count_x * g_terrain.chunk_count_z; i++) {
        g_terrain.chunks[i].dirty = 1;
    }
    
    editor_log(0, "Loaded heightmap: %s", filename);
    return 1;
}

/**
 * Exports terrain heightmap to file
 * @param filename Output filename
 * @return 1 on success, 0 on failure
 */
int export_terrain_heightmap(const char* filename)
{
    FILE* file = fopen(filename, "wb");
    if (!file) {
        editor_log(2, "Failed to create heightmap file: %s", filename);
        return 0;
    }
    
    // Write header
    fwrite(&g_terrain.size_x, sizeof(int), 1, file);
    fwrite(&g_terrain.size_z, sizeof(int), 1, file);
    
    // Write height data
    fwrite(g_terrain.heights, sizeof(float), 
           g_terrain.size_x * g_terrain.size_z, file);
    
    fclose(file);
    
    editor_log(0, "Exported heightmap: %s", filename);
    return 1;
}

/**
 * Resizes terrain
 * @param new_width New terrain width
 * @param new_height New terrain height
 */
void resize_terrain(int new_width, int new_height)
{
    if (new_width == g_terrain.size_x && new_height == g_terrain.size_z) {
        return;
    }
    
    // Allocate new arrays
    int new_size = new_width * new_height;
    float* new_heights = (float*)calloc(new_size, sizeof(float));
    float* new_normals = (float*)calloc(new_size * 3, sizeof(float));
    float* new_blend_weights = (float*)calloc(new_size * 8, sizeof(float));
    
    // Copy existing data (with bilinear sampling for scaling)
    for (int z = 0; z < new_height; z++) {
        for (int x = 0; x < new_width; x++) {
            float src_x = (x * (g_terrain.size_x - 1)) / (float)(new_width - 1);
            float src_z = (z * (g_terrain.size_z - 1)) / (float)(new_height - 1);
            
            int x0 = (int)src_x;
            int z0 = (int)src_z;
            int x1 = fminf(x0 + 1, g_terrain.size_x - 1);
            int z1 = fminf(z0 + 1, g_terrain.size_z - 1);
            
            float fx = src_x - x0;
            float fz = src_z - z0;
            
            // Bilinear interpolation
            float h00 = g_terrain.heights[z0 * g_terrain.size_x + x0];
            float h10 = g_terrain.heights[z0 * g_terrain.size_x + x1];
            float h01 = g_terrain.heights[z1 * g_terrain.size_x + x0];
            float h11 = g_terrain.heights[z1 * g_terrain.size_x + x1];
            
            float h0 = h00 * (1 - fx) + h10 * fx;
            float h1 = h01 * (1 - fx) + h11 * fx;
            float h = h0 * (1 - fz) + h1 * fz;
            
            new_heights[z * new_width + x] = h;
            
            // Copy blend weights
            for (int i = 0; i < 8; i++) {
                new_blend_weights[(z * new_width + x) * 8 + i] = 
                    g_terrain.blend_weights[(z0 * g_terrain.size_x + x0) * 8 + i];
            }
        }
    }
    
    // Free old arrays
    free(g_terrain.heights);
    free(g_terrain.normals);
    free(g_terrain.blend_weights);
    free(g_terrain.vegetation_map);
    free(g_terrain.water_map);
    free(g_terrain.chunks);
    
    // Update terrain data
    g_terrain.size_x = new_width;
    g_terrain.size_z = new_height;
    g_terrain.heights = new_heights;
    g_terrain.normals = new_normals;
    g_terrain.blend_weights = new_blend_weights;
    g_terrain.vegetation_map = (float*)calloc(new_size, sizeof(float));
    g_terrain.water_map = (float*)calloc(new_size, sizeof(float));
    
    // Recreate chunks
    g_terrain.chunk_count_x = (new_width + TERRAIN_CHUNK_SIZE - 1) / TERRAIN_CHUNK_SIZE;
    g_terrain.chunk_count_z = (new_height + TERRAIN_CHUNK_SIZE - 1) / TERRAIN_CHUNK_SIZE;
    g_terrain.chunks = (TerrainChunk*)calloc(g_terrain.chunk_count_x * g_terrain.chunk_count_z, 
                                             sizeof(TerrainChunk));
    
    // Update normals
    update_terrain_normals(0, new_width - 1, 0, new_height - 1);
    
    editor_log(0, "Resized terrain to %dx%d", new_width, new_height);
}

// ========================================================================
// LAYER SYSTEM
// ========================================================================

/**
 * Creates a new layer
 * @param name Layer name
 * @param parent_id Parent layer ID (-1 for root)
 * @return Layer ID or -1 on failure
 */
int create_layer(const char* name, int parent_id)
{
    if (g_layer_count >= MAX_LAYERS) {
        editor_log(2, "Maximum layer count reached");
        return -1;
    }
    
    EditorLayer* layer = &g_layers[g_layer_count];
    strncpy(layer->name, name, sizeof(layer->name) - 1);
    layer->visible = 1;
    layer->locked = 0;
    layer->selectable = 1;
    layer->color[0] = 0.5f + (rand() % 50) / 100.0f;
    layer->color[1] = 0.5f + (rand() % 50) / 100.0f;
    layer->color[2] = 0.5f + (rand() % 50) / 100.0f;
    layer->color[3] = 1.0f;
    layer->object_count = 0;
    layer->parent_layer = parent_id;
    
    editor_log(0, "Created layer: %s", name);
    return g_layer_count++;
}

/**
 * Deletes a layer
 * @param layer_id Layer ID to delete
 */
void delete_layer(int layer_id)
{
    if (layer_id <= 0 || layer_id >= g_layer_count) {
        return;  // Can't delete default layer
    }
    
    // Move all objects to default layer
    for (int i = 0; i < g_object_count; i++) {
        if (g_editor_objects[i].layer_id == layer_id) {
            g_editor_objects[i].layer_id = 0;
        }
    }
    
    // Update child layers
    for (int i = 0; i < g_layer_count; i++) {
        if (g_layers[i].parent_layer == layer_id) {
            g_layers[i].parent_layer = -1;
        }
    }
    
    editor_log(0, "Deleted layer: %s", g_layers[layer_id].name);
    
    // Shift layers down
    for (int i = layer_id; i < g_layer_count - 1; i++) {
        g_layers[i] = g_layers[i + 1];
    }
    g_layer_count--;
    
    // Update object layer references
    for (int i = 0; i < g_object_count; i++) {
        if (g_editor_objects[i].layer_id > layer_id) {
            g_editor_objects[i].layer_id--;
        }
    }
}

/**
 * Sets layer visibility
 * @param layer_id Layer ID
 * @param visible Visibility state
 */
void set_layer_visibility(int layer_id, int visible)
{
    if (layer_id < 0 || layer_id >= g_layer_count) {
        return;
    }
    
    g_layers[layer_id].visible = visible;
    
    // Update visible objects
    update_visible_objects();
    
    editor_log(0, "Set layer %s visibility to %d", 
              g_layers[layer_id].name, visible);
}

/**
 * Moves objects to a layer
 * @param object_ids Array of object IDs
 * @param count Number of objects
 * @param target_layer Target layer ID
 */
void move_objects_to_layer(int* object_ids, int count, int target_layer)
{
    if (target_layer < 0 || target_layer >= g_layer_count) {
        return;
    }
    
    begin_undo_group("Move Objects to Layer");
    
    for (int i = 0; i < count; i++) {
        EditorObject* obj = find_editor_object(object_ids[i]);
        if (obj) {
            EditorObject old_state = *obj;
            
            // Update layer counts
            if (obj->layer_id < g_layer_count) {
                g_layers[obj->layer_id].object_count--;
            }
            g_layers[target_layer].object_count++;
            
            obj->layer_id = target_layer;
            
            add_undo_action(UNDO_TYPE_OBJECT_MODIFY, obj->id, &old_state, obj);
        }
    }
    
    end_undo_group();
    
    editor_log(0, "Moved %d objects to layer %s", count, g_layers[target_layer].name);
}

// ========================================================================
// PREFAB SYSTEM
// ========================================================================

/**
 * Creates a prefab from selection
 * @param name Prefab name
 * @param description Prefab description
 * @return Prefab ID or -1 on failure
 */
int create_prefab_from_selection(const char* name, const char* description)
{
    if (g_selection.count == 0) {
        editor_log(1, "No objects selected for prefab creation");
        return -1;
    }
    
    if (g_prefab_count >= MAX_PREFABS) {
        editor_log(2, "Maximum prefab count reached");
        return -1;
    }
    
    Prefab* prefab = &g_prefabs[g_prefab_count];
    strncpy(prefab->name, name, sizeof(prefab->name) - 1);
    strncpy(prefab->description, description, sizeof(prefab->description) - 1);
    
    // Copy selected object IDs
    prefab->object_count = fminf(g_selection.count, MAX_SELECTION);
    for (int i = 0; i < prefab->object_count; i++) {
        prefab->object_ids[i] = g_selection.object_ids[i];
    }
    
    // Calculate bounds
    memcpy(prefab->bounds_min, g_selection.bounds_min, sizeof(float) * 3);
    memcpy(prefab->bounds_max, g_selection.bounds_max, sizeof(float) * 3);
    
    // Set metadata
    prefab->creation_date = time(NULL);
    strncpy(prefab->author, "Editor User", sizeof(prefab->author) - 1);
    sprintf(prefab->tags, "objects:%d", prefab->object_count);
    
    // Generate thumbnail path
    sprintf(prefab->thumbnail_path, "thumbnails/prefab_%d.bmp", g_prefab_count);
    
    // TODO: Generate prefab thumbnail
    
    editor_log(0, "Created prefab: %s with %d objects", name, prefab->object_count);
    return g_prefab_count++;
}

/**
 * Instantiates a prefab
 * @param prefab_id Prefab ID
 * @param x X position
 * @param y Y position
 * @param z Z position
 * @return Root object ID or -1 on failure
 */
int instantiate_prefab(int prefab_id, float x, float y, float z)
{
    if (prefab_id < 0 || prefab_id >= g_prefab_count) {
        return -1;
    }
    
    Prefab* prefab = &g_prefabs[prefab_id];
    
    // Calculate offset from prefab center
    float prefab_center[3] = {
        (prefab->bounds_min[0] + prefab->bounds_max[0]) * 0.5f,
        (prefab->bounds_min[1] + prefab->bounds_max[1]) * 0.5f,
        (prefab->bounds_min[2] + prefab->bounds_max[2]) * 0.5f
    };
    
    float offset[3] = {
        x - prefab_center[0],
        y - prefab_center[1],
        z - prefab_center[2]
    };
    
    // Clear selection
    clear_selection();
    
    begin_undo_group("Instantiate Prefab");
    
    // Create prefab instance parent
    int parent_id = create_editor_object(EDITOR_OBJ_PREFAB_INSTANCE, x, y, z);
    if (parent_id < 0) {
        end_undo_group();
        return -1;
    }
    
    EditorObject* parent = &g_editor_objects[parent_id - 1];
    sprintf(parent->name, "%s_Instance", prefab->name);
    parent->properties.prefab.prefab_id = prefab_id;
    parent->properties.prefab.instance_seed = (float)rand() / RAND_MAX;
    
    // Duplicate all prefab objects
    for (int i = 0; i < prefab->object_count; i++) {
        EditorObject* template_obj = find_editor_object(prefab->object_ids[i]);
        if (!template_obj) continue;
        
        int new_id = create_editor_object(template_obj->type,
                                         template_obj->position[0] + offset[0],
                                         template_obj->position[1] + offset[1],
                                         template_obj->position[2] + offset[2]);
        
        if (new_id >= 0) {
            EditorObject* new_obj = &g_editor_objects[new_id - 1];
            
            // Copy properties
            memcpy(new_obj->rotation, template_obj->rotation, sizeof(float) * 3);
            memcpy(new_obj->scale, template_obj->scale, sizeof(float) * 3);
            new_obj->asset_id = template_obj->asset_id;
            new_obj->material_id = template_obj->material_id;
            new_obj->parent_id = parent_id;
            
            // Update asset reference count
            if (new_obj->asset_id >= 0) {
                g_asset_cache[new_obj->asset_id].reference_count++;
            }
            
            add_to_selection(new_id);
        }
    }
    
    end_undo_group();
    
    editor_log(0, "Instantiated prefab: %s at (%.2f, %.2f, %.2f)", 
              prefab->name, x, y, z);
    
    return parent_id;
}

// ========================================================================
// UNDO/REDO SYSTEM
// ========================================================================

/**
 * Begins an undo group
 * @param description Group description
 */
void begin_undo_group(const char* description)
{
    g_undo_group_depth++;
    
    if (g_undo_group_depth == 1) {
        // Create group action
        UndoAction* group = (UndoAction*)calloc(1, sizeof(UndoAction));
        group->type = UNDO_TYPE_BATCH;
        strncpy(group->description, description, sizeof(group->description) - 1);
        group->timestamp = time(NULL);
        
        add_undo_action_data(group);
    }
}

/**
 * Ends an undo group
 */
void end_undo_group(void)
{
    g_undo_group_depth--;
    if (g_undo_group_depth < 0) {
        g_undo_group_depth = 0;
    }
}

/**
 * Adds an undo action
 * @param type Action type
 * @param object_id Object ID
 * @param old_state Old object state
 * @param new_state New object state
 */
void add_undo_action(UndoType type, int object_id, EditorObject* old_state, EditorObject* new_state)
{
    UndoAction* action = (UndoAction*)calloc(1, sizeof(UndoAction));
    action->type = type;
    action->timestamp = time(NULL);
    
    // Set description based on type
    switch (type) {
        case UNDO_TYPE_OBJECT_CREATE:
            sprintf(action->description, "Create %s", new_state ? new_state->name : "Object");
            break;
        case UNDO_TYPE_OBJECT_DELETE:
            sprintf(action->description, "Delete %s", old_state ? old_state->name : "Object");
            break;
        case UNDO_TYPE_OBJECT_MODIFY:
            sprintf(action->description, "Modify %s", old_state ? old_state->name : "Object");
            break;
        default:
            strcpy(action->description, "Unknown Action");
            break;
    }
    
    // Store object data
    action->object_count = 1;
    action->object_ids = (int*)malloc(sizeof(int));
    action->object_ids[0] = object_id;
    
    if (old_state) {
        action->old_states = (EditorObject*)malloc(sizeof(EditorObject));
        memcpy(action->old_states, old_state, sizeof(EditorObject));
    }
    
    if (new_state) {
        action->new_states = (EditorObject*)malloc(sizeof(EditorObject));
        memcpy(action->new_states, new_state, sizeof(EditorObject));
    }
    
    add_undo_action_data(action);
}

/**
 * Adds undo action data to stack
 * @param action Action to add
 */
void add_undo_action_data(UndoAction* action)
{
    // Remove any redo actions
    while (g_undo_count > g_undo_head) {
        g_undo_count--;
        int idx = (g_undo_tail + g_undo_count) % g_undo_capacity;
        free_undo_action(&g_undo_stack[idx]);
    }
    
    // Check capacity
    if (g_undo_count >= g_config.max_undo_steps) {
        // Remove oldest action
        free_undo_action(&g_undo_stack[g_undo_tail]);
        g_undo_tail = (g_undo_tail + 1) % g_undo_capacity;
        g_undo_count--;
    }
    
    // Add new action
    int idx = (g_undo_tail + g_undo_head) % g_undo_capacity;
    
    // If we're in a group, add as sub-action
    if (g_undo_group_depth > 0 && g_undo_head > 0) {
        int group_idx = (g_undo_tail + g_undo_head - 1) % g_undo_capacity;
        UndoAction* group = &g_undo_stack[group_idx];
        
        if (group->type == UNDO_TYPE_BATCH) {
            // Add as sub-action
            group->sub_action_count++;
            group->sub_actions = (UndoAction*)realloc(group->sub_actions, 
                                                      group->sub_action_count * sizeof(UndoAction));
            memcpy(&group->sub_actions[group->sub_action_count - 1], action, sizeof(UndoAction));
            free(action);
            return;
        }
    }
    
    // Add as top-level action
    memcpy(&g_undo_stack[idx], action, sizeof(UndoAction));
    free(action);
    
    g_undo_head++;
    g_undo_count++;
}

/**
 * Performs undo
 */
void undo(void)
{
    if (g_undo_head == 0) {
        editor_log(1, "Nothing to undo");
        return;
    }
    
    g_undo_head--;
    int idx = (g_undo_tail + g_undo_head) % g_undo_capacity;
    UndoAction* action = &g_undo_stack[idx];
    
    editor_log(0, "Undo: %s", action->description);
    apply_undo_action(action, 1);
    
    g_stats.changes_since_save++;
}

/**
 * Performs redo
 */
void redo(void)
{
    if (g_undo_head >= g_undo_count) {
        editor_log(1, "Nothing to redo");
        return;
    }
    
    int idx = (g_undo_tail + g_undo_head) % g_undo_capacity;
    UndoAction* action = &g_undo_stack[idx];
    
    editor_log(0, "Redo: %s", action->description);
    apply_undo_action(action, 0);
    
    g_undo_head++;
    g_stats.changes_since_save++;
}

/**
 * Applies an undo action
 * @param action Action to apply
 * @param is_undo Whether this is an undo (vs redo)
 */
void apply_undo_action(UndoAction* action, int is_undo)
{
    switch (action->type) {
        case UNDO_TYPE_OBJECT_CREATE:
            if (is_undo) {
                // Delete the created object
                delete_editor_object(action->object_ids[0]);
            } else {
                // Recreate the object
                if (action->new_states) {
                    // Find a free slot
                    if (g_object_count < g_object_capacity) {
                        memcpy(&g_editor_objects[g_object_count], 
                               action->new_states, sizeof(EditorObject));
                        g_object_count++;
                    }
                }
            }
            break;
            
        case UNDO_TYPE_OBJECT_DELETE:
            if (is_undo) {
                // Recreate the deleted object
                if (action->old_states && g_object_count < g_object_capacity) {
                    memcpy(&g_editor_objects[g_object_count], 
                           action->old_states, sizeof(EditorObject));
                    g_object_count++;
                }
            } else {
                // Delete the object again
                delete_editor_object(action->object_ids[0]);
            }
            break;
            
        case UNDO_TYPE_OBJECT_MODIFY:
            {
                EditorObject* obj = find_editor_object(action->object_ids[0]);
                if (obj) {
                    EditorObject temp = *obj;
                    if (is_undo && action->old_states) {
                        *obj = *action->old_states;
                    } else if (!is_undo && action->new_states) {
                        *obj = *action->new_states;
                    }
                }
            }
            break;
            
        case UNDO_TYPE_TERRAIN_MODIFY:
            if (action->terrain_backup) {
                // Swap terrain data
                int x1 = action->terrain_region[0];
                int z1 = action->terrain_region[1];
                int x2 = action->terrain_region[2];
                int z2 = action->terrain_region[3];
                int region_width = x2 - x1 + 1;
                
                for (int z = z1; z <= z2; z++) {
                    for (int x = x1; x <= x2; x++) {
                        int backup_idx = (z - z1) * region_width + (x - x1);
                        int terrain_idx = z * g_terrain.size_x + x;
                        
                        float temp = g_terrain.heights[terrain_idx];
                        g_terrain.heights[terrain_idx] = action->terrain_backup[backup_idx];
                        action->terrain_backup[backup_idx] = temp;
                    }
                }
                
                // Update normals and chunks
                update_terrain_normals(x1, x2, z1, z2);
                mark_terrain_chunks_dirty(x1, x2, z1, z2);
            }
            break;
            
        case UNDO_TYPE_BATCH:
            // Apply sub-actions in reverse order for undo
            if (is_undo) {
                for (int i = action->sub_action_count - 1; i >= 0; i--) {
                    apply_undo_action(&action->sub_actions[i], is_undo);
                }
            } else {
                for (int i = 0; i < action->sub_action_count; i++) {
                    apply_undo_action(&action->sub_actions[i], is_undo);
                }
            }
            break;
    }
    
    // Update selection bounds if needed
    if (action->type != UNDO_TYPE_TERRAIN_MODIFY) {
        update_selection_bounds();
    }
}

/**
 * Frees undo action memory
 * @param action Action to free
 */
void free_undo_action(UndoAction* action)
{
    if (action->object_ids) {
        free(action->object_ids);
    }
    if (action->old_states) {
        free(action->old_states);
    }
    if (action->new_states) {
        free(action->new_states);
    }
    if (action->terrain_backup) {
        free(action->terrain_backup);
    }
    if (action->sub_actions) {
        for (int i = 0; i < action->sub_action_count; i++) {
            free_undo_action(&action->sub_actions[i]);
        }
        free(action->sub_actions);
    }
}

// ========================================================================
// FILE I/O
// ========================================================================

/**
 * Saves level to file
 * @param filename Output filename
 * @return 1 on success, 0 on failure
 */
int save_level(const char* filename)
{
    FILE* file = fopen(filename, "w");
    if (!file) {
        editor_log(2, "Failed to create level file: %s", filename);
        return 0;
    }
    
    editor_log(0, "Saving level: %s", filename);
    
    // Write header
    fprintf(file, "[ENDOR_LEVEL]\n");
    fprintf(file, "version=2.0\n");
    fprintf(file, "name=Level\n");
    fprintf(file, "author=Editor User\n");
    fprintf(file, "date=%s\n", get_timestamp());
    fprintf(file, "\n");
    
    // Write terrain data
    save_terrain_data(file);
    
    // Write layers
    fprintf(file, "[LAYERS]\n");
    fprintf(file, "count=%d\n", g_layer_count);
    for (int i = 0; i < g_layer_count; i++) {
        fprintf(file, "\n[LAYER_%d]\n", i);
        fprintf(file, "name=%s\n", g_layers[i].name);
        fprintf(file, "visible=%d\n", g_layers[i].visible);
        fprintf(file, "locked=%d\n", g_layers[i].locked);
        fprintf(file, "selectable=%d\n", g_layers[i].selectable);
        fprintf(file, "color=%.3f,%.3f,%.3f,%.3f\n", 
                g_layers[i].color[0], g_layers[i].color[1], 
                g_layers[i].color[2], g_layers[i].color[3]);
        fprintf(file, "parent=%d\n", g_layers[i].parent_layer);
    }
    fprintf(file, "\n");
    
    // Write objects
    save_object_data(file);
    
    // Write camera settings
    fprintf(file, "[CAMERA]\n");
    fprintf(file, "position=%.3f,%.3f,%.3f\n", 
            g_camera.position[0], g_camera.position[1], g_camera.position[2]);
    fprintf(file, "target=%.3f,%.3f,%.3f\n", 
            g_camera.target[0], g_camera.target[1], g_camera.target[2]);
    fprintf(file, "rotation=%.3f,%.3f\n", g_camera.rotation[0], g_camera.rotation[1]);
    fprintf(file, "distance=%.3f\n", g_camera.distance);
    fprintf(file, "fov=%.3f\n", g_camera.fov);
    fprintf(file, "\n");
    
    fclose(file);
    
    // Save current filename
    strncpy(g_current_level_file, filename, sizeof(g_current_level_file) - 1);
    g_stats.changes_since_save = 0;
    
    editor_log(0, "Level saved successfully");
    return 1;
}

/**
 * Saves terrain data to file
 * @param file Output file
 */
void save_terrain_data(FILE* file)
{
    fprintf(file, "[TERRAIN]\n");
    fprintf(file, "size=%d,%d\n", g_terrain.size_x, g_terrain.size_z);
    fprintf(file, "scale=%.3f\n", g_terrain.scale);
    fprintf(file, "vertical_scale=%.3f\n", g_terrain.vertical_scale);
    fprintf(file, "detail_texture=%d\n", g_terrain.detail_texture_id);
    fprintf(file, "detail_scale=%.3f\n", g_terrain.detail_scale);
    fprintf(file, "\n");
    
    // Write texture layers
    fprintf(file, "[TERRAIN_TEXTURES]\n");
    for (int i = 0; i < 8; i++) {
        if (g_terrain.texture_ids[i] >= 0) {
            fprintf(file, "texture_%d=%d,%.3f\n", i, 
                    g_terrain.texture_ids[i], g_terrain.texture_scales[i]);
        }
    }
    fprintf(file, "\n");
    
    // Write heightmap data (compressed)
    fprintf(file, "[TERRAIN_HEIGHTS]\n");
    fprintf(file, "compression=RLE\n");
    
    // Simple RLE compression
    int count = 0;
    float last_height = g_terrain.heights[0];
    
    for (int i = 0; i < g_terrain.size_x * g_terrain.size_z; i++) {
        if (fabsf(g_terrain.heights[i] - last_height) < 0.001f && count < 255) {
            count++;
        } else {
            fprintf(file, "%d,%.3f,", count, last_height);
            last_height = g_terrain.heights[i];
            count = 1;
        }
    }
    fprintf(file, "%d,%.3f\n", count, last_height);
    fprintf(file, "\n");
}

/**
 * Saves object data to file
 * @param file Output file
 */
void save_object_data(FILE* file)
{
    fprintf(file, "[OBJECTS]\n");
    fprintf(file, "count=%d\n", g_object_count);
    fprintf(file, "\n");
    
    for (int i = 0; i < g_object_count; i++) {
        EditorObject* obj = &g_editor_objects[i];
        
        fprintf(file, "[OBJECT_%d]\n", obj->id);
        fprintf(file, "type=%d\n", obj->type);
        fprintf(file, "name=%s\n", obj->name);
        fprintf(file, "position=%.3f,%.3f,%.3f\n", 
                obj->position[0], obj->position[1], obj->position[2]);
        fprintf(file, "rotation=%.3f,%.3f,%.3f\n", 
                obj->rotation[0], obj->rotation[1], obj->rotation[2]);
        fprintf(file, "scale=%.3f,%.3f,%.3f\n", 
                obj->scale[0], obj->scale[1], obj->scale[2]);
        fprintf(file, "layer=%d\n", obj->layer_id);
        fprintf(file, "parent=%d\n", obj->parent_id);
        fprintf(file, "visible=%d\n", obj->visible);
        fprintf(file, "locked=%d\n", obj->locked);
        fprintf(file, "static=%d\n", obj->static_object);
        
        if (obj->asset_id >= 0) {
            fprintf(file, "asset=%s\n", g_asset_cache[obj->asset_id].filename);
        }
        if (obj->material_id >= 0) {
            fprintf(file, "material=%d\n", obj->material_id);
        }
        
        // Save type-specific properties
        save_object_properties(file, obj);
        
        fprintf(file, "\n");
    }
}

/**
 * Saves object type-specific properties
 * @param file Output file
 * @param obj Object to save
 */
void save_object_properties(FILE* file, EditorObject* obj)
{
    switch (obj->type) {
        case EDITOR_OBJ_LIGHT:
            fprintf(file, "light_type=%d\n", obj->properties.light.light_type);
            fprintf(file, "light_color=%.3f,%.3f,%.3f\n", 
                    obj->properties.light.color[0], 
                    obj->properties.light.color[1], 
                    obj->properties.light.color[2]);
            fprintf(file, "light_intensity=%.3f\n", obj->properties.light.intensity);
            fprintf(file, "light_range=%.3f\n", obj->properties.light.range);
            fprintf(file, "cast_shadows=%d\n", obj->properties.light.cast_shadows);
            fprintf(file, "shadow_resolution=%d\n", obj->properties.light.shadow_resolution);
            break;
            
        case EDITOR_OBJ_SPAWN_POINT:
            fprintf(file, "spawn_team=%d\n", obj->properties.spawn.team);
            fprintf(file, "spawn_type=%d\n", obj->properties.spawn.spawn_type);
            fprintf(file, "spawn_radius=%.3f\n", obj->properties.spawn.spawn_radius);
            fprintf(file, "spawn_priority=%d\n", obj->properties.spawn.priority);
            break;
            
        case EDITOR_OBJ_TRIGGER:
            fprintf(file, "trigger_bounds=%.3f,%.3f,%.3f,%.3f,%.3f,%.3f\n",
                    obj->properties.trigger.bounds[0], obj->properties.trigger.bounds[1],
                    obj->properties.trigger.bounds[2], obj->properties.trigger.bounds[3],
                    obj->properties.trigger.bounds[4], obj->properties.trigger.bounds[5]);
            fprintf(file, "trigger_script=%s\n", obj->properties.trigger.script);
            fprintf(file, "trigger_once=%d\n", obj->properties.trigger.trigger_once);
            fprintf(file, "trigger_delay=%d\n", obj->properties.trigger.trigger_delay);
            break;
            
        // Add other object types as needed
    }
}

/**
 * Loads level from file
 * @param filename Input filename
 * @return 1 on success, 0 on failure
 */
int load_level(const char* filename)
{
    FILE* file = fopen(filename, "r");
    if (!file) {
        editor_log(2, "Failed to open level file: %s", filename);
        return 0;
    }
    
    editor_log(0, "Loading level: %s", filename);
    
    // Clear existing data
    clear_selection();
    g_object_count = 0;
    g_layer_count = 1;  // Keep default layer
    
    char line[1024];
    char section[64] = "";
    int current_object_id = -1;
    EditorObject* current_object = NULL;
    
    while (fgets(line, sizeof(line), file)) {
        // Remove newline
        char* newline = strchr(line, '\n');
        if (newline) *newline = '\0';
        
        // Skip empty lines and comments
        if (line[0] == '\0' || line[0] == '#' || line[0] == ';') {
            continue;
        }
        
        // Check for section header
        if (line[0] == '[') {
            char* end = strchr(line, ']');
            if (end) {
                *end = '\0';
                strncpy(section, line + 1, sizeof(section) - 1);
                
                // Check if it's an object section
                if (strncmp(section, "OBJECT_", 7) == 0) {
                    current_object_id = atoi(section + 7);
                    // Create new object
                    if (g_object_count < g_object_capacity) {
                        current_object = &g_editor_objects[g_object_count++];
                        memset(current_object, 0, sizeof(EditorObject));
                        current_object->id = current_object_id;
                        current_object->scale[0] = current_object->scale[1] = current_object->scale[2] = 1.0f;
                        current_object->visible = 1;
                        current_object->asset_id = -1;
                        current_object->material_id = -1;
                        current_object->parent_id = -1;
                    }
                } else {
                    current_object = NULL;
                }
            }
            continue;
        }
        
        // Parse key=value
        char* equals = strchr(line, '=');
        if (!equals) continue;
        
        *equals = '\0';
        char* key = line;
        char* value = equals + 1;
        
        // Process based on section
        if (strcmp(section, "TERRAIN") == 0) {
            load_terrain_property(key, value);
        } else if (current_object != NULL) {
            load_object_property(current_object, key, value);
        }
        // Add other sections as needed
    }
    
    fclose(file);
    
    // Update terrain normals
    update_terrain_normals(0, g_terrain.size_x - 1, 0, g_terrain.size_z - 1);
    
    // Update statistics
    g_stats.total_objects = g_object_count;
    g_stats.changes_since_save = 0;
    
    // Save current filename
    strncpy(g_current_level_file, filename, sizeof(g_current_level_file) - 1);
    
    editor_log(0, "Level loaded successfully");
    return 1;
}

/**
 * Loads terrain property from file
 * @param key Property key
 * @param value Property value
 */
void load_terrain_property(const char* key, const char* value)
{
    if (strcmp(key, "size") == 0) {
        int width, height;
        if (sscanf(value, "%d,%d", &width, &height) == 2) {
            resize_terrain(width, height);
        }
    } else if (strcmp(key, "scale") == 0) {
        g_terrain.scale = atof(value);
    } else if (strcmp(key, "vertical_scale") == 0) {
        g_terrain.vertical_scale = atof(value);
    }
    // Add other terrain properties
}

/**
 * Loads object property from file
 * @param obj Object to load into
 * @param key Property key
 * @param value Property value
 */
void load_object_property(EditorObject* obj, const char* key, const char* value)
{
    if (strcmp(key, "type") == 0) {
        obj->type = atoi(value);
    } else if (strcmp(key, "name") == 0) {
        strncpy(obj->name, value, sizeof(obj->name) - 1);
    } else if (strcmp(key, "position") == 0) {
        sscanf(value, "%f,%f,%f", &obj->position[0], &obj->position[1], &obj->position[2]);
    } else if (strcmp(key, "rotation") == 0) {
        sscanf(value, "%f,%f,%f", &obj->rotation[0], &obj->rotation[1], &obj->rotation[2]);
    } else if (strcmp(key, "scale") == 0) {
        sscanf(value, "%f,%f,%f", &obj->scale[0], &obj->scale[1], &obj->scale[2]);
    } else if (strcmp(key, "layer") == 0) {
        obj->layer_id = atoi(value);
    } else if (strcmp(key, "parent") == 0) {
        obj->parent_id = atoi(value);
    } else if (strcmp(key, "visible") == 0) {
        obj->visible = atoi(value);
    } else if (strcmp(key, "locked") == 0) {
        obj->locked = atoi(value);
    } else if (strcmp(key, "static") == 0) {
        obj->static_object = atoi(value);
    } else if (strcmp(key, "asset") == 0) {
        // Find or load asset
        for (int i = 0; i < g_asset_count; i++) {
            if (strcmp(g_asset_cache[i].filename, value) == 0) {
                obj->asset_id = i;
                g_asset_cache[i].reference_count++;
                break;
            }
        }
    } else {
        // Type-specific properties
        load_object_type_property(obj, key, value);
    }
    
    // Update bounds
    calculate_object_bounds(obj);
}

/**
 * Loads object type-specific property
 * @param obj Object to load into
 * @param key Property key
 * @param value Property value
 */
void load_object_type_property(EditorObject* obj, const char* key, const char* value)
{
    switch (obj->type) {
        case EDITOR_OBJ_LIGHT:
            if (strcmp(key, "light_type") == 0) {
                obj->properties.light.light_type = atoi(value);
            } else if (strcmp(key, "light_color") == 0) {
                sscanf(value, "%f,%f,%f", 
                       &obj->properties.light.color[0],
                       &obj->properties.light.color[1],
                       &obj->properties.light.color[2]);
            } else if (strcmp(key, "light_intensity") == 0) {
                obj->properties.light.intensity = atof(value);
            } else if (strcmp(key, "light_range") == 0) {
                obj->properties.light.range = atof(value);
            } else if (strcmp(key, "cast_shadows") == 0) {
                obj->properties.light.cast_shadows = atoi(value);
            } else if (strcmp(key, "shadow_resolution") == 0) {
                obj->properties.light.shadow_resolution = atoi(value);
            }
            break;
            
        case EDITOR_OBJ_SPAWN_POINT:
            if (strcmp(key, "spawn_team") == 0) {
                obj->properties.spawn.team = atoi(value);
            } else if (strcmp(key, "spawn_type") == 0) {
                obj->properties.spawn.spawn_type = atoi(value);
            } else if (strcmp(key, "spawn_radius") == 0) {
                obj->properties.spawn.spawn_radius = atof(value);
            } else if (strcmp(key, "spawn_priority") == 0) {
                obj->properties.spawn.priority = atoi(value);
            }
            break;
            
        // Add other object types
    }
}

/**
 * Checks and performs autosave
 */
void check_autosave(void)
{
    if (!g_config.autosave_enabled || g_stats.changes_since_save == 0) {
        return;
    }
    
    time_t now = time(NULL);
    if (now - g_last_autosave >= g_config.autosave_interval / 1000) {
        char autosave_file[512];
        if (strlen(g_current_level_file) > 0) {
            // Create autosave based on current file
            char* dot = strrchr(g_current_level_file, '.');
            if (dot) {
                *dot = '\0';
                sprintf(autosave_file, "%s_autosave.lvl", g_current_level_file);
                *dot = '.';
            } else {
                sprintf(autosave_file, "%s_autosave.lvl", g_current_level_file);
            }
        } else {
            sprintf(autosave_file, "autosave_%ld.lvl", now);
        }
        
        if (save_level(autosave_file)) {
            g_last_autosave = now;
            editor_log(0, "Autosaved to: %s", autosave_file);
        }
    }
}

/**
 * Exports optimized level for game use
 * @param filename Output filename
 * @return 1 on success, 0 on failure
 */
int export_level_optimized(const char* filename)
{
    FILE* file = fopen(filename, "wb");
    if (!file) {
        editor_log(2, "Failed to create export file: %s", filename);
        return 0;
    }
    
    editor_log(0, "Exporting optimized level: %s", filename);
    
    // Write binary header
    unsigned int magic = 0x4C444E45;  // "ENDL"
    unsigned int version = 0x00020000;  // 2.0
    fwrite(&magic, sizeof(unsigned int), 1, file);
    fwrite(&version, sizeof(unsigned int), 1, file);
    
    // TODO: Write optimized binary data
    // - Packed terrain data
    // - Optimized object placement
    // - Baked lighting data
    // - Collision data
    // - Navigation mesh
    
    fclose(file);
    
    editor_log(0, "Level exported successfully");
    return 1;
}

// ========================================================================
// MATERIAL SYSTEM
// ========================================================================

/**
 * Creates default materials
 */
void create_default_materials(void)
{
    // Default material
    create_material("Default", 0, 3, -1, -1);  // White diffuse, flat normal
    
    // Basic materials
    create_material("Stone", -1, -1, -1, -1);
    create_material("Metal", -1, -1, -1, -1);
    create_material("Wood", -1, -1, -1, -1);
    create_material("Glass", -1, -1, -1, -1);
}

/**
 * Creates a new material
 * @param name Material name
 * @param diffuse_tex Diffuse texture ID
 * @param normal_tex Normal texture ID
 * @param specular_tex Specular texture ID
 * @param emissive_tex Emissive texture ID
 * @return Material ID or -1 on failure
 */
int create_material(const char* name, int diffuse_tex, int normal_tex, 
                   int specular_tex, int emissive_tex)
{
    if (g_material_count >= g_material_capacity) {
        // Expand array
        int new_capacity = g_material_capacity * 2;
        Material* new_materials = (Material*)realloc(g_materials, 
                                                    new_capacity * sizeof(Material));
        if (!new_materials) {
            editor_log(2, "Failed to expand material array");
            return -1;
        }
        g_materials = new_materials;
        g_material_capacity = new_capacity;
    }
    
    Material* mat = &g_materials[g_material_count];
    memset(mat, 0, sizeof(Material));
    
    mat->diffuse_texture_id = diffuse_tex;
    mat->normal_texture_id = normal_tex;
    mat->specular_texture_id = specular_tex;
    mat->emissive_texture_id = emissive_tex;
    
    // Default colors
    mat->diffuse_color[0] = mat->diffuse_color[1] = mat->diffuse_color[2] = 1.0f;
    mat->diffuse_color[3] = 1.0f;
    mat->specular_color[0] = mat->specular_color[1] = mat->specular_color[2] = 0.5f;
    mat->specular_color[3] = 1.0f;
    mat->emissive_color[0] = mat->emissive_color[1] = mat->emissive_color[2] = 0.0f;
    mat->emissive_color[3] = 1.0f;
    
    // Default properties
    mat->roughness = 0.5f;
    mat->metallic = 0.0f;
    mat->opacity = 1.0f;
    mat->normal_strength = 1.0f;
    mat->blend_mode = 0;  // Opaque
    mat->two_sided = 0;
    mat->cast_shadows = 1;
    
    strncpy(mat->shader_name, "standard", sizeof(mat->shader_name) - 1);
    
    editor_log(0, "Created material: %s", name);
    return g_material_count++;
}

// ========================================================================
// UTILITY FUNCTIONS
// ========================================================================

/**
 * Gets current timestamp string
 * @return Timestamp string
 */
const char* get_timestamp(void)
{
    static char timestamp[32];
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_info);
    return timestamp;
}

/**
 * Gets filename without path
 * @param path Full path
 * @return Filename portion
 */
const char* get_filename_without_path(const char* path)
{
    const char* filename = strrchr(path, '\\');
    if (!filename) {
        filename = strrchr(path, '/');
    }
    return filename ? filename + 1 : path;
}

/**
 * Checks if a key is pressed
 * @param key Virtual key code
 * @return 1 if pressed, 0 otherwise
 */
int is_key_pressed(int key)
{
    return (GetAsyncKeyState(key) & 0x8000) != 0;
}

/**
 * Creates default prefabs
 */
void create_default_prefabs(void)
{
    // TODO: Create some default prefabs
    // - Basic light setup
    // - Spawn point setup
    // - Common object arrangements
}

/**
 * Loads editor configuration
 * @param filename Configuration filename
 */
void load_editor_config(const char* filename)
{
    FILE* file = fopen(filename, "r");
    if (!file) {
        editor_log(1, "No editor config found, using defaults");
        return;
    }
    
    char line[256];
    while (fgets(line, sizeof(line), file)) {
        char* newline = strchr(line, '\n');
        if (newline) *newline = '\0';
        
        char* equals = strchr(line, '=');
        if (!equals) continue;
        
        *equals = '\0';
        char* key = line;
        char* value = equals + 1;
        
        // Parse configuration values
        if (strcmp(key, "autosave_enabled") == 0) {
            g_config.autosave_enabled = atoi(value);
        } else if (strcmp(key, "autosave_interval") == 0) {
            g_config.autosave_interval = atoi(value);
        } else if (strcmp(key, "camera_speed") == 0) {
            g_config.camera_speed_multiplier = atof(value);
        } else if (strcmp(key, "grid_size") == 0) {
            g_config.grid_size = atof(value);
        } else if (strcmp(key, "snap_angle") == 0) {
            g_config.snap_angle = atof(value);
        }
        // Add other config options
    }
    
    fclose(file);
    editor_log(0, "Loaded editor configuration");
}

/**
 * Saves editor configuration
 * @param filename Configuration filename
 */
void save_editor_config(const char* filename)
{
    FILE* file = fopen(filename, "w");
    if (!file) {
        editor_log(2, "Failed to save editor config");
        return;
    }
    
    fprintf(file, "# Endor Level Editor Configuration\n");
    fprintf(file, "autosave_enabled=%d\n", g_config.autosave_enabled);
    fprintf(file, "autosave_interval=%d\n", g_config.autosave_interval);
    fprintf(file, "camera_speed=%.3f\n", g_config.camera_speed_multiplier);
    fprintf(file, "grid_size=%.3f\n", g_config.grid_size);
    fprintf(file, "snap_angle=%.3f\n", g_config.snap_angle);
    fprintf(file, "show_grid=%d\n", g_config.show_grid);
    fprintf(file, "show_gizmos=%d\n", g_config.show_gizmos);
    fprintf(file, "show_statistics=%d\n", g_config.show_statistics);
    
    fclose(file);
    editor_log(0, "Saved editor configuration");
}

/**
 * Updates list of visible objects
 */
void update_visible_objects(void)
{
    g_visible_object_count = 0;
    
    for (int i = 0; i < g_object_count; i++) {
        EditorObject* obj = &g_editor_objects[i];
        
        // Check visibility
        if (!obj->visible || !g_layers[obj->layer_id].visible) {
            continue;
        }
        
        // TODO: Frustum culling
        if (g_frustum_cull_enabled) {
            // Check if object is in view frustum
        }
        
        // Add to visible list
        if (g_visible_object_count < g_object_capacity) {
            g_visible_objects[g_visible_object_count++] = i;
        }
    }
    
    g_stats.visible_objects = g_visible_object_count;
}

/**
 * Cleans up the level editor
 */
void cleanup_level_editor(void)
{
    if (!g_editor_initialized) {
        return;
    }
    
    editor_log(0, "Cleaning up level editor");
    
    // Save configuration
    save_editor_config("editor_config.ini");
    
    // Free dynamic memory
    if (g_editor_objects) {
        // Free type-specific data
        for (int i = 0; i < g_object_count; i++) {
            if (g_editor_objects[i].type == EDITOR_OBJ_SPLINE && 
                g_editor_objects[i].properties.spline.control_points) {
                free(g_editor_objects[i].properties.spline.control_points);
            }
            if (g_editor_objects[i].custom_properties) {
                free(g_editor_objects[i].custom_properties);
            }
        }
        free(g_editor_objects);
    }
    
    if (g_asset_cache) {
        // Free asset data
        for (int i = 0; i < g_asset_count; i++) {
            if (g_asset_cache[i].data) {
                free(g_asset_cache[i].data);
            }
            if (g_asset_cache[i].thumbnail_data) {
                free(g_asset_cache[i].thumbnail_data);
            }
            // Free dependency list
            AssetDependency* dep = g_asset_cache[i].dependencies;
            while (dep) {
                AssetDependency* next = dep->next;
                free(dep);
                dep = next;
            }
        }
        free(g_asset_cache);
    }
    
    if (g_materials) {
        free(g_materials);
    }
    
    if (g_undo_stack) {
        // Free undo actions
        for (int i = 0; i < g_undo_capacity; i++) {
            free_undo_action(&g_undo_stack[i]);
        }
        free(g_undo_stack);
    }
    
    if (g_visible_objects) {
        free(g_visible_objects);
    }
    
    if (g_selection.object_ids) {
        free(g_selection.object_ids);
    }
    
    if (g_selection.lasso_points) {
        free(g_selection.lasso_points);
    }
    
    // Free terrain data
    if (g_terrain.heights) free(g_terrain.heights);
    if (g_terrain.normals) free(g_terrain.normals);
    if (g_terrain.blend_weights) free(g_terrain.blend_weights);
    if (g_terrain.vegetation_map) free(g_terrain.vegetation_map);
    if (g_terrain.water_map) free(g_terrain.water_map);
    if (g_terrain.chunks) free(g_terrain.chunks);
    
    g_editor_initialized = 0;
    
    editor_log(0, "Level editor cleanup complete");
}

// ========================================================================
// PUBLIC API
// ========================================================================

/**
 * Main editor update function
 * @param delta_time Frame delta time in seconds
 */
void update_level_editor(float delta_time)
{
    if (!g_editor_initialized) {
        return;
    }
    
    // Update camera
    update_editor_camera(delta_time);
    
    // Update visible objects
    update_visible_objects();
    
    // Check for hot-reload
    static float hot_reload_timer = 0.0f;
    hot_reload_timer += delta_time;
    if (hot_reload_timer > 1.0f) {
        hot_reload_timer = 0.0f;
        for (int i = 0; i < g_asset_count; i++) {
            check_asset_hot_reload(i);
        }
    }
    
    // Check autosave
    check_autosave();
    
    // Update statistics
    g_stats.last_frame_time = delta_time;
}

/**
 * Renders the level editor
 */
void render_level_editor(void)
{
    if (!g_editor_initialized) {
        return;
    }
    
    // TODO: Implement rendering
    // - Render terrain
    // - Render objects
    // - Render selection
    // - Render gizmos
    // - Render grid
    // - Render UI
}

/**
 * Handles editor input
 * @param msg Windows message
 * @param wparam WPARAM
 * @param lparam LPARAM
 * @return 1 if handled, 0 otherwise
 */
int handle_editor_input(UINT msg, WPARAM wparam, LPARAM lparam)
{
    if (!g_editor_initialized) {
        return 0;
    }
    
    switch (msg) {
        case WM_KEYDOWN:
            switch (wparam) {
                case 'Z':
                    if (GetAsyncKeyState(VK_CONTROL) & 0x8000) {
                        if (GetAsyncKeyState(VK_SHIFT) & 0x8000) {
                            redo();
                        } else {
                            undo();
                        }
                        return 1;
                    }
                    break;
                    
                case 'S':
                    if (GetAsyncKeyState(VK_CONTROL) & 0x8000) {
                        if (strlen(g_current_level_file) > 0) {
                            save_level(g_current_level_file);
                        }
                        return 1;
                    }
                    break;
                    
                case VK_DELETE:
                    if (g_selection.count > 0) {
                        begin_undo_group("Delete Selection");
                        for (int i = g_selection.count - 1; i >= 0; i--) {
                            delete_editor_object(g_selection.object_ids[i]);
                        }
                        end_undo_group();
                        return 1;
                    }
                    break;
                    
                case VK_F2:
                    focus_camera_on_selection();
                    return 1;
            }
            break;
            
        case WM_LBUTTONDOWN:
        case WM_RBUTTONDOWN:
        case WM_MBUTTONDOWN:
            // Handle mouse input
            break;
            
        case WM_MOUSEWHEEL:
            {
                short wheel_delta = GET_WHEEL_DELTA_WPARAM(wparam);
                camera_mouse_input(0, 0, 0, wheel_delta / WHEEL_DELTA);
                return 1;
            }
            break;
    }
    
    return 0;
};
    
    for (int z = min_z; z <= max_z; z++) {
        for (int x = min_x; x <= max_x; x++) {
            float dx = (x - center_x) * g_terrain.scale;
            float dz = (z - center_z) * g_terrain.scale;
            float distance = sqrtf(dx * dx + dz * dz);
            
            if (distance <= g_terrain_brush.radius) {
                float influence = 1.0f - (distance / g_terrain_brush.radius);
                influence = powf(influence, g_terrain_brush.falloff);
                
                // Generate Perlin noise
                float noise = 0.0f;
                float amplitude = 1.0f;
                float frequency = g_terrain_brush.noise_scale;
                
                for (int octave = 0; octave < (int)g_terrain_brush.noise_octaves; octave++) {
                    float nx = x * frequency * 0.01f;
                    float nz = z * frequency * 0.01f;
                    
                    // Simple noise function (should use proper Perlin noise)
                    noise += sinf(nx * 12.9898f + nz * 78.233f) * amplitude;
                    
                    amplitude *= 0.5f;
                    frequency *= 2.0f;
                }
                
                int idx = z * g_terrain.size_x + x;
                g_terrain.heights[idx] += noise * g_terrain_brush.strength * influence;
                
                // Clamp height
                g_terrain.heights[idx] = fmaxf(-100.0f, fminf(100.0f, g_terrain.heights[idx]));
            }
        }
    }
}

/**
 * Applies erosion brush
 */
void apply_terrain_brush_erosion(int center_x, int center_z, 
                                int min_x, int max_x, int min_z, int max_z, int apply)
{
    if (!apply) return;
    
    // Simple erosion simulation
    float* water_flow = (float*)calloc((max_x - min_x + 1) * (max_z - min_z + 1), sizeof(float));
    float* sediment = (float*)calloc((max_x - min_x + 1) * (max_z - min_z + 1), sizeof(float));
    
    // Simulate water flow
    for (int iteration = 0; iteration < 10; iteration++) {
        for (int z = min_z; z <= max_z; z++) {
            for (int x = min_x; x <= max_x; x++) {
                float dx = (x - center_x) * g_terrain.scale;
                float dz = (z - center_z) * g_terrain.scale;
                float distance = sqrtf(dx * dx + dz * dz);
                
                if (distance <= g_terrain_brush.radius) {
                    int local_idx = (z - min_z) * (max_x - min_x + 1) + (x - min_x);
                    int terrain_idx = z * g_terrain.size_x + x;
                    
                    // Calculate gradient
                    float gradient_x = 0.0f;
                    float gradient_z = 0.0f;
                    
                    if (x > 0 && x < g_terrain.size_x - 1) {
                        gradient_x = g_terrain.heights[terrain_idx + 1] - 
                                    g_terrain.heights[terrain_idx - 1];
                    }
                    if (z > 0 && z < g_terrain.size_z - 1) {
                        gradient_z = g_terrain.heights[(z + 1) * g_terrain.size_x + x] - 
                                    g_terrain.heights[(z - 1) * g_terrain.size_x + x];
                    }
                    
                    // Water flows downhill
                    float flow_strength = sqrtf(gradient_x * gradient_x + gradient_z * gradient_z);
                    water_flow[local_idx] += flow_strength * g_terrain_brush.flow_rate;
                    
                    // Erosion
                    float erosion = water_flow[local_idx] * g_terrain_brush.strength * 0.1f;
                    g_terrain.heights[terrain_idx] -= erosion;
                    sediment[local_idx] += erosion;
                    
                    // Deposition
                    float deposition = sediment[local_idx] * g_terrain_brush.deposition_rate;
                    g_terrain.heights[terrain_idx] += deposition;
                    sediment[local_idx] -= deposition;
                }
            }
        }
    }
    
    free(water_flow);
    free(sediment);
}

/**
 * Paints terrain texture
 * @param world_x World X coordinate
 * @param world_z World Z coordinate
 * @param texture_layer Texture layer to paint
 */
void paint_terrain_texture(float world_x, float world_z, int texture_layer)
{
    if (texture_layer < 0 || texture_layer >= 8) {
        return;
    }
    
    // Convert world to terrain coordinates
    int center_x = (int)((world_x / g_terrain.scale) + g_terrain.size_x * 0.5f);
    int center_z = (int)((world_z / g_terrain.scale) + g_terrain.size_z * 0.5f);
    
    if (center_x < 0 || center_x >= g_terrain.size_x || 
        center_z < 0 || center_z >= g_terrain.size_z) {
        return;
    }
    
    // Calculate affected region
    int radius_cells = (int)(g_terrain_brush.radius / g_terrain.scale) + 1;
    int min_x = fmaxf(0, center_x - radius_cells);
    int max_x = fminf(g_terrain.size_x - 1, center_x + radius_cells);
    int min_z = fmaxf(0, center_z - radius_cells);
    int max_z = fminf(g_terrain.size_z - 1, center_z + radius_cells);
    
    for (int z = min_z; z <= max_z; z++) {
        for (int x = min_x; x <= max_x; x++) {
            float dx = (x - center_x) * g_terrain.scale;
            float dz = (z - center_z) * g_terrain.scale;
            float distance = sqrtf(dx * dx + dz * dz);
            
            if (distance <= g_terrain_brush.radius) {
                float influence = 1.0f - (distance / g_terrain_brush.radius);
                influence = powf(influence, g_terrain_brush.falloff) * g_terrain_brush.strength;
                
                int idx = z * g_terrain.size_x + x;
                
                // Update blend weights
                float* weights = &g_terrain.blend_weights[idx * 8];
                
                // Increase target layer
                weights[texture_layer] += influence;
                weights[texture_layer] = fminf(1.0f, weights[texture_layer]);
                
                // Normalize weights
                float total = 0.0f;
                for (int i = 0; i < 8; i++) {
                    total += weights[i];
                }
                
                if (total > 0.0f) {
                    for (int i = 0; i < 8; i++) {
                        weights[i] /= total;
                    }
                }
            }
        }
    }
    
    // Mark chunks as dirty
    mark_terrain_chunks_dirty(min_x, max_x, min_z, max_z);
}

/**
 * Updates terrain normals for a region
 * @param min_x Minimum X coordinate
 * @param max_x Maximum X coordinate
 * @param min_z Minimum Z coordinate
 * @param max_z Maximum Z coordinate
 */
void update_terrain_normals(int min_x, int max_x, int min_z, int max_z)
{
    // Expand region by 1 to include neighbors
    min_x = fmaxf(1, min_x - 1);
    max_x = fminf(g_terrain.size_x - 2, max_x + 1);
    min_z = fmaxf(1, min_z - 1);
    max_z = fminf(g_terrain.size_z - 2, max_z + 1);
    
    for (int z = min_z; z <= max_z; z++) {
        for (int x = min_x; x <= max_x; x++) {
            int idx = z * g_terrain.size_x + x;
            
            // Calculate normal from neighboring heights
            float h_left = g_terrain.heights[idx - 1];
            float h_right = g_terrain.heights[idx + 1];
            float h_down = g_terrain.heights[(z - 1) * g_terrain.size_x + x];
            float h_up = g_terrain.heights[(z + 1) * g_terrain.size_x + x];
            
            // Cross product of tangent vectors
            float dx = 2.0f * g_terrain.scale;
            float dy_x = h_right - h_left;
            float dy_z = h_up - h_down;
            
            // Normal = (dx, dy_x, 0) x (0, dy_z, dx)
            float nx = -dy_x * g_terrain.vertical_scale;
            float ny = dx * g_terrain.vertical_scale;
            float nz = -dy_z * g_terrain.vertical_scale;
            
            // Normalize
            float length = sqrtf(nx * nx + ny * ny + nz * nz);
            if (length > 0.0f) {
                g_terrain.normals[idx * 3] = nx / length;
                g_terrain.normals[idx * 3 + 1] = ny / length;
                g_terrain.normals[idx * 3 + 2] = nz / length;
            } else {
                g_terrain.normals[idx * 3] = 0.0f;
                g_terrain.normals[idx * 3 + 1] = 1.0f;
                g_terrain.normals[idx * 3 + 2] = 0.0f;
            }
        }
    }
}

/**
 * Marks terrain chunks as dirty
 * @param min_x Minimum X coordinate
 * @param max_x Maximum X coordinate
 * @param min_z Minimum Z coordinate
 * @param max_z Maximum Z coordinate
 */
void mark_terrain_chunks_dirty(int min_x, int max_x, int min_z, int max_z)
{
    int chunk_min_x = min_x / TERRAIN_CHUNK_SIZE;
    int chunk_max_x = max_x / TERRAIN_CHUNK_SIZE;
    int chunk_min_z = min_z / TERRAIN_CHUNK_SIZE;
    int chunk_max_z = max_z / TERRAIN_CHUNK_SIZE;
    
    for (int cz = chunk_min_z; cz <= chunk_max_z; cz++) {
        for (int cx = chunk_min_x; cx <= chunk_max_x; cx++) {
            if (cx >= 0 && cx < g_terrain.chunk_count_x &&
                cz >= 0 && cz < g_terrain.chunk_count_z) {
                g_terrain.chunks[cz * g_terrain.chunk_count_x + cx].dirty = 1;
            }
        }
    }
}

/**
 * Loads terrain heightmap from file
 * @param filename Heightmap filename
 * @return 1 on success, 0 on failure
 */
int load_terrain_heightmap(const char* filename)
{
    FILE* file = fopen(filename, "rb");
    if (!file) {
        editor_log(2, "Failed to open heightmap: %s", filename);
        return 0;
    }
    
    // Read header (assuming raw float format)
    int width, height;
    fread(&width, sizeof(int), 1, file);
    fread(&height, sizeof(int), 1, file);
    
    if (width != g_terrain.size_x || height != g_terrain.size_z) {
        editor_log(1, "Heightmap size mismatch: %dx%d vs %dx%d", 
                  width, height, g_terrain.size_x, g_terrain.size_z);
        fclose(file);
        return 0;
    }
    
    // Read height data
    fread(g_terrain.heights, sizeof(float), width * height, file);
    fclose(file);
    
    // Update normals for entire terrain
    update_terrain_normals(0, g_terrain.size_x - 1, 0, g_terrain.size_z - 1);
    
    // Mark all chunks as dirty
    for (int i = 0; i < g_terrain.chunk_count_x * g_terrain.chunk_count_z; i++) {
        g_terrain.chunks[i].dirty = 1;
    }
    
    editor_log(0, "Loaded heightmap: %s", filename);
    return 1;
}

/**
 * Exports terrain heightmap to file
 * @param filename Output filename
 * @return 1 on success, 0 on failure
 */
int export_terrain_heightmap(const char* filename)
{
    FILE* file = fopen(filename, "wb");
    if (!file) {
        editor_log(2, "Failed to create heightmap file: %s", filename);
        return 0;
    }
    
    // Write header
    fwrite(&g_terrain.size_x, sizeof(int), 1, file);
    fwrite(&g_terrain.size_z, sizeof(int), 1, file);
    
    // Write height data
    fwrite(g_terrain.heights, sizeof(float), 
           g_terrain.size_x * g_terrain.size_z, file);
    
    fclose(file);
    
    editor_log(0, "Exported heightmap: %s", filename);
    return 1;
}

/**
 * Resizes terrain
 * @param new_width New terrain width
 * @param new_height New terrain height
 */
void resize_terrain(int new_width, int new_height)
{
    if (new_width == g_terrain.size_x && new_height == g_terrain.size_z) {
        return;
    }
    
    // Allocate new arrays
    int new_size = new_width * new_height;
    float* new_heights = (float*)calloc(new_size, sizeof(float));
    float* new_normals = (float*)calloc(new_size * 3, sizeof(float));
    float* new_blend_weights = (float*)calloc(new_size * 8, sizeof(float));
    
    // Copy existing data (with bilinear sampling for scaling)
    for (int z = 0; z < new_height; z++) {
        for (int x = 0; x < new_width; x++) {
            float src_x = (x * (g_terrain.size_x - 1)) / (float)(new_width - 1);
            float src_z = (z * (g_terrain.size_z - 1)) / (float)(new_height - 1);
            
            int x0 = (int)src_x;
            int z0 = (int)src_z;
            int x1 = fminf(x0 + 1, g_terrain.size_x - 1);
            int z1 = fminf(z0 + 1, g_terrain.size_z - 1);
            
            float fx = src_x - x0;
            float fz = src_z - z0;
            
            // Bilinear interpolation
            float h00 = g_terrain.heights[z0 * g_terrain.size_x + x0];
            float h10 = g_terrain.heights[z0 * g_terrain.size_x + x1];
            float h01 = g_terrain.heights[z1 * g_terrain.size_x + x0];
            float h11 = g_terrain.heights[z1 * g_terrain.size_x + x1];
            
            float h0 = h00 * (1 - fx) + h10 * fx;
            float h1 = h01 * (1 - fx) + h11 * fx;
            float h = h0 * (1 - fz) + h1 * fz;
            
            new_heights[z * new_width + x] = h;
            
            // Copy blend weights
            for (int i = 0; i < 8; i++) {
                new_blend_weights[(z * new_width + x) * 8 + i] = 
                    g_terrain.blend_weights[(z0 * g_terrain.size_x + x0) * 8 + i];
            }
        }
    }
    
    // Free old arrays
    free(g_terrain.heights);
    free(g_terrain.normals);
    free(g_terrain.blend_weights);
    free(g_terrain.vegetation_map);
    free(g_terrain.water_map);
    free(g_terrain.chunks);
    
    // Update terrain data
    g_terrain.size_x = new_width;
    g_terrain.size_z = new_height;
    g_terrain.heights = new_heights;
    g_terrain.normals = new_normals;
    g_terrain.blend_weights = new_blend_weights;
    g_terrain.vegetation_map = (float*)calloc(new_size, sizeof(float));
    g_terrain.water_map = (float*)calloc(new_size, sizeof(float));
    
    // Recreate chunks
    g_terrain.chunk_count_x = (new_width + TERRAIN_CHUNK_SIZE - 1) / TERRAIN_CHUNK_SIZE;
    g_terrain.chunk_count_z = (new_height + TERRAIN_CHUNK_SIZE - 1) / TERRAIN_CHUNK_SIZE;
    g_terrain.chunks = (TerrainChunk*)calloc(g_terrain.chunk_count_x * g_terrain.chunk_count_z, 
                                             sizeof(TerrainChunk));
    
    // Update normals
    update_terrain_normals(0, new_width - 1, 0, new_height - 1);
    
    editor_log(0, "Resized terrain to %dx%d", new_width, new_height);
}

// ========================================================================
// LAYER SYSTEM
// ========================================================================

/**
 * Creates a new layer
 * @param name Layer name
 * @param parent_id Parent layer ID (-1 for root)
 * @return Layer ID or -1 on failure
 */
int create_layer(const char* name, int parent_id)
{
    if (g_layer_count >= MAX_LAYERS) {
        editor_log(2, "Maximum layer count reached");
        return -1;
    }
    
    EditorLayer* layer = &g_layers[g_layer_count];
    strncpy(layer->name, name, sizeof(layer->name) - 1);
    layer->visible = 1;
    layer->locked = 0;
    layer->selectable = 1;
    layer->color[0] = 0.5f + (rand() % 50) / 100.0f;
    layer->color[1] = 0.5f + (rand() % 50) / 100.0f;
    layer->color[2] = 0.5f + (rand() % 50) / 100.0f;
    layer->color[3] = 1.0f;
    layer->object_count = 0;
    layer->parent_layer = parent_id;
    
    editor_log(0, "Created layer: %s", name);
    return g_layer_count++;
}

/**
 * Deletes a layer
 * @param layer_id Layer ID to delete
 */
void delete_layer(int layer_id)
{
    if (layer_id <= 0 || layer_id >= g_layer_count) {
        return;  // Can't delete default layer
    }
    
    // Move all objects to default layer
    for (int i = 0; i < g_object_count; i++) {
        if (g_editor_objects[i].layer_id == layer_id) {
            g_editor_objects[i].layer_id = 0;
        }
    }
    
    // Update child layers
    for (int i = 0; i < g_layer_count; i++) {
        if (g_layers[i].parent_layer == layer_id) {
            g_layers[i].parent_layer = -1;
        }
    }
    
    editor_log(0, "Deleted layer: %s", g_layers[layer_id].name);
    
    // Shift layers down
    for (int i = layer_id; i < g_layer_count - 1; i++) {
        g_layers[i] = g_layers[i + 1];
    }
    g_layer_count--;
    
    // Update object layer references
    for (int i = 0; i < g_object_count; i++) {
        if (g_editor_objects[i].layer_id > layer_id) {
            g_editor_objects[i].layer_id--;
        }
    }
}

/**
 * Sets layer visibility
 * @param layer_id Layer ID
 * @param visible Visibility state
 */
void set_layer_visibility(int layer_id, int visible)
{
    if (layer_id < 0 || layer_id >= g_layer_count) {
        return;
    }
    
    g_layers[layer_id].visible = visible;
    
    // Update visible objects
    update_visible_objects();
    
    editor_log(0, "Set layer %s visibility to %d", 
              g_layers[layer_id].name, visible);
}

/**
 * Moves objects to a layer
 * @param object_ids Array of object IDs
 * @param count Number of objects
 * @param target_layer Target layer ID
 */
void move_objects_to_layer(int* object_ids, int count, int target_layer)
{
    if (target_layer < 0 || target_layer >= g_layer_count) {
        return;
    }
    
    begin_undo_group("Move Objects to Layer");
    
    for (int i = 0; i < count; i++) {
        EditorObject* obj = find_editor_object(object_ids[i]);
        if (obj) {
            EditorObject old_state = *obj;
            
            // Update layer counts
            if (obj->layer_id < g_layer_count) {
                g_layers[obj->layer_id].object_count--;
            }
            g_layers[target_layer].object_count++;
            
            obj->layer_id = target_layer;
            
            add_undo_action(UNDO_TYPE_OBJECT_MODIFY, obj->id, &old_state, obj);
        }
    }
    
    end_undo_group();
    
    editor_log(0, "Moved %d objects to layer %s", count, g_layers[target_layer].name);
}

// ========================================================================
// PREFAB SYSTEM
// ========================================================================

/**
 * Creates a prefab from selection
 * @param name Prefab name
 * @param description Prefab description
 * @return Prefab ID or -1 on failure
 */
int create_prefab_from_selection(const char* name, const char* description)
{
    if (g_selection.count == 0) {
        editor_log(1, "No objects selected for prefab creation");
        return -1;
    }
    
    if (g_prefab_count >= MAX_PREFABS) {
        editor_log(2, "Maximum prefab count reached");
        return -1;
    }
    
    Prefab* prefab = &g_prefabs[g_prefab_count];
    strncpy(prefab->name, name, sizeof(prefab->name) - 1);
    strncpy(prefab->description, description, sizeof(prefab->description) - 1);
    
    // Copy selected object IDs
    prefab->object_count = fminf(g_selection.count, MAX_SELECTION);
    for (int i = 0; i < prefab->object_count; i++) {
        prefab->object_ids[i] = g_selection.object_ids[i];
    }
    
    // Calculate bounds
    memcpy(prefab->bounds_min, g_selection.bounds_min, sizeof(float) * 3);
    memcpy(prefab->bounds_max, g_selection.bounds_max, sizeof(float) * 3);
    
    // Set metadata
    prefab->creation_date = time(NULL);
    strncpy(prefab->author, "Editor User", sizeof(prefab->author) - 1);
    sprintf(prefab->tags, "objects:%d", prefab->object_count);
    
    // Generate thumbnail path
    sprintf(prefab->thumbnail_path, "thumbnails/prefab_%d.bmp", g_prefab_count);
    
    // TODO: Generate prefab thumbnail
    
    editor_log(0, "Created prefab: %s with %d objects", name, prefab->object_count);
    return g_prefab_count++;
}

/**
 * Instantiates a prefab
 * @param prefab_id Prefab ID
 * @param x X position
 * @param y Y position
 * @param z Z position
 * @return Root object ID or -1 on failure
 */
int instantiate_prefab(int prefab_id, float x, float y, float z)
{
    if (prefab_id < 0 || prefab_id >= g_prefab_count) {
        return -1;
    }
    
    Prefab* prefab = &g_prefabs[prefab_id];
    
    // Calculate offset from prefab center
    float prefab_center[3] = {
        (prefab->bounds_min[0] + prefab->bounds_max[0]) * 0.5f,
        (prefab->bounds_min[1] + prefab->bounds_max[1]) * 0.5f,
        (prefab->bounds_min[2] + prefab->bounds_max[2]) * 0.5f
    };
    
    float offset[3] = {
        x - prefab_center[0],
        y - prefab_center[1],
        z - prefab_center[2]
    };
    
    // Clear selection
    clear_selection();
    
    begin_undo_group("Instantiate Prefab");
    
    // Create prefab instance parent
    int parent_id = create_editor_object(EDITOR_OBJ_PREFAB_INSTANCE, x, y, z);
    if (parent_id < 0) {
        end_undo_group();
        return -1;
    }
    
    EditorObject* parent = &g_editor_objects[parent_id - 1];
    sprintf(parent->name, "%s_Instance", prefab->name);
    parent->properties.prefab.prefab_id = prefab_id;
    parent->properties.prefab.instance_seed = (float)rand() / RAND_MAX;
    
    // Duplicate all prefab objects
    for (int i = 0; i < prefab->object_count; i++) {
        EditorObject* template_obj = find_editor_object(prefab->object_ids[i]);
        if (!template_obj) continue;
        
        int new_id = create_editor_object(template_obj->type,
                                         template_obj->position[0] + offset[0],
                                         template_obj->position[1] + offset[1],
                                         template_obj->position[2] + offset[2]);
        
        if (new_id >= 0) {
            EditorObject* new_obj = &g_editor_objects[new_id - 1];
            
            // Copy properties
            memcpy(new_obj->rotation, template_obj->rotation, sizeof(float) * 3);
            memcpy(new_obj->scale, template_obj->scale, sizeof(float) * 3);
            new_obj->asset_id = template_obj->asset_id;
            new_obj->material_id = template_obj->material_id;
            new_obj->parent_id = parent_id;
            
            // Update asset reference count
            if (new_obj->asset_id >= 0) {
                g_asset_cache[new_obj->asset_id].reference_count++;
            }
            
            add_to_selection(new_id);
        }
    }
    
    end_undo_group();
    
    editor_log(0, "Instantiated prefab: %s at (%.2f, %.2f, %.2f)", 
              prefab->name, x, y, z);
    
    return parent_id;
}

// ========================================================================
// UNDO/REDO SYSTEM
// ========================================================================

/**
 * Begins an undo group
 * @param description Group description
 */
void begin_undo_group(const char* description)
{
    g_undo_group_depth++;
    
    if (g_undo_group_depth == 1) {
        // Create group action
        UndoAction* group = (UndoAction*)calloc(1, sizeof(UndoAction));
        group->type = UNDO_TYPE_BATCH;
        strncpy(group->description, description, sizeof(group->description) - 1);
        group->timestamp = time(NULL);
        
        add_undo_action_data(group);
    }
}

/**
 * Ends an undo group
 */
void end_undo_group(void)
{
    g_undo_group_depth--;
    if (g_undo_group_depth < 0) {
        g_undo_group_depth = 0;
    }
}

/**
 * Adds an undo action
 * @param type Action type
 * @param object_id Object ID
 * @param old_state Old object state
 * @param new_state New object state
 */
void add_undo_action(UndoType type, int object_id, EditorObject* old_state, EditorObject* new_state)
{
    UndoAction* action = (UndoAction*)calloc(1, sizeof(UndoAction));
    action->type = type;
    action->timestamp = time(NULL);
    
    // Set description based on type
    switch (type) {
        case UNDO_TYPE_OBJECT_CREATE:
            sprintf(action->description, "Create %s", new_state ? new_state->name : "Object");
            break;
        case UNDO_TYPE_OBJECT_DELETE:
            sprintf(action->description, "Delete %s", old_state ? old_state->name : "Object");
            break;
        case UNDO_TYPE_OBJECT_MODIFY:
            sprintf(action->description, "Modify %s", old_state ? old_state->name : "Object");
            break;
        default:
            strcpy(action->description, "Unknown Action");
            break;
    }
    
    // Store object data
    action->object_count = 1;
    action->object_ids = (int*)malloc(sizeof(int));
    action->object_ids[0] = object_id;
    
    if (old_state) {
        action->old_states = (EditorObject*)malloc(sizeof(EditorObject));
        memcpy(action->old_states, old_state, sizeof(EditorObject));
    }
    
    if (new_state) {
        action->new_states = (EditorObject*)malloc(sizeof(EditorObject));
        memcpy(action->new_states, new_state, sizeof(EditorObject));
    }
    
    add_undo_action_data(action);
}

/**
 * Adds undo action data to stack
 * @param action Action to add
 */
void add_undo_action_data(UndoAction* action)
{
    // Remove any redo actions
    while (g_undo_count > g_undo_head) {
        g_undo_count--;
        int idx = (g_undo_tail + g_undo_count) % g_undo_capacity;
        free_undo_action(&g_undo_stack[idx]);
    }
    
    // Check capacity
    if (g_undo_count >= g_config.max_undo_steps) {
        // Remove oldest action
        free_undo_action(&g_undo_stack[g_undo_tail]);
        g_undo_tail = (g_undo_tail + 1) % g_undo_capacity;
        g_undo_count--;
    }
    
    // Add new action
    int idx = (g_undo_tail + g_undo_head) % g_undo_capacity;
    
    // If we're in a group, add as sub-action
    if (g_undo_group_depth > 0 && g_undo_head > 0) {
        int group_idx = (g_undo_tail + g_undo_head - 1) % g_undo_capacity;
        UndoAction* group = &g_undo_stack[group_idx];
        
        if (group->type == UNDO_TYPE_BATCH) {
            // Add as sub-action
            group->sub_action_count++;
            group->sub_actions = (UndoAction*)realloc(group->sub_actions, 
                                                      group->sub_action_count * sizeof(UndoAction));
            memcpy(&group->sub_actions[group->sub_action_count - 1], action, sizeof(UndoAction));
            free(action);
            return;
        }
    }
    
    // Add as top-level action
    memcpy(&g_undo_stack[idx], action, sizeof(UndoAction));
    free(action);
    
    g_undo_head++;
    g_undo_count++;
}

/**
 * Performs undo
 */
void undo(void)
{
    if (g_undo_head == 0) {
        editor_log(1, "Nothing to undo");
        return;
    }
    
    g_undo_head--;
    int idx = (g_undo_tail + g_undo_head) % g_undo_capacity;
    UndoAction* action = &g_undo_stack[idx];
    
    editor_log(0, "Undo: %s", action->description);
    apply_undo_action(action, 1);
    
    g_stats.changes_since_save++;
}

/**
 * Performs redo
 */
void redo(void)
{
    if (g_undo_head >= g_undo_count) {
        editor_log(1, "Nothing to redo");
        return;
    }
    
    int idx = (g_undo_tail + g_undo_head) % g_undo_capacity;
    UndoAction* action = &g_undo_stack[idx];
    
    editor_log(0, "Redo: %s", action->description);
    apply_undo_action(action, 0);
    
    g_undo_head++;
    g_stats.changes_since_save++;
}

/**
 * Applies an undo action
 * @param action Action to apply
 * @param is_undo Whether this is an undo (vs redo)
 */
void apply_undo_action(UndoAction* action, int is_undo)
{
    switch (action->type) {
        case UNDO_TYPE_OBJECT_CREATE:
            if (is_undo) {
                // Delete the created object
                delete_editor_object(action->object_ids[0]);
            } else {
                // Recreate the object
                if (action->new_states) {
                    // Find a free slot
                    if (g_object_count < g_object_capacity) {
                        memcpy(&g_editor_objects[g_object_count], 
                               action->new_states, sizeof(EditorObject));
                        g_object_count++;
                    }
                }
            }
            break;
            
        case UNDO_TYPE_OBJECT_DELETE:
            if (is_undo) {
                // Recreate the deleted object
                if (action->old_states && g_object_count < g_object_capacity) {
                    memcpy(&g_editor_objects[g_object_count], 
                           action->old_states, sizeof(EditorObject));
                    g_object_count++;
                }
            } else {
                // Delete the object again
                delete_editor_object(action->object_ids[0]);
            }
            break;
            
        case UNDO_TYPE_OBJECT_MODIFY:
            {
                EditorObject* obj = find_editor_object(action->object_ids[0]);
                if (obj) {
                    EditorObject temp = *obj;
                    if (is_undo && action->old_states) {
                        *obj = *action->old_states;
                    } else if (!is_undo && action->new_states) {
                        *obj = *action->new_states;
                    }
                }
            }
            break;
            
        case UNDO_TYPE_TERRAIN_MODIFY:
            if (action->terrain_backup) {
                // Swap terrain data
                int x1 = action->terrain_region[0];
                int z1 = action->terrain_region[1];
                int x2 = action->terrain_region[2];
                int z2 = action->terrain_region[3];
                int region_width = x2 - x1 + 1;
                
                for (int z = z1; z <= z2; z++) {
                    for (int x = x1; x <= x2; x++) {
                        int backup_idx = (z - z1) * region_width + (x - x1);
                        int terrain_idx = z * g_terrain.size_x + x;
                        
                        float temp = g_terrain.heights[terrain_idx];
                        g_terrain.heights[terrain_idx] = action->terrain_backup[backup_idx];
                        action->terrain_backup[backup_idx] = temp;
                    }
                }
                
                // Update normals and chunks
                update_terrain_normals(x1, x2, z1, z2);
                mark_terrain_chunks_dirty(x1, x2, z1, z2);
            }
            break;
            
        case UNDO_TYPE_BATCH:
            // Apply sub-actions in reverse order for undo
            if (is_undo) {
                for (int i = action->sub_action_count - 1; i >= 0; i--) {
                    apply_undo_action(&action->sub_actions[i], is_undo);
                }
            } else {
                for (int i = 0; i < action->sub_action_count; i++) {
                    apply_undo_action(&action->sub_actions[i], is_undo);
                }
            }
            break;
    }
    
    // Update selection bounds if needed
    if (action->type != UNDO_TYPE_TERRAIN_MODIFY) {
        update_selection_bounds();
    }
}

/**
 * Frees undo action memory
 * @param action Action to free
 */
void free_undo_action(UndoAction* action)
{
    if (action->object_ids) {
        free(action->object_ids);
    }
    if (action->old_states) {
        free(action->old_states);
    }
    if (action->new_states) {
        free(action->new_states);
    }
    if (action->terrain_backup) {
        free(action->terrain_backup);
    }
    if (action->sub_actions) {
        for (int i = 0; i < action->sub_action_count; i++) {
            free_undo_action(&action->sub_actions[i]);
        }
        free(action->sub_actions);
    }
}

// ========================================================================
// FILE I/O
// ========================================================================

/**
 * Saves level to file
 * @param filename Output filename
 * @return 1 on success, 0 on failure
 */
int save_level(const char* filename)
{
    FILE* file = fopen(filename, "w");
    if (!file) {
        editor_log(2, "Failed to create level file: %s", filename);
        return 0;
    }
    
    editor_log(0, "Saving level: %s", filename);
    
    // Write header
    fprintf(file, "[ENDOR_LEVEL]\n");
    fprintf(file, "version=2.0\n");
    fprintf(file, "name=Level\n");
    fprintf(file, "author=Editor User\n");
    fprintf(file, "date=%s\n", get_timestamp());
    fprintf(file, "\n");
    
    // Write terrain data
    save_terrain_data(file);
    
    // Write layers
    fprintf(file, "[LAYERS]\n");
    fprintf(file, "count=%d\n", g_layer_count);
    for (int i = 0; i < g_layer_count; i++) {
        fprintf(file, "\n[LAYER_%d]\n", i);
        fprintf(file, "name=%s\n", g_layers[i].name);
        fprintf(file, "visible=%d\n", g_layers[i].visible);
        fprintf(file, "locked=%d\n", g_layers[i].locked);
        fprintf(file, "selectable=%d\n", g_layers[i].selectable);
        fprintf(file, "color=%.3f,%.3f,%.3f,%.3f\n", 
                g_layers[i].color[0], g_layers[i].color[1], 
                g_layers[i].color[2], g_layers[i].color[3]);
        fprintf(file, "parent=%d\n", g_layers[i].parent_layer);
    }
    fprintf(file, "\n");
    
    // Write objects
    save_object_data(file);
    
    // Write camera settings
    fprintf(file, "[CAMERA]\n");
    fprintf(file, "position=%.3f,%.3f,%.3f\n", 
            g_camera.position[0], g_camera.position[1], g_camera.position[2]);
    fprintf(file, "target=%.3f,%.3f,%.3f\n", 
            g_camera.target[0], g_camera.target[1], g_camera.target[2]);
    fprintf(file, "rotation=%.3f,%.3f\n", g_camera.rotation[0], g_camera.rotation[1]);
    fprintf(file, "distance=%.3f\n", g_camera.distance);
    fprintf(file, "fov=%.3f\n", g_camera.fov);
    fprintf(file, "\n");
    
    fclose(file);
    
    // Save current filename
    strncpy(g_current_level_file, filename, sizeof(g_current_level_file) - 1);
    g_stats.changes_since_save = 0;
    
    editor_log(0, "Level saved successfully");
    return 1;
}

/**
 * Saves terrain data to file
 * @param file Output file
 */
void save_terrain_data(FILE* file)
{
    fprintf(file, "[TERRAIN]\n");
    fprintf(file, "size=%d,%d\n", g_terrain.size_x, g_terrain.size_z);
    fprintf(file, "scale=%.3f\n", g_terrain.scale);
    fprintf(file, "vertical_scale=%.3f\n", g_terrain.vertical_scale);
    fprintf(file, "detail_texture=%d\n", g_terrain.detail_texture_id);
    fprintf(file, "detail_scale=%.3f\n", g_terrain.detail_scale);
    fprintf(file, "\n");
    
    // Write texture layers
    fprintf(file, "[TERRAIN_TEXTURES]\n");
    for (int i = 0; i < 8; i++) {
        if (g_terrain.texture_ids[i] >= 0) {
            fprintf(file, "texture_%d=%d,%.3f\n", i, 
                    g_terrain.texture_ids[i], g_terrain.texture_scales[i]);
        }
    }
    fprintf(file, "\n");
    
    // Write heightmap data (compressed)
    fprintf(file, "[TERRAIN_HEIGHTS]\n");
    fprintf(file, "compression=RLE\n");
    
    // Simple RLE compression
    int count = 0;
    float last_height = g_terrain.heights[0];
    
    for (int i = 0; i < g_terrain.size_x * g_terrain.size_z; i++) {
        if (fabsf(g_terrain.heights[i] - last_height) < 0.001f && count < 255) {
            count++;
        } else {
            fprintf(file, "%d,%.3f,", count, last_height);
            last_height = g_terrain.heights[i];
            count = 1;
        }
    }
    fprintf(file, "%d,%.3f\n", count, last_height);
    fprintf(file, "\n");
}

/**
 * Saves object data to file
 * @param file Output file
 */
void save_object_data(FILE* file)
{
    fprintf(file, "[OBJECTS]\n");
    fprintf(file, "count=%d\n", g_object_count);
    fprintf(file, "\n");
    
    for (int i = 0; i < g_object_count; i++) {
        EditorObject* obj = &g_editor_objects[i];
        
        fprintf(file, "[OBJECT_%d]\n", obj->id);
        fprintf(file, "type=%d\n", obj->type);
        fprintf(file, "name=%s\n", obj->name);
        fprintf(file, "position=%.3f,%.3f,%.3f\n", 
                obj->position[0], obj->position[1], obj->position[2]);
        fprintf(file, "rotation=%.3f,%.3f,%.3f\n", 
                obj->rotation[0], obj->rotation[1], obj->rotation[2]);
        fprintf(file, "scale=%.3f,%.3f,%.3f\n", 
                obj->scale[0], obj->scale[1], obj->scale[2]);
        fprintf(file, "layer=%d\n", obj->layer_id);
        fprintf(file, "parent=%d\n", obj->parent_id);
        fprintf(file, "visible=%d\n", obj->visible);
        fprintf(file, "locked=%d\n", obj->locked);
        fprintf(file, "static=%d\n", obj->static_object);
        
        if (obj->asset_id >= 0) {
            fprintf(file, "asset=%s\n", g_asset_cache[obj->asset_id].filename);
        }
        if (obj->material_id >= 0) {
            fprintf(file, "material=%d\n", obj->material_id);
        }
        
        // Save type-specific properties
        save_object_properties(file, obj);
        
        fprintf(file, "\n");
    }
}

/**
 * Saves object type-specific properties
 * @param file Output file
 * @param obj Object to save
 */
void save_object_properties(FILE* file, EditorObject* obj)
{
    switch (obj->type) {
        case EDITOR_OBJ_LIGHT:
            fprintf(file, "light_type=%d\n", obj->properties.light.light_type);
            fprintf(file, "light_color=%.3f,%.3f,%.3f\n", 
                    obj->properties.light.color[0], 
                    obj->properties.light.color[1], 
                    obj->properties.light.color[2]);
            fprintf(file, "light_intensity=%.3f\n", obj->properties.light.intensity);
            fprintf(file, "light_range=%.3f\n", obj->properties.light.range);
            fprintf(file, "cast_shadows=%d\n", obj->properties.light.cast_shadows);
            fprintf(file, "shadow_resolution=%d\n", obj->properties.light.shadow_resolution);
            break;
            
        case EDITOR_OBJ_SPAWN_POINT:
            fprintf(file, "spawn_team=%d\n", obj->properties.spawn.team);
            fprintf(file, "spawn_type=%d\n", obj->properties.spawn.spawn_type);
            fprintf(file, "spawn_radius=%.3f\n", obj->properties.spawn.spawn_radius);
            fprintf(file, "spawn_priority=%d\n", obj->properties.spawn.priority);
            break;
            
        case EDITOR_OBJ_TRIGGER:
            fprintf(file, "trigger_bounds=%.3f,%.3f,%.3f,%.3f,%.3f,%.3f\n",
                    obj->properties.trigger.bounds[0], obj->properties.trigger.bounds[1],
                    obj->properties.trigger.bounds[2], obj->properties.trigger.bounds[3],
                    obj->properties.trigger.bounds[4], obj->properties.trigger.bounds[5]);
            fprintf(file, "trigger_script=%s\n", obj->properties.trigger.script);
            fprintf(file, "trigger_once=%d\n", obj->properties.trigger.trigger_once);
            fprintf(file, "trigger_delay=%d\n", obj->properties.trigger.trigger_delay);
            break;
            
        // Add other object types as needed
    }
}

/**
 * Loads level from file
 * @param filename Input filename
 * @return 1 on success, 0 on failure
 */
int load_level(const char* filename)
{
    FILE* file = fopen(filename, "r");
    if (!file) {
        editor_log(2, "Failed to open level file: %s", filename);
        return 0;
    }
    
    editor_log(0, "Loading level: %s", filename);
    
    // Clear existing data
    clear_selection();
    g_object_count = 0;
    g_layer_count = 1;  // Keep default layer
    
    char line[1024];
    char section[64] = "";
    int current_object_id = -1;
    EditorObject* current_object = NULL;
    
    while (fgets(line, sizeof(line), file)) {
        // Remove newline
        char* newline = strchr(line, '\n');
        if (newline) *newline = '\0';
        
        // Skip empty lines and comments
        if (line[0] == '\0' || line[0] == '#' || line[0] == ';') {
            continue;
        }
        
        // Check for section header
        if (line[0] == '[') {
            char* end = strchr(line, ']');
            if (end) {
                *end = '\0';
                strncpy(section, line + 1, sizeof(section) - 1);
                
                // Check if it's an object section
                if (strncmp(section, "OBJECT_", 7) == 0) {
                    current_object_id = atoi(section + 7);
                    // Create new object
                    if (g_object_count < g_object_capacity) {
                        current_object = &g_editor_objects[g_object_count++];
                        memset(current_object, 0, sizeof(EditorObject));
                        current_object->id = current_object_id;
                        current_object->scale[0] = current_object->scale[1] = current_object->scale[2] = 1.0f;
                        current_object->visible = 1;
                        current_object->asset_id = -1;
                        current_object->material_id = -1;
                        current_object->parent_id = -1;
                    }
                } else {
                    current_object = NULL;
                }
            }
            continue;
        }
        
        // Parse key=value
        char* equals = strchr(line, '=');
        if (!equals) continue;
        
        *equals = '\0';
        char* key = line;
        char* value = equals + 1;
        
        // Process based on section
        if (strcmp(section, "TERRAIN") == 0) {
            load_terrain_property(key, value);
        } else if (current_object != NULL) {
            load_object_property(current_object, key, value);
        }
        // Add other sections as needed
    }
    
    fclose(file);
    
    // Update terrain normals
    update_terrain_normals(0, g_terrain.size_x - 1, 0, g_terrain.size_z - 1);
    
    // Update statistics
    g_stats.total_objects = g_object_count;
    g_stats.changes_since_save = 0;
    
    // Save current filename
    strncpy(g_current_level_file, filename, sizeof(g_current_level_file) - 1);
    
    editor_log(0, "Level loaded successfully");
    return 1;
}

/**
 * Loads terrain property from file
 * @param key Property key
 * @param value Property value
 */
void load_terrain_property(const char* key, const char* value)
{
    if (strcmp(key, "size") == 0) {
        int width, height;
        if (sscanf(value, "%d,%d", &width, &height) == 2) {
            resize_terrain(width, height);
        }
    } else if (strcmp(key, "scale") == 0) {
        g_terrain.scale = atof(value);
    } else if (strcmp(key, "vertical_scale") == 0) {
        g_terrain.vertical_scale = atof(value);
    }
    // Add other terrain properties
}

/**
 * Loads object property from file
 * @param obj Object to load into
 * @param key Property key
 * @param value Property value
 */
void load_object_property(EditorObject* obj, const char* key, const char* value)
{
    if (strcmp(key, "type") == 0) {
        obj->type = atoi(value);
    } else if (strcmp(key, "name") == 0) {
        strncpy(obj->name, value, sizeof(obj->name) - 1);
    } else if (strcmp(key, "position") == 0) {
        sscanf(value, "%f,%f,%f", &obj->position[0], &obj->position[1], &obj->position[2]);
    } else if (strcmp(key, "rotation") == 0) {
        sscanf(value, "%f,%f,%f", &obj->rotation[0], &obj->rotation[1], &obj->rotation[2]);
    } else if (strcmp(key, "scale") == 0) {
        sscanf(value, "%f,%f,%f", &obj->scale[0], &obj->scale[1], &obj->scale[2]);
    } else if (strcmp(key, "layer") == 0) {
        obj->layer_id = atoi(value);
    } else if (strcmp(key, "parent") == 0) {
        obj->parent_id = atoi(value);
    } else if (strcmp(key, "visible") == 0) {
        obj->visible = atoi(value);
    } else if (strcmp(key, "locked") == 0) {
        obj->locked = atoi(value);
    } else if (strcmp(key, "static") == 0) {
        obj->static_object = atoi(value);
    } else if (strcmp(key, "asset") == 0) {
        // Find or load asset
        for (int i = 0; i < g_asset_count; i++) {
            if (strcmp(g_asset_cache[i].filename, value) == 0) {
                obj->asset_id = i;
                g_asset_cache[i].reference_count++;
                break;
            }
        }
    } else {
        // Type-specific properties
        load_object_type_property(obj, key, value);
    }
    
    // Update bounds
    calculate_object_bounds(obj);
}

/**
 * Loads object type-specific property
 * @param obj Object to load into
 * @param key Property key
 * @param value Property value
 */
void load_object_type_property(EditorObject* obj, const char* key, const char* value)
{
    switch (obj->type) {
        case EDITOR_OBJ_LIGHT:
            if (strcmp(key, "light_type") == 0) {
                obj->properties.light.light_type = atoi(value);
            } else if (strcmp(key, "light_color") == 0) {
                sscanf(value, "%f,%f,%f", 
                       &obj->properties.light.color[0],
                       &obj->properties.light.color[1],
                       &obj->properties.light.color[2]);
            } else if (strcmp(key, "light_intensity") == 0) {
                obj->properties.light.intensity = atof(value);
            } else if (strcmp(key, "light_range") == 0) {
                obj->properties.light.range = atof(value);
            } else if (strcmp(key, "cast_shadows") == 0) {
                obj->properties.light.cast_shadows = atoi(value);
            } else if (strcmp(key, "shadow_resolution") == 0) {
                obj->properties.light.shadow_resolution = atoi(value);
            }
            break;
            
        case EDITOR_OBJ_SPAWN_POINT:
            if (strcmp(key, "spawn_team") == 0) {
                obj->properties.spawn.team = atoi(value);
            } else if (strcmp(key, "spawn_type") == 0) {
                obj->properties.spawn.spawn_type = atoi(value);
            } else if (strcmp(key, "spawn_radius") == 0) {
                obj->properties.spawn.spawn_radius = atof(value);
            } else if (strcmp(key, "spawn_priority") == 0) {
                obj->properties.spawn.priority = atoi(value);
            }
            break;
            
        // Add other object types
    }
}

/**
 * Checks and performs autosave
 */
void check_autosave(void)
{
    if (!g_config.autosave_enabled || g_stats.changes_since_save == 0) {
        return;
    }
    
    time_t now = time(NULL);
    if (now - g_last_autosave >= g_config.autosave_interval / 1000) {
        char autosave_file[512];
        if (strlen(g_current_level_file) > 0) {
            // Create autosave based on current file
            char* dot = strrchr(g_current_level_file, '.');
            if (dot) {
                *dot = '\0';
                sprintf(autosave_file, "%s_autosave.lvl", g_current_level_file);
                *dot = '.';
            } else {
                sprintf(autosave_file, "%s_autosave.lvl", g_current_level_file);
            }
        } else {
            sprintf(autosave_file, "autosave_%ld.lvl", now);
        }
        
        if (save_level(autosave_file)) {
            g_last_autosave = now;
            editor_log(0, "Autosaved to: %s", autosave_file);
        }
    }
}

/**
 * Exports optimized level for game use
 * @param filename Output filename
 * @return 1 on success, 0 on failure
 */
int export_level_optimized(const char* filename)
{
    FILE* file = fopen(filename, "wb");
    if (!file) {
        editor_log(2, "Failed to create export file: %s", filename);
        return 0;
    }
    
    editor_log(0, "Exporting optimized level: %s", filename);
    
    // Write binary header
    unsigned int magic = 0x4C444E45;  // "ENDL"
    unsigned int version = 0x00020000;  // 2.0
    fwrite(&magic, sizeof(unsigned int), 1, file);
    fwrite(&version, sizeof(unsigned int), 1, file);
    
    // TODO: Write optimized binary data
    // - Packed terrain data
    // - Optimized object placement
    // - Baked lighting data
    // - Collision data
    // - Navigation mesh
    
    fclose(file);
    
    editor_log(0, "Level exported successfully");
    return 1;
}

// ========================================================================
// MATERIAL SYSTEM
// ========================================================================

/**
 * Creates default materials
 */
void create_default_materials(void)
{
    // Default material
    create_material("Default", 0, 3, -1, -1);  // White diffuse, flat normal
    
    // Basic materials
    create_material("Stone", -1, -1, -1, -1);
    create_material("Metal", -1, -1, -1, -1);
    create_material("Wood", -1, -1, -1, -1);
    create_material("Glass", -1, -1, -1, -1);
}

/**
 * Creates a new material
 * @param name Material name
 * @param diffuse_tex Diffuse texture ID
 * @param normal_tex Normal texture ID
 * @param specular_tex Specular texture ID
 * @param emissive_tex Emissive texture ID
 * @return Material ID or -1 on failure
 */
int create_material(const char* name, int diffuse_tex, int normal_tex, 
                   int specular_tex, int emissive_tex)
{
    if (g_material_count >= g_material_capacity) {
        // Expand array
        int new_capacity = g_material_capacity * 2;
        Material* new_materials = (Material*)realloc(g_materials, 
                                                    new_capacity * sizeof(Material));
        if (!new_materials) {
            editor_log(2, "Failed to expand material array");
            return -1;
        }
        g_materials = new_materials;
        g_material_capacity = new_capacity;
    }
    
    Material* mat = &g_materials[g_material_count];
    memset(mat, 0, sizeof(Material));
    
    mat->diffuse_texture_id = diffuse_tex;
    mat->normal_texture_id = normal_tex;
    mat->specular_texture_id = specular_tex;
    mat->emissive_texture_id = emissive_tex;
    
    // Default colors
    mat->diffuse_color[0] = mat->diffuse_color[1] = mat->diffuse_color[2] = 1.0f;
    mat->diffuse_color[3] = 1.0f;
    mat->specular_color[0] = mat->specular_color[1] = mat->specular_color[2] = 0.5f;
    mat->specular_color[3] = 1.0f;
    mat->emissive_color[0] = mat->emissive_color[1] = mat->emissive_color[2] = 0.0f;
    mat->emissive_color[3] = 1.0f;
    
    // Default properties
    mat->roughness = 0.5f;
    mat->metallic = 0.0f;
    mat->opacity = 1.0f;
    mat->normal_strength = 1.0f;
    mat->blend_mode = 0;  // Opaque
    mat->two_sided = 0;
    mat->cast_shadows = 1;
    
    strncpy(mat->shader_name, "standard", sizeof(mat->shader_name) - 1);
    
    editor_log(0, "Created material: %s", name);
    return g_material_count++;
}

// ========================================================================
// UTILITY FUNCTIONS
// ========================================================================

/**
 * Gets current timestamp string
 * @return Timestamp string
 */
const char* get_timestamp(void)
{
    static char timestamp[32];
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_info);
    return timestamp;
}

/**
 * Gets filename without path
 * @param path Full path
 * @return Filename portion
 */
const char* get_filename_without_path(const char* path)
{
    const char* filename = strrchr(path, '\\');
    if (!filename) {
        filename = strrchr(path, '/');
    }
    return filename ? filename + 1 : path;
}

/**
 * Checks if a key is pressed
 * @param key Virtual key code
 * @return 1 if pressed, 0 otherwise
 */
int is_key_pressed(int key)
{
    return (GetAsyncKeyState(key) & 0x8000) != 0;
}

/**
 * Creates default prefabs
 */
void create_default_prefabs(void)
{
    // TODO: Create some default prefabs
    // - Basic light setup
    // - Spawn point setup
    // - Common object arrangements
}

/**
 * Loads editor configuration
 * @param filename Configuration filename
 */
void load_editor_config(const char* filename)
{
    FILE* file = fopen(filename, "r");
    if (!file) {
        editor_log(1, "No editor config found, using defaults");
        return;
    }
    
    char line[256];
    while (fgets(line, sizeof(line), file)) {
        char* newline = strchr(line, '\n');
        if (newline) *newline = '\0';
        
        char* equals = strchr(line, '=');
        if (!equals) continue;
        
        *equals = '\0';
        char* key = line;
        char* value = equals + 1;
        
        // Parse configuration values
        if (strcmp(key, "autosave_enabled") == 0) {
            g_config.autosave_enabled = atoi(value);
        } else if (strcmp(key, "autosave_interval") == 0) {
            g_config.autosave_interval = atoi(value);
        } else if (strcmp(key, "camera_speed") == 0) {
            g_config.camera_speed_multiplier = atof(value);
        } else if (strcmp(key, "grid_size") == 0) {
            g_config.grid_size = atof(value);
        } else if (strcmp(key, "snap_angle") == 0) {
            g_config.snap_angle = atof(value);
        }
        // Add other config options
    }
    
    fclose(file);
    editor_log(0, "Loaded editor configuration");
}

/**
 * Saves editor configuration
 * @param filename Configuration filename
 */
void save_editor_config(const char* filename)
{
    FILE* file = fopen(filename, "w");
    if (!file) {
        editor_log(2, "Failed to save editor config");
        return;
    }
    
    fprintf(file, "# Endor Level Editor Configuration\n");
    fprintf(file, "autosave_enabled=%d\n", g_config.autosave_enabled);
    fprintf(file, "autosave_interval=%d\n", g_config.autosave_interval);
    fprintf(file, "camera_speed=%.3f\n", g_config.camera_speed_multiplier);
    fprintf(file, "grid_size=%.3f\n", g_config.grid_size);
    fprintf(file, "snap_angle=%.3f\n", g_config.snap_angle);
    fprintf(file, "show_grid=%d\n", g_config.show_grid);
    fprintf(file, "show_gizmos=%d\n", g_config.show_gizmos);
    fprintf(file, "show_statistics=%d\n", g_config.show_statistics);
    
    fclose(file);
    editor_log(0, "Saved editor configuration");
}

/**
 * Updates list of visible objects
 */
void update_visible_objects(void)
{
    g_visible_object_count = 0;
    
    for (int i = 0; i < g_object_count; i++) {
        EditorObject* obj = &g_editor_objects[i];
        
        // Check visibility
        if (!obj->visible || !g_layers[obj->layer_id].visible) {
            continue;
        }
        
        // TODO: Frustum culling
        if (g_frustum_cull_enabled) {
            // Check if object is in view frustum
        }
        
        // Add to visible list
        if (g_visible_object_count < g_object_capacity) {
            g_visible_objects[g_visible_object_count++] = i;
        }
    }
    
    g_stats.visible_objects = g_visible_object_count;
}

/**
 * Cleans up the level editor
 */
void cleanup_level_editor(void)
{
    if (!g_editor_initialized) {
        return;
    }
    
    editor_log(0, "Cleaning up level editor");
    
    // Save configuration
    save_editor_config("editor_config.ini");
    
    // Free dynamic memory
    if (g_editor_objects) {
        // Free type-specific data
        for (int i = 0; i < g_object_count; i++) {
            if (g_editor_objects[i].type == EDITOR_OBJ_SPLINE && 
                g_editor_objects[i].properties.spline.control_points) {
                free(g_editor_objects[i].properties.spline.control_points);
            }
            if (g_editor_objects[i].custom_properties) {
                free(g_editor_objects[i].custom_properties);
            }
        }
        free(g_editor_objects);
    }
    
    if (g_asset_cache) {
        // Free asset data
        for (int i = 0; i < g_asset_count; i++) {
            if (g_asset_cache[i].data) {
                free(g_asset_cache[i].data);
            }
            if (g_asset_cache[i].thumbnail_data) {
                free(g_asset_cache[i].thumbnail_data);
            }
            // Free dependency list
            AssetDependency* dep = g_asset_cache[i].dependencies;
            while (dep) {
                AssetDependency* next = dep->next;
                free(dep);
                dep = next;
            }
        }
        free(g_asset_cache);
    }
    
    if (g_materials) {
        free(g_materials);
    }
    
    if (g_undo_stack) {
        // Free undo actions
        for (int i = 0; i < g_undo_capacity; i++) {
            free_undo_action(&g_undo_stack[i]);
        }
        free(g_undo_stack);
    }
    
    if (g_visible_objects) {
        free(g_visible_objects);
    }
    
    if (g_selection.object_ids) {
        free(g_selection.object_ids);
    }
    
    if (g_selection.lasso_points) {
        free(g_selection.lasso_points);
    }
    
    // Free terrain data
    if (g_terrain.heights) free(g_terrain.heights);
    if (g_terrain.normals) free(g_terrain.normals);
    if (g_terrain.blend_weights) free(g_terrain.blend_weights);
    if (g_terrain.vegetation_map) free(g_terrain.vegetation_map);
    if (g_terrain.water_map) free(g_terrain.water_map);
    if (g_terrain.chunks) free(g_terrain.chunks);
    
    g_editor_initialized = 0;
    
    editor_log(0, "Level editor cleanup complete");
}

// ========================================================================
// PUBLIC API
// ========================================================================

/**
 * Main editor update function
 * @param delta_time Frame delta time in seconds
 */
void update_level_editor(float delta_time)
{
    if (!g_editor_initialized) {
        return;
    }
    
    // Update camera
    update_editor_camera(delta_time);
    
    // Update visible objects
    update_visible_objects();
    
    // Check for hot-reload
    static float hot_reload_timer = 0.0f;
    hot_reload_timer += delta_time;
    if (hot_reload_timer > 1.0f) {
        hot_reload_timer = 0.0f;
        for (int i = 0; i < g_asset_count; i++) {
            check_asset_hot_reload(i);
        }
    }
    
    // Check autosave
    check_autosave();
    
    // Update statistics
    g_stats.last_frame_time = delta_time;
}

/**
 * Renders the level editor
 */
void render_level_editor(void)
{
    if (!g_editor_initialized) {
        return;
    }
    
    // TODO: Implement rendering
    // - Render terrain
    // - Render objects
    // - Render selection
    // - Render gizmos
    // - Render grid
    // - Render UI
}

/**
 * Handles editor input
 * @param msg Windows message
 * @param wparam WPARAM
 * @param lparam LPARAM
 * @return 1 if handled, 0 otherwise
 */
int handle_editor_input(UINT msg, WPARAM wparam, LPARAM lparam)
{
    if (!g_editor_initialized) {
        return 0;
    }
    
    switch (msg) {
        case WM_KEYDOWN:
            switch (wparam) {
                case 'Z':
                    if (GetAsyncKeyState(VK_CONTROL) & 0x8000) {
                        if (GetAsyncKeyState(VK_SHIFT) & 0x8000) {
                            redo();
                        } else {
                            undo();
                        }
                        return 1;
                    }
                    break;
                    
                case 'S':
                    if (GetAsyncKeyState(VK_CONTROL) & 0x8000) {
                        if (strlen(g_current_level_file) > 0) {
                            save_level(g_current_level_file);
                        }
                        return 1;
                    }
                    break;
                    
                case VK_DELETE:
                    if (g_selection.count > 0) {
                        begin_undo_group("Delete Selection");
                        for (int i = g_selection.count - 1; i >= 0; i--) {
                            delete_editor_object(g_selection.object_ids[i]);
                        }
                        end_undo_group();
                        return 1;
                    }
                    break;
                    
                case VK_F2:
                    focus_camera_on_selection();
                    return 1;
            }
            break;
            
        case WM_LBUTTONDOWN:
        case WM_RBUTTONDOWN:
        case WM_MBUTTONDOWN:
            // Handle mouse input
            break;
            
        case WM_MOUSEWHEEL:
            {
                short wheel_delta = GET_WHEEL_DELTA_WPARAM(wparam);
                camera_mouse_input(0, 0, 0, wheel_delta / WHEEL_DELTA);
                return 1;
            }
            break;
    }
    
    return 0;
}
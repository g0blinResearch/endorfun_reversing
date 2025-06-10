/**
 * ========================================================================
 * ENDOR LEVEL EDITOR AND ASSET MANAGEMENT SYSTEM
 * ========================================================================
 * 
 * Advanced level editor with real-time editing capabilities, asset browser,
 * terrain editing, entity placement, lighting setup, and export functionality.
 * Includes comprehensive asset management with texture loading, model import,
 * and resource optimization.
 */

#include "endor_readable.h"
#include <math.h>
#include <stdio.h>
#include <direct.h> // For directory operations on Windows

// ========================================================================
// EDITOR CONSTANTS AND STRUCTURES
// ========================================================================

#define MAX_EDITOR_OBJECTS 1024
#define MAX_TERRAIN_SIZE 128
#define MAX_ASSET_CACHE 512
#define MAX_UNDO_STACK 100
#define MAX_SELECTION 64
#define GRID_SIZE 1.0f
#define SNAP_THRESHOLD 0.5f

// Editor modes
typedef enum {
    EDITOR_MODE_SELECT,
    EDITOR_MODE_MOVE,
    EDITOR_MODE_ROTATE,
    EDITOR_MODE_SCALE,
    EDITOR_MODE_TERRAIN,
    EDITOR_MODE_PAINT,
    EDITOR_MODE_LIGHTING,
    EDITOR_MODE_CAMERA
} EditorMode;

// Asset types
typedef enum {
    ASSET_TYPE_TEXTURE,
    ASSET_TYPE_MODEL,
    ASSET_TYPE_SOUND,
    ASSET_TYPE_SCRIPT,
    ASSET_TYPE_MATERIAL,
    ASSET_TYPE_PREFAB,
    ASSET_TYPE_ANIMATION
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
    EDITOR_OBJ_DECAL
} EditorObjectType;

// Asset structure
typedef struct {
    char filename[256];
    char display_name[64];
    AssetType type;
    int loaded;
    void* data;
    size_t data_size;
    int reference_count;
    double last_modified;
    char thumbnail_path[256];
} Asset;

// Editor object structure
typedef struct {
    int id;
    EditorObjectType type;
    float position[3];
    float rotation[3];
    float scale[3];
    char name[64];
    int asset_id;
    int selected;
    int visible;
    int locked;
    
    // Type-specific properties
    union {
        struct { // Static mesh
            int material_id;
            int cast_shadows;
            int receive_shadows;
        } mesh;
        
        struct { // Light
            float color[3];
            float intensity;
            float range;
            int light_type; // 0=directional, 1=point, 2=spot
            float spot_angle;
        } light;
        
        struct { // Spawn point
            int team;
            int spawn_type;
        } spawn;
        
        struct { // Trigger
            float bounds[6]; // min_x, min_y, min_z, max_x, max_y, max_z
            char script[256];
            int trigger_once;
        } trigger;
    } properties;
} EditorObject;

// Terrain structure
typedef struct {
    float heights[MAX_TERRAIN_SIZE][MAX_TERRAIN_SIZE];
    int texture_ids[4]; // Up to 4 blended textures
    float texture_scales[4];
    float blend_map[MAX_TERRAIN_SIZE][MAX_TERRAIN_SIZE][4];
    int size;
    float scale;
} TerrainData;

// Undo/Redo system
typedef enum {
    UNDO_TYPE_OBJECT_CREATE,
    UNDO_TYPE_OBJECT_DELETE,
    UNDO_TYPE_OBJECT_MODIFY,
    UNDO_TYPE_TERRAIN_MODIFY,
    UNDO_TYPE_SELECTION_CHANGE
} UndoType;

typedef struct {
    UndoType type;
    int object_id;
    EditorObject old_state;
    EditorObject new_state;
    float terrain_backup[MAX_TERRAIN_SIZE][MAX_TERRAIN_SIZE];
    int terrain_region[4]; // x1, y1, x2, y2
} UndoAction;

// Editor camera
typedef struct {
    float position[3];
    float target[3];
    float rotation[2]; // yaw, pitch
    float distance;
    float speed;
    int free_camera;
} EditorCamera;

// Selection system
typedef struct {
    int object_ids[MAX_SELECTION];
    int count;
    float bounds_min[3];
    float bounds_max[3];
    float center[3];
} Selection;

// ========================================================================
// GLOBAL EDITOR STATE
// ========================================================================

static EditorObject editor_objects[MAX_EDITOR_OBJECTS];
static int object_count = 0;
static int next_object_id = 1;

static Asset asset_cache[MAX_ASSET_CACHE];
static int asset_count = 0;

static TerrainData terrain;
static EditorCamera camera;
static Selection selection;

static UndoAction undo_stack[MAX_UNDO_STACK];
static int undo_head = 0;
static int undo_tail = 0;
static int undo_count = 0;

static EditorMode current_mode = EDITOR_MODE_SELECT;
static int grid_enabled = 1;
static int snap_enabled = 1;
static float grid_size = GRID_SIZE;
static int show_gizmos = 1;
static int show_wireframe = 0;
static int show_bounds = 0;

// Tool state
static int dragging = 0;
static float drag_start[3];
static float drag_offset[3];
static int last_mouse_x = 0;
static int last_mouse_y = 0;

// ========================================================================
// CORE EDITOR FUNCTIONS
// ========================================================================

void initialize_level_editor() {
    // Clear all objects
    memset(editor_objects, 0, sizeof(editor_objects));
    object_count = 0;
    next_object_id = 1;
    
    // Clear asset cache
    memset(asset_cache, 0, sizeof(asset_cache));
    asset_count = 0;
    
    // Initialize terrain
    memset(&terrain, 0, sizeof(TerrainData));
    terrain.size = 64;
    terrain.scale = 1.0f;
    for (int i = 0; i < 4; i++) {
        terrain.texture_ids[i] = -1;
        terrain.texture_scales[i] = 1.0f;
    }
    
    // Initialize camera
    camera.position[0] = 0.0f;
    camera.position[1] = 10.0f;
    camera.position[2] = -10.0f;
    camera.target[0] = 0.0f;
    camera.target[1] = 0.0f;
    camera.target[2] = 0.0f;
    camera.rotation[0] = 0.0f; // yaw
    camera.rotation[1] = -30.0f; // pitch
    camera.distance = 15.0f;
    camera.speed = 10.0f;
    camera.free_camera = 0;
    
    // Clear selection
    memset(&selection, 0, sizeof(Selection));
    
    // Clear undo stack
    memset(undo_stack, 0, sizeof(undo_stack));
    undo_head = 0;
    undo_tail = 0;
    undo_count = 0;
    
    // Load default assets
    load_default_assets();
}

void load_default_assets() {
    // Load basic primitive models
    load_asset("models/cube.obj", "Cube", ASSET_TYPE_MODEL);
    load_asset("models/sphere.obj", "Sphere", ASSET_TYPE_MODEL);
    load_asset("models/plane.obj", "Plane", ASSET_TYPE_MODEL);
    load_asset("models/cylinder.obj", "Cylinder", ASSET_TYPE_MODEL);
    
    // Load default textures
    load_asset("textures/default.bmp", "Default", ASSET_TYPE_TEXTURE);
    load_asset("textures/grass.bmp", "Grass", ASSET_TYPE_TEXTURE);
    load_asset("textures/stone.bmp", "Stone", ASSET_TYPE_TEXTURE);
    load_asset("textures/metal.bmp", "Metal", ASSET_TYPE_TEXTURE);
    
    // Load default materials
    create_default_materials();
}

int load_asset(const char* filename, const char* display_name, AssetType type) {
    if (asset_count >= MAX_ASSET_CACHE) {
        return -1;
    }
    
    // Check if already loaded
    for (int i = 0; i < asset_count; i++) {
        if (strcmp(asset_cache[i].filename, filename) == 0) {
            asset_cache[i].reference_count++;
            return i;
        }
    }
    
    Asset* asset = &asset_cache[asset_count];
    strcpy(asset->filename, filename);
    strcpy(asset->display_name, display_name);
    asset->type = type;
    asset->loaded = 0;
    asset->data = NULL;
    asset->data_size = 0;
    asset->reference_count = 1;
    asset->last_modified = 0.0;
    
    // Generate thumbnail path
    sprintf(asset->thumbnail_path, "thumbnails/%s.bmp", display_name);
    
    // Load the actual asset data
    if (load_asset_data(asset)) {
        asset->loaded = 1;
        return asset_count++;
    }
    
    return -1;
}

int load_asset_data(Asset* asset) {
    FILE* file = fopen(asset->filename, "rb");
    if (!file) {
        return 0;
    }
    
    // Get file size
    fseek(file, 0, SEEK_END);
    size_t file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    // Allocate memory
    asset->data = malloc(file_size);
    if (!asset->data) {
        fclose(file);
        return 0;
    }
    
    // Read file
    size_t bytes_read = fread(asset->data, 1, file_size, file);
    fclose(file);
    
    if (bytes_read != file_size) {
        free(asset->data);
        asset->data = NULL;
        return 0;
    }
    
    asset->data_size = file_size;
    
    // Process asset based on type
    switch (asset->type) {
        case ASSET_TYPE_TEXTURE:
            return process_texture_asset(asset);
            
        case ASSET_TYPE_MODEL:
            return process_model_asset(asset);
            
        case ASSET_TYPE_SOUND:
            return process_sound_asset(asset);
            
        default:
            return 1; // Basic file loading successful
    }
}

int process_texture_asset(Asset* asset) {
    // Basic BMP texture processing
    if (asset->data_size < 54) {
        return 0; // Too small for BMP header
    }
    
    unsigned char* data = (unsigned char*)asset->data;
    
    // Check BMP signature
    if (data[0] != 'B' || data[1] != 'M') {
        return 0;
    }
    
    // Extract width and height
    int width = *(int*)(data + 18);
    int height = *(int*)(data + 22);
    
    if (width <= 0 || height <= 0 || width > 2048 || height > 2048) {
        return 0;
    }
    
    // Generate thumbnail if needed
    generate_texture_thumbnail(asset, width, height);
    
    return 1;
}

int process_model_asset(Asset* asset) {
    // Basic OBJ model processing
    char* data = (char*)asset->data;
    
    // Simple validation - check for OBJ format indicators
    if (strstr(data, "v ") == NULL && strstr(data, "f ") == NULL) {
        return 0; // Not a valid OBJ file
    }
    
    // Count vertices and faces for validation
    int vertex_count = 0;
    int face_count = 0;
    
    char* line = strtok(data, "\n");
    while (line != NULL) {
        if (strncmp(line, "v ", 2) == 0) {
            vertex_count++;
        } else if (strncmp(line, "f ", 2) == 0) {
            face_count++;
        }
        line = strtok(NULL, "\n");
    }
    
    if (vertex_count == 0 || face_count == 0) {
        return 0;
    }
    
    // Generate thumbnail
    generate_model_thumbnail(asset);
    
    return 1;
}

int process_sound_asset(Asset* asset) {
    // Basic WAV sound processing
    if (asset->data_size < 44) {
        return 0; // Too small for WAV header
    }
    
    unsigned char* data = (unsigned char*)asset->data;
    
    // Check WAV signature
    if (memcmp(data, "RIFF", 4) != 0 || memcmp(data + 8, "WAVE", 4) != 0) {
        return 0;
    }
    
    return 1;
}

void generate_texture_thumbnail(Asset* asset, int width, int height) {
    // Create a simple 64x64 thumbnail
    const int thumb_size = 64;
    unsigned char* thumb_data = (unsigned char*)malloc(thumb_size * thumb_size * 3);
    
    if (!thumb_data) return;
    
    // Simple downsampling (very basic)
    unsigned char* src_data = (unsigned char*)asset->data + 54; // Skip BMP header
    
    for (int y = 0; y < thumb_size; y++) {
        for (int x = 0; x < thumb_size; x++) {
            int src_x = (x * width) / thumb_size;
            int src_y = (y * height) / thumb_size;
            int src_index = (src_y * width + src_x) * 3;
            int thumb_index = (y * thumb_size + x) * 3;
            
            if (src_index + 2 < asset->data_size - 54) {
                thumb_data[thumb_index] = src_data[src_index];
                thumb_data[thumb_index + 1] = src_data[src_index + 1];
                thumb_data[thumb_index + 2] = src_data[src_index + 2];
            }
        }
    }
    
    // Save thumbnail (simplified)
    save_thumbnail_bmp(asset->thumbnail_path, thumb_data, thumb_size, thumb_size);
    
    free(thumb_data);
}

void generate_model_thumbnail(Asset* asset) {
    // Generate a simple wireframe thumbnail for models
    // This would involve parsing the OBJ and rendering to a small bitmap
    // For now, just create a placeholder
    
    const int thumb_size = 64;
    unsigned char* thumb_data = (unsigned char*)malloc(thumb_size * thumb_size * 3);
    
    if (!thumb_data) return;
    
    // Fill with a pattern indicating it's a model
    for (int i = 0; i < thumb_size * thumb_size * 3; i += 3) {
        thumb_data[i] = 100;     // R
        thumb_data[i + 1] = 100; // G
        thumb_data[i + 2] = 150; // B
    }
    
    save_thumbnail_bmp(asset->thumbnail_path, thumb_data, thumb_size, thumb_size);
    free(thumb_data);
}

void save_thumbnail_bmp(const char* filename, unsigned char* data, int width, int height) {
    FILE* file = fopen(filename, "wb");
    if (!file) return;
    
    // BMP header
    unsigned char header[54] = {
        'B', 'M', 0, 0, 0, 0, 0, 0, 0, 0, 54, 0, 0, 0, 40, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 24, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    };
    
    int file_size = 54 + width * height * 3;
    header[2] = file_size & 0xFF;
    header[3] = (file_size >> 8) & 0xFF;
    header[4] = (file_size >> 16) & 0xFF;
    header[5] = (file_size >> 24) & 0xFF;
    
    header[18] = width & 0xFF;
    header[19] = (width >> 8) & 0xFF;
    header[20] = (width >> 16) & 0xFF;
    header[21] = (width >> 24) & 0xFF;
    
    header[22] = height & 0xFF;
    header[23] = (height >> 8) & 0xFF;
    header[24] = (height >> 16) & 0xFF;
    header[25] = (height >> 24) & 0xFF;
    
    fwrite(header, 1, 54, file);
    fwrite(data, 1, width * height * 3, file);
    fclose(file);
}

// ========================================================================
// OBJECT MANAGEMENT
// ========================================================================

int create_editor_object(EditorObjectType type, float x, float y, float z) {
    if (object_count >= MAX_EDITOR_OBJECTS) {
        return -1;
    }
    
    EditorObject* obj = &editor_objects[object_count];
    
    obj->id = next_object_id++;
    obj->type = type;
    obj->position[0] = x;
    obj->position[1] = y;
    obj->position[2] = z;
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
    
    // Set default name and properties based on type
    switch (type) {
        case EDITOR_OBJ_STATIC_MESH:
            sprintf(obj->name, "StaticMesh_%d", obj->id);
            obj->properties.mesh.material_id = -1;
            obj->properties.mesh.cast_shadows = 1;
            obj->properties.mesh.receive_shadows = 1;
            break;
            
        case EDITOR_OBJ_LIGHT:
            sprintf(obj->name, "Light_%d", obj->id);
            obj->properties.light.color[0] = 1.0f;
            obj->properties.light.color[1] = 1.0f;
            obj->properties.light.color[2] = 1.0f;
            obj->properties.light.intensity = 1.0f;
            obj->properties.light.range = 10.0f;
            obj->properties.light.light_type = 1; // Point light
            obj->properties.light.spot_angle = 45.0f;
            break;
            
        case EDITOR_OBJ_SPAWN_POINT:
            sprintf(obj->name, "SpawnPoint_%d", obj->id);
            obj->properties.spawn.team = 0;
            obj->properties.spawn.spawn_type = 0;
            break;
            
        case EDITOR_OBJ_TRIGGER:
            sprintf(obj->name, "Trigger_%d", obj->id);
            obj->properties.trigger.bounds[0] = x - 1.0f;
            obj->properties.trigger.bounds[1] = y - 1.0f;
            obj->properties.trigger.bounds[2] = z - 1.0f;
            obj->properties.trigger.bounds[3] = x + 1.0f;
            obj->properties.trigger.bounds[4] = y + 1.0f;
            obj->properties.trigger.bounds[5] = z + 1.0f;
            strcpy(obj->properties.trigger.script, "");
            obj->properties.trigger.trigger_once = 0;
            break;
            
        default:
            sprintf(obj->name, "Object_%d", obj->id);
            break;
    }
    
    // Add to undo stack
    add_undo_action(UNDO_TYPE_OBJECT_CREATE, obj->id, NULL, obj);
    
    return object_count++;
}

void delete_editor_object(int object_id) {
    for (int i = 0; i < object_count; i++) {
        if (editor_objects[i].id == object_id) {
            // Add to undo stack
            add_undo_action(UNDO_TYPE_OBJECT_DELETE, object_id, &editor_objects[i], NULL);
            
            // Remove from selection if selected
            remove_from_selection(object_id);
            
            // Shift objects down
            for (int j = i; j < object_count - 1; j++) {
                editor_objects[j] = editor_objects[j + 1];
            }
            object_count--;
            break;
        }
    }
}

EditorObject* find_editor_object(int object_id) {
    for (int i = 0; i < object_count; i++) {
        if (editor_objects[i].id == object_id) {
            return &editor_objects[i];
        }
    }
    return NULL;
}

void duplicate_selected_objects() {
    int selected_ids[MAX_SELECTION];
    int selected_count = 0;
    
    // Collect selected object IDs
    for (int i = 0; i < object_count && selected_count < MAX_SELECTION; i++) {
        if (editor_objects[i].selected) {
            selected_ids[selected_count++] = editor_objects[i].id;
        }
    }
    
    // Duplicate each selected object
    for (int i = 0; i < selected_count; i++) {
        EditorObject* original = find_editor_object(selected_ids[i]);
        if (original) {
            int new_index = create_editor_object(original->type, 
                                               original->position[0] + 2.0f, 
                                               original->position[1], 
                                               original->position[2] + 2.0f);
            
            if (new_index >= 0) {
                EditorObject* duplicate = &editor_objects[new_index];
                
                // Copy properties
                memcpy(duplicate->rotation, original->rotation, sizeof(float) * 3);
                memcpy(duplicate->scale, original->scale, sizeof(float) * 3);
                duplicate->asset_id = original->asset_id;
                duplicate->properties = original->properties;
                
                // Update name
                sprintf(duplicate->name, "%s_Copy", original->name);
            }
        }
    }
}

// ========================================================================
// SELECTION SYSTEM
// ========================================================================

void clear_selection() {
    for (int i = 0; i < object_count; i++) {
        editor_objects[i].selected = 0;
    }
    selection.count = 0;
    update_selection_bounds();
}

void add_to_selection(int object_id) {
    EditorObject* obj = find_editor_object(object_id);
    if (obj && !obj->selected && selection.count < MAX_SELECTION) {
        obj->selected = 1;
        selection.object_ids[selection.count++] = object_id;
        update_selection_bounds();
    }
}

void remove_from_selection(int object_id) {
    EditorObject* obj = find_editor_object(object_id);
    if (obj && obj->selected) {
        obj->selected = 0;
        
        // Remove from selection array
        for (int i = 0; i < selection.count; i++) {
            if (selection.object_ids[i] == object_id) {
                for (int j = i; j < selection.count - 1; j++) {
                    selection.object_ids[j] = selection.object_ids[j + 1];
                }
                selection.count--;
                break;
            }
        }
        
        update_selection_bounds();
    }
}

void toggle_selection(int object_id) {
    EditorObject* obj = find_editor_object(object_id);
    if (obj) {
        if (obj->selected) {
            remove_from_selection(object_id);
        } else {
            add_to_selection(object_id);
        }
    }
}

void select_all() {
    clear_selection();
    for (int i = 0; i < object_count && selection.count < MAX_SELECTION; i++) {
        if (editor_objects[i].visible && !editor_objects[i].locked) {
            add_to_selection(editor_objects[i].id);
        }
    }
}

void update_selection_bounds() {
    if (selection.count == 0) {
        memset(selection.bounds_min, 0, sizeof(float) * 3);
        memset(selection.bounds_max, 0, sizeof(float) * 3);
        memset(selection.center, 0, sizeof(float) * 3);
        return;
    }
    
    // Initialize bounds with first selected object
    EditorObject* first_obj = find_editor_object(selection.object_ids[0]);
    if (!first_obj) return;
    
    memcpy(selection.bounds_min, first_obj->position, sizeof(float) * 3);
    memcpy(selection.bounds_max, first_obj->position, sizeof(float) * 3);
    
    // Expand bounds to include all selected objects
    for (int i = 1; i < selection.count; i++) {
        EditorObject* obj = find_editor_object(selection.object_ids[i]);
        if (obj) {
            for (int j = 0; j < 3; j++) {
                if (obj->position[j] < selection.bounds_min[j]) {
                    selection.bounds_min[j] = obj->position[j];
                }
                if (obj->position[j] > selection.bounds_max[j]) {
                    selection.bounds_max[j] = obj->position[j];
                }
            }
        }
    }
    
    // Calculate center
    for (int i = 0; i < 3; i++) {
        selection.center[i] = (selection.bounds_min[i] + selection.bounds_max[i]) * 0.5f;
    }
}

// ========================================================================
// CAMERA SYSTEM
// ========================================================================

void update_editor_camera(float delta_time) {
    if (camera.free_camera) {
        // Free camera movement
        float speed = camera.speed * delta_time;
        
        // Forward/backward movement
        float forward_x = sin(camera.rotation[0] * M_PI / 180.0f);
        float forward_z = cos(camera.rotation[0] * M_PI / 180.0f);
        
        // Right/left movement
        float right_x = sin((camera.rotation[0] + 90.0f) * M_PI / 180.0f);
        float right_z = cos((camera.rotation[0] + 90.0f) * M_PI / 180.0f);
        
        // Apply movement based on input
        if (is_key_pressed('W')) {
            camera.position[0] += forward_x * speed;
            camera.position[2] += forward_z * speed;
        }
        if (is_key_pressed('S')) {
            camera.position[0] -= forward_x * speed;
            camera.position[2] -= forward_z * speed;
        }
        if (is_key_pressed('A')) {
            camera.position[0] -= right_x * speed;
            camera.position[2] -= right_z * speed;
        }
        if (is_key_pressed('D')) {
            camera.position[0] += right_x * speed;
            camera.position[2] += right_z * speed;
        }
        if (is_key_pressed('Q')) {
            camera.position[1] -= speed;
        }
        if (is_key_pressed('E')) {
            camera.position[1] += speed;
        }
        
        // Update target based on camera rotation
        camera.target[0] = camera.position[0] + forward_x * 10.0f;
        camera.target[1] = camera.position[1] + sin(camera.rotation[1] * M_PI / 180.0f) * 10.0f;
        camera.target[2] = camera.position[2] + forward_z * 10.0f;
    } else {
        // Orbit camera around target
        float orbit_x = sin(camera.rotation[0] * M_PI / 180.0f) * cos(camera.rotation[1] * M_PI / 180.0f);
        float orbit_y = sin(camera.rotation[1] * M_PI / 180.0f);
        float orbit_z = cos(camera.rotation[0] * M_PI / 180.0f) * cos(camera.rotation[1] * M_PI / 180.0f);
        
        camera.position[0] = camera.target[0] + orbit_x * camera.distance;
        camera.position[1] = camera.target[1] + orbit_y * camera.distance;
        camera.position[2] = camera.target[2] + orbit_z * camera.distance;
    }
}

void focus_camera_on_selection() {
    if (selection.count == 0) return;
    
    // Set target to selection center
    memcpy(camera.target, selection.center, sizeof(float) * 3);
    
    // Calculate appropriate distance based on selection bounds
    float size_x = selection.bounds_max[0] - selection.bounds_min[0];
    float size_y = selection.bounds_max[1] - selection.bounds_min[1];
    float size_z = selection.bounds_max[2] - selection.bounds_min[2];
    float max_size = fmax(size_x, fmax(size_y, size_z));
    
    camera.distance = fmax(5.0f, max_size * 2.0f);
}

// ========================================================================
// TERRAIN EDITING
// ========================================================================

void modify_terrain_height(float world_x, float world_z, float delta, float radius, int smooth) {
    int terrain_x = (int)((world_x / terrain.scale) + terrain.size * 0.5f);
    int terrain_z = (int)((world_z / terrain.scale) + terrain.size * 0.5f);
    
    if (terrain_x < 0 || terrain_x >= terrain.size || terrain_z < 0 || terrain_z >= terrain.size) {
        return;
    }
    
    // Calculate affected region
    int radius_cells = (int)(radius / terrain.scale) + 1;
    int min_x = fmax(0, terrain_x - radius_cells);
    int max_x = fmin(terrain.size - 1, terrain_x + radius_cells);
    int min_z = fmax(0, terrain_z - radius_cells);
    int max_z = fmin(terrain.size - 1, terrain_z + radius_cells);
    
    // Backup terrain for undo
    UndoAction undo;
    undo.type = UNDO_TYPE_TERRAIN_MODIFY;
    undo.terrain_region[0] = min_x;
    undo.terrain_region[1] = min_z;
    undo.terrain_region[2] = max_x;
    undo.terrain_region[3] = max_z;
    
    for (int z = min_z; z <= max_z; z++) {
        for (int x = min_x; x <= max_x; x++) {
            undo.terrain_backup[z][x] = terrain.heights[z][x];
        }
    }
    
    // Apply height modification
    for (int z = min_z; z <= max_z; z++) {
        for (int x = min_x; x <= max_x; x++) {
            float dx = (x - terrain_x) * terrain.scale;
            float dz = (z - terrain_z) * terrain.scale;
            float distance = sqrt(dx * dx + dz * dz);
            
            if (distance <= radius) {
                float influence = 1.0f - (distance / radius);
                if (smooth) {
                    influence = influence * influence * (3.0f - 2.0f * influence); // Smoothstep
                }
                
                terrain.heights[z][x] += delta * influence;
                terrain.heights[z][x] = fmax(-50.0f, fmin(50.0f, terrain.heights[z][x]));
            }
        }
    }
    
    add_undo_action_data(&undo);
}

void paint_terrain_texture(float world_x, float world_z, int texture_layer, float intensity, float radius) {
    int terrain_x = (int)((world_x / terrain.scale) + terrain.size * 0.5f);
    int terrain_z = (int)((world_z / terrain.scale) + terrain.size * 0.5f);
    
    if (terrain_x < 0 || terrain_x >= terrain.size || terrain_z < 0 || terrain_z >= terrain.size) {
        return;
    }
    
    if (texture_layer < 0 || texture_layer >= 4) {
        return;
    }
    
    int radius_cells = (int)(radius / terrain.scale) + 1;
    int min_x = fmax(0, terrain_x - radius_cells);
    int max_x = fmin(terrain.size - 1, terrain_x + radius_cells);
    int min_z = fmax(0, terrain_z - radius_cells);
    int max_z = fmin(terrain.size - 1, terrain_z + radius_cells);
    
    for (int z = min_z; z <= max_z; z++) {
        for (int x = min_x; x <= max_x; x++) {
            float dx = (x - terrain_x) * terrain.scale;
            float dz = (z - terrain_z) * terrain.scale;
            float distance = sqrt(dx * dx + dz * dz);
            
            if (distance <= radius) {
                float influence = (1.0f - (distance / radius)) * intensity;
                
                // Increase target layer
                terrain.blend_map[z][x][texture_layer] += influence;
                terrain.blend_map[z][x][texture_layer] = fmin(1.0f, terrain.blend_map[z][x][texture_layer]);
                
                // Normalize blend weights
                float total_weight = 0.0f;
                for (int i = 0; i < 4; i++) {
                    total_weight += terrain.blend_map[z][x][i];
                }
                
                if (total_weight > 1.0f) {
                    for (int i = 0; i < 4; i++) {
                        terrain.blend_map[z][x][i] /= total_weight;
                    }
                }
            }
        }
    }
}

void smooth_terrain(float world_x, float world_z, float radius, int iterations) {
    for (int iter = 0; iter < iterations; iter++) {
        modify_terrain_height(world_x, world_z, 0.0f, radius, 1);
        
        // Additional smoothing pass
        int terrain_x = (int)((world_x / terrain.scale) + terrain.size * 0.5f);
        int terrain_z = (int)((world_z / terrain.scale) + terrain.size * 0.5f);
        int radius_cells = (int)(radius / terrain.scale);
        
        for (int z = terrain_z - radius_cells; z <= terrain_z + radius_cells; z++) {
            for (int x = terrain_x - radius_cells; x <= terrain_x + radius_cells; x++) {
                if (x > 0 && x < terrain.size - 1 && z > 0 && z < terrain.size - 1) {
                    float avg = (terrain.heights[z-1][x] + terrain.heights[z+1][x] +
                               terrain.heights[z][x-1] + terrain.heights[z][x+1] +
                               terrain.heights[z][x]) / 5.0f;
                    terrain.heights[z][x] = (terrain.heights[z][x] + avg) * 0.5f;
                }
            }
        }
    }
}

// ========================================================================
// UNDO/REDO SYSTEM
// ========================================================================

void add_undo_action(UndoType type, int object_id, EditorObject* old_state, EditorObject* new_state) {
    UndoAction* action = &undo_stack[undo_head];
    
    action->type = type;
    action->object_id = object_id;
    
    if (old_state) {
        action->old_state = *old_state;
    }
    if (new_state) {
        action->new_state = *new_state;
    }
    
    undo_head = (undo_head + 1) % MAX_UNDO_STACK;
    
    if (undo_count < MAX_UNDO_STACK) {
        undo_count++;
    } else {
        undo_tail = (undo_tail + 1) % MAX_UNDO_STACK;
    }
}

void add_undo_action_data(UndoAction* action) {
    undo_stack[undo_head] = *action;
    undo_head = (undo_head + 1) % MAX_UNDO_STACK;
    
    if (undo_count < MAX_UNDO_STACK) {
        undo_count++;
    } else {
        undo_tail = (undo_tail + 1) % MAX_UNDO_STACK;
    }
}

void undo_last_action() {
    if (undo_count == 0) return;
    
    undo_head = (undo_head - 1 + MAX_UNDO_STACK) % MAX_UNDO_STACK;
    UndoAction* action = &undo_stack[undo_head];
    
    switch (action->type) {
        case UNDO_TYPE_OBJECT_CREATE:
            delete_editor_object(action->object_id);
            break;
            
        case UNDO_TYPE_OBJECT_DELETE:
            // Restore deleted object
            if (object_count < MAX_EDITOR_OBJECTS) {
                editor_objects[object_count++] = action->old_state;
            }
            break;
            
        case UNDO_TYPE_OBJECT_MODIFY:
            {
                EditorObject* obj = find_editor_object(action->object_id);
                if (obj) {
                    *obj = action->old_state;
                }
            }
            break;
            
        case UNDO_TYPE_TERRAIN_MODIFY:
            // Restore terrain region
            for (int z = action->terrain_region[1]; z <= action->terrain_region[3]; z++) {
                for (int x = action->terrain_region[0]; x <= action->terrain_region[2]; x++) {
                    terrain.heights[z][x] = action->terrain_backup[z][x];
                }
            }
            break;
    }
    
    undo_count--;
}

// ========================================================================
// FILE I/O SYSTEM
// ========================================================================

void save_level(const char* filename) {
    FILE* file = fopen(filename, "w");
    if (!file) return;
    
    fprintf(file, "# Endor Level File v1.0\n");
    fprintf(file, "# Generated by Endor Level Editor\n\n");
    
    // Save terrain data
    fprintf(file, "[TERRAIN]\n");
    fprintf(file, "size=%d\n", terrain.size);
    fprintf(file, "scale=%.2f\n", terrain.scale);
    
    for (int i = 0; i < 4; i++) {
        if (terrain.texture_ids[i] >= 0) {
            fprintf(file, "texture%d=%s\n", i, asset_cache[terrain.texture_ids[i]].filename);
            fprintf(file, "texture%d_scale=%.2f\n", i, terrain.texture_scales[i]);
        }
    }
    
    fprintf(file, "heights=");
    for (int z = 0; z < terrain.size; z++) {
        for (int x = 0; x < terrain.size; x++) {
            fprintf(file, "%.2f", terrain.heights[z][x]);
            if (x < terrain.size - 1 || z < terrain.size - 1) {
                fprintf(file, ",");
            }
        }
    }
    fprintf(file, "\n\n");
    
    // Save objects
    fprintf(file, "[OBJECTS]\n");
    fprintf(file, "count=%d\n", object_count);
    
    for (int i = 0; i < object_count; i++) {
        EditorObject* obj = &editor_objects[i];
        
        fprintf(file, "\n[OBJECT_%d]\n", obj->id);
        fprintf(file, "type=%d\n", obj->type);
        fprintf(file, "name=%s\n", obj->name);
        fprintf(file, "position=%.2f,%.2f,%.2f\n", obj->position[0], obj->position[1], obj->position[2]);
        fprintf(file, "rotation=%.2f,%.2f,%.2f\n", obj->rotation[0], obj->rotation[1], obj->rotation[2]);
        fprintf(file, "scale=%.2f,%.2f,%.2f\n", obj->scale[0], obj->scale[1], obj->scale[2]);
        fprintf(file, "visible=%d\n", obj->visible);
        fprintf(file, "locked=%d\n", obj->locked);
        
        if (obj->asset_id >= 0) {
            fprintf(file, "asset=%s\n", asset_cache[obj->asset_id].filename);
        }
        
        // Save type-specific properties
        switch (obj->type) {
            case EDITOR_OBJ_LIGHT:
                fprintf(file, "light_color=%.2f,%.2f,%.2f\n", 
                       obj->properties.light.color[0], 
                       obj->properties.light.color[1], 
                       obj->properties.light.color[2]);
                fprintf(file, "light_intensity=%.2f\n", obj->properties.light.intensity);
                fprintf(file, "light_range=%.2f\n", obj->properties.light.range);
                fprintf(file, "light_type=%d\n", obj->properties.light.light_type);
                break;
                
            case EDITOR_OBJ_TRIGGER:
                fprintf(file, "trigger_bounds=%.2f,%.2f,%.2f,%.2f,%.2f,%.2f\n",
                       obj->properties.trigger.bounds[0], obj->properties.trigger.bounds[1],
                       obj->properties.trigger.bounds[2], obj->properties.trigger.bounds[3],
                       obj->properties.trigger.bounds[4], obj->properties.trigger.bounds[5]);
                fprintf(file, "trigger_script=%s\n", obj->properties.trigger.script);
                fprintf(file, "trigger_once=%d\n", obj->properties.trigger.trigger_once);
                break;
        }
    }
    
    fclose(file);
}

int load_level(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) return 0;
    
    // Clear current level
    initialize_level_editor();
    
    char line[512];
    char section[64] = "";
    EditorObject* current_obj = NULL;
    
    while (fgets(line, sizeof(line), file)) {
        // Remove newline
        line[strcspn(line, "\n")] = 0;
        
        // Skip comments and empty lines
        if (line[0] == '#' || line[0] == '\0') continue;
        
        // Check for section headers
        if (line[0] == '[') {
            strcpy(section, line);
            
            // Check for object section
            if (strncmp(line, "[OBJECT_", 8) == 0) {
                int obj_id = atoi(line + 8);
                int index = create_editor_object(EDITOR_OBJ_STATIC_MESH, 0, 0, 0);
                if (index >= 0) {
                    current_obj = &editor_objects[index];
                    current_obj->id = obj_id;
                }
            } else {
                current_obj = NULL;
            }
            continue;
        }
        
        // Parse key=value pairs
        char* equals = strchr(line, '=');
        if (!equals) continue;
        
        *equals = '\0';
        char* key = line;
        char* value = equals + 1;
        
        // Process based on current section
        if (strcmp(section, "[TERRAIN]") == 0) {
            if (strcmp(key, "size") == 0) {
                terrain.size = atoi(value);
            } else if (strcmp(key, "scale") == 0) {
                terrain.scale = atof(value);
            } else if (strncmp(key, "texture", 7) == 0 && !strstr(key, "_scale")) {
                int tex_index = key[7] - '0';
                if (tex_index >= 0 && tex_index < 4) {
                    terrain.texture_ids[tex_index] = load_asset(value, value, ASSET_TYPE_TEXTURE);
                }
            } else if (strcmp(key, "heights") == 0) {
                // Parse height data (simplified)
                char* token = strtok(value, ",");
                int index = 0;
                while (token && index < terrain.size * terrain.size) {
                    int z = index / terrain.size;
                    int x = index % terrain.size;
                    terrain.heights[z][x] = atof(token);
                    token = strtok(NULL, ",");
                    index++;
                }
            }
        } else if (current_obj) {
            // Parse object properties
            if (strcmp(key, "type") == 0) {
                current_obj->type = (EditorObjectType)atoi(value);
            } else if (strcmp(key, "name") == 0) {
                strcpy(current_obj->name, value);
            } else if (strcmp(key, "position") == 0) {
                sscanf(value, "%f,%f,%f", &current_obj->position[0], &current_obj->position[1], &current_obj->position[2]);
            } else if (strcmp(key, "rotation") == 0) {
                sscanf(value, "%f,%f,%f", &current_obj->rotation[0], &current_obj->rotation[1], &current_obj->rotation[2]);
            } else if (strcmp(key, "scale") == 0) {
                sscanf(value, "%f,%f,%f", &current_obj->scale[0], &current_obj->scale[1], &current_obj->scale[2]);
            } else if (strcmp(key, "asset") == 0) {
                current_obj->asset_id = load_asset(value, value, ASSET_TYPE_MODEL);
            }
            // Additional property parsing would go here
        }
    }
    
    fclose(file);
    return 1;
}

void create_default_materials() {
    // Create basic materials for editor use
    // This would involve setting up default material properties
    // For now, just placeholder implementation
}

// ========================================================================
// UTILITY FUNCTIONS
// ========================================================================

void set_editor_mode(EditorMode mode) {
    current_mode = mode;
    clear_selection(); // Clear selection when changing modes
}

EditorMode get_editor_mode() {
    return current_mode;
}

void toggle_grid() {
    grid_enabled = !grid_enabled;
}

void toggle_snap() {
    snap_enabled = !snap_enabled;
}

void set_grid_size(float size) {
    grid_size = fmax(0.1f, fmin(10.0f, size));
}

float snap_to_grid(float value) {
    if (!snap_enabled) return value;
    return round(value / grid_size) * grid_size;
}

int get_object_count() {
    return object_count;
}

EditorObject* get_editor_objects() {
    return editor_objects;
}

Asset* get_asset_cache() {
    return asset_cache;
}

int get_asset_count() {
    return asset_count;
}

void cleanup_level_editor() {
    // Free all asset data
    for (int i = 0; i < asset_count; i++) {
        if (asset_cache[i].data) {
            free(asset_cache[i].data);
        }
    }
    
    // Clear all states
    object_count = 0;
    asset_count = 0;
    undo_count = 0;
}
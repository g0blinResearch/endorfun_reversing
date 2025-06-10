/**
 * ========================================================================
 * ENDOR INPUT AND CONTROLS SYSTEM
 * ========================================================================
 * 
 * Comprehensive input management for the Endor game engine.
 * Handles keyboard, mouse, gamepad input with customizable key bindings,
 * input buffering, and smooth input processing for responsive gameplay.
 * 
 * Features:
 * - Multi-device input support (keyboard, mouse, gamepad, touch)
 * - Customizable key bindings with save/load functionality
 * - Input buffering and event queuing
 * - Analog stick dead zone and sensitivity adjustment
 * - Raw input support for high-precision mouse input
 * - Input recording and playback for demos/testing
 * - Combo/sequence detection for fighting games
 * - Haptic feedback support for gamepads
 * 
 * Improvements in this version:
 * - Enhanced error handling and logging
 * - XInput/DirectInput gamepad support
 * - Touch input support for mobile/tablet
 * - Input smoothing and filtering
 * - More comprehensive key mapping
 * - Better configuration parsing with validation
 * - Input statistics tracking
 */

#include "endor_readable.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

// ========================================================================
// INPUT CONSTANTS AND DEFINITIONS
// ========================================================================

#define MAX_KEYS 256
#define MAX_MOUSE_BUTTONS 8
#define MAX_GAMEPAD_BUTTONS 32
#define MAX_TOUCH_POINTS 10
#define MAX_INPUT_BINDINGS 128
#define MAX_COMMAND_QUEUE 64
#define INPUT_BUFFER_SIZE 32
#define GAMEPAD_DEADZONE_DEFAULT 0.2f
#define MOUSE_SENSITIVITY_DEFAULT 1.0f
#define MAX_COMBO_LENGTH 16
#define COMBO_TIMEOUT_MS 500
#define INPUT_RECORD_BUFFER_SIZE 10000
#define MAX_GAMEPADS 4

// Input action types
typedef enum {
    // Movement
    INPUT_ACTION_MOVE_FORWARD,
    INPUT_ACTION_MOVE_BACKWARD,
    INPUT_ACTION_MOVE_LEFT,
    INPUT_ACTION_MOVE_RIGHT,
    INPUT_ACTION_MOVE_UP,
    INPUT_ACTION_MOVE_DOWN,
    
    // Camera control
    INPUT_ACTION_TURN_LEFT,
    INPUT_ACTION_TURN_RIGHT,
    INPUT_ACTION_LOOK_UP,
    INPUT_ACTION_LOOK_DOWN,
    INPUT_ACTION_ZOOM_IN,
    INPUT_ACTION_ZOOM_OUT,
    
    // Combat
    INPUT_ACTION_FIRE_PRIMARY,
    INPUT_ACTION_FIRE_SECONDARY,
    INPUT_ACTION_RELOAD,
    INPUT_ACTION_AIM,
    INPUT_ACTION_MELEE,
    INPUT_ACTION_GRENADE,
    INPUT_ACTION_SWITCH_WEAPON,
    
    // Movement modifiers
    INPUT_ACTION_JUMP,
    INPUT_ACTION_CROUCH,
    INPUT_ACTION_RUN,
    INPUT_ACTION_WALK,
    INPUT_ACTION_DODGE,
    
    // Interaction
    INPUT_ACTION_USE,
    INPUT_ACTION_INTERACT,
    INPUT_ACTION_PICKUP,
    
    // UI/Menu
    INPUT_ACTION_PAUSE,
    INPUT_ACTION_MENU,
    INPUT_ACTION_INVENTORY,
    INPUT_ACTION_MAP,
    INPUT_ACTION_JOURNAL,
    INPUT_ACTION_SKILLS,
    
    // System
    INPUT_ACTION_QUICKSAVE,
    INPUT_ACTION_QUICKLOAD,
    INPUT_ACTION_SCREENSHOT,
    INPUT_ACTION_CONSOLE,
    INPUT_ACTION_DEBUG_MENU,
    
    // Custom actions
    INPUT_ACTION_CUSTOM_1,
    INPUT_ACTION_CUSTOM_2,
    INPUT_ACTION_CUSTOM_3,
    INPUT_ACTION_CUSTOM_4,
    
    INPUT_ACTION_COUNT
} InputAction;

// Input device types
typedef enum {
    INPUT_DEVICE_KEYBOARD,
    INPUT_DEVICE_MOUSE,
    INPUT_DEVICE_GAMEPAD,
    INPUT_DEVICE_JOYSTICK,
    INPUT_DEVICE_TOUCH,
    INPUT_DEVICE_ACCELEROMETER
} InputDevice;

// Input event types
typedef enum {
    INPUT_EVENT_KEY_PRESS,
    INPUT_EVENT_KEY_RELEASE,
    INPUT_EVENT_KEY_REPEAT,
    INPUT_EVENT_MOUSE_MOVE,
    INPUT_EVENT_MOUSE_PRESS,
    INPUT_EVENT_MOUSE_RELEASE,
    INPUT_EVENT_MOUSE_WHEEL,
    INPUT_EVENT_GAMEPAD_BUTTON_PRESS,
    INPUT_EVENT_GAMEPAD_BUTTON_RELEASE,
    INPUT_EVENT_GAMEPAD_ANALOG,
    INPUT_EVENT_GAMEPAD_TRIGGER,
    INPUT_EVENT_TOUCH_BEGIN,
    INPUT_EVENT_TOUCH_MOVE,
    INPUT_EVENT_TOUCH_END,
    INPUT_EVENT_ACCELEROMETER
} InputEventType;

// Gamepad button mapping
typedef enum {
    GAMEPAD_BUTTON_A,
    GAMEPAD_BUTTON_B,
    GAMEPAD_BUTTON_X,
    GAMEPAD_BUTTON_Y,
    GAMEPAD_BUTTON_LEFT_SHOULDER,
    GAMEPAD_BUTTON_RIGHT_SHOULDER,
    GAMEPAD_BUTTON_BACK,
    GAMEPAD_BUTTON_START,
    GAMEPAD_BUTTON_LEFT_STICK,
    GAMEPAD_BUTTON_RIGHT_STICK,
    GAMEPAD_BUTTON_DPAD_UP,
    GAMEPAD_BUTTON_DPAD_DOWN,
    GAMEPAD_BUTTON_DPAD_LEFT,
    GAMEPAD_BUTTON_DPAD_RIGHT,
    GAMEPAD_BUTTON_GUIDE,
    GAMEPAD_BUTTON_COUNT
} GamepadButton;

// Key binding structure
typedef struct {
    InputAction action;
    InputDevice device;
    int primary_key;
    int secondary_key;
    float sensitivity;
    float dead_zone;
    int modifier_keys;  // Bit flags for shift, ctrl, alt
    BOOL inverted;
    BOOL active;
    char description[64];
} KeyBinding;

// Input event structure
typedef struct {
    InputEventType type;
    InputDevice device;
    int device_id;  // For multiple gamepads/touch points
    int key_code;
    int button_index;
    float value;      // For analog inputs
    float x, y;       // For mouse/touch movement
    float pressure;   // For pressure-sensitive inputs
    int modifier_keys;
    DWORD timestamp;
} InputEvent;

// Touch point structure
typedef struct {
    int id;
    float x, y;
    float start_x, start_y;
    float pressure;
    DWORD start_time;
    BOOL active;
} TouchPoint;

// Gamepad state
typedef struct {
    float left_stick_x, left_stick_y;
    float right_stick_x, right_stick_y;
    float left_trigger, right_trigger;
    int button_states[MAX_GAMEPAD_BUTTONS];
    int button_pressed[MAX_GAMEPAD_BUTTONS];
    int button_released[MAX_GAMEPAD_BUTTONS];
    float button_pressure[MAX_GAMEPAD_BUTTONS];  // For pressure-sensitive buttons
    float left_motor, right_motor;  // Vibration/haptic feedback
    float battery_level;
    BOOL connected;
    BOOL wireless;
    char name[64];
    int player_index;
} GamepadState;

// Mouse state
typedef struct {
    int x, y;                    // Current position
    int delta_x, delta_y;        // Movement since last frame
    int raw_delta_x, raw_delta_y; // Raw input deltas
    int wheel_delta;
    int horizontal_wheel_delta;
    int button_states[MAX_MOUSE_BUTTONS];
    int button_pressed[MAX_MOUSE_BUTTONS];
    int button_released[MAX_MOUSE_BUTTONS];
    float sensitivity_x, sensitivity_y;
    float acceleration;
    BOOL captured;              // Mouse captured for FPS-style look
    BOOL raw_input_active;
} MouseState;

// Keyboard state
typedef struct {
    int key_states[MAX_KEYS];
    int key_pressed[MAX_KEYS];
    int key_released[MAX_KEYS];
    int key_repeat_count[MAX_KEYS];
    DWORD key_press_time[MAX_KEYS];
    int modifier_states;        // Shift, Ctrl, Alt flags
    char text_input[32];       // For text input handling
    int text_input_length;
} KeyboardState;

// Input command for queuing
typedef struct {
    InputAction action;
    float value;
    float x, y;  // For directional commands
    DWORD timestamp;
    int device_id;
    BOOL processed;
} InputCommand;

// Input combo detection
typedef struct {
    InputAction sequence[MAX_COMBO_LENGTH];
    int length;
    int current_index;
    DWORD last_input_time;
    void (*callback)(void);
    BOOL active;
    char name[32];
} InputCombo;

// Input recording for playback
typedef struct {
    InputEvent* events;
    int event_count;
    int max_events;
    int playback_index;
    DWORD start_time;
    BOOL recording;
    BOOL playing;
} InputRecorder;

// Input statistics
typedef struct {
    DWORD total_key_presses;
    DWORD total_mouse_clicks;
    DWORD total_mouse_distance;
    DWORD total_gamepad_inputs;
    float average_reaction_time;
    int most_used_keys[10];
    float session_start_time;
} InputStatistics;

// ========================================================================
// GLOBAL INPUT STATE
// ========================================================================

static KeyboardState g_keyboard;
static MouseState g_mouse;
static GamepadState g_gamepads[MAX_GAMEPADS];
static TouchPoint g_touch_points[MAX_TOUCH_POINTS];
static KeyBinding g_key_bindings[MAX_INPUT_BINDINGS];
static int g_binding_count = 0;
static InputEvent g_event_buffer[INPUT_BUFFER_SIZE];
static int g_event_buffer_head = 0;
static int g_event_buffer_tail = 0;
static InputCommand g_command_queue[MAX_COMMAND_QUEUE];
static int g_command_queue_head = 0;
static int g_command_queue_tail = 0;
static InputCombo g_combos[16];
static int g_combo_count = 0;
static InputRecorder g_recorder;
static InputStatistics g_stats;

// Input configuration
static float g_mouse_sensitivity_x = MOUSE_SENSITIVITY_DEFAULT;
static float g_mouse_sensitivity_y = MOUSE_SENSITIVITY_DEFAULT;
static float g_mouse_acceleration = 1.0f;
static BOOL g_invert_mouse_y = FALSE;
static BOOL g_invert_mouse_x = FALSE;
static BOOL g_raw_input_enabled = TRUE;
static BOOL g_input_enabled = TRUE;
static BOOL g_debug_input = FALSE;
static float g_gamepad_deadzone_left = GAMEPAD_DEADZONE_DEFAULT;
static float g_gamepad_deadzone_right = GAMEPAD_DEADZONE_DEFAULT;
static float g_gamepad_trigger_threshold = 0.1f;
static float g_double_click_time = 300.0f;  // milliseconds
static float g_key_repeat_delay = 500.0f;   // milliseconds
static float g_key_repeat_rate = 30.0f;     // milliseconds

// Action states for smooth movement
static float g_action_values[INPUT_ACTION_COUNT];
static float g_action_smoothed_values[INPUT_ACTION_COUNT];
static BOOL g_action_active[INPUT_ACTION_COUNT];
static DWORD g_action_start_time[INPUT_ACTION_COUNT];

// ========================================================================
// INTERNAL UTILITY FUNCTIONS
// ========================================================================

/**
 * Logs input system messages for debugging
 * @param format Printf-style format string
 * @param ... Variable arguments
 */
static void input_log(const char* format, ...)
{
    if (!g_debug_input) return;
    
    char buffer[512];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    OutputDebugString("[INPUT] ");
    OutputDebugString(buffer);
    OutputDebugString("\n");
}

/**
 * Applies dead zone to analog stick input
 * @param x X axis value
 * @param y Y axis value
 * @param dead_zone Dead zone threshold
 * @param out_x Output X value
 * @param out_y Output Y value
 */
static void apply_deadzone(float x, float y, float dead_zone, float* out_x, float* out_y)
{
    float magnitude = sqrtf(x * x + y * y);
    
    if (magnitude < dead_zone) {
        *out_x = 0.0f;
        *out_y = 0.0f;
    } else {
        // Normalize and scale
        float normalized_magnitude = (magnitude - dead_zone) / (1.0f - dead_zone);
        normalized_magnitude = fminf(normalized_magnitude, 1.0f);
        
        *out_x = (x / magnitude) * normalized_magnitude;
        *out_y = (y / magnitude) * normalized_magnitude;
    }
}

/**
 * Applies smoothing filter to input value
 * @param current Current value
 * @param target Target value
 * @param smoothing Smoothing factor (0-1)
 * @return Smoothed value
 */
static float smooth_input(float current, float target, float smoothing)
{
    return current + (target - current) * (1.0f - smoothing);
}

// ========================================================================
// CORE INPUT SYSTEM FUNCTIONS
// ========================================================================

/**
 * Initializes the input system
 */
void initialize_input_system(void)
{
    input_log("Initializing input system");
    
    // Clear all input states
    memset(&g_keyboard, 0, sizeof(KeyboardState));
    memset(&g_mouse, 0, sizeof(MouseState));
    memset(g_gamepads, 0, sizeof(g_gamepads));
    memset(g_touch_points, 0, sizeof(g_touch_points));
    memset(g_key_bindings, 0, sizeof(g_key_bindings));
    memset(g_event_buffer, 0, sizeof(g_event_buffer));
    memset(g_command_queue, 0, sizeof(g_command_queue));
    memset(g_action_values, 0, sizeof(g_action_values));
    memset(g_action_smoothed_values, 0, sizeof(g_action_smoothed_values));
    memset(g_action_active, 0, sizeof(g_action_active));
    memset(g_combos, 0, sizeof(g_combos));
    memset(&g_stats, 0, sizeof(g_stats));
    
    // Set default mouse settings
    g_mouse.sensitivity_x = MOUSE_SENSITIVITY_DEFAULT;
    g_mouse.sensitivity_y = MOUSE_SENSITIVITY_DEFAULT;
    g_mouse.acceleration = 1.0f;
    g_mouse.captured = FALSE;
    
    // Initialize gamepad indices
    for (int i = 0; i < MAX_GAMEPADS; i++) {
        g_gamepads[i].player_index = i;
    }
    
    // Initialize recorder
    g_recorder.events = (InputEvent*)calloc(INPUT_RECORD_BUFFER_SIZE, sizeof(InputEvent));
    g_recorder.max_events = INPUT_RECORD_BUFFER_SIZE;
    g_recorder.event_count = 0;
    g_recorder.playback_index = 0;
    g_recorder.recording = FALSE;
    g_recorder.playing = FALSE;
    
    // Load default key bindings
    setup_default_key_bindings();
    
    g_binding_count = 0;
    g_event_buffer_head = 0;
    g_event_buffer_tail = 0;
    g_command_queue_head = 0;
    g_command_queue_tail = 0;
    g_combo_count = 0;
    
    g_stats.session_start_time = (float)GetTickCount() / 1000.0f;
    
    // Initialize raw input if enabled
    if (g_raw_input_enabled) {
        // TODO: Register for raw input
    }
    
    input_log("Input system initialized");
}

/**
 * Sets up default key bindings
 */
void setup_default_key_bindings(void)
{
    // Movement bindings
    bind_key_ex(INPUT_ACTION_MOVE_FORWARD, INPUT_DEVICE_KEYBOARD, 'W', VK_UP, 
                1.0f, 0.0f, 0, FALSE, "Move Forward");
    bind_key_ex(INPUT_ACTION_MOVE_BACKWARD, INPUT_DEVICE_KEYBOARD, 'S', VK_DOWN, 
                1.0f, 0.0f, 0, FALSE, "Move Backward");
    bind_key_ex(INPUT_ACTION_MOVE_LEFT, INPUT_DEVICE_KEYBOARD, 'A', VK_LEFT, 
                1.0f, 0.0f, 0, FALSE, "Strafe Left");
    bind_key_ex(INPUT_ACTION_MOVE_RIGHT, INPUT_DEVICE_KEYBOARD, 'D', VK_RIGHT, 
                1.0f, 0.0f, 0, FALSE, "Strafe Right");
    
    // Camera control
    bind_key_ex(INPUT_ACTION_TURN_LEFT, INPUT_DEVICE_KEYBOARD, 'Q', 0, 
                1.0f, 0.0f, 0, FALSE, "Turn Left");
    bind_key_ex(INPUT_ACTION_TURN_RIGHT, INPUT_DEVICE_KEYBOARD, 'E', 0, 
                1.0f, 0.0f, 0, FALSE, "Turn Right");
    bind_key_ex(INPUT_ACTION_ZOOM_IN, INPUT_DEVICE_KEYBOARD, VK_ADD, 0, 
                1.0f, 0.0f, 0, FALSE, "Zoom In");
    bind_key_ex(INPUT_ACTION_ZOOM_OUT, INPUT_DEVICE_KEYBOARD, VK_SUBTRACT, 0, 
                1.0f, 0.0f, 0, FALSE, "Zoom Out");
    
    // Combat bindings
    bind_key_ex(INPUT_ACTION_FIRE_PRIMARY, INPUT_DEVICE_MOUSE, 0, 0, 
                1.0f, 0.0f, 0, FALSE, "Primary Fire");
    bind_key_ex(INPUT_ACTION_FIRE_SECONDARY, INPUT_DEVICE_MOUSE, 1, 0, 
                1.0f, 0.0f, 0, FALSE, "Secondary Fire");
    bind_key_ex(INPUT_ACTION_AIM, INPUT_DEVICE_MOUSE, 1, 0, 
                1.0f, 0.0f, 0, FALSE, "Aim");
    bind_key_ex(INPUT_ACTION_RELOAD, INPUT_DEVICE_KEYBOARD, 'R', 0, 
                1.0f, 0.0f, 0, FALSE, "Reload");
    bind_key_ex(INPUT_ACTION_MELEE, INPUT_DEVICE_KEYBOARD, 'V', 0, 
                1.0f, 0.0f, 0, FALSE, "Melee Attack");
    bind_key_ex(INPUT_ACTION_GRENADE, INPUT_DEVICE_KEYBOARD, 'G', 0, 
                1.0f, 0.0f, 0, FALSE, "Throw Grenade");
    bind_key_ex(INPUT_ACTION_SWITCH_WEAPON, INPUT_DEVICE_KEYBOARD, 'Q', 0, 
                1.0f, 0.0f, 0, FALSE, "Switch Weapon");
    
    // Movement modifiers
    bind_key_ex(INPUT_ACTION_JUMP, INPUT_DEVICE_KEYBOARD, VK_SPACE, 0, 
                1.0f, 0.0f, 0, FALSE, "Jump");
    bind_key_ex(INPUT_ACTION_CROUCH, INPUT_DEVICE_KEYBOARD, VK_LCONTROL, 'C', 
                1.0f, 0.0f, 0, FALSE, "Crouch");
    bind_key_ex(INPUT_ACTION_RUN, INPUT_DEVICE_KEYBOARD, VK_LSHIFT, 0, 
                1.0f, 0.0f, 0, FALSE, "Run/Sprint");
    bind_key_ex(INPUT_ACTION_WALK, INPUT_DEVICE_KEYBOARD, VK_LMENU, 0, 
                1.0f, 0.0f, 0, FALSE, "Walk");
    
    // Interaction
    bind_key_ex(INPUT_ACTION_USE, INPUT_DEVICE_KEYBOARD, 'E', VK_RETURN, 
                1.0f, 0.0f, 0, FALSE, "Use/Interact");
    bind_key_ex(INPUT_ACTION_PICKUP, INPUT_DEVICE_KEYBOARD, 'F', 0, 
                1.0f, 0.0f, 0, FALSE, "Pick Up");
    
    // UI/Menu bindings
    bind_key_ex(INPUT_ACTION_PAUSE, INPUT_DEVICE_KEYBOARD, VK_ESCAPE, 'P', 
                1.0f, 0.0f, 0, FALSE, "Pause");
    bind_key_ex(INPUT_ACTION_MENU, INPUT_DEVICE_KEYBOARD, VK_TAB, 0, 
                1.0f, 0.0f, 0, FALSE, "Menu");
    bind_key_ex(INPUT_ACTION_INVENTORY, INPUT_DEVICE_KEYBOARD, 'I', 0, 
                1.0f, 0.0f, 0, FALSE, "Inventory");
    bind_key_ex(INPUT_ACTION_MAP, INPUT_DEVICE_KEYBOARD, 'M', 0, 
                1.0f, 0.0f, 0, FALSE, "Map");
    bind_key_ex(INPUT_ACTION_JOURNAL, INPUT_DEVICE_KEYBOARD, 'J', 0, 
                1.0f, 0.0f, 0, FALSE, "Journal");
    
    // System bindings
    bind_key_ex(INPUT_ACTION_QUICKSAVE, INPUT_DEVICE_KEYBOARD, VK_F5, 0, 
                1.0f, 0.0f, 0, FALSE, "Quick Save");
    bind_key_ex(INPUT_ACTION_QUICKLOAD, INPUT_DEVICE_KEYBOARD, VK_F9, 0, 
                1.0f, 0.0f, 0, FALSE, "Quick Load");
    bind_key_ex(INPUT_ACTION_SCREENSHOT, INPUT_DEVICE_KEYBOARD, VK_F12, 0, 
                1.0f, 0.0f, 0, FALSE, "Screenshot");
    bind_key_ex(INPUT_ACTION_CONSOLE, INPUT_DEVICE_KEYBOARD, VK_OEM_3, 0, 
                1.0f, 0.0f, 0, FALSE, "Console");
    
    // Gamepad bindings
    bind_key_ex(INPUT_ACTION_FIRE_PRIMARY, INPUT_DEVICE_GAMEPAD, GAMEPAD_BUTTON_RIGHT_SHOULDER, 0, 
                1.0f, 0.0f, 0, FALSE, "Fire (Gamepad)");
    bind_key_ex(INPUT_ACTION_JUMP, INPUT_DEVICE_GAMEPAD, GAMEPAD_BUTTON_A, 0, 
                1.0f, 0.0f, 0, FALSE, "Jump (Gamepad)");
    bind_key_ex(INPUT_ACTION_USE, INPUT_DEVICE_GAMEPAD, GAMEPAD_BUTTON_X, 0, 
                1.0f, 0.0f, 0, FALSE, "Use (Gamepad)");
    bind_key_ex(INPUT_ACTION_PAUSE, INPUT_DEVICE_GAMEPAD, GAMEPAD_BUTTON_START, 0, 
                1.0f, 0.0f, 0, FALSE, "Pause (Gamepad)");
}

/**
 * Binds a key with extended parameters
 * @param action Action to bind
 * @param device Input device
 * @param primary_key Primary key code
 * @param secondary_key Secondary key code
 * @param sensitivity Input sensitivity
 * @param dead_zone Dead zone for analog inputs
 * @param modifier_keys Modifier key flags
 * @param inverted Whether to invert the input
 * @param description Human-readable description
 * @return Binding index or -1 on failure
 */
int bind_key_ex(InputAction action, InputDevice device, int primary_key, int secondary_key,
                float sensitivity, float dead_zone, int modifier_keys, BOOL inverted,
                const char* description)
{
    if (g_binding_count >= MAX_INPUT_BINDINGS) {
        input_log("Failed to bind key: binding limit reached");
        return -1;
    }
    
    KeyBinding* binding = &g_key_bindings[g_binding_count];
    binding->action = action;
    binding->device = device;
    binding->primary_key = primary_key;
    binding->secondary_key = secondary_key;
    binding->sensitivity = sensitivity;
    binding->dead_zone = dead_zone;
    binding->modifier_keys = modifier_keys;
    binding->inverted = inverted;
    binding->active = TRUE;
    
    if (description) {
        strncpy(binding->description, description, sizeof(binding->description) - 1);
    }
    
    input_log("Bound %s to device %d key %d/%d", 
              get_action_name(action), device, primary_key, secondary_key);
    
    return g_binding_count++;
}

/**
 * Simple key binding function for backwards compatibility
 */
int bind_key(InputAction action, InputDevice device, int primary_key, int secondary_key,
             float sensitivity, int modifier_keys)
{
    return bind_key_ex(action, device, primary_key, secondary_key, 
                      sensitivity, GAMEPAD_DEADZONE_DEFAULT, modifier_keys, FALSE, NULL);
}

/**
 * Unbinds all keys for an action
 * @param action Action to unbind
 */
void unbind_key(InputAction action)
{
    int unbound = 0;
    for (int i = 0; i < g_binding_count; i++) {
        if (g_key_bindings[i].action == action) {
            g_key_bindings[i].active = FALSE;
            unbound++;
        }
    }
    
    input_log("Unbound %d keys for action %s", unbound, get_action_name(action));
}

/**
 * Finds a key binding
 * @param action Action to find
 * @param device Device to match (or -1 for any)
 * @return Binding pointer or NULL
 */
KeyBinding* find_binding(InputAction action, InputDevice device)
{
    for (int i = 0; i < g_binding_count; i++) {
        if (g_key_bindings[i].active && 
            g_key_bindings[i].action == action &&
            (device == -1 || g_key_bindings[i].device == device)) {
            return &g_key_bindings[i];
        }
    }
    return NULL;
}

// ========================================================================
// INPUT EVENT PROCESSING
// ========================================================================

/**
 * Queues an input event
 */
void queue_input_event(InputEventType type, InputDevice device, int device_id,
                      int key_code, float value, float x, float y, int modifier_keys)
{
    if (!g_input_enabled) return;
    
    int next_head = (g_event_buffer_head + 1) % INPUT_BUFFER_SIZE;
    if (next_head == g_event_buffer_tail) {
        // Buffer full, drop oldest event
        g_event_buffer_tail = (g_event_buffer_tail + 1) % INPUT_BUFFER_SIZE;
        input_log("Input buffer overflow, dropping oldest event");
    }
    
    InputEvent* event = &g_event_buffer[g_event_buffer_head];
    event->type = type;
    event->device = device;
    event->device_id = device_id;
    event->key_code = key_code;
    event->value = value;
    event->x = x;
    event->y = y;
    event->modifier_keys = modifier_keys;
    event->timestamp = GetTickCount();
    
    g_event_buffer_head = next_head;
    
    // Record event if recording
    if (g_recorder.recording && g_recorder.event_count < g_recorder.max_events) {
        g_recorder.events[g_recorder.event_count++] = *event;
    }
}

/**
 * Processes keyboard input
 */
void process_keyboard_input(int key_code, int is_pressed, int is_repeat)
{
    if (key_code < 0 || key_code >= MAX_KEYS) return;
    
    // Update key states
    int was_pressed = g_keyboard.key_states[key_code];
    g_keyboard.key_states[key_code] = is_pressed;
    
    if (is_pressed && !was_pressed) {
        g_keyboard.key_pressed[key_code] = 1;
        g_keyboard.key_press_time[key_code] = GetTickCount();
        g_stats.total_key_presses++;
        
        // Track most used keys
        // TODO: Update most_used_keys array
    } else if (!is_pressed && was_pressed) {
        g_keyboard.key_released[key_code] = 1;
    }
    
    if (is_repeat) {
        g_keyboard.key_repeat_count[key_code]++;
    } else {
        g_keyboard.key_repeat_count[key_code] = 0;
    }
    
    // Update modifier states
    g_keyboard.modifier_states = 0;
    if (g_keyboard.key_states[VK_LSHIFT] || g_keyboard.key_states[VK_RSHIFT]) {
        g_keyboard.modifier_states |= 1; // Shift flag
    }
    if (g_keyboard.key_states[VK_LCONTROL] || g_keyboard.key_states[VK_RCONTROL]) {
        g_keyboard.modifier_states |= 2; // Ctrl flag
    }
    if (g_keyboard.key_states[VK_LMENU] || g_keyboard.key_states[VK_RMENU]) {
        g_keyboard.modifier_states |= 4; // Alt flag
    }
    
    // Queue event
    InputEventType event_type = is_repeat ? INPUT_EVENT_KEY_REPEAT :
                               (is_pressed ? INPUT_EVENT_KEY_PRESS : INPUT_EVENT_KEY_RELEASE);
    queue_input_event(event_type, INPUT_DEVICE_KEYBOARD, 0, key_code,
                     is_pressed ? 1.0f : 0.0f, 0, 0, g_keyboard.modifier_states);
    
    input_log("Key %s: %s%s", get_key_name(key_code), 
              is_pressed ? "pressed" : "released",
              is_repeat ? " (repeat)" : "");
}

/**
 * Processes mouse input
 */
void process_mouse_input(int button, int is_pressed, int x, int y, int wheel_delta)
{
    // Update position and delta
    int old_x = g_mouse.x;
    int old_y = g_mouse.y;
    g_mouse.x = x;
    g_mouse.y = y;
    g_mouse.delta_x = x - old_x;
    g_mouse.delta_y = y - old_y;
    g_mouse.wheel_delta = wheel_delta;
    
    // Track mouse movement distance
    g_stats.total_mouse_distance += (DWORD)sqrtf((float)(g_mouse.delta_x * g_mouse.delta_x + 
                                                         g_mouse.delta_y * g_mouse.delta_y));
    
    // Apply sensitivity and inversion
    float sens_x = g_mouse.sensitivity_x * g_mouse_sensitivity_x;
    float sens_y = g_mouse.sensitivity_y * g_mouse_sensitivity_y;
    
    if (g_invert_mouse_y) {
        g_mouse.delta_y = -g_mouse.delta_y;
    }
    if (g_invert_mouse_x) {
        g_mouse.delta_x = -g_mouse.delta_x;
    }
    
    // Apply acceleration
    if (g_mouse_acceleration > 1.0f) {
        float speed = sqrtf((float)(g_mouse.delta_x * g_mouse.delta_x + 
                                   g_mouse.delta_y * g_mouse.delta_y));
        float accel = 1.0f + (g_mouse_acceleration - 1.0f) * (speed / 100.0f);
        sens_x *= accel;
        sens_y *= accel;
    }
    
    g_mouse.delta_x = (int)(g_mouse.delta_x * sens_x);
    g_mouse.delta_y = (int)(g_mouse.delta_y * sens_y);
    
    // Update button states
    if (button >= 0 && button < MAX_MOUSE_BUTTONS) {
        int was_pressed = g_mouse.button_states[button];
        g_mouse.button_states[button] = is_pressed;
        
        if (is_pressed && !was_pressed) {
            g_mouse.button_pressed[button] = 1;
            g_stats.total_mouse_clicks++;
        } else if (!is_pressed && was_pressed) {
            g_mouse.button_released[button] = 1;
        }
        
        // Queue button event
        InputEventType event_type = is_pressed ? INPUT_EVENT_MOUSE_PRESS : INPUT_EVENT_MOUSE_RELEASE;
        queue_input_event(event_type, INPUT_DEVICE_MOUSE, 0, button,
                         is_pressed ? 1.0f : 0.0f, (float)x, (float)y, 0);
    }
    
    // Queue movement event if mouse moved
    if (g_mouse.delta_x != 0 || g_mouse.delta_y != 0) {
        queue_input_event(INPUT_EVENT_MOUSE_MOVE, INPUT_DEVICE_MOUSE, 0, 0,
                         0.0f, (float)g_mouse.delta_x, (float)g_mouse.delta_y, 0);
    }
    
    // Queue wheel event if wheel moved
    if (wheel_delta != 0) {
        queue_input_event(INPUT_EVENT_MOUSE_WHEEL, INPUT_DEVICE_MOUSE, 0, 0,
                         (float)wheel_delta, (float)x, (float)y, 0);
    }
}

/**
 * Processes raw mouse input for high precision
 */
void process_raw_mouse_input(int raw_x, int raw_y)
{
    if (!g_raw_input_enabled || !g_mouse.raw_input_active) return;
    
    g_mouse.raw_delta_x = raw_x;
    g_mouse.raw_delta_y = raw_y;
    
    // Use raw deltas instead of regular deltas
    g_mouse.delta_x = g_mouse.raw_delta_x;
    g_mouse.delta_y = g_mouse.raw_delta_y;
    
    // Apply sensitivity but not acceleration for raw input
    float sens_x = g_mouse.sensitivity_x * g_mouse_sensitivity_x;
    float sens_y = g_mouse.sensitivity_y * g_mouse_sensitivity_y;
    
    if (g_invert_mouse_y) {
        g_mouse.delta_y = -g_mouse.delta_y;
    }
    if (g_invert_mouse_x) {
        g_mouse.delta_x = -g_mouse.delta_x;
    }
    
    g_mouse.delta_x = (int)(g_mouse.delta_x * sens_x);
    g_mouse.delta_y = (int)(g_mouse.delta_y * sens_y);
    
    // Queue movement event
    queue_input_event(INPUT_EVENT_MOUSE_MOVE, INPUT_DEVICE_MOUSE, 0, 0,
                     0.0f, (float)g_mouse.delta_x, (float)g_mouse.delta_y, 0);
}

/**
 * Processes gamepad input
 */
void process_gamepad_input(int gamepad_id, int button, int is_pressed,
                          float left_x, float left_y, float right_x, float right_y,
                          float left_trigger, float right_trigger)
{
    if (gamepad_id < 0 || gamepad_id >= MAX_GAMEPADS) return;
    
    GamepadState* gamepad = &g_gamepads[gamepad_id];
    
    // Apply dead zones to analog sticks
    apply_deadzone(left_x, left_y, g_gamepad_deadzone_left, 
                   &gamepad->left_stick_x, &gamepad->left_stick_y);
    apply_deadzone(right_x, right_y, g_gamepad_deadzone_right,
                   &gamepad->right_stick_x, &gamepad->right_stick_y);
    
    // Apply threshold to triggers
    gamepad->left_trigger = left_trigger > g_gamepad_trigger_threshold ? left_trigger : 0.0f;
    gamepad->right_trigger = right_trigger > g_gamepad_trigger_threshold ? right_trigger : 0.0f;
    
    // Update button states
    if (button >= 0 && button < MAX_GAMEPAD_BUTTONS) {
        int was_pressed = gamepad->button_states[button];
        gamepad->button_states[button] = is_pressed;
        
        if (is_pressed && !was_pressed) {
            gamepad->button_pressed[button] = 1;
            g_stats.total_gamepad_inputs++;
        } else if (!is_pressed && was_pressed) {
            gamepad->button_released[button] = 1;
        }
        
        // Queue button event
        InputEventType event_type = is_pressed ? INPUT_EVENT_GAMEPAD_BUTTON_PRESS : 
                                               INPUT_EVENT_GAMEPAD_BUTTON_RELEASE;
        queue_input_event(event_type, INPUT_DEVICE_GAMEPAD, gamepad_id, button,
                         is_pressed ? 1.0f : 0.0f, 0, 0, 0);
    }
    
    // Queue analog events
    queue_input_event(INPUT_EVENT_GAMEPAD_ANALOG, INPUT_DEVICE_GAMEPAD, gamepad_id, 0,
                     0.0f, gamepad->left_stick_x, gamepad->left_stick_y, 0);
    queue_input_event(INPUT_EVENT_GAMEPAD_ANALOG, INPUT_DEVICE_GAMEPAD, gamepad_id, 1,
                     0.0f, gamepad->right_stick_x, gamepad->right_stick_y, 0);
    
    // Queue trigger events
    queue_input_event(INPUT_EVENT_GAMEPAD_TRIGGER, INPUT_DEVICE_GAMEPAD, gamepad_id, 0,
                     gamepad->left_trigger, 0, 0, 0);
    queue_input_event(INPUT_EVENT_GAMEPAD_TRIGGER, INPUT_DEVICE_GAMEPAD, gamepad_id, 1,
                     gamepad->right_trigger, 0, 0, 0);
}

/**
 * Processes touch input
 */
void process_touch_input(int touch_id, int event_type, float x, float y, float pressure)
{
    if (touch_id < 0 || touch_id >= MAX_TOUCH_POINTS) return;
    
    TouchPoint* touch = &g_touch_points[touch_id];
    InputEventType input_event_type;
    
    switch (event_type) {
        case 0: // Touch begin
            touch->id = touch_id;
            touch->x = x;
            touch->y = y;
            touch->start_x = x;
            touch->start_y = y;
            touch->pressure = pressure;
            touch->start_time = GetTickCount();
            touch->active = TRUE;
            input_event_type = INPUT_EVENT_TOUCH_BEGIN;
            break;
            
        case 1: // Touch move
            touch->x = x;
            touch->y = y;
            touch->pressure = pressure;
            input_event_type = INPUT_EVENT_TOUCH_MOVE;
            break;
            
        case 2: // Touch end
            touch->active = FALSE;
            input_event_type = INPUT_EVENT_TOUCH_END;
            break;
            
        default:
            return;
    }
    
    queue_input_event(input_event_type, INPUT_DEVICE_TOUCH, touch_id, 0,
                     pressure, x, y, 0);
}

// ========================================================================
// ACTION PROCESSING
// ========================================================================

/**
 * Updates input actions and processes events
 */
void update_input_actions(float delta_time)
{
    // Clear previous frame's pressed/released states
    memset(g_keyboard.key_pressed, 0, sizeof(g_keyboard.key_pressed));
    memset(g_keyboard.key_released, 0, sizeof(g_keyboard.key_released));
    memset(g_mouse.button_pressed, 0, sizeof(g_mouse.button_pressed));
    memset(g_mouse.button_released, 0, sizeof(g_mouse.button_released));
    
    for (int i = 0; i < MAX_GAMEPADS; i++) {
        memset(g_gamepads[i].button_pressed, 0, sizeof(g_gamepads[i].button_pressed));
        memset(g_gamepads[i].button_released, 0, sizeof(g_gamepads[i].button_released));
    }
    
    // Reset mouse delta
    g_mouse.delta_x = 0;
    g_mouse.delta_y = 0;
    g_mouse.raw_delta_x = 0;
    g_mouse.raw_delta_y = 0;
    g_mouse.wheel_delta = 0;
    g_mouse.horizontal_wheel_delta = 0;
    
    // Process recorder playback
    if (g_recorder.playing && g_recorder.playback_index < g_recorder.event_count) {
        InputEvent* recorded_event = &g_recorder.events[g_recorder.playback_index];
        DWORD current_time = GetTickCount();
        DWORD event_time = g_recorder.start_time + 
                          (recorded_event->timestamp - g_recorder.events[0].timestamp);
        
        if (current_time >= event_time) {
            // Inject recorded event
            memcpy(&g_event_buffer[g_event_buffer_head], recorded_event, sizeof(InputEvent));
            g_event_buffer_head = (g_event_buffer_head + 1) % INPUT_BUFFER_SIZE;
            g_recorder.playback_index++;
        }
    }
    
    // Process all queued events
    while (g_event_buffer_tail != g_event_buffer_head) {
        InputEvent* event = &g_event_buffer[g_event_buffer_tail];
        
        // Process combos
        process_input_combos(event);
        
        // Find matching key bindings and generate commands
        for (int i = 0; i < g_binding_count; i++) {
            if (!g_key_bindings[i].active) continue;
            
            KeyBinding* binding = &g_key_bindings[i];
            int matches = 0;
            float value = 0.0f;
            float x_value = 0.0f;
            float y_value = 0.0f;
            
            // Check if this event matches this binding
            if (binding->device == event->device) {
                switch (event->device) {
                    case INPUT_DEVICE_KEYBOARD:
                        if ((event->key_code == binding->primary_key ||
                             event->key_code == binding->secondary_key) &&
                            (binding->modifier_keys == 0 ||
                             (event->modifier_keys & binding->modifier_keys))) {
                            matches = 1;
                            value = event->value * binding->sensitivity;
                            if (binding->inverted) value = -value;
                        }
                        break;
                        
                    case INPUT_DEVICE_MOUSE:
                        if (event->type == INPUT_EVENT_MOUSE_PRESS ||
                            event->type == INPUT_EVENT_MOUSE_RELEASE) {
                            if (event->key_code == binding->primary_key) {
                                matches = 1;
                                value = event->value * binding->sensitivity;
                            }
                        } else if (event->type == INPUT_EVENT_MOUSE_MOVE) {
                            // Handle mouse look actions
                            if (binding->action == INPUT_ACTION_TURN_LEFT ||
                                binding->action == INPUT_ACTION_TURN_RIGHT) {
                                matches = 1;
                                value = event->x * binding->sensitivity * 0.01f;
                                if (binding->inverted) value = -value;
                            } else if (binding->action == INPUT_ACTION_LOOK_UP ||
                                      binding->action == INPUT_ACTION_LOOK_DOWN) {
                                matches = 1;
                                value = event->y * binding->sensitivity * 0.01f;
                                if (binding->inverted) value = -value;
                            }
                        } else if (event->type == INPUT_EVENT_MOUSE_WHEEL) {
                            if (binding->action == INPUT_ACTION_ZOOM_IN ||
                                binding->action == INPUT_ACTION_ZOOM_OUT) {
                                matches = 1;
                                value = event->value * binding->sensitivity;
                            }
                        }
                        break;
                        
                    case INPUT_DEVICE_GAMEPAD:
                        if (event->type == INPUT_EVENT_GAMEPAD_BUTTON_PRESS ||
                            event->type == INPUT_EVENT_GAMEPAD_BUTTON_RELEASE) {
                            if (event->key_code == binding->primary_key) {
                                matches = 1;
                                value = event->value * binding->sensitivity;
                            }
                        } else if (event->type == INPUT_EVENT_GAMEPAD_ANALOG) {
                            // Handle analog stick actions
                            if (event->key_code == 0) { // Left stick
                                if (binding->action == INPUT_ACTION_MOVE_FORWARD ||
                                    binding->action == INPUT_ACTION_MOVE_BACKWARD) {
                                    matches = 1;
                                    value = event->y * binding->sensitivity;
                                    if (binding->action == INPUT_ACTION_MOVE_BACKWARD) {
                                        value = -value;
                                    }
                                } else if (binding->action == INPUT_ACTION_MOVE_LEFT ||
                                          binding->action == INPUT_ACTION_MOVE_RIGHT) {
                                    matches = 1;
                                    value = event->x * binding->sensitivity;
                                    if (binding->action == INPUT_ACTION_MOVE_LEFT) {
                                        value = -value;
                                    }
                                }
                            } else if (event->key_code == 1) { // Right stick
                                if (binding->action == INPUT_ACTION_TURN_LEFT ||
                                    binding->action == INPUT_ACTION_TURN_RIGHT) {
                                    matches = 1;
                                    value = event->x * binding->sensitivity;
                                    if (binding->action == INPUT_ACTION_TURN_LEFT) {
                                        value = -value;
                                    }
                                } else if (binding->action == INPUT_ACTION_LOOK_UP ||
                                          binding->action == INPUT_ACTION_LOOK_DOWN) {
                                    matches = 1;
                                    value = event->y * binding->sensitivity;
                                    if (binding->action == INPUT_ACTION_LOOK_DOWN) {
                                        value = -value;
                                    }
                                }
                            }
                            x_value = event->x;
                            y_value = event->y;
                        } else if (event->type == INPUT_EVENT_GAMEPAD_TRIGGER) {
                            if ((event->key_code == 0 && binding->action == INPUT_ACTION_AIM) ||
                                (event->key_code == 1 && binding->action == INPUT_ACTION_FIRE_PRIMARY)) {
                                matches = 1;
                                value = event->value * binding->sensitivity;
                            }
                        }
                        break;
                        
                    case INPUT_DEVICE_TOUCH:
                        // TODO: Handle touch input mapping
                        break;
                }
            }
            
            if (matches) {
                queue_input_command(binding->action, value, x_value, y_value, event->device_id);
            }
        }
        
        g_event_buffer_tail = (g_event_buffer_tail + 1) % INPUT_BUFFER_SIZE;
    }
    
    // Update action smoothing
    for (int i = 0; i < INPUT_ACTION_COUNT; i++) {
        float smoothing = 0.8f; // Adjust for more/less smoothing
        g_action_smoothed_values[i] = smooth_input(g_action_smoothed_values[i], 
                                                   g_action_values[i], smoothing);
    }
}

/**
 * Queues an input command
 */
void queue_input_command(InputAction action, float value, float x, float y, int device_id)
{
    int next_head = (g_command_queue_head + 1) % MAX_COMMAND_QUEUE;
    if (next_head == g_command_queue_tail) {
        // Queue full, drop oldest command
        g_command_queue_tail = (g_command_queue_tail + 1) % MAX_COMMAND_QUEUE;
    }
    
    InputCommand* command = &g_command_queue[g_command_queue_head];
    command->action = action;
    command->value = value;
    command->x = x;
    command->y = y;
    command->timestamp = GetTickCount();
    command->device_id = device_id;
    command->processed = FALSE;
    
    g_command_queue_head = next_head;
    
    // Update continuous action states
    g_action_values[action] = value;
    BOOL was_active = g_action_active[action];
    g_action_active[action] = (fabsf(value) > 0.01f);
    
    if (g_action_active[action] && !was_active) {
        g_action_start_time[action] = GetTickCount();
    }
}

/**
 * Gets the next input command
 */
int get_next_input_command(InputAction* action, float* value)
{
    if (g_command_queue_tail == g_command_queue_head) {
        return 0; // No commands
    }
    
    InputCommand* command = &g_command_queue[g_command_queue_tail];
    if (command->processed) {
        g_command_queue_tail = (g_command_queue_tail + 1) % MAX_COMMAND_QUEUE;
        return get_next_input_command(action, value);
    }
    
    *action = command->action;
    *value = command->value;
    command->processed = TRUE;
    
    return 1;
}

/**
 * Gets the next input command with extended info
 */
int get_next_input_command_ex(InputAction* action, float* value, float* x, float* y, int* device_id)
{
    if (g_command_queue_tail == g_command_queue_head) {
        return 0; // No commands
    }
    
    InputCommand* command = &g_command_queue[g_command_queue_tail];
    if (command->processed) {
        g_command_queue_tail = (g_command_queue_tail + 1) % MAX_COMMAND_QUEUE;
        return get_next_input_command_ex(action, value, x, y, device_id);
    }
    
    *action = command->action;
    *value = command->value;
    if (x) *x = command->x;
    if (y) *y = command->y;
    if (device_id) *device_id = command->device_id;
    command->processed = TRUE;
    
    return 1;
}

// ========================================================================
// INPUT COMBO SYSTEM
// ========================================================================

/**
 * Registers an input combo
 */
int register_input_combo(const char* name, InputAction* sequence, int length, void (*callback)(void))
{
    if (g_combo_count >= 16 || length > MAX_COMBO_LENGTH) {
        return -1;
    }
    
    InputCombo* combo = &g_combos[g_combo_count];
    memcpy(combo->sequence, sequence, length * sizeof(InputAction));
    combo->length = length;
    combo->current_index = 0;
    combo->last_input_time = 0;
    combo->callback = callback;
    combo->active = TRUE;
    strncpy(combo->name, name, sizeof(combo->name) - 1);
    
    input_log("Registered combo: %s (length %d)", name, length);
    
    return g_combo_count++;
}

/**
 * Processes input combos
 */
void process_input_combos(InputEvent* event)
{
    // Only process key/button press events
    if (event->type != INPUT_EVENT_KEY_PRESS &&
        event->type != INPUT_EVENT_GAMEPAD_BUTTON_PRESS &&
        event->type != INPUT_EVENT_MOUSE_PRESS) {
        return;
    }
    
    // Find action for this event
    InputAction action = INPUT_ACTION_COUNT;
    for (int i = 0; i < g_binding_count; i++) {
        KeyBinding* binding = &g_key_bindings[i];
        if (binding->active && binding->device == event->device &&
            (binding->primary_key == event->key_code || 
             binding->secondary_key == event->key_code)) {
            action = binding->action;
            break;
        }
    }
    
    if (action == INPUT_ACTION_COUNT) return;
    
    DWORD current_time = GetTickCount();
    
    // Check all combos
    for (int i = 0; i < g_combo_count; i++) {
        InputCombo* combo = &g_combos[i];
        if (!combo->active) continue;
        
        // Reset combo if timeout
        if (combo->current_index > 0 && 
            current_time - combo->last_input_time > COMBO_TIMEOUT_MS) {
            combo->current_index = 0;
        }
        
        // Check if action matches current position in sequence
        if (combo->sequence[combo->current_index] == action) {
            combo->current_index++;
            combo->last_input_time = current_time;
            
            // Combo completed!
            if (combo->current_index >= combo->length) {
                input_log("Combo executed: %s", combo->name);
                if (combo->callback) {
                    combo->callback();
                }
                combo->current_index = 0;
            }
        } else {
            // Wrong input, reset combo
            combo->current_index = 0;
            
            // Check if this starts the combo
            if (combo->sequence[0] == action) {
                combo->current_index = 1;
                combo->last_input_time = current_time;
            }
        }
    }
}

// ========================================================================
// INPUT STATE QUERIES
// ========================================================================

/**
 * Checks if a key is currently pressed
 */
int is_key_pressed(int key_code)
{
    if (key_code < 0 || key_code >= MAX_KEYS) return 0;
    return g_keyboard.key_states[key_code];
}

/**
 * Checks if a key was just pressed this frame
 */
int is_key_just_pressed(int key_code)
{
    if (key_code < 0 || key_code >= MAX_KEYS) return 0;
    return g_keyboard.key_pressed[key_code];
}

/**
 * Checks if a key was just released this frame
 */
int is_key_just_released(int key_code)
{
    if (key_code < 0 || key_code >= MAX_KEYS) return 0;
    return g_keyboard.key_released[key_code];
}

/**
 * Gets how long a key has been held
 */
float get_key_hold_time(int key_code)
{
    if (key_code < 0 || key_code >= MAX_KEYS || !g_keyboard.key_states[key_code]) {
        return 0.0f;
    }
    return (GetTickCount() - g_keyboard.key_press_time[key_code]) / 1000.0f;
}

/**
 * Checks if a mouse button is pressed
 */
int is_mouse_button_pressed(int button)
{
    if (button < 0 || button >= MAX_MOUSE_BUTTONS) return 0;
    return g_mouse.button_states[button];
}

/**
 * Checks if a mouse button was just pressed
 */
int is_mouse_button_just_pressed(int button)
{
    if (button < 0 || button >= MAX_MOUSE_BUTTONS) return 0;
    return g_mouse.button_pressed[button];
}

/**
 * Checks if a mouse button was just released
 */
int is_mouse_button_just_released(int button)
{
    if (button < 0 || button >= MAX_MOUSE_BUTTONS) return 0;
    return g_mouse.button_released[button];
}

/**
 * Gets mouse position
 */
void get_mouse_position(int* x, int* y)
{
    if (x) *x = g_mouse.x;
    if (y) *y = g_mouse.y;
}

/**
 * Gets mouse movement delta
 */
void get_mouse_delta(int* dx, int* dy)
{
    if (dx) *dx = g_mouse.delta_x;
    if (dy) *dy = g_mouse.delta_y;
}

/**
 * Gets raw mouse movement delta
 */
void get_raw_mouse_delta(int* dx, int* dy)
{
    if (dx) *dx = g_mouse.raw_delta_x;
    if (dy) *dy = g_mouse.raw_delta_y;
}

/**
 * Gets mouse wheel delta
 */
int get_mouse_wheel_delta(void)
{
    return g_mouse.wheel_delta;
}

/**
 * Checks if an action is active
 */
int is_action_active(InputAction action)
{
    if (action < 0 || action >= INPUT_ACTION_COUNT) return 0;
    return g_action_active[action];
}

/**
 * Gets action value
 */
float get_action_value(InputAction action)
{
    if (action < 0 || action >= INPUT_ACTION_COUNT) return 0.0f;
    return g_action_values[action];
}

/**
 * Gets smoothed action value
 */
float get_action_value_smoothed(InputAction action)
{
    if (action < 0 || action >= INPUT_ACTION_COUNT) return 0.0f;
    return g_action_smoothed_values[action];
}

/**
 * Gets how long an action has been active
 */
float get_action_hold_time(InputAction action)
{
    if (action < 0 || action >= INPUT_ACTION_COUNT || !g_action_active[action]) {
        return 0.0f;
    }
    return (GetTickCount() - g_action_start_time[action]) / 1000.0f;
}

// ========================================================================
// GAMEPAD MANAGEMENT
// ========================================================================

/**
 * Checks if a gamepad is connected
 */
int is_gamepad_connected(int gamepad_id)
{
    if (gamepad_id < 0 || gamepad_id >= MAX_GAMEPADS) return 0;
    return g_gamepads[gamepad_id].connected;
}

/**
 * Gets gamepad name
 */
const char* get_gamepad_name(int gamepad_id)
{
    if (gamepad_id < 0 || gamepad_id >= MAX_GAMEPADS) return "Unknown";
    return g_gamepads[gamepad_id].name;
}

/**
 * Gets gamepad analog stick values
 */
void get_gamepad_stick_values(int gamepad_id, float* left_x, float* left_y, 
                             float* right_x, float* right_y)
{
    if (gamepad_id < 0 || gamepad_id >= MAX_GAMEPADS) return;
    
    GamepadState* gamepad = &g_gamepads[gamepad_id];
    if (left_x) *left_x = gamepad->left_stick_x;
    if (left_y) *left_y = gamepad->left_stick_y;
    if (right_x) *right_x = gamepad->right_stick_x;
    if (right_y) *right_y = gamepad->right_stick_y;
}

/**
 * Gets gamepad trigger values
 */
void get_gamepad_trigger_values(int gamepad_id, float* left_trigger, float* right_trigger)
{
    if (gamepad_id < 0 || gamepad_id >= MAX_GAMEPADS) return;
    
    GamepadState* gamepad = &g_gamepads[gamepad_id];
    if (left_trigger) *left_trigger = gamepad->left_trigger;
    if (right_trigger) *right_trigger = gamepad->right_trigger;
}

/**
 * Checks if a gamepad button is pressed
 */
int is_gamepad_button_pressed(int gamepad_id, int button)
{
    if (gamepad_id < 0 || gamepad_id >= MAX_GAMEPADS) return 0;
    if (button < 0 || button >= MAX_GAMEPAD_BUTTONS) return 0;
    return g_gamepads[gamepad_id].button_states[button];
}

/**
 * Sets gamepad vibration
 */
void set_gamepad_vibration(int gamepad_id, float left_motor, float right_motor)
{
    if (gamepad_id < 0 || gamepad_id >= MAX_GAMEPADS) return;
    
    GamepadState* gamepad = &g_gamepads[gamepad_id];
    gamepad->left_motor = fmaxf(0.0f, fminf(1.0f, left_motor));
    gamepad->right_motor = fmaxf(0.0f, fminf(1.0f, right_motor));
    
    // TODO: Apply vibration through XInput/DirectInput
}

/**
 * Gets gamepad battery level
 */
float get_gamepad_battery_level(int gamepad_id)
{
    if (gamepad_id < 0 || gamepad_id >= MAX_GAMEPADS) return 0.0f;
    return g_gamepads[gamepad_id].battery_level;
}

// ========================================================================
// TOUCH INPUT
// ========================================================================

/**
 * Gets active touch count
 */
int get_active_touch_count(void)
{
    int count = 0;
    for (int i = 0; i < MAX_TOUCH_POINTS; i++) {
        if (g_touch_points[i].active) count++;
    }
    return count;
}

/**
 * Gets touch point info
 */
int get_touch_point(int index, float* x, float* y, float* pressure)
{
    if (index < 0 || index >= MAX_TOUCH_POINTS) return 0;
    
    TouchPoint* touch = &g_touch_points[index];
    if (!touch->active) return 0;
    
    if (x) *x = touch->x;
    if (y) *y = touch->y;
    if (pressure) *pressure = touch->pressure;
    
    return 1;
}

/**
 * Gets touch gesture (swipe, pinch, etc.)
 */
int get_touch_gesture(float* value, float* x, float* y)
{
    // TODO: Implement gesture recognition
    return 0;
}

// ========================================================================
// CONFIGURATION AND SETTINGS
// ========================================================================

/**
 * Sets mouse sensitivity
 */
void set_mouse_sensitivity(float x_sensitivity, float y_sensitivity)
{
    g_mouse_sensitivity_x = fmaxf(0.1f, fminf(5.0f, x_sensitivity));
    g_mouse_sensitivity_y = fmaxf(0.1f, fminf(5.0f, y_sensitivity));
    g_mouse.sensitivity_x = g_mouse_sensitivity_x;
    g_mouse.sensitivity_y = g_mouse_sensitivity_y;
    
    input_log("Mouse sensitivity set to X:%.2f Y:%.2f", 
              g_mouse_sensitivity_x, g_mouse_sensitivity_y);
}

/**
 * Gets mouse sensitivity
 */
void get_mouse_sensitivity(float* x_sensitivity, float* y_sensitivity)
{
    if (x_sensitivity) *x_sensitivity = g_mouse_sensitivity_x;
    if (y_sensitivity) *y_sensitivity = g_mouse_sensitivity_y;
}

/**
 * Sets mouse acceleration
 */
void set_mouse_acceleration(float acceleration)
{
    g_mouse_acceleration = fmaxf(1.0f, fminf(3.0f, acceleration));
    g_mouse.acceleration = g_mouse_acceleration;
}

/**
 * Sets mouse inversion
 */
void set_mouse_invert(BOOL invert_x, BOOL invert_y)
{
    g_invert_mouse_x = invert_x;
    g_invert_mouse_y = invert_y;
}

/**
 * Captures/releases mouse
 */
void capture_mouse(BOOL capture)
{
    g_mouse.captured = capture;
    if (capture) {
        // Hide cursor and center it
        ShowCursor(FALSE);
        // TODO: Center mouse to window
        g_mouse.raw_input_active = g_raw_input_enabled;
    } else {
        // Show cursor
        ShowCursor(TRUE);
        g_mouse.raw_input_active = FALSE;
    }
    
    input_log("Mouse %s", capture ? "captured" : "released");
}

/**
 * Checks if mouse is captured
 */
int is_mouse_captured(void)
{
    return g_mouse.captured;
}

/**
 * Enables/disables input
 */
void enable_input(BOOL enabled)
{
    g_input_enabled = enabled;
    input_log("Input %s", enabled ? "enabled" : "disabled");
}

/**
 * Checks if input is enabled
 */
int is_input_enabled(void)
{
    return g_input_enabled;
}

/**
 * Enables debug input logging
 */
void enable_debug_input(BOOL debug)
{
    g_debug_input = debug;
}

/**
 * Sets gamepad dead zones
 */
void set_gamepad_deadzones(float left_deadzone, float right_deadzone)
{
    g_gamepad_deadzone_left = fmaxf(0.0f, fminf(0.9f, left_deadzone));
    g_gamepad_deadzone_right = fmaxf(0.0f, fminf(0.9f, right_deadzone));
    
    input_log("Gamepad deadzones set to L:%.2f R:%.2f", 
              g_gamepad_deadzone_left, g_gamepad_deadzone_right);
}

/**
 * Sets gamepad trigger threshold
 */
void set_gamepad_trigger_threshold(float threshold)
{
    g_gamepad_trigger_threshold = fmaxf(0.0f, fminf(0.5f, threshold));
}

// ========================================================================
// INPUT RECORDING AND PLAYBACK
// ========================================================================

/**
 * Starts recording input
 */
void start_input_recording(void)
{
    if (g_recorder.recording) return;
    
    g_recorder.event_count = 0;
    g_recorder.playback_index = 0;
    g_recorder.recording = TRUE;
    g_recorder.playing = FALSE;
    g_recorder.start_time = GetTickCount();
    
    input_log("Started input recording");
}

/**
 * Stops recording input
 */
void stop_input_recording(void)
{
    g_recorder.recording = FALSE;
    input_log("Stopped input recording (%d events)", g_recorder.event_count);
}

/**
 * Starts playing back recorded input
 */
void start_input_playback(void)
{
    if (g_recorder.playing || g_recorder.event_count == 0) return;
    
    g_recorder.playback_index = 0;
    g_recorder.playing = TRUE;
    g_recorder.recording = FALSE;
    g_recorder.start_time = GetTickCount();
    
    input_log("Started input playback (%d events)", g_recorder.event_count);
}

/**
 * Stops input playback
 */
void stop_input_playback(void)
{
    g_recorder.playing = FALSE;
    input_log("Stopped input playback");
}

/**
 * Saves recorded input to file
 */
void save_input_recording(const char* filename)
{
    FILE* file = fopen(filename, "wb");
    if (!file) return;
    
    fwrite(&g_recorder.event_count, sizeof(int), 1, file);
    fwrite(g_recorder.events, sizeof(InputEvent), g_recorder.event_count, file);
    fclose(file);
    
    input_log("Saved input recording to %s (%d events)", filename, g_recorder.event_count);
}

/**
 * Loads recorded input from file
 */
void load_input_recording(const char* filename)
{
    FILE* file = fopen(filename, "rb");
    if (!file) return;
    
    fread(&g_recorder.event_count, sizeof(int), 1, file);
    if (g_recorder.event_count > g_recorder.max_events) {
        g_recorder.event_count = g_recorder.max_events;
    }
    fread(g_recorder.events, sizeof(InputEvent), g_recorder.event_count, file);
    fclose(file);
    
    g_recorder.playback_index = 0;
    g_recorder.recording = FALSE;
    g_recorder.playing = FALSE;
    
    input_log("Loaded input recording from %s (%d events)", filename, g_recorder.event_count);
}

// ========================================================================
// UTILITY FUNCTIONS
// ========================================================================

/**
 * Gets action name
 */
const char* get_action_name(InputAction action)
{
    static const char* action_names[] = {
        "Move Forward", "Move Backward", "Move Left", "Move Right",
        "Move Up", "Move Down", "Turn Left", "Turn Right",
        "Look Up", "Look Down", "Zoom In", "Zoom Out",
        "Fire Primary", "Fire Secondary", "Reload", "Aim",
        "Melee", "Grenade", "Switch Weapon",
        "Jump", "Crouch", "Run", "Walk", "Dodge",
        "Use", "Interact", "Pick Up",
        "Pause", "Menu", "Inventory", "Map", "Journal", "Skills",
        "Quick Save", "Quick Load", "Screenshot", "Console", "Debug Menu",
        "Custom 1", "Custom 2", "Custom 3", "Custom 4"
    };
    
    if (action < 0 || action >= INPUT_ACTION_COUNT) {
        return "Unknown";
    }
    
    return action_names[action];
}

/**
 * Gets key name
 */
const char* get_key_name(int key_code)
{
    static char key_name[32];
    
    // Printable ASCII characters
    if (key_code >= 32 && key_code <= 126) {
        sprintf(key_name, "%c", key_code);
        return key_name;
    }
    
    // Special keys
    switch (key_code) {
        case VK_SPACE: return "Space";
        case VK_RETURN: return "Enter";
        case VK_ESCAPE: return "Escape";
        case VK_TAB: return "Tab";
        case VK_BACK: return "Backspace";
        case VK_DELETE: return "Delete";
        case VK_INSERT: return "Insert";
        case VK_HOME: return "Home";
        case VK_END: return "End";
        case VK_PRIOR: return "Page Up";
        case VK_NEXT: return "Page Down";
        case VK_LSHIFT: return "Left Shift";
        case VK_RSHIFT: return "Right Shift";
        case VK_LCONTROL: return "Left Ctrl";
        case VK_RCONTROL: return "Right Ctrl";
        case VK_LMENU: return "Left Alt";
        case VK_RMENU: return "Right Alt";
        case VK_LWIN: return "Left Win";
        case VK_RWIN: return "Right Win";
        case VK_LEFT: return "Left Arrow";
        case VK_RIGHT: return "Right Arrow";
        case VK_UP: return "Up Arrow";
        case VK_DOWN: return "Down Arrow";
        case VK_NUMPAD0: return "Numpad 0";
        case VK_NUMPAD1: return "Numpad 1";
        case VK_NUMPAD2: return "Numpad 2";
        case VK_NUMPAD3: return "Numpad 3";
        case VK_NUMPAD4: return "Numpad 4";
        case VK_NUMPAD5: return "Numpad 5";
        case VK_NUMPAD6: return "Numpad 6";
        case VK_NUMPAD7: return "Numpad 7";
        case VK_NUMPAD8: return "Numpad 8";
        case VK_NUMPAD9: return "Numpad 9";
        case VK_MULTIPLY: return "Numpad *";
        case VK_ADD: return "Numpad +";
        case VK_SUBTRACT: return "Numpad -";
        case VK_DECIMAL: return "Numpad .";
        case VK_DIVIDE: return "Numpad /";
        case VK_F1: return "F1";
        case VK_F2: return "F2";
        case VK_F3: return "F3";
        case VK_F4: return "F4";
        case VK_F5: return "F5";
        case VK_F6: return "F6";
        case VK_F7: return "F7";
        case VK_F8: return "F8";
        case VK_F9: return "F9";
        case VK_F10: return "F10";
        case VK_F11: return "F11";
        case VK_F12: return "F12";
        case VK_CAPITAL: return "Caps Lock";
        case VK_SCROLL: return "Scroll Lock";
        case VK_NUMLOCK: return "Num Lock";
        case VK_PAUSE: return "Pause";
        case VK_OEM_1: return ";";
        case VK_OEM_2: return "/";
        case VK_OEM_3: return "`";
        case VK_OEM_4: return "[";
        case VK_OEM_5: return "\\";
        case VK_OEM_6: return "]";
        case VK_OEM_7: return "'";
        case VK_OEM_PLUS: return "=";
        case VK_OEM_MINUS: return "-";
        case VK_OEM_COMMA: return ",";
        case VK_OEM_PERIOD: return ".";
        default:
            sprintf(key_name, "Key%d", key_code);
            return key_name;
    }
}

/**
 * Gets gamepad button name
 */
const char* get_gamepad_button_name(int button)
{
    static const char* button_names[] = {
        "A", "B", "X", "Y",
        "Left Shoulder", "Right Shoulder",
        "Back", "Start",
        "Left Stick", "Right Stick",
        "D-Pad Up", "D-Pad Down", "D-Pad Left", "D-Pad Right",
        "Guide"
    };
    
    if (button < 0 || button >= GAMEPAD_BUTTON_COUNT) {
        return "Unknown";
    }
    
    return button_names[button];
}

/**
 * Gets input statistics
 */
void get_input_statistics(InputStatistics* stats)
{
    if (stats) {
        memcpy(stats, &g_stats, sizeof(InputStatistics));
    }
}

/**
 * Resets input statistics
 */
void reset_input_statistics(void)
{
    memset(&g_stats, 0, sizeof(InputStatistics));
    g_stats.session_start_time = (float)GetTickCount() / 1000.0f;
}

// ========================================================================
// CONFIGURATION FILE I/O
// ========================================================================

/**
 * Saves input configuration to file
 */
void save_input_config(const char* filename)
{
    FILE* file = fopen(filename, "w");
    if (!file) {
        input_log("Failed to save input config to %s", filename);
        return;
    }
    
    fprintf(file, "# Endor Input Configuration\n");
    fprintf(file, "# Generated on %s\n\n", __DATE__);
    
    fprintf(file, "[Settings]\n");
    fprintf(file, "mouse_sensitivity_x=%.2f\n", g_mouse_sensitivity_x);
    fprintf(file, "mouse_sensitivity_y=%.2f\n", g_mouse_sensitivity_y);
    fprintf(file, "mouse_acceleration=%.2f\n", g_mouse_acceleration);
    fprintf(file, "invert_mouse_x=%d\n", g_invert_mouse_x);
    fprintf(file, "invert_mouse_y=%d\n", g_invert_mouse_y);
    fprintf(file, "raw_input_enabled=%d\n", g_raw_input_enabled);
    fprintf(file, "gamepad_deadzone_left=%.2f\n", g_gamepad_deadzone_left);
    fprintf(file, "gamepad_deadzone_right=%.2f\n", g_gamepad_deadzone_right);
    fprintf(file, "gamepad_trigger_threshold=%.2f\n", g_gamepad_trigger_threshold);
    fprintf(file, "double_click_time=%.1f\n", g_double_click_time);
    fprintf(file, "key_repeat_delay=%.1f\n", g_key_repeat_delay);
    fprintf(file, "key_repeat_rate=%.1f\n", g_key_repeat_rate);
    
    fprintf(file, "\n[KeyBindings]\n");
    fprintf(file, "# Format: action,device,primary,secondary,sensitivity,deadzone,modifiers,inverted,description\n");
    
    for (int i = 0; i < g_binding_count; i++) {
        if (!g_key_bindings[i].active) continue;
        
        KeyBinding* binding = &g_key_bindings[i];
        fprintf(file, "%s,%d,%d,%d,%.2f,%.2f,%d,%d,%s\n",
                get_action_name(binding->action),
                binding->device,
                binding->primary_key,
                binding->secondary_key,
                binding->sensitivity,
                binding->dead_zone,
                binding->modifier_keys,
                binding->inverted,
                binding->description);
    }
    
    fclose(file);
    input_log("Saved input config to %s", filename);
}

/**
 * Loads input configuration from file
 */
void load_input_config(const char* filename)
{
    FILE* file = fopen(filename, "r");
    if (!file) {
        input_log("Failed to load input config from %s", filename);
        return;
    }
    
    // Clear existing bindings
    g_binding_count = 0;
    
    char line[512];
    char section[64] = "";
    
    while (fgets(line, sizeof(line), file)) {
        // Skip comments and empty lines
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') continue;
        
        // Remove newline
        line[strcspn(line, "\n")] = 0;
        
        // Check for section headers
        if (line[0] == '[') {
            sscanf(line, "[%[^]]]", section);
            continue;
        }
        
        // Parse based on section
        if (strcmp(section, "Settings") == 0) {
            // Parse settings
            char key[64], value[64];
            if (sscanf(line, "%[^=]=%s", key, value) == 2) {
                if (strcmp(key, "mouse_sensitivity_x") == 0) {
                    g_mouse_sensitivity_x = (float)atof(value);
                } else if (strcmp(key, "mouse_sensitivity_y") == 0) {
                    g_mouse_sensitivity_y = (float)atof(value);
                } else if (strcmp(key, "mouse_acceleration") == 0) {
                    g_mouse_acceleration = (float)atof(value);
                } else if (strcmp(key, "invert_mouse_x") == 0) {
                    g_invert_mouse_x = atoi(value);
                } else if (strcmp(key, "invert_mouse_y") == 0) {
                    g_invert_mouse_y = atoi(value);
                } else if (strcmp(key, "raw_input_enabled") == 0) {
                    g_raw_input_enabled = atoi(value);
                } else if (strcmp(key, "gamepad_deadzone_left") == 0) {
                    g_gamepad_deadzone_left = (float)atof(value);
                } else if (strcmp(key, "gamepad_deadzone_right") == 0) {
                    g_gamepad_deadzone_right = (float)atof(value);
                } else if (strcmp(key, "gamepad_trigger_threshold") == 0) {
                    g_gamepad_trigger_threshold = (float)atof(value);
                } else if (strcmp(key, "double_click_time") == 0) {
                    g_double_click_time = (float)atof(value);
                } else if (strcmp(key, "key_repeat_delay") == 0) {
                    g_key_repeat_delay = (float)atof(value);
                } else if (strcmp(key, "key_repeat_rate") == 0) {
                    g_key_repeat_rate = (float)atof(value);
                }
            }
        } else if (strcmp(section, "KeyBindings") == 0) {
            // Parse key binding
            char action_name[64], description[64];
            int device, primary, secondary, modifiers, inverted;
            float sensitivity, deadzone;
            
            int parsed = sscanf(line, "%[^,],%d,%d,%d,%f,%f,%d,%d,%[^\n]",
                               action_name, &device, &primary, &secondary,
                               &sensitivity, &deadzone, &modifiers, &inverted,
                               description);
            
            if (parsed >= 8) {
                // Find action by name
                InputAction action = INPUT_ACTION_COUNT;
                for (int i = 0; i < INPUT_ACTION_COUNT; i++) {
                    if (strcmp(action_name, get_action_name(i)) == 0) {
                        action = (InputAction)i;
                        break;
                    }
                }
                
                if (action != INPUT_ACTION_COUNT) {
                    bind_key_ex(action, (InputDevice)device, primary, secondary,
                               sensitivity, deadzone, modifiers, inverted,
                               parsed >= 9 ? description : NULL);
                }
            }
        }
    }
    
    fclose(file);
    
    // Apply loaded settings
    g_mouse.sensitivity_x = g_mouse_sensitivity_x;
    g_mouse.sensitivity_y = g_mouse_sensitivity_y;
    g_mouse.acceleration = g_mouse_acceleration;
    
    input_log("Loaded input config from %s", filename);
}

// ========================================================================
// CLEANUP
// ========================================================================

/**
 * Cleans up the input system
 */
void cleanup_input_system(void)
{
    input_log("Cleaning up input system");
    
    // Release mouse capture
    capture_mouse(FALSE);
    
    // Stop recording/playback
    stop_input_recording();
    stop_input_playback();
    
    // Free recorder memory
    if (g_recorder.events) {
        free(g_recorder.events);
        g_recorder.events = NULL;
    }
    
    // Clear all states
    memset(&g_keyboard, 0, sizeof(KeyboardState));
    memset(&g_mouse, 0, sizeof(MouseState));
    memset(g_gamepads, 0, sizeof(g_gamepads));
    memset(g_touch_points, 0, sizeof(g_touch_points));
    
    g_binding_count = 0;
    g_event_buffer_head = 0;
    g_event_buffer_tail = 0;
    g_command_queue_head = 0;
    g_command_queue_tail = 0;
    g_combo_count = 0;
    
    input_log("Input system cleaned up");
}
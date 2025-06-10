/**
 * ========================================================================
 * ENDOR INPUT AND CONTROLS SYSTEM
 * ========================================================================
 * 
 * Comprehensive input management for the Endor game engine.
 * Handles keyboard, mouse, gamepad input with customizable key bindings,
 * input buffering, and smooth input processing for responsive gameplay.
 */

#include "endor_readable.h"
#include <math.h>

// ========================================================================
// INPUT CONSTANTS AND DEFINITIONS
// ========================================================================

#define MAX_KEYS 256
#define MAX_MOUSE_BUTTONS 8
#define MAX_GAMEPAD_BUTTONS 16
#define MAX_INPUT_BINDINGS 64
#define MAX_COMMAND_QUEUE 32
#define INPUT_BUFFER_SIZE 16
#define GAMEPAD_DEADZONE 0.2f
#define MOUSE_SENSITIVITY_DEFAULT 1.0f

// Input action types
typedef enum {
    INPUT_ACTION_MOVE_FORWARD,
    INPUT_ACTION_MOVE_BACKWARD,
    INPUT_ACTION_MOVE_LEFT,
    INPUT_ACTION_MOVE_RIGHT,
    INPUT_ACTION_MOVE_UP,
    INPUT_ACTION_MOVE_DOWN,
    INPUT_ACTION_TURN_LEFT,
    INPUT_ACTION_TURN_RIGHT,
    INPUT_ACTION_LOOK_UP,
    INPUT_ACTION_LOOK_DOWN,
    INPUT_ACTION_FIRE_PRIMARY,
    INPUT_ACTION_FIRE_SECONDARY,
    INPUT_ACTION_RELOAD,
    INPUT_ACTION_JUMP,
    INPUT_ACTION_CROUCH,
    INPUT_ACTION_RUN,
    INPUT_ACTION_USE,
    INPUT_ACTION_PAUSE,
    INPUT_ACTION_MENU,
    INPUT_ACTION_INVENTORY,
    INPUT_ACTION_MAP,
    INPUT_ACTION_QUICKSAVE,
    INPUT_ACTION_QUICKLOAD,
    INPUT_ACTION_SCREENSHOT,
    INPUT_ACTION_CONSOLE,
    INPUT_ACTION_COUNT
} InputAction;

// Input device types
typedef enum {
    INPUT_DEVICE_KEYBOARD,
    INPUT_DEVICE_MOUSE,
    INPUT_DEVICE_GAMEPAD,
    INPUT_DEVICE_JOYSTICK
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
    INPUT_EVENT_GAMEPAD_BUTTON,
    INPUT_EVENT_GAMEPAD_ANALOG
} InputEventType;

// Key binding structure
typedef struct {
    InputAction action;
    InputDevice device;
    int primary_key;
    int secondary_key;
    float sensitivity;
    int modifier_keys; // Bit flags for shift, ctrl, alt
    int active;
} KeyBinding;

// Input event structure
typedef struct {
    InputEventType type;
    InputDevice device;
    int key_code;
    int button_index;
    float value; // For analog inputs
    float x, y; // For mouse movement
    int modifier_keys;
    double timestamp;
} InputEvent;

// Gamepad state
typedef struct {
    float left_stick_x, left_stick_y;
    float right_stick_x, right_stick_y;
    float left_trigger, right_trigger;
    int button_states[MAX_GAMEPAD_BUTTONS];
    int button_pressed[MAX_GAMEPAD_BUTTONS];
    int button_released[MAX_GAMEPAD_BUTTONS];
    int connected;
    char name[64];
} GamepadState;

// Mouse state
typedef struct {
    int x, y; // Current position
    int delta_x, delta_y; // Movement since last frame
    int wheel_delta;
    int button_states[MAX_MOUSE_BUTTONS];
    int button_pressed[MAX_MOUSE_BUTTONS];
    int button_released[MAX_MOUSE_BUTTONS];
    float sensitivity;
    int captured; // Mouse captured for FPS-style look
} MouseState;

// Keyboard state
typedef struct {
    int key_states[MAX_KEYS];
    int key_pressed[MAX_KEYS];
    int key_released[MAX_KEYS];
    int modifier_states; // Shift, Ctrl, Alt flags
} KeyboardState;

// Input command for queuing
typedef struct {
    InputAction action;
    float value;
    double timestamp;
    int processed;
} InputCommand;

// ========================================================================
// GLOBAL INPUT STATE
// ========================================================================

static KeyboardState keyboard;
static MouseState mouse;
static GamepadState gamepad;
static KeyBinding key_bindings[MAX_INPUT_BINDINGS];
static int binding_count = 0;
static InputEvent event_buffer[INPUT_BUFFER_SIZE];
static int event_buffer_head = 0;
static int event_buffer_tail = 0;
static InputCommand command_queue[MAX_COMMAND_QUEUE];
static int command_queue_head = 0;
static int command_queue_tail = 0;

// Input configuration
static float mouse_sensitivity = MOUSE_SENSITIVITY_DEFAULT;
static int invert_mouse_y = 0;
static int raw_input_enabled = 1;
static int input_enabled = 1;
static int debug_input = 0;

// Action states for smooth movement
static float action_values[INPUT_ACTION_COUNT];
static int action_active[INPUT_ACTION_COUNT];

// ========================================================================
// CORE INPUT SYSTEM FUNCTIONS
// ========================================================================

void initialize_input_system() {
    // Clear all input states
    memset(&keyboard, 0, sizeof(KeyboardState));
    memset(&mouse, 0, sizeof(MouseState));
    memset(&gamepad, 0, sizeof(GamepadState));
    memset(key_bindings, 0, sizeof(key_bindings));
    memset(event_buffer, 0, sizeof(event_buffer));
    memset(command_queue, 0, sizeof(command_queue));
    memset(action_values, 0, sizeof(action_values));
    memset(action_active, 0, sizeof(action_active));
    
    // Set default mouse settings
    mouse.sensitivity = MOUSE_SENSITIVITY_DEFAULT;
    mouse.captured = 0;
    
    // Load default key bindings
    setup_default_key_bindings();
    
    binding_count = 0;
    event_buffer_head = 0;
    event_buffer_tail = 0;
    command_queue_head = 0;
    command_queue_tail = 0;
}

void setup_default_key_bindings() {
    // Movement bindings
    bind_key(INPUT_ACTION_MOVE_FORWARD, INPUT_DEVICE_KEYBOARD, 'W', 0, 1.0f, 0);
    bind_key(INPUT_ACTION_MOVE_BACKWARD, INPUT_DEVICE_KEYBOARD, 'S', 0, 1.0f, 0);
    bind_key(INPUT_ACTION_MOVE_LEFT, INPUT_DEVICE_KEYBOARD, 'A', 0, 1.0f, 0);
    bind_key(INPUT_ACTION_MOVE_RIGHT, INPUT_DEVICE_KEYBOARD, 'D', 0, 1.0f, 0);
    bind_key(INPUT_ACTION_MOVE_UP, INPUT_DEVICE_KEYBOARD, VK_SPACE, 0, 1.0f, 0);
    bind_key(INPUT_ACTION_MOVE_DOWN, INPUT_DEVICE_KEYBOARD, 'C', 0, 1.0f, 0);
    
    // Look/turn bindings
    bind_key(INPUT_ACTION_TURN_LEFT, INPUT_DEVICE_KEYBOARD, VK_LEFT, 0, 1.0f, 0);
    bind_key(INPUT_ACTION_TURN_RIGHT, INPUT_DEVICE_KEYBOARD, VK_RIGHT, 0, 1.0f, 0);
    bind_key(INPUT_ACTION_LOOK_UP, INPUT_DEVICE_KEYBOARD, VK_UP, 0, 1.0f, 0);
    bind_key(INPUT_ACTION_LOOK_DOWN, INPUT_DEVICE_KEYBOARD, VK_DOWN, 0, 1.0f, 0);
    
    // Combat bindings
    bind_key(INPUT_ACTION_FIRE_PRIMARY, INPUT_DEVICE_MOUSE, 0, 0, 1.0f, 0); // Left mouse button
    bind_key(INPUT_ACTION_FIRE_SECONDARY, INPUT_DEVICE_MOUSE, 1, 0, 1.0f, 0); // Right mouse button
    bind_key(INPUT_ACTION_RELOAD, INPUT_DEVICE_KEYBOARD, 'R', 0, 1.0f, 0);
    
    // Game controls
    bind_key(INPUT_ACTION_JUMP, INPUT_DEVICE_KEYBOARD, VK_SPACE, 0, 1.0f, 0);
    bind_key(INPUT_ACTION_CROUCH, INPUT_DEVICE_KEYBOARD, VK_LCONTROL, 0, 1.0f, 0);
    bind_key(INPUT_ACTION_RUN, INPUT_DEVICE_KEYBOARD, VK_LSHIFT, 0, 1.0f, 0);
    bind_key(INPUT_ACTION_USE, INPUT_DEVICE_KEYBOARD, 'E', 0, 1.0f, 0);
    
    // Menu bindings
    bind_key(INPUT_ACTION_PAUSE, INPUT_DEVICE_KEYBOARD, VK_ESCAPE, 0, 1.0f, 0);
    bind_key(INPUT_ACTION_MENU, INPUT_DEVICE_KEYBOARD, VK_TAB, 0, 1.0f, 0);
    bind_key(INPUT_ACTION_INVENTORY, INPUT_DEVICE_KEYBOARD, 'I', 0, 1.0f, 0);
    bind_key(INPUT_ACTION_MAP, INPUT_DEVICE_KEYBOARD, 'M', 0, 1.0f, 0);
    
    // System bindings
    bind_key(INPUT_ACTION_QUICKSAVE, INPUT_DEVICE_KEYBOARD, VK_F5, 0, 1.0f, 0);
    bind_key(INPUT_ACTION_QUICKLOAD, INPUT_DEVICE_KEYBOARD, VK_F9, 0, 1.0f, 0);
    bind_key(INPUT_ACTION_SCREENSHOT, INPUT_DEVICE_KEYBOARD, VK_F12, 0, 1.0f, 0);
    bind_key(INPUT_ACTION_CONSOLE, INPUT_DEVICE_KEYBOARD, VK_OEM_3, 0, 1.0f, 0); // Tilde key
}

int bind_key(InputAction action, InputDevice device, int primary_key, int secondary_key, 
             float sensitivity, int modifier_keys) {
    if (binding_count >= MAX_INPUT_BINDINGS) {
        return -1;
    }
    
    KeyBinding* binding = &key_bindings[binding_count];
    binding->action = action;
    binding->device = device;
    binding->primary_key = primary_key;
    binding->secondary_key = secondary_key;
    binding->sensitivity = sensitivity;
    binding->modifier_keys = modifier_keys;
    binding->active = 1;
    
    return binding_count++;
}

void unbind_key(InputAction action) {
    for (int i = 0; i < binding_count; i++) {
        if (key_bindings[i].action == action) {
            key_bindings[i].active = 0;
        }
    }
}

// ========================================================================
// INPUT EVENT PROCESSING
// ========================================================================

void queue_input_event(InputEventType type, InputDevice device, int key_code, 
                      float value, float x, float y, int modifier_keys) {
    int next_head = (event_buffer_head + 1) % INPUT_BUFFER_SIZE;
    if (next_head == event_buffer_tail) {
        // Buffer full, drop oldest event
        event_buffer_tail = (event_buffer_tail + 1) % INPUT_BUFFER_SIZE;
    }
    
    InputEvent* event = &event_buffer[event_buffer_head];
    event->type = type;
    event->device = device;
    event->key_code = key_code;
    event->value = value;
    event->x = x;
    event->y = y;
    event->modifier_keys = modifier_keys;
    event->timestamp = GetTickCount(); // Windows-specific, should be cross-platform
    
    event_buffer_head = next_head;
}

void process_keyboard_input(int key_code, int is_pressed, int is_repeat) {
    if (key_code < 0 || key_code >= MAX_KEYS) return;
    
    // Update key states
    int was_pressed = keyboard.key_states[key_code];
    keyboard.key_states[key_code] = is_pressed;
    
    if (is_pressed && !was_pressed) {
        keyboard.key_pressed[key_code] = 1;
    } else if (!is_pressed && was_pressed) {
        keyboard.key_released[key_code] = 1;
    }
    
    // Update modifier states
    keyboard.modifier_states = 0;
    if (keyboard.key_states[VK_LSHIFT] || keyboard.key_states[VK_RSHIFT]) {
        keyboard.modifier_states |= 1; // Shift flag
    }
    if (keyboard.key_states[VK_LCONTROL] || keyboard.key_states[VK_RCONTROL]) {
        keyboard.modifier_states |= 2; // Ctrl flag
    }
    if (keyboard.key_states[VK_LMENU] || keyboard.key_states[VK_RMENU]) {
        keyboard.modifier_states |= 4; // Alt flag
    }
    
    // Queue event
    InputEventType event_type = is_repeat ? INPUT_EVENT_KEY_REPEAT : 
                               (is_pressed ? INPUT_EVENT_KEY_PRESS : INPUT_EVENT_KEY_RELEASE);
    queue_input_event(event_type, INPUT_DEVICE_KEYBOARD, key_code, 
                     is_pressed ? 1.0f : 0.0f, 0, 0, keyboard.modifier_states);
}

void process_mouse_input(int button, int is_pressed, int x, int y, int wheel_delta) {
    // Update position and delta
    int old_x = mouse.x;
    int old_y = mouse.y;
    mouse.x = x;
    mouse.y = y;
    mouse.delta_x = x - old_x;
    mouse.delta_y = y - old_y;
    mouse.wheel_delta = wheel_delta;
    
    // Invert Y if enabled
    if (invert_mouse_y) {
        mouse.delta_y = -mouse.delta_y;
    }
    
    // Apply sensitivity
    mouse.delta_x = (int)(mouse.delta_x * mouse.sensitivity);
    mouse.delta_y = (int)(mouse.delta_y * mouse.sensitivity);
    
    // Update button states
    if (button >= 0 && button < MAX_MOUSE_BUTTONS) {
        int was_pressed = mouse.button_states[button];
        mouse.button_states[button] = is_pressed;
        
        if (is_pressed && !was_pressed) {
            mouse.button_pressed[button] = 1;
        } else if (!is_pressed && was_pressed) {
            mouse.button_released[button] = 1;
        }
        
        // Queue button event
        InputEventType event_type = is_pressed ? INPUT_EVENT_MOUSE_PRESS : INPUT_EVENT_MOUSE_RELEASE;
        queue_input_event(event_type, INPUT_DEVICE_MOUSE, button, 
                         is_pressed ? 1.0f : 0.0f, (float)x, (float)y, 0);
    }
    
    // Queue movement event if mouse moved
    if (mouse.delta_x != 0 || mouse.delta_y != 0) {
        queue_input_event(INPUT_EVENT_MOUSE_MOVE, INPUT_DEVICE_MOUSE, 0, 
                         0.0f, (float)mouse.delta_x, (float)mouse.delta_y, 0);
    }
    
    // Queue wheel event if wheel moved
    if (wheel_delta != 0) {
        queue_input_event(INPUT_EVENT_MOUSE_WHEEL, INPUT_DEVICE_MOUSE, 0, 
                         (float)wheel_delta, (float)x, (float)y, 0);
    }
}

void process_gamepad_input(int button, int is_pressed, float left_x, float left_y, 
                          float right_x, float right_y, float left_trigger, float right_trigger) {
    // Update analog stick values with deadzone
    gamepad.left_stick_x = fabs(left_x) > GAMEPAD_DEADZONE ? left_x : 0.0f;
    gamepad.left_stick_y = fabs(left_y) > GAMEPAD_DEADZONE ? left_y : 0.0f;
    gamepad.right_stick_x = fabs(right_x) > GAMEPAD_DEADZONE ? right_x : 0.0f;
    gamepad.right_stick_y = fabs(right_y) > GAMEPAD_DEADZONE ? right_y : 0.0f;
    gamepad.left_trigger = left_trigger;
    gamepad.right_trigger = right_trigger;
    
    // Update button states
    if (button >= 0 && button < MAX_GAMEPAD_BUTTONS) {
        int was_pressed = gamepad.button_states[button];
        gamepad.button_states[button] = is_pressed;
        
        if (is_pressed && !was_pressed) {
            gamepad.button_pressed[button] = 1;
        } else if (!is_pressed && was_pressed) {
            gamepad.button_released[button] = 1;
        }
        
        // Queue button event
        queue_input_event(INPUT_EVENT_GAMEPAD_BUTTON, INPUT_DEVICE_GAMEPAD, button, 
                         is_pressed ? 1.0f : 0.0f, 0, 0, 0);
    }
    
    // Queue analog events
    queue_input_event(INPUT_EVENT_GAMEPAD_ANALOG, INPUT_DEVICE_GAMEPAD, 0, 
                     left_x, left_y, 0, 0);
    queue_input_event(INPUT_EVENT_GAMEPAD_ANALOG, INPUT_DEVICE_GAMEPAD, 1, 
                     right_x, right_y, 0, 0);
}

// ========================================================================
// ACTION PROCESSING
// ========================================================================

void update_input_actions() {
    // Clear previous frame's pressed/released states
    memset(keyboard.key_pressed, 0, sizeof(keyboard.key_pressed));
    memset(keyboard.key_released, 0, sizeof(keyboard.key_released));
    memset(mouse.button_pressed, 0, sizeof(mouse.button_pressed));
    memset(mouse.button_released, 0, sizeof(mouse.button_released));
    memset(gamepad.button_pressed, 0, sizeof(gamepad.button_pressed));
    memset(gamepad.button_released, 0, sizeof(gamepad.button_released));
    
    // Reset mouse delta
    mouse.delta_x = 0;
    mouse.delta_y = 0;
    mouse.wheel_delta = 0;
    
    // Process all queued events
    while (event_buffer_tail != event_buffer_head) {
        InputEvent* event = &event_buffer[event_buffer_tail];
        
        // Find matching key bindings and generate commands
        for (int i = 0; i < binding_count; i++) {
            if (!key_bindings[i].active) continue;
            
            KeyBinding* binding = &key_bindings[i];
            int matches = 0;
            float value = 0.0f;
            
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
                                value = event->x * binding->sensitivity;
                            } else if (binding->action == INPUT_ACTION_LOOK_UP || 
                                      binding->action == INPUT_ACTION_LOOK_DOWN) {
                                matches = 1;
                                value = event->y * binding->sensitivity;
                            }
                        }
                        break;
                        
                    case INPUT_DEVICE_GAMEPAD:
                        if (event->type == INPUT_EVENT_GAMEPAD_BUTTON) {
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
                                } else if (binding->action == INPUT_ACTION_MOVE_LEFT ||
                                          binding->action == INPUT_ACTION_MOVE_RIGHT) {
                                    matches = 1;
                                    value = event->x * binding->sensitivity;
                                }
                            } else if (event->key_code == 1) { // Right stick
                                if (binding->action == INPUT_ACTION_TURN_LEFT ||
                                    binding->action == INPUT_ACTION_TURN_RIGHT) {
                                    matches = 1;
                                    value = event->x * binding->sensitivity;
                                } else if (binding->action == INPUT_ACTION_LOOK_UP ||
                                          binding->action == INPUT_ACTION_LOOK_DOWN) {
                                    matches = 1;
                                    value = event->y * binding->sensitivity;
                                }
                            }
                        }
                        break;
                }
            }
            
            if (matches) {
                queue_input_command(binding->action, value);
            }
        }
        
        event_buffer_tail = (event_buffer_tail + 1) % INPUT_BUFFER_SIZE;
    }
}

void queue_input_command(InputAction action, float value) {
    int next_head = (command_queue_head + 1) % MAX_COMMAND_QUEUE;
    if (next_head == command_queue_tail) {
        // Queue full, drop oldest command
        command_queue_tail = (command_queue_tail + 1) % MAX_COMMAND_QUEUE;
    }
    
    InputCommand* command = &command_queue[command_queue_head];
    command->action = action;
    command->value = value;
    command->timestamp = GetTickCount();
    command->processed = 0;
    
    command_queue_head = next_head;
    
    // Update continuous action states
    action_values[action] = value;
    action_active[action] = (fabs(value) > 0.01f);
}

int get_next_input_command(InputAction* action, float* value) {
    if (command_queue_tail == command_queue_head) {
        return 0; // No commands
    }
    
    InputCommand* command = &command_queue[command_queue_tail];
    if (command->processed) {
        command_queue_tail = (command_queue_tail + 1) % MAX_COMMAND_QUEUE;
        return get_next_input_command(action, value);
    }
    
    *action = command->action;
    *value = command->value;
    command->processed = 1;
    
    return 1;
}

// ========================================================================
// INPUT STATE QUERIES
// ========================================================================

int is_key_pressed(int key_code) {
    if (key_code < 0 || key_code >= MAX_KEYS) return 0;
    return keyboard.key_states[key_code];
}

int is_key_just_pressed(int key_code) {
    if (key_code < 0 || key_code >= MAX_KEYS) return 0;
    return keyboard.key_pressed[key_code];
}

int is_key_just_released(int key_code) {
    if (key_code < 0 || key_code >= MAX_KEYS) return 0;
    return keyboard.key_released[key_code];
}

int is_mouse_button_pressed(int button) {
    if (button < 0 || button >= MAX_MOUSE_BUTTONS) return 0;
    return mouse.button_states[button];
}

int is_mouse_button_just_pressed(int button) {
    if (button < 0 || button >= MAX_MOUSE_BUTTONS) return 0;
    return mouse.button_pressed[button];
}

int is_mouse_button_just_released(int button) {
    if (button < 0 || button >= MAX_MOUSE_BUTTONS) return 0;
    return mouse.button_released[button];
}

void get_mouse_position(int* x, int* y) {
    if (x) *x = mouse.x;
    if (y) *y = mouse.y;
}

void get_mouse_delta(int* dx, int* dy) {
    if (dx) *dx = mouse.delta_x;
    if (dy) *dy = mouse.delta_y;
}

int get_mouse_wheel_delta() {
    return mouse.wheel_delta;
}

int is_action_active(InputAction action) {
    if (action < 0 || action >= INPUT_ACTION_COUNT) return 0;
    return action_active[action];
}

float get_action_value(InputAction action) {
    if (action < 0 || action >= INPUT_ACTION_COUNT) return 0.0f;
    return action_values[action];
}

// ========================================================================
// CONFIGURATION AND SETTINGS
// ========================================================================

void set_mouse_sensitivity(float sensitivity) {
    mouse_sensitivity = fmax(0.1f, fmin(5.0f, sensitivity));
    mouse.sensitivity = mouse_sensitivity;
}

float get_mouse_sensitivity() {
    return mouse_sensitivity;
}

void set_mouse_invert_y(int invert) {
    invert_mouse_y = invert;
}

int get_mouse_invert_y() {
    return invert_mouse_y;
}

void capture_mouse(int capture) {
    mouse.captured = capture;
    if (capture) {
        // Hide cursor and center it
        ShowCursor(FALSE);
        // Center mouse to window (would need window handle)
    } else {
        // Show cursor
        ShowCursor(TRUE);
    }
}

int is_mouse_captured() {
    return mouse.captured;
}

void enable_input(int enabled) {
    input_enabled = enabled;
}

int is_input_enabled() {
    return input_enabled;
}

void enable_debug_input(int debug) {
    debug_input = debug;
}

// ========================================================================
// GAMEPAD MANAGEMENT
// ========================================================================

int is_gamepad_connected() {
    return gamepad.connected;
}

void get_gamepad_stick_values(float* left_x, float* left_y, float* right_x, float* right_y) {
    if (left_x) *left_x = gamepad.left_stick_x;
    if (left_y) *left_y = gamepad.left_stick_y;
    if (right_x) *right_x = gamepad.right_stick_x;
    if (right_y) *right_y = gamepad.right_stick_y;
}

void get_gamepad_trigger_values(float* left_trigger, float* right_trigger) {
    if (left_trigger) *left_trigger = gamepad.left_trigger;
    if (right_trigger) *right_trigger = gamepad.right_trigger;
}

int is_gamepad_button_pressed(int button) {
    if (button < 0 || button >= MAX_GAMEPAD_BUTTONS) return 0;
    return gamepad.button_states[button];
}

// ========================================================================
// UTILITY FUNCTIONS
// ========================================================================

const char* get_action_name(InputAction action) {
    static const char* action_names[] = {
        "Move Forward", "Move Backward", "Move Left", "Move Right",
        "Move Up", "Move Down", "Turn Left", "Turn Right",
        "Look Up", "Look Down", "Fire Primary", "Fire Secondary",
        "Reload", "Jump", "Crouch", "Run", "Use",
        "Pause", "Menu", "Inventory", "Map",
        "Quick Save", "Quick Load", "Screenshot", "Console"
    };
    
    if (action < 0 || action >= INPUT_ACTION_COUNT) {
        return "Unknown";
    }
    
    return action_names[action];
}

const char* get_key_name(int key_code) {
    // Simple key name mapping (should be more comprehensive)
    static char key_name[32];
    
    if (key_code >= 'A' && key_code <= 'Z') {
        sprintf(key_name, "%c", key_code);
        return key_name;
    }
    
    switch (key_code) {
        case VK_SPACE: return "Space";
        case VK_RETURN: return "Enter";
        case VK_ESCAPE: return "Escape";
        case VK_TAB: return "Tab";
        case VK_LSHIFT: return "Left Shift";
        case VK_RSHIFT: return "Right Shift";
        case VK_LCONTROL: return "Left Ctrl";
        case VK_RCONTROL: return "Right Ctrl";
        case VK_LMENU: return "Left Alt";
        case VK_RMENU: return "Right Alt";
        case VK_LEFT: return "Left Arrow";
        case VK_RIGHT: return "Right Arrow";
        case VK_UP: return "Up Arrow";
        case VK_DOWN: return "Down Arrow";
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
        default:
            sprintf(key_name, "Key%d", key_code);
            return key_name;
    }
}

void save_input_config(const char* filename) {
    FILE* file = fopen(filename, "w");
    if (!file) return;
    
    fprintf(file, "# Endor Input Configuration\n");
    fprintf(file, "mouse_sensitivity=%.2f\n", mouse_sensitivity);
    fprintf(file, "invert_mouse_y=%d\n", invert_mouse_y);
    fprintf(file, "raw_input_enabled=%d\n", raw_input_enabled);
    
    fprintf(file, "\n# Key Bindings\n");
    for (int i = 0; i < binding_count; i++) {
        if (!key_bindings[i].active) continue;
        
        KeyBinding* binding = &key_bindings[i];
        fprintf(file, "bind %s %d %d %d %.2f %d\n",
                get_action_name(binding->action),
                binding->device,
                binding->primary_key,
                binding->secondary_key,
                binding->sensitivity,
                binding->modifier_keys);
    }
    
    fclose(file);
}

void load_input_config(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) return;
    
    char line[256];
    while (fgets(line, sizeof(line), file)) {
        // Skip comments and empty lines
        if (line[0] == '#' || line[0] == '\n') continue;
        
        // Parse configuration lines
        if (strncmp(line, "mouse_sensitivity=", 18) == 0) {
            sscanf(line + 18, "%f", &mouse_sensitivity);
            mouse.sensitivity = mouse_sensitivity;
        } else if (strncmp(line, "invert_mouse_y=", 15) == 0) {
            sscanf(line + 15, "%d", &invert_mouse_y);
        } else if (strncmp(line, "raw_input_enabled=", 18) == 0) {
            sscanf(line + 18, "%d", &raw_input_enabled);
        }
        // Key binding parsing would go here (more complex)
    }
    
    fclose(file);
}

void cleanup_input_system() {
    // Release mouse capture
    capture_mouse(0);
    
    // Clear all states
    memset(&keyboard, 0, sizeof(KeyboardState));
    memset(&mouse, 0, sizeof(MouseState));
    memset(&gamepad, 0, sizeof(GamepadState));
    
    binding_count = 0;
    event_buffer_head = 0;
    event_buffer_tail = 0;
    command_queue_head = 0;
    command_queue_tail = 0;
}
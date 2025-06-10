/**
 * ========================================================================
 * ENDOR GAME LOGIC AND AI SYSTEM
 * ========================================================================
 * 
 * Core gameplay mechanics, AI behaviors, and game state management
 * for the Endor game engine. This module handles all game logic including:
 * 
 * - Player mechanics and movement
 * - Enemy AI with state machines
 * - Projectile physics and collision detection
 * - Power-up system with various effects
 * - Level progression and wave spawning
 * - Score tracking and achievements
 * - Difficulty scaling and balancing
 * 
 * The AI system uses behavior trees and state machines to create
 * intelligent enemy behaviors that adapt to player actions. The physics
 * system provides realistic movement and collision detection.
 * 
 * Performance optimizations include spatial partitioning for collision
 * detection and object pooling for projectiles and particles.
 */

#include "endor_readable.h"
#include <math.h>
#include <time.h>
#include <string.h>
#include <stdio.h>

// Define M_PI if not already defined
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ========================================================================
// GAME CONSTANTS AND CONFIGURATION
// ========================================================================

// Entity limits
#define MAX_ENEMIES 64
#define MAX_PROJECTILES 128
#define MAX_POWER_UPS 32
#define MAX_LEVEL_OBJECTS 256
#define MAX_PARTICLES 256
#define MAX_ACHIEVEMENTS 32

// Player constants
#define PLAYER_MAX_HEALTH 100
#define PLAYER_MAX_ENERGY 100
#define PLAYER_MAX_SHIELD 100
#define PLAYER_BASE_SPEED 10.0f
#define PLAYER_MAX_WEAPON_LEVEL 5
#define PLAYER_RESPAWN_TIME 3.0f
#define PLAYER_INVULNERABLE_TIME 3.0f

// Game physics
#define GRAVITY -9.81f
#define FRICTION_GROUND 0.9f
#define FRICTION_AIR 0.98f
#define PROJECTILE_SPEED 500.0f
#define MAX_VELOCITY 100.0f

// AI parameters
#define AI_UPDATE_INTERVAL 0.1f
#define AI_REACTION_TIME 0.2f
#define AI_PREDICTION_TIME 0.5f
#define AI_HEARING_RANGE 15.0f
#define AI_TEAM_COORDINATION_RANGE 10.0f

// Collision detection
#define COLLISION_EPSILON 0.001f
#define SPATIAL_GRID_SIZE 10.0f
#define MAX_COLLISION_ITERATIONS 3

// ========================================================================
// ENUMERATIONS
// ========================================================================

/**
 * Main game states
 */
typedef enum {
    GAME_STATE_MENU,
    GAME_STATE_PLAYING,
    GAME_STATE_PAUSED,
    GAME_STATE_GAME_OVER,
    GAME_STATE_LEVEL_COMPLETE,
    GAME_STATE_CUTSCENE,
    GAME_STATE_LOADING,
    GAME_STATE_VICTORY
} GameState;

/**
 * Enemy types with unique behaviors
 */
typedef enum {
    ENEMY_TYPE_GRUNT,      // Basic melee enemy
    ENEMY_TYPE_SCOUT,      // Fast, weak ranged enemy
    ENEMY_TYPE_HEAVY,      // Slow, heavily armored
    ENEMY_TYPE_SNIPER,     // Long range, high damage
    ENEMY_TYPE_BOMBER,     // Explosive attacks
    ENEMY_TYPE_ELITE,      // Advanced AI, mixed attacks
    ENEMY_TYPE_BOSS,       // Level boss
    ENEMY_TYPE_TURRET,     // Stationary defense
    ENEMY_TYPE_DRONE       // Flying enemy
} EnemyType;

/**
 * AI behavior states
 */
typedef enum {
    AI_STATE_IDLE,
    AI_STATE_PATROL,
    AI_STATE_SEARCH,
    AI_STATE_CHASE,
    AI_STATE_ATTACK,
    AI_STATE_RETREAT,
    AI_STATE_COVER,
    AI_STATE_FLANK,
    AI_STATE_COORDINATE,
    AI_STATE_DEAD
} AIState;

/**
 * Power-up types
 */
typedef enum {
    POWERUP_HEALTH,
    POWERUP_ENERGY,
    POWERUP_WEAPON_UPGRADE,
    POWERUP_SHIELD,
    POWERUP_SPEED_BOOST,
    POWERUP_EXTRA_LIFE,
    POWERUP_INVULNERABILITY,
    POWERUP_RAPID_FIRE,
    POWERUP_SPREAD_SHOT,
    POWERUP_HOMING_MISSILES
} PowerUpType;

/**
 * Weapon types
 */
typedef enum {
    WEAPON_BLASTER,
    WEAPON_SHOTGUN,
    WEAPON_MACHINEGUN,
    WEAPON_ROCKET_LAUNCHER,
    WEAPON_LASER,
    WEAPON_PLASMA_CANNON
} WeaponType;

/**
 * Achievement types
 */
typedef enum {
    ACHIEVEMENT_FIRST_KILL,
    ACHIEVEMENT_HEADSHOT_MASTER,
    ACHIEVEMENT_SURVIVOR,
    ACHIEVEMENT_SPEEDRUN,
    ACHIEVEMENT_PACIFIST,
    ACHIEVEMENT_PERFECTIONIST,
    ACHIEVEMENT_BOSS_SLAYER
} AchievementType;

// ========================================================================
// DATA STRUCTURES
// ========================================================================

/**
 * 3D bounding box for collision detection
 */
typedef struct {
    Vector3D min;
    Vector3D max;
} BoundingBox;

/**
 * Player data structure
 */
typedef struct {
    // Position and physics
    Vector3D position;
    Vector3D velocity;
    Vector3D rotation;
    BoundingBox bounds;
    
    // Stats
    int health;
    int max_health;
    int energy;
    int max_energy;
    int shield;
    int max_shield;
    
    // Game state
    int lives;
    int score;
    int kills;
    int deaths;
    float play_time;
    
    // Weapons and abilities
    WeaponType current_weapon;
    int weapon_levels[6];  // Level for each weapon type
    float fire_rate_multiplier;
    float damage_multiplier;
    float speed_multiplier;
    
    // Status effects
    float shield_time;
    float speed_boost_time;
    float invulnerable_time;
    float rapid_fire_time;
    
    // State flags
    BOOL active;
    BOOL on_ground;
    BOOL is_crouching;
    BOOL is_sprinting;
} Player;

/**
 * Enemy data structure with enhanced AI
 */
typedef struct {
    // Position and physics
    Vector3D position;
    Vector3D velocity;
    Vector3D rotation;
    BoundingBox bounds;
    
    // Enemy properties
    EnemyType type;
    int variant;  // Sub-variant for visual/behavior differences
    
    // Stats
    int health;
    int max_health;
    int armor;
    float movement_speed;
    
    // AI state
    AIState ai_state;
    AIState previous_state;
    float ai_timer;
    float reaction_timer;
    
    // AI memory
    Vector3D last_known_player_pos;
    float last_seen_player_time;
    Vector3D patrol_points[4];
    int current_patrol_point;
    int patrol_point_count;
    
    // Combat
    float detection_range;
    float attack_range;
    float field_of_view;
    float last_attack_time;
    float attack_cooldown;
    int damage;
    
    // Team coordination
    int team_id;
    int squad_role;  // 0=leader, 1=flanker, 2=support
    
    // State flags
    BOOL active;
    BOOL alerted;
    BOOL can_see_player;
    
    // Rewards
    int points_value;
    float powerup_drop_chance;
} Enemy;

/**
 * Projectile data structure
 */
typedef struct {
    // Physics
    Vector3D position;
    Vector3D velocity;
    Vector3D rotation;
    float gravity_multiplier;
    
    // Properties
    int damage;
    float splash_radius;
    float lifetime;
    float max_lifetime;
    
    // Ownership
    int owner_type;  // 0=player, 1=enemy
    int owner_id;    // Index of owner
    
    // Behavior
    BOOL homing;
    int homing_target;
    float homing_strength;
    
    // Visuals
    COLORREF color;
    int particle_trail;
    
    BOOL active;
} Projectile;

/**
 * Power-up data structure
 */
typedef struct {
    // Position
    Vector3D position;
    Vector3D rotation;
    
    // Properties
    PowerUpType type;
    int value;
    float duration;
    float lifetime;
    
    // Animation
    float bob_offset;
    float spin_speed;
    float pulse_scale;
    
    // Visuals
    COLORREF color;
    COLORREF glow_color;
    
    BOOL active;
} PowerUp;

/**
 * Achievement data
 */
typedef struct {
    AchievementType type;
    char name[64];
    char description[256];
    int progress;
    int target;
    BOOL unlocked;
    DWORD unlock_time;
} Achievement;

/**
 * Level data
 */
typedef struct {
    int level_number;
    char level_name[64];
    float time_limit;
    int enemy_count;
    int waves;
    float difficulty_modifier;
    BOOL has_boss;
} LevelData;

// ========================================================================
// GLOBAL GAME STATE
// ========================================================================

// Core game state
static GameState g_current_game_state = GAME_STATE_MENU;
static GameState g_previous_game_state = GAME_STATE_MENU;
static Player g_player;
static Enemy g_enemies[MAX_ENEMIES];
static Projectile g_projectiles[MAX_PROJECTILES];
static PowerUp g_power_ups[MAX_POWER_UPS];
static Achievement g_achievements[MAX_ACHIEVEMENTS];

// Level state
static LevelData g_current_level_data;
static int g_current_level = 1;
static float g_game_time = 0.0f;
static float g_level_time = 0.0f;
static int g_enemies_remaining = 0;
static int g_total_enemies_spawned = 0;
static int g_current_wave = 0;

// Game configuration
static float g_difficulty_multiplier = 1.0f;
static BOOL g_auto_aim_enabled = FALSE;
static BOOL g_friendly_fire = FALSE;
static BOOL g_god_mode = FALSE;

// Statistics
static struct {
    int total_kills;
    int total_deaths;
    int shots_fired;
    int shots_hit;
    int power_ups_collected;
    float total_play_time;
    int highest_combo;
    int current_combo;
    float combo_timer;
} g_stats;

// ========================================================================
// INTERNAL UTILITY FUNCTIONS
// ========================================================================

/**
 * Logs game logic messages for debugging
 * @param format Printf-style format string
 * @param ... Variable arguments
 */
static void game_log(const char* format, ...)
{
    char buffer[512];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    OutputDebugString("[GAME] ");
    OutputDebugString(buffer);
    OutputDebugString("\n");
}

/**
 * Generates a random float between min and max
 * @param min Minimum value
 * @param max Maximum value
 * @return Random float value
 */
static float random_range(float min, float max)
{
    return min + ((float)rand() / RAND_MAX) * (max - min);
}

/**
 * Calculates distance between two 3D points
 * @param a First point
 * @param b Second point
 * @return Distance
 */
static float distance_3d(const Vector3D* a, const Vector3D* b)
{
    float dx = a->x - b->x;
    float dy = a->y - b->y;
    float dz = a->z - b->z;
    return sqrtf(dx*dx + dy*dy + dz*dz);
}

/**
 * Normalizes an angle to [-PI, PI]
 * @param angle Angle in radians
 * @return Normalized angle
 */
static float normalize_angle(float angle)
{
    while (angle > M_PI) angle -= 2.0f * M_PI;
    while (angle < -M_PI) angle += 2.0f * M_PI;
    return angle;
}

// ========================================================================
// COLLISION DETECTION SYSTEM
// ========================================================================

/**
 * Creates a bounding box from position and size
 * @param center Center position
 * @param half_size Half extents of the box
 * @return Bounding box
 */
static BoundingBox create_bounding_box(Vector3D center, Vector3D half_size)
{
    BoundingBox box;
    box.min.x = center.x - half_size.x;
    box.min.y = center.y - half_size.y;
    box.min.z = center.z - half_size.z;
    box.max.x = center.x + half_size.x;
    box.max.y = center.y + half_size.y;
    box.max.z = center.z + half_size.z;
    return box;
}

/**
 * Tests if two bounding boxes intersect
 * @param a First bounding box
 * @param b Second bounding box
 * @return TRUE if boxes intersect
 */
static BOOL bounding_box_intersect(const BoundingBox* a, const BoundingBox* b)
{
    return (a->min.x <= b->max.x && a->max.x >= b->min.x) &&
           (a->min.y <= b->max.y && a->max.y >= b->min.y) &&
           (a->min.z <= b->max.z && a->max.z >= b->min.z);
}

/**
 * Tests if a point is inside a bounding box
 * @param point Point to test
 * @param box Bounding box
 * @return TRUE if point is inside
 */
static BOOL point_in_bounding_box(const Vector3D* point, const BoundingBox* box)
{
    return point->x >= box->min.x && point->x <= box->max.x &&
           point->y >= box->min.y && point->y <= box->max.y &&
           point->z >= box->min.z && point->z <= box->max.z;
}

/**
 * Performs sphere-box collision test
 * @param sphere_center Sphere center position
 * @param sphere_radius Sphere radius
 * @param box Bounding box
 * @return TRUE if collision detected
 */
static BOOL sphere_box_collision(const Vector3D* sphere_center, float sphere_radius, 
                                const BoundingBox* box)
{
    // Find closest point on box to sphere center
    Vector3D closest;
    closest.x = fmaxf(box->min.x, fminf(sphere_center->x, box->max.x));
    closest.y = fmaxf(box->min.y, fminf(sphere_center->y, box->max.y));
    closest.z = fmaxf(box->min.z, fminf(sphere_center->z, box->max.z));
    
    // Check if distance is less than radius
    float distance = distance_3d(sphere_center, &closest);
    return distance < sphere_radius;
}

// ========================================================================
// PLAYER MANAGEMENT
// ========================================================================

/**
 * Initializes the player for a new game
 */
void initialize_player(void)
{
    memset(&g_player, 0, sizeof(Player));
    
    // Set initial position
    g_player.position = (Vector3D){0.0f, 0.0f, 0.0f};
    g_player.velocity = (Vector3D){0.0f, 0.0f, 0.0f};
    g_player.rotation = (Vector3D){0.0f, 0.0f, 0.0f};
    
    // Initialize stats
    g_player.health = PLAYER_MAX_HEALTH;
    g_player.max_health = PLAYER_MAX_HEALTH;
    g_player.energy = PLAYER_MAX_ENERGY;
    g_player.max_energy = PLAYER_MAX_ENERGY;
    g_player.shield = 0;
    g_player.max_shield = PLAYER_MAX_SHIELD;
    
    // Game state
    g_player.lives = 3;
    g_player.score = 0;
    g_player.kills = 0;
    g_player.deaths = 0;
    g_player.play_time = 0.0f;
    
    // Weapons
    g_player.current_weapon = WEAPON_BLASTER;
    g_player.weapon_levels[WEAPON_BLASTER] = 1;
    for (int i = 1; i < 6; i++) {
        g_player.weapon_levels[i] = 0;
    }
    
    // Multipliers
    g_player.fire_rate_multiplier = 1.0f;
    g_player.damage_multiplier = 1.0f;
    g_player.speed_multiplier = 1.0f;
    
    // Create bounding box
    Vector3D half_size = {0.5f, 1.0f, 0.5f};
    g_player.bounds = create_bounding_box(g_player.position, half_size);
    
    g_player.active = TRUE;
    g_player.on_ground = TRUE;
    
    game_log("Player initialized with %d lives", g_player.lives);
}

/**
 * Updates player state each frame
 * @param delta_time Time since last update in seconds
 */
void update_player(float delta_time)
{
    if (!g_player.active) return;
    
    // Update timers
    g_player.play_time += delta_time;
    
    // Apply gravity if not on ground
    if (!g_player.on_ground) {
        g_player.velocity.y += GRAVITY * delta_time;
    }
    
    // Update position
    g_player.position.x += g_player.velocity.x * delta_time;
    g_player.position.y += g_player.velocity.y * delta_time;
    g_player.position.z += g_player.velocity.z * delta_time;
    
    // Apply friction
    float friction = g_player.on_ground ? FRICTION_GROUND : FRICTION_AIR;
    g_player.velocity.x *= friction;
    g_player.velocity.z *= friction;
    
    // Update bounding box
    Vector3D half_size = {0.5f, 1.0f, 0.5f};
    g_player.bounds = create_bounding_box(g_player.position, half_size);
    
    // Update status effects
    if (g_player.invulnerable_time > 0.0f) {
        g_player.invulnerable_time -= delta_time;
    }
    
    if (g_player.shield_time > 0.0f) {
        g_player.shield_time -= delta_time;
        if (g_player.shield_time <= 0.0f) {
            g_player.shield = 0;
        }
    }
    
    if (g_player.speed_boost_time > 0.0f) {
        g_player.speed_boost_time -= delta_time;
        if (g_player.speed_boost_time <= 0.0f) {
            g_player.speed_multiplier = 1.0f;
        }
    }
    
    if (g_player.rapid_fire_time > 0.0f) {
        g_player.rapid_fire_time -= delta_time;
        if (g_player.rapid_fire_time <= 0.0f) {
            g_player.fire_rate_multiplier = 1.0f;
        }
    }
    
    // Regenerate energy slowly
    if (g_player.energy < g_player.max_energy) {
        g_player.energy = (int)fminf(g_player.max_energy, 
                                     g_player.energy + 20.0f * delta_time);
    }
    
    // Check boundaries
    g_player.position.x = fmaxf(-50.0f, fminf(50.0f, g_player.position.x));
    g_player.position.z = fmaxf(-50.0f, fminf(50.0f, g_player.position.z));
    g_player.position.y = fmaxf(0.0f, fminf(20.0f, g_player.position.y));
    
    // Ground check
    if (g_player.position.y <= 0.0f) {
        g_player.position.y = 0.0f;
        g_player.velocity.y = 0.0f;
        g_player.on_ground = TRUE;
    }
}

/**
 * Moves the player based on input
 * @param forward Forward/backward movement (-1 to 1)
 * @param strafe Left/right movement (-1 to 1)
 * @param up Up/down movement (-1 to 1)
 */
void move_player(float forward, float strafe, float up)
{
    if (!g_player.active) return;
    
    float speed = PLAYER_BASE_SPEED * g_player.speed_multiplier;
    if (g_player.is_sprinting) speed *= 1.5f;
    if (g_player.is_crouching) speed *= 0.5f;
    
    // Calculate movement based on rotation
    float cos_y = cosf(g_player.rotation.y);
    float sin_y = sinf(g_player.rotation.y);
    
    // Apply movement
    g_player.velocity.x += (forward * sin_y + strafe * cos_y) * speed * 0.1f;
    g_player.velocity.z += (forward * cos_y - strafe * sin_y) * speed * 0.1f;
    
    // Jump
    if (up > 0 && g_player.on_ground) {
        g_player.velocity.y = 10.0f;
        g_player.on_ground = FALSE;
    }
    
    // Clamp velocity
    float horizontal_speed = sqrtf(g_player.velocity.x * g_player.velocity.x + 
                                  g_player.velocity.z * g_player.velocity.z);
    if (horizontal_speed > MAX_VELOCITY) {
        float scale = MAX_VELOCITY / horizontal_speed;
        g_player.velocity.x *= scale;
        g_player.velocity.z *= scale;
    }
}

/**
 * Rotates the player view
 * @param yaw Horizontal rotation
 * @param pitch Vertical rotation
 */
void rotate_player(float yaw, float pitch)
{
    if (!g_player.active) return;
    
    g_player.rotation.y += yaw;
    g_player.rotation.x += pitch;
    
    // Normalize and clamp angles
    g_player.rotation.y = normalize_angle(g_player.rotation.y);
    g_player.rotation.x = fmaxf(-M_PI/2.0f, fminf(M_PI/2.0f, g_player.rotation.x));
}

/**
 * Damages the player
 * @param damage Amount of damage to deal
 * @return TRUE if damage was applied
 */
int damage_player(int damage)
{
    if (!g_player.active || g_player.invulnerable_time > 0.0f || g_god_mode) {
        return FALSE;
    }
    
    // Apply damage to shield first
    if (g_player.shield > 0) {
        int shield_damage = (int)fminf(damage, g_player.shield);
        g_player.shield -= shield_damage;
        damage -= shield_damage;
    }
    
    // Apply remaining damage to health
    if (damage > 0) {
        g_player.health -= damage;
        g_player.invulnerable_time = 0.5f;  // Brief invulnerability
        
        if (g_player.health <= 0) {
            g_player.health = 0;
            g_player.deaths++;
            g_player.lives--;
            
            if (g_player.lives <= 0) {
                g_player.active = FALSE;
                g_current_game_state = GAME_STATE_GAME_OVER;
                game_log("Game Over! Final score: %d", g_player.score);
            } else {
                // Respawn
                g_player.health = g_player.max_health;
                g_player.position = (Vector3D){0.0f, 0.0f, 0.0f};
                g_player.velocity = (Vector3D){0.0f, 0.0f, 0.0f};
                g_player.invulnerable_time = PLAYER_INVULNERABLE_TIME;
                game_log("Player respawned with %d lives remaining", g_player.lives);
            }
        }
    }
    
    return TRUE;
}

/**
 * Heals the player
 * @param amount Amount to heal
 */
void heal_player(int amount)
{
    g_player.health = (int)fminf(g_player.max_health, g_player.health + amount);
    game_log("Player healed for %d HP (current: %d/%d)", 
             amount, g_player.health, g_player.max_health);
}

/**
 * Restores player energy
 * @param amount Amount to restore
 */
void restore_energy(int amount)
{
    g_player.energy = (int)fminf(g_player.max_energy, g_player.energy + amount);
}

/**
 * Adds shield to the player
 * @param amount Shield amount
 */
void add_shield(int amount)
{
    g_player.shield = (int)fminf(g_player.max_shield, g_player.shield + amount);
    if (amount > 0 && g_player.shield_time <= 0.0f) {
        g_player.shield_time = 10.0f;  // Shield lasts 10 seconds
    }
}

// ========================================================================
// ENEMY AI SYSTEM
// ========================================================================

/**
 * Spawns an enemy at specified location
 * @param type Enemy type
 * @param position Spawn position
 * @return Enemy index or -1 if failed
 */
int spawn_enemy(EnemyType type, Vector3D position)
{
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (!g_enemies[i].active) {
            Enemy* enemy = &g_enemies[i];
            memset(enemy, 0, sizeof(Enemy));
            
            // Set position
            enemy->position = position;
            enemy->velocity = (Vector3D){0.0f, 0.0f, 0.0f};
            enemy->rotation = (Vector3D){0.0f, random_range(0, 2*M_PI), 0.0f};
            
            // Set type and variant
            enemy->type = type;
            enemy->variant = rand() % 3;  // 3 visual variants per type
            
            // Initialize AI state
            enemy->ai_state = AI_STATE_PATROL;
            enemy->previous_state = AI_STATE_PATROL;
            enemy->ai_timer = 0.0f;
            enemy->reaction_timer = AI_REACTION_TIME;
            
            // Set patrol points around spawn position
            enemy->patrol_point_count = 4;
            for (int j = 0; j < 4; j++) {
                float angle = j * M_PI / 2.0f;
                enemy->patrol_points[j].x = position.x + cosf(angle) * 5.0f;
                enemy->patrol_points[j].y = position.y;
                enemy->patrol_points[j].z = position.z + sinf(angle) * 5.0f;
            }
            enemy->current_patrol_point = 0;
            
            // Set type-specific properties
            switch (type) {
                case ENEMY_TYPE_GRUNT:
                    enemy->health = 30;
                    enemy->max_health = 30;
                    enemy->armor = 0;
                    enemy->movement_speed = 4.0f;
                    enemy->detection_range = 10.0f;
                    enemy->attack_range = 2.0f;
                    enemy->field_of_view = 120.0f;
                    enemy->attack_cooldown = 1.5f;
                    enemy->damage = 10;
                    enemy->points_value = 100;
                    enemy->powerup_drop_chance = 0.1f;
                    break;
                    
                case ENEMY_TYPE_SCOUT:
                    enemy->health = 15;
                    enemy->max_health = 15;
                    enemy->armor = 0;
                    enemy->movement_speed = 7.0f;
                    enemy->detection_range = 15.0f;
                    enemy->attack_range = 8.0f;
                    enemy->field_of_view = 160.0f;
                    enemy->attack_cooldown = 0.8f;
                    enemy->damage = 5;
                    enemy->points_value = 150;
                    enemy->powerup_drop_chance = 0.15f;
                    break;
                    
                case ENEMY_TYPE_HEAVY:
                    enemy->health = 80;
                    enemy->max_health = 80;
                    enemy->armor = 20;
                    enemy->movement_speed = 2.0f;
                    enemy->detection_range = 8.0f;
                    enemy->attack_range = 4.0f;
                    enemy->field_of_view = 90.0f;
                    enemy->attack_cooldown = 2.5f;
                    enemy->damage = 25;
                    enemy->points_value = 300;
                    enemy->powerup_drop_chance = 0.25f;
                    break;
                    
                case ENEMY_TYPE_SNIPER:
                    enemy->health = 20;
                    enemy->max_health = 20;
                    enemy->armor = 5;
                    enemy->movement_speed = 3.0f;
                    enemy->detection_range = 25.0f;
                    enemy->attack_range = 20.0f;
                    enemy->field_of_view = 60.0f;
                    enemy->attack_cooldown = 3.0f;
                    enemy->damage = 30;
                    enemy->points_value = 250;
                    enemy->powerup_drop_chance = 0.2f;
                    break;
                    
                case ENEMY_TYPE_BOMBER:
                    enemy->health = 40;
                    enemy->max_health = 40;
                    enemy->armor = 10;
                    enemy->movement_speed = 3.5f;
                    enemy->detection_range = 12.0f;
                    enemy->attack_range = 6.0f;
                    enemy->field_of_view = 120.0f;
                    enemy->attack_cooldown = 2.0f;
                    enemy->damage = 40;
                    enemy->points_value = 350;
                    enemy->powerup_drop_chance = 0.3f;
                    break;
                    
                case ENEMY_TYPE_ELITE:
                    enemy->health = 60;
                    enemy->max_health = 60;
                    enemy->armor = 15;
                    enemy->movement_speed = 5.0f;
                    enemy->detection_range = 15.0f;
                    enemy->attack_range = 10.0f;
                    enemy->field_of_view = 140.0f;
                    enemy->attack_cooldown = 1.0f;
                    enemy->damage = 15;
                    enemy->points_value = 500;
                    enemy->powerup_drop_chance = 0.4f;
                    break;
                    
                case ENEMY_TYPE_BOSS:
                    enemy->health = 500;
                    enemy->max_health = 500;
                    enemy->armor = 50;
                    enemy->movement_speed = 3.0f;
                    enemy->detection_range = 20.0f;
                    enemy->attack_range = 12.0f;
                    enemy->field_of_view = 180.0f;
                    enemy->attack_cooldown = 0.5f;
                    enemy->damage = 35;
                    enemy->points_value = 2000;
                    enemy->powerup_drop_chance = 1.0f;
                    break;
                    
                case ENEMY_TYPE_TURRET:
                    enemy->health = 50;
                    enemy->max_health = 50;
                    enemy->armor = 30;
                    enemy->movement_speed = 0.0f;
                    enemy->detection_range = 30.0f;
                    enemy->attack_range = 25.0f;
                    enemy->field_of_view = 360.0f;
                    enemy->attack_cooldown = 1.2f;
                    enemy->damage = 20;
                    enemy->points_value = 200;
                    enemy->powerup_drop_chance = 0.2f;
                    enemy->ai_state = AI_STATE_IDLE;
                    break;
                    
                case ENEMY_TYPE_DRONE:
                    enemy->health = 25;
                    enemy->max_health = 25;
                    enemy->armor = 0;
                    enemy->movement_speed = 6.0f;
                    enemy->detection_range = 12.0f;
                    enemy->attack_range = 6.0f;
                    enemy->field_of_view = 180.0f;
                    enemy->attack_cooldown = 1.0f;
                    enemy->damage = 8;
                    enemy->points_value = 180;
                    enemy->powerup_drop_chance = 0.15f;
                    enemy->position.y = 5.0f;  // Flying height
                    break;
            }
            
            // Apply difficulty multiplier
            enemy->health = (int)(enemy->health * g_difficulty_multiplier);
            enemy->max_health = enemy->health;
            enemy->damage = (int)(enemy->damage * g_difficulty_multiplier);
            
            // Create bounding box
            Vector3D half_size = {0.5f, 1.0f, 0.5f};
            if (type == ENEMY_TYPE_HEAVY || type == ENEMY_TYPE_BOSS) {
                half_size = (Vector3D){1.0f, 1.5f, 1.0f};
            }
            enemy->bounds = create_bounding_box(enemy->position, half_size);
            
            // Activate enemy
            enemy->active = TRUE;
            enemy->alerted = FALSE;
            enemy->can_see_player = FALSE;
            
            g_enemies_remaining++;
            g_total_enemies_spawned++;
            
            game_log("Spawned %s enemy at (%.1f, %.1f, %.1f)", 
                     get_enemy_type_name(type), position.x, position.y, position.z);
            
            return i;
        }
    }
    
    game_log("Failed to spawn enemy - no free slots");
    return -1;
}

/**
 * Gets enemy type name for logging
 * @param type Enemy type
 * @return Type name string
 */
const char* get_enemy_type_name(EnemyType type)
{
    static const char* names[] = {
        "Grunt", "Scout", "Heavy", "Sniper", "Bomber", 
        "Elite", "Boss", "Turret", "Drone"
    };
    return names[type];
}

/**
 * Checks if enemy can see the player
 * @param enemy Enemy to check
 * @return TRUE if player is visible
 */
static BOOL enemy_can_see_player(Enemy* enemy)
{
    float distance = distance_3d(&enemy->position, &g_player.position);
    
    // Check detection range
    if (distance > enemy->detection_range) {
        return FALSE;
    }
    
    // Check field of view
    Vector3D to_player = {
        g_player.position.x - enemy->position.x,
        g_player.position.y - enemy->position.y,
        g_player.position.z - enemy->position.z
    };
    
    float angle_to_player = atan2f(to_player.x, to_player.z);
    float angle_diff = fabsf(normalize_angle(angle_to_player - enemy->rotation.y));
    
    if (angle_diff > (enemy->field_of_view * M_PI / 360.0f)) {
        return FALSE;
    }
    
    // TODO: Add line-of-sight check with level geometry
    
    return TRUE;
}

// Forward declaration
static void update_enemy_movement(Enemy* enemy, float delta_time);

/**
 * Updates enemy AI behavior
 * @param enemy Enemy to update
 * @param delta_time Time since last update
 */
static void update_enemy_ai(Enemy* enemy, float delta_time)
{
    if (!enemy->active || enemy->ai_state == AI_STATE_DEAD) {
        return;
    }
    
    enemy->ai_timer += delta_time;
    
    // Update vision
    enemy->can_see_player = enemy_can_see_player(enemy);
    if (enemy->can_see_player) {
        enemy->last_known_player_pos = g_player.position;
        enemy->last_seen_player_time = g_game_time;
        enemy->alerted = TRUE;
    }
    
    // Calculate distance to player
    float distance_to_player = distance_3d(&enemy->position, &g_player.position);
    
    // Update reaction timer
    if (enemy->reaction_timer > 0.0f) {
        enemy->reaction_timer -= delta_time;
        return;  // Still reacting
    }
    
    // AI State machine
    switch (enemy->ai_state) {
        case AI_STATE_IDLE:
            if (enemy->can_see_player || 
                (enemy->alerted && g_game_time - enemy->last_seen_player_time < 5.0f)) {
                enemy->ai_state = AI_STATE_CHASE;
                enemy->reaction_timer = AI_REACTION_TIME;
            }
            break;
            
        case AI_STATE_PATROL:
            if (enemy->can_see_player) {
                enemy->ai_state = AI_STATE_CHASE;
                enemy->reaction_timer = AI_REACTION_TIME;
            } else {
                // Move to next patrol point
                Vector3D* target = &enemy->patrol_points[enemy->current_patrol_point];
                float dist_to_target = distance_3d(&enemy->position, target);
                
                if (dist_to_target < 1.0f) {
                    enemy->current_patrol_point = (enemy->current_patrol_point + 1) % 
                                                  enemy->patrol_point_count;
                }
            }
            break;
            
        case AI_STATE_SEARCH:
            if (enemy->can_see_player) {
                enemy->ai_state = AI_STATE_CHASE;
                enemy->reaction_timer = AI_REACTION_TIME * 0.5f;  // Faster reaction
            } else if (g_game_time - enemy->last_seen_player_time > 10.0f) {
                enemy->ai_state = AI_STATE_PATROL;
                enemy->alerted = FALSE;
            }
            break;
            
        case AI_STATE_CHASE:
            if (!enemy->can_see_player && 
                g_game_time - enemy->last_seen_player_time > 2.0f) {
                enemy->ai_state = AI_STATE_SEARCH;
            } else if (distance_to_player <= enemy->attack_range) {
                enemy->ai_state = AI_STATE_ATTACK;
                enemy->reaction_timer = AI_REACTION_TIME * 0.5f;
            } else if (enemy->health < enemy->max_health * 0.3f) {
                // Low health - consider retreat
                if (rand() % 100 < 30) {
                    enemy->ai_state = AI_STATE_RETREAT;
                }
            }
            break;
            
        case AI_STATE_ATTACK:
            if (distance_to_player > enemy->attack_range * 1.5f) {
                enemy->ai_state = AI_STATE_CHASE;
            } else if (g_game_time - enemy->last_attack_time >= enemy->attack_cooldown) {
                enemy_attack(enemy);
                enemy->last_attack_time = g_game_time;
                
                // Elite enemies may try to flank after attacking
                if (enemy->type == ENEMY_TYPE_ELITE && rand() % 100 < 50) {
                    enemy->ai_state = AI_STATE_FLANK;
                }
            }
            break;
            
        case AI_STATE_RETREAT:
            if (distance_to_player > enemy->detection_range * 1.5f ||
                enemy->health > enemy->max_health * 0.5f) {
                enemy->ai_state = AI_STATE_PATROL;
            }
            break;
            
        case AI_STATE_COVER:
            // TODO: Implement cover-seeking behavior
            if (enemy->can_see_player) {
                enemy->ai_state = AI_STATE_ATTACK;
            }
            break;
            
        case AI_STATE_FLANK:
            // Try to move to player's side
            if (distance_to_player < enemy->attack_range) {
                enemy->ai_state = AI_STATE_ATTACK;
            }
            break;
            
        case AI_STATE_COORDINATE:
            // TODO: Implement team coordination
            enemy->ai_state = AI_STATE_CHASE;
            break;
    }
    
    // Update movement based on state
    update_enemy_movement(enemy, delta_time);
}

/**
 * Updates enemy movement based on AI state
 * @param enemy Enemy to update
 * @param delta_time Time since last update
 */
static void update_enemy_movement(Enemy* enemy, float delta_time)
{
    Vector3D target_pos = enemy->position;
    BOOL should_move = TRUE;
    
    // Determine target position based on state
    switch (enemy->ai_state) {
        case AI_STATE_IDLE:
            should_move = FALSE;
            break;
            
        case AI_STATE_PATROL:
            target_pos = enemy->patrol_points[enemy->current_patrol_point];
            break;
            
        case AI_STATE_SEARCH:
            // Move toward last known player position
            target_pos = enemy->last_known_player_pos;
            break;
            
        case AI_STATE_CHASE:
            // Move directly toward player
            target_pos = g_player.position;
            break;
            
        case AI_STATE_ATTACK:
            // Strafe around player while attacking
            if (enemy->type != ENEMY_TYPE_TURRET) {
                float strafe_angle = g_game_time * 2.0f;
                target_pos.x = g_player.position.x + cosf(strafe_angle) * enemy->attack_range;
                target_pos.z = g_player.position.z + sinf(strafe_angle) * enemy->attack_range;
            } else {
                should_move = FALSE;
            }
            break;
            
        case AI_STATE_RETREAT:
            // Move away from player
            {
                Vector3D away = {
                    enemy->position.x - g_player.position.x,
                    0,
                    enemy->position.z - g_player.position.z
                };
                float mag = sqrtf(away.x * away.x + away.z * away.z);
                if (mag > 0.1f) {
                    away.x /= mag;
                    away.z /= mag;
                    target_pos.x = enemy->position.x + away.x * 10.0f;
                    target_pos.z = enemy->position.z + away.z * 10.0f;
                }
            }
            break;
            
        case AI_STATE_FLANK:
            // Move to player's side
            {
                Vector3D to_player = {
                    g_player.position.x - enemy->position.x,
                    0,
                    g_player.position.z - enemy->position.z
                };
                // Rotate 90 degrees
                float temp = to_player.x;
                to_player.x = -to_player.z;
                to_player.z = temp;
                float mag = sqrtf(to_player.x * to_player.x + to_player.z * to_player.z);
                if (mag > 0.1f) {
                    to_player.x /= mag;
                    to_player.z /= mag;
                    target_pos.x = g_player.position.x + to_player.x * enemy->attack_range;
                    target_pos.z = g_player.position.z + to_player.z * enemy->attack_range;
                }
            }
            break;
    }
    
    // Move toward target if needed
    if (should_move && enemy->movement_speed > 0.0f) {
        Vector3D direction = {
            target_pos.x - enemy->position.x,
            target_pos.y - enemy->position.y,
            target_pos.z - enemy->position.z
        };
        
        float distance = sqrtf(direction.x * direction.x + 
                              direction.y * direction.y + 
                              direction.z * direction.z);
        
        if (distance > 0.1f) {
            // Normalize direction
            direction.x /= distance;
            direction.y /= distance;
            direction.z /= distance;
            
            // Apply movement
            enemy->velocity.x = direction.x * enemy->movement_speed;
            enemy->velocity.z = direction.z * enemy->movement_speed;
            
            // Flying enemies
            if (enemy->type == ENEMY_TYPE_DRONE) {
                enemy->velocity.y = direction.y * enemy->movement_speed;
            }
            
            // Update rotation to face movement direction
            enemy->rotation.y = atan2f(direction.x, direction.z);
        }
    } else if (enemy->type == ENEMY_TYPE_TURRET) {
        // Turrets rotate to face player
        if (enemy->can_see_player) {
            float angle_to_player = atan2f(g_player.position.x - enemy->position.x,
                                          g_player.position.z - enemy->position.z);
            enemy->rotation.y = angle_to_player;
        }
    }
}

/**
 * Makes an enemy attack
 * @param enemy Enemy performing the attack
 */
static void enemy_attack(Enemy* enemy)
{
    Vector3D direction = {
        g_player.position.x - enemy->position.x,
        g_player.position.y - enemy->position.y,
        g_player.position.z - enemy->position.z
    };
    
    float distance = sqrtf(direction.x * direction.x + 
                          direction.y * direction.y + 
                          direction.z * direction.z);
    
    if (distance < 0.1f) return;
    
    // Normalize direction
    direction.x /= distance;
    direction.y /= distance;
    direction.z /= distance;
    
    // Add some inaccuracy based on enemy type
    float accuracy = 1.0f;
    switch (enemy->type) {
        case ENEMY_TYPE_GRUNT: accuracy = 0.7f; break;
        case ENEMY_TYPE_SCOUT: accuracy = 0.8f; break;
        case ENEMY_TYPE_SNIPER: accuracy = 0.95f; break;
        case ENEMY_TYPE_ELITE: accuracy = 0.9f; break;
        case ENEMY_TYPE_BOSS: accuracy = 0.85f; break;
    }
    
    if (accuracy < 1.0f) {
        direction.x += random_range(-0.1f, 0.1f) * (1.0f - accuracy);
        direction.y += random_range(-0.1f, 0.1f) * (1.0f - accuracy);
        direction.z += random_range(-0.1f, 0.1f) * (1.0f - accuracy);
        
        // Re-normalize
        float mag = sqrtf(direction.x * direction.x + 
                         direction.y * direction.y + 
                         direction.z * direction.z);
        direction.x /= mag;
        direction.y /= mag;
        direction.z /= mag;
    }
    
    // Spawn projectile
    Vector3D spawn_pos = enemy->position;
    spawn_pos.y += 1.0f;  // Spawn at chest height
    
    COLORREF projectile_color = RGB(255, 100, 100);
    
    // Calculate enemy index
    int enemy_index = (int)(enemy - g_enemies);
    
    // Special projectiles for certain enemy types
    if (enemy->type == ENEMY_TYPE_BOMBER) {
        // Explosive projectile
        Projectile* proj = spawn_projectile(
            spawn_pos,
            (Vector3D){direction.x * PROJECTILE_SPEED * 0.5f,
                      direction.y * PROJECTILE_SPEED * 0.5f,
                      direction.z * PROJECTILE_SPEED * 0.5f},
            enemy->damage,
            1,  // Enemy owned
            enemy_index
        );
        if (proj) {
            proj->splash_radius = 3.0f;
            proj->gravity_multiplier = 0.5f;
            proj->color = RGB(255, 200, 0);
        }
    } else {
        // Normal projectile
        spawn_projectile(
            spawn_pos,
            (Vector3D){direction.x * PROJECTILE_SPEED,
                      direction.y * PROJECTILE_SPEED,
                      direction.z * PROJECTILE_SPEED},
            enemy->damage,
            1,  // Enemy owned
            enemy_index
        );
    }
    
    game_log("%s attacked player for %d damage", 
             get_enemy_type_name(enemy->type), enemy->damage);
}

/**
 * Updates all active enemies
 * @param delta_time Time since last update
 */
void update_enemies(float delta_time)
{
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (!g_enemies[i].active) continue;
        
        Enemy* enemy = &g_enemies[i];
        
        // Update AI
        update_enemy_ai(enemy, delta_time);
        
        // Update physics
        enemy->position.x += enemy->velocity.x * delta_time;
        enemy->position.y += enemy->velocity.y * delta_time;
        enemy->position.z += enemy->velocity.z * delta_time;
        
        // Apply friction
        enemy->velocity.x *= FRICTION_GROUND;
        enemy->velocity.z *= FRICTION_GROUND;
        
        // Gravity for non-flying enemies
        if (enemy->type != ENEMY_TYPE_DRONE) {
            enemy->velocity.y += GRAVITY * delta_time;
            
            // Ground collision
            if (enemy->position.y <= 0.0f) {
                enemy->position.y = 0.0f;
                enemy->velocity.y = 0.0f;
            }
        }
        
        // Keep enemies in bounds
        enemy->position.x = fmaxf(-60.0f, fminf(60.0f, enemy->position.x));
        enemy->position.z = fmaxf(-60.0f, fminf(60.0f, enemy->position.z));
        enemy->position.y = fmaxf(0.0f, fminf(20.0f, enemy->position.y));
        
        // Update bounding box
        Vector3D half_size = {0.5f, 1.0f, 0.5f};
        if (enemy->type == ENEMY_TYPE_HEAVY || enemy->type == ENEMY_TYPE_BOSS) {
            half_size = (Vector3D){1.0f, 1.5f, 1.0f};
        }
        enemy->bounds = create_bounding_box(enemy->position, half_size);
    }
}

/**
 * Damages an enemy
 * @param enemy_index Enemy array index
 * @param damage Amount of damage
 * @return TRUE if enemy was killed
 */
int damage_enemy(int enemy_index, int damage)
{
    if (enemy_index < 0 || enemy_index >= MAX_ENEMIES || 
        !g_enemies[enemy_index].active) {
        return FALSE;
    }
    
    Enemy* enemy = &g_enemies[enemy_index];
    
    // Apply armor reduction
    if (enemy->armor > 0) {
        damage = (int)(damage * (1.0f - enemy->armor / 100.0f));
    }
    
    enemy->health -= damage;
    
    // Alert nearby enemies
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (i != enemy_index && g_enemies[i].active) {
            float dist = distance_3d(&enemy->position, &g_enemies[i].position);
            if (dist < AI_HEARING_RANGE) {
                g_enemies[i].alerted = TRUE;
                g_enemies[i].last_known_player_pos = g_player.position;
            }
        }
    }
    
    if (enemy->health <= 0) {
        // Enemy killed
        enemy->active = FALSE;
        enemy->ai_state = AI_STATE_DEAD;
        g_enemies_remaining--;
        
        // Update player stats
        g_player.score += enemy->points_value;
        g_player.kills++;
        g_stats.total_kills++;
        
        // Update combo
        g_stats.combo_timer = 2.0f;
        g_stats.current_combo++;
        if (g_stats.current_combo > g_stats.highest_combo) {
            g_stats.highest_combo = g_stats.current_combo;
        }
        
        // Combo bonus
        if (g_stats.current_combo > 5) {
            int combo_bonus = enemy->points_value * (g_stats.current_combo / 5);
            g_player.score += combo_bonus;
            game_log("Combo x%d! Bonus: %d points", g_stats.current_combo, combo_bonus);
        }
        
        // Drop power-up
        if (random_range(0, 1) < enemy->powerup_drop_chance) {
            PowerUpType type = (PowerUpType)(rand() % 10);
            spawn_power_up(type, enemy->position);
        }
        
        // Check achievements
        check_kill_achievements(enemy->type);
        
        game_log("Enemy killed: %s (score: %d)", 
                 get_enemy_type_name(enemy->type), enemy->points_value);
        
        return TRUE;
    }
    
    return FALSE;
}

// ========================================================================
// PROJECTILE SYSTEM
// ========================================================================

/**
 * Spawns a projectile
 * @param position Starting position
 * @param velocity Initial velocity
 * @param damage Damage amount
 * @param owner_type 0=player, 1=enemy
 * @param owner_id Index of owner
 * @return Pointer to projectile or NULL
 */
Projectile* spawn_projectile(Vector3D position, Vector3D velocity, 
                           int damage, int owner_type, int owner_id)
{
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        if (!g_projectiles[i].active) {
            Projectile* proj = &g_projectiles[i];
            
            proj->position = position;
            proj->velocity = velocity;
            proj->rotation = (Vector3D){0, atan2f(velocity.x, velocity.z), 0};
            proj->damage = damage;
            proj->splash_radius = 0.0f;
            proj->lifetime = 0.0f;
            proj->max_lifetime = 5.0f;
            proj->owner_type = owner_type;
            proj->owner_id = owner_id;
            proj->gravity_multiplier = 0.0f;
            proj->homing = FALSE;
            proj->homing_target = -1;
            proj->homing_strength = 0.0f;
            proj->active = TRUE;
            
            // Set color based on owner
            if (owner_type == 0) {
                proj->color = RGB(100, 255, 100);  // Player = green
            } else {
                proj->color = RGB(255, 100, 100);  // Enemy = red
            }
            
            return proj;
        }
    }
    
    return NULL;
}

/**
 * Updates all active projectiles
 * @param delta_time Time since last update
 */
void update_projectiles(float delta_time)
{
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        if (!g_projectiles[i].active) continue;
        
        Projectile* proj = &g_projectiles[i];
        
        // Apply gravity if enabled
        if (proj->gravity_multiplier > 0.0f) {
            proj->velocity.y += GRAVITY * proj->gravity_multiplier * delta_time;
        }
        
        // Homing behavior
        if (proj->homing && proj->homing_target >= 0) {
            Vector3D* target_pos = NULL;
            
            if (proj->owner_type == 0 && proj->homing_target < MAX_ENEMIES) {
                // Player projectile homing on enemy
                if (g_enemies[proj->homing_target].active) {
                    target_pos = &g_enemies[proj->homing_target].position;
                }
            } else if (proj->owner_type == 1) {
                // Enemy projectile homing on player
                target_pos = &g_player.position;
            }
            
            if (target_pos) {
                Vector3D to_target = {
                    target_pos->x - proj->position.x,
                    target_pos->y - proj->position.y,
                    target_pos->z - proj->position.z
                };
                
                float dist = sqrtf(to_target.x * to_target.x + 
                                  to_target.y * to_target.y + 
                                  to_target.z * to_target.z);
                
                if (dist > 0.1f) {
                    to_target.x /= dist;
                    to_target.y /= dist;
                    to_target.z /= dist;
                    
                    // Adjust velocity toward target
                    proj->velocity.x += to_target.x * proj->homing_strength * delta_time;
                    proj->velocity.y += to_target.y * proj->homing_strength * delta_time;
                    proj->velocity.z += to_target.z * proj->homing_strength * delta_time;
                    
                    // Limit speed
                    float speed = sqrtf(proj->velocity.x * proj->velocity.x + 
                                       proj->velocity.y * proj->velocity.y + 
                                       proj->velocity.z * proj->velocity.z);
                    if (speed > PROJECTILE_SPEED * 1.5f) {
                        float scale = (PROJECTILE_SPEED * 1.5f) / speed;
                        proj->velocity.x *= scale;
                        proj->velocity.y *= scale;
                        proj->velocity.z *= scale;
                    }
                }
            }
        }
        
        // Update position
        proj->position.x += proj->velocity.x * delta_time;
        proj->position.y += proj->velocity.y * delta_time;
        proj->position.z += proj->velocity.z * delta_time;
        
        // Update lifetime
        proj->lifetime += delta_time;
        if (proj->lifetime >= proj->max_lifetime) {
            proj->active = FALSE;
            continue;
        }
        
        // Check boundaries
        if (proj->position.x < -70.0f || proj->position.x > 70.0f ||
            proj->position.y < -10.0f || proj->position.y > 30.0f ||
            proj->position.z < -70.0f || proj->position.z > 70.0f) {
            proj->active = FALSE;
            continue;
        }
        
        // Ground collision
        if (proj->position.y <= 0.0f && proj->splash_radius > 0.0f) {
            // Explode on ground contact
            create_explosion(proj->position, proj->splash_radius, proj->damage);
            proj->active = FALSE;
            continue;
        }
        
        // Check collisions
        if (proj->owner_type == 0) {
            // Player projectile - check enemy collisions
            for (int j = 0; j < MAX_ENEMIES; j++) {
                if (!g_enemies[j].active) continue;
                
                if (sphere_box_collision(&proj->position, 0.2f, &g_enemies[j].bounds)) {
                    if (proj->splash_radius > 0.0f) {
                        create_explosion(proj->position, proj->splash_radius, proj->damage);
                    } else {
                        damage_enemy(j, proj->damage);
                    }
                    proj->active = FALSE;
                    g_stats.shots_hit++;
                    break;
                }
            }
        } else {
            // Enemy projectile - check player collision
            if (sphere_box_collision(&proj->position, 0.2f, &g_player.bounds)) {
                if (proj->splash_radius > 0.0f) {
                    create_explosion(proj->position, proj->splash_radius, proj->damage);
                } else {
                    damage_player(proj->damage);
                }
                proj->active = FALSE;
            }
        }
    }
}

/**
 * Creates an explosion at the specified position
 * @param position Explosion center
 * @param radius Explosion radius
 * @param damage Maximum damage
 */
void create_explosion(Vector3D position, float radius, int damage)
{
    // Damage player if in range
    float player_dist = distance_3d(&position, &g_player.position);
    if (player_dist < radius) {
        float damage_factor = 1.0f - (player_dist / radius);
        damage_player((int)(damage * damage_factor));
    }
    
    // Damage enemies in range
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (!g_enemies[i].active) continue;
        
        float enemy_dist = distance_3d(&position, &g_enemies[i].position);
        if (enemy_dist < radius) {
            float damage_factor = 1.0f - (enemy_dist / radius);
            damage_enemy(i, (int)(damage * damage_factor));
        }
    }
    
    // Create visual effect
    // TODO: Spawn explosion particles
    
    game_log("Explosion at (%.1f, %.1f, %.1f) radius=%.1f damage=%d",
             position.x, position.y, position.z, radius, damage);
}

/**
 * Player fires their weapon
 */
void player_shoot(void)
{
    if (!g_player.active || g_player.energy < 10) return;
    
    // Use energy
    int energy_cost = 10;
    if (g_player.current_weapon == WEAPON_ROCKET_LAUNCHER) {
        energy_cost = 25;
    } else if (g_player.current_weapon == WEAPON_PLASMA_CANNON) {
        energy_cost = 30;
    }
    
    if (g_player.energy < energy_cost) return;
    g_player.energy -= energy_cost;
    
    // Calculate shooting direction
    float cos_x = cosf(g_player.rotation.x);
    float sin_x = sinf(g_player.rotation.x);
    float cos_y = cosf(g_player.rotation.y);
    float sin_y = sinf(g_player.rotation.y);
    
    Vector3D direction = {
        sin_y * cos_x,
        -sin_x,
        cos_y * cos_x
    };
    
    // Auto-aim assistance
    if (g_auto_aim_enabled) {
        int closest_enemy = -1;
        float closest_distance = 20.0f;
        float closest_angle = 30.0f * M_PI / 180.0f;  // 30 degree cone
        
        for (int i = 0; i < MAX_ENEMIES; i++) {
            if (!g_enemies[i].active) continue;
            
            Vector3D to_enemy = {
                g_enemies[i].position.x - g_player.position.x,
                g_enemies[i].position.y - g_player.position.y,
                g_enemies[i].position.z - g_player.position.z
            };
            
            float distance = sqrtf(to_enemy.x * to_enemy.x + 
                                  to_enemy.y * to_enemy.y + 
                                  to_enemy.z * to_enemy.z);
            
            if (distance < closest_distance) {
                // Normalize
                to_enemy.x /= distance;
                to_enemy.y /= distance;
                to_enemy.z /= distance;
                
                // Check angle
                float dot = direction.x * to_enemy.x + 
                           direction.y * to_enemy.y + 
                           direction.z * to_enemy.z;
                float angle = acosf(fmaxf(-1.0f, fminf(1.0f, dot)));
                
                if (angle < closest_angle) {
                    closest_enemy = i;
                    closest_distance = distance;
                }
            }
        }
        
        if (closest_enemy >= 0) {
            // Adjust aim toward enemy
            Vector3D to_enemy = {
                g_enemies[closest_enemy].position.x - g_player.position.x,
                g_enemies[closest_enemy].position.y - g_player.position.y,
                g_enemies[closest_enemy].position.z - g_player.position.z
            };
            
            float distance = sqrtf(to_enemy.x * to_enemy.x + 
                                  to_enemy.y * to_enemy.y + 
                                  to_enemy.z * to_enemy.z);
            
            direction.x = to_enemy.x / distance;
            direction.y = to_enemy.y / distance;
            direction.z = to_enemy.z / distance;
        }
    }
    
    // Spawn projectile(s) based on weapon
    Vector3D spawn_pos = g_player.position;
    spawn_pos.y += 1.0f;  // Eye level
    
    int base_damage = 15 + g_player.weapon_levels[g_player.current_weapon] * 5;
    base_damage = (int)(base_damage * g_player.damage_multiplier);
    
    switch (g_player.current_weapon) {
        case WEAPON_BLASTER:
            spawn_projectile(spawn_pos,
                           (Vector3D){direction.x * PROJECTILE_SPEED,
                                     direction.y * PROJECTILE_SPEED,
                                     direction.z * PROJECTILE_SPEED},
                           base_damage, 0, -1);
            break;
            
        case WEAPON_SHOTGUN:
            // Multiple pellets with spread
            for (int i = 0; i < 5; i++) {
                Vector3D spread_dir = direction;
                spread_dir.x += random_range(-0.1f, 0.1f);
                spread_dir.y += random_range(-0.1f, 0.1f);
                spread_dir.z += random_range(-0.1f, 0.1f);
                
                // Normalize
                float mag = sqrtf(spread_dir.x * spread_dir.x + 
                                 spread_dir.y * spread_dir.y + 
                                 spread_dir.z * spread_dir.z);
                spread_dir.x /= mag;
                spread_dir.y /= mag;
                spread_dir.z /= mag;
                
                spawn_projectile(spawn_pos,
                               (Vector3D){spread_dir.x * PROJECTILE_SPEED,
                                         spread_dir.y * PROJECTILE_SPEED,
                                         spread_dir.z * PROJECTILE_SPEED},
                               base_damage / 2, 0, -1);
            }
            break;
            
        case WEAPON_ROCKET_LAUNCHER:
            {
                Projectile* rocket = spawn_projectile(spawn_pos,
                                                    (Vector3D){direction.x * PROJECTILE_SPEED * 0.6f,
                                                              direction.y * PROJECTILE_SPEED * 0.6f,
                                                              direction.z * PROJECTILE_SPEED * 0.6f},
                                                    base_damage * 2, 0, -1);
                if (rocket) {
                    rocket->splash_radius = 4.0f;
                    rocket->gravity_multiplier = 0.2f;
                    rocket->color = RGB(255, 200, 0);
                }
            }
            break;
            
        case WEAPON_PLASMA_CANNON:
            spawn_projectile(spawn_pos,
                           (Vector3D){direction.x * PROJECTILE_SPEED * 1.5f,
                                     direction.y * PROJECTILE_SPEED * 1.5f,
                                     direction.z * PROJECTILE_SPEED * 1.5f},
                           base_damage * 3, 0, -1);
            break;
    }
    
    g_stats.shots_fired++;
}

// ========================================================================
// POWER-UP SYSTEM
// ========================================================================

/**
 * Spawns a power-up
 * @param type Power-up type
 * @param position Spawn position
 * @return Power-up index or -1
 */
int spawn_power_up(PowerUpType type, Vector3D position)
{
    for (int i = 0; i < MAX_POWER_UPS; i++) {
        if (!g_power_ups[i].active) {
            PowerUp* powerup = &g_power_ups[i];
            
            powerup->position = position;
            powerup->position.y += 1.0f;  // Float above ground
            powerup->rotation = (Vector3D){0, 0, 0};
            powerup->type = type;
            powerup->lifetime = 30.0f;  // 30 seconds before despawn
            powerup->bob_offset = 0.0f;
            powerup->spin_speed = 2.0f;
            powerup->pulse_scale = 1.0f;
            powerup->active = TRUE;
            
            // Set type-specific properties
            switch (type) {
                case POWERUP_HEALTH:
                    powerup->value = 25;
                    powerup->duration = 0.0f;
                    powerup->color = RGB(255, 100, 100);
                    powerup->glow_color = RGB(255, 200, 200);
                    break;
                    
                case POWERUP_ENERGY:
                    powerup->value = 50;
                    powerup->duration = 0.0f;
                    powerup->color = RGB(100, 100, 255);
                    powerup->glow_color = RGB(200, 200, 255);
                    break;
                    
                case POWERUP_WEAPON_UPGRADE:
                    powerup->value = 1;
                    powerup->duration = 0.0f;
                    powerup->color = RGB(255, 255, 100);
                    powerup->glow_color = RGB(255, 255, 200);
                    break;
                    
                case POWERUP_SHIELD:
                    powerup->value = 50;
                    powerup->duration = 15.0f;
                    powerup->color = RGB(100, 255, 255);
                    powerup->glow_color = RGB(200, 255, 255);
                    break;
                    
                case POWERUP_SPEED_BOOST:
                    powerup->value = 2;  // 2x speed
                    powerup->duration = 20.0f;
                    powerup->color = RGB(255, 100, 255);
                    powerup->glow_color = RGB(255, 200, 255);
                    break;
                    
                case POWERUP_EXTRA_LIFE:
                    powerup->value = 1;
                    powerup->duration = 0.0f;
                    powerup->color = RGB(255, 255, 255);
                    powerup->glow_color = RGB(255, 255, 255);
                    break;
                    
                case POWERUP_INVULNERABILITY:
                    powerup->value = 10;  // 10 seconds
                    powerup->duration = 10.0f;
                    powerup->color = RGB(255, 200, 100);
                    powerup->glow_color = RGB(255, 255, 200);
                    break;
                    
                case POWERUP_RAPID_FIRE:
                    powerup->value = 3;  // 3x fire rate
                    powerup->duration = 15.0f;
                    powerup->color = RGB(200, 255, 100);
                    powerup->glow_color = RGB(255, 255, 200);
                    break;
            }
            
            game_log("Spawned power-up: type=%d at (%.1f, %.1f, %.1f)",
                     type, position.x, position.y, position.z);
            
            return i;
        }
    }
    
    return -1;
}

/**
 * Updates all active power-ups
 * @param delta_time Time since last update
 */
void update_power_ups(float delta_time)
{
    for (int i = 0; i < MAX_POWER_UPS; i++) {
        if (!g_power_ups[i].active) continue;
        
        PowerUp* powerup = &g_power_ups[i];
        
        // Update animation
        powerup->rotation.y += powerup->spin_speed * delta_time;
        powerup->bob_offset = sinf(g_game_time * 3.0f) * 0.2f;
        powerup->pulse_scale = 1.0f + sinf(g_game_time * 5.0f) * 0.1f;
        
        // Update lifetime
        powerup->lifetime -= delta_time;
        if (powerup->lifetime <= 0.0f) {
            powerup->active = FALSE;
            continue;
        }
        
        // Check player collision
        float distance = distance_3d(&powerup->position, &g_player.position);
        
        if (distance < 1.5f) {
            // Player collected power-up
            apply_power_up(powerup);
            powerup->active = FALSE;
            g_stats.power_ups_collected++;
        }
    }
}

/**
 * Applies a power-up effect to the player
 * @param powerup Power-up to apply
 */
void apply_power_up(PowerUp* powerup)
{
    switch (powerup->type) {
        case POWERUP_HEALTH:
            heal_player(powerup->value);
            break;
            
        case POWERUP_ENERGY:
            restore_energy(powerup->value);
            break;
            
        case POWERUP_WEAPON_UPGRADE:
            if (g_player.weapon_levels[g_player.current_weapon] < PLAYER_MAX_WEAPON_LEVEL) {
                g_player.weapon_levels[g_player.current_weapon]++;
                game_log("Weapon upgraded to level %d", 
                         g_player.weapon_levels[g_player.current_weapon]);
            }
            break;
            
        case POWERUP_SHIELD:
            add_shield(powerup->value);
            g_player.shield_time = powerup->duration;
            break;
            
        case POWERUP_SPEED_BOOST:
            g_player.speed_multiplier = (float)powerup->value;
            g_player.speed_boost_time = powerup->duration;
            break;
            
        case POWERUP_EXTRA_LIFE:
            g_player.lives += powerup->value;
            game_log("Extra life! Lives: %d", g_player.lives);
            break;
            
        case POWERUP_INVULNERABILITY:
            g_player.invulnerable_time = (float)powerup->value;
            break;
            
        case POWERUP_RAPID_FIRE:
            g_player.fire_rate_multiplier = (float)powerup->value;
            g_player.rapid_fire_time = powerup->duration;
            break;
    }
    
    g_player.score += 50;  // Bonus points for collecting power-up
}

// ========================================================================
// LEVEL MANAGEMENT
// ========================================================================

/**
 * Starts a new level
 * @param level_number Level to start
 */
void start_level(int level_number)
{
    game_log("Starting level %d", level_number);
    
    g_current_level = level_number;
    g_level_time = 0.0f;
    g_enemies_remaining = 0;
    g_total_enemies_spawned = 0;
    g_current_wave = 0;
    
    // Clear existing entities
    for (int i = 0; i < MAX_ENEMIES; i++) {
        g_enemies[i].active = FALSE;
    }
    
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        g_projectiles[i].active = FALSE;
    }
    
    for (int i = 0; i < MAX_POWER_UPS; i++) {
        g_power_ups[i].active = FALSE;
    }
    
    // Setup level data
    g_current_level_data.level_number = level_number;
    sprintf(g_current_level_data.level_name, "Level %d", level_number);
    g_current_level_data.time_limit = 0.0f;  // No time limit
    g_current_level_data.enemy_count = 10 + level_number * 5;
    g_current_level_data.waves = 1 + (level_number / 3);
    g_current_level_data.difficulty_modifier = 1.0f + (level_number - 1) * 0.15f;
    g_current_level_data.has_boss = (level_number % 5 == 0);
    
    g_difficulty_multiplier = g_current_level_data.difficulty_modifier;
    
    // Spawn initial wave
    spawn_enemy_wave(0);
    
    // Boss level
    if (g_current_level_data.has_boss) {
        Vector3D boss_pos = {0.0f, 0.0f, 20.0f};
        spawn_enemy(ENEMY_TYPE_BOSS, boss_pos);
    }
    
    g_current_game_state = GAME_STATE_PLAYING;
}

/**
 * Spawns a wave of enemies
 * @param wave_number Wave index
 */
void spawn_enemy_wave(int wave_number)
{
    int enemies_per_wave = g_current_level_data.enemy_count / g_current_level_data.waves;
    
    game_log("Spawning wave %d with %d enemies", wave_number + 1, enemies_per_wave);
    
    for (int i = 0; i < enemies_per_wave; i++) {
        // Random spawn position
        float angle = random_range(0, 2 * M_PI);
        float distance = random_range(20.0f, 40.0f);
        Vector3D spawn_pos = {
            cosf(angle) * distance,
            0.0f,
            sinf(angle) * distance
        };
        
        // Determine enemy type based on level
        EnemyType type = ENEMY_TYPE_GRUNT;
        int type_roll = rand() % 100;
        
        if (g_current_level >= 2 && type_roll < 30) {
            type = ENEMY_TYPE_SCOUT;
        } else if (g_current_level >= 4 && type_roll < 20) {
            type = ENEMY_TYPE_HEAVY;
        } else if (g_current_level >= 6 && type_roll < 15) {
            type = ENEMY_TYPE_SNIPER;
        } else if (g_current_level >= 8 && type_roll < 10) {
            type = ENEMY_TYPE_ELITE;
        } else if (g_current_level >= 10 && type_roll < 5) {
            type = ENEMY_TYPE_TURRET;
            spawn_pos.y = 0.0f;  // Turrets on ground
        }
        
        spawn_enemy(type, spawn_pos);
    }
}

/**
 * Updates level progression
 * @param delta_time Time since last update
 */
void update_level(float delta_time)
{
    g_level_time += delta_time;
    
    // Update combo timer
    if (g_stats.combo_timer > 0.0f) {
        g_stats.combo_timer -= delta_time;
        if (g_stats.combo_timer <= 0.0f) {
            g_stats.current_combo = 0;
        }
    }
    
    // Check for level completion
    if (g_enemies_remaining <= 0 && g_current_game_state == GAME_STATE_PLAYING) {
        if (g_current_wave < g_current_level_data.waves - 1) {
            // Spawn next wave
            g_current_wave++;
            spawn_enemy_wave(g_current_wave);
        } else {
            // Level complete
            g_current_game_state = GAME_STATE_LEVEL_COMPLETE;
            
            // Calculate bonuses
            int time_bonus = (int)fmaxf(0, 1000 - g_level_time * 10);
            int accuracy_bonus = 0;
            if (g_stats.shots_fired > 0) {
                float accuracy = (float)g_stats.shots_hit / g_stats.shots_fired;
                accuracy_bonus = (int)(accuracy * 500);
            }
            
            g_player.score += 1000;  // Base completion bonus
            g_player.score += time_bonus;
            g_player.score += accuracy_bonus;
            g_player.score += g_current_level * 500;  // Level bonus
            
            game_log("Level %d complete! Time: %.1fs, Accuracy: %d%%",
                     g_current_level, g_level_time,
                     g_stats.shots_fired > 0 ? 
                     (g_stats.shots_hit * 100 / g_stats.shots_fired) : 0);
        }
    }
}

// ========================================================================
// GAME STATE MANAGEMENT
// ========================================================================

/**
 * Updates the main game logic
 * @param delta_time Time since last update
 */
void update_game_logic(float delta_time)
{
    g_game_time += delta_time;
    g_stats.total_play_time += delta_time;
    
    switch (g_current_game_state) {
        case GAME_STATE_MENU:
            // Menu logic handled elsewhere
            break;
            
        case GAME_STATE_PLAYING:
            update_player(delta_time);
            update_enemies(delta_time);
            update_projectiles(delta_time);
            update_power_ups(delta_time);
            update_level(delta_time);
            break;
            
        case GAME_STATE_PAUSED:
            // Game paused - no updates
            break;
            
        case GAME_STATE_GAME_OVER:
            // Handle high score submission
            if (check_high_score(g_player.score, g_current_level, 
                                (DWORD)g_stats.total_play_time)) {
                // High score achieved!
                game_log("New high score: %d", g_player.score);
            }
            break;
            
        case GAME_STATE_LEVEL_COMPLETE:
            // Wait for input to continue
            break;
            
        case GAME_STATE_CUTSCENE:
            // Cutscene logic
            break;
            
        case GAME_STATE_LOADING:
            // Loading screen
            break;
            
        case GAME_STATE_VICTORY:
            // Game completed!
            break;
    }
}

/**
 * Starts a new game
 */
void start_new_game(void)
{
    game_log("Starting new game");
    
    // Reset stats
    memset(&g_stats, 0, sizeof(g_stats));
    
    // Initialize player
    initialize_player();
    
    // Start first level
    g_current_level = 1;
    g_game_time = 0.0f;
    start_level(1);
}

/**
 * Pauses or unpauses the game
 */
void pause_game(void)
{
    if (g_current_game_state == GAME_STATE_PLAYING) {
        g_previous_game_state = g_current_game_state;
        g_current_game_state = GAME_STATE_PAUSED;
        game_log("Game paused");
    } else if (g_current_game_state == GAME_STATE_PAUSED) {
        g_current_game_state = g_previous_game_state;
        game_log("Game resumed");
    }
}

/**
 * Advances to the next level
 */
void next_level(void)
{
    if (g_current_game_state == GAME_STATE_LEVEL_COMPLETE) {
        start_level(g_current_level + 1);
    }
}

/**
 * Gets the current game state
 * @return Current game state
 */
GameState get_game_state(void)
{
    return g_current_game_state;
}

/**
 * Gets player data
 * @return Pointer to player structure
 */
Player* get_player(void)
{
    return &g_player;
}

// ========================================================================
// ACHIEVEMENT SYSTEM
// ========================================================================

/**
 * Initializes achievements
 */
void initialize_achievements(void)
{
    Achievement* ach;
    
    // First Kill
    ach = &g_achievements[ACHIEVEMENT_FIRST_KILL];
    ach->type = ACHIEVEMENT_FIRST_KILL;
    strcpy(ach->name, "First Blood");
    strcpy(ach->description, "Get your first kill");
    ach->target = 1;
    
    // Headshot Master
    ach = &g_achievements[ACHIEVEMENT_HEADSHOT_MASTER];
    ach->type = ACHIEVEMENT_HEADSHOT_MASTER;
    strcpy(ach->name, "Sharpshooter");
    strcpy(ach->description, "Get 100 headshots");
    ach->target = 100;
    
    // Add more achievements...
}

/**
 * Checks achievements after a kill
 * @param enemy_type Type of enemy killed
 */
void check_kill_achievements(EnemyType enemy_type)
{
    // First kill
    if (!g_achievements[ACHIEVEMENT_FIRST_KILL].unlocked && g_player.kills >= 1) {
        unlock_achievement(ACHIEVEMENT_FIRST_KILL);
    }
    
    // Boss slayer
    if (enemy_type == ENEMY_TYPE_BOSS && !g_achievements[ACHIEVEMENT_BOSS_SLAYER].unlocked) {
        unlock_achievement(ACHIEVEMENT_BOSS_SLAYER);
    }
}

/**
 * Unlocks an achievement
 * @param type Achievement type
 */
void unlock_achievement(AchievementType type)
{
    Achievement* ach = &g_achievements[type];
    if (!ach->unlocked) {
        ach->unlocked = TRUE;
        ach->unlock_time = GetTickCount();
        g_player.score += 1000;  // Achievement bonus
        
        game_log("Achievement unlocked: %s", ach->name);
        
        // TODO: Display achievement notification
    }
}

// ========================================================================
// DEBUG AND CONFIGURATION
// ========================================================================

/**
 * Toggles god mode
 */
void toggle_god_mode(void)
{
    g_god_mode = !g_god_mode;
    game_log("God mode: %s", g_god_mode ? "ON" : "OFF");
}

/**
 * Toggles auto-aim
 */
void toggle_auto_aim(void)
{
    g_auto_aim_enabled = !g_auto_aim_enabled;
    game_log("Auto-aim: %s", g_auto_aim_enabled ? "ON" : "OFF");
}

/**
 * Sets game difficulty
 * @param multiplier Difficulty multiplier
 */
void set_difficulty(float multiplier)
{
    g_difficulty_multiplier = fmaxf(0.5f, fminf(3.0f, multiplier));
    game_log("Difficulty set to %.1fx", g_difficulty_multiplier);
}

/**
 * Gets active enemy count
 * @return Number of active enemies
 */
int get_active_enemy_count(void)
{
    return g_enemies_remaining;
}

/**
 * Gets active projectile count
 * @return Number of active projectiles
 */
int get_active_projectile_count(void)
{
    int count = 0;
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        if (g_projectiles[i].active) count++;
    }
    return count;
}

/**
 * Gets game statistics string
 * @param buffer Output buffer
 * @param size Buffer size
 */
void get_game_stats(char* buffer, size_t size)
{
    float accuracy = 0.0f;
    if (g_stats.shots_fired > 0) {
        accuracy = (float)g_stats.shots_hit / g_stats.shots_fired * 100.0f;
    }
    
    snprintf(buffer, size,
        "Game Statistics:\n"
        "  Level: %d\n"
        "  Score: %d\n"
        "  Kills: %d\n"
        "  Deaths: %d\n"
        "  Accuracy: %.1f%%\n"
        "  Combo: x%d (best: x%d)\n"
        "  Power-ups: %d\n"
        "  Play Time: %.1f min\n"
        "  Enemies: %d\n"
        "  Projectiles: %d\n",
        g_current_level,
        g_player.score,
        g_player.kills,
        g_player.deaths,
        accuracy,
        g_stats.current_combo,
        g_stats.highest_combo,
        g_stats.power_ups_collected,
        g_stats.total_play_time / 60.0f,
        get_active_enemy_count(),
        get_active_projectile_count()
    );
}
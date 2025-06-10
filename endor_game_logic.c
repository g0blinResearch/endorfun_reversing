/**
 * ========================================================================
 * ENDOR GAME LOGIC AND AI SYSTEM
 * ========================================================================
 * 
 * Core gameplay mechanics, AI behaviors, and game state management
 * for the Endor game engine. Includes enemy AI, player mechanics,
 * power-ups, level progression, and game rules.
 */

#include "endor_readable.h"
#include <math.h>
#include <time.h>

// ========================================================================
// GAME CONSTANTS AND STATE
// ========================================================================

#define MAX_ENEMIES 64
#define MAX_PROJECTILES 128
#define MAX_POWER_UPS 32
#define MAX_LEVEL_OBJECTS 256
#define PLAYER_MAX_HEALTH 100
#define PLAYER_MAX_ENERGY 100
#define AI_UPDATE_INTERVAL 0.1f
#define PROJECTILE_SPEED 500.0f

// Game state enumeration
typedef enum {
    GAME_STATE_MENU,
    GAME_STATE_PLAYING,
    GAME_STATE_PAUSED,
    GAME_STATE_GAME_OVER,
    GAME_STATE_LEVEL_COMPLETE,
    GAME_STATE_CUTSCENE
} GameState;

// Enemy types
typedef enum {
    ENEMY_TYPE_GRUNT,
    ENEMY_TYPE_SCOUT,
    ENEMY_TYPE_HEAVY,
    ENEMY_TYPE_BOSS,
    ENEMY_TYPE_TURRET
} EnemyType;

// AI states
typedef enum {
    AI_STATE_IDLE,
    AI_STATE_PATROL,
    AI_STATE_SEARCH,
    AI_STATE_CHASE,
    AI_STATE_ATTACK,
    AI_STATE_RETREAT,
    AI_STATE_DEAD
} AIState;

// Power-up types
typedef enum {
    POWERUP_HEALTH,
    POWERUP_ENERGY,
    POWERUP_WEAPON_UPGRADE,
    POWERUP_SHIELD,
    POWERUP_SPEED_BOOST,
    POWERUP_EXTRA_LIFE
} PowerUpType;

// Player structure
typedef struct {
    float x, y, z;
    float velocity_x, velocity_y, velocity_z;
    float rotation_x, rotation_y, rotation_z;
    int health;
    int max_health;
    int energy;
    int max_energy;
    int lives;
    int score;
    int weapon_level;
    float speed_multiplier;
    float shield_time;
    int invulnerable_time;
    int active;
} Player;

// Enemy structure
typedef struct {
    float x, y, z;
    float velocity_x, velocity_y, velocity_z;
    float rotation_y;
    EnemyType type;
    AIState ai_state;
    int health;
    int max_health;
    float ai_timer;
    float target_x, target_y, target_z;
    float patrol_start_x, patrol_start_z;
    float patrol_end_x, patrol_end_z;
    float detection_range;
    float attack_range;
    float last_attack_time;
    float attack_cooldown;
    int active;
    int points_value;
} Enemy;

// Projectile structure
typedef struct {
    float x, y, z;
    float velocity_x, velocity_y, velocity_z;
    float rotation_x, rotation_y, rotation_z;
    int damage;
    float lifetime;
    int owner_type; // 0 = player, 1 = enemy
    int active;
    COLORREF color;
} Projectile;

// Power-up structure
typedef struct {
    float x, y, z;
    PowerUpType type;
    float rotation_y;
    float bob_offset;
    float duration;
    int value;
    int active;
    COLORREF color;
} PowerUp;

// Global game state
static GameState current_game_state = GAME_STATE_MENU;
static Player player;
static Enemy enemies[MAX_ENEMIES];
static Projectile projectiles[MAX_PROJECTILES];
static PowerUp power_ups[MAX_POWER_UPS];
static int current_level = 1;
static float game_time = 0.0f;
static float level_time = 0.0f;
static int enemies_remaining = 0;
static int total_enemies_spawned = 0;

// Game configuration
static float difficulty_multiplier = 1.0f;
static int auto_aim_enabled = 0;
static float sound_volume = 1.0f;
static int god_mode = 0;

// ========================================================================
// PLAYER MANAGEMENT
// ========================================================================

void initialize_player() {
    player.x = 0.0f;
    player.y = 0.0f;
    player.z = 0.0f;
    player.velocity_x = 0.0f;
    player.velocity_y = 0.0f;
    player.velocity_z = 0.0f;
    player.rotation_x = 0.0f;
    player.rotation_y = 0.0f;
    player.rotation_z = 0.0f;
    player.health = PLAYER_MAX_HEALTH;
    player.max_health = PLAYER_MAX_HEALTH;
    player.energy = PLAYER_MAX_ENERGY;
    player.max_energy = PLAYER_MAX_ENERGY;
    player.lives = 3;
    player.score = 0;
    player.weapon_level = 1;
    player.speed_multiplier = 1.0f;
    player.shield_time = 0.0f;
    player.invulnerable_time = 0;
    player.active = 1;
}

void update_player(float delta_time) {
    if (!player.active) return;
    
    // Update position
    player.x += player.velocity_x * delta_time;
    player.y += player.velocity_y * delta_time;
    player.z += player.velocity_z * delta_time;
    
    // Apply friction
    player.velocity_x *= 0.9f;
    player.velocity_y *= 0.9f;
    player.velocity_z *= 0.9f;
    
    // Update timers
    if (player.invulnerable_time > 0) {
        player.invulnerable_time--;
    }
    
    if (player.shield_time > 0.0f) {
        player.shield_time -= delta_time;
        if (player.shield_time <= 0.0f) {
            player.shield_time = 0.0f;
        }
    }
    
    // Regenerate energy slowly
    if (player.energy < player.max_energy) {
        player.energy = fmin(player.max_energy, player.energy + (int)(20.0f * delta_time));
    }
    
    // Keep player in bounds (assuming level boundaries)
    player.x = fmax(-50.0f, fmin(50.0f, player.x));
    player.z = fmax(-50.0f, fmin(50.0f, player.z));
    player.y = fmax(0.0f, fmin(10.0f, player.y));
}

void move_player(float forward, float strafe, float up) {
    if (!player.active) return;
    
    float speed = 10.0f * player.speed_multiplier;
    
    // Calculate movement based on rotation
    float cos_y = cos(player.rotation_y);
    float sin_y = sin(player.rotation_y);
    
    player.velocity_x += (forward * sin_y + strafe * cos_y) * speed * 0.1f;
    player.velocity_z += (forward * cos_y - strafe * sin_y) * speed * 0.1f;
    player.velocity_y += up * speed * 0.1f;
}

void rotate_player(float yaw, float pitch) {
    if (!player.active) return;
    
    player.rotation_y += yaw;
    player.rotation_x += pitch;
    
    // Normalize angles
    while (player.rotation_y > M_PI) player.rotation_y -= 2.0f * M_PI;
    while (player.rotation_y < -M_PI) player.rotation_y += 2.0f * M_PI;
    
    player.rotation_x = fmax(-M_PI/2.0f, fmin(M_PI/2.0f, player.rotation_x));
}

int damage_player(int damage) {
    if (!player.active || player.invulnerable_time > 0 || player.shield_time > 0.0f || god_mode) {
        return 0;
    }
    
    player.health -= damage;
    player.invulnerable_time = 60; // 1 second at 60 FPS
    
    if (player.health <= 0) {
        player.health = 0;
        player.lives--;
        
        if (player.lives <= 0) {
            player.active = 0;
            current_game_state = GAME_STATE_GAME_OVER;
        } else {
            // Respawn
            player.health = player.max_health;
            player.x = 0.0f;
            player.y = 0.0f;
            player.z = 0.0f;
            player.invulnerable_time = 180; // 3 seconds invulnerability
        }
    }
    
    return 1;
}

void heal_player(int amount) {
    player.health = fmin(player.max_health, player.health + amount);
}

void restore_energy(int amount) {
    player.energy = fmin(player.max_energy, player.energy + amount);
}

// ========================================================================
// ENEMY MANAGEMENT
// ========================================================================

int spawn_enemy(EnemyType type, float x, float y, float z) {
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (!enemies[i].active) {
            Enemy* enemy = &enemies[i];
            
            enemy->x = x;
            enemy->y = y;
            enemy->z = z;
            enemy->velocity_x = 0.0f;
            enemy->velocity_y = 0.0f;
            enemy->velocity_z = 0.0f;
            enemy->rotation_y = 0.0f;
            enemy->type = type;
            enemy->ai_state = AI_STATE_PATROL;
            enemy->ai_timer = 0.0f;
            enemy->target_x = x;
            enemy->target_y = y;
            enemy->target_z = z;
            enemy->patrol_start_x = x - 5.0f;
            enemy->patrol_start_z = z;
            enemy->patrol_end_x = x + 5.0f;
            enemy->patrol_end_z = z;
            enemy->last_attack_time = 0.0f;
            enemy->active = 1;
            
            // Set type-specific properties
            switch (type) {
                case ENEMY_TYPE_GRUNT:
                    enemy->health = 20;
                    enemy->max_health = 20;
                    enemy->detection_range = 8.0f;
                    enemy->attack_range = 2.0f;
                    enemy->attack_cooldown = 1.5f;
                    enemy->points_value = 100;
                    break;
                    
                case ENEMY_TYPE_SCOUT:
                    enemy->health = 10;
                    enemy->max_health = 10;
                    enemy->detection_range = 12.0f;
                    enemy->attack_range = 6.0f;
                    enemy->attack_cooldown = 0.8f;
                    enemy->points_value = 150;
                    break;
                    
                case ENEMY_TYPE_HEAVY:
                    enemy->health = 50;
                    enemy->max_health = 50;
                    enemy->detection_range = 6.0f;
                    enemy->attack_range = 3.0f;
                    enemy->attack_cooldown = 2.0f;
                    enemy->points_value = 300;
                    break;
                    
                case ENEMY_TYPE_BOSS:
                    enemy->health = 200;
                    enemy->max_health = 200;
                    enemy->detection_range = 15.0f;
                    enemy->attack_range = 10.0f;
                    enemy->attack_cooldown = 0.5f;
                    enemy->points_value = 1000;
                    break;
                    
                case ENEMY_TYPE_TURRET:
                    enemy->health = 30;
                    enemy->max_health = 30;
                    enemy->detection_range = 20.0f;
                    enemy->attack_range = 15.0f;
                    enemy->attack_cooldown = 1.0f;
                    enemy->points_value = 200;
                    enemy->ai_state = AI_STATE_IDLE;
                    break;
            }
            
            enemy->health = (int)(enemy->health * difficulty_multiplier);
            enemy->max_health = enemy->health;
            
            enemies_remaining++;
            total_enemies_spawned++;
            
            return i;
        }
    }
    
    return -1; // No free slot
}

void update_enemy_ai(Enemy* enemy, float delta_time) {
    if (!enemy->active || enemy->ai_state == AI_STATE_DEAD) {
        return;
    }
    
    enemy->ai_timer += delta_time;
    
    // Calculate distance to player
    float dx = player.x - enemy->x;
    float dy = player.y - enemy->y;
    float dz = player.z - enemy->z;
    float distance_to_player = sqrt(dx*dx + dy*dy + dz*dz);
    
    // State machine
    switch (enemy->ai_state) {
        case AI_STATE_IDLE:
            if (distance_to_player <= enemy->detection_range) {
                enemy->ai_state = AI_STATE_CHASE;
                enemy->ai_timer = 0.0f;
            }
            break;
            
        case AI_STATE_PATROL:
            if (distance_to_player <= enemy->detection_range) {
                enemy->ai_state = AI_STATE_CHASE;
                enemy->ai_timer = 0.0f;
            } else {
                // Move between patrol points
                float patrol_dx = enemy->patrol_end_x - enemy->patrol_start_x;
                float patrol_t = (sin(enemy->ai_timer) + 1.0f) * 0.5f;
                enemy->target_x = enemy->patrol_start_x + patrol_dx * patrol_t;
                enemy->target_z = enemy->patrol_start_z;
            }
            break;
            
        case AI_STATE_CHASE:
            if (distance_to_player > enemy->detection_range * 1.5f) {
                enemy->ai_state = AI_STATE_PATROL;
                enemy->ai_timer = 0.0f;
            } else if (distance_to_player <= enemy->attack_range) {
                enemy->ai_state = AI_STATE_ATTACK;
                enemy->ai_timer = 0.0f;
            } else {
                // Move toward player
                enemy->target_x = player.x;
                enemy->target_y = player.y;
                enemy->target_z = player.z;
            }
            break;
            
        case AI_STATE_ATTACK:
            if (distance_to_player > enemy->attack_range * 1.2f) {
                enemy->ai_state = AI_STATE_CHASE;
                enemy->ai_timer = 0.0f;
            } else {
                // Attack player
                if (game_time - enemy->last_attack_time >= enemy->attack_cooldown) {
                    enemy_attack(enemy);
                    enemy->last_attack_time = game_time;
                }
            }
            break;
            
        case AI_STATE_RETREAT:
            // Move away from player
            if (distance_to_player > enemy->detection_range) {
                enemy->ai_state = AI_STATE_PATROL;
            } else {
                enemy->target_x = enemy->x - dx;
                enemy->target_z = enemy->z - dz;
            }
            break;
    }
    
    // Move toward target
    if (enemy->type != ENEMY_TYPE_TURRET) {
        float target_dx = enemy->target_x - enemy->x;
        float target_dz = enemy->target_z - enemy->z;
        float target_distance = sqrt(target_dx*target_dx + target_dz*target_dz);
        
        if (target_distance > 0.1f) {
            float speed = 3.0f;
            switch (enemy->type) {
                case ENEMY_TYPE_SCOUT: speed = 5.0f; break;
                case ENEMY_TYPE_HEAVY: speed = 1.5f; break;
                case ENEMY_TYPE_BOSS: speed = 2.0f; break;
            }
            
            enemy->velocity_x = (target_dx / target_distance) * speed;
            enemy->velocity_z = (target_dz / target_distance) * speed;
            
            // Update rotation to face movement direction
            enemy->rotation_y = atan2(target_dx, target_dz);
        }
    } else {
        // Turrets don't move but rotate to face player
        if (distance_to_player <= enemy->detection_range) {
            enemy->rotation_y = atan2(dx, dz);
        }
    }
}

void update_enemies(float delta_time) {
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (!enemies[i].active) continue;
        
        Enemy* enemy = &enemies[i];
        
        // Update AI
        update_enemy_ai(enemy, delta_time);
        
        // Update position
        enemy->x += enemy->velocity_x * delta_time;
        enemy->y += enemy->velocity_y * delta_time;
        enemy->z += enemy->velocity_z * delta_time;
        
        // Apply friction
        enemy->velocity_x *= 0.8f;
        enemy->velocity_y *= 0.8f;
        enemy->velocity_z *= 0.8f;
        
        // Keep enemies in bounds
        enemy->x = fmax(-60.0f, fmin(60.0f, enemy->x));
        enemy->z = fmax(-60.0f, fmin(60.0f, enemy->z));
        enemy->y = fmax(0.0f, fmin(10.0f, enemy->y));
    }
}

int damage_enemy(int enemy_index, int damage) {
    if (enemy_index < 0 || enemy_index >= MAX_ENEMIES || !enemies[enemy_index].active) {
        return 0;
    }
    
    Enemy* enemy = &enemies[enemy_index];
    enemy->health -= damage;
    
    if (enemy->health <= 0) {
        // Enemy killed
        player.score += enemy->points_value;
        enemies_remaining--;
        enemy->active = 0;
        enemy->ai_state = AI_STATE_DEAD;
        
        // Chance to drop power-up
        if (rand() % 100 < 20) { // 20% chance
            spawn_power_up((PowerUpType)(rand() % 6), enemy->x, enemy->y + 1.0f, enemy->z);
        }
        
        return 1;
    }
    
    return 0;
}

void enemy_attack(Enemy* enemy) {
    // Create projectile toward player
    float dx = player.x - enemy->x;
    float dy = player.y - enemy->y;
    float dz = player.z - enemy->z;
    float distance = sqrt(dx*dx + dy*dy + dz*dz);
    
    if (distance > 0.1f) {
        dx /= distance;
        dy /= distance;
        dz /= distance;
        
        int damage = 10;
        switch (enemy->type) {
            case ENEMY_TYPE_HEAVY: damage = 20; break;
            case ENEMY_TYPE_BOSS: damage = 30; break;
        }
        
        spawn_projectile(enemy->x, enemy->y + 1.0f, enemy->z,
                        dx * PROJECTILE_SPEED, dy * PROJECTILE_SPEED, dz * PROJECTILE_SPEED,
                        damage, 1, RGB(255, 100, 100));
    }
}

// ========================================================================
// PROJECTILE SYSTEM
// ========================================================================

int spawn_projectile(float x, float y, float z, float vel_x, float vel_y, float vel_z, 
                    int damage, int owner_type, COLORREF color) {
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        if (!projectiles[i].active) {
            Projectile* proj = &projectiles[i];
            
            proj->x = x;
            proj->y = y;
            proj->z = z;
            proj->velocity_x = vel_x;
            proj->velocity_y = vel_y;
            proj->velocity_z = vel_z;
            proj->rotation_x = 0.0f;
            proj->rotation_y = atan2(vel_x, vel_z);
            proj->rotation_z = 0.0f;
            proj->damage = damage;
            proj->lifetime = 5.0f; // 5 seconds
            proj->owner_type = owner_type;
            proj->active = 1;
            proj->color = color;
            
            return i;
        }
    }
    
    return -1;
}

void update_projectiles(float delta_time) {
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        if (!projectiles[i].active) continue;
        
        Projectile* proj = &projectiles[i];
        
        // Update position
        proj->x += proj->velocity_x * delta_time;
        proj->y += proj->velocity_y * delta_time;
        proj->z += proj->velocity_z * delta_time;
        
        // Update lifetime
        proj->lifetime -= delta_time;
        if (proj->lifetime <= 0.0f) {
            proj->active = 0;
            continue;
        }
        
        // Check bounds
        if (proj->x < -70.0f || proj->x > 70.0f ||
            proj->y < -10.0f || proj->y > 20.0f ||
            proj->z < -70.0f || proj->z > 70.0f) {
            proj->active = 0;
            continue;
        }
        
        // Check collisions
        if (proj->owner_type == 0) { // Player projectile
            // Check enemy collisions
            for (int j = 0; j < MAX_ENEMIES; j++) {
                if (!enemies[j].active) continue;
                
                float dx = proj->x - enemies[j].x;
                float dy = proj->y - enemies[j].y;
                float dz = proj->z - enemies[j].z;
                float distance = sqrt(dx*dx + dy*dy + dz*dz);
                
                if (distance < 1.0f) { // Hit
                    damage_enemy(j, proj->damage);
                    proj->active = 0;
                    break;
                }
            }
        } else { // Enemy projectile
            // Check player collision
            float dx = proj->x - player.x;
            float dy = proj->y - player.y;
            float dz = proj->z - player.z;
            float distance = sqrt(dx*dx + dy*dy + dz*dz);
            
            if (distance < 1.0f) { // Hit
                damage_player(proj->damage);
                proj->active = 0;
            }
        }
    }
}

void player_shoot() {
    if (!player.active || player.energy < 10) return;
    
    player.energy -= 10;
    
    // Calculate shooting direction
    float cos_x = cos(player.rotation_x);
    float sin_x = sin(player.rotation_x);
    float cos_y = cos(player.rotation_y);
    float sin_y = sin(player.rotation_y);
    
    float vel_x = sin_y * cos_x * PROJECTILE_SPEED;
    float vel_y = -sin_x * PROJECTILE_SPEED;
    float vel_z = cos_y * cos_x * PROJECTILE_SPEED;
    
    int damage = 15 + (player.weapon_level - 1) * 5;
    
    if (auto_aim_enabled) {
        // Find closest enemy and adjust aim
        float closest_distance = 20.0f;
        int closest_enemy = -1;
        
        for (int i = 0; i < MAX_ENEMIES; i++) {
            if (!enemies[i].active) continue;
            
            float dx = enemies[i].x - player.x;
            float dy = enemies[i].y - player.y;
            float dz = enemies[i].z - player.z;
            float distance = sqrt(dx*dx + dy*dy + dz*dz);
            
            if (distance < closest_distance) {
                closest_distance = distance;
                closest_enemy = i;
            }
        }
        
        if (closest_enemy >= 0) {
            float dx = enemies[closest_enemy].x - player.x;
            float dy = enemies[closest_enemy].y - player.y;
            float dz = enemies[closest_enemy].z - player.z;
            float distance = sqrt(dx*dx + dy*dy + dz*dz);
            
            vel_x = (dx / distance) * PROJECTILE_SPEED;
            vel_y = (dy / distance) * PROJECTILE_SPEED;
            vel_z = (dz / distance) * PROJECTILE_SPEED;
        }
    }
    
    spawn_projectile(player.x, player.y + 1.0f, player.z,
                    vel_x, vel_y, vel_z, damage, 0, RGB(100, 255, 100));
    
    // Multi-shot for higher weapon levels
    if (player.weapon_level >= 3) {
        float spread = 0.3f;
        spawn_projectile(player.x, player.y + 1.0f, player.z,
                        vel_x + spread, vel_y, vel_z, damage, 0, RGB(100, 255, 100));
        spawn_projectile(player.x, player.y + 1.0f, player.z,
                        vel_x - spread, vel_y, vel_z, damage, 0, RGB(100, 255, 100));
    }
}

// ========================================================================
// POWER-UP SYSTEM
// ========================================================================

int spawn_power_up(PowerUpType type, float x, float y, float z) {
    for (int i = 0; i < MAX_POWER_UPS; i++) {
        if (!power_ups[i].active) {
            PowerUp* powerup = &power_ups[i];
            
            powerup->x = x;
            powerup->y = y;
            powerup->z = z;
            powerup->type = type;
            powerup->rotation_y = 0.0f;
            powerup->bob_offset = 0.0f;
            powerup->duration = 30.0f; // 30 seconds before despawn
            powerup->active = 1;
            
            switch (type) {
                case POWERUP_HEALTH:
                    powerup->value = 25;
                    powerup->color = RGB(255, 100, 100);
                    break;
                case POWERUP_ENERGY:
                    powerup->value = 50;
                    powerup->color = RGB(100, 100, 255);
                    break;
                case POWERUP_WEAPON_UPGRADE:
                    powerup->value = 1;
                    powerup->color = RGB(255, 255, 100);
                    break;
                case POWERUP_SHIELD:
                    powerup->value = 10; // 10 seconds
                    powerup->color = RGB(100, 255, 255);
                    break;
                case POWERUP_SPEED_BOOST:
                    powerup->value = 15; // 15 seconds
                    powerup->color = RGB(255, 100, 255);
                    break;
                case POWERUP_EXTRA_LIFE:
                    powerup->value = 1;
                    powerup->color = RGB(255, 255, 255);
                    break;
            }
            
            return i;
        }
    }
    
    return -1;
}

void update_power_ups(float delta_time) {
    for (int i = 0; i < MAX_POWER_UPS; i++) {
        if (!power_ups[i].active) continue;
        
        PowerUp* powerup = &power_ups[i];
        
        // Update animation
        powerup->rotation_y += delta_time * 2.0f;
        powerup->bob_offset = sin(game_time * 3.0f) * 0.2f;
        
        // Update lifetime
        powerup->duration -= delta_time;
        if (powerup->duration <= 0.0f) {
            powerup->active = 0;
            continue;
        }
        
        // Check player collision
        float dx = powerup->x - player.x;
        float dy = (powerup->y + powerup->bob_offset) - player.y;
        float dz = powerup->z - player.z;
        float distance = sqrt(dx*dx + dy*dy + dz*dz);
        
        if (distance < 1.5f) {
            // Player collected power-up
            apply_power_up(powerup);
            powerup->active = 0;
        }
    }
}

void apply_power_up(PowerUp* powerup) {
    switch (powerup->type) {
        case POWERUP_HEALTH:
            heal_player(powerup->value);
            break;
            
        case POWERUP_ENERGY:
            restore_energy(powerup->value);
            break;
            
        case POWERUP_WEAPON_UPGRADE:
            if (player.weapon_level < 5) {
                player.weapon_level++;
            }
            break;
            
        case POWERUP_SHIELD:
            player.shield_time = (float)powerup->value;
            break;
            
        case POWERUP_SPEED_BOOST:
            player.speed_multiplier = 2.0f;
            // Note: This should be managed with a timer system
            break;
            
        case POWERUP_EXTRA_LIFE:
            player.lives += powerup->value;
            break;
    }
    
    player.score += 50;
}

// ========================================================================
// LEVEL MANAGEMENT
// ========================================================================

void start_level(int level_number) {
    current_level = level_number;
    level_time = 0.0f;
    enemies_remaining = 0;
    total_enemies_spawned = 0;
    
    // Clear existing entities
    for (int i = 0; i < MAX_ENEMIES; i++) {
        enemies[i].active = 0;
    }
    
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        projectiles[i].active = 0;
    }
    
    for (int i = 0; i < MAX_POWER_UPS; i++) {
        power_ups[i].active = 0;
    }
    
    // Spawn enemies based on level
    int enemy_count = 5 + level_number * 2;
    difficulty_multiplier = 1.0f + (level_number - 1) * 0.2f;
    
    for (int i = 0; i < enemy_count; i++) {
        float x = ((float)rand() / RAND_MAX - 0.5f) * 80.0f;
        float z = ((float)rand() / RAND_MAX - 0.5f) * 80.0f;
        
        EnemyType type = ENEMY_TYPE_GRUNT;
        int type_roll = rand() % 100;
        
        if (level_number >= 3 && type_roll < 20) {
            type = ENEMY_TYPE_SCOUT;
        } else if (level_number >= 5 && type_roll < 10) {
            type = ENEMY_TYPE_HEAVY;
        } else if (level_number >= 10 && type_roll < 5) {
            type = ENEMY_TYPE_TURRET;
        }
        
        spawn_enemy(type, x, 0.0f, z);
    }
    
    // Spawn boss on boss levels
    if (level_number % 5 == 0) {
        spawn_enemy(ENEMY_TYPE_BOSS, 0.0f, 0.0f, 20.0f);
    }
    
    current_game_state = GAME_STATE_PLAYING;
}

void update_level(float delta_time) {
    level_time += delta_time;
    
    // Check for level completion
    if (enemies_remaining <= 0 && current_game_state == GAME_STATE_PLAYING) {
        current_game_state = GAME_STATE_LEVEL_COMPLETE;
        player.score += 1000 + current_level * 500; // Level completion bonus
    }
    
    // Spawn additional enemies over time (wave system)
    static float last_spawn_time = 0.0f;
    if (level_time - last_spawn_time > 30.0f && enemies_remaining < 3) {
        float x = ((float)rand() / RAND_MAX - 0.5f) * 80.0f;
        float z = ((float)rand() / RAND_MAX - 0.5f) * 80.0f;
        spawn_enemy(ENEMY_TYPE_GRUNT, x, 0.0f, z);
        last_spawn_time = level_time;
    }
}

// ========================================================================
// GAME STATE MANAGEMENT
// ========================================================================

void update_game_logic(float delta_time) {
    game_time += delta_time;
    
    switch (current_game_state) {
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
            // Handle high score submission, etc.
            break;
            
        case GAME_STATE_LEVEL_COMPLETE:
            // Wait for input to continue
            break;
            
        case GAME_STATE_CUTSCENE:
            // Cutscene logic
            break;
    }
}

void start_new_game() {
    initialize_player();
    current_level = 1;
    game_time = 0.0f;
    start_level(1);
}

void pause_game() {
    if (current_game_state == GAME_STATE_PLAYING) {
        current_game_state = GAME_STATE_PAUSED;
    } else if (current_game_state == GAME_STATE_PAUSED) {
        current_game_state = GAME_STATE_PLAYING;
    }
}

void next_level() {
    if (current_game_state == GAME_STATE_LEVEL_COMPLETE) {
        start_level(current_level + 1);
    }
}

// ========================================================================
// DEBUG AND UTILITY FUNCTIONS
// ========================================================================

void toggle_god_mode() {
    god_mode = !god_mode;
}

void toggle_auto_aim() {
    auto_aim_enabled = !auto_aim_enabled;
}

void set_difficulty(float multiplier) {
    difficulty_multiplier = multiplier;
}

int get_active_enemy_count() {
    return enemies_remaining;
}

int get_active_projectile_count() {
    int count = 0;
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        if (projectiles[i].active) count++;
    }
    return count;
}

GameState get_game_state() {
    return current_game_state;
}

Player* get_player() {
    return &player;
}
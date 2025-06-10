/**
 * ========================================================================
 * ENDOR MATHEMATICAL UTILITIES
 * ========================================================================
 * 
 * Mathematical functions for the Endor game engine including vector and
 * matrix operations, trigonometric functions, interpolation, and random
 * number generation. Optimized for game performance.
 */

#include "endor_readable.h"
#include <math.h>
#include <time.h>

// ========================================================================
// MATH CONSTANTS
// ========================================================================

#define PI 3.14159265358979323846
#define TWO_PI (PI * 2.0)
#define HALF_PI (PI / 2.0)
#define DEG_TO_RAD (PI / 180.0)
#define RAD_TO_DEG (180.0 / PI)
#define EPSILON 0.00001f

// Fast math table sizes
#define SIN_TABLE_SIZE 1024
#define SQRT_TABLE_SIZE 256

// ========================================================================
// MATH STRUCTURES
// ========================================================================

// 2D Vector
typedef struct {
    float x, y;
} Vector2D;

// 3D Vector (already defined in endor_readable.h)
// typedef struct {
//     float x, y, z;
// } Vector3D;

// 4x4 Matrix
typedef struct {
    float m[4][4];
} Matrix4x4;

// Quaternion for rotations
typedef struct {
    float w, x, y, z;
} Quaternion;

// Plane equation (Ax + By + Cz + D = 0)
typedef struct {
    float a, b, c, d;
} Plane;

// Axis-aligned bounding box
typedef struct {
    Vector3D min;
    Vector3D max;
} AABB;

// ========================================================================
// MATH GLOBALS
// ========================================================================

// Precalculated tables for performance
static float g_sin_table[SIN_TABLE_SIZE];
static float g_cos_table[SIN_TABLE_SIZE];
static float g_sqrt_table[SQRT_TABLE_SIZE];
static BOOL g_tables_initialized = FALSE;

// Random number generator state
static uint32_t g_random_seed = 0;

// ========================================================================
// INITIALIZATION
// ========================================================================

/**
 * Initializes the math system and precalculated tables
 */
void initialize_math_system()
{
    if (g_tables_initialized)
        return;
    
    // Initialize sin/cos tables
    for (int i = 0; i < SIN_TABLE_SIZE; i++)
    {
        float angle = (float)i * TWO_PI / SIN_TABLE_SIZE;
        g_sin_table[i] = sinf(angle);
        g_cos_table[i] = cosf(angle);
    }
    
    // Initialize sqrt table for small values
    for (int i = 0; i < SQRT_TABLE_SIZE; i++)
    {
        g_sqrt_table[i] = sqrtf((float)i);
    }
    
    // Initialize random seed
    g_random_seed = (uint32_t)time(NULL);
    
    g_tables_initialized = TRUE;
}

// ========================================================================
// FAST MATH FUNCTIONS
// ========================================================================

/**
 * Fast sine using lookup table
 * @param angle Angle in radians
 * @return Sine value
 */
float fast_sin(float angle)
{
    // Normalize angle to [0, 2π]
    while (angle < 0) angle += TWO_PI;
    while (angle >= TWO_PI) angle -= TWO_PI;
    
    // Convert to table index
    int index = (int)(angle * SIN_TABLE_SIZE / TWO_PI) & (SIN_TABLE_SIZE - 1);
    return g_sin_table[index];
}

/**
 * Fast cosine using lookup table
 * @param angle Angle in radians
 * @return Cosine value
 */
float fast_cos(float angle)
{
    // Normalize angle to [0, 2π]
    while (angle < 0) angle += TWO_PI;
    while (angle >= TWO_PI) angle -= TWO_PI;
    
    // Convert to table index
    int index = (int)(angle * SIN_TABLE_SIZE / TWO_PI) & (SIN_TABLE_SIZE - 1);
    return g_cos_table[index];
}

/**
 * Fast inverse square root (Quake III algorithm)
 * @param x Value to compute 1/sqrt(x)
 * @return Inverse square root
 */
float fast_inverse_sqrt(float x)
{
    float xhalf = 0.5f * x;
    int i = *(int*)&x;
    i = 0x5f3759df - (i >> 1);  // Magic number
    x = *(float*)&i;
    x = x * (1.5f - xhalf * x * x);  // Newton iteration
    return x;
}

// ========================================================================
// VECTOR OPERATIONS
// ========================================================================

/**
 * Adds two 3D vectors
 * @param result Output vector (a + b)
 * @param a First vector
 * @param b Second vector
 * @return Pointer to result
 */
Vector3D* vector3_add(Vector3D* result, const Vector3D* a, const Vector3D* b)  // Improved from sub_4086a0
{
    result->x = a->x + b->x;
    result->y = a->y + b->y;
    result->z = a->z + b->z;
    return result;
}

/**
 * Subtracts two 3D vectors
 * @param result Output vector (a - b)
 * @param a First vector
 * @param b Second vector
 * @return Pointer to result
 */
Vector3D* vector3_subtract(Vector3D* result, const Vector3D* a, const Vector3D* b)  // Improved from sub_408ef0
{
    result->x = a->x - b->x;
    result->y = a->y - b->y;
    result->z = a->z - b->z;
    return result;
}

/**
 * Multiplies a 3D vector by a scalar
 * @param result Output vector
 * @param v Input vector
 * @param scalar Scalar value
 * @return Pointer to result
 */
Vector3D* vector3_scale(Vector3D* result, const Vector3D* v, float scalar)
{
    result->x = v->x * scalar;
    result->y = v->y * scalar;
    result->z = v->z * scalar;
    return result;
}

/**
 * Computes dot product of two 3D vectors
 * @param a First vector
 * @param b Second vector
 * @return Dot product
 */
float vector3_dot(const Vector3D* a, const Vector3D* b)
{
    return a->x * b->x + a->y * b->y + a->z * b->z;
}

/**
 * Computes cross product of two 3D vectors
 * @param result Output vector (a × b)
 * @param a First vector
 * @param b Second vector
 * @return Pointer to result
 */
Vector3D* vector3_cross(Vector3D* result, const Vector3D* a, const Vector3D* b)
{
    result->x = a->y * b->z - a->z * b->y;
    result->y = a->z * b->x - a->x * b->z;
    result->z = a->x * b->y - a->y * b->x;
    return result;
}

/**
 * Computes length of a 3D vector
 * @param v Vector
 * @return Vector length
 */
float vector3_length(const Vector3D* v)
{
    return sqrtf(v->x * v->x + v->y * v->y + v->z * v->z);
}

/**
 * Normalizes a 3D vector
 * @param result Output normalized vector
 * @param v Input vector
 * @return Pointer to result
 */
Vector3D* vector3_normalize(Vector3D* result, const Vector3D* v)
{
    float inv_len = fast_inverse_sqrt(v->x * v->x + v->y * v->y + v->z * v->z);
    result->x = v->x * inv_len;
    result->y = v->y * inv_len;
    result->z = v->z * inv_len;
    return result;
}

/**
 * Linearly interpolates between two vectors
 * @param result Output vector
 * @param a Start vector
 * @param b End vector
 * @param t Interpolation factor (0-1)
 * @return Pointer to result
 */
Vector3D* vector3_lerp(Vector3D* result, const Vector3D* a, const Vector3D* b, float t)
{
    result->x = a->x + (b->x - a->x) * t;
    result->y = a->y + (b->y - a->y) * t;
    result->z = a->z + (b->z - a->z) * t;
    return result;
}

// ========================================================================
// MATRIX OPERATIONS
// ========================================================================

/**
 * Creates an identity matrix
 * @param m Output matrix
 */
void matrix4_identity(Matrix4x4* m)
{
    memset(m, 0, sizeof(Matrix4x4));
    m->m[0][0] = 1.0f;
    m->m[1][1] = 1.0f;
    m->m[2][2] = 1.0f;
    m->m[3][3] = 1.0f;
}

/**
 * Multiplies two 4x4 matrices
 * @param result Output matrix (a × b)
 * @param a First matrix
 * @param b Second matrix
 */
void matrix4_multiply(Matrix4x4* result, const Matrix4x4* a, const Matrix4x4* b)
{
    Matrix4x4 temp;
    
    for (int i = 0; i < 4; i++)
    {
        for (int j = 0; j < 4; j++)
        {
            temp.m[i][j] = 0;
            for (int k = 0; k < 4; k++)
            {
                temp.m[i][j] += a->m[i][k] * b->m[k][j];
            }
        }
    }
    
    memcpy(result, &temp, sizeof(Matrix4x4));
}

/**
 * Creates a translation matrix
 * @param m Output matrix
 * @param x X translation
 * @param y Y translation
 * @param z Z translation
 */
void matrix4_translation(Matrix4x4* m, float x, float y, float z)
{
    matrix4_identity(m);
    m->m[3][0] = x;
    m->m[3][1] = y;
    m->m[3][2] = z;
}

/**
 * Creates a scaling matrix
 * @param m Output matrix
 * @param x X scale
 * @param y Y scale
 * @param z Z scale
 */
void matrix4_scale(Matrix4x4* m, float x, float y, float z)
{
    matrix4_identity(m);
    m->m[0][0] = x;
    m->m[1][1] = y;
    m->m[2][2] = z;
}

/**
 * Creates a rotation matrix around X axis
 * @param m Output matrix
 * @param angle Rotation angle in radians
 */
void matrix4_rotation_x(Matrix4x4* m, float angle)
{
    float c = cosf(angle);
    float s = sinf(angle);
    
    matrix4_identity(m);
    m->m[1][1] = c;
    m->m[1][2] = s;
    m->m[2][1] = -s;
    m->m[2][2] = c;
}

/**
 * Creates a rotation matrix around Y axis
 * @param m Output matrix
 * @param angle Rotation angle in radians
 */
void matrix4_rotation_y(Matrix4x4* m, float angle)
{
    float c = cosf(angle);
    float s = sinf(angle);
    
    matrix4_identity(m);
    m->m[0][0] = c;
    m->m[0][2] = -s;
    m->m[2][0] = s;
    m->m[2][2] = c;
}

/**
 * Creates a rotation matrix around Z axis
 * @param m Output matrix
 * @param angle Rotation angle in radians
 */
void matrix4_rotation_z(Matrix4x4* m, float angle)
{
    float c = cosf(angle);
    float s = sinf(angle);
    
    matrix4_identity(m);
    m->m[0][0] = c;
    m->m[0][1] = s;
    m->m[1][0] = -s;
    m->m[1][1] = c;
}

/**
 * Creates a perspective projection matrix
 * @param m Output matrix
 * @param fov Field of view in radians
 * @param aspect Aspect ratio (width/height)
 * @param near Near clipping plane
 * @param far Far clipping plane
 */
void matrix4_perspective(Matrix4x4* m, float fov, float aspect, float near_plane, float far_plane)
{
    float f = 1.0f / tanf(fov * 0.5f);
    float range_inv = 1.0f / (near_plane - far_plane);
    
    memset(m, 0, sizeof(Matrix4x4));
    m->m[0][0] = f / aspect;
    m->m[1][1] = f;
    m->m[2][2] = (far_plane + near_plane) * range_inv;
    m->m[2][3] = -1.0f;
    m->m[3][2] = 2.0f * far_plane * near_plane * range_inv;
}

/**
 * Creates a look-at view matrix
 * @param m Output matrix
 * @param eye Eye position
 * @param target Target position
 * @param up Up vector
 */
void matrix4_look_at(Matrix4x4* m, const Vector3D* eye, const Vector3D* target, const Vector3D* up)
{
    Vector3D zaxis, xaxis, yaxis;
    
    // Calculate view axes
    vector3_subtract(&zaxis, eye, target);
    vector3_normalize(&zaxis, &zaxis);
    
    vector3_cross(&xaxis, up, &zaxis);
    vector3_normalize(&xaxis, &xaxis);
    
    vector3_cross(&yaxis, &zaxis, &xaxis);
    
    // Build view matrix
    m->m[0][0] = xaxis.x;
    m->m[1][0] = xaxis.y;
    m->m[2][0] = xaxis.z;
    m->m[3][0] = -vector3_dot(&xaxis, eye);
    
    m->m[0][1] = yaxis.x;
    m->m[1][1] = yaxis.y;
    m->m[2][1] = yaxis.z;
    m->m[3][1] = -vector3_dot(&yaxis, eye);
    
    m->m[0][2] = zaxis.x;
    m->m[1][2] = zaxis.y;
    m->m[2][2] = zaxis.z;
    m->m[3][2] = -vector3_dot(&zaxis, eye);
    
    m->m[0][3] = 0.0f;
    m->m[1][3] = 0.0f;
    m->m[2][3] = 0.0f;
    m->m[3][3] = 1.0f;
}

/**
 * Transforms a 3D vector by a 4x4 matrix
 * @param result Output vector
 * @param v Input vector
 * @param m Transformation matrix
 * @return Pointer to result
 */
Vector3D* matrix4_transform_vector3(Vector3D* result, const Vector3D* v, const Matrix4x4* m)
{
    float w = v->x * m->m[0][3] + v->y * m->m[1][3] + v->z * m->m[2][3] + m->m[3][3];
    
    result->x = (v->x * m->m[0][0] + v->y * m->m[1][0] + v->z * m->m[2][0] + m->m[3][0]) / w;
    result->y = (v->x * m->m[0][1] + v->y * m->m[1][1] + v->z * m->m[2][1] + m->m[3][1]) / w;
    result->z = (v->x * m->m[0][2] + v->y * m->m[1][2] + v->z * m->m[2][2] + m->m[3][2]) / w;
    
    return result;
}

// ========================================================================
// QUATERNION OPERATIONS
// ========================================================================

/**
 * Creates a quaternion from axis and angle
 * @param q Output quaternion
 * @param axis Rotation axis (normalized)
 * @param angle Rotation angle in radians
 */
void quaternion_from_axis_angle(Quaternion* q, const Vector3D* axis, float angle)
{
    float half_angle = angle * 0.5f;
    float s = sinf(half_angle);
    
    q->w = cosf(half_angle);
    q->x = axis->x * s;
    q->y = axis->y * s;
    q->z = axis->z * s;
}

/**
 * Multiplies two quaternions
 * @param result Output quaternion (a × b)
 * @param a First quaternion
 * @param b Second quaternion
 */
void quaternion_multiply(Quaternion* result, const Quaternion* a, const Quaternion* b)
{
    result->w = a->w * b->w - a->x * b->x - a->y * b->y - a->z * b->z;
    result->x = a->w * b->x + a->x * b->w + a->y * b->z - a->z * b->y;
    result->y = a->w * b->y - a->x * b->z + a->y * b->w + a->z * b->x;
    result->z = a->w * b->z + a->x * b->y - a->y * b->x + a->z * b->w;
}

/**
 * Converts quaternion to rotation matrix
 * @param m Output matrix
 * @param q Input quaternion
 */
void quaternion_to_matrix(Matrix4x4* m, const Quaternion* q)
{
    float xx = q->x * q->x;
    float yy = q->y * q->y;
    float zz = q->z * q->z;
    float xy = q->x * q->y;
    float xz = q->x * q->z;
    float yz = q->y * q->z;
    float wx = q->w * q->x;
    float wy = q->w * q->y;
    float wz = q->w * q->z;
    
    matrix4_identity(m);
    
    m->m[0][0] = 1.0f - 2.0f * (yy + zz);
    m->m[0][1] = 2.0f * (xy + wz);
    m->m[0][2] = 2.0f * (xz - wy);
    
    m->m[1][0] = 2.0f * (xy - wz);
    m->m[1][1] = 1.0f - 2.0f * (xx + zz);
    m->m[1][2] = 2.0f * (yz + wx);
    
    m->m[2][0] = 2.0f * (xz + wy);
    m->m[2][1] = 2.0f * (yz - wx);
    m->m[2][2] = 1.0f - 2.0f * (xx + yy);
}

// ========================================================================
// GEOMETRIC FUNCTIONS
// ========================================================================

/**
 * Calculates distance between two points
 * @param a First point
 * @param b Second point
 * @return Distance
 */
float distance_3d(const Vector3D* a, const Vector3D* b)
{
    float dx = b->x - a->x;
    float dy = b->y - a->y;
    float dz = b->z - a->z;
    return sqrtf(dx * dx + dy * dy + dz * dz);
}

/**
 * Calculates squared distance between two points (faster, no sqrt)
 * @param a First point
 * @param b Second point
 * @return Squared distance
 */
float distance_squared_3d(const Vector3D* a, const Vector3D* b)
{
    float dx = b->x - a->x;
    float dy = b->y - a->y;
    float dz = b->z - a->z;
    return dx * dx + dy * dy + dz * dz;
}

/**
 * Tests if a point is inside an axis-aligned bounding box
 * @param point Point to test
 * @param box Bounding box
 * @return TRUE if inside, FALSE otherwise
 */
BOOL point_in_aabb(const Vector3D* point, const AABB* box)
{
    return point->x >= box->min.x && point->x <= box->max.x &&
           point->y >= box->min.y && point->y <= box->max.y &&
           point->z >= box->min.z && point->z <= box->max.z;
}

/**
 * Tests if two AABBs intersect
 * @param a First bounding box
 * @param b Second bounding box
 * @return TRUE if intersecting, FALSE otherwise
 */
BOOL aabb_intersect(const AABB* a, const AABB* b)
{
    return a->min.x <= b->max.x && a->max.x >= b->min.x &&
           a->min.y <= b->max.y && a->max.y >= b->min.y &&
           a->min.z <= b->max.z && a->max.z >= b->min.z;
}

/**
 * Computes distance from point to plane
 * @param point Point to test
 * @param plane Plane equation
 * @return Signed distance (positive = in front)
 */
float point_to_plane_distance(const Vector3D* point, const Plane* plane)
{
    return plane->a * point->x + plane->b * point->y + plane->c * point->z + plane->d;
}

// ========================================================================
// INTERPOLATION FUNCTIONS
// ========================================================================

/**
 * Linear interpolation
 * @param a Start value
 * @param b End value
 * @param t Interpolation factor (0-1)
 * @return Interpolated value
 */
float lerp(float a, float b, float t)
{
    return a + (b - a) * t;
}

/**
 * Smooth step interpolation (cubic)
 * @param a Start value
 * @param b End value
 * @param t Interpolation factor (0-1)
 * @return Smoothly interpolated value
 */
float smooth_step(float a, float b, float t)
{
    t = t * t * (3.0f - 2.0f * t);
    return lerp(a, b, t);
}

/**
 * Smoother step interpolation (quintic)
 * @param a Start value
 * @param b End value
 * @param t Interpolation factor (0-1)
 * @return Very smoothly interpolated value
 */
float smoother_step(float a, float b, float t)
{
    t = t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
    return lerp(a, b, t);
}

/**
 * Cubic Bezier interpolation
 * @param p0 Start point
 * @param p1 Control point 1
 * @param p2 Control point 2
 * @param p3 End point
 * @param t Interpolation factor (0-1)
 * @return Bezier curve value
 */
float bezier_cubic(float p0, float p1, float p2, float p3, float t)
{
    float t2 = t * t;
    float t3 = t2 * t;
    float mt = 1.0f - t;
    float mt2 = mt * mt;
    float mt3 = mt2 * mt;
    
    return mt3 * p0 + 3.0f * mt2 * t * p1 + 3.0f * mt * t2 * p2 + t3 * p3;
}

// ========================================================================
// RANDOM NUMBER GENERATION
// ========================================================================

/**
 * Seeds the random number generator
 * @param seed Random seed
 */
void random_seed(uint32_t seed)
{
    g_random_seed = seed;
}

/**
 * Generates a random 32-bit integer (xorshift algorithm)
 * @return Random integer
 */
uint32_t random_int()
{
    g_random_seed ^= g_random_seed << 13;
    g_random_seed ^= g_random_seed >> 17;
    g_random_seed ^= g_random_seed << 5;
    return g_random_seed;
}

/**
 * Generates a random float between 0 and 1
 * @return Random float [0, 1)
 */
float random_float()
{
    return (float)(random_int() & 0x7FFFFFFF) / (float)0x80000000;
}

/**
 * Generates a random float in a range
 * @param min Minimum value
 * @param max Maximum value
 * @return Random float [min, max)
 */
float random_range(float min, float max)
{
    return min + random_float() * (max - min);
}

/**
 * Generates a random integer in a range
 * @param min Minimum value (inclusive)
 * @param max Maximum value (exclusive)
 * @return Random integer [min, max)
 */
int random_int_range(int min, int max)
{
    return min + (random_int() % (max - min));
}

/**
 * Generates a random vector within a sphere
 * @param result Output vector
 * @param radius Sphere radius
 * @return Pointer to result
 */
Vector3D* random_vector_in_sphere(Vector3D* result, float radius)
{
    // Use rejection sampling for uniform distribution
    do {
        result->x = random_range(-1.0f, 1.0f);
        result->y = random_range(-1.0f, 1.0f);
        result->z = random_range(-1.0f, 1.0f);
    } while (vector3_length(result) > 1.0f);
    
    vector3_scale(result, result, radius);
    return result;
}

// ========================================================================
// UTILITY FUNCTIONS
// ========================================================================

/**
 * Clamps a value to a range
 * @param value Value to clamp
 * @param min Minimum value
 * @param max Maximum value
 * @return Clamped value
 */
float clamp(float value, float min, float max)
{
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

/**
 * Clamps an integer to a range
 * @param value Value to clamp
 * @param min Minimum value
 * @param max Maximum value
 * @return Clamped value
 */
int clamp_int(int value, int min, int max)
{
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

/**
 * Wraps an angle to [-π, π]
 * @param angle Angle in radians
 * @return Wrapped angle
 */
float wrap_angle(float angle)
{
    while (angle > PI) angle -= TWO_PI;
    while (angle < -PI) angle += TWO_PI;
    return angle;
}

/**
 * Computes the sign of a value
 * @param value Input value
 * @return -1, 0, or 1
 */
int sign(float value)
{
    if (value > 0) return 1;
    if (value < 0) return -1;
    return 0;
}

/**
 * Tests if two floats are approximately equal
 * @param a First value
 * @param b Second value
 * @param epsilon Maximum difference
 * @return TRUE if approximately equal
 */
BOOL float_equals(float a, float b, float epsilon)
{
    return fabsf(a - b) < epsilon;
}

/**
 * Computes the next power of 2
 * @param value Input value
 * @return Next power of 2
 */
uint32_t next_power_of_2(uint32_t value)
{
    value--;
    value |= value >> 1;
    value |= value >> 2;
    value |= value >> 4;
    value |= value >> 8;
    value |= value >> 16;
    value++;
    return value;
}

/**
 * Fast float to int conversion
 * @param value Float value
 * @return Integer value
 */
inline int fast_float_to_int(float value)
{
    return (int)(value + 0.5f);
}

/**
 * Computes factorial
 * @param n Input value
 * @return n!
 */
uint32_t factorial(uint32_t n)
{
    uint32_t result = 1;
    for (uint32_t i = 2; i <= n; i++)
        result *= i;
    return result;
}

/**
 * Computes binomial coefficient (n choose k)
 * @param n Total items
 * @param k Items to choose
 * @return Binomial coefficient
 */
uint32_t binomial_coefficient(uint32_t n, uint32_t k)
{
    if (k > n) return 0;
    if (k == 0 || k == n) return 1;
    
    // Use symmetry property
    if (k > n - k) k = n - k;
    
    uint32_t result = 1;
    for (uint32_t i = 0; i < k; i++)
    {
        result *= (n - i);
        result /= (i + 1);
    }
    
    return result;
}
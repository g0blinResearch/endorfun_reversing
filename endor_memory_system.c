/**
 * ========================================================================
 * ENDOR MEMORY MANAGEMENT SYSTEM
 * ========================================================================
 * 
 * Custom memory allocation, tracking, and debugging utilities for the
 * Endor game engine. Provides memory pooling, leak detection, and
 * optimized allocation strategies for game objects.
 */

#include "endor_readable.h"
#include <stdlib.h>
#include <string.h>

// ========================================================================
// MEMORY SYSTEM CONSTANTS
// ========================================================================

#define MAX_MEMORY_POOLS 16
#define MAX_ALLOCATIONS 4096
#define MEMORY_GUARD_VALUE 0xDEADBEEF
#define MEMORY_FREED_VALUE 0xFEEDFACE
#define MEMORY_ALIGNMENT 16

// Memory pool sizes
#define POOL_TINY_SIZE 64
#define POOL_SMALL_SIZE 256
#define POOL_MEDIUM_SIZE 1024
#define POOL_LARGE_SIZE 4096

// ========================================================================
// MEMORY SYSTEM STRUCTURES
// ========================================================================

// Memory allocation header
typedef struct MemoryHeader {
    uint32_t guard_start;
    size_t size;
    size_t actual_size;
    const char* file;
    int line;
    uint32_t allocation_id;
    struct MemoryHeader* next;
    struct MemoryHeader* prev;
} MemoryHeader;

// Memory footer for overflow detection
typedef struct {
    uint32_t guard_end;
} MemoryFooter;

// Memory pool structure
typedef struct {
    void* base_address;
    size_t total_size;
    size_t block_size;
    size_t num_blocks;
    uint32_t* free_list;
    int free_count;
    int initialized;
} MemoryPool;

// Memory statistics
typedef struct {
    size_t total_allocated;
    size_t current_allocated;
    size_t peak_allocated;
    uint32_t allocation_count;
    uint32_t deallocation_count;
    uint32_t reallocation_count;
} MemoryStats;

// ========================================================================
// MEMORY SYSTEM GLOBALS
// ========================================================================

// Memory pools for different sizes
static MemoryPool g_memory_pools[MAX_MEMORY_POOLS];
static int g_num_pools = 0;

// Memory tracking
static MemoryHeader* g_allocation_list = NULL;
static uint32_t g_next_allocation_id = 1;
static MemoryStats g_memory_stats = {0};

// Debug settings
static BOOL g_memory_debug_enabled = FALSE;
static BOOL g_memory_tracking_enabled = FALSE;
static FILE* g_memory_log_file = NULL;

// Global memory addresses from original code
static void* g_memory_base = NULL;        // was data_42ac38
static void* g_memory_current = NULL;     // was data_42ac3c
static size_t g_memory_limit = 0;         // was data_427000
static size_t g_memory_used = 0;          // was data_427004

// ========================================================================
// MEMORY INITIALIZATION
// ========================================================================

/**
 * Initializes the memory management system
 * @param heap_size Size of the main heap in bytes
 * @param enable_debug Enable memory debugging features
 * @return TRUE on success, FALSE on failure
 */
BOOL initialize_memory_system(size_t heap_size, BOOL enable_debug)  // Improved from sub_4026b0
{
    // Allocate main heap
    g_memory_base = VirtualAlloc(NULL, heap_size, 
                                MEM_COMMIT | MEM_RESERVE, 
                                PAGE_READWRITE);
    if (!g_memory_base)
        return FALSE;
    
    g_memory_current = g_memory_base;
    g_memory_limit = heap_size;
    g_memory_used = 0;
    
    // Initialize memory pools
    create_memory_pool(POOL_TINY_SIZE, 256);
    create_memory_pool(POOL_SMALL_SIZE, 128);
    create_memory_pool(POOL_MEDIUM_SIZE, 64);
    create_memory_pool(POOL_LARGE_SIZE, 32);
    
    // Enable debugging if requested
    g_memory_debug_enabled = enable_debug;
    g_memory_tracking_enabled = enable_debug;
    
    if (enable_debug)
    {
        g_memory_log_file = fopen("memory_log.txt", "w");
        if (g_memory_log_file)
            fprintf(g_memory_log_file, "Memory system initialized with %zu bytes\n", heap_size);
    }
    
    return TRUE;
}

/**
 * Shuts down the memory system and releases all resources
 */
void shutdown_memory_system()
{
    // Report any memory leaks
    if (g_memory_tracking_enabled)
        report_memory_leaks();
    
    // Free all memory pools
    for (int i = 0; i < g_num_pools; i++)
    {
        if (g_memory_pools[i].base_address)
        {
            VirtualFree(g_memory_pools[i].base_address, 0, MEM_RELEASE);
            free(g_memory_pools[i].free_list);
        }
    }
    
    // Free main heap
    if (g_memory_base)
    {
        VirtualFree(g_memory_base, 0, MEM_RELEASE);
        g_memory_base = NULL;
    }
    
    // Close log file
    if (g_memory_log_file)
    {
        fclose(g_memory_log_file);
        g_memory_log_file = NULL;
    }
}

// ========================================================================
// MEMORY ALLOCATION
// ========================================================================

/**
 * Allocates memory with tracking and debugging
 * @param size Number of bytes to allocate
 * @param file Source file name (for debugging)
 * @param line Source line number (for debugging)
 * @return Pointer to allocated memory, or NULL on failure
 */
void* allocate_memory_debug(size_t size, const char* file, int line)
{
    // Try pool allocation first for small sizes
    if (size <= POOL_LARGE_SIZE)
    {
        void* ptr = allocate_from_pool(size);
        if (ptr)
        {
            if (g_memory_tracking_enabled)
                track_allocation(ptr, size, file, line);
            return ptr;
        }
    }
    
    // Fall back to heap allocation
    size_t total_size = size + sizeof(MemoryHeader) + sizeof(MemoryFooter);
    total_size = (total_size + MEMORY_ALIGNMENT - 1) & ~(MEMORY_ALIGNMENT - 1);
    
    MemoryHeader* header = (MemoryHeader*)malloc(total_size);
    if (!header)
        return NULL;
    
    // Initialize header
    header->guard_start = MEMORY_GUARD_VALUE;
    header->size = size;
    header->actual_size = total_size;
    header->file = file;
    header->line = line;
    header->allocation_id = g_next_allocation_id++;
    header->next = NULL;
    header->prev = NULL;
    
    // Initialize footer
    MemoryFooter* footer = (MemoryFooter*)((char*)header + sizeof(MemoryHeader) + size);
    footer->guard_end = MEMORY_GUARD_VALUE;
    
    // Update statistics
    g_memory_stats.total_allocated += size;
    g_memory_stats.current_allocated += size;
    if (g_memory_stats.current_allocated > g_memory_stats.peak_allocated)
        g_memory_stats.peak_allocated = g_memory_stats.current_allocated;
    g_memory_stats.allocation_count++;
    
    // Add to tracking list
    if (g_memory_tracking_enabled)
    {
        header->next = g_allocation_list;
        if (g_allocation_list)
            g_allocation_list->prev = header;
        g_allocation_list = header;
    }
    
    // Log allocation
    if (g_memory_log_file)
    {
        fprintf(g_memory_log_file, "ALLOC: %u bytes at %s:%d (ID: %u)\n",
                (unsigned)size, file, line, header->allocation_id);
    }
    
    return (char*)header + sizeof(MemoryHeader);
}

/**
 * Standard allocation wrapper
 */
void* allocate_memory(size_t size)
{
    return allocate_memory_debug(size, "unknown", 0);
}

/**
 * Frees allocated memory with validation
 * @param ptr Pointer to memory to free
 */
void free_memory(void* ptr)
{
    if (!ptr)
        return;
    
    // Check if from pool
    if (is_pool_allocation(ptr))
    {
        free_to_pool(ptr);
        return;
    }
    
    // Get header
    MemoryHeader* header = (MemoryHeader*)((char*)ptr - sizeof(MemoryHeader));
    
    // Validate guards
    if (g_memory_debug_enabled)
    {
        if (header->guard_start != MEMORY_GUARD_VALUE)
        {
            if (g_memory_log_file)
                fprintf(g_memory_log_file, "ERROR: Memory underflow detected!\n");
            return;
        }
        
        MemoryFooter* footer = (MemoryFooter*)((char*)ptr + header->size);
        if (footer->guard_end != MEMORY_GUARD_VALUE)
        {
            if (g_memory_log_file)
                fprintf(g_memory_log_file, "ERROR: Memory overflow detected!\n");
            return;
        }
    }
    
    // Update statistics
    g_memory_stats.current_allocated -= header->size;
    g_memory_stats.deallocation_count++;
    
    // Remove from tracking list
    if (g_memory_tracking_enabled)
    {
        if (header->prev)
            header->prev->next = header->next;
        else
            g_allocation_list = header->next;
        
        if (header->next)
            header->next->prev = header->prev;
    }
    
    // Log deallocation
    if (g_memory_log_file)
    {
        fprintf(g_memory_log_file, "FREE: ID %u (%u bytes)\n",
                header->allocation_id, (unsigned)header->size);
    }
    
    // Mark as freed (for double-free detection)
    header->guard_start = MEMORY_FREED_VALUE;
    
    free(header);
}

// ========================================================================
// MEMORY POOLS
// ========================================================================

/**
 * Creates a memory pool for fixed-size allocations
 * @param block_size Size of each block in the pool
 * @param num_blocks Number of blocks in the pool
 * @return Pool index on success, -1 on failure
 */
int create_memory_pool(size_t block_size, size_t num_blocks)
{
    if (g_num_pools >= MAX_MEMORY_POOLS)
        return -1;
    
    MemoryPool* pool = &g_memory_pools[g_num_pools];
    
    // Align block size
    block_size = (block_size + MEMORY_ALIGNMENT - 1) & ~(MEMORY_ALIGNMENT - 1);
    
    // Allocate pool memory
    pool->total_size = block_size * num_blocks;
    pool->base_address = VirtualAlloc(NULL, pool->total_size,
                                     MEM_COMMIT | MEM_RESERVE,
                                     PAGE_READWRITE);
    if (!pool->base_address)
        return -1;
    
    // Initialize pool
    pool->block_size = block_size;
    pool->num_blocks = num_blocks;
    pool->free_list = (uint32_t*)malloc(num_blocks * sizeof(uint32_t));
    pool->free_count = num_blocks;
    pool->initialized = 1;
    
    // Initialize free list
    for (size_t i = 0; i < num_blocks; i++)
        pool->free_list[i] = i;
    
    return g_num_pools++;
}

/**
 * Allocates from an appropriate memory pool
 * @param size Size to allocate
 * @return Pointer to allocated memory, or NULL if no suitable pool
 */
void* allocate_from_pool(size_t size)
{
    // Find suitable pool
    for (int i = 0; i < g_num_pools; i++)
    {
        MemoryPool* pool = &g_memory_pools[i];
        if (pool->initialized && pool->block_size >= size && pool->free_count > 0)
        {
            // Get free block index
            uint32_t block_index = pool->free_list[--pool->free_count];
            
            // Calculate block address
            void* block = (char*)pool->base_address + (block_index * pool->block_size);
            
            // Clear block
            memset(block, 0, pool->block_size);
            
            return block;
        }
    }
    
    return NULL;
}

/**
 * Returns memory to its pool
 * @param ptr Pointer to pool memory
 */
void free_to_pool(void* ptr)
{
    // Find which pool this belongs to
    for (int i = 0; i < g_num_pools; i++)
    {
        MemoryPool* pool = &g_memory_pools[i];
        if (ptr >= pool->base_address && 
            ptr < (char*)pool->base_address + pool->total_size)
        {
            // Calculate block index
            size_t offset = (char*)ptr - (char*)pool->base_address;
            uint32_t block_index = offset / pool->block_size;
            
            // Return to free list
            pool->free_list[pool->free_count++] = block_index;
            return;
        }
    }
}

/**
 * Checks if a pointer is from a memory pool
 * @param ptr Pointer to check
 * @return TRUE if from pool, FALSE otherwise
 */
BOOL is_pool_allocation(void* ptr)
{
    for (int i = 0; i < g_num_pools; i++)
    {
        MemoryPool* pool = &g_memory_pools[i];
        if (ptr >= pool->base_address && 
            ptr < (char*)pool->base_address + pool->total_size)
            return TRUE;
    }
    return FALSE;
}

// ========================================================================
// MEMORY UTILITIES
// ========================================================================

/**
 * Reallocates memory to a new size
 * @param ptr Original pointer
 * @param new_size New size in bytes
 * @return Pointer to reallocated memory
 */
void* reallocate_memory(void* ptr, size_t new_size)
{
    if (!ptr)
        return allocate_memory(new_size);
    
    if (new_size == 0)
    {
        free_memory(ptr);
        return NULL;
    }
    
    // Get old size
    MemoryHeader* header = (MemoryHeader*)((char*)ptr - sizeof(MemoryHeader));
    size_t old_size = header->size;
    
    // Allocate new block
    void* new_ptr = allocate_memory(new_size);
    if (!new_ptr)
        return NULL;
    
    // Copy data
    memcpy(new_ptr, ptr, (old_size < new_size) ? old_size : new_size);
    
    // Free old block
    free_memory(ptr);
    
    g_memory_stats.reallocation_count++;
    
    return new_ptr;
}

/**
 * Duplicates a string with memory tracking
 * @param str String to duplicate
 * @return Pointer to duplicated string
 */
char* duplicate_string(const char* str)
{
    if (!str)
        return NULL;
    
    size_t len = strlen(str) + 1;
    char* dup = (char*)allocate_memory(len);
    if (dup)
        memcpy(dup, str, len);
    
    return dup;
}

// ========================================================================
// MEMORY DEBUGGING
// ========================================================================

/**
 * Reports current memory statistics
 */
void report_memory_stats()
{
    printf("Memory Statistics:\n");
    printf("  Total Allocated: %zu bytes\n", g_memory_stats.total_allocated);
    printf("  Current Allocated: %zu bytes\n", g_memory_stats.current_allocated);
    printf("  Peak Allocated: %zu bytes\n", g_memory_stats.peak_allocated);
    printf("  Allocations: %u\n", g_memory_stats.allocation_count);
    printf("  Deallocations: %u\n", g_memory_stats.deallocation_count);
    printf("  Reallocations: %u\n", g_memory_stats.reallocation_count);
    
    if (g_memory_log_file)
    {
        fprintf(g_memory_log_file, "\nMemory Statistics:\n");
        fprintf(g_memory_log_file, "  Total Allocated: %zu bytes\n", g_memory_stats.total_allocated);
        fprintf(g_memory_log_file, "  Current Allocated: %zu bytes\n", g_memory_stats.current_allocated);
        fprintf(g_memory_log_file, "  Peak Allocated: %zu bytes\n", g_memory_stats.peak_allocated);
        fprintf(g_memory_log_file, "  Allocations: %u\n", g_memory_stats.allocation_count);
        fprintf(g_memory_log_file, "  Deallocations: %u\n", g_memory_stats.deallocation_count);
        fprintf(g_memory_log_file, "  Reallocations: %u\n", g_memory_stats.reallocation_count);
    }
}

/**
 * Reports any memory leaks
 */
void report_memory_leaks()
{
    if (!g_memory_tracking_enabled)
        return;
    
    int leak_count = 0;
    size_t leak_size = 0;
    
    MemoryHeader* current = g_allocation_list;
    while (current)
    {
        leak_count++;
        leak_size += current->size;
        
        if (g_memory_log_file)
        {
            fprintf(g_memory_log_file, "LEAK: ID %u - %zu bytes at %s:%d\n",
                    current->allocation_id, current->size,
                    current->file, current->line);
        }
        
        current = current->next;
    }
    
    if (leak_count > 0)
    {
        printf("WARNING: %d memory leaks detected (%zu bytes)\n", leak_count, leak_size);
        if (g_memory_log_file)
            fprintf(g_memory_log_file, "\nTotal Leaks: %d (%zu bytes)\n", leak_count, leak_size);
    }
}

/**
 * Tracks an allocation for debugging
 */
void track_allocation(void* ptr, size_t size, const char* file, int line)
{
    // Implementation for pool allocations
    // Would need additional tracking structure for pool allocations
}

// ========================================================================
// MEMORY DEFRAGMENTATION
// ========================================================================

/**
 * Defragments the memory heap (compacts allocations)
 * @return Number of bytes reclaimed
 */
size_t defragment_memory()
{
    // This would be a complex operation requiring:
    // 1. Pause all allocations
    // 2. Move all allocations to be contiguous
    // 3. Update all pointers
    // 4. Resume allocations
    // For now, return 0 as placeholder
    return 0;
}

// ========================================================================
// MACROS FOR EASY USE
// ========================================================================

#ifdef _DEBUG
    #define ALLOC(size) allocate_memory_debug(size, __FILE__, __LINE__)
    #define FREE(ptr) free_memory(ptr)
    #define REALLOC(ptr, size) reallocate_memory(ptr, size)
#else
    #define ALLOC(size) allocate_memory(size)
    #define FREE(ptr) free_memory(ptr)
    #define REALLOC(ptr, size) reallocate_memory(ptr, size)
#endif
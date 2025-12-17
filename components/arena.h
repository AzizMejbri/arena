
#ifndef ARENA_H
#define ARENA_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/**
 * @struct Arena
 * @brief Arena memory allocator context
 * 
 * An arena allocator provides fast, region-based memory allocation
 * where all allocations are freed simultaneously when the arena is reset or destroyed.
 * This eliminates individual allocation overhead and fragmentation.
 * 
 * @var buffer    - Pointer to the underlying memory buffer
 * @var capacity  - Total size of the arena in bytes
 * @var used      - Number of bytes currently allocated
 * @var commit    - Committed memory (for virtual memory systems, defaults to capacity)
 * @var alignment - Default alignment for allocations
 * @var max_align - Maximum supported alignment
 */
typedef struct Arena {
    uint8_t *buffer;      // Pointer to allocated memory
    uint64_t capacity;    // Total size of the arena
    uint64_t used;        // Currently used bytes
    uint64_t commit;      // Committed memory (if using virtual memory)
    uint64_t alignment;   // Default alignment
    uint64_t max_align;   // Maximum alignment supported
} Arena;

/**
 * @brief Create a new arena allocator on the heap
 * 
 * Allocates memory from the heap and initializes an arena context.
 * The arena will use the default alignment (typically 8 bytes).
 * 
 * @param capacity Total size of the arena in bytes
 * @return Arena Initialized arena structure
 * 
 * @note Use arena_destroy() to free the arena's memory
 * @warning Returns zeroed structure on allocation failure
 * 
 * @example
 * Arena arena = arena_create(1024 * 1024); // 1MB arena
 * if (arena.buffer) { ... }
 */
Arena arena_create(uint64_t capacity);

/**
 * @brief Create a new arena allocator with custom alignment
 * 
 * Similar to arena_create() but allows specifying the default alignment.
 * Alignment will be adjusted to the nearest power of two if needed.
 * 
 * @param capacity  Total size of the arena in bytes
 * @param alignment Default alignment for allocations (must be power of two)
 * @return Arena Initialized arena structure
 * 
 * @note If alignment is 0 or not a power of two, default alignment is used
 * @note Maximum alignment is capped (typically 16 bytes)
 * 
 * @example
 * Arena arena = arena_create_with_alignment(4096, 16); // 4KB arena, 16-byte aligned
 */
Arena arena_create_with_alignment(uint64_t capacity, uint64_t alignment);

/**
 * @brief Initialize an arena with an existing memory buffer
 * 
 * Uses a pre-allocated buffer (e.g., stack memory, static buffer) as arena storage.
 * This avoids heap allocation for the arena itself.
 * 
 * @param arena   Pointer to Arena structure to initialize
 * @param buffer  Pre-allocated memory buffer
 * @param capacity Size of the buffer in bytes
 * @return bool   True if initialization succeeded, false otherwise
 * 
 * @note The buffer must remain valid for the lifetime of arena usage
 * @note Does not allocate memory - uses provided buffer
 * @warning Do not call arena_destroy() on arena-initial arenas (no heap allocation)
 * 
 * @example
 * uint8_t buffer[1024];
 * Arena arena;
 * if (arena_init(&arena, buffer, sizeof(buffer))) { ... }
 */
bool arena_init(Arena *arena, void *buffer, uint64_t capacity);

/**
 * @brief Destroy an arena and free its memory
 * 
 * Releases the arena's buffer back to the heap (if created with arena_create).
 * After destruction, the arena structure should not be used.
 * 
 * @param arena Pointer to arena to destroy
 * 
 * @note Safe to call on zero-initialized or already-destroyed arenas
 * @note For arena_init() arenas, this only zeros the structure (no free)
 * 
 * @example
 * Arena arena = arena_create(1024);
 * // ... use arena ...
 * arena_destroy(&arena);
 * // arena.buffer is now NULL
 */
void arena_destroy(Arena *arena);

/**
 * @brief Reset an arena, freeing all allocations
 * 
 * Resets the arena's used counter to zero, effectively freeing all allocations.
 * The arena's memory remains allocated and can be reused.
 * 
 * @param arena Pointer to arena to reset
 * 
 * @note O(1) operation - very fast
 * @note All pointers previously returned by arena_alloc become invalid
 * 
 * @example
 * arena_reset(&arena); // All previous allocations are now freed
 */
void arena_reset(Arena *arena);

/**
 * @brief Get the number of bytes currently allocated in the arena
 * 
 * @param arena Pointer to arena (can be NULL)
 * @return uint64_t Number of bytes currently in use
 * 
 * @note Returns 0 if arena is NULL
 * 
 * @example
 * uint64_t used = arena_used(&arena);
 * printf("%llu bytes used\n", (unsigned long long)used);
 */
uint64_t arena_used(const Arena *arena);

/**
 * @brief Get the number of bytes remaining in the arena
 * 
 * @param arena Pointer to arena (can be NULL)
 * @return uint64_t Number of bytes available for allocation
 * 
 * @note Returns 0 if arena is NULL
 * @note Does not account for alignment padding that may occur in next allocation
 * 
 * @example
 * if (arena_remaining(&arena) >= required_size) {
 *     // Safe to allocate
 * }
 */
uint64_t arena_remaining(const Arena *arena);

/**
 * @brief Allocate memory from the arena
 * 
 * Allocates a contiguous block of memory from the arena.
 * Memory is aligned to the arena's default alignment.
 * 
 * @param arena Pointer to arena
 * @param size  Number of bytes to allocate
 * @return void* Pointer to allocated memory, or NULL if out of memory
 * 
 * @note Returns NULL if arena is NULL or size is 0
 * @note Allocation is O(1) with no per-allocation overhead
 * @warning Pointers remain valid until arena_reset() or arena_destroy()
 * 
 * @example
 * int *array = arena_alloc(&arena, 100 * sizeof(int));
 * if (array) { ... }
 */
void *arena_alloc(Arena *arena, uint64_t size);

/**
 * @brief Allocate aligned memory from the arena
 * 
 * Allocates memory with specific alignment requirements.
 * Alignment must be a power of two.
 * 
 * @param arena     Pointer to arena
 * @param size      Number of bytes to allocate
 * @param alignment Required alignment (must be power of two)
 * @return void*    Pointer to aligned memory, or NULL if out of memory
 * 
 * @note If alignment is 0 or not a power of two, default alignment is used
 * @note Alignment is capped to the arena's max_align
 * @note May consume extra padding bytes for alignment
 * 
 * @example
 * // Allocate cache-line aligned memory
 * void *aligned = arena_alloc_aligned(&arena, 256, 64);
 */
void *arena_alloc_aligned(Arena *arena, uint64_t size, uint64_t alignment);

/**
 * @brief Allocate zero-initialized memory from the arena
 * 
 * Allocates memory and sets all bytes to zero.
 * Equivalent to arena_alloc() followed by memset to zero.
 * 
 * @param arena Pointer to arena
 * @param size  Number of bytes to allocate and zero
 * @return void* Pointer to zeroed memory, or NULL if out of memory
 * 
 * @note Slightly slower than arena_alloc() due to zeroing
 * @note Useful for arrays, structs, or any data that should start zeroed
 * 
 * @example
 * // Allocate and zero a struct
 * MyStruct *s = arena_alloc_zeroed(&arena, sizeof(MyStruct));
 * // s->all_fields_are_zero is guaranteed
 */
void *arena_alloc_zeroed(Arena *arena, uint64_t size);

/**
 * @brief Reallocate memory in the arena
 * 
 * Attempts to resize an existing allocation. If the allocation
 * is at the end of the arena and there's enough space, it extends in-place.
 * Otherwise, allocates new memory and copies old data.
 * 
 * @param arena    Pointer to arena
 * @param ptr      Pointer to existing allocation (must be from this arena)
 * @param old_size Current size of allocation
 * @param new_size Desired new size
 * @return void*   Pointer to resized memory, or NULL if out of memory
 * 
 * @note If NULL is returned, original allocation remains valid
 * @note If ptr is NULL, behaves like arena_alloc(new_size)
 * @note If new_size is 0, returns NULL (memory is not freed)
 * @warning old_size must be accurate for in-place extension to work
 * 
 * @example
 * int *arr = arena_alloc(&arena, 10 * sizeof(int));
 * arr = arena_realloc(&arena, arr, 10 * sizeof(int), 20 * sizeof(int));
 */
void *arena_realloc(Arena *arena, void *ptr, uint64_t old_size, uint64_t new_size);

/**
 * @brief Allocate and zero memory for an array
 * 
 * Allocates memory for count elements of size bytes each, and zeroes it.
 * Equivalent to arena_alloc_zeroed(count * size).
 * 
 * @param arena Pointer to arena
 * @param count Number of elements
 * @param size  Size of each element
 * @return void* Pointer to zeroed memory, or NULL if out of memory
 * 
 * @note Checks for multiplication overflow (returns NULL if overflow occurs)
 * @note Similar to standard calloc() but from arena memory
 * 
 * @example
 * int *zeros = arena_calloc(&arena, 100, sizeof(int));
 * // zeros[0..99] are all 0
 */
void *arena_calloc(Arena *arena, uint64_t count, uint64_t size);

/**
 * @brief Duplicate a string in arena memory
 * 
 * Allocates memory and copies a null-terminated string into it.
 * 
 * @param arena Pointer to arena
 * @param str   String to duplicate (must be null-terminated)
 * @return char* Pointer to duplicated string, or NULL if out of memory
 * 
 * @note Includes null terminator in allocation
 * @note Returns NULL if str is NULL
 * 
 * @example
 * char *copy = arena_strdup(&arena, "Hello, World!");
 */
char *arena_strdup(Arena *arena, const char *str);

/**
 * @brief Duplicate a string with length limit
 * 
 * Allocates memory and copies at most n characters from string,
 * plus a null terminator.
 * 
 * @param arena Pointer to arena
 * @param str   String to duplicate
 * @param n     Maximum number of characters to copy
 * @return char* Pointer to duplicated string, or NULL if out of memory
 * 
 * @note Always null-terminates the result
 * @note If strlen(str) < n, copies entire string
 * @note Returns NULL if str is NULL
 * 
 * @example
 * // Copy at most 10 chars
 * char *short_copy = arena_strndup(&arena, "Very long string", 10);
 * // short_copy is "Very long "
 */
char *arena_strndup(Arena *arena, const char *str, uint64_t n);

/**
 * @struct ArenaTemp
 * @brief Temporary arena scope marker
 * 
 * Used to create temporary allocation scopes within an arena.
 * Allocations made after arena_temp_begin() are automatically
 * freed when arena_temp_end() is called.
 * 
 * @var arena - Pointer to parent arena
 * @var used  - Snapshot of arena's used counter at scope start
 */
typedef struct {
    Arena *arena;
    uint64_t used;
} ArenaTemp;

/**
 * @brief Begin a temporary allocation scope
 * 
 * Creates a checkpoint in the arena. Allocations made after this call
 * can be rolled back by calling arena_temp_end() with the returned marker.
 * 
 * @param arena Pointer to arena
 * @return ArenaTemp Scope marker for later rollback
 * 
 * @note Multiple nested scopes are supported
 * @note If arena is NULL, returns zeroed marker (safe but useless)
 * 
 * @example
 * ArenaTemp temp = arena_temp_begin(&arena);
 * // ... temporary allocations ...
 * arena_temp_end(temp); // All temporary allocations freed
 */
ArenaTemp arena_temp_begin(Arena *arena);

/**
 * @brief End a temporary allocation scope
 * 
 * Rolls back all allocations made since the corresponding arena_temp_begin().
 * 
 * @param temp Scope marker returned by arena_temp_begin()
 * 
 * @note Safe to call with zeroed/invalid marker (no effect)
 * @note Must be called in reverse order of arena_temp_begin() calls
 * 
 * @example
 * ArenaTemp temp = arena_temp_begin(&arena);
 * void *data = arena_alloc(&arena, 100);
 * arena_temp_end(temp); // data is now invalid
 */
void arena_temp_end(ArenaTemp temp);

#endif // ARENA_H

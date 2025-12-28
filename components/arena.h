#ifndef ARENA_H
#define ARENA_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * @struct ArenaBlock
 * @brief Internal block for multi-block arena allocator
 */
typedef struct ArenaBlock {
  uint8_t *buffer;         // Pointer to allocated memory
  uint64_t capacity;       // Total size of this block
  uint64_t used;           // Bytes currently used in this block
  struct ArenaBlock *next; // Next block in linked list
} ArenaBlock;

/**
 * @struct Arena
 * @brief Multi-block arena memory allocator context
 */
typedef struct Arena {
  ArenaBlock *head;   // Current allocation block
  uint64_t alignment; // Default alignment
  uint64_t max_align; // Maximum alignment supported
} Arena;

/**
 * @struct ArenaTemp
 * @brief Temporary arena scope marker
 */
typedef struct {
  Arena *arena;
  uint64_t used;
} ArenaTemp;

// Arena creation & destruction
Arena arena_create(uint64_t capacity);
Arena arena_create_with_alignment(uint64_t capacity, uint64_t alignment);
void arena_destroy(Arena *arena);
void arena_reset(Arena *arena);

// Memory usage
uint64_t arena_used(const Arena *arena);
uint64_t arena_remaining(const Arena *arena);

// Memory allocation
void *arena_alloc(Arena *arena, uint64_t size);
void *arena_alloc_aligned(Arena *arena, uint64_t size, uint64_t alignment);
void *arena_alloc_zeroed(Arena *arena, uint64_t size);
void *arena_calloc(Arena *arena, uint64_t count, uint64_t size);
void *arena_realloc(Arena *arena, void *ptr, uint64_t old_size,
                    uint64_t new_size);

// String utilities
char *arena_strdup(Arena *arena, const char *str);
char *arena_strndup(Arena *arena, const char *str, uint64_t n);

// Temporary allocation scopes
ArenaTemp arena_temp_begin(Arena *arena);
void arena_temp_end(ArenaTemp temp);

#endif // ARENA_H

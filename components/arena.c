#include "arena.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifndef ARENA_DEFAULT_ALIGNMENT
#define ARENA_DEFAULT_ALIGNMENT 8
#endif

#ifndef ARENA_MAX_ALIGNMENT
#define ARENA_MAX_ALIGNMENT 16
#endif

// Helper: align pointer to next multiple of alignment
static inline uintptr_t align_forward(uintptr_t ptr, uint64_t alignment) {
  return (ptr + (alignment - 1)) & ~(alignment - 1);
}

// Create a new block
static ArenaBlock *arena_block_create(uint64_t capacity, uint64_t alignment) {
  if (alignment == 0 || (alignment & (alignment - 1)) != 0)
    alignment = ARENA_DEFAULT_ALIGNMENT;

  if (alignment > ARENA_MAX_ALIGNMENT)
    alignment = ARENA_MAX_ALIGNMENT;

  void *mem = NULL;
  if (posix_memalign(&mem, alignment, capacity) != 0)
    return NULL;

  ArenaBlock *block = malloc(sizeof(ArenaBlock));
  if (!block) {
    free(mem);
    return NULL;
  }

  block->buffer = (uint8_t *)mem;
  block->capacity = capacity;
  block->used = 0;
  block->next = NULL;
  return block;
}

// ------------------------
// Arena API
// ------------------------
Arena arena_create(uint64_t capacity) {
  return arena_create_with_alignment(capacity, ARENA_DEFAULT_ALIGNMENT);
}

Arena arena_create_with_alignment(uint64_t capacity, uint64_t alignment) {
  Arena arena = {0};
  ArenaBlock *block =
      arena_block_create(capacity > 0 ? capacity : 1024, alignment);
  if (!block)
    return arena;

  arena.head = block;
  arena.alignment = alignment;
  arena.max_align = ARENA_MAX_ALIGNMENT;
  return arena;
}

void arena_destroy(Arena *arena) {
  if (!arena)
    return;

  ArenaBlock *block = arena->head;
  while (block) {
    ArenaBlock *next = block->next;
    free(block->buffer);
    free(block);
    block = next;
  }
  arena->head = NULL;
}

void arena_reset(Arena *arena) {
  if (!arena)
    return;
  for (ArenaBlock *b = arena->head; b; b = b->next)
    b->used = 0;
}

uint64_t arena_used(const Arena *arena) {
  if (!arena)
    return 0;
  uint64_t total = 0;
  for (ArenaBlock *b = arena->head; b; b = b->next)
    total += b->used;
  return total;
}

uint64_t arena_remaining(const Arena *arena) {
  if (!arena || !arena->head)
    return 0;
  return arena->head->capacity - arena->head->used;
}

// Allocate memory from arena (with alignment)
void *arena_alloc_aligned(Arena *arena, uint64_t size, uint64_t alignment) {
  if (!arena || size == 0)
    return NULL;
  if (!arena->head)
    return NULL;

  if (alignment == 0 || (alignment & (alignment - 1)) != 0)
    alignment = arena->alignment;

  if (alignment > arena->max_align)
    alignment = arena->max_align;

  ArenaBlock *block = arena->head;
  uintptr_t current_ptr = (uintptr_t)block->buffer + block->used;
  uintptr_t aligned_ptr = align_forward(current_ptr, alignment);
  size_t padding = aligned_ptr - current_ptr;

  if (block->used + padding + size <= block->capacity) {
    block->used += padding + size;
    return (void *)aligned_ptr;
  }

  // Not enough space: create new block
  uint64_t new_capacity = block->capacity * 2;
  if (new_capacity < size)
    new_capacity = size * 2;

  ArenaBlock *new_block = arena_block_create(new_capacity, arena->alignment);
  if (!new_block)
    return NULL;

  new_block->next = arena->head;
  arena->head = new_block;

  aligned_ptr = align_forward((uintptr_t)new_block->buffer, alignment);
  new_block->used = size;
  return (void *)aligned_ptr;
}

void *arena_alloc(Arena *arena, uint64_t size) {
  return arena_alloc_aligned(
      arena, size, arena ? arena->alignment : ARENA_DEFAULT_ALIGNMENT);
}

void *arena_alloc_zeroed(Arena *arena, uint64_t size) {
  void *ptr = arena_alloc(arena, size);
  if (ptr)
    memset(ptr, 0, size);
  return ptr;
}

void *arena_calloc(Arena *arena, uint64_t count, uint64_t size) {
  uint64_t total = count * size;
  return arena_alloc_zeroed(arena, total);
}

void *arena_realloc(Arena *arena, void *ptr, uint64_t old_size,
                    uint64_t new_size) {
  if (!arena || !ptr || new_size == 0)
    return NULL;

  ArenaBlock *block = arena->head;
  uintptr_t last_byte = (uintptr_t)block->buffer + block->used;
  uintptr_t ptr_end = (uintptr_t)ptr + old_size;

  if (ptr_end == last_byte) {
    uint64_t new_end = ((uintptr_t)ptr - (uintptr_t)block->buffer) + new_size;
    if (new_end <= block->capacity) {
      block->used = new_end;
      return ptr;
    }
  }

  void *new_ptr = arena_alloc(arena, new_size);
  if (new_ptr) {
    memcpy(new_ptr, ptr, old_size < new_size ? old_size : new_size);
  }
  return new_ptr;
}

// ------------------------
// Strings
// ------------------------
char *arena_strdup(Arena *arena, const char *str) {
  if (!str)
    return NULL;
  size_t len = strlen(str) + 1;
  char *dup = arena_alloc(arena, len);
  if (dup)
    memcpy(dup, str, len);
  return dup;
}

char *arena_strndup(Arena *arena, const char *str, uint64_t n) {
  if (!str)
    return NULL;
  size_t len = strlen(str);
  if (len > n)
    len = n;

  char *dup = arena_alloc(arena, len + 1);
  if (dup) {
    memcpy(dup, str, len);
    dup[len] = '\0';
  }
  return dup;
}

// ------------------------
// Temporary scope
// ------------------------
ArenaTemp arena_temp_begin(Arena *arena) {
  ArenaTemp temp = {arena, arena && arena->head ? arena->head->used : 0};
  return temp;
}

void arena_temp_end(ArenaTemp temp) {
  if (temp.arena && temp.arena->head)
    temp.arena->head->used = temp.used;
}

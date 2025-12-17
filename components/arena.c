#include "arena.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#ifndef ARENA_DEFAULT_ALIGNMENT
#define ARENA_DEFAULT_ALIGNMENT 8
#endif

#ifndef ARENA_MAX_ALIGNMENT
#define ARENA_MAX_ALIGNMENT 16
#endif

// Helper function to align pointer
static inline uint64_t align_forward(uint64_t ptr, uint64_t alignment) {
    return (ptr + (alignment - 1)) & ~(alignment - 1);
}

// Helper function to align pointer (for uintptr_t)
static inline uintptr_t align_forward_ptr(uintptr_t ptr, uint64_t alignment) {
    return (ptr + (alignment - 1)) & ~(alignment - 1);
}

Arena arena_create(uint64_t capacity) {
    return arena_create_with_alignment(capacity, ARENA_DEFAULT_ALIGNMENT);
}

Arena arena_create_with_alignment(uint64_t capacity, uint64_t alignment) {
    Arena arena = {0};

    if (alignment == 0 || (alignment & (alignment - 1)) != 0)
        alignment = ARENA_DEFAULT_ALIGNMENT;

    if (alignment > ARENA_MAX_ALIGNMENT)
        alignment = ARENA_MAX_ALIGNMENT;

    void *mem = NULL;
    if (posix_memalign(&mem, alignment, capacity) != 0)
        return arena;

    arena.buffer    = (uint8_t*)mem;
    arena.capacity  = capacity;
    arena.used      = 0;
    arena.commit    = capacity;
    arena.alignment = alignment;
    arena.max_align = ARENA_MAX_ALIGNMENT;

    return arena;
}


bool arena_init(Arena *arena, void *buffer, uint64_t capacity) {
    if (!arena || !buffer) return false;
    
    arena->buffer = (uint8_t*)buffer;
    arena->capacity = capacity;
    arena->used = 0;
    arena->commit = capacity;
    arena->alignment = ARENA_DEFAULT_ALIGNMENT;
    arena->max_align = ARENA_MAX_ALIGNMENT;
    
    return true;
}

void arena_destroy(Arena *arena) {
    if (arena && arena->buffer) {
        free(arena->buffer);
        arena->buffer = NULL;
        arena->capacity = 0;
        arena->used = 0;
        arena->commit = 0;
    }
}

void arena_reset(Arena *arena) {
    if (arena) {
        arena->used = 0;
    }
}

uint64_t arena_used(const Arena *arena) {
    return arena ? arena->used : 0;
}

uint64_t arena_remaining(const Arena *arena) {
    if (!arena) return 0;
    return arena->capacity - arena->used;
}

void *arena_alloc(Arena *arena, uint64_t size) {
    return arena_alloc_aligned(arena, size, arena ? arena->alignment : ARENA_DEFAULT_ALIGNMENT);
}

void *arena_alloc_aligned(Arena *arena, uint64_t size, uint64_t alignment) {
    if (!arena || size == 0) return NULL;
    
    // Ensure alignment is power of two
    if (alignment == 0 || (alignment & (alignment - 1)) != 0) {
        alignment = arena->alignment;
    }
    
    // Cap alignment to max_align to prevent excessive padding
    if (alignment > arena->max_align) {
        alignment = arena->max_align;
    }
    
    // Calculate aligned offset
    uintptr_t current_ptr = (uintptr_t)arena->buffer + arena->used;
    uintptr_t aligned_ptr = align_forward_ptr(current_ptr, alignment);
    size_t alignment_padding = aligned_ptr - current_ptr;
    
    // Check if we have enough space
    if (arena->used + alignment_padding + size > arena->capacity) {
        return NULL; // Out of memory
    }
    
    void *result = (void*)aligned_ptr;
    arena->used += alignment_padding + size;
    
    return result;
}

void *arena_alloc_zeroed(Arena *arena, uint64_t size) {
    void *ptr = arena_alloc(arena, size);
    if (ptr) {
        memset(ptr, 0, size);
    }
    return ptr;
}

void *arena_realloc(Arena *arena, void *ptr, uint64_t old_size, uint64_t new_size) {
    if (!arena || !ptr || new_size == 0) return NULL;
    
    // If the pointer is the last allocation, we can try to resize in place
    uintptr_t last_byte = (uintptr_t)arena->buffer + arena->used;
    uintptr_t ptr_end = (uintptr_t)ptr + old_size;
    
    if (ptr_end == last_byte) {
        // This is the last allocation, check if we can extend it
        uint64_t current_end = arena->used;
        uint64_t new_end = ((uintptr_t)ptr - (uintptr_t)arena->buffer) + new_size;
        
        if (new_end <= arena->capacity) {
            arena->used = new_end;
            return ptr;
        }
    }
    
    // Otherwise, allocate new memory and copy
    void *new_ptr = arena_alloc(arena, new_size);
    if (new_ptr) {
        uint64_t copy_size = old_size < new_size ? old_size : new_size;
        memcpy(new_ptr, ptr, copy_size);
    }
    
    return new_ptr;
}

void *arena_calloc(Arena *arena, uint64_t count, uint64_t size) {
    uint64_t total_size = count * size;
    void *ptr = arena_alloc(arena, total_size);
    if (ptr) {
        memset(ptr, 0, total_size);
    }
    return ptr;
}

char *arena_strdup(Arena *arena, const char *str) {
    if (!arena || !str) return NULL;
    
    uint64_t len = strlen(str) + 1;
    char *dup = (char*)arena_alloc(arena, len);
    if (dup) {
        memcpy(dup, str, len);
    }
    return dup;
}

char *arena_strndup(Arena *arena, const char *str, uint64_t n) {
    if (!arena || !str) return NULL;
    
    uint64_t len = strlen(str);
    if (len > n) len = n;
    
    char *dup = (char*)arena_alloc(arena, len + 1);
    if (dup) {
        memcpy(dup, str, len);
        dup[len] = '\0';
    }
    return dup;
}

ArenaTemp arena_temp_begin(Arena *arena) {
    ArenaTemp temp = {arena, arena ? arena->used : 0};
    return temp;
}

void arena_temp_end(ArenaTemp temp) {
    if (temp.arena) {
        temp.arena->used = temp.used;
    }
}

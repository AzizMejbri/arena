# Arena Allocator (C)

A **high-performance multi-block arena (region) allocator** written in C, designed for **fast, predictable memory allocation** with minimal overhead and automatic growth.

Ideal for workloads with clear allocation lifetimes such as **thread pools, HTTP servers, parsers, game engines, compilers, and batch systems**.

This allocator prioritizes:
* 🚀 **Speed**
* 🧠 **Deterministic memory usage**
* 🧹 **O(1) bulk deallocation**
* ⚙️ **Alignment-aware allocations**
* 📈 **Automatic capacity growth**

---

## ✨ Features

### Core Functionality
* **Linear (bump) allocation** within blocks
* **Multi-block architecture** for automatic growth
* **Custom alignment support** (with max_align capping)
* **Zeroed allocation**
* **Reallocation within arena**
* **String duplication** (with length-limited variant)
* **Temporary allocation scopes** for easy rollback
* **Full arena reset** across all blocks
* **Graceful scaling** when capacity is exceeded

### Safety & Correctness
* Power-of-two alignment enforcement
* Alignment clamping via `max_align`
* Robust NULL and edge-case handling
* Deterministic behavior under fuzzing
* Tested against use-after-free patterns (arena semantics)
* Memory usage tracking across all blocks

### Performance-Focused Design
* No per-allocation metadata
* No free lists
* No fragmentation within blocks
* Cache-friendly sequential memory access
* Efficient block reuse on reset
* Grows only when needed

---

## 🧩 API Overview

### Arena Creation & Destruction
```c
Arena arena_create(uint64_t capacity);
Arena arena_create_with_alignment(uint64_t capacity, uint64_t alignment);
void arena_destroy(Arena *arena);
```

**Note**: The arena automatically grows by allocating new blocks when capacity is exceeded. Initial capacity determines the first block size.

### Memory Allocation
```c
void *arena_alloc(Arena *arena, uint64_t size);
void *arena_alloc_aligned(Arena *arena, uint64_t size, uint64_t alignment);
void *arena_alloc_zeroed(Arena *arena, uint64_t size);
void *arena_calloc(Arena *arena, uint64_t count, uint64_t size);
```

### Reallocation
```c
void *arena_realloc(Arena *arena, void *old_ptr, uint64_t old_size, uint64_t new_size);
```

**Note**: Reallocation may copy data if the allocation doesn't fit in the current position.

### String Utilities
```c
char *arena_strdup(Arena *arena, const char *str);
char *arena_strndup(Arena *arena, const char *str, uint64_t n);
```

### Memory Usage Tracking
```c
uint64_t arena_used(const Arena *arena);
uint64_t arena_remaining(const Arena *arena);
```

These functions provide insight into total memory usage across all blocks and remaining space in the current block.

### Lifetime Control
```c
void arena_reset(Arena *arena);
ArenaTemp arena_temp_begin(Arena *arena);
void arena_temp_end(ArenaTemp temp);
```

**Temporary scopes** allow rollback of allocations without destroying the arena. Perfect for parsing, intermediate computations, or request handling.

---

## 🏗️ Multi-Block Architecture

The arena uses a **linked list of blocks** to support automatic growth:

```
Arena
  ├─→ Block 1 (1024 bytes) [current]
  ├─→ Block 2 (2048 bytes)
  └─→ Block 3 (4096 bytes)
```

### How It Works
1. Initial capacity determines first block size
2. When a block fills up, a new block is allocated
3. New blocks are sized to fit the allocation (or double previous size)
4. `arena_reset()` reuses existing blocks
5. `arena_destroy()` frees all blocks

### Benefits
* **No hard capacity limit** - arena grows as needed
* **Efficient memory reuse** - reset preserves allocated blocks
* **Predictable growth** - new blocks sized appropriately
* **Zero fragmentation** - within each block

---

## 🧠 Arena vs `malloc` / `free`

### Why use an arena?

Traditional heap allocation (`malloc`/`free`) is:
* Slower due to bookkeeping
* Prone to fragmentation
* Non-deterministic in performance
* Expensive under high allocation rates

An arena allocator:
* Allocates memory with a **single pointer increment**
* Frees memory **all at once**
* Has **zero per-allocation overhead**
* Exhibits **predictable performance**
* **Automatically grows** without manual resizing

---

## 📊 Benchmarks

Benchmarks were run with:
* `-O3 -march=native -flto -DNDEBUG`
* Linux (glibc)
* 1,000,000 allocations per test

### Allocation Throughput
```
Arena time:   0.1080 seconds
Malloc time:  0.1759 seconds
Speedup:      1.63× faster
```

### Batch Allocation with Resets
```
Arena

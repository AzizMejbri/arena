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
Arena + reset:   0.0432 seconds
Malloc/free:     0.0860 seconds
Speedup:         1.99× faster
```

### Fuzz Testing
```
~15,000,000 random operations in 5 seconds
0 failures
0 memory issues
100% test pass rate
```

**Note**: Performance improvements are more dramatic with:
* Smaller allocation sizes
* Higher allocation frequency
* More frequent batch deallocations

---

## 💡 Usage Examples

### Basic Usage
```c
// Create arena with initial capacity
Arena arena = arena_create(4096);

// Allocate memory
int *numbers = arena_alloc(&arena, 100 * sizeof(int));
char *string = arena_strdup(&arena, "Hello, World!");

// Use memory...

// Free everything at once
arena_destroy(&arena);
```

### Temporary Scopes
```c
Arena arena = arena_create(4096);

// Persistent allocation
Config *config = arena_alloc(&arena, sizeof(Config));

// Temporary work
ArenaTemp temp = arena_temp_begin(&arena);
Buffer *scratch = arena_alloc(&arena, 1024);
// ... use scratch buffer ...
arena_temp_end(temp); // Rollback scratch allocation

// config is still valid
arena_destroy(&arena);
```

### Request Handler Pattern
```c
Arena arena = arena_create(8192);

while (request = get_next_request()) {
    // Allocate request-specific data
    Response *response = process_request(&arena, request);
    send_response(response);
    
    // Clear for next request
    arena_reset(&arena);
}

arena_destroy(&arena);
```

### Custom Alignment
```c
// Create arena with 64-byte alignment
Arena arena = arena_create_with_alignment(4096, 64);

// Allocations will be 64-byte aligned (up to max_align)
void *aligned_data = arena_alloc(&arena, 256);

arena_destroy(&arena);
```

---

## ⚠️ Important Semantics

This arena **does NOT**:
* Detect buffer overflows
* Protect against out-of-bounds writes
* Provide individual `free()` calls
* Shrink allocated blocks

These are **intentional design choices** for performance.

### What You Get Instead
* **Bulk deallocation** via `arena_reset()` or `arena_destroy()`
* **Automatic growth** - never fails due to capacity
* **Temporary scopes** for fine-grained lifetime control
* **Memory tracking** to monitor usage

If you need:
* Guard zones
* Canary checks
* Debug memory poisoning

→ Implement them in a **debug wrapper**, not in the fast path.

---

## 🧪 Testing

The arena is tested with:
* ✅ Basic allocation tests
* ✅ Alignment verification tests
* ✅ Multi-block growth tests
* ✅ Edge-case and NULL handling
* ✅ Temporary scope tests
* ✅ String duplication tests
* ✅ Memory tracking tests
* ✅ Fuzz testing (15M+ operations)
* ✅ Performance benchmarks

**All tests pass with 100% success rate.**

---

## 🛠️ Installation

### Build and Install
```bash
make install
```

This:
- Builds `libarena.a` and copies it to `/usr/local/lib/`
- Copies `arena.h` to `/usr/local/include/`

### Usage in Your Project
```c
#include <arena.h>
```

Link with:
```bash
gcc your_code.c -larena -o your_program
```

Or with static linking:
```bash
gcc your_code.c /usr/local/lib/libarena.a -o your_program
```

---

## 🎯 When to Use This Arena

### Perfect For ✅
* Request/response servers (HTTP, RPC)
* Parsers and compilers (AST construction)
* Game engines (per-frame allocations)
* Batch processing systems
* Data structure building (temporary graphs, trees)
* Thread-local allocators
* Any workload with clear allocation phases

### Not Ideal For ❌
* Long-lived objects with mixed lifetimes
* Applications requiring individual frees
* Memory-constrained embedded systems (without growth limits)
* General-purpose allocator replacement

---

## 🔧 Configuration

### Alignment
The arena supports custom alignment with automatic capping:
* Default alignment: typically 8 or 16 bytes
* Maximum alignment (`max_align`): typically 16 bytes
* Requested alignments > `max_align` are clamped

### Block Growth Strategy
* First block: uses specified initial capacity
* Subsequent blocks: sized to fit allocation or 2× previous capacity
* Reset: preserves allocated blocks for reuse

---

## 📜 License

MIT License — free to use in commercial and open-source projects.

---

## 🏁 Summary

This multi-block arena allocator is:
* ✅ **Fast** - 1.6-2× faster than malloc/free
* ✅ **Simple** - clean API with minimal configuration
* ✅ **Deterministic** - predictable performance characteristics
* ✅ **Well-tested** - 100% pass rate across all tests
* ✅ **Scalable** - automatic growth without manual intervention
* ✅ **Production-ready** - battle-tested design patterns

Perfect for high-performance systems where allocation patterns are well-defined.

**If you're still calling `malloc` in hot paths — this is your upgrade.**

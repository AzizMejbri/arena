
# Arena Allocator (C)

A **high-performance arena (region) allocator** written in C, designed for **fast, predictable memory allocation** with minimal overhead.
Ideal for workloads with clear allocation lifetimes such as **thread pools, HTTP servers, parsers, game engines, compilers, and batch systems**.

This allocator prioritizes:

* 🚀 **Speed**
* 🧠 **Deterministic memory usage**
* 🧹 **O(1) bulk deallocation**
* ⚙️ **Alignment-aware allocations**

---

## ✨ Features

### Core Functionality

* **Linear (bump) allocation**
* **Custom alignment support**
* **Zeroed allocation**
* **Reallocation within arena**
* **String duplication**
* **Temporary allocation scopes**
* **Full arena reset**
* **Graceful out-of-memory handling**

### Safety & Correctness

* Power-of-two alignment enforcement
* Alignment clamping via `max_align`
* Robust NULL and edge-case handling
* Deterministic behavior under fuzzing
* Tested against use-after-free patterns (arena semantics)

### Performance-Focused Design

* No per-allocation metadata
* No free lists
* No fragmentation
* Cache-friendly sequential memory access

---

## 🧩 API Overview

### Arena Creation

```c
Arena arena_create(uint64_t capacity);
Arena arena_create_with_alignment(uint64_t capacity, uint64_t alignment);
void arena_destroy(Arena *arena);
```

### Allocation

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

### Strings

```c
char *arena_strdup(Arena *arena, const char *str);
```

### Lifetime Control

```c
void arena_reset(Arena *arena);

ArenaTemp arena_temp_begin(Arena *arena);
void arena_temp_end(ArenaTemp temp);
```

Temporary scopes allow rollback of allocations without destroying the arena.

---

## 🧠 Arena vs `malloc` / `free`

### Why use an arena?

Traditional heap allocation (`malloc/free`) is:

* Slower due to bookkeeping
* Prone to fragmentation
* Non-deterministic in performance
* Expensive under high allocation rates

An arena allocator:

* Allocates memory with a **single pointer increment**
* Frees memory **all at once**
* Has **zero per-allocation overhead**
* Exhibits **predictable performance**

---

## 📊 Benchmarks

Benchmarks were run with:

* `-O3 -march=native -flto -DNDEBUG`
* Linux (glibc)
* Millions of allocations

### Allocation Throughput

```
Arena time:   ~0.033 seconds
Malloc time:  ~0.099 seconds
Speedup:      ~3.0× faster
```

### Batch Allocation with Resets

```
Arena + reset:   ~0.034 seconds
Malloc/free:     ~0.072 seconds
Speedup:         ~2.1× faster
```

### Fuzz Testing

```
~47,000,000 random operations
0 failures
0 memory issues
```

---

## ⚠️ Important Semantics

This arena **does NOT**:

* Detect buffer overflows
* Protect against out-of-bounds writes
* Provide individual `free()` calls

These are **intentional design choices** for performance.

If you need:

* Guard zones
* Canary checks
* Debug memory poisoning

→ implement them in a **debug wrapper**, not in the fast path.

---

## 🧪 Testing

The arena is tested with:

* Unit tests
* Edge-case tests
* Alignment tests
* Fuzz testing
* Performance benchmarks

All tests pass with **100% success rate**.

---

## 🛠️ Installation
Build:

```bash
make install
```

This:

- builds libarena.a and copies it to /usr/local/lib/
- copies arena.h to /usr/local/include/


To use afterwards include headers:

```c
#include <arena.h>
```

---

## 📜 License

MIT License — free to use in commercial and open-source projects.

---

## 🏁 Summary

This arena allocator is:

* **Fast**
* **Simple**
* **Deterministic**
* **Well-tested**

Perfect for high-performance systems where allocation patterns are well-defined.

If you’re still calling `malloc` in hot paths — this is your upgrade.

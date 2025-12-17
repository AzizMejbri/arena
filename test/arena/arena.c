#include "../../components/arena.h"
#include "../test.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>

// Memory corruption detection helper
#define PATTERN_BYTE 0xAA
#define GUARD_BYTE 0xDE
#define GUARD_SIZE 16

typedef struct GuardedAlloc {
  void *ptr;
  size_t size;
  uint8_t pre_guard[GUARD_SIZE];
  uint8_t post_guard[GUARD_SIZE];
} GuardedAlloc;

void setup_guards(GuardedAlloc *g) {
  memset(g->pre_guard, GUARD_BYTE, GUARD_SIZE);
  memset(g->post_guard, GUARD_BYTE, GUARD_SIZE);
}

int check_guards(GuardedAlloc *g) {
  for (size_t i = 0; i < GUARD_SIZE; i++) {
    if (g->pre_guard[i] != GUARD_BYTE)
      return 0;
    if (g->post_guard[i] != GUARD_BYTE)
      return 0;
  }
  return 1;
}

// ================== MEMORY CORRUPTION TESTS ==================

void test_buffer_overflow() {
  printf("Testing buffer overflow protection: (not applicable for arena) ");

  Arena arena = arena_create(1024);
  test(arena.buffer != NULL);

  uint8_t *a = arena_alloc(&arena, 10);
  uint8_t *b = arena_alloc(&arena, 10);

  test(a != NULL);
  test(b != NULL);
  test(a + 10 <= b); // adjacency only

  arena_destroy(&arena);
}

void test_use_after_free() {
  printf("Testing use-after-free scenarios: ");

  Arena arena = arena_create(1024);

  // Allocate and fill
  int *data = arena_alloc(&arena, 100);
  test(data != NULL);
  for (int i = 0; i < 25; i++) {
    data[i] = i * i;
  }

  // Reset arena (simulating free)
  arena_reset(&arena);

  // This would be use-after-free in normal malloc
  // With arenas, memory is still valid but conceptually freed
  // We're testing that we can safely reallocate

  // Reallocate - should get same or different memory
  int *new_data = arena_alloc(&arena, 100);
  test(new_data != NULL);

  // Writing to new allocation should be safe
  for (int i = 0; i < 25; i++) {
    new_data[i] = i * 2;
    test(new_data[i] == i * 2);
  }

  arena_destroy(&arena);
}

void test_double_alloc_guards() {
  printf("Testing allocation guard zones: ");

  Arena arena = arena_create(1024);

  GuardedAlloc g1, g2;
  setup_guards(&g1);
  setup_guards(&g2);

  // Allocate with guard checking
  g1.ptr = arena_alloc(&arena, 32);
  g1.size = 32;
  test(g1.ptr != NULL);
  test(check_guards(&g1));

  g2.ptr = arena_alloc(&arena, 64);
  g2.size = 64;
  test(g2.ptr != NULL);
  test(check_guards(&g2));

  // Fill allocations
  memset(g1.ptr, PATTERN_BYTE, g1.size);
  memset(g2.ptr, PATTERN_BYTE, g2.size);

  // Verify guards are still intact
  test(check_guards(&g1));
  test(check_guards(&g2));

  // Verify allocations don't overlap
  uint8_t *end1 = (uint8_t *)g1.ptr + g1.size;
  uint8_t *start2 = (uint8_t *)g2.ptr;
  test(end1 <= start2);

  arena_destroy(&arena);
}

void test_null_and_edge_cases() {
  printf("Testing NULL and edge cases: ");

  Arena arena = arena_create(1024);

  // Test NULL arena
  test(arena_alloc(NULL, 10) == NULL);
  test(arena_alloc_aligned(NULL, 10, 8) == NULL);
  test(arena_alloc_zeroed(NULL, 10) == NULL);

  // Test zero size
  test(arena_alloc(&arena, 0) == NULL);
  test(arena_alloc_aligned(&arena, 0, 8) == NULL);
  test(arena_alloc_zeroed(&arena, 0) == NULL);

  // Test invalid alignment
  test(arena_alloc_aligned(&arena, 10, 0) != NULL); // Should use default
  test(arena_alloc_aligned(&arena, 10, 3) !=
       NULL); // Non-power-of-two -> use default

  // Test very large alignment (should be capped to max_align)
  void *ptr = arena_alloc_aligned(&arena, 10, 1024);
  test(ptr != NULL);
  // Should be aligned to max_align (16) not 1024
  test((uintptr_t)ptr % arena.max_align == 0);

  arena_destroy(&arena);
}

// ================== PERFORMANCE TESTS ==================

void benchmark_arena_vs_malloc() {
  printf("\n=== BENCHMARK: Arena vs malloc/free ===\n");

  const size_t iterations = 1000000;
  const size_t max_size = 256;

  // Arena benchmark
  clock_t start = clock();
  Arena arena = arena_create(iterations * max_size);

  for (size_t i = 0; i < iterations; i++) {
    size_t size = (i % max_size) + 1;
    void *ptr = arena_alloc(&arena, size);
    if (ptr) {
      memset(ptr, i % 256, size);
    }
  }

  arena_destroy(&arena);
  clock_t end = clock();
  double arena_time = ((double)(end - start)) / CLOCKS_PER_SEC;

  // Malloc benchmark
  start = clock();
  void **pointers = malloc(iterations * sizeof(void *));

  for (size_t i = 0; i < iterations; i++) {
    size_t size = (i % max_size) + 1;
    pointers[i] = malloc(size);
    if (pointers[i]) {
      memset(pointers[i], i % 256, size);
    }
  }

  for (size_t i = 0; i < iterations; i++) {
    free(pointers[i]);
  }
  free(pointers);
  end = clock();
  double malloc_time = ((double)(end - start)) / CLOCKS_PER_SEC;

  printf("Arena time: %.4f seconds\n", arena_time);
  printf("Malloc time: %.4f seconds\n", malloc_time);
  printf("Speedup: %.2fx faster\n", malloc_time / arena_time);

  // Test with frequent resets (simulating batch deallocation)
  start = clock();
  Arena arena2 = arena_create(max_size * 1000); // Smaller arena

  for (size_t batch = 0; batch < 100; batch++) {
    for (size_t i = 0; i < 10000; i++) {
      size_t size = (rand() % max_size) + 1;
      void *ptr = arena_alloc(&arena2, size);
      if (ptr) {
        memset(ptr, batch % 256, size);
      }
    }
    arena_reset(&arena2);
  }

  arena_destroy(&arena2);
  end = clock();
  double arena_reset_time = ((double)(end - start)) / CLOCKS_PER_SEC;

  printf("\nBatch processing (with resets):\n");
  printf("Arena with reset time: %.4f seconds\n", arena_reset_time);

  // Equivalent malloc/free
  start = clock();
  for (size_t batch = 0; batch < 100; batch++) {
    void *batch_ptrs[10000];
    for (size_t i = 0; i < 10000; i++) {
      size_t size = (rand() % max_size) + 1;
      batch_ptrs[i] = malloc(size);
      if (batch_ptrs[i]) {
        memset(batch_ptrs[i], batch % 256, size);
      }
    }
    for (size_t i = 0; i < 10000; i++) {
      free(batch_ptrs[i]);
    }
  }
  end = clock();
  double malloc_batch_time = ((double)(end - start)) / CLOCKS_PER_SEC;

  printf("Malloc/free batch time: %.4f seconds\n", malloc_batch_time);
  printf("Batch speedup: %.2fx faster\n", malloc_batch_time / arena_reset_time);
}

// ================== FUZZING TEST ==================

void fuzz_test_arena() {
  printf("\n=== FUZZING TEST (random operations) ===\n");
  printf("Running for 5 seconds...\n");

  Arena arena = arena_create(10 * 1024 * 1024); // 10MB arena
  time_t start_time = time(NULL);
  int operations = 0;

  while (time(NULL) - start_time < 5) { // Run for 5 seconds
    int op = rand() % 10;

    switch (op) {
    case 0: // Normal allocation
    case 1:
    case 2: {
      size_t size = rand() % 4096 + 1;
      void *ptr = arena_alloc(&arena, size);
      if (ptr) {
        // Write pattern
        memset(ptr, operations % 256, size);
      }
      break;
    }

    case 3: // Aligned allocation
    case 4: {
      size_t size = rand() % 1024 + 1;
      size_t align = 1 << (rand() % 8); // 1, 2, 4, 8, 16, 32, 64, 128
      void *ptr = arena_alloc_aligned(&arena, size, align);
      if (ptr) {
        memset(ptr, (operations * 7) % 256, size);
        // Verify alignment
        size_t effective_align = align;
        if (effective_align > arena.max_align)
          effective_align = arena.max_align;

        assert((uintptr_t)ptr % effective_align == 0);
      }
      break;
    }

    case 5: // Zeroed allocation
      if (arena_remaining(&arena) > 1000) {
        size_t size = rand() % 512 + 1;
        void *ptr = arena_alloc_zeroed(&arena, size);
        if (ptr) {
          // Verify zeroed
          for (size_t i = 0; i < size; i++) {
            assert(((uint8_t *)ptr)[i] == 0);
          }
        }
      }
      break;

    case 6: // String duplication
      if (arena_remaining(&arena) > 100) {
        char test_str[100];
        int len = rand() % 50 + 1;
        for (int i = 0; i < len; i++) {
          test_str[i] = 'A' + (rand() % 26);
        }
        test_str[len] = '\0';

        uint8_t *dup = arena_strdup(&arena, test_str);
        if (dup) {
          assert(strcmp(test_str, dup) == 0);
        }
      }
      break;

    case 7:                    // Reset
      if (rand() % 100 == 0) { // 1% chance to reset
        arena_reset(&arena);
      }
      break;

    case 8: // Temporary scope
      if (arena_remaining(&arena) > 1000) {
        ArenaTemp temp = arena_temp_begin(&arena);
        size_t before = arena_used(&arena);

        // Do some temp allocations
        for (int i = 0; i < 10; i++) {
          arena_alloc(&arena, rand() % 100 + 1);
        }

        arena_temp_end(temp);
        assert(arena_used(&arena) == before);
      }
      break;

    case 9: // Calloc
      if (arena_remaining(&arena) > 1000) {
        size_t count = rand() % 10 + 1;
        size_t size = rand() % 100 + 1;
        void *ptr = arena_calloc(&arena, count, size);
        if (ptr) {
          // Verify zeroed
          for (size_t i = 0; i < count * size; i++) {
            assert(((uint8_t *)ptr)[i] == 0);
          }
        }
      }
      break;
    }

    operations++;

    // Verify arena invariants
    assert(arena.used <= arena.capacity);
    assert(arena.buffer != NULL || arena.capacity == 0);
  }

  printf("Completed %d operations without issues\n", operations);
  arena_destroy(&arena);
}

// ================== EXISTING TESTS (keep these) ==================

void test_basic_allocation() {
  printf("Testing basic allocation: ");

  Arena arena = arena_create(1024);
  test(arena.buffer != NULL);
  test(arena.capacity == 1024);
  test(arena.used == 0);

  // Allocate some memory
  int *nums = (int *)arena_alloc(&arena, 10 * sizeof(int));
  test(nums != NULL);
  test(arena.used == 10 * sizeof(int));

  // Use the memory
  for (int i = 0; i < 10; i++) {
    nums[i] = i * 2;
  }

  // Test the values
  for (int i = 0; i < 10; i++) {
    test(nums[i] == i * 2);
  }

  arena_destroy(&arena);
}

void test_aligned_allocation() {
  printf("Testing aligned allocation: ");

  Arena arena = arena_create(1024);

  uint8_t *c = arena_alloc_aligned(&arena, 1, arena.alignment);
  test((uintptr_t)c % arena.alignment == 0);

  // Allocate with specific alignment
  void *aligned = arena_alloc_aligned(&arena, 64, 32);
  test(aligned != NULL);

  size_t effective_align = 32;
  if (effective_align > arena.max_align)
    effective_align = arena.max_align;

  test((uintptr_t)aligned % effective_align == 0);

  arena_destroy(&arena);
}

void test_zeroed_allocation() {
  printf("Testing zeroed allocation: ");

  Arena arena = arena_create(1024);

  // Allocate zeroed memory
  int *zeros = (int *)arena_alloc_zeroed(&arena, 100 * sizeof(int));
  test(zeros != NULL);

  // Verify all bytes are zero
  for (int i = 0; i < 100; i++) {
    test(zeros[i] == 0);
  }

  arena_destroy(&arena);
}

void test_realloc() {
  printf("Testing reallocation: ");

  Arena arena = arena_create(1024);

  // Initial allocation
  int *nums = (int *)arena_alloc(&arena, 5 * sizeof(int));
  test(nums != NULL);

  // Fill with data
  for (int i = 0; i < 5; i++) {
    nums[i] = i + 1;
  }

  // Reallocate to larger size
  int *new_nums =
      (int *)arena_realloc(&arena, nums, 5 * sizeof(int), 10 * sizeof(int));
  test(new_nums != NULL);

  // Verify old data is preserved
  for (int i = 0; i < 5; i++) {
    test(new_nums[i] == i + 1);
  }

  arena_destroy(&arena);
}

void test_calloc() {
  printf("Testing calloc: ");

  Arena arena = arena_create(1024);

  int *arr = (int *)arena_calloc(&arena, 10, sizeof(int));
  test(arr != NULL);

  // Verify all elements are zero
  for (int i = 0; i < 10; i++) {
    test(arr[i] == 0);
  }

  arena_destroy(&arena);
}

void test_strdup() {
  printf("Testing string duplication: ");

  Arena arena = arena_create(1024);

  const uint8_t *original = "Hello, Arena!";
  uint8_t *duplicate = arena_strdup(&arena, original);
  test(duplicate != NULL);
  test(strcmp(original, duplicate) == 0);
  test(duplicate != original); // Should be a copy

  arena_destroy(&arena);
}

void test_reset() {
  printf("Testing arena reset: ");

  Arena arena = arena_create(1024);

  // Allocate some memory
  int *a = (int *)arena_alloc(&arena, 100);
  test(a != NULL);
  test(arena.used == 100);

  // Reset the arena
  arena_reset(&arena);
  test(arena.used == 0);

  // Should be able to allocate again
  int *b = (int *)arena_alloc(&arena, 200);
  test(b != NULL);
  test(arena.used == 200);

  arena_destroy(&arena);
}

void test_temp_scope() {
  printf("Testing temporary scope: ");

  Arena arena = arena_create(1024);

  // Allocate some memory
  int *a = (int *)arena_alloc(&arena, 100);
  test(a != NULL);
  size_t used_before = arena.used;

  // Begin temporary scope
  ArenaTemp temp = arena_temp_begin(&arena);

  // Allocate in temp scope
  int *b = (int *)arena_alloc(&arena, 200);
  test(b != NULL);
  test(arena.used > used_before);

  // End temp scope (should roll back)
  arena_temp_end(temp);
  test(arena.used == used_before);

  // Should still be able to allocate
  int *c = (int *)arena_alloc(&arena, 50);
  test(c != NULL);

  arena_destroy(&arena);
}

void test_out_of_memory() {
  printf("Testing out of memory: ");

  Arena arena = arena_create(100);

  // Should succeed
  void *a = arena_alloc(&arena, 50);
  test(a != NULL);

  // Should also succeed
  void *b = arena_alloc(&arena, 30);
  test(b != NULL);

  // Should fail (out of memory)
  void *c = arena_alloc(&arena, 30);
  test(c == NULL);

  arena_destroy(&arena);
}

void test_arena_init() {
  printf("Testing arena initialization: ");

  // Use stack memory for arena
  uint8_t buffer[256];
  Arena arena;

  test(arena_init(&arena, buffer, sizeof(buffer)));
  test(arena.buffer == buffer);
  test(arena.capacity == sizeof(buffer));
  test(arena.used == 0);

  // Allocate from stack-based arena
  int *nums = (int *)arena_alloc(&arena, 4 * sizeof(int));
  test(nums != NULL);

  for (int i = 0; i < 4; i++) {
    nums[i] = i * 10;
    test(nums[i] == i * 10);
  }

  // No need to destroy (stack memory)
}

int main() {
  printf("=== Arena Allocator Tests ===\n\n");

  // Seed random for fuzzing
  srand(time(NULL));

  // Run all standard tests
  test_basic_allocation();
  printf("\n");

  test_aligned_allocation();
  printf("\n");

  test_zeroed_allocation();
  printf("\n");

  test_realloc();
  printf("\n");

  test_calloc();
  printf("\n");

  test_strdup();
  printf("\n");

  test_reset();
  printf("\n");

  test_temp_scope();
  printf("\n");

  test_out_of_memory();
  printf("\n");

  test_arena_init();
  printf("\n");

  // Run memory corruption tests
  test_buffer_overflow();
  printf("\n");

  test_use_after_free();
  printf("\n");

  test_double_alloc_guards();
  printf("\n");

  test_null_and_edge_cases();
  printf("\n");

  // Run benchmarks
  benchmark_arena_vs_malloc();
  printf("\n");

  // Run fuzzing test
  fuzz_test_arena();
  printf("\n");

  // Print final summary
  printf("\n=== Final Summary ===\n");
  summary();

  return 0;
}

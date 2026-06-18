#pragma once
#include <assert.h>
#include <stdint.h>
#include <sys/mman.h>

#include <vector>

template <typename K, typename V, K EMPTY, bool Identity = false>
class FlatHashMap {
 public:
  FlatHashMap(size_t capacity) : mask(capacity - 1) {
    assert((capacity & (capacity - 1)) == 0 && capacity > 0);
    table.resize(capacity, {EMPTY, V()});
  }

  void insert(K key, V value) {
    size_t idx = hash(key);
    while (table[idx].key != EMPTY && table[idx].key != key) {
      idx = (idx + 1) & mask;
    }
    table[idx] = {key, value};
  }

  V* find(K key) {
    size_t idx = hash(key);
    while (table[idx].key != EMPTY) {
      if (table[idx].key == key) {
        return &table[idx].value;
      }
      idx = (idx + 1) & mask;
    }
    return nullptr;
  }

  void erase(K key) {
    size_t i = hash(key);

    while (table[i].key != EMPTY) {
      if (table[i].key == key) break;
      i = (i + 1) & mask;
    }
    if (table[i].key == EMPTY) return;

    // Knuth's Algorithm R: when an element can't move, continue scanning
    // rather than stopping. Stopping early leaves holes that break
    // wrap-around probe chains (common with identity hash).
    table[i].key = EMPTY;
    size_t j = i;
    while (true) {
      j = (j + 1) & mask;
      if (table[j].key == EMPTY) return;
      size_t h = hash(table[j].key);
      // Move j to i if j's home h is NOT in (i, j] cyclically.
      if (((j - h) & mask) >= ((j - i) & mask)) {
        table[i] = table[j];
        table[j].key = EMPTY;
        i = j;
      }
    }
  }

  void hugepages() {
    madvise(table.data(), table.size() * sizeof(Entry), MADV_HUGEPAGE);
  }

  void prefetch(K key) const {
    __builtin_prefetch(&table[hash(key)], 0, 3);
  }

 private:
  struct Entry {
    K key;
    V value;
  };
  size_t mask;
  std::vector<Entry> table;
  uint64_t hash(K key) const {
    // Identity hashing (key & mask) suits dense, ~sequential keys like ITCH
    // order reference numbers: consecutive refs land in adjacent slots, so the
    // live working set is a small migrating band instead of being scattered
    // across the whole table — much better cache locality. Linear probing still
    // resolves the collisions from long-lived orders (refs differing by a
    // multiple of capacity). For general keys the Fibonacci multiply spreads
    // clustered keys out.
    if constexpr (Identity) {
      return key & mask;
    }
    return (key * 11400714819323198485ULL) & mask;
  }
};
#pragma once
#include <stdint.h>

#include <vector>

template <typename K, typename V, K EMPTY>
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

    while (true) {
      size_t j = (i + 1) & mask;
      if (table[j].key == EMPTY) {
        table[i].key = EMPTY;
        return;
      }
      size_t h = hash(table[j].key);
      if (((i - h) & mask) <= ((j - h) & mask)) {
        table[i] = table[j];
        i = j;
      } else {
        table[i].key = EMPTY;
        return;
      }
    }
  }

 private:
  struct Entry {
    K key;
    V value;
  };
  size_t mask;
  std::vector<Entry> table;
  uint64_t hash(K key) const { return (key * 11400714819323198485ULL) & mask; }
};
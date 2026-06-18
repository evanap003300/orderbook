#pragma once
#include <stdint.h>
#include <sys/mman.h>

#include <vector>

#include "itch.hpp"

// Sentinel index meaning "no node" (null pointer in index form).
constexpr uint32_t INVALID_INDEX = UINT32_MAX;

// A resting order plus its intrusive list links. next/prev are indices into
// the owning OrderPool, not pointers, so the storage stays one flat vector.
struct PoolNode {
  Order order;
  uint32_t next;
  uint32_t prev;
};

// Flat arena of order nodes shared across every OrderBook. Free slots are
// chained through their own `next` field (the "custom malloc" free-list), so
// freeing and reusing a slot is O(1) and never touches the heap.
class OrderPool {
 public:
  explicit OrderPool(size_t reserveCapacity = 0) {
    if (reserveCapacity) {
      nodes.reserve(reserveCapacity);
    }
  }

  // Stores `order` in a slot and returns its index. Reuses a freed slot if one
  // exists, otherwise grows the arena.
  uint32_t allocate(const Order& order) {
    uint32_t idx;
    if (freeHead != INVALID_INDEX) {
      idx = freeHead;
      freeHead = nodes[idx].next;
    } else {
      idx = static_cast<uint32_t>(nodes.size());
      nodes.emplace_back();
    }
    nodes[idx].order = order;
    nodes[idx].next = INVALID_INDEX;
    nodes[idx].prev = INVALID_INDEX;
    return idx;
  }

  // Returns a slot to the free-list. The caller must have already unlinked it
  // from its price level.
  void free(uint32_t idx) {
    nodes[idx].next = freeHead;
    freeHead = idx;
  }

  void hugepages() {
    madvise(nodes.data(), nodes.capacity() * sizeof(PoolNode), MADV_HUGEPAGE);
  }

  PoolNode& operator[](uint32_t idx) { return nodes[idx]; }
  const PoolNode& operator[](uint32_t idx) const { return nodes[idx]; }

 private:
  std::vector<PoolNode> nodes;
  uint32_t freeHead = INVALID_INDEX;
};

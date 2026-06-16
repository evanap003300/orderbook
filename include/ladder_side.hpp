#pragma once
#include <stdint.h>

#include <algorithm>
#include <utility>
#include <vector>

#include "order_pool.hpp"

// A price level: head and tail are pool indices. The orders at this level form
// a FIFO chain via PoolNode::next/prev. Empty when head == INVALID_INDEX.
struct Level {
  uint32_t head = INVALID_INDEX;
  uint32_t tail = INVALID_INDEX;
};

// One side (bids or asks) of an order book.
//
// Layout:
//   * `ladder`  - fixed-size vector of Levels indexed by
//                 slot = (price - base) / TICK. Lazily allocated on first use;
//                 `base` is centered on the first price so subsequent activity
//                 stays in the window.
//   * `overflow`- (price, Level) pairs kept sorted ascending by price, for
//                 prices outside the window or not on a TICK multiple. A flat
//                 vector rather than std::map: contiguous, no per-node heap
//                 allocation, and the best sits at an end (back for bids, front
//                 for asks). Profiling showed ~22% of resting orders land here,
//                 so the slow path has to be cache-friendly, not a red-black
//                 tree of scattered nodes.
//
// Best tracking:
//   * `bestSlot` is the highest populated slot for bids, lowest for asks, or
//     INVALID_INDEX if the ladder is empty. When the best slot empties we
//     scan toward the worse end until we find another populated slot.
//   * The true best price is the better of (ladder slot price, overflow best
//     price); `bestLevel()` combines them.
class LadderSide {
 public:
  static constexpr uint32_t TICK = 100;      // ITCH units per slot
  static constexpr uint32_t WINDOW = 2048;   // slots per side

  explicit LadderSide(bool isBid) : bid(isBid) {}

  // Appends a resting order at `price` (pool index `idx`) to the side.
  void add(OrderPool& pool, uint32_t price, uint32_t idx) {
    if (!anchored) {
      anchor(price);
    }
    uint32_t slot = priceToSlot(price);
    if (slot != INVALID_INDEX) {
      linkAtTail(pool, ladder[slot], idx);
      if (bestSlot == INVALID_INDEX ||
          (bid ? slot > bestSlot : slot < bestSlot)) {
        bestSlot = slot;
      }
    } else {
      auto it = overflowLowerBound(price);
      if (it == overflow.end() || it->first != price) {
        it = overflow.insert(it, {price, Level{}});
      }
      linkAtTail(pool, it->second, idx);
    }
  }

  // Unlinks the order at pool index `idx` from its level. Caller frees the
  // pool slot; this only touches the level / best tracking.
  void remove(OrderPool& pool, uint32_t price, uint32_t idx) {
    uint32_t slot = priceToSlot(price);
    if (slot != INVALID_INDEX) {
      Level& lvl = ladder[slot];
      unlink(pool, lvl, idx);
      if (lvl.head == INVALID_INDEX && slot == bestSlot) {
        advanceBest();
      }
    } else {
      auto it = overflowLowerBound(price);
      if (it == overflow.end() || it->first != price) return;
      unlink(pool, it->second, idx);
      if (it->second.head == INVALID_INDEX) {
        overflow.erase(it);
      }
    }
  }

  // Returns the level holding the best resting order on this side, or
  // nullptr if both ladder and overflow are empty. Writes the best price
  // to *priceOut.
  Level* bestLevel(uint32_t& priceOut) {
    bool ladderEmpty = bestSlot == INVALID_INDEX;
    bool overflowEmpty = overflow.empty();
    if (ladderEmpty && overflowEmpty) return nullptr;

    if (overflowEmpty) {
      priceOut = slotToPrice(bestSlot);
      return &ladder[bestSlot];
    }
    // The best overflow price sits at an end of the sorted vector.
    std::pair<uint32_t, Level>& edge = bid ? overflow.back() : overflow.front();
    if (ladderEmpty) {
      priceOut = edge.first;
      return &edge.second;
    }
    uint32_t ladderPrice = slotToPrice(bestSlot);
    bool ladderWins =
        bid ? (ladderPrice >= edge.first) : (ladderPrice <= edge.first);
    if (ladderWins) {
      priceOut = ladderPrice;
      return &ladder[bestSlot];
    }
    priceOut = edge.first;
    return &edge.second;
  }

  // Called after the matching loop drains a level. Updates best tracking /
  // erases the overflow entry. `price` is the value bestLevel() wrote.
  void removeEmptyBest(uint32_t price) {
    uint32_t slot = priceToSlot(price);
    if (slot != INVALID_INDEX) {
      if (slot == bestSlot) {
        advanceBest();
      }
    } else {
      auto it = overflowLowerBound(price);
      if (it != overflow.end() && it->first == price) {
        overflow.erase(it);
      }
    }
  }

 private:
  using OverflowVec = std::vector<std::pair<uint32_t, Level>>;

  bool bid;
  std::vector<Level> ladder;
  OverflowVec overflow;  // sorted ascending by price
  uint32_t base = 0;
  bool anchored = false;
  uint32_t bestSlot = INVALID_INDEX;

  // First overflow element whose price is >= `price` (binary search).
  OverflowVec::iterator overflowLowerBound(uint32_t price) {
    return std::lower_bound(
        overflow.begin(), overflow.end(), price,
        [](const std::pair<uint32_t, Level>& e, uint32_t p) {
          return e.first < p;
        });
  }

  void anchor(uint32_t price) {
    uint64_t half = static_cast<uint64_t>(WINDOW / 2) * TICK;
    uint64_t candidate = price > half ? price - static_cast<uint32_t>(half) : 0;
    candidate -= candidate % TICK;  // snap so TICK-aligned prices map cleanly
    base = static_cast<uint32_t>(candidate);
    ladder.assign(WINDOW, Level{});
    anchored = true;
  }

  uint32_t priceToSlot(uint32_t price) const {
    if (!anchored || price < base) return INVALID_INDEX;
    uint64_t offset = static_cast<uint64_t>(price) - base;
    if (offset % TICK != 0) return INVALID_INDEX;
    uint64_t slot = offset / TICK;
    if (slot >= WINDOW) return INVALID_INDEX;
    return static_cast<uint32_t>(slot);
  }

  uint32_t slotToPrice(uint32_t slot) const { return base + slot * TICK; }

  static void linkAtTail(OrderPool& pool, Level& lvl, uint32_t idx) {
    PoolNode& node = pool[idx];
    node.prev = lvl.tail;
    node.next = INVALID_INDEX;
    if (lvl.tail != INVALID_INDEX) {
      pool[lvl.tail].next = idx;
    } else {
      lvl.head = idx;
    }
    lvl.tail = idx;
  }

  static void unlink(OrderPool& pool, Level& lvl, uint32_t idx) {
    PoolNode& node = pool[idx];
    if (node.prev != INVALID_INDEX) {
      pool[node.prev].next = node.next;
    } else {
      lvl.head = node.next;
    }
    if (node.next != INVALID_INDEX) {
      pool[node.next].prev = node.prev;
    } else {
      lvl.tail = node.prev;
    }
  }

  // Scan from bestSlot toward the worse end for the next populated slot.
  void advanceBest() {
    if (bid) {
      while (bestSlot != INVALID_INDEX &&
             ladder[bestSlot].head == INVALID_INDEX) {
        if (bestSlot == 0) {
          bestSlot = INVALID_INDEX;
          break;
        }
        bestSlot--;
      }
    } else {
      while (bestSlot != INVALID_INDEX &&
             ladder[bestSlot].head == INVALID_INDEX) {
        bestSlot++;
        if (bestSlot >= WINDOW) {
          bestSlot = INVALID_INDEX;
          break;
        }
      }
    }
  }
};

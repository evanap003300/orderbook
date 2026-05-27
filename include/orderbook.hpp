#pragma once
#include <stdint.h>

#include <map>
#include <vector>

#include "itch.hpp"
#include "order_pool.hpp"

struct ItchOrderExecuted {
  char message_type;
  uint16_t stock_locate;
  uint16_t tracking_number;
  uint16_t timestamp_high;
  uint32_t timestamp_low;
  uint64_t order_reference_number;
  uint32_t executed_shares;
  uint64_t match_number;
};

// A price level is an intrusive FIFO list of orders living in the shared
// OrderPool. head/tail are pool indices, INVALID_INDEX when the level is empty.
struct Level {
  uint32_t head = INVALID_INDEX;
  uint32_t tail = INVALID_INDEX;
};

class OrderBook {
 public:
  explicit OrderBook(OrderPool* pool) : pool(pool) {}

  // Matches `order` against the book and returns the execution records. If the
  // order (or its unfilled remainder) rests, `restingIdx` is set to its pool
  // index; otherwise it is INVALID_INDEX. Resting orders that are fully filled
  // during matching have their reference numbers appended to `removedRefs` so
  // the engine can drop them from its order map.
  std::vector<ItchOrderExecuted> handleOrder(Order& order, uint32_t& restingIdx,
                                             std::vector<uint64_t>& removedRefs);

  // Removes the order at the given pool index from its price level and frees
  // the slot. The engine resolves a reference number to a pool index first.
  void removeByIndex(uint32_t idx);

 private:
  OrderPool* pool;
  std::map<uint32_t, Level, std::greater<uint32_t>> bids;
  std::map<uint32_t, Level> asks;

  std::vector<ItchOrderExecuted> handleBuyOrder(
      Order& order, uint32_t& restingIdx, std::vector<uint64_t>& removedRefs);
  std::vector<ItchOrderExecuted> handleSellOrder(
      Order& order, uint32_t& restingIdx, std::vector<uint64_t>& removedRefs);
  void unlink(Level& level, uint32_t idx);
};

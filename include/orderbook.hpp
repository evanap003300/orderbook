#pragma once
#include <stdint.h>

#include <vector>

#include "itch.hpp"
#include "ladder_side.hpp"

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

class OrderBook {
 public:
  explicit OrderBook(OrderPool* pool)
      : pool(pool), bids(/*isBid=*/true), asks(/*isBid=*/false) {}

  // Matches `order` against the book and appends one execution record per
  // match to `executedOrders`. If the order (or its unfilled remainder) rests,
  // `restingIdx` is set to its pool index; otherwise it is INVALID_INDEX.
  // Resting orders that are fully filled during matching have their reference
  // numbers appended to `removedRefs` so the engine can drop them from its
  // order map. The caller is responsible for clear()-ing the output buffers
  // before the call; reusing them across calls avoids per-message allocation.
  void handleOrder(Order& order, uint32_t& restingIdx,
                   std::vector<uint64_t>& removedRefs,
                   std::vector<ItchOrderExecuted>& executedOrders);

  // Removes the order at the given pool index from its price level and frees
  // the slot. The engine resolves a reference number to a pool index first.
  void removeByIndex(uint32_t idx);

 private:
  OrderPool* pool;
  LadderSide bids;
  LadderSide asks;

  void handleBuyOrder(Order& order, uint32_t& restingIdx,
                      std::vector<uint64_t>& removedRefs,
                      std::vector<ItchOrderExecuted>& executedOrders);
  void handleSellOrder(Order& order, uint32_t& restingIdx,
                       std::vector<uint64_t>& removedRefs,
                       std::vector<ItchOrderExecuted>& executedOrders);
};

#pragma once
#include <stdint.h>

#include <chrono>
#include <deque>
#include <limits>
#include <map>
#include <stdexcept>
#include <vector>

#include "flat_hash_map.hpp"
#include "itch.hpp"

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
  OrderBook() : orderMap(512) {}
  std::vector<ItchOrderExecuted> handleOrder(Order& order);
  void handleDeleteOrder(DeleteOrder& order);

 private:
  FlatHashMap<uint64_t, Order, std::numeric_limits<uint64_t>::max()> orderMap;
  std::map<uint32_t, std::deque<Order>, std::greater<uint32_t>> bids;
  std::map<uint32_t, std::deque<Order>> asks;
  std::vector<ItchOrderExecuted> handleBuyOrder(Order& order);
  std::vector<ItchOrderExecuted> handleSellOrder(Order& order);
  void deleteOrder(DeleteOrder& order);
};
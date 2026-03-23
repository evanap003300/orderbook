#pragma once
#include <stdint.h>

#include <deque>
#include <map>
#include <vector>

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
  std::vector<ItchOrderExecuted> handleOrder(Order& order);

 private:
  std::map<uint32_t, std::deque<Order>, std::greater<uint32_t>> bids;
  std::map<uint32_t, std::deque<Order>> asks;
  std::vector<ItchOrderExecuted> handleBuyOrder(Order& order);
  std::vector<ItchOrderExecuted> handleSellOrder(Order& order);
};
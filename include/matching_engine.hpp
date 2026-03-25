#pragma once
#include <cstring>
#include <unordered_map>

#include "itch.hpp"
#include "orderbook.hpp"

class MatchingEngine {
 public:
  void run();

 private:
  std::unordered_map<uint64_t, OrderBook> orderBooks;
  void logExecutedOrders(const std::vector<ItchOrderExecuted>& executedOrders);
};
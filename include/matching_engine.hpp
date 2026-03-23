#pragma once
#include <unordered_map>

#include "itch.hpp"
#include "orderbook.hpp"

class MatchingEngine {
 public:
  void run();

 private:
  std::unordered_map<std::string, OrderBook> orderBooks;
  std::string parseTicker(Order& order);
  void logExecutedOrders(const std::vector<ItchOrderExecuted>& executedOrders);
};
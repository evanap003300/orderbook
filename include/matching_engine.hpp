#pragma once
#include <cstring>
#include <iostream>
#include <unordered_map>

#include "itch.hpp"
#include "orderbook.hpp"

class MatchingEngine {
 public:
  void run();

 private:
  std::unordered_map<uint64_t, Order> orderMap;
  std::unordered_map<uint64_t, OrderBook> orderBooks;
  void logExecutedOrders(const std::vector<ItchOrderExecuted>& executedOrders);
  uint64_t getTickerAsInt(const Order& order);
};
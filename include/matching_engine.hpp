#pragma once
#include <cstring>
#include <iostream>
#include <unordered_map>

#include "itch.hpp"
#include "orderbook.hpp"

class MatchingEngine {
 public:
  std::vector<uint64_t> latencies;
  MatchingEngine() { latencies.reserve(10000000); }
  void run();

 private:
  std::unordered_map<uint64_t, uint64_t> orderMap;
  std::unordered_map<uint64_t, OrderBook> orderBooks;
  void logExecutedOrders(const std::vector<ItchOrderExecuted>& executedOrders);
  uint64_t getTickerAsInt(const Order& order);
};
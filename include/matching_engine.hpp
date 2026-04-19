#pragma once
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

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
  std::unordered_map<uint64_t, uint16_t> orderMap;
  std::vector<OrderBook> orderBooks{65536};
  void logExecutedOrders(const std::vector<ItchOrderExecuted>& executedOrders);
  inline uint64_t rdtsc();
};
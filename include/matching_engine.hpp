#pragma once
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstring>
#include <limits>

#include "flat_hash_map.hpp"
#include "itch.hpp"
#include "orderbook.hpp"

class MatchingEngine {
 public:
  std::vector<uint64_t> latencies;
  MatchingEngine() : orderMap(33554432) { latencies.reserve(10000000); }
  void run();

 private:
  FlatHashMap<uint64_t, uint16_t, std::numeric_limits<uint64_t>::max()>
      orderMap;
  std::vector<OrderBook> orderBooks{65536};
  void logExecutedOrders(const std::vector<ItchOrderExecuted>& executedOrders);
  inline uint64_t rdtsc();
};
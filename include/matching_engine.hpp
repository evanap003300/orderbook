#pragma once
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstring>
#include <limits>

#include "flat_hash_map.hpp"
#include "itch.hpp"
#include "order_pool.hpp"
#include "orderbook.hpp"

// Where a resting order lives: which book owns it and its slot in the pool.
struct OrderLocation {
  uint16_t stockLocate;
  uint32_t poolIdx;
};

class MatchingEngine {
 public:
  std::vector<uint64_t> latencies;
  MatchingEngine();
  void run();

 private:
  OrderPool pool;
  FlatHashMap<uint64_t, OrderLocation, std::numeric_limits<uint64_t>::max()>
      orderMap;
  std::vector<OrderBook> orderBooks;
  void logExecutedOrders(const std::vector<ItchOrderExecuted>& executedOrders);
  inline uint64_t rdtsc();
};

#include "matching_engine.hpp"

#include <time.h>
#include <x86intrin.h>

#include <cstdio>
#include <string>

MatchingEngine::MatchingEngine() : pool(67108864), orderMap(8388608) {
  // A full ITCH trading day has ~136M Add messages; reserving up front avoids
  // vector grow events during the timed loop (which show up as ms-scale tail
  // spikes). orderMap is sized for the measured peak of ~2.1M live orders:
  // 8.4M slots keeps the load factor ~25% (was 33.5M / 512 MB at ~6% load).
  latencies.reserve(150000000);
  orderBooks.reserve(65536);
  for (uint32_t i = 0; i < 65536; i++) {
    orderBooks.emplace_back(&pool);
  }
}

void MatchingEngine::logExecutedOrders(
    const std::vector<ItchOrderExecuted>& executedOrders) {
  for (const auto& order : executedOrders) {
    printf("Executed Order - Executed Shares: %u\n", order.executed_shares);
  }
}

void MatchingEngine::run() {
  std::string fileName = "itch_data.NASDAQ_ITCH50";

  int fd = open(fileName.c_str(), O_RDONLY);
  struct stat sb;
  fstat(fd, &sb);
  auto fileSize = sb.st_size;

  void* file =
      mmap(nullptr, fileSize, PROT_READ, MAP_FILE | MAP_PRIVATE, fd, 0);

  if (file == MAP_FAILED) {
    close(fd);
    return;
  }

  const char* data = static_cast<const char*>(file);
  const char* end = data + fileSize;

  ItchParser parser;
  uint16_t messageLength;
  char messageType;
  uint16_t stockLocate;
  // Reused buffers; clear()-ed before each Add so capacity is preserved and
  // we don't allocate per message.
  std::vector<ItchOrderExecuted> executedOrders;
  executedOrders.reserve(1024);
  std::vector<uint64_t> removedRefs;
  removedRefs.reserve(1024);
  Order order;
  DeleteOrder deleteOrder;
  uint64_t orderReferenceNumber;
  uint64_t messageCount = 0;

  // Calibrate the TSC against CLOCK_MONOTONIC once, before the timed loop. The
  // TSC is invariant on this CPU (constant tick rate regardless of core P-state),
  // so cycles-per-ns stays valid even while the governor throttles the core.
  auto monoNs = []() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<uint64_t>(ts.tv_sec) * 1000000000ull + ts.tv_nsec;
  };
  unsigned tscAux;
  uint64_t calNs0 = monoNs();
  uint64_t calCycles0 = __rdtscp(&tscAux);
  uint64_t calNs1;
  do {
    calNs1 = monoNs();
  } while (calNs1 - calNs0 < 100000000ull);  // ~100 ms
  uint64_t calCycles1 = __rdtscp(&tscAux);
  double cyclesPerNs =
      static_cast<double>(calCycles1 - calCycles0) / (calNs1 - calNs0);
  fprintf(stderr, "TSC calibrated: %.4f cycles/ns (%.1f MHz)\n", cyclesPerNs,
          cyclesPerNs * 1000.0);

  while (data < end) {
    messageLength = ntohs(*reinterpret_cast<const uint16_t*>(data));
    data += 2;
    messageCount++;
    if (messageCount % 1000000 == 0) {
      printf("Progress: %.1f%%\n",
             (double)(data - static_cast<const char*>(file)) / fileSize * 100);
    }

    messageType = *data;
    data++;

    switch (messageType) {
      case 'A': {
        stockLocate = ntohs(*reinterpret_cast<const uint16_t*>(data));
        order = parser.readAddOrder(data);
        removedRefs.clear();
        executedOrders.clear();
        uint32_t restingIdx;
        uint64_t startCycles = __rdtscp(&tscAux);
        orderBooks[stockLocate].handleOrder(order, restingIdx, removedRefs,
                                            executedOrders);
        uint64_t endCycles = __rdtscp(&tscAux);

        // Resting orders consumed during matching are gone; drop their entries.
        for (uint64_t ref : removedRefs) {
          orderMap.erase(ref);
        }
        // Record this order's slot if any of it rested.
        if (restingIdx != INVALID_INDEX) {
          orderReferenceNumber =
              (static_cast<uint64_t>(order.orderReferenceNumberHigh) << 32) |
              order.orderReferenceNumberLow;
          orderMap.insert(orderReferenceNumber, {stockLocate, restingIdx});
        }

        // Store raw TSC cycles; converted to ns in one pass after the loop.
        latencies.push_back(endCycles - startCycles);
        break;
      }
      case 'D': {
        deleteOrder = parser.readDeleteOrder(data);
        orderReferenceNumber =
            (static_cast<uint64_t>(deleteOrder.orderReferenceNumberHigh)
             << 32) |
            deleteOrder.orderReferenceNumberLow;
        OrderLocation* found = orderMap.find(orderReferenceNumber);
        if (!found) {
          break;
        }
        orderBooks[found->stockLocate].removeByIndex(found->poolIdx);
        orderMap.erase(orderReferenceNumber);
        break;
      }
      default:
        data += messageLength - 1;
        break;
    }
  }

  // Convert recorded cycle counts to nanoseconds, off the hot path.
  for (uint64_t& cycles : latencies) {
    cycles = static_cast<uint64_t>(cycles / cyclesPerNs);
  }

  munmap(file, fileSize);
  close(fd);
}

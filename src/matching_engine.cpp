#include "matching_engine.hpp"

#include <chrono>
#include <cstdio>
#include <string>

MatchingEngine::MatchingEngine() : pool(67108864), orderMap(33554432) {
  // A full ITCH trading day has ~136M Add messages; reserving up front avoids
  // vector grow events during the timed loop (which show up as ms-scale tail
  // spikes).
  latencies.reserve(150000000);
  orderBooks.reserve(65536);
  for (uint32_t i = 0; i < 65536; i++) {
    orderBooks.emplace_back(&pool);
  }
}

// Use cpu timestamp counter for latency measurement
inline uint64_t MatchingEngine::rdtsc() {
  /*unsigned int lo, hi;
  __asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
  return ((uint64_t)hi << 32) | lo; */
  return 0;
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
        auto start = std::chrono::high_resolution_clock::now();
        orderBooks[stockLocate].handleOrder(order, restingIdx, removedRefs,
                                            executedOrders);
        auto end_time = std::chrono::high_resolution_clock::now();

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

        auto latency = end_time - start;
        latencies.push_back(latency.count());
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

  munmap(file, fileSize);
  close(fd);
}

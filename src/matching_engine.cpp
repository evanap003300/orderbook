#include "matching_engine.hpp"

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
  std::vector<ItchOrderExecuted> executedOrders;
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
        auto start = std::chrono::high_resolution_clock::now();
        executedOrders = orderBooks[stockLocate].handleOrder(order);
        auto end_time = std::chrono::high_resolution_clock::now();
        orderReferenceNumber =
            (static_cast<uint64_t>(order.orderReferenceNumberHigh) << 32) |
            order.orderReferenceNumberLow;
        orderMap.insert(orderReferenceNumber, stockLocate);
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
        uint16_t* found = orderMap.find(orderReferenceNumber);
        if (!found) {
          break;
        }
        orderBooks[*found].handleDeleteOrder(deleteOrder);
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
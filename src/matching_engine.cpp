#include "matching_engine.hpp"

// Use cpu timestamp counter for latency measurement
inline uint64_t MatchingEngine::rdtsc() {
  unsigned int lo, hi;
  __asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
  return ((uint64_t)hi << 32) | lo;
}

uint64_t MatchingEngine::getTickerAsInt(const Order& order) {
  uint64_t tickerInt = 0;
  memcpy(&tickerInt, order.stock, 8);
  return tickerInt;
}

void MatchingEngine::logExecutedOrders(
    const std::vector<ItchOrderExecuted>& executedOrders) {
  for (const auto& order : executedOrders) {
    printf("Executed Order - Executed Shares: %u\n", order.executed_shares);
  }
}

void MatchingEngine::run() {
  std::string fileName = "itch_data.NASDAQ_ITCH50";
  ItchParser parser;

  std::ifstream file(fileName, std::ios::binary);

  if (!file.is_open()) {
    throw std::runtime_error("Could not open file");
  }

  uint16_t messageLength;
  char messageType;
  uint64_t ticker;
  std::vector<ItchOrderExecuted> executedOrders;
  Order order;
  DeleteOrder deleteOrder;
  uint64_t orderReferenceNumber;

  file.seekg(0, std::ios::end);
  auto fileSize = file.tellg();
  file.seekg(0, std::ios::beg);

  uint64_t messageCount = 0;

  while (file.read(reinterpret_cast<char*>(&messageLength),
                   sizeof(messageLength))) {
    messageLength = ntohs(messageLength);
    file.read(&messageType, sizeof(messageType));

    messageCount++;
    if (messageCount % 1000000 == 0) {
      printf("Progress: %.1f%%\n", (double)file.tellg() / fileSize * 100);
    }

    switch (messageType) {
      case 'A': {
        order = parser.readAddOrder(file);
        ticker = getTickerAsInt(order);
        auto start = std::chrono::high_resolution_clock::now();
        executedOrders = orderBooks[ticker].handleOrder(order);
        orderReferenceNumber =
            (static_cast<uint64_t>(order.orderReferenceNumberHigh) << 32) |
            order.orderReferenceNumberLow;
        orderMap[orderReferenceNumber] = ticker;
        auto end = std::chrono::high_resolution_clock::now();
        auto latency = end - start;
        latencies.push_back(latency.count());
        break;
      }
      case 'D': {
        deleteOrder = parser.readDeleteOrder(file);
        orderReferenceNumber =
            (static_cast<uint64_t>(deleteOrder.orderReferenceNumberHigh)
             << 32) |
            deleteOrder.orderReferenceNumberLow;
        if (!orderMap.count(orderReferenceNumber)) {
          break;
        }
        ticker = orderMap[orderReferenceNumber];
        orderBooks[ticker].handleDeleteOrder(deleteOrder);
        break;
      }
      default:
        file.seekg(messageLength - 1, std::ios::cur);
        break;
    }
  }

  file.close();
}
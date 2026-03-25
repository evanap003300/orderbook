#include "matching_engine.hpp"

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

  while (file.read(reinterpret_cast<char*>(&messageLength),
                   sizeof(messageLength))) {
    messageLength = ntohs(messageLength);
    file.read(&messageType, sizeof(messageType));

    switch (messageType) {
      case 'A':
        order = parser.readAddOrder(file);
        ticker = getTickerAsInt(order);
        executedOrders = orderBooks[ticker].handleOrder(order);
        orderReferenceNumber =
            (static_cast<uint64_t>(order.orderReferenceNumberHigh) << 32) |
            order.orderReferenceNumberLow;
        orderMap[orderReferenceNumber] = order;
        // logExecutedOrders(executedOrders);
        break;
      case 'D': {
        deleteOrder = parser.readDeleteOrder(file);
        orderReferenceNumber =
            (static_cast<uint64_t>(deleteOrder.orderReferenceNumberHigh)
             << 32) |
            deleteOrder.orderReferenceNumberLow;
        if (!orderMap.count(orderReferenceNumber)) {
          break;
        }
        order = orderMap[orderReferenceNumber];
        ticker = getTickerAsInt(order);
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
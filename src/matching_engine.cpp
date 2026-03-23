#include "matching_engine.hpp"

std::string MatchingEngine::parseTicker(Order& order) {
  std::string ticker = "";
  for (int i = 0; i < 8; i++) {
    if (order.stock[i] == ' ') {
      return ticker;
    } else {
      ticker.push_back(order.stock[i]);
    }
  }
  return ticker;
}

void MatchingEngine::logExecutedOrders(
    const std::vector<ItchOrderExecuted>& executedOrders) {
  for (const auto& order : executedOrders) {
    printf("Executed Order - Executed Shares: %u\n", order.executed_shares);
  }
}

void MatchingEngine::run() {
  ItchParser parser;
  auto orders = parser.readItch("itch_data.NASDAQ_ITCH50");
  for (auto& order : orders) {
    std::string ticker = parseTicker(order);
    std::vector<ItchOrderExecuted> executedOrders =
        orderBooks[ticker].handleOrder(order);
    // logExecutedOrders(executedOrders);
  }
}
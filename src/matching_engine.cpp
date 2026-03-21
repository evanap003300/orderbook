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

void MatchingEngine::logExecutedOrders(const std::vector<ItchOrderExecuted>& executedOrders) {
    for (const auto& order : executedOrders) {
        printf("Executed Order - Message Type: %c, Stock Locate: %u, Tracking Number: %u, Timestamp High: %u, Timestamp Low: %u, Order Reference Number: %lu, Executed Shares: %u, Match Number: %lu\n",
               order.message_type,
               order.stock_locate,
               order.tracking_number,
               order.timestamp_high,
               order.timestamp_low,
               order.order_reference_number,
               order.executed_shares,
               order.match_number);
    }
}

void MatchingEngine::run() {
    ItchParser parser;
    auto orders = parser.readItch(); 
    for (auto& order : orders) {
        std::string ticker = parseTicker(order);
        std::vector<ItchOrderExecuted> executedOrders = orderBooks[ticker].handleOrder(order);
        logExecutedOrders(executedOrders);
    }
}
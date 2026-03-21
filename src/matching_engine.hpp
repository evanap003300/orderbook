#pragma once
#include "orderbook.hpp"
#include "itch.hpp"
#include <unordered_map>

class MatchingEngine {
public:
    void run();

private:
    std::unordered_map<std::string, OrderBook> orderBooks;
    std::string parseTicker(Order& order);
    void logExecutedOrders(const std::vector<ItchOrderExecuted>& executedOrders);
};
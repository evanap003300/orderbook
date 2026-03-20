#include "order_matching.hpp"

void OrderMatching::matchOrders(const std::vector<Order>& orders) {
    std::unordered_map<std::string, std::vector<Order>> orderBook;

    for (const auto& order : orders) {
        std::string stock;
        std::copy(std::begin(order.stock), std::end(order.stock), std::back_inserter(stock));
        orderBook[stock].push_back(order);
    }

    std::vector<ItchOrderExecuted> executedOrders;

    for (auto& [stock, orders] : orderBook) {
        auto tickerExecutedOrders = matchTicker(orders);
        executedOrders.insert(executedOrders.end(), tickerExecutedOrders.begin(), tickerExecutedOrders.end());
    }
}


std::vector<ItchOrderExecuted> matchTicker(const std::vector<Order>& orders) {
    std::vector<ItchOrderExecuted> executedOrders;
    
    std::vector<Order> buyOrders;
    std::vector<Order> sellOrders;

    for (const auto& order : orders) {
        if (order.buySellIndicator == 'B') {
            buyOrders.push_back(order);
        } else if (order.buySellIndicator == 'S') {
            sellOrders.push_back(order);
        }
    }

    sort(buyOrders.begin(), buyOrders.end(), [](const Order& a, const Order& b) {
        return a.price > b.price;
    });

    sort(sellOrders.begin(), sellOrders.end(), [](const Order& a, const Order& b) {
        return a.price < b.price;
    });

    size_t buyIndex = 0, sellIndex = 0;

    while (buyIndex < buyOrders.size() && sellIndex < sellOrders.size()) {
        auto& buyOrder = buyOrders[buyIndex];
        auto& sellOrder = sellOrders[sellIndex];

        if (buyOrder.price >= sellOrder.price) {
            uint32_t executedShares = std::min(buyOrder.shares, sellOrder.shares);
            ItchOrderExecuted executedOrder;
            executedOrder.message_type = 'E';
            executedOrder.stock_locate = buyOrder.stockLocate;
            executedOrder.tracking_number = 0;
            executedOrder.timestamp_high = 0; // get the current time here
            executedOrder.timestamp_low = 0;
            executedOrder.order_reference_number = (static_cast<uint64_t>(buyOrder.orderReferenceNumberHigh) << 32) | buyOrder.orderReferenceNumberLow;
            executedOrder.executed_shares = executedShares;
            executedOrder.match_number = 0; 
            executedOrders.push_back(executedOrder);

            if (buyOrder.shares == executedShares) {
                buyIndex++;
            }
            if (sellOrder.shares == executedShares) {
                sellIndex++;
            }
            sellOrder.shares -= executedShares;
            buyOrder.shares -= executedShares;
        } else {
            break;
        }
    }

    return executedOrders;
}

#include "orderbook.hpp"

std::vector<ItchOrderExecuted> OrderBook::handleBuyOrder(Order& buyOrder) {
  std::vector<ItchOrderExecuted> executedOrders;

  // loop through all the asks and match orders
  while (buyOrder.shares > 0 && !asks.empty()) {
    auto& [price, currentAsks] = *asks.begin();

    if (price > buyOrder.price) {
      break;
    }

    while (buyOrder.shares > 0 && !currentAsks.empty()) {
      auto& sellOrder = currentAsks.front();
      uint32_t executedShares = std::min(buyOrder.shares, sellOrder.shares);
      ItchOrderExecuted executedOrder;
      executedOrder.message_type = 'E';
      executedOrder.stock_locate = buyOrder.stockLocate;
      executedOrder.tracking_number = std::rand();
      executedOrder.timestamp_high = buyOrder.timestampHigh;
      executedOrder.timestamp_low = buyOrder.timestampLow;
      executedOrder.order_reference_number =
          (static_cast<uint64_t>(sellOrder.orderReferenceNumberHigh) << 32) |
          sellOrder.orderReferenceNumberLow;
      executedOrder.executed_shares = executedShares;
      executedOrder.match_number = std::rand();
      executedOrders.push_back(executedOrder);

      buyOrder.shares -= executedShares;
      sellOrder.shares -= executedShares;

      if (sellOrder.shares == 0) {
        uint64_t orderRefNum =
            (static_cast<uint64_t>(sellOrder.orderReferenceNumberHigh) << 32) |
            sellOrder.orderReferenceNumberLow;
        currentAsks.pop_front();
        orderMap.erase(orderRefNum);
      }
    }

    if (currentAsks.empty()) {
      asks.erase(asks.begin());
    }
  }

  // Add order if not empty
  if (buyOrder.shares > 0) {
    bids[buyOrder.price].push_back(buyOrder);
    uint64_t orderRefNum =
        (static_cast<uint64_t>(buyOrder.orderReferenceNumberHigh) << 32) |
        buyOrder.orderReferenceNumberLow;
    orderMap[orderRefNum] = buyOrder;
  }

  return executedOrders;
}

std::vector<ItchOrderExecuted> OrderBook::handleSellOrder(Order& sellOrder) {
  std::vector<ItchOrderExecuted> executedOrders;

  // loop through all the bids and match orders
  while (sellOrder.shares > 0 && !bids.empty()) {
    auto& [price, currentBids] = *bids.begin();

    if (price < sellOrder.price) {
      break;
    }

    while (sellOrder.shares > 0 && !currentBids.empty()) {
      auto& buyOrder = currentBids.front();
      uint32_t executedShares = std::min(sellOrder.shares, buyOrder.shares);
      ItchOrderExecuted executedOrder;
      executedOrder.message_type = 'E';
      executedOrder.stock_locate = sellOrder.stockLocate;
      executedOrder.tracking_number = std::rand();
      executedOrder.timestamp_high = sellOrder.timestampHigh;
      executedOrder.timestamp_low = sellOrder.timestampLow;
      executedOrder.order_reference_number =
          (static_cast<uint64_t>(buyOrder.orderReferenceNumberHigh) << 32) |
          buyOrder.orderReferenceNumberLow;
      executedOrder.executed_shares = executedShares;
      executedOrder.match_number = std::rand();
      executedOrders.push_back(executedOrder);

      buyOrder.shares -= executedShares;
      sellOrder.shares -= executedShares;

      if (buyOrder.shares == 0) {
        uint64_t orderRefNum =
            (static_cast<uint64_t>(buyOrder.orderReferenceNumberHigh) << 32) |
            buyOrder.orderReferenceNumberLow;
        currentBids.pop_front();
        orderMap.erase(orderRefNum);
      }
    }

    if (currentBids.empty()) {
      bids.erase(bids.begin());
    }
  }

  // Add order if not empty
  if (sellOrder.shares > 0) {
    asks[sellOrder.price].push_back(sellOrder);
    uint64_t orderRefNum =
        (static_cast<uint64_t>(sellOrder.orderReferenceNumberHigh) << 32) |
        sellOrder.orderReferenceNumberLow;
    orderMap[orderRefNum] = sellOrder;
  }

  return executedOrders;
}

void OrderBook::deleteOrder(DeleteOrder& order) {
  uint64_t orderRefNum =
      (static_cast<uint64_t>(order.orderReferenceNumberHigh) << 32) |
      order.orderReferenceNumberLow;
  if (orderMap.find(orderRefNum) == orderMap.end()) {
    throw std::runtime_error("Order not found for deletion");
  }

  Order deleteOrder = orderMap[orderRefNum];

  auto& ordersAtPrice = deleteOrder.buySellIndicator == 'B'
                            ? bids[deleteOrder.price]
                            : asks[deleteOrder.price];

  for (auto it = ordersAtPrice.begin(); it != ordersAtPrice.end(); ++it) {
    if (it->orderReferenceNumberHigh == deleteOrder.orderReferenceNumberHigh &&
        it->orderReferenceNumberLow == deleteOrder.orderReferenceNumberLow) {
      ordersAtPrice.erase(it);
      if (ordersAtPrice.empty()) {
        if (deleteOrder.buySellIndicator == 'B') {
          bids.erase(deleteOrder.price);
        } else {
          asks.erase(deleteOrder.price);
        }
      }
      break;
    }
  }

  orderMap.erase(orderRefNum);
}

std::vector<ItchOrderExecuted> OrderBook::handleOrder(Order& order) {
  if (order.buySellIndicator == 'B') {
    return handleBuyOrder(order);
  } else if (order.buySellIndicator == 'S') {
    return handleSellOrder(order);
  } else {
    throw std::runtime_error("Invalid buy/sell indicator");
  }
}

void OrderBook::handleDeleteOrder(DeleteOrder& order) { deleteOrder(order); }

#include "orderbook.hpp"

#include <algorithm>
#include <stdexcept>

std::vector<ItchOrderExecuted> OrderBook::handleBuyOrder(
    Order& buyOrder, uint32_t& restingIdx, std::vector<uint64_t>& removedRefs) {
  std::vector<ItchOrderExecuted> executedOrders;

  // Match against the lowest asks while the buy crosses them.
  while (buyOrder.shares > 0 && !asks.empty()) {
    auto it = asks.begin();
    if (it->first > buyOrder.price) {
      break;
    }
    Level& level = it->second;

    while (buyOrder.shares > 0 && level.head != INVALID_INDEX) {
      PoolNode& node = (*pool)[level.head];
      Order& sellOrder = node.order;
      uint32_t executedShares = std::min(buyOrder.shares, sellOrder.shares);

      ItchOrderExecuted executedOrder;
      executedOrder.message_type = 'E';
      executedOrder.stock_locate = -1;
      executedOrder.tracking_number = -1;
      executedOrder.timestamp_high = -1;
      executedOrder.timestamp_low = -1;
      executedOrder.order_reference_number =
          (static_cast<uint64_t>(sellOrder.orderReferenceNumberHigh) << 32) |
          sellOrder.orderReferenceNumberLow;
      executedOrder.executed_shares = executedShares;
      executedOrder.match_number = -1;
      executedOrders.push_back(executedOrder);

      buyOrder.shares -= executedShares;
      sellOrder.shares -= executedShares;

      if (sellOrder.shares == 0) {
        uint32_t filledIdx = level.head;
        removedRefs.push_back(executedOrder.order_reference_number);
        level.head = node.next;
        if (level.head != INVALID_INDEX) {
          (*pool)[level.head].prev = INVALID_INDEX;
        } else {
          level.tail = INVALID_INDEX;
        }
        pool->free(filledIdx);
      }
    }

    if (level.head == INVALID_INDEX) {
      asks.erase(it);
    }
  }

  // Rest the unfilled remainder at the back of its price level.
  if (buyOrder.shares > 0) {
    restingIdx = pool->allocate(buyOrder);
    Level& level = bids[buyOrder.price];
    PoolNode& node = (*pool)[restingIdx];
    node.prev = level.tail;
    node.next = INVALID_INDEX;
    if (level.tail != INVALID_INDEX) {
      (*pool)[level.tail].next = restingIdx;
    } else {
      level.head = restingIdx;
    }
    level.tail = restingIdx;
  } else {
    restingIdx = INVALID_INDEX;
  }

  return executedOrders;
}

std::vector<ItchOrderExecuted> OrderBook::handleSellOrder(
    Order& sellOrder, uint32_t& restingIdx, std::vector<uint64_t>& removedRefs) {
  std::vector<ItchOrderExecuted> executedOrders;

  // Match against the highest bids while the sell crosses them.
  while (sellOrder.shares > 0 && !bids.empty()) {
    auto it = bids.begin();
    if (it->first < sellOrder.price) {
      break;
    }
    Level& level = it->second;

    while (sellOrder.shares > 0 && level.head != INVALID_INDEX) {
      PoolNode& node = (*pool)[level.head];
      Order& buyOrder = node.order;
      uint32_t executedShares = std::min(sellOrder.shares, buyOrder.shares);

      ItchOrderExecuted executedOrder;
      executedOrder.message_type = 'E';
      executedOrder.stock_locate = -1;
      executedOrder.tracking_number = -1;
      executedOrder.timestamp_high = -1;
      executedOrder.timestamp_low = -1;
      executedOrder.order_reference_number =
          (static_cast<uint64_t>(buyOrder.orderReferenceNumberHigh) << 32) |
          buyOrder.orderReferenceNumberLow;
      executedOrder.executed_shares = executedShares;
      executedOrder.match_number = -1;
      executedOrders.push_back(executedOrder);

      sellOrder.shares -= executedShares;
      buyOrder.shares -= executedShares;

      if (buyOrder.shares == 0) {
        uint32_t filledIdx = level.head;
        removedRefs.push_back(executedOrder.order_reference_number);
        level.head = node.next;
        if (level.head != INVALID_INDEX) {
          (*pool)[level.head].prev = INVALID_INDEX;
        } else {
          level.tail = INVALID_INDEX;
        }
        pool->free(filledIdx);
      }
    }

    if (level.head == INVALID_INDEX) {
      bids.erase(it);
    }
  }

  // Rest the unfilled remainder at the back of its price level.
  if (sellOrder.shares > 0) {
    restingIdx = pool->allocate(sellOrder);
    Level& level = asks[sellOrder.price];
    PoolNode& node = (*pool)[restingIdx];
    node.prev = level.tail;
    node.next = INVALID_INDEX;
    if (level.tail != INVALID_INDEX) {
      (*pool)[level.tail].next = restingIdx;
    } else {
      level.head = restingIdx;
    }
    level.tail = restingIdx;
  } else {
    restingIdx = INVALID_INDEX;
  }

  return executedOrders;
}

// Splices a node out of its price level's doubly-linked list. Does not free it.
void OrderBook::unlink(Level& level, uint32_t idx) {
  PoolNode& node = (*pool)[idx];
  if (node.prev != INVALID_INDEX) {
    (*pool)[node.prev].next = node.next;
  } else {
    level.head = node.next;
  }
  if (node.next != INVALID_INDEX) {
    (*pool)[node.next].prev = node.prev;
  } else {
    level.tail = node.prev;
  }
}

void OrderBook::removeByIndex(uint32_t idx) {
  PoolNode& node = (*pool)[idx];
  uint32_t price = node.order.price;

  if (node.order.buySellIndicator == 'B') {
    auto it = bids.find(price);
    if (it == bids.end()) {
      return;
    }
    unlink(it->second, idx);
    if (it->second.head == INVALID_INDEX) {
      bids.erase(it);
    }
  } else {
    auto it = asks.find(price);
    if (it == asks.end()) {
      return;
    }
    unlink(it->second, idx);
    if (it->second.head == INVALID_INDEX) {
      asks.erase(it);
    }
  }

  pool->free(idx);
}

std::vector<ItchOrderExecuted> OrderBook::handleOrder(
    Order& order, uint32_t& restingIdx, std::vector<uint64_t>& removedRefs) {
  if (order.buySellIndicator == 'B') {
    return handleBuyOrder(order, restingIdx, removedRefs);
  } else if (order.buySellIndicator == 'S') {
    return handleSellOrder(order, restingIdx, removedRefs);
  } else {
    throw std::runtime_error("Invalid buy/sell indicator");
  }
}

#include "orderbook.hpp"

#include <algorithm>
#include <stdexcept>

void OrderBook::handleBuyOrder(Order& buyOrder, uint32_t& restingIdx,
                               std::vector<uint64_t>& removedRefs,
                               std::vector<ItchOrderExecuted>& executedOrders) {
  // Match against the best ask while the buy crosses it. The ladder's
  // bestLevel() handles ladder-vs-overflow ordering in one call.
  while (buyOrder.shares > 0) {
    uint32_t bestPrice;
    Level* level = asks.bestLevel(bestPrice);
    if (!level || bestPrice > buyOrder.price) {
      break;
    }

    while (buyOrder.shares > 0 && level->head != INVALID_INDEX) {
      PoolNode& node = (*pool)[level->head];
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
        uint32_t filledIdx = level->head;
        removedRefs.push_back(executedOrder.order_reference_number);
        level->head = node.next;
        if (level->head != INVALID_INDEX) {
          (*pool)[level->head].prev = INVALID_INDEX;
        } else {
          level->tail = INVALID_INDEX;
        }
        pool->free(filledIdx);
      }
    }

    if (level->head == INVALID_INDEX) {
      asks.removeEmptyBest(bestPrice);
    }
  }

  // Rest the unfilled remainder.
  if (buyOrder.shares > 0) {
    restingIdx = pool->allocate(buyOrder);
    bids.add(*pool, buyOrder.price, restingIdx);
  } else {
    restingIdx = INVALID_INDEX;
  }
}

void OrderBook::handleSellOrder(Order& sellOrder, uint32_t& restingIdx,
                                std::vector<uint64_t>& removedRefs,
                                std::vector<ItchOrderExecuted>& executedOrders) {
  while (sellOrder.shares > 0) {
    uint32_t bestPrice;
    Level* level = bids.bestLevel(bestPrice);
    if (!level || bestPrice < sellOrder.price) {
      break;
    }

    while (sellOrder.shares > 0 && level->head != INVALID_INDEX) {
      PoolNode& node = (*pool)[level->head];
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
        uint32_t filledIdx = level->head;
        removedRefs.push_back(executedOrder.order_reference_number);
        level->head = node.next;
        if (level->head != INVALID_INDEX) {
          (*pool)[level->head].prev = INVALID_INDEX;
        } else {
          level->tail = INVALID_INDEX;
        }
        pool->free(filledIdx);
      }
    }

    if (level->head == INVALID_INDEX) {
      bids.removeEmptyBest(bestPrice);
    }
  }

  if (sellOrder.shares > 0) {
    restingIdx = pool->allocate(sellOrder);
    asks.add(*pool, sellOrder.price, restingIdx);
  } else {
    restingIdx = INVALID_INDEX;
  }
}

void OrderBook::removeByIndex(uint32_t idx) {
  PoolNode& node = (*pool)[idx];
  uint32_t price = node.order.price;
  if (node.order.buySellIndicator == 'B') {
    bids.remove(*pool, price, idx);
  } else {
    asks.remove(*pool, price, idx);
  }
  pool->free(idx);
}

void OrderBook::handleOrder(Order& order, uint32_t& restingIdx,
                            std::vector<uint64_t>& removedRefs,
                            std::vector<ItchOrderExecuted>& executedOrders) {
  if (order.buySellIndicator == 'B') {
    handleBuyOrder(order, restingIdx, removedRefs, executedOrders);
  } else if (order.buySellIndicator == 'S') {
    handleSellOrder(order, restingIdx, removedRefs, executedOrders);
  } else {
    throw std::runtime_error("Invalid buy/sell indicator");
  }
}

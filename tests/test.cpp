#include <gtest/gtest.h>

#include "itch.hpp"
#include "orderbook.hpp"

// Basic functionality

TEST(OrderBookTest, AddBuyOrderToEmptyBook) {
  OrderBook ob;
  Order order{0, 1, 100, 1000, 'B'};
  auto executed = ob.handleOrder(order);
  EXPECT_TRUE(executed.empty());
}

TEST(OrderBookTest, AddSellOrderToEmptyBook) {
  OrderBook ob;
  Order order{0, 1, 100, 1000, 'S'};
  auto executed = ob.handleOrder(order);
  EXPECT_TRUE(executed.empty());
}

TEST(OrderBookTest, BuyAndSellMatchAtCrossingPrice) {
  OrderBook ob;
  Order sell{0, 1, 100, 900, 'S'};
  ob.handleOrder(sell);

  Order buy{0, 2, 100, 1000, 'B'};
  auto executed = ob.handleOrder(buy);
  EXPECT_EQ(executed.size(), 1);
  EXPECT_EQ(executed[0].executed_shares, 100);
}

TEST(OrderBookTest, DeleteOrderRemovesFromBook) {
  OrderBook ob;
  Order buy{0, 1, 100, 1000, 'B'};
  ob.handleOrder(buy);

  DeleteOrder del{0, 1};
  ob.handleDeleteOrder(del);

  Order sell{0, 2, 100, 900, 'S'};
  auto executed = ob.handleOrder(sell);
  EXPECT_TRUE(executed.empty());
}

// Price priority

TEST(OrderBookTest, HigherBidFilledFirst) {
  OrderBook ob;
  Order lowBid{0, 1, 50, 900, 'B'};
  Order highBid{0, 2, 50, 1100, 'B'};
  ob.handleOrder(lowBid);
  ob.handleOrder(highBid);

  Order sell{0, 3, 50, 800, 'S'};
  auto executed = ob.handleOrder(sell);
  EXPECT_EQ(executed.size(), 1);
  EXPECT_EQ(executed[0].order_reference_number, 2);
}

TEST(OrderBookTest, LowerAskFilledFirst) {
  OrderBook ob;
  Order highAsk{0, 1, 50, 1100, 'S'};
  Order lowAsk{0, 2, 50, 900, 'S'};
  ob.handleOrder(highAsk);
  ob.handleOrder(lowAsk);

  Order buy{0, 3, 50, 1200, 'B'};
  auto executed = ob.handleOrder(buy);
  EXPECT_EQ(executed.size(), 1);
  EXPECT_EQ(executed[0].order_reference_number, 2);
}

// Time priority

TEST(OrderBookTest, EarlierBidFilledFirstAtSamePrice) {
  OrderBook ob;
  Order first{0, 1, 50, 1000, 'B'};
  Order second{0, 2, 50, 1000, 'B'};
  ob.handleOrder(first);
  ob.handleOrder(second);

  Order sell{0, 3, 50, 900, 'S'};
  auto executed = ob.handleOrder(sell);
  EXPECT_EQ(executed.size(), 1);
  EXPECT_EQ(executed[0].order_reference_number, 1);
}

TEST(OrderBookTest, EarlierAskFilledFirstAtSamePrice) {
  OrderBook ob;
  Order first{0, 1, 50, 1000, 'S'};
  Order second{0, 2, 50, 1000, 'S'};
  ob.handleOrder(first);
  ob.handleOrder(second);

  Order buy{0, 3, 50, 1100, 'B'};
  auto executed = ob.handleOrder(buy);
  EXPECT_EQ(executed.size(), 1);
  EXPECT_EQ(executed[0].order_reference_number, 1);
}

// Partial fills

TEST(OrderBookTest, IncomingOrderPartiallyFillsRestingOrder) {
  OrderBook ob;
  Order sell{0, 1, 200, 900, 'S'};
  ob.handleOrder(sell);

  Order buy{0, 2, 50, 1000, 'B'};
  auto executed = ob.handleOrder(buy);
  EXPECT_EQ(executed.size(), 1);
  EXPECT_EQ(executed[0].executed_shares, 50);
}

TEST(OrderBookTest, IncomingOrderFillsAgainstMultipleRestingOrders) {
  OrderBook ob;
  Order sell1{0, 1, 30, 900, 'S'};
  Order sell2{0, 2, 30, 950, 'S'};
  Order sell3{0, 3, 30, 1000, 'S'};
  ob.handleOrder(sell1);
  ob.handleOrder(sell2);
  ob.handleOrder(sell3);

  Order buy{0, 4, 80, 1000, 'B'};
  auto executed = ob.handleOrder(buy);
  EXPECT_EQ(executed.size(), 3);
  EXPECT_EQ(executed[0].executed_shares, 30);
  EXPECT_EQ(executed[1].executed_shares, 30);
  EXPECT_EQ(executed[2].executed_shares, 20);
}

TEST(OrderBookTest, RestingOrderHasRemainingSharesAfterPartialFill) {
  OrderBook ob;
  Order sell{0, 1, 200, 900, 'S'};
  ob.handleOrder(sell);

  Order buy1{0, 2, 50, 1000, 'B'};
  ob.handleOrder(buy1);

  Order buy2{0, 3, 50, 1000, 'B'};
  auto executed = ob.handleOrder(buy2);
  EXPECT_EQ(executed.size(), 1);
  EXPECT_EQ(executed[0].executed_shares, 50);
}

// Edge cases

TEST(OrderBookTest, DeleteAlreadyFilledOrderIsNoOp) {
  OrderBook ob;
  Order sell{0, 1, 100, 900, 'S'};
  ob.handleOrder(sell);

  Order buy{0, 2, 100, 1000, 'B'};
  ob.handleOrder(buy);

  DeleteOrder del{0, 1};
  EXPECT_NO_THROW(ob.handleDeleteOrder(del));
}

TEST(OrderBookTest, DeleteThenNoMatch) {
  OrderBook ob;
  Order buy1{0, 1, 100, 1000, 'B'};
  Order buy2{0, 2, 100, 900, 'B'};
  ob.handleOrder(buy1);
  ob.handleOrder(buy2);

  DeleteOrder del{0, 1};
  ob.handleDeleteOrder(del);

  Order sell{0, 3, 100, 950, 'S'};
  auto executed = ob.handleOrder(sell);
  EXPECT_TRUE(executed.empty());
}

TEST(OrderBookTest, NoMatchBuyPriceTooLow) {
  OrderBook ob;
  Order sell{0, 1, 100, 1000, 'S'};
  ob.handleOrder(sell);

  Order buy{0, 2, 100, 900, 'B'};
  auto executed = ob.handleOrder(buy);
  EXPECT_TRUE(executed.empty());
}

TEST(OrderBookTest, NoMatchSellPriceTooHigh) {
  OrderBook ob;
  Order buy{0, 1, 100, 1000, 'B'};
  ob.handleOrder(buy);

  Order sell{0, 2, 100, 1100, 'S'};
  auto executed = ob.handleOrder(sell);
  EXPECT_TRUE(executed.empty());
}

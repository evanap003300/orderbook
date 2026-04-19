#include <gtest/gtest.h>

#include <cstdint>
#include <limits>

#include "flat_hash_map.hpp"
#include "itch.hpp"
#include "orderbook.hpp"

// FlatHashMap tests

constexpr uint64_t EMPTY_KEY = std::numeric_limits<uint64_t>::max();

TEST(FlatHashMapTest, InsertAndFind) {
  FlatHashMap<uint64_t, uint16_t, EMPTY_KEY> map(16);
  map.insert(42, 100);
  uint16_t* val = map.find(42);
  ASSERT_NE(val, nullptr);
  EXPECT_EQ(*val, 100);
}

TEST(FlatHashMapTest, FindMissingReturnsNull) {
  FlatHashMap<uint64_t, uint16_t, EMPTY_KEY> map(16);
  EXPECT_EQ(map.find(42), nullptr);
}

TEST(FlatHashMapTest, InsertOverwritesExistingKey) {
  FlatHashMap<uint64_t, uint16_t, EMPTY_KEY> map(16);
  map.insert(42, 100);
  map.insert(42, 200);
  uint16_t* val = map.find(42);
  ASSERT_NE(val, nullptr);
  EXPECT_EQ(*val, 200);
}

TEST(FlatHashMapTest, EraseRemovesKey) {
  FlatHashMap<uint64_t, uint16_t, EMPTY_KEY> map(16);
  map.insert(42, 100);
  map.erase(42);
  EXPECT_EQ(map.find(42), nullptr);
}

TEST(FlatHashMapTest, EraseMissingKeyIsNoOp) {
  FlatHashMap<uint64_t, uint16_t, EMPTY_KEY> map(16);
  EXPECT_NO_THROW(map.erase(42));
}

TEST(FlatHashMapTest, MultipleInsertsAndFinds) {
  FlatHashMap<uint64_t, uint16_t, EMPTY_KEY> map(64);
  for (uint64_t i = 0; i < 20; i++) {
    map.insert(i, static_cast<uint16_t>(i * 10));
  }
  for (uint64_t i = 0; i < 20; i++) {
    uint16_t* val = map.find(i);
    ASSERT_NE(val, nullptr);
    EXPECT_EQ(*val, static_cast<uint16_t>(i * 10));
  }
}

TEST(FlatHashMapTest, EraseWithCollisionsStillFinds) {
  FlatHashMap<uint64_t, uint16_t, EMPTY_KEY> map(8);
  map.insert(1, 10);
  map.insert(2, 20);
  map.insert(3, 30);
  map.insert(4, 40);

  map.erase(2);

  EXPECT_EQ(map.find(2), nullptr);

  uint16_t* v1 = map.find(1);
  uint16_t* v3 = map.find(3);
  uint16_t* v4 = map.find(4);
  ASSERT_NE(v1, nullptr);
  ASSERT_NE(v3, nullptr);
  ASSERT_NE(v4, nullptr);
  EXPECT_EQ(*v1, 10);
  EXPECT_EQ(*v3, 30);
  EXPECT_EQ(*v4, 40);
}

TEST(FlatHashMapTest, EraseAllEntries) {
  FlatHashMap<uint64_t, uint16_t, EMPTY_KEY> map(64);
  for (uint64_t i = 0; i < 30; i++) {
    map.insert(i, static_cast<uint16_t>(i));
  }
  for (uint64_t i = 0; i < 30; i++) {
    map.erase(i);
  }
  for (uint64_t i = 0; i < 30; i++) {
    EXPECT_EQ(map.find(i), nullptr);
  }
}

TEST(FlatHashMapTest, ReinsertAfterErase) {
  FlatHashMap<uint64_t, uint16_t, EMPTY_KEY> map(16);
  map.insert(42, 100);
  map.erase(42);
  map.insert(42, 999);
  uint16_t* val = map.find(42);
  ASSERT_NE(val, nullptr);
  EXPECT_EQ(*val, 999);
}

TEST(FlatHashMapTest, LargeNumberOfInsertsAndErases) {
  FlatHashMap<uint64_t, uint16_t, EMPTY_KEY> map(1024);
  for (uint64_t i = 0; i < 500; i++) {
    map.insert(i, static_cast<uint16_t>(i % 65536));
  }
  // Erase half
  for (uint64_t i = 0; i < 250; i++) {
    map.erase(i);
  }
  // Verify remaining
  for (uint64_t i = 0; i < 250; i++) {
    EXPECT_EQ(map.find(i), nullptr);
  }
  for (uint64_t i = 250; i < 500; i++) {
    uint16_t* val = map.find(i);
    ASSERT_NE(val, nullptr);
    EXPECT_EQ(*val, static_cast<uint16_t>(i % 65536));
  }
}

// OrderBook tests

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

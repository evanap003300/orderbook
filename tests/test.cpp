#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
#include <vector>

#include "flat_hash_map.hpp"
#include "itch.hpp"
#include "order_pool.hpp"
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
//
// The engine resolves a delete's reference number to a pool index before
// calling the book, so these tests mirror that: an OrderPool is owned here,
// add() captures the pool index of any resting order, and del() removes by
// index. removed[] collects resting orders fully filled during matching.
struct TestBook {
  OrderPool pool;
  OrderBook ob{&pool};
  uint32_t lastRestingIdx = INVALID_INDEX;
  std::vector<uint64_t> removed;

  std::vector<ItchOrderExecuted> add(Order order) {
    removed.clear();
    return ob.handleOrder(order, lastRestingIdx, removed);
  }

  void del(uint32_t idx) { ob.removeByIndex(idx); }
};

// Basic functionality

TEST(OrderBookTest, AddBuyOrderToEmptyBook) {
  TestBook tb;
  Order order{0, 1, 100, 1000, 'B'};
  auto executed = tb.add(order);
  EXPECT_TRUE(executed.empty());
  EXPECT_NE(tb.lastRestingIdx, INVALID_INDEX);
}

TEST(OrderBookTest, AddSellOrderToEmptyBook) {
  TestBook tb;
  Order order{0, 1, 100, 1000, 'S'};
  auto executed = tb.add(order);
  EXPECT_TRUE(executed.empty());
  EXPECT_NE(tb.lastRestingIdx, INVALID_INDEX);
}

TEST(OrderBookTest, BuyAndSellMatchAtCrossingPrice) {
  TestBook tb;
  Order sell{0, 1, 100, 900, 'S'};
  tb.add(sell);

  Order buy{0, 2, 100, 1000, 'B'};
  auto executed = tb.add(buy);
  EXPECT_EQ(executed.size(), 1);
  EXPECT_EQ(executed[0].executed_shares, 100);
  // Both sides fully filled: nothing rests, resting sell is reported removed.
  EXPECT_EQ(tb.lastRestingIdx, INVALID_INDEX);
  ASSERT_EQ(tb.removed.size(), 1u);
  EXPECT_EQ(tb.removed[0], 1u);
}

TEST(OrderBookTest, DeleteOrderRemovesFromBook) {
  TestBook tb;
  Order buy{0, 1, 100, 1000, 'B'};
  tb.add(buy);
  tb.del(tb.lastRestingIdx);

  Order sell{0, 2, 100, 900, 'S'};
  auto executed = tb.add(sell);
  EXPECT_TRUE(executed.empty());
}

// Price priority

TEST(OrderBookTest, HigherBidFilledFirst) {
  TestBook tb;
  Order lowBid{0, 1, 50, 900, 'B'};
  Order highBid{0, 2, 50, 1100, 'B'};
  tb.add(lowBid);
  tb.add(highBid);

  Order sell{0, 3, 50, 800, 'S'};
  auto executed = tb.add(sell);
  EXPECT_EQ(executed.size(), 1);
  EXPECT_EQ(executed[0].order_reference_number, 2);
}

TEST(OrderBookTest, LowerAskFilledFirst) {
  TestBook tb;
  Order highAsk{0, 1, 50, 1100, 'S'};
  Order lowAsk{0, 2, 50, 900, 'S'};
  tb.add(highAsk);
  tb.add(lowAsk);

  Order buy{0, 3, 50, 1200, 'B'};
  auto executed = tb.add(buy);
  EXPECT_EQ(executed.size(), 1);
  EXPECT_EQ(executed[0].order_reference_number, 2);
}

// Time priority

TEST(OrderBookTest, EarlierBidFilledFirstAtSamePrice) {
  TestBook tb;
  Order first{0, 1, 50, 1000, 'B'};
  Order second{0, 2, 50, 1000, 'B'};
  tb.add(first);
  tb.add(second);

  Order sell{0, 3, 50, 900, 'S'};
  auto executed = tb.add(sell);
  EXPECT_EQ(executed.size(), 1);
  EXPECT_EQ(executed[0].order_reference_number, 1);
}

TEST(OrderBookTest, EarlierAskFilledFirstAtSamePrice) {
  TestBook tb;
  Order first{0, 1, 50, 1000, 'S'};
  Order second{0, 2, 50, 1000, 'S'};
  tb.add(first);
  tb.add(second);

  Order buy{0, 3, 50, 1100, 'B'};
  auto executed = tb.add(buy);
  EXPECT_EQ(executed.size(), 1);
  EXPECT_EQ(executed[0].order_reference_number, 1);
}

// Partial fills

TEST(OrderBookTest, IncomingOrderPartiallyFillsRestingOrder) {
  TestBook tb;
  Order sell{0, 1, 200, 900, 'S'};
  tb.add(sell);

  Order buy{0, 2, 50, 1000, 'B'};
  auto executed = tb.add(buy);
  EXPECT_EQ(executed.size(), 1);
  EXPECT_EQ(executed[0].executed_shares, 50);
}

TEST(OrderBookTest, IncomingOrderFillsAgainstMultipleRestingOrders) {
  TestBook tb;
  Order sell1{0, 1, 30, 900, 'S'};
  Order sell2{0, 2, 30, 950, 'S'};
  Order sell3{0, 3, 30, 1000, 'S'};
  tb.add(sell1);
  tb.add(sell2);
  tb.add(sell3);

  Order buy{0, 4, 80, 1000, 'B'};
  auto executed = tb.add(buy);
  EXPECT_EQ(executed.size(), 3);
  EXPECT_EQ(executed[0].executed_shares, 30);
  EXPECT_EQ(executed[1].executed_shares, 30);
  EXPECT_EQ(executed[2].executed_shares, 20);
}

TEST(OrderBookTest, RestingOrderHasRemainingSharesAfterPartialFill) {
  TestBook tb;
  Order sell{0, 1, 200, 900, 'S'};
  tb.add(sell);

  Order buy1{0, 2, 50, 1000, 'B'};
  tb.add(buy1);

  Order buy2{0, 3, 50, 1000, 'B'};
  auto executed = tb.add(buy2);
  EXPECT_EQ(executed.size(), 1);
  EXPECT_EQ(executed[0].executed_shares, 50);
}

// Edge cases

TEST(OrderBookTest, FullyFilledRestingOrderIsReported) {
  TestBook tb;
  Order sell{0, 1, 100, 900, 'S'};
  tb.add(sell);

  Order buy{0, 2, 100, 1000, 'B'};
  tb.add(buy);

  // The resting sell (ref 1) was fully consumed, so the engine is told to drop
  // it from the order map.
  ASSERT_EQ(tb.removed.size(), 1u);
  EXPECT_EQ(tb.removed[0], 1u);
}

TEST(OrderBookTest, DeleteThenNoMatch) {
  TestBook tb;
  Order buy1{0, 1, 100, 1000, 'B'};
  Order buy2{0, 2, 100, 900, 'B'};
  tb.add(buy1);
  uint32_t buy1Idx = tb.lastRestingIdx;
  tb.add(buy2);

  tb.del(buy1Idx);

  Order sell{0, 3, 100, 950, 'S'};
  auto executed = tb.add(sell);
  EXPECT_TRUE(executed.empty());
}

TEST(OrderBookTest, NoMatchBuyPriceTooLow) {
  TestBook tb;
  Order sell{0, 1, 100, 1000, 'S'};
  tb.add(sell);

  Order buy{0, 2, 100, 900, 'B'};
  auto executed = tb.add(buy);
  EXPECT_TRUE(executed.empty());
}

TEST(OrderBookTest, NoMatchSellPriceTooHigh) {
  TestBook tb;
  Order buy{0, 1, 100, 1000, 'B'};
  tb.add(buy);

  Order sell{0, 2, 100, 1100, 'S'};
  auto executed = tb.add(sell);
  EXPECT_TRUE(executed.empty());
}

// Pool reuse: a slot freed by a fill is handed back out to the next rester.

TEST(OrderBookTest, PoolSlotReusedAfterFill) {
  TestBook tb;
  Order sell{0, 1, 100, 900, 'S'};
  tb.add(sell);
  uint32_t sellIdx = tb.lastRestingIdx;

  Order buy{0, 2, 100, 1000, 'B'};
  tb.add(buy);  // fully fills the sell, freeing sellIdx

  Order sell2{0, 3, 100, 900, 'S'};
  tb.add(sell2);  // should reclaim the freed slot
  EXPECT_EQ(tb.lastRestingIdx, sellIdx);
}

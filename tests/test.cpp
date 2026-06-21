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

// Identity hashing (key & mask): keys differing by a multiple of capacity
// collide and must still be found/erased via probing + robin-hood backshift.
TEST(FlatHashMapTest, IdentityHashHandlesCollisions) {
  FlatHashMap<uint64_t, uint16_t, EMPTY_KEY, /*Identity=*/true> map(8);
  map.insert(1, 10);
  map.insert(9, 90);    // 9 & 7 == 1 -> collides with key 1
  map.insert(17, 170);  // 17 & 7 == 1 -> collides too
  ASSERT_NE(map.find(1), nullptr);
  EXPECT_EQ(*map.find(1), 10);
  ASSERT_NE(map.find(9), nullptr);
  EXPECT_EQ(*map.find(9), 90);
  ASSERT_NE(map.find(17), nullptr);
  EXPECT_EQ(*map.find(17), 170);

  map.erase(9);  // backshift across the collision cluster
  EXPECT_EQ(map.find(9), nullptr);
  ASSERT_NE(map.find(1), nullptr);
  EXPECT_EQ(*map.find(1), 10);
  ASSERT_NE(map.find(17), nullptr);
  EXPECT_EQ(*map.find(17), 170);
}

// Regression: erasing a key whose probe chain wraps around the table boundary
// must not leave a hole that disconnects keys placed beyond the wrap.
// With mask=7: insert 7 (home 7), 0 (home 0), 1 (home 1), 2 (home 2),
// then 15 (home 7 → probes 7→0→1→2→3). Erase 0 must shift 15 into slot 0
// so find(15) still works.
TEST(FlatHashMapTest, IdentityHashEraseWrapAroundChain) {
  FlatHashMap<uint64_t, uint16_t, EMPTY_KEY, /*Identity=*/true> map(8);
  map.insert(7, 70);
  map.insert(0, 0);
  map.insert(1, 10);
  map.insert(2, 20);
  map.insert(15, 150);  // home=15&7=7, probes 7→0→1→2→3

  map.erase(0);
  EXPECT_EQ(map.find(0), nullptr);
  ASSERT_NE(map.find(7), nullptr);
  EXPECT_EQ(*map.find(7), 70);
  ASSERT_NE(map.find(1), nullptr);
  EXPECT_EQ(*map.find(1), 10);
  ASSERT_NE(map.find(2), nullptr);
  EXPECT_EQ(*map.find(2), 20);
  ASSERT_NE(map.find(15), nullptr);  // was lost with the old algorithm
  EXPECT_EQ(*map.find(15), 150);
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
  std::vector<ItchOrderExecuted> executed;

  // Returns a reference into TestBook's own buffer so callers can either
  // `auto& e = tb.add(...)` (no copy) or `auto e = tb.add(...)` (copy).
  const std::vector<ItchOrderExecuted>& add(Order order) {
    removed.clear();
    executed.clear();
    ob.handleOrder(order, lastRestingIdx, removed, executed);
    return executed;
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

// Ladder-specific paths: overflow, best-across-both, walking down levels.

// 950 isn't a TICK (=100) multiple, so it lives in the overflow map rather
// than the ladder. Matching must still treat it as a regular ask.
TEST(OrderBookTest, UnalignedPriceUsesOverflow) {
  TestBook tb;
  Order sell{0, 1, 100, 950, 'S'};
  tb.add(sell);

  Order buy{0, 2, 100, 1000, 'B'};
  auto executed = tb.add(buy);
  ASSERT_EQ(executed.size(), 1);
  EXPECT_EQ(executed[0].executed_shares, 100);
  EXPECT_EQ(executed[0].order_reference_number, 1u);
  ASSERT_EQ(tb.removed.size(), 1u);
  EXPECT_EQ(tb.removed[0], 1u);
}

// A bid above the ladder window (window covers base..base+WINDOW*TICK) lands in
// overflow. bestLevel() must pick it over the in-ladder bid.
TEST(OrderBookTest, OverflowBidBeatsLadderBid) {
  TestBook tb;
  Order ladderBid{0, 1, 50, 1000, 'B'};  // anchors bids at base=0; slot 10
  tb.add(ladderBid);
  Order farBid{0, 2, 50, 250000, 'B'};   // > WINDOW*TICK from base -> overflow
  tb.add(farBid);

  Order sell{0, 3, 50, 100, 'S'};
  auto executed = tb.add(sell);
  ASSERT_EQ(executed.size(), 1);
  // Best bid is the overflow one (price 250000), so ref 2 fills first.
  EXPECT_EQ(executed[0].order_reference_number, 2u);
}

// Three bids at adjacent slots; each sweep removes the current best and the
// next-best must surface. Exercises advanceBest scanning across slots.
TEST(OrderBookTest, BestSlotAdvancesAfterEachSweep) {
  TestBook tb;
  Order b1{0, 1, 30, 1000, 'B'};
  Order b2{0, 2, 30, 1100, 'B'};
  Order b3{0, 3, 30, 1200, 'B'};
  tb.add(b1);
  tb.add(b2);
  tb.add(b3);

  Order sell1{0, 4, 30, 800, 'S'};  // best bid 1200
  auto e1 = tb.add(sell1);
  ASSERT_EQ(e1.size(), 1);
  EXPECT_EQ(e1[0].order_reference_number, 3u);

  Order sell2{0, 5, 30, 800, 'S'};  // best bid now 1100
  auto e2 = tb.add(sell2);
  ASSERT_EQ(e2.size(), 1);
  EXPECT_EQ(e2[0].order_reference_number, 2u);

  Order sell3{0, 6, 30, 800, 'S'};  // best bid now 1000
  auto e3 = tb.add(sell3);
  ASSERT_EQ(e3.size(), 1);
  EXPECT_EQ(e3[0].order_reference_number, 1u);
}

// Deleting an overflow order goes through the overflow path of remove(); the
// remaining ladder order should still be findable on the next match.
TEST(OrderBookTest, DeleteOverflowOrderLeavesLadderIntact) {
  TestBook tb;
  Order ladderBid{0, 1, 50, 1000, 'B'};
  tb.add(ladderBid);
  Order overBid{0, 2, 50, 950, 'B'};  // unaligned -> overflow
  tb.add(overBid);
  uint32_t overIdx = tb.lastRestingIdx;

  tb.del(overIdx);

  Order sell{0, 3, 50, 1000, 'S'};
  auto executed = tb.add(sell);
  ASSERT_EQ(executed.size(), 1);
  EXPECT_EQ(executed[0].order_reference_number, 1u);  // the ladder bid
}

// Several overflow asks (all unaligned) added out of price order; a sweeping
// buy must fill them lowest-price-first. Exercises the sorted-vector ordering
// and the front-pop / shift path on the ask side.
TEST(OrderBookTest, MultipleOverflowAsksMatchInPriceOrder) {
  TestBook tb;
  Order a1{0, 1, 10, 1150, 'S'};  // unaligned -> overflow
  Order a2{0, 2, 10, 950, 'S'};   // unaligned -> overflow (lowest)
  Order a3{0, 3, 10, 1050, 'S'};  // unaligned -> overflow
  tb.add(a1);
  tb.add(a2);
  tb.add(a3);

  Order buy{0, 4, 30, 2000, 'B'};  // crosses all three
  auto executed = tb.add(buy);
  ASSERT_EQ(executed.size(), 3u);
  EXPECT_EQ(executed[0].order_reference_number, 2u);  // 950 first
  EXPECT_EQ(executed[1].order_reference_number, 3u);  // 1050
  EXPECT_EQ(executed[2].order_reference_number, 1u);  // 1150
}

// Same for bids: a sweeping sell must fill highest-price-first, exercising the
// back-pop path on the bid side.
TEST(OrderBookTest, MultipleOverflowBidsMatchInPriceOrder) {
  TestBook tb;
  Order b1{0, 1, 10, 950, 'B'};   // unaligned -> overflow
  Order b2{0, 2, 10, 1150, 'B'};  // unaligned -> overflow (highest)
  Order b3{0, 3, 10, 1050, 'B'};  // unaligned -> overflow
  tb.add(b1);
  tb.add(b2);
  tb.add(b3);

  Order sell{0, 4, 30, 100, 'S'};  // crosses all three
  auto executed = tb.add(sell);
  ASSERT_EQ(executed.size(), 3u);
  EXPECT_EQ(executed[0].order_reference_number, 2u);  // 1150 first
  EXPECT_EQ(executed[1].order_reference_number, 3u);  // 1050
  EXPECT_EQ(executed[2].order_reference_number, 1u);  // 950
}

// A single incoming order whose match walks from the ladder into the overflow:
// the ladder ask (1000) is better than the overflow ask (1050) and fills first.
TEST(OrderBookTest, MatchSweepsLadderThenOverflowAsk) {
  TestBook tb;
  Order ladderAsk{0, 1, 10, 1000, 'S'};  // aligned -> ladder
  tb.add(ladderAsk);
  Order overAsk{0, 2, 10, 1050, 'S'};    // unaligned -> overflow
  tb.add(overAsk);

  Order buy{0, 3, 20, 2000, 'B'};  // crosses both
  auto executed = tb.add(buy);
  ASSERT_EQ(executed.size(), 2u);
  EXPECT_EQ(executed[0].order_reference_number, 1u);  // ladder 1000 first
  EXPECT_EQ(executed[1].order_reference_number, 2u);  // then overflow 1050
}

// ============================================================================
// Phase 5: SPSC ring, packet pool, MoldUDP64 framing
// ============================================================================

#include <atomic>
#include <thread>

#include "moldudp64.hpp"
#include "packet_pool.hpp"
#include "spsc_ring.hpp"

// --- SpscRing ---------------------------------------------------------------

TEST(SpscRingTest, PushPopRoundTrip) {
  SpscRing<uint32_t> r(8);
  EXPECT_TRUE(r.push(7));
  uint32_t out = 0;
  EXPECT_TRUE(r.pop(out));
  EXPECT_EQ(out, 7u);
}

TEST(SpscRingTest, EmptyPopReturnsFalse) {
  SpscRing<uint32_t> r(8);
  uint32_t out;
  EXPECT_FALSE(r.pop(out));
}

TEST(SpscRingTest, FullPushReturnsFalse) {
  SpscRing<uint32_t> r(4);  // capacity 4 => 3 usable slots (one always open)
  EXPECT_TRUE(r.push(1));
  EXPECT_TRUE(r.push(2));
  EXPECT_TRUE(r.push(3));
  EXPECT_FALSE(r.push(4));  // full
}

TEST(SpscRingTest, WrapAroundManyCycles) {
  SpscRing<uint32_t> r(8);
  for (uint32_t i = 0; i < 100; ++i) {
    EXPECT_TRUE(r.push(i));
    uint32_t out;
    EXPECT_TRUE(r.pop(out));
    EXPECT_EQ(out, i);
  }
}

TEST(SpscRingTest, ProducerConsumerThreads) {
  SpscRing<uint64_t> r(1024);
  constexpr uint64_t N = 100000;
  std::atomic<bool> doneProducer{false};
  std::thread prod([&]() {
    for (uint64_t i = 0; i < N; ++i) {
      while (!r.push(i)) std::this_thread::yield();
    }
    doneProducer.store(true);
  });
  uint64_t expected = 0;
  while (expected < N) {
    uint64_t out;
    if (r.pop(out)) {
      EXPECT_EQ(out, expected);
      expected++;
    }
  }
  prod.join();
  EXPECT_EQ(expected, N);
}

// --- PacketPool -------------------------------------------------------------

TEST(PacketPoolTest, AcquireReleaseReuses) {
  PacketPool p(4);
  uint32_t a = p.acquire();
  uint32_t b = p.acquire();
  EXPECT_NE(a, kPoolInvalid);
  EXPECT_NE(b, kPoolInvalid);
  EXPECT_NE(a, b);
  p.release(a);
  uint32_t c = p.acquire();
  EXPECT_EQ(c, a);  // freed slot comes back first (Treiber stack)
}

TEST(PacketPoolTest, ExhaustionReturnsInvalid) {
  PacketPool p(2);
  EXPECT_NE(p.acquire(), kPoolInvalid);
  EXPECT_NE(p.acquire(), kPoolInvalid);
  EXPECT_EQ(p.acquire(), kPoolInvalid);
}

// --- MoldUDP64 round-trip ----------------------------------------------------

TEST(MoldUdp64Test, BuildAndParseSingleMessage) {
  uint8_t packet[256];
  char sess[10] = {'S','E','S','S','I','O','N','0','0','1'};
  const uint8_t msg[] = {'A', 0x00, 0x01, 0xAA, 0xBB};
  const uint8_t* msgs[] = {msg};
  uint16_t lens[] = {sizeof(msg)};

  size_t n = buildMoldPacket(packet, sizeof(packet), sess, /*seq=*/42, msgs,
                             lens, 1);
  ASSERT_GT(n, kMoldHeaderBytes);

  MoldHeader h;
  parseMoldHeader(packet, h);
  EXPECT_EQ(memcmp(h.session, sess, 10), 0);
  EXPECT_EQ(h.sequenceNumber, 42u);
  EXPECT_EQ(h.messageCount, 1u);

  MoldMessageIterator it(packet + kMoldHeaderBytes, n - kMoldHeaderBytes);
  uint16_t outLen;
  const uint8_t* got = it.next(outLen);
  ASSERT_NE(got, nullptr);
  EXPECT_EQ(outLen, sizeof(msg));
  EXPECT_EQ(memcmp(got, msg, sizeof(msg)), 0);
  EXPECT_EQ(it.next(outLen), nullptr);
}

TEST(MoldUdp64Test, BuildAndParseMultipleMessages) {
  uint8_t packet[512];
  char sess[10] = {'S','E','S','S','I','O','N','0','0','2'};
  const uint8_t m1[] = {'A', 1, 2, 3};
  const uint8_t m2[] = {'D', 9, 8};
  const uint8_t m3[] = {'A', 7, 7, 7, 7, 7};
  const uint8_t* msgs[] = {m1, m2, m3};
  uint16_t lens[] = {sizeof(m1), sizeof(m2), sizeof(m3)};

  size_t n = buildMoldPacket(packet, sizeof(packet), sess, /*seq=*/1000, msgs,
                             lens, 3);
  ASSERT_GT(n, kMoldHeaderBytes);

  MoldHeader h;
  parseMoldHeader(packet, h);
  EXPECT_EQ(h.sequenceNumber, 1000u);
  EXPECT_EQ(h.messageCount, 3u);

  MoldMessageIterator it(packet + kMoldHeaderBytes, n - kMoldHeaderBytes);
  uint16_t outLen;
  const uint8_t* g1 = it.next(outLen);
  ASSERT_NE(g1, nullptr);
  EXPECT_EQ(outLen, sizeof(m1));
  EXPECT_EQ(memcmp(g1, m1, sizeof(m1)), 0);
  const uint8_t* g2 = it.next(outLen);
  ASSERT_NE(g2, nullptr);
  EXPECT_EQ(outLen, sizeof(m2));
  EXPECT_EQ(memcmp(g2, m2, sizeof(m2)), 0);
  const uint8_t* g3 = it.next(outLen);
  ASSERT_NE(g3, nullptr);
  EXPECT_EQ(outLen, sizeof(m3));
  EXPECT_EQ(memcmp(g3, m3, sizeof(m3)), 0);
  EXPECT_EQ(it.next(outLen), nullptr);
}

TEST(MoldUdp64Test, TruncatedPacketReturnsNull) {
  // Build a valid 2-message packet, then truncate the buffer mid-message-2.
  uint8_t packet[256];
  char sess[10] = {0};
  const uint8_t m1[] = {'A', 1, 2};
  const uint8_t m2[] = {'A', 9, 9, 9, 9};
  const uint8_t* msgs[] = {m1, m2};
  uint16_t lens[] = {sizeof(m1), sizeof(m2)};
  size_t n = buildMoldPacket(packet, sizeof(packet), sess, 1, msgs, lens, 2);
  ASSERT_GT(n, kMoldHeaderBytes);

  // Chop off the last 3 bytes of m2's payload.
  size_t truncated = n - 3;
  MoldMessageIterator it(packet + kMoldHeaderBytes,
                         truncated - kMoldHeaderBytes);
  uint16_t outLen;
  ASSERT_NE(it.next(outLen), nullptr);   // m1 ok
  EXPECT_EQ(it.next(outLen), nullptr);   // m2 truncated -> nullptr
}

TEST(MoldUdp64Test, BigEndianSequenceNumberAcrossBoundary) {
  // Sequence numbers larger than 2^32 to confirm the hi/lo word handling.
  uint8_t packet[64];
  char sess[10] = {0};
  uint64_t seq = (uint64_t{1} << 40) | 12345;
  buildMoldPacket(packet, sizeof(packet), sess, seq, nullptr, nullptr, 0);
  MoldHeader h;
  parseMoldHeader(packet, h);
  EXPECT_EQ(h.sequenceNumber, seq);
}

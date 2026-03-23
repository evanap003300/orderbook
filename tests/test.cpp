/* 
    * This file contains unit tests for the order book matching engine.
    * It uses the Googletest testing framework to define and run test cases.
*/

#include <gtest/gtest.h>
#include "itch.hpp"
#include "matching_engine.hpp"
#include "orderbook.hpp"

class MatchingEngineTest : public ::testing::Test {
protected:
    MatchingEngine engine;
    OrderBook orderBook;
    ItchParser parser;
    Order bidOrder;
    Order askOrder;

    void SetUp() override {
        engine = MatchingEngine();
        orderBook = OrderBook();
        parser = ItchParser();
        bidOrder = Order();
        askOrder = Order();

        bidOrder.messageType = 'A';
        bidOrder.stockLocate = 1;
        bidOrder.trackingNumber = 1;
        bidOrder.timestampHigh = 0;
        bidOrder.timestampLow = 0;
        bidOrder.orderReferenceNumberHigh = 0;
        bidOrder.orderReferenceNumberLow = 1;
        bidOrder.buySellIndicator = 'B';
        bidOrder.shares = 100;
        strncpy(bidOrder.stock, "TEST    ", 8);
        bidOrder.price = 1000;

        askOrder.messageType = 'A';
        askOrder.stockLocate = 1;
        askOrder.trackingNumber = 2;
        askOrder.timestampHigh = 0;
        askOrder.timestampLow = 0;
        askOrder.orderReferenceNumberHigh = 0;
        askOrder.orderReferenceNumberLow = 2;
        askOrder.buySellIndicator = 'S';
        askOrder.shares = 100;
        strncpy(askOrder.stock, "TEST    ", 8);
        askOrder.price = 900;
    }

    void TearDown() override {}
};


/* Simple Functionality
* Test adding a bid order 
* Test adding a ask order 
* Reading in orders works
*/
TEST_F(MatchingEngineTest, PlacesBidOrder) {
    std::vector<ItchOrderExecuted> executedOrders = orderBook.handleOrder(bidOrder);
    EXPECT_TRUE(executedOrders.empty());
}

TEST_F(MatchingEngineTest, PlacesAskOrder) {
    std::vector<ItchOrderExecuted> executedOrders = orderBook.handleOrder(askOrder);
    EXPECT_TRUE(executedOrders.empty());
}

TEST_F(MatchingEngineTest, ReadsInOrders) {
    const uint32_t numOrders = 1000;
    parser.generateItch(numOrders);
    auto orders = parser.readItch();
    EXPECT_EQ(orders.size(), numOrders);
}

/* Edge Cases to Cover
* Price should have precedence
* Time should have precedence if there is a tie
* Current order should match with multiple orders until there is no more stock or no more orders
* No match should occur if buying price is lower than ask
* No match should occur if selling price is higher than bid
*/

TEST_F(MatchingEngineTest, PricePrecedence) {
    Order higherBid = bidOrder;
    higherBid.price = 1100;
    higherBid.shares = 50;
    orderBook.handleOrder(higherBid);
    orderBook.handleOrder(bidOrder);

    std::vector<ItchOrderExecuted> executedOrders = orderBook.handleOrder(askOrder);
    EXPECT_EQ(executedOrders.size(), 2);
    EXPECT_EQ(executedOrders[0].executed_shares, 50);
}

TEST_F(MatchingEngineTest, TimePrecedence) {
    uint32_t startShares = 1000;
    Order earlierBid = bidOrder;
    earlierBid.price = 1000;
    earlierBid.shares = startShares;
    earlierBid.orderReferenceNumberLow = 10;
    orderBook.handleOrder(earlierBid);
    orderBook.handleOrder(bidOrder);

    std::vector<ItchOrderExecuted> executedOrders = orderBook.handleOrder(askOrder);
    EXPECT_EQ(10, executedOrders[0].order_reference_number);
}

TEST_F(MatchingEngineTest, PartialFill) {
    Order smallAsk = askOrder;
    smallAsk.price = 800;
    smallAsk.shares = 50;
    orderBook.handleOrder(smallAsk);
    orderBook.handleOrder(askOrder);

    std::vector<ItchOrderExecuted> executedOrders = orderBook.handleOrder(bidOrder);
    EXPECT_EQ(executedOrders.size(), 2);
    EXPECT_EQ(executedOrders[0].executed_shares, 50);
}

TEST_F(MatchingEngineTest, NoMatchHighAsk) {
    Order highAsk = askOrder;
    highAsk.price = 1100;
    orderBook.handleOrder(highAsk);

    std::vector<ItchOrderExecuted> executedOrders = orderBook.handleOrder(bidOrder);
    EXPECT_TRUE(executedOrders.empty());
}

TEST_F(MatchingEngineTest, NoMatchLowBid) {
    Order lowBid = bidOrder;
    lowBid.price = 800;
    orderBook.handleOrder(lowBid);

    std::vector<ItchOrderExecuted> executedOrders = orderBook.handleOrder(askOrder);
    EXPECT_TRUE(executedOrders.empty());
}
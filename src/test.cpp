#include "itch.hpp"
#include "test.hpp"

void Test::generateItch() {
    ItchParser parser{};
    parser.generateItch();
}

void Test::readItch() {
    ItchParser parser{};
    Order order = parser.readItch();
    printf("Message Type: %c\n", order.messageType);
    printf("Stock Locate: %u\n", order.stockLocate);
    printf("Tracking Number: %u\n", order.trackingNumber);
    printf("Timestamp High: %u\n", order.timestampHigh);
    printf("Timestamp Low: %u\n", order.timestampLow);
    printf("Order Reference Number High: %u\n", order.orderReferenceNumberHigh);
    printf("Order Reference Number Low: %u\n", order.orderReferenceNumberLow);
    printf("Buy/Sell Indicator: %c\n", order.buySellIndicator);
    printf("Shares: %u\n", order.shares);
    printf("Stock: %s\n", order.stock);
    printf("Price: %u\n", order.price);
}
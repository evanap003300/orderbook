#include "itch.hpp"

Order ItchParser::readItch() {
    std::ifstream file("itch.bin", std::ios::binary);
    char messageType;
    file.read(&messageType, sizeof(messageType));
    
    switch (messageType) {
        case 'A':
            return readAddOrder();
        default:
            throw std::runtime_error("Unsupported message type");
    }
}

Order ItchParser::readAddOrder() {
    std::ifstream file("itch.bin", std::ios::binary);
    uint16_t stockLocate;
    file.read(reinterpret_cast<char*>(&stockLocate), sizeof(stockLocate));
    stockLocate = ntohs(stockLocate);

    uint16_t trackingNumber;
    file.read(reinterpret_cast<char*>(&trackingNumber), sizeof(trackingNumber));
    trackingNumber = ntohs(trackingNumber);

    uint16_t timestampHigh;
    uint32_t timestampLow;
    file.read(reinterpret_cast<char*>(&timestampHigh), sizeof(timestampHigh));
    file.read(reinterpret_cast<char*>(&timestampLow), sizeof(timestampLow));
    timestampHigh = ntohs(timestampHigh);
    timestampLow = ntohl(timestampLow);
    
    uint32_t orderReferenceNumberHigh;
    uint32_t orderReferenceNumberLow;
    file.read(reinterpret_cast<char*>(&orderReferenceNumberHigh), sizeof(orderReferenceNumberHigh));
    file.read(reinterpret_cast<char*>(&orderReferenceNumberLow), sizeof(orderReferenceNumberLow));
    orderReferenceNumberHigh = ntohl(orderReferenceNumberHigh);
    orderReferenceNumberLow = ntohl(orderReferenceNumberLow);

    char buySellIndicator;
    file.read(&buySellIndicator, sizeof(buySellIndicator));
    
    uint32_t shares;
    file.read(reinterpret_cast<char*>(&shares), sizeof(shares));
    shares = ntohl(shares);
    
    char stock[8];
    file.read(stock, sizeof(stock));
    
    uint32_t price;
    file.read(reinterpret_cast<char*>(&price), sizeof(price));
    price = ntohl(price);

    file.close();

    Order order;
    order.messageType = 'A';
    order.stockLocate = stockLocate;
    order.trackingNumber = trackingNumber;
    order.timestampHigh = timestampHigh;
    order.timestampLow = timestampLow;
    order.orderReferenceNumberHigh = orderReferenceNumberHigh;
    order.orderReferenceNumberLow = orderReferenceNumberLow;
    order.buySellIndicator = buySellIndicator;
    order.shares = shares;
    std::move(std::begin(stock), std::end(stock), std::begin(order.stock));
    order.price = price;
    return order;
}

/* 
 * Generates synthetic ITCH messages and writes them to a binary file.
*/
void ItchParser::generateItch() {
    std::ofstream file("itch.bin", std::ios::binary);

    char messageType = 'A';
    file.write(&messageType, sizeof(messageType));

    uint16_t stockLocate = htons(100);
    file.write(reinterpret_cast<char*>(&stockLocate), sizeof(stockLocate));

    // Can skip this when parsing for efficency
    uint16_t trackingNumber = htons(0);
    file.write(reinterpret_cast<char*>(&trackingNumber), sizeof(trackingNumber));

    uint32_t timestampLow = htonl(0);
    uint16_t timestampHigh = htons(0);
    file.write(reinterpret_cast<char*>(&timestampHigh), sizeof(timestampHigh));
    file.write(reinterpret_cast<char*>(&timestampLow), sizeof(timestampLow));

    uint32_t orderReferenceNumberLow = htonl(1234);
    uint32_t orderReferenceNumberHigh = htonl(0);
    file.write(reinterpret_cast<char*>(&orderReferenceNumberHigh), sizeof(orderReferenceNumberHigh));
    file.write(reinterpret_cast<char*>(&orderReferenceNumberLow), sizeof(orderReferenceNumberLow));

    char buySellIndicator = 'B';
    file.write(&buySellIndicator, sizeof(buySellIndicator));

    uint32_t shares = htonl(1000);
    file.write(reinterpret_cast<char*>(&shares), sizeof(shares));

    char stock[8] = "AAPL   ";
    stock[7] = ' '; 
    file.write(stock, sizeof(stock));

    uint32_t price = htonl(15000);
    file.write(reinterpret_cast<char*>(&price), sizeof(price));

    file.close();
}
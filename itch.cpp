#include "itch.hpp"

void ItchParser::readItch(uint32_t order) {
    return;
}

/* 
 * Generates synthetic ITCH messages and writes them to a binary file.
*/
void ItchParser::generateItch() {
    std::ofstream file("itch.bin", std::ios::binary);

    char messageType = 'A';
    file.write(&messageType, sizeof(messageType));

    uint16_t stockLocate = 100;
    file.write(reinterpret_cast<char*>(&stockLocate), sizeof(stockLocate));

    // Can skip this when parsing for efficency
    uint16_t trackingNumber = 0;
    file.write(reinterpret_cast<char*>(&trackingNumber), sizeof(trackingNumber));

    uint32_t timestampLow = 0;
    uint16_t timestampHigh = 0;
    file.write(reinterpret_cast<char*>(&timestampLow), sizeof(timestampLow));
    file.write(reinterpret_cast<char*>(&timestampHigh), sizeof(timestampHigh));

    uint64_t orderReferenceNumber = 123456789;
    file.write(reinterpret_cast<char*>(&orderReferenceNumber), sizeof(orderReferenceNumber));

    char buySellIndicator = 'B';
    file.write(&buySellIndicator, sizeof(buySellIndicator));

    uint32_t shares = 1000;
    file.write(reinterpret_cast<char*>(&shares), sizeof(shares));

    char stock[8] = "AAPL   ";
    stock[7] = ' '; 
    file.write(stock, sizeof(stock));

    uint32_t price = 15000;
    file.write(reinterpret_cast<char*>(&price), sizeof(price));

    file.close();
}
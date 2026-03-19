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
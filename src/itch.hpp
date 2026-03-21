#pragma once
#include <stdint.h>
#include <stdio.h>
#include <fstream>
#include <arpa/inet.h>
#include <string>

struct Order {
    char messageType;
    uint16_t stockLocate;
    uint16_t trackingNumber;
    uint16_t timestampHigh;
    uint32_t timestampLow;
    uint32_t orderReferenceNumberHigh;
    uint32_t orderReferenceNumberLow;
    char buySellIndicator;
    uint32_t shares;
    char stock[8];
    uint32_t price;
};

class ItchParser {
public:
    std::vector<Order> readItch();
    Order readAddOrder(std::ifstream& file);
    void generateItch();
};
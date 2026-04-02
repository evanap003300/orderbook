#pragma once
#include <arpa/inet.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstring>
#include <fstream>
#include <string>
#include <vector>

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

struct DeleteOrder {
  char messageType;
  uint16_t stockLocate;
  uint16_t trackingNumber;
  uint16_t timestampHigh;
  uint32_t timestampLow;
  uint32_t orderReferenceNumberHigh;
  uint32_t orderReferenceNumberLow;
};

class ItchParser {
 public:
  std::vector<Order> readItch(std::string fileName);
  Order readAddOrder(const char*& data);
  DeleteOrder readDeleteOrder(const char*& data);
};
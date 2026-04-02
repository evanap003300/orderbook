#pragma once
#include <arpa/inet.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstring>
#include <string>
#include <vector>

struct Order {
  uint32_t orderReferenceNumberHigh;
  uint32_t orderReferenceNumberLow;
  uint32_t shares;
  uint32_t price;
  char buySellIndicator;
};

struct DeleteOrder {
  uint32_t orderReferenceNumberHigh;
  uint32_t orderReferenceNumberLow;
};

class ItchParser {
 public:
  Order readAddOrder(const char*& data);
  DeleteOrder readDeleteOrder(const char*& data);
};
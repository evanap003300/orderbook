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

// Parsed form of 'E' (Order Executed), 'C' (Executed With Price), and
// 'X' (Order Cancel). All three carry the same fields we care about.
struct OrderExecutedMsg {
  uint64_t orderRef;
  uint32_t shares;  // executedShares for E/C, cancelledShares for X
};

// Parsed form of 'U' (Order Replace).
struct OrderReplaceMsg {
  uint64_t originalOrderRef;
  uint64_t newOrderRef;
  uint32_t shares;
  uint32_t price;
};

class ItchParser {
 public:
  Order            readAddOrder(const char*& data);
  DeleteOrder      readDeleteOrder(const char*& data);
  OrderExecutedMsg readOrderExecuted(const char*& data);  // E, C, X
  OrderReplaceMsg  readOrderReplace(const char*& data);   // U
};
#include "itch.hpp"

// Skip the common 10-byte header present in every ITCH message after the type
// byte: stockLocate(2) + trackingNumber(2) + timestamp(6).
static void skipHeader(const char*& data) { data += 10; }

// Read a big-endian uint32 and advance the pointer.
static uint32_t readU32(const char*& data) {
  uint32_t v;
  memcpy(&v, data, 4);
  data += 4;
  return ntohl(v);
}

// Read an 8-byte big-endian order reference number (stored as two uint32s).
static uint64_t readRef(const char*& data) {
  uint32_t hi = readU32(data);
  uint32_t lo = readU32(data);
  return (static_cast<uint64_t>(hi) << 32) | lo;
}

DeleteOrder ItchParser::readDeleteOrder(const char*& data) {
  skipHeader(data);
  uint64_t ref = readRef(data);
  return {static_cast<uint32_t>(ref >> 32), static_cast<uint32_t>(ref)};
}

Order ItchParser::readAddOrder(const char*& data) {
  skipHeader(data);
  uint64_t ref = readRef(data);
  char side = *data++;
  uint32_t shares = readU32(data);
  data += 8;  // stock symbol (8 ASCII bytes, not needed for matching)
  uint32_t price = readU32(data);

  Order order;
  order.orderReferenceNumberHigh = static_cast<uint32_t>(ref >> 32);
  order.orderReferenceNumberLow  = static_cast<uint32_t>(ref);
  order.buySellIndicator = side;
  order.shares = shares;
  order.price  = price;
  return order;
}

// Used for 'E' (Order Executed), 'C' (Executed With Price), and 'X' (Cancel).
// All three share the same wire layout up through the shares field.
OrderExecutedMsg ItchParser::readOrderExecuted(const char*& data) {
  skipHeader(data);
  uint64_t ref    = readRef(data);
  uint32_t shares = readU32(data);
  return {ref, shares};
}

// 'U' — Order Replace: cancel the original ref, add a replacement at a new
// price and quantity. The side is not in the wire message; the engine inherits
// it from the original resting order.
OrderReplaceMsg ItchParser::readOrderReplace(const char*& data) {
  skipHeader(data);
  uint64_t origRef = readRef(data);
  uint64_t newRef  = readRef(data);
  uint32_t shares  = readU32(data);
  uint32_t price   = readU32(data);
  return {origRef, newRef, shares, price};
}

#include "itch.hpp"

/*
 * Reads a Delete Order message from itch format and returns a DeleteOrder
 * struct.
 */
DeleteOrder ItchParser::readDeleteOrder(const char*& data) {
  // Skip fields we don't need for the matching engine
  data += sizeof(uint16_t);
  data += sizeof(uint16_t);
  data += sizeof(uint32_t) + sizeof(uint16_t);

  uint32_t orderReferenceNumberHigh;
  uint32_t orderReferenceNumberLow;
  memcpy(&orderReferenceNumberHigh, data, sizeof(orderReferenceNumberHigh));
  memcpy(&orderReferenceNumberLow, data + sizeof(orderReferenceNumberHigh),
         sizeof(orderReferenceNumberLow));
  data += sizeof(orderReferenceNumberHigh) + sizeof(orderReferenceNumberLow);
  orderReferenceNumberHigh = ntohl(orderReferenceNumberHigh);
  orderReferenceNumberLow = ntohl(orderReferenceNumberLow);

  DeleteOrder order;
  order.orderReferenceNumberHigh = orderReferenceNumberHigh;
  order.orderReferenceNumberLow = orderReferenceNumberLow;

  return order;
}

/*
 * Reads an Add Order message from itch format and returns an Order struct.
 */
Order ItchParser::readAddOrder(const char*& data) {
  // Skip fields we don't need for the matching engine
  data += sizeof(uint16_t);
  data += sizeof(uint16_t);
  data += sizeof(uint32_t) + sizeof(uint16_t);

  uint32_t orderReferenceNumberHigh;
  uint32_t orderReferenceNumberLow;
  memcpy(&orderReferenceNumberHigh, data, sizeof(orderReferenceNumberHigh));
  memcpy(&orderReferenceNumberLow, data + sizeof(orderReferenceNumberHigh),
         sizeof(orderReferenceNumberLow));
  data += sizeof(orderReferenceNumberHigh) + sizeof(orderReferenceNumberLow);
  orderReferenceNumberHigh = ntohl(orderReferenceNumberHigh);
  orderReferenceNumberLow = ntohl(orderReferenceNumberLow);

  char buySellIndicator = *data;
  data++;

  uint32_t shares;
  memcpy(&shares, data, sizeof(shares));
  shares = ntohl(shares);
  data += sizeof(shares);
  data += sizeof(char) * 8;

  uint32_t price;
  memcpy(&price, data, sizeof(price));
  price = ntohl(price);
  data += sizeof(price);

  Order order;
  order.orderReferenceNumberHigh = orderReferenceNumberHigh;
  order.orderReferenceNumberLow = orderReferenceNumberLow;
  order.buySellIndicator = buySellIndicator;
  order.shares = shares;
  order.price = price;
  return order;
}

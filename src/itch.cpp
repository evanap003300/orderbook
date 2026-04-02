#include "itch.hpp"

DeleteOrder ItchParser::readDeleteOrder(const char*& data) {
  uint16_t stockLocate;
  memcpy(&stockLocate, data, sizeof(stockLocate));
  stockLocate = ntohs(stockLocate);
  data += sizeof(stockLocate);

  uint16_t trackingNumber;
  memcpy(&trackingNumber, data, sizeof(trackingNumber));
  trackingNumber = ntohs(trackingNumber);
  data += sizeof(trackingNumber);

  uint16_t timestampHigh;
  uint32_t timestampLow;
  memcpy(&timestampHigh, data, sizeof(timestampHigh));
  memcpy(&timestampLow, data + sizeof(timestampHigh), sizeof(timestampLow));
  timestampHigh = ntohs(timestampHigh);
  timestampLow = ntohl(timestampLow);
  data += sizeof(timestampHigh) + sizeof(timestampLow);

  uint32_t orderReferenceNumberHigh;
  uint32_t orderReferenceNumberLow;
  memcpy(&orderReferenceNumberHigh, data, sizeof(orderReferenceNumberHigh));
  memcpy(&orderReferenceNumberLow, data + sizeof(orderReferenceNumberHigh),
         sizeof(orderReferenceNumberLow));
  data += sizeof(orderReferenceNumberHigh) + sizeof(orderReferenceNumberLow);
  orderReferenceNumberHigh = ntohl(orderReferenceNumberHigh);
  orderReferenceNumberLow = ntohl(orderReferenceNumberLow);

  DeleteOrder order;
  order.messageType = 'D';
  order.stockLocate = stockLocate;
  order.trackingNumber = trackingNumber;
  order.timestampHigh = timestampHigh;
  order.timestampLow = timestampLow;
  order.orderReferenceNumberHigh = orderReferenceNumberHigh;
  order.orderReferenceNumberLow = orderReferenceNumberLow;

  return order;
}

/*
 * Reads an Add Order message from itch format and returns an Order struct.
 */
Order ItchParser::readAddOrder(const char*& data) {
  uint16_t stockLocate;
  memcpy(&stockLocate, data, sizeof(stockLocate));
  stockLocate = ntohs(stockLocate);
  data += sizeof(stockLocate);

  uint16_t trackingNumber;
  memcpy(&trackingNumber, data, sizeof(trackingNumber));
  trackingNumber = ntohs(trackingNumber);
  data += sizeof(trackingNumber);

  uint16_t timestampHigh;
  uint32_t timestampLow;
  memcpy(&timestampHigh, data, sizeof(timestampHigh));
  memcpy(&timestampLow, data + sizeof(timestampHigh), sizeof(timestampLow));
  timestampHigh = ntohs(timestampHigh);
  timestampLow = ntohl(timestampLow);
  data += sizeof(timestampHigh) + sizeof(timestampLow);

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

  char stock[8];
  memcpy(stock, data, sizeof(stock));
  data += sizeof(stock);

  uint32_t price;
  memcpy(&price, data, sizeof(price));
  price = ntohl(price);
  data += sizeof(price);

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
  std::copy(std::begin(stock), std::end(stock), std::begin(order.stock));
  order.price = price;
  return order;
}

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

std::vector<Order> ItchParser::readItch(std::string fileName) {
  /*std::ifstream file(fileName, std::ios::binary);

  if (!file.is_open()) {
    throw std::runtime_error("Could not open file");
  }

  std::vector<Order> orders;
  uint32_t capacity = 100000;
  uint16_t messageLength;
  char messageType;
  while (file.read(reinterpret_cast<char*>(&messageLength),
                   sizeof(messageLength))) {
    messageLength = ntohs(messageLength);
    file.read(&messageType, sizeof(messageType));

    switch (messageType) {
      case 'A':
        if (orders.size() < capacity) {
          orders.push_back(readAddOrder(file));
        }
        break;
      default:
        file.seekg(messageLength - 1, std::ios::cur);
        break;
    }
  }

  file.close();
  return orders; */
  return {};
}

void ItchParser::generateSyntheticOrder(std::ofstream& file, bool buyOrder,
                                        uint32_t orderReferenceNumber) {
  uint16_t messageLength = htons(36);
  file.write(reinterpret_cast<char*>(&messageLength), sizeof(messageLength));

  char messageType = 'A';
  file.write(&messageType, sizeof(messageType));

  uint16_t stockLocate = htons(100);
  file.write(reinterpret_cast<char*>(&stockLocate), sizeof(stockLocate));

  uint16_t trackingNumber = htons(0);
  file.write(reinterpret_cast<char*>(&trackingNumber), sizeof(trackingNumber));

  // Add current timestamp later
  uint32_t timestampLow = htonl(0);
  uint16_t timestampHigh = htons(0);
  file.write(reinterpret_cast<char*>(&timestampHigh), sizeof(timestampHigh));
  file.write(reinterpret_cast<char*>(&timestampLow), sizeof(timestampLow));

  uint32_t orderReferenceNumberLow = htonl(orderReferenceNumber);
  uint32_t orderReferenceNumberHigh = htonl(0);
  file.write(reinterpret_cast<char*>(&orderReferenceNumberHigh),
             sizeof(orderReferenceNumberHigh));
  file.write(reinterpret_cast<char*>(&orderReferenceNumberLow),
             sizeof(orderReferenceNumberLow));

  // Randomize buy/sell indicator
  char buySellIndicator = buyOrder ? 'B' : 'S';
  file.write(&buySellIndicator, sizeof(buySellIndicator));

  // Randomize shares
  uint32_t sharesRandom = std::rand();
  uint32_t shares = htonl(sharesRandom);
  file.write(reinterpret_cast<char*>(&shares), sizeof(shares));

  // Pick a random ticker
  std::vector<std::string> tickers = {"AAPL", "MSFT", "GOOGL", "AMZN",
                                      "NVDA", "TSLA", "META"};
  std::string chosenTicker = tickers[std::rand() % tickers.size()];
  char stock[8] = {' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '};
  std::copy(chosenTicker.begin(), chosenTicker.end(), stock);
  file.write(stock, sizeof(stock));

  // Randomize Price
  uint32_t priceRandom = 14900 + (std::rand() % 200);
  uint32_t price = htonl(priceRandom);
  file.write(reinterpret_cast<char*>(&price), sizeof(price));
}

/*
 * Generates synthetic ITCH messages and writes them to a binary file.
 */
void ItchParser::generateItch(uint32_t numOrders) {
  std::ofstream file("itch.bin", std::ios::binary);

  for (uint32_t i = 0; i < numOrders; i++) {
    generateSyntheticOrder(file, (i % 2) == 0, i + 1);
  }

  file.close();
}
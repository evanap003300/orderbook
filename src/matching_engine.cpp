#include "matching_engine.hpp"

#include <time.h>

#include <cstdio>
#include <string>

#include "tsc.hpp"

MatchingEngine::MatchingEngine() : pool(67108864), orderMap(8388608) {
  // A full ITCH trading day has ~136M Add messages; reserving up front avoids
  // vector grow events during the timed loop (which show up as ms-scale tail
  // spikes). orderMap is sized for the measured peak of ~2.1M live orders:
  // 8.4M slots keeps the load factor ~25% (was 33.5M / 512 MB at ~6% load).
  latencies.reserve(150000000);
  wireLatencies.reserve(150000000);
  orderBooks.reserve(65536);
  for (uint32_t i = 0; i < 65536; i++) {
    orderBooks.emplace_back(&pool);
  }
  pool.hugepages();
  orderMap.hugepages();
#ifdef MADV_HUGEPAGE
  madvise(latencies.data(), latencies.capacity() * sizeof(uint64_t),
          MADV_HUGEPAGE);
  madvise(wireLatencies.data(), wireLatencies.capacity() * sizeof(uint64_t),
          MADV_HUGEPAGE);
#endif
}

void MatchingEngine::logExecutedOrders(
    const std::vector<ItchOrderExecuted>& executedOrders) {
  for (const auto& order : executedOrders) {
    printf("Executed Order - Executed Shares: %u\n", order.executed_shares);
  }
}

// Calibrate the TSC against CLOCK_MONOTONIC once. The TSC is invariant on
// modern CPUs (constant tick rate regardless of core P-state), so cycles/ns
// stays valid even while the governor throttles. Returns cycles per ns.
static double calibrateTsc() {
  auto monoNs = []() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<uint64_t>(ts.tv_sec) * 1000000000ull + ts.tv_nsec;
  };
  uint64_t calNs0 = monoNs();
  uint64_t calCycles0 = tscNow();
  uint64_t calNs1;
  do {
    calNs1 = monoNs();
  } while (calNs1 - calNs0 < 100000000ull);  // ~100 ms
  uint64_t calCycles1 = tscNow();
  double cyclesPerNs =
      static_cast<double>(calCycles1 - calCycles0) / (calNs1 - calNs0);
  // On non-x86 platforms tscNow() already returns nanoseconds, so the ratio
  // collapses to 1.0 and we skip the divide later via tscIsCycles().
  fprintf(stderr, "TSC calibrated: %.4f cycles/ns (%.1f MHz)\n", cyclesPerNs,
          cyclesPerNs * 1000.0);
  return cyclesPerNs;
}

// Process one ITCH message. `data` points at the message-type byte (the
// 2-byte length prefix is upstream's concern). `messageLength` is the
// ITCH-spec length: it includes the type byte but excludes the length prefix.
// `tWireCycles` is 0 in the file path; in the UDP path it's the TSC value
// stamped by the network thread when recvmsg returned.
void MatchingEngine::processMessage(const char* data, uint16_t messageLength,
                                    EngineScratch& scratch,
                                    uint64_t tWireCycles) {
  (void)messageLength;  // unused for A/D; future message types may need it
  ItchParser parser;
  char messageType = *data;
  data++;

  switch (messageType) {
    case 'A': {
      uint16_t stockLocate =
          ntohs(*reinterpret_cast<const uint16_t*>(data));
      Order order = parser.readAddOrder(data);
      scratch.removedRefs.clear();
      scratch.executedOrders.clear();
      uint32_t restingIdx;
      uint64_t startCycles = tscNow();
      orderBooks[stockLocate].handleOrder(order, restingIdx, scratch.removedRefs,
                                          scratch.executedOrders);
      uint64_t endCycles = tscNow();

      for (uint64_t ref : scratch.removedRefs) {
        orderMap.erase(ref);
      }
      if (restingIdx != INVALID_INDEX) {
        uint64_t orderReferenceNumber =
            (static_cast<uint64_t>(order.orderReferenceNumberHigh) << 32) |
            order.orderReferenceNumberLow;
        orderMap.insert(orderReferenceNumber, {stockLocate, restingIdx});
      }

      latencies.push_back(endCycles - startCycles);
      if (tWireCycles != 0) {
        wireLatencies.push_back(startCycles - tWireCycles);
      }
      break;
    }
    case 'D': {
      DeleteOrder deleteOrder = parser.readDeleteOrder(data);
      uint64_t orderReferenceNumber =
          (static_cast<uint64_t>(deleteOrder.orderReferenceNumberHigh) << 32) |
          deleteOrder.orderReferenceNumberLow;
      OrderLocation* found = orderMap.find(orderReferenceNumber);
      if (!found) break;
      orderBooks[found->stockLocate].removeByIndex(found->poolIdx);
      orderMap.erase(orderReferenceNumber);
      break;
    }
    default:
      break;
  }
}

void MatchingEngine::run() {
  std::string fileName = "itch_data.NASDAQ_ITCH50";

  int fd = open(fileName.c_str(), O_RDONLY);
  struct stat sb;
  fstat(fd, &sb);
  auto fileSize = sb.st_size;

  void* file =
      mmap(nullptr, fileSize, PROT_READ, MAP_FILE | MAP_PRIVATE, fd, 0);

  if (file == MAP_FAILED) {
    close(fd);
    return;
  }
  madvise(file, fileSize, MADV_SEQUENTIAL);

  const char* data = static_cast<const char*>(file);
  const char* end = data + fileSize;

  EngineScratch scratch;
  uint64_t messageCount = 0;

  double cyclesPerNs = calibrateTsc();

  // ITCH wire layout: [2 bytes BE length][messageLength bytes of message,
  // starting with the 1-byte message type]. After consuming the length prefix,
  // `data` points at the type byte; advancing by `messageLength` moves to the
  // next packet's length prefix.
  while (data < end) {
    uint16_t messageLength = ntohs(*reinterpret_cast<const uint16_t*>(data));
    data += 2;
    messageCount++;
    if (messageCount % 1000000 == 0) {
      printf("Progress: %.1f%%\n",
             (double)(data - static_cast<const char*>(file)) / fileSize * 100);
    }

    // Prefetch the orderMap slot for the NEXT A/D message while we work on
    // this one. Layout from the next message's length-prefix byte:
    //   +0..+1   length
    //   +2       type
    //   +3..+4   stockLocate
    //   +5..+6   trackingNumber
    //   +7..+12  timestamp (6 bytes)
    //   +13..+16 orderReferenceNumber high
    //   +17..+20 orderReferenceNumber low
    const char* nextPkt = data + messageLength;
    if (nextPkt + 21 <= end) {
      char nt = nextPkt[2];
      if (nt == 'A' || nt == 'D') {
        uint32_t hi, lo;
        memcpy(&hi, nextPkt + 13, 4);
        memcpy(&lo, nextPkt + 17, 4);
        uint64_t ref =
            (static_cast<uint64_t>(ntohl(hi)) << 32) | ntohl(lo);
        orderMap.prefetch(ref);
      }
    }

    processMessage(data, messageLength, scratch, /*tWireCycles=*/0);
    data += messageLength;
  }

  // Convert recorded cycle counts to nanoseconds, off the hot path.
  // On non-x86 platforms tscNow() already returned ns, so this is a no-op.
  if (tscIsCycles()) {
    for (uint64_t& cycles : latencies) {
      cycles = static_cast<uint64_t>(cycles / cyclesPerNs);
    }
    for (uint64_t& cycles : wireLatencies) {
      cycles = static_cast<uint64_t>(cycles / cyclesPerNs);
    }
  }

  munmap(file, fileSize);
  close(fd);
}

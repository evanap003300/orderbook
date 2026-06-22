#pragma once
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstring>
#include <limits>
#include <vector>

#include "flat_hash_map.hpp"
#include "itch.hpp"
#include "order_pool.hpp"
#include "orderbook.hpp"

// Where a resting order lives: which book owns it and its slot in the pool.
struct OrderLocation {
  uint16_t stockLocate;
  uint32_t poolIdx;
};

// Reusable scratch buffers a caller passes into processMessage() so the engine
// itself stays decoupled from how many independent message streams feed it.
struct EngineScratch {
  std::vector<uint64_t> removedRefs;
  std::vector<ItchOrderExecuted> executedOrders;
  EngineScratch() {
    removedRefs.reserve(1024);
    executedOrders.reserve(1024);
  }
};

class MatchingEngine {
 public:
  // matchLatencies = ns spent inside handleOrder for each Add (the existing
  // metric). wireLatencies = ns between the producer-stamped t_wire and the
  // consumer entering handleOrder; only populated by the UDP path. Both kept
  // as raw TSC cycles during the timed loop and converted to ns at the end.
  std::vector<uint64_t> latencies;        // alias kept for back-compat
  std::vector<uint64_t> wireLatencies;
  MatchingEngine();

  // File mode: mmap the ITCH file and feed every message through the engine.
  // (Unchanged behaviour - this is the existing benchmark path.)
  void run();

  // UDP mode entrypoint - lives in feed_handler.cpp. Spawns a network thread
  // that pushes packet-slot indices through an SPSC ring; this call returns
  // when the engine receives an end-of-session marker or `stop` is signalled.
  // netCore: CPU core to pin the network receive thread to (-1 = no pinning).
  void runUdp(const char* bindAddr, uint16_t port, const char* multicastGroup,
              int netCore = -1);

  // Process one ITCH message (already past the 2-byte length prefix; `data`
  // points at the message-type byte). Called by both file and UDP modes.
  // `tWireCycles` should be 0 for the file path (no wire timing); otherwise
  // it's the TSC value stamped by the network thread when recvmsg returned.
  void processMessage(const char* data, uint16_t messageLength,
                      EngineScratch& scratch, uint64_t tWireCycles);

 private:
  OrderPool pool;
  FlatHashMap<uint64_t, OrderLocation, std::numeric_limits<uint64_t>::max(),
              /*Identity=*/true>
      orderMap;
  std::vector<OrderBook> orderBooks;
  void logExecutedOrders(const std::vector<ItchOrderExecuted>& executedOrders);
};

#pragma once
#include <stdint.h>

#include <atomic>

#include "packet_pool.hpp"
#include "spsc_ring.hpp"

// One slot index carries everything the matching thread needs to find the
// packet body in the pool and reconstruct wire-to-match latency.
struct PacketRef {
  uint32_t poolIdx;
  uint64_t tWireCycles;  // TSC value stamped right after recvmsg returned
};

// Runtime stats the feed handler keeps for the engine to print at end of run.
struct FeedStats {
  std::atomic<uint64_t> packetsReceived{0};
  std::atomic<uint64_t> packetsDropped{0};   // ring was full
  std::atomic<uint64_t> bytesReceived{0};
  std::atomic<uint64_t> gapsDetected{0};
  std::atomic<uint64_t> duplicatesIgnored{0};
  std::atomic<uint64_t> endOfSession{0};
};

// Run the feed-handler loop. Blocks until the sender sends an
// end-of-session marker (MoldUDP64 messageCount == 0xFFFF) or `*stop` is set.
// `bindAddr`: address to bind/recv on; "0.0.0.0" for any interface.
// `port`: UDP port.
// `multicastGroup`: nullptr for unicast, or a multicast group to IGMP-join.
// `pool`: shared packet buffer pool.
// `ring`: SPSC ring the network thread pushes packet refs into.
// `stats`: counters reported back to the engine.
// `stop`: external stop flag for clean shutdown.
//
// LINUX-AGENT NOTE: This routine uses POSIX BSD sockets. On Linux, add a
// follow-up AF_XDP receive path here that bypasses the kernel network stack.
// Compare wire-to-match latency between the two; expect a large p99/tail
// reduction with AF_XDP because no syscall is on the receive path.
void runFeedHandler(const char* bindAddr, uint16_t port,
                    const char* multicastGroup, PacketPool& pool,
                    SpscRing<PacketRef>& ring, FeedStats& stats,
                    std::atomic<bool>& stop);

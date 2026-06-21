// UDP-mode entry point for MatchingEngine. Lives in its own TU so the rest of
// the engine doesn't pull in <thread> or socket headers when only the file
// path is used.

#include <atomic>
#include <cstdio>
#include <thread>

#include "feed_handler.hpp"
#include "matching_engine.hpp"
#include "moldudp64.hpp"
#include "packet_pool.hpp"
#include "spsc_ring.hpp"
#include "tsc.hpp"

// Calibration helper duplicated here to avoid exposing it in a header. Keeps
// matching_engine.cpp's static unchanged.
static double calibrateTscUdp() {
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
  } while (calNs1 - calNs0 < 100000000ull);
  uint64_t calCycles1 = tscNow();
  return static_cast<double>(calCycles1 - calCycles0) / (calNs1 - calNs0);
}

void MatchingEngine::runUdp(const char* bindAddr, uint16_t port,
                            const char* multicastGroup) {
  // Sizing: ring size is power-of-two, ~4x typical burst depth. Pool slot
  // count must be >= ring slots so the producer never starves on free buffers
  // when the consumer hasn't yet returned them.
  const size_t kRingSlots = 65536;
  const size_t kPoolSlots = 131072;

  PacketPool pool(kPoolSlots);
  SpscRing<PacketRef> ring(kRingSlots);
  FeedStats stats;
  std::atomic<bool> stop{false};

  // Pre-calibrate TSC before any timed work so wireLatencies are convertible.
  double cyclesPerNs = calibrateTscUdp();
  fprintf(stderr, "UDP mode: cyclesPerNs=%.4f (%.1f MHz)\n", cyclesPerNs,
          cyclesPerNs * 1000.0);

  // LINUX-AGENT NOTE: pin the network thread to one isolated core
  // (pthread_setaffinity_np) and the matching loop (this thread) to another.
  // On the dev machine (macOS) we can't isolate cores, so we just run them
  // unpinned for correctness checks.
  std::thread netThread([&]() {
    runFeedHandler(bindAddr, port, multicastGroup, pool, ring, stats, stop);
  });

  EngineScratch scratch;
  uint64_t messageCount = 0;
  bool sessionEnded = false;

  while (!sessionEnded) {
    PacketRef ref;
    // Bounded spin-pop: check ring, PAUSE, retry. Periodically check the stop
    // flag so we can shut down cleanly if the producer dies.
    int spinBudget = 0;
    while (!ring.pop(ref)) {
      cpu_pause();
      if (++spinBudget >= 1024) {
        spinBudget = 0;
        if (stop.load(std::memory_order_relaxed)) {
          sessionEnded = true;
          break;
        }
      }
    }
    if (sessionEnded) break;

    // End-of-session sentinel from the network thread.
    if (ref.poolIdx == kPoolInvalid) break;

    BufferSlot& slot = pool[ref.poolIdx];
    // Skip the 20-byte Mold header; iterate messages.
    MoldMessageIterator it(slot.data + kMoldHeaderBytes,
                           slot.length - kMoldHeaderBytes);
    uint16_t msgLen;
    while (const uint8_t* msg = it.next(msgLen)) {
      processMessage(reinterpret_cast<const char*>(msg), msgLen, scratch,
                     ref.tWireCycles);
      messageCount++;
      if (messageCount % 1000000 == 0) {
        fprintf(stderr, "UDP: %lu messages processed (rx=%lu drop=%lu gap=%lu)\n",
                static_cast<unsigned long>(messageCount),
                static_cast<unsigned long>(
                    stats.packetsReceived.load(std::memory_order_relaxed)),
                static_cast<unsigned long>(
                    stats.packetsDropped.load(std::memory_order_relaxed)),
                static_cast<unsigned long>(
                    stats.gapsDetected.load(std::memory_order_relaxed)));
      }
    }
    pool.release(ref.poolIdx);
  }

  stop.store(true, std::memory_order_relaxed);
  if (netThread.joinable()) netThread.join();

  // Convert the recorded TSC deltas to nanoseconds (no-op on platforms where
  // tscNow() is already ns).
  if (tscIsCycles()) {
    for (uint64_t& c : latencies) c = static_cast<uint64_t>(c / cyclesPerNs);
    for (uint64_t& c : wireLatencies) c = static_cast<uint64_t>(c / cyclesPerNs);
  }

  fprintf(stderr,
          "UDP run complete:\n"
          "  matched messages: %lu\n"
          "  packets received: %lu\n"
          "  packets dropped:  %lu\n"
          "  bytes received:   %lu\n"
          "  gaps detected:    %lu\n"
          "  duplicates:       %lu\n",
          static_cast<unsigned long>(messageCount),
          static_cast<unsigned long>(
              stats.packetsReceived.load(std::memory_order_relaxed)),
          static_cast<unsigned long>(
              stats.packetsDropped.load(std::memory_order_relaxed)),
          static_cast<unsigned long>(
              stats.bytesReceived.load(std::memory_order_relaxed)),
          static_cast<unsigned long>(
              stats.gapsDetected.load(std::memory_order_relaxed)),
          static_cast<unsigned long>(
              stats.duplicatesIgnored.load(std::memory_order_relaxed)));
}

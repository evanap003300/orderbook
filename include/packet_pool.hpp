#pragma once
#include <stdint.h>

#include <atomic>
#include <cstddef>
#include <cstring>
#include <vector>

// LINUX-AGENT NOTE: PacketPool is the buffer arena that the SPSC ring carries
// indices into. The producer (net thread) reserves a free index, fills it
// with the received UDP packet, and pushes the index. The consumer (matching
// thread) pops the index, processes the packet, and returns the index. No
// allocation in the hot path. Returned indices are reused via a Treiber-stack
// free-list using `nextFree` inside the BufferSlot itself.
//
// The free-list pop/push uses CAS so the pool itself is safe for SPSC use
// without requiring producer and consumer to coordinate. In an SPSC setup
// only the producer reserves and only the consumer releases (or vice versa,
// depending on direction), so contention is bounded -- this is just being
// defensive in case we ever extend to MPSC later.

// Max UDP datagram we accept. NASDAQ MoldUDP64 packets are at most a few KB;
// 2 KiB covers typical jumbo-friendly setups while keeping the per-slot cost
// reasonable. Tune up if a real exchange feed needs more.
constexpr size_t kMaxPacketBytes = 2048;

struct BufferSlot {
  uint8_t data[kMaxPacketBytes];
  uint16_t length;          // actual bytes used
  uint32_t nextFree;        // free-list link
};

constexpr uint32_t kPoolInvalid = UINT32_MAX;

class PacketPool {
 public:
  explicit PacketPool(size_t numSlots) : slots(numSlots) {
    // Pre-link all slots into the free-list: 0 -> 1 -> 2 -> ... -> last -> end
    for (size_t i = 0; i + 1 < numSlots; ++i) {
      slots[i].nextFree = static_cast<uint32_t>(i + 1);
    }
    slots[numSlots - 1].nextFree = kPoolInvalid;
    freeHead.store(0, std::memory_order_release);
  }

  // Pop a free slot index. Returns kPoolInvalid if pool exhausted.
  uint32_t acquire() {
    uint32_t head = freeHead.load(std::memory_order_acquire);
    while (head != kPoolInvalid) {
      uint32_t next = slots[head].nextFree;
      if (freeHead.compare_exchange_weak(head, next, std::memory_order_acq_rel,
                                         std::memory_order_acquire)) {
        return head;
      }
    }
    return kPoolInvalid;
  }

  // Return a slot to the free-list.
  void release(uint32_t idx) {
    uint32_t head = freeHead.load(std::memory_order_acquire);
    do {
      slots[idx].nextFree = head;
    } while (!freeHead.compare_exchange_weak(
        head, idx, std::memory_order_acq_rel, std::memory_order_acquire));
  }

  BufferSlot& operator[](uint32_t idx) { return slots[idx]; }
  const BufferSlot& operator[](uint32_t idx) const { return slots[idx]; }

 private:
  std::vector<BufferSlot> slots;
  std::atomic<uint32_t> freeHead{kPoolInvalid};
};

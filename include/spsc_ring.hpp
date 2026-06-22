#pragma once
#include <stdint.h>

#include <atomic>
#include <cstddef>
#include <cstdlib>
#include <new>
#include <vector>

// LINUX-AGENT NOTE: This SPSC ring is the spine of the UDP feed pipeline.
// Net thread (producer) pushes packet buffer indices; matching thread
// (consumer) pops them. Cache-line-padded head/tail prevents false sharing
// between the two cores. Pair this with isolcpus + taskset for the two
// participating cores; the consumer busy-spins with PAUSE so the matching
// thread should live on an isolated core to make that cost free.

#ifdef __cpp_lib_hardware_interference_size
constexpr size_t kCacheLineSize = std::hardware_destructive_interference_size;
#else
constexpr size_t kCacheLineSize = 64;
#endif

// Portable CPU PAUSE / yield hint used inside the busy-wait. On x86 this maps
// to the `pause` instruction (lowers contention on the inter-core bus and
// saves a small amount of power); on ARM/Apple silicon it maps to YIELD.
static inline void cpu_pause() {
#if defined(__x86_64__) || defined(__i386__)
  __asm__ __volatile__("pause" ::: "memory");
#elif defined(__aarch64__) || defined(__arm__)
  __asm__ __volatile__("yield" ::: "memory");
#else
  std::atomic_signal_fence(std::memory_order_seq_cst);
#endif
}

// Single-producer / single-consumer lock-free ring of T values.
//
// Capacity must be a power of two; we mask instead of modulo. Head and tail
// each live on their own cache line so the producer's writes to `tail` don't
// invalidate the consumer's cached `head` (and vice versa) -- without this,
// every push/pop costs ~tens of nanoseconds of cache-coherence traffic.
//
// Memory ordering: producer release-stores `tail`, consumer acquire-loads it
// (and symmetrically for `head`). This is the standard SPSC pattern and is
// sufficient -- no CAS, no full fences.
//
// Empty if head == tail; full if (tail + 1) == head (one slot kept open so
// we can distinguish full from empty without a separate count).
template <typename T>
class SpscRing {
 public:
  explicit SpscRing(size_t capacity)
      : mask(capacity - 1), buffer(capacity) {
    // power-of-two enforcement
    if ((capacity & (capacity - 1)) != 0 || capacity == 0) {
      std::abort();
    }
  }

  // Producer side. Returns false if the ring is full (caller drops on full).
  bool push(const T& value) {
    const size_t t = tail.load(std::memory_order_relaxed);
    const size_t next = (t + 1) & mask;
    // acquire-load head so we see the consumer's progress
    if (next == head.load(std::memory_order_acquire)) {
      return false;  // full
    }
    buffer[t] = value;
    // release-store tail so the consumer observes the slot's new value
    // before it sees the updated tail
    tail.store(next, std::memory_order_release);
    return true;
  }

  // Consumer side. Returns false if empty (caller decides to spin / yield).
  bool pop(T& out) {
    const size_t h = head.load(std::memory_order_relaxed);
    if (h == tail.load(std::memory_order_acquire)) {
      return false;  // empty
    }
    out = buffer[h];
    head.store((h + 1) & mask, std::memory_order_release);
    return true;
  }

  // Busy-wait pop that spins with CPU PAUSE. Pair the matching thread with an
  // isolated core (Linux: isolcpus + taskset -c <core>) so the spin is free.
  // Returns the popped value once available. There is no stop condition; the
  // caller can break out by checking some external flag and using pop() in a
  // bounded loop instead.
  T spinPop() {
    T out;
    while (!pop(out)) {
      cpu_pause();
    }
    return out;
  }

 private:
  alignas(kCacheLineSize) std::atomic<size_t> head{0};
  // Pad away from `head` so consumer and producer don't share a cache line.
  char pad1[kCacheLineSize - sizeof(std::atomic<size_t>)]{};
  alignas(kCacheLineSize) std::atomic<size_t> tail{0};
  char pad2[kCacheLineSize - sizeof(std::atomic<size_t>)]{};
  size_t mask;
  std::vector<T> buffer;
};

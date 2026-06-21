#pragma once
#include <stdint.h>
#include <time.h>

// Portable monotonic cycle counter. On x86_64 with the Linux build chain we
// use __rdtscp (invariant TSC) for the lowest-overhead high-resolution timer.
// On other platforms (notably Apple silicon during development) we fall back
// to CLOCK_MONOTONIC_RAW in nanoseconds. The exact tick units differ between
// platforms; calibrateTsc() turns ticks into ns either way.

#if defined(__x86_64__) || defined(__i386__)
#include <x86intrin.h>
static inline uint64_t tscNow() {
  unsigned aux;
  return __rdtscp(&aux);
}
static inline bool tscIsCycles() { return true; }
#else
static inline uint64_t tscNow() {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return static_cast<uint64_t>(ts.tv_sec) * 1000000000ull + ts.tv_nsec;
}
static inline bool tscIsCycles() { return false; }  // already in ns
#endif

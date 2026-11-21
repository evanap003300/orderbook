# Matching Engine

[![CI](https://github.com/evanap003300/orderbook/actions/workflows/ci.yml/badge.svg)](https://github.com/evanap003300/orderbook/actions/workflows/ci.yml)

A C++17 limit order book and matching engine that replays a full day of NASDAQ
TotalView-ITCH 5.0 market data (~10 GB) and measures how long each book
operation takes.

## Results

162.8M Add messages, full trading day (01/30/2019). Linux i5-8350U, isolated
core, performance governor @ 3.6 GHz, TSC timing. Times cover `handleOrder` —
matching plus book insert.

| Stat | Value |
|------|------:|
| mean | 85.5 ns |
| p50 | 53 ns |
| p90 | 170 ns |
| p99 | 437 ns |
| p99.9 | 2,021 ns |
| p99.99 | 6,466 ns |
| wall time | 49.4 s for the full 10.2 GB file |

The first version of this, built on `std::map` and `std::unordered_map`, had a
p99.9 of 474 µs. [How it got here](#how-it-got-here) has the steps in between.

## Build & run

```sh
cmake -S . -B build
cmake --build build

./build/run_tests          # 56 tests, ~1s
./build/order_matching     # reads itch_data.NASDAQ_ITCH50, writes latencies.txt
python3 results/get_results.py
```

The ITCH capture is not in the repo. NASDAQ publishes full-day samples at
<https://emi.nasdaq.com/ITCH/Nasdaq%20ITCH/> as `MMDDYYYY.NASDAQ_ITCH50.gz`.
Unpack it to `itch_data.NASDAQ_ITCH50` in the repo root and run the binary from
there — the path is relative.

The UDP path links `libxdp` and `libbpf` on Linux (`libxdp-dev libbpf-dev` on
Ubuntu). On macOS the engine and tests build without them. CI builds with
AddressSanitizer and runs the tests on every push.

## How it works

The file is `mmap`ed and walked message by message. `A`/`F` adds are matched and
rested; `D`, `E`/`C`/`X`, and `U` mutate resting orders in place. Books are
per-symbol, indexed directly by the `stock_locate` field rather than by hashing
the ticker.

Three structures carry the design:

- **[`OrderPool`](include/order_pool.hpp)** — one flat arena of nodes shared by
  every book. Free slots are chained through their own `next` field, so
  allocation is two index writes and never touches the heap. Links are
  `uint32_t` indices, not pointers: smaller nodes, and they survive the arena
  growing.
- **[`LadderSide`](include/ladder_side.hpp)** — a fixed array of price levels
  indexed by `(price - base) / 100`, one slot per penny, with a cached best-slot
  pointer. Sub-penny and out-of-window prices fall back to a flat sorted vector.
  Replaces the `std::map` the book originally used.
- **[`FlatHashMap`](include/flat_hash_map.hpp)** — open-addressed,
  linear-probing map from order reference to `{stock_locate, pool index}`.
  Identity-hashed: ITCH refs are near-sequential, so consecutive orders land in
  adjacent slots and the live working set stays a narrow migrating band instead
  of scattering across the table. Deletion uses Knuth's Algorithm R to keep
  wrap-around probe chains intact.

Together these give O(1) cancel — no search for the symbol, the order, or its
position in the price-level queue.

Further tuning: transparent huge pages on the arena and hash table, a software
prefetch of the next message's hash slot, and core isolation.

The engine is memory-bound at this point — roughly 50% LLC miss rate and 22 LLC
misses per Add, against a working set well past the 6 MB L3.

## How it got here

Each row is a run I recorded in the repo at the time; the hash links to the
commit.

| Change | mean | p50 | p99 | p99.9 |
|--------|-----:|----:|----:|------:|
| First end-to-end version: `std::map` of `std::deque` per side, `std::unordered_map` for lookup, `ifstream` ([`4e2878d`](https://github.com/evanap003300/orderbook/commit/4e2878d)) | 5.7 µs | 1.2 µs | 33 µs | 474 µs |
| `mmap` the capture instead of stream I/O ([`1ac655f`](https://github.com/evanap003300/orderbook/commit/1ac655f)) | 2.4 µs | 875 ns | 16 µs | 132 µs |
| Cut the order struct down to the fields matching needs ([`08de822`](https://github.com/evanap003300/orderbook/commit/08de822)) | 1.4 µs | 709 ns | 9.5 µs | 41 µs |
| Flat hash map, order pool with intrusive lists, price ladder ([`8fa0ff2`](https://github.com/evanap003300/orderbook/commit/8fa0ff2)) | 84 ns | 42 ns | 709 ns | 2.0 µs |
| Same code, moved to the Linux box | 146 ns | | 818 ns | |
| Huge pages, prefetch, smaller identity-hashed table, isolated core ([`8d4ee3a`](https://github.com/evanap003300/orderbook/commit/8d4ee3a)) | 82 ns | 56 ns | 418 ns | 1.5 µs |
| Full ITCH message set: `F`, `E`, `C`, `X`, `U` ([`8cb7d00`](https://github.com/evanap003300/orderbook/commit/8cb7d00)) | 85.5 ns | 53 ns | 437 ns | 2.0 µs |

The first four rows were measured on my Mac, where the clock ticks in ~42 ns
steps (hence a p50 of exactly 42) and nothing is pinned or isolated. The first
three come from the original `std::chrono` harness, and the 132 µs run covered
about 10M orders rather than the whole day, so read those as order-of-magnitude
context, not a controlled comparison. The last three are the full day on the
Linux machine from the Results section.

Two things stand out. Shrinking the order struct and then replacing the
node-based containers took p99.9 from 132 µs to 2 µs on the same laptop, far
more than everything else combined. The Linux-side tuning was worth about
another 2× (p99 818 ns to 418 ns, wall time for the full file 102 s to 49 s).

## Benchmark setup

Core 3 is isolated at boot via `isolcpus=3 nohz_full=3 rcu_nocbs=3`. Before a run:

```sh
echo performance | sudo tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor
taskset -c 3 ./build/order_matching
```

## UDP feed handler

`--udp` runs the engine against a live MoldUDP64 feed instead of a file: a
receive thread hands packets to the matching thread over a
[lock-free SPSC ring](include/spsc_ring.hpp) carrying pre-allocated buffer
indices. `--afxdp --iface IFNAME` swaps `recvmsg` for an AF_XDP kernel-bypass
path. `src/replay.cpp` builds a sender that pushes the ITCH file out over UDP.

Wire-to-match latency over `recvmsg` at max replay rate: p50 1.1 µs, p90 2.2 µs,
p99 6.7 µs. That is the time from `recvmsg` returning on the receive thread to
the matching thread entering `handleOrder`. The AF_XDP path runs but has not
been benchmarked against it yet.

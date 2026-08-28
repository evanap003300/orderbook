# Matching Engine

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

Down from a p99.9 of 132 µs before the data structures were flattened.

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

## How it works

The file is `mmap`ed and walked message by message. `A`/`F` adds are matched and
rested; `D`, `E`/`C`/`X`, and `U` mutate resting orders in place. Books are
per-symbol, indexed directly by the `stock_locate` field rather than by hashing
the ticker.

Three structures carry the design:

- **`OrderPool`** — one flat arena of nodes shared by every book. Free slots are
  chained through their own `next` field, so allocation is two index writes and
  never touches the heap. Links are `uint32_t` indices, not pointers: smaller
  nodes, and they survive the arena growing.
- **`LadderSide`** — a fixed array of price levels indexed by
  `(price - base) / 100`, one slot per penny, with a cached best-slot pointer.
  Sub-penny and out-of-window prices fall back to a flat sorted vector. Replaces
  the `std::map` the book originally used.
- **`FlatHashMap`** — open-addressed, linear-probing map from order reference to
  `{stock_locate, pool index}`. Identity-hashed: ITCH refs are near-sequential,
  so consecutive orders land in adjacent slots and the live working set stays a
  narrow migrating band instead of scattering across the table. Deletion uses
  Knuth's Algorithm R to keep wrap-around probe chains intact.

Together these give O(1) cancel — no search for the symbol, the order, or its
position in the price-level queue.

Further tuning: transparent huge pages on the arena and hash table, a software
prefetch of the next message's hash slot, and core isolation.

The engine is memory-bound at this point — roughly 50% LLC miss rate and 22 LLC
misses per Add, against a working set well past the 6 MB L3.

## Benchmark setup

Core 3 is isolated at boot via `isolcpus=3 nohz_full=3 rcu_nocbs=3`. Before a run:

```sh
echo performance | sudo tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor
taskset -c 3 ./build/order_matching
```

## UDP feed handler

`--udp` runs the engine against a live MoldUDP64 feed instead of a file: a
receive thread hands packets to the matching thread over a lock-free SPSC ring
carrying pre-allocated buffer indices. `--afxdp --iface IFNAME` swaps `recvmsg`
for an AF_XDP kernel-bypass path. `src/replay.cpp` builds a sender that pushes
the ITCH file out over UDP.

Wire-to-match latency over `recvmsg` at max replay rate: p50 1.1 µs, p90 2.2 µs,
p99 6.7 µs.

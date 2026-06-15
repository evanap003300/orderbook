# Context for AI assistants working on this repo

A C++17 NASDAQ ITCH 5.0 matching engine focused on low-latency order processing. The owner is Evan, iterating on it for performance and as a resume-relevant project. He wants to control design decisions and understand the tradeoffs — explain non-obvious choices, surface forks, don't auto-decide big things.

## Build & run

```sh
cmake -S . -B build              # only needed first time / after CMakeLists changes
cmake --build build
./build/run_tests                # 30 tests, completes in ms
./build/order_matching           # processes itch_data.NASDAQ_ITCH50, writes latencies.txt
```

The ITCH file is 9.5 GB and not in git. The engine `mmap`s it; full run takes ~3 minutes and pushes ~136M Add messages through. Don't run it without asking — it's not instant.

**Getting the ITCH file** (the engine looks for `itch_data.NASDAQ_ITCH50` at the repo root, relative path, so run the binary from the repo root not from `build/`):

```sh
# NASDAQ publishes sample full-day ITCH 5.0 captures at https://emi.nasdaq.com/ITCH/Nasdaq%20ITCH/
# Filenames are MMDDYYYY.NASDAQ_ITCH50.gz, ~3.5-5 GB gzipped, ~9-10 GB raw.
curl -O https://emi.nasdaq.com/ITCH/Nasdaq%20ITCH/01302019.NASDAQ_ITCH50.gz
curl -O https://emi.nasdaq.com/ITCH/Nasdaq%20ITCH/01302019.NASDAQ_ITCH50.md5sum
md5sum -c 01302019.NASDAQ_ITCH50.md5sum
gunzip 01302019.NASDAQ_ITCH50.gz
mv 01302019.NASDAQ_ITCH50 itch_data.NASDAQ_ITCH50
```

Clangd in-editor often shows false "file not found" / undeclared-identifier errors because it doesn't know the `-Iinclude` from CMakeLists. **Trust the CMake build, not clangd.**

## Architecture

```
include/
  flat_hash_map.hpp   Linear-probing open-addressed hash map. Robin-hood erase.
  order_pool.hpp      Flat PoolNode arena + index-based free-list. Engine-wide.
  ladder_side.hpp     One side (bid/ask) as a price ladder + overflow map + Level.
  orderbook.hpp       Per-symbol OrderBook: pool ptr + bids LadderSide + asks LadderSide.
  matching_engine.hpp Engine: owns pool, orderMap, 65536 OrderBooks.
src/
  itch.cpp            Parses ITCH Add / Delete wire format.
  orderbook.cpp       handleBuyOrder / handleSellOrder / removeByIndex.
  matching_engine.cpp Engine loop: reads ITCH messages, dispatches A/D.
  main.cpp            Entry point: runs engine, writes latencies.txt.
tests/test.cpp        FlatHashMap tests + OrderBook tests via a TestBook fixture.
```

The pieces:

- **`OrderPool`** is one flat `vector<PoolNode>` shared engine-wide. `PoolNode { Order order; uint32 next, prev; }`. `allocate()` pops a free slot or pushes back; `free()` links to free head. Indices are stable; the pool is never resized in a way that invalidates them (just `push_back` past current size).
- **`LadderSide`** has a `ladder` (fixed-size `vector<Level>`, indexed by `(price - base) / TICK`, lazily allocated, anchored centered on first price), an `overflow` `std::map<price, Level>` for misaligned/out-of-window prices, and `bestSlot` tracking. `TICK = 100` (NMS penny granularity in ITCH 1/10000-dollar units), `WINDOW = 2048` (~$20.48 range). `bestLevel()` picks the better of the ladder vs the overflow's edge.
- **`OrderBook`** holds an `OrderPool*` and two `LadderSide`s (bid + ask). API:
  - `handleOrder(Order&, restingIdx&, removedRefs&, executedOrders&)` — all results via reused out-param buffers. Returns void. Caller `clear()`s the buffers before calling.
  - `removeByIndex(uint32)` — O(1) delete given the pool index.
- **`MatchingEngine`** owns the `OrderPool`, a `FlatHashMap<uint64, OrderLocation>` where `OrderLocation { uint16 stockLocate; uint32 poolIdx }`, and a `vector<OrderBook>` of 65536 books (one per `stock_locate`).

`Level { uint32 head, tail }` = intrusive doubly-linked list endpoints into the shared pool; FIFO time priority.

## Design decisions worth knowing

These are non-obvious and worth understanding before changing anything:

1. **Engine-wide pool, not per-book.** Better cache locality across symbols, single allocation strategy. The cost: `OrderBook` is no longer self-contained — it holds a raw `OrderPool*`. Tests construct an `OrderPool` and pass it.
2. **Engine resolves deletes (Variant B).** `orderMap` value type is `{stockLocate, poolIdx}`. On delete the engine looks up, then calls `orderBooks[loc].removeByIndex(idx)`. No per-book `orderRef → idx` map. `handleDeleteOrder(DeleteOrder&)` does not exist; you delete by pool index.
3. **Fully-filled resting orders are reported via `removedRefs`.** Matching frees the pool slot — but the engine's `orderMap` still references it until told. The engine `clear()`s and `erase()`s `removedRefs` from `orderMap` **before** inserting the new resting ref (a freed slot can be reused in the same message; erase-before-insert keeps the map consistent).
4. **`PoolNode` wraps `Order` rather than adding next/prev to it.** Keeps the ITCH wire-parsing struct (`Order`) clean and unaware of storage linkage.
5. **Ladder TICK=100, WINDOW=2048.** NMS Rule 612 mandates penny ticks for ≥$1 stocks (most volume), so most prices land in the ladder fast path. Sub-penny / out-of-window prices go to the overflow map — correctness preserved without paying for it in the hot path.
6. **All matching output goes through reused out-param buffers** (executed records, removed refs, resting idx). The engine `clear()`s them per call; the allocator gets hit at startup, not per message.

## Current performance

Measured over 136.5M Add messages on a full NASDAQ ITCH 5.0 trading day, macOS (post Phase 4):

| Stat | Value |
|------|------:|
| mean | 84.1 ns |
| p50 | 42 ns (near `high_resolution_clock` granularity floor) |
| p90 | 167 ns |
| p99 | 709 ns |
| p99.9 | 2,042 ns |
| p99.99 | 9,750 ns |
| max | ~20 ms (run-to-run noise; OS stall) |
| wall time | ~3 min for the whole 9.5 GB file (~750K msg/s on Add path) |

For context, the pre-flattening README baseline was p99.9 = 132,083 ns over ~10M samples — so the structural changes (FlatHashMap, intrusive list + flat pool, price ladder, reused out-param buffers) bought ~65× at p99.9.

## What's next on the Linux machine

The work is moving to Linux specifically for the tooling. Planned order:

1. **Re-benchmark on Linux.** Establish a clean baseline before profiling. Same workload (the 9.5 GB ITCH file).
2. **Profile with `perf`.** Sampling profile (`perf record -g --call-graph dwarf ./build/order_matching`) and flame graph. Look for: hot branches in `LadderSide::bestLevel`, `FlatHashMap` probe sequences, the timer overhead under `high_resolution_clock` (the p50 of 42 ns is suspiciously close to the macOS chrono floor; on Linux it may reveal more), branch-mispredict / cache-miss stats via `perf stat -e ...`.
3. **Huge pages.** The big eager allocations (orderMap ~512 MB committed, pool reserved at ~1.8 GB virtual) benefit from THP. Try `madvise(addr, len, MADV_HUGEPAGE)` on the underlying buffers or run with transparent hugepage always on.
4. **Core isolation + pinning.** Boot kernel with `isolcpus=<core>` (or use `cset shield`), pin the engine to that core with `taskset -c <core>`, move IRQs off it. Quieter run, tighter tail.
5. **Further structural opts** the profile reveals. Candidates: packing `PoolNode` from 28 B to 32 B for better cache-line behavior, batching `chrono` reads (per-N-messages), removing branches in the bestLevel hot path.
6. **(Longer-term) live UDP feed.** Read ITCH 5.0 over multicast UDP instead of mmap'd file. Different shape: SPSC ring buffer, kernel bypass (DPDK / AF_XDP), no allocations in the receive path.

## Working style

- **Explain tradeoffs before deciding.** Especially on API shape, allocation strategy, per-vs-engine-wide, what to put in tests. He wants to be the one who picks. Surface options, recommend, ask.
- **Don't auto-commit.** He'll say when.
- **Don't run the full engine without asking** — it churns ~3 minutes and the disk has been tight (was at 11 GB free when we last cleaned caches).
- **Re-run `./build/run_tests` after non-trivial changes.** Fast and cheap; catches the obvious things.
- **Be concise.** Short status updates, results tables, no narration of internal deliberation.
- **Memory file** (`~/.claude/projects/.../memory/orderbook-flattening-plan.md`) on the macOS machine has the longer phased plan. Won't transfer — this CLAUDE.md is the durable handoff.

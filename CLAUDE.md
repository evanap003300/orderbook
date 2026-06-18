# Context for AI assistants working on this repo

A C++17 NASDAQ ITCH 5.0 matching engine focused on low-latency order processing. The owner is Evan, iterating on it for performance and as a resume-relevant project. He wants to control design decisions and understand the tradeoffs — explain non-obvious choices, surface forks, don't auto-decide big things.

## Build & run

```sh
cmake -S . -B build              # only needed first time / after CMakeLists changes
cmake --build build
./build/run_tests                # 35 tests, completes in ms
./build/order_matching           # processes itch_data.NASDAQ_ITCH50, writes latencies.txt (~50s)
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
- **`LadderSide`** has a `ladder` (fixed-size `vector<Level>`, indexed by `(price - base) / TICK`, lazily allocated, anchored centered on first price), an `overflow` flat sorted `vector<pair<price, Level>>` for out-of-window prices, and `bestSlot` tracking. `TICK = 100` (NMS penny granularity in ITCH 1/10000-dollar units), `WINDOW = 2048` (~$20.48 range). `bestLevel()` picks the better of the ladder vs the overflow's edge. Overflow was formerly `std::map` but handled 22% of resting orders (mostly window-misses, not sub-penny), so it was replaced with a flat vector.
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
5. **Ladder TICK=100, WINDOW=2048.** NMS Rule 612 mandates penny ticks for ≥$1 stocks (most volume), so most prices land in the ladder fast path. Sub-penny / out-of-window prices go to the overflow vector — correctness preserved without paying for it in the hot path.
6. **All matching output goes through reused out-param buffers** (executed records, removed refs, resting idx). The engine `clear()`s them per call; the allocator gets hit at startup, not per message.
7. **`FlatHashMap` uses identity hash (`key & mask`) for `orderMap`.** ITCH order reference numbers are globally near-sequential (min=92, max=264M over a day). Identity hash places consecutive refs in adjacent slots → live working set is a ~33 MB migrating band rather than scattered across 128 MB → much better cache locality. `bool Identity` is a compile-time template parameter.
8. **`FlatHashMap::erase` uses Knuth's Algorithm R.** The naive Robin-Hood backward-shift erase stops when it hits an element at its home position, leaving holes that break wrap-around probe chains. With identity hash + sequential refs wrapping every ~8.4M orders, this caused lost keys → table filled with ghosts → infinite loop. Algorithm R continues scanning past unmovable elements. Regression test: `IdentityHashEraseWrapAroundChain`.
9. **orderMap is 8.4M slots (128 MB), not 33.5M (512 MB).** Peak live orders measured at 2.1M (~25% load). Smaller table means most of the working set fits in fewer cache lines. Direct-indexed array ruled out: ref span 264M vs 2.1M live → 2 GB mostly-dead array.

## Current performance

Measured over 163M Add messages on a full NASDAQ ITCH 5.0 trading day (01/30/2019), Linux i5-8350U, isolated core 3, performance governor @ 3.6 GHz, TSC timing:

| Stat | Value |
|------|------:|
| mean | 82.4 ns |
| p50 | 56 ns |
| p90 | 152 ns |
| p99 | 418 ns |
| p99.9 | 1,492 ns |
| p99.99 | ~5,900 ns |
| wall time | 49.4 s for the full 10.2 GB file |

Pre-optimization Linux baseline (same machine, same file, before this work): mean 145.6 ns, p99 818 ns, wall 102.2 s. **Net gain: ~2× across the distribution body.**

For historical context, the pre-flattening macOS baseline was p99.9 = 132,083 ns — the structural changes (FlatHashMap, intrusive list + flat pool, price ladder, reused out-param buffers) bought ~65× at p99.9 before these Linux-phase optimizations.

Remaining ceiling: engine is still memory-bound (50% LLC miss rate, ~22 LLC misses per Add). Working set (2.1M live orders × 32 B pool + 128 MB orderMap) exceeds the 6 MB L3. Further gains need smaller data or a different architecture.

## Benchmark setup (Linux)

Core 3 is permanently isolated (`isolcpus=3 nohz_full=3 rcu_nocbs=3` in grub). Before each benchmark run:

```sh
echo performance | sudo tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor
taskset -c 3 ./build/order_matching
```

To undo isolation: remove the three flags from `/etc/default/grub`, run `sudo grub-mkconfig -o /boot/grub/grub.cfg`, reboot.

## What's next

1. **(Next) Live UDP feed.** Read ITCH 5.0 over multicast UDP instead of mmap'd file. Completely different shape: SPSC ring buffer from kernel, kernel bypass (DPDK / AF_XDP), no allocations in the receive path. Will naturally change the memory access pattern and may shift the bottleneck.
2. **1-in-N timing sampling.** The two `rdtscp` calls per Add still cost ~20-30 ns. Sampling every 16th message gets overhead near zero with 10M+ samples still collected. Minor cosmetic improvement to the measured mean.
3. **Re-profile after UDP.** The bottleneck will shift; don't optimize blind.

## Working style

- **Explain tradeoffs before deciding.** Especially on API shape, allocation strategy, per-vs-engine-wide, what to put in tests. He wants to be the one who picks. Surface options, recommend, ask.
- **Don't auto-commit.** He'll say when.
- **Don't run the full engine without asking** — it takes ~50 seconds but the ITCH file is 10.2 GB and disk space matters.
- **Re-run `./build/run_tests` after non-trivial changes.** 35 tests, completes in ms.
- **Be concise.** Short status updates, results tables, no narration of internal deliberation.
- **Memory files** at `~/.claude/projects/.../memory/` have full profiling history and machine gotchas.

# Context for AI assistants working on this repo

A C++17 NASDAQ ITCH 5.0 matching engine focused on low-latency order processing. The owner is Evan, iterating on it for performance and as a resume-relevant project. He wants to control design decisions and understand the tradeoffs — explain non-obvious choices, surface forks, don't auto-decide big things.

## Build & run

```sh
cmake -S . -B build              # only needed first time / after CMakeLists changes
cmake --build build
./build/run_tests                # 46 tests, completes in ms
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
  flat_hash_map.hpp     Linear-probing open-addressed hash map. Robin-hood erase.
  order_pool.hpp        Flat PoolNode arena + index-based free-list. Engine-wide.
  ladder_side.hpp       One side (bid/ask) as a price ladder + overflow map + Level.
  orderbook.hpp         Per-symbol OrderBook: pool ptr + bids LadderSide + asks LadderSide.
  matching_engine.hpp   Engine: owns pool, orderMap, 65536 OrderBooks.
  spsc_ring.hpp         Lock-free SPSC ring (cache-line padded, PAUSE busy-spin).
  packet_pool.hpp       Free-list arena of fixed-size UDP packet buffers.
  moldudp64.hpp         MoldUDP64 packet header parser/builder + message iterator.
  feed_handler.hpp      UDP receive thread API + FeedStats + PacketRef.
  feed_handler_xdp.hpp  AF_XDP receive path declaration.
  tsc.hpp               Portable monotonic cycle counter (rdtscp on x86, MONOTONIC ns elsewhere).
src/
  itch.cpp              Parses ITCH Add / Delete wire format.
  orderbook.cpp         handleBuyOrder / handleSellOrder / removeByIndex.
  matching_engine.cpp   Engine loop: file mode + processMessage().
  engine_udp.cpp        UDP mode: spawns feed handler, consumes the ring.
  feed_handler.cpp      recvmsg loop + MoldUDP64 framing + sequence-gap detect + SPSC push.
  feed_handler_xdp.cpp  AF_XDP UMEM+socket setup, busy-poll RX ring, Eth/IP/UDP parse.
  replay.cpp            Standalone binary: ITCH file -> MoldUDP64 packets over UDP.
  main.cpp              Entry point: --file (default) | --udp | --udp --afxdp --iface IFNAME.
tests/test.cpp          FlatHashMap + OrderBook + SpscRing + PacketPool + MoldUDP64.
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

## Phase 5: UDP feed handler + AF_XDP bypass

Phase 5 added a real UDP feed-handler pipeline that decouples networking from matching. Two-thread architecture, SPSC ring, MoldUDP64 framing, sequence-gap detection. **Benchmarked on Linux (isolated cores 2+3); AF_XDP path also implemented.**

### recvmsg pipeline (--udp)

```
replay binary  ──UDP/MoldUDP64──►  feed handler thread (recvmsg → mold parse → seq check
                                    → memcpy into pool slot → push slot idx on SPSC ring)
                                                  │
                                                  ▼ ring of PacketRef {poolIdx, tWireCycles}
                                       matching thread (spin-pop ring → MoldMessageIterator
                                        → processMessage → return slot to pool)
```

### AF_XDP pipeline (--udp --afxdp --iface IFNAME)

```
replay binary  ──UDP/MoldUDP64──►  XDP hook on NIC/loopback
                                       │ redirects to AF_XDP UMEM ring (no syscall)
                                       ▼
                               feed handler thread (busy-polls RX ring → parse Eth/IP/UDP
                                → seq check → memcpy MoldUDP64 into PacketPool → push ring)
                                                  │
                                                  ▼ same SPSC ring as recvmsg path
                                       matching thread (unchanged)
```

AF_XDP eliminates the `recvmsg` syscall from the receive hot path. The kernel XDP hook redirects packets directly into the UMEM shared-memory region; user space busy-polls the RX ring with no syscall needed.

Design decisions worth knowing:

- **SPSC ring carries pre-allocated buffer indices, not pointers.** PacketPool owns a free-list of fixed 2 KiB slots; producer acquires, fills, pushes the index; consumer pops, walks, releases. Zero allocation on either hot path.
- **Cache-line padding** on the ring's head/tail (`alignas(kCacheLineSize)`); without this the two cores ping-pong the same cache line on every push/pop.
- **Busy-spin consumer with CPU PAUSE** — the matching thread spins on an empty ring with `pause` (x86) or `yield` (aarch64). Only safe because the matching thread is meant to live on an isolated core.
- **Backpressure: drop on full.** Producer never blocks. Sequence-gap detection on the next packet exposes the dropped messages.
- **MoldUDP64 multi-message packets.** The replay binary packs up to N ITCH messages per UDP packet (configurable via `--max-per-packet`), each prefixed by a 2-byte length, all under a 20-byte session header. End-of-session is `messageCount == 0xFFFF`.
- **Wire-to-match latency** is measured from "TSC just after recvmsg returns / XDP batch received" to "TSC just before handleOrder enters". Written to `wire_latencies.txt`.
- **AF_XDP uses SKB/generic mode + copy mode.** `XDP_FLAGS_SKB_MODE` works on any interface including `lo`; `XDP_COPY` works without zero-copy hardware support. libxdp auto-loads the built-in XDP redirect program (requires root or CAP_NET_ADMIN + CAP_BPF).
- **UMEM: 4096 frames × 4096 B = 16 MB.** Pre-populated into the fill ring at startup. After each batch: consumed RX frame addresses are returned to the fill ring so the kernel can reuse them.
- **AF_XDP receives raw Ethernet frames** (Eth/IP/UDP headers + payload). The path parses L2→L3→L4, filters by UDP dest port, then copies only the MoldUDP64 payload into a PacketPool slot — same downstream path as recvmsg.

Build & run:

```sh
# build
cmake --build build         # produces order_matching, replay, run_tests; links libxdp + libbpf

# unit tests (46 of them)
./build/run_tests

# recvmsg path on loopback (one terminal each)
./build/order_matching --udp --bind 127.0.0.1 --port 30001
./build/replay --file itch_data.NASDAQ_ITCH50 --target 127.0.0.1 --port 30001 \
               --max-per-packet 16

# AF_XDP path on loopback (requires root or cap_net_admin,cap_bpf)
sudo ./build/order_matching --udp --afxdp --iface lo --port 30001
./build/replay --file itch_data.NASDAQ_ITCH50 --target 127.0.0.1 --port 30001 \
               --max-per-packet 16
```

For multicast: `--multicast 239.1.1.1` on the engine (recvmsg path only; AF_XDP receives all interface traffic and filters by port).

Linux wire-to-match numbers (recvmsg, isolated cores 2+3, rate=2M msg/s):
- p50: ~1.3 µs, p90: ~2.9 µs, p99: ~6.2 µs
- (AF_XDP numbers: pending benchmark — expect p99 reduction of 3–10×)

## What's next (Linux-side)

1. **Benchmark AF_XDP vs recvmsg.** Run both paths side-by-side on the tuned Linux box, same rate, compare `wire_latencies.txt` distributions. Expected: similar or better p50, meaningfully lower tail (p99+) from eliminating the recvmsg syscall.
2. **1-in-N timing sampling.** The two `tscNow()` calls per Add cost ~20-30 ns. Sample every 16th message; minor cosmetic mean improvement.
3. **Multicast group join testing.** Verify IGMP join + multi-receiver fan-out works on Linux.
4. **Re-profile after AF_XDP.** Bottleneck likely shifts back into matching.

## Working style

- **Explain tradeoffs before deciding.** Especially on API shape, allocation strategy, per-vs-engine-wide, what to put in tests. He wants to be the one who picks. Surface options, recommend, ask.
- **Don't auto-commit.** He'll say when.
- **Don't run the full engine without asking** — it takes ~50 seconds but the ITCH file is 10.2 GB and disk space matters.
- **Re-run `./build/run_tests` after non-trivial changes.** 46 tests, completes in ms.
- **Be concise.** Short status updates, results tables, no narration of internal deliberation.
- **Memory files** at `~/.claude/projects/.../memory/` have full profiling history and machine gotchas.

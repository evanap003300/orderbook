#pragma once
#include <atomic>
#include <cstdint>

#include "feed_handler.hpp"
#include "packet_pool.hpp"
#include "spsc_ring.hpp"

// AF_XDP receive path: bypasses the kernel UDP socket layer via XDP sockets.
// libxdp auto-loads a built-in XDP redirect program onto `iface` (requires
// CAP_NET_ADMIN + CAP_BPF, i.e. run as root or with the right capabilities).
//
// Receive path: NIC/kernel XDP hook → UMEM ring → user-space busy-poll → parse
// Eth/IP/UDP → filter by `port` → memcpy MoldUDP64 payload into PacketPool
// slot → push ref to SPSC ring.  No recvmsg syscall on the hot path.
//
// `iface`: network interface name (e.g. "lo", "eth0").
// `port`: UDP destination port to accept; other packets are skipped.
// Other params identical to runFeedHandler().
void runFeedHandlerXdp(const char* iface, uint16_t port,
                        PacketPool& pool, SpscRing<PacketRef>& ring,
                        FeedStats& stats, std::atomic<bool>& stop);

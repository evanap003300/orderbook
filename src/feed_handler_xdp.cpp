// AF_XDP receive path. Replaces the recvmsg loop in feed_handler.cpp with a
// shared-memory UMEM ring that the kernel writes packets into directly, so
// the matching thread's hot path has zero syscalls on the receive side.
//
// Requires root or CAP_NET_ADMIN + CAP_BPF: libxdp loads an XDP redirect
// program onto the interface during xsk_socket__create().

#include "feed_handler_xdp.hpp"

#include <arpa/inet.h>
#include <errno.h>
#include <linux/if_ether.h>
#include <linux/if_link.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <xdp/xsk.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>

#include "moldudp64.hpp"
#include "packet_pool.hpp"
#include "spsc_ring.hpp"
#include "tsc.hpp"

static constexpr uint32_t kNumFrames  = 4096;
static constexpr uint32_t kFrameSize  = XSK_UMEM__DEFAULT_FRAME_SIZE;  // 4096 B
static constexpr uint32_t kBatchSize  = 64;

// Byte offsets from start of raw Ethernet frame to each header layer
static constexpr size_t kEthHdrSize = ETH_HLEN;    // 14
static constexpr size_t kIpHdrMin   = 20;
static constexpr size_t kUdpHdrSize = 8;

void runFeedHandlerXdp(const char* iface, uint16_t port,
                        PacketPool& pktPool, SpscRing<PacketRef>& ring,
                        FeedStats& stats, std::atomic<bool>& stop) {
  size_t umemSize = (size_t)kNumFrames * kFrameSize;

  // Page-aligned UMEM region shared between user space and the kernel
  void* umemArea = nullptr;
  if (posix_memalign(&umemArea, getpagesize(), umemSize) != 0) {
    fprintf(stderr, "xdp: posix_memalign(%zu): %s\n", umemSize, strerror(errno));
    return;
  }
  memset(umemArea, 0, umemSize);
  madvise(umemArea, umemSize, MADV_HUGEPAGE);

  // Register UMEM with the kernel
  struct xsk_umem*     umem = nullptr;
  struct xsk_ring_prod fill{};
  struct xsk_ring_cons comp{};
  {
    struct xsk_umem_config ucfg{};
    ucfg.fill_size      = kNumFrames;
    ucfg.comp_size      = kNumFrames;
    ucfg.frame_size     = kFrameSize;
    ucfg.frame_headroom = 0;
    int ret = xsk_umem__create(&umem, umemArea, umemSize, &fill, &comp, &ucfg);
    if (ret != 0) {
      fprintf(stderr, "xdp: xsk_umem__create: %s\n", strerror(-ret));
      free(umemArea);
      return;
    }
  }

  // Hand all frames to the kernel upfront (fill ring = "kernel can write here")
  {
    uint32_t idx;
    uint32_t reserved = xsk_ring_prod__reserve(&fill, kNumFrames, &idx);
    if (reserved != kNumFrames) {
      fprintf(stderr, "xdp: fill ring reserve: got %u of %u\n", reserved, kNumFrames);
      xsk_umem__delete(umem);
      free(umemArea);
      return;
    }
    for (uint32_t i = 0; i < kNumFrames; i++)
      *xsk_ring_prod__fill_addr(&fill, idx + i) = (uint64_t)i * kFrameSize;
    xsk_ring_prod__submit(&fill, kNumFrames);
  }

  // Create AF_XDP socket. libxdp auto-loads a redirect XDP program onto the
  // interface (requires CAP_NET_ADMIN / root). SKB mode works on any interface
  // including loopback; XDP_COPY works without zero-copy hardware support.
  struct xsk_socket*   xsk = nullptr;
  struct xsk_ring_cons rx{};
  struct xsk_ring_prod tx{};
  {
    struct xsk_socket_config scfg{};
    scfg.rx_size      = kNumFrames;
    scfg.tx_size      = 64;             // TX ring exists but we never transmit
    scfg.libxdp_flags = 0;             // auto-load built-in XDP redirect program
    scfg.xdp_flags    = XDP_FLAGS_SKB_MODE;  // generic mode: works on lo + any NIC
    scfg.bind_flags   = XDP_COPY;      // copy mode: no zero-copy HW required
    int ret = xsk_socket__create(&xsk, iface, 0 /*queue*/, umem, &rx, &tx, &scfg);
    if (ret != 0) {
      fprintf(stderr, "xdp: xsk_socket__create(%s): %s\n"
              "  (AF_XDP requires root or CAP_NET_ADMIN + CAP_BPF)\n",
              iface, strerror(-ret));
      xsk_umem__delete(umem);
      free(umemArea);
      return;
    }
  }

  fprintf(stderr, "xdp: AF_XDP socket ready on %s queue 0 (SKB/copy), port %u\n",
          iface, port);

  // Per-session sequence-number tracker (same logic as recvmsg path)
  char     activeSession[10] = {};
  bool     sessionInit       = false;
  uint64_t nextExpected      = 1;

  // Saved frame addresses for a single batch; returned to fill ring after processing
  uint64_t batchAddrs[kBatchSize];

  bool done = false;
  while (!done && !stop.load(std::memory_order_relaxed)) {
    uint32_t rxIdx;
    uint32_t rcvd = xsk_ring_cons__peek(&rx, kBatchSize, &rxIdx);
    if (rcvd == 0) {
      cpu_pause();
      continue;
    }

    uint64_t tWire = tscNow();  // stamp once per batch

    for (uint32_t i = 0; i < rcvd; i++) {
      const struct xdp_desc* desc = xsk_ring_cons__rx_desc(&rx, rxIdx + i);
      batchAddrs[i] = desc->addr;  // save for fill-ring refill below

      const uint8_t* frame    = static_cast<const uint8_t*>(
          xsk_umem__get_data(umemArea, desc->addr));
      uint32_t       frameLen = desc->len;

      // ── L2: Ethernet ─────────────────────────────────────────────────────
      if (frameLen < kEthHdrSize + kIpHdrMin + kUdpHdrSize + kMoldHeaderBytes)
        continue;
      const struct ethhdr* eth = reinterpret_cast<const struct ethhdr*>(frame);
      if (ntohs(eth->h_proto) != ETH_P_IP) continue;

      // ── L3: IPv4 ─────────────────────────────────────────────────────────
      const struct iphdr* ip =
          reinterpret_cast<const struct iphdr*>(frame + kEthHdrSize);
      if (ip->version != 4) continue;
      size_t ipLen = (size_t)ip->ihl * 4;
      if (ipLen < kIpHdrMin) continue;
      if (ip->protocol != IPPROTO_UDP) continue;

      // ── L4: UDP ──────────────────────────────────────────────────────────
      size_t udpOffset = kEthHdrSize + ipLen;
      if (frameLen < udpOffset + kUdpHdrSize + kMoldHeaderBytes) continue;
      const struct udphdr* udp =
          reinterpret_cast<const struct udphdr*>(frame + udpOffset);
      if (ntohs(udp->dest) != port) continue;

      // ── MoldUDP64 ────────────────────────────────────────────────────────
      const uint8_t* moldPkt = frame + udpOffset + kUdpHdrSize;
      size_t         moldLen = frameLen - udpOffset - kUdpHdrSize;
      if (moldLen < kMoldHeaderBytes) continue;

      stats.packetsReceived.fetch_add(1, std::memory_order_relaxed);
      stats.bytesReceived.fetch_add(moldLen, std::memory_order_relaxed);

      MoldHeader hdr;
      parseMoldHeader(moldPkt, hdr);

      if (!sessionInit) {
        memcpy(activeSession, hdr.session, 10);
        sessionInit   = true;
        nextExpected  = hdr.sequenceNumber;
      }

      if (hdr.messageCount == kMoldEndOfSession) {
        stats.endOfSession.fetch_add(1, std::memory_order_relaxed);
        PacketRef sentinel{kPoolInvalid, tWire};
        while (!ring.push(sentinel) && !stop.load(std::memory_order_relaxed))
          cpu_pause();
        done = true;
        break;
      }
      if (hdr.messageCount == kMoldHeartbeat) continue;

      if (hdr.sequenceNumber < nextExpected) {
        stats.duplicatesIgnored.fetch_add(1, std::memory_order_relaxed);
        continue;
      }
      if (hdr.sequenceNumber > nextExpected) {
        stats.gapsDetected.fetch_add(1, std::memory_order_relaxed);
        nextExpected = hdr.sequenceNumber;
      }
      nextExpected += hdr.messageCount;

      // Copy MoldUDP64 payload into a PacketPool slot and push the ref
      uint32_t slotIdx = pktPool.acquire();
      if (slotIdx == kPoolInvalid) {
        stats.packetsDropped.fetch_add(1, std::memory_order_relaxed);
        continue;
      }
      BufferSlot& slot   = pktPool[slotIdx];
      auto        cpyLen = static_cast<uint16_t>(
          std::min(moldLen, static_cast<size_t>(kMaxPacketBytes)));
      memcpy(slot.data, moldPkt, cpyLen);
      slot.length = cpyLen;

      PacketRef ref{slotIdx, tWire};
      if (!ring.push(ref)) {
        pktPool.release(slotIdx);
        stats.packetsDropped.fetch_add(1, std::memory_order_relaxed);
      }
    }

    // Release consumed slots from the RX ring
    xsk_ring_cons__release(&rx, rcvd);

    // Return the frame addresses to the fill ring so the kernel can reuse them
    uint32_t fillIdx;
    uint32_t got = xsk_ring_prod__reserve(&fill, rcvd, &fillIdx);
    for (uint32_t i = 0; i < got; i++)
      *xsk_ring_prod__fill_addr(&fill, fillIdx + i) = batchAddrs[i];
    if (got > 0) xsk_ring_prod__submit(&fill, got);
    // If got < rcvd the fill ring is momentarily starved; the kernel will stall
    // gracefully until more frames arrive on the next batch.
  }

  xsk_socket__delete(xsk);
  xsk_umem__delete(umem);
  free(umemArea);
}

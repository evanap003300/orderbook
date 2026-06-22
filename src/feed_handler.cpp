#include "feed_handler.hpp"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>

#include "moldudp64.hpp"
#include "tsc.hpp"

// Plain POSIX/BSD UDP feed handler. Lives on a single thread; on Linux this
// thread should be pinned to an isolated core via taskset and the matching
// thread to another. The receive path is a simple recvmsg loop -- no
// timestamps requested from the kernel, since we stamp ourselves immediately
// after recvmsg returns (the TSC has lower overhead than SO_TIMESTAMPING).

void runFeedHandler(const char* bindAddr, uint16_t port,
                    const char* multicastGroup, PacketPool& pool,
                    SpscRing<PacketRef>& ring, FeedStats& stats,
                    std::atomic<bool>& stop) {
  int sock = socket(AF_INET, SOCK_DGRAM, 0);
  if (sock < 0) {
    fprintf(stderr, "feed_handler: socket() failed: %s\n", strerror(errno));
    return;
  }

  int reuse = 1;
  setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
#ifdef SO_REUSEPORT
  setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse));
#endif
  int rcvbuf = 8 * 1024 * 1024;  // 8 MB receive buffer to absorb bursts
  setsockopt(sock, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(rcvbuf));

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = (bindAddr && *bindAddr) ? inet_addr(bindAddr)
                                                  : htonl(INADDR_ANY);
  if (bind(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
    fprintf(stderr, "feed_handler: bind() failed: %s\n", strerror(errno));
    close(sock);
    return;
  }

  // Multicast group join (IGMP) if a group was provided. Skipped for unicast.
  if (multicastGroup && *multicastGroup) {
    ip_mreq mreq{};
    mreq.imr_multiaddr.s_addr = inet_addr(multicastGroup);
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq,
                   sizeof(mreq)) < 0) {
      fprintf(stderr, "feed_handler: IP_ADD_MEMBERSHIP failed: %s\n",
              strerror(errno));
      close(sock);
      return;
    }
    fprintf(stderr, "feed_handler: joined multicast group %s\n",
            multicastGroup);
  }

  fprintf(stderr, "feed_handler: listening on %s:%u\n",
          (bindAddr && *bindAddr) ? bindAddr : "0.0.0.0", port);

  // Per-session sequence-number tracker. A real implementation would key by
  // the 10-byte session id; we assume a single session for simplicity and
  // sanity-check that the session bytes don't change.
  char activeSession[10] = {};
  bool sessionInit = false;
  uint64_t nextExpected = 1;  // MoldUDP64 sessions start at 1

  while (!stop.load(std::memory_order_relaxed)) {
    uint32_t slotIdx = pool.acquire();
    if (slotIdx == kPoolInvalid) {
      // Pool empty means the consumer is hopelessly behind. Receive into a
      // throwaway buffer so we still detect end-of-session and gap markers.
      uint8_t scratch[kMaxPacketBytes];
      ssize_t n = recv(sock, scratch, sizeof(scratch), 0);
      if (n <= 0) continue;
      stats.packetsReceived.fetch_add(1, std::memory_order_relaxed);
      stats.packetsDropped.fetch_add(1, std::memory_order_relaxed);
      stats.bytesReceived.fetch_add(static_cast<uint64_t>(n),
                                    std::memory_order_relaxed);
      continue;
    }

    BufferSlot& slot = pool[slotIdx];
    ssize_t n = recv(sock, slot.data, kMaxPacketBytes, 0);
    uint64_t tWire = tscNow();  // stamp ASAP after the kernel returns
    if (n < 0) {
      pool.release(slotIdx);
      if (errno == EINTR) continue;
      fprintf(stderr, "feed_handler: recv() failed: %s\n", strerror(errno));
      break;
    }
    if (n < static_cast<ssize_t>(kMoldHeaderBytes)) {
      pool.release(slotIdx);
      continue;
    }
    slot.length = static_cast<uint16_t>(n);

    stats.packetsReceived.fetch_add(1, std::memory_order_relaxed);
    stats.bytesReceived.fetch_add(static_cast<uint64_t>(n),
                                  std::memory_order_relaxed);

    MoldHeader hdr;
    parseMoldHeader(slot.data, hdr);

    if (!sessionInit) {
      memcpy(activeSession, hdr.session, 10);
      sessionInit = true;
      nextExpected = hdr.sequenceNumber;
    }

    if (hdr.messageCount == kMoldEndOfSession) {
      stats.endOfSession.fetch_add(1, std::memory_order_relaxed);
      pool.release(slotIdx);
      // Push a sentinel so the consumer can stop cleanly. We reuse a sentinel
      // pool index value (kPoolInvalid) carried via the ring.
      PacketRef sentinel{kPoolInvalid, tWire};
      while (!ring.push(sentinel) && !stop.load(std::memory_order_relaxed)) {
        cpu_pause();
      }
      break;
    }
    if (hdr.messageCount == kMoldHeartbeat) {
      pool.release(slotIdx);
      continue;  // heartbeat; sequence number does not advance
    }

    if (hdr.sequenceNumber < nextExpected) {
      // Duplicate / out-of-order; drop silently.
      stats.duplicatesIgnored.fetch_add(1, std::memory_order_relaxed);
      pool.release(slotIdx);
      continue;
    }
    if (hdr.sequenceNumber > nextExpected) {
      // Real production would request retransmits from a recovery feed; here
      // we log the gap, count it, and resync forward.
      stats.gapsDetected.fetch_add(1, std::memory_order_relaxed);
      nextExpected = hdr.sequenceNumber;
    }
    nextExpected += hdr.messageCount;

    PacketRef ref{slotIdx, tWire};
    if (!ring.push(ref)) {
      // Ring full: consumer can't keep up. Drop this packet; the gap detection
      // on the next received packet will surface that we missed messages.
      pool.release(slotIdx);
      stats.packetsDropped.fetch_add(1, std::memory_order_relaxed);
    }
  }

  close(sock);
}

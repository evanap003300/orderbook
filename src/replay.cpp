// Replay sender: reads an ITCH file, batches messages into MoldUDP64 packets,
// and sends them over UDP to a target host/port. Used to drive the feed
// handler end-to-end without needing a live exchange feed.
//
// Usage:
//   ./replay --file PATH [--target 127.0.0.1] [--port 30001]
//            [--max-per-packet 16] [--rate max|<msg/s>] [--multicast]
//
// LINUX-AGENT NOTE: For multicast, set --multicast and pick a 224-239.x.x.x
// target. On macOS multicast on loopback is finicky; for dev we recommend
// unicast on 127.0.0.1. On Linux with isolcpus, run this on a separate core
// from the engine (taskset -c <core> ./replay ...).

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "moldudp64.hpp"

struct Args {
  std::string file = "itch_data.NASDAQ_ITCH50";
  std::string target = "127.0.0.1";
  uint16_t port = 30001;
  uint16_t maxPerPacket = 16;
  uint64_t rateMsgPerSec = 0;  // 0 = unlimited
  bool multicast = false;
};

static Args parseArgs(int argc, char** argv) {
  Args a;
  for (int i = 1; i < argc; ++i) {
    std::string k = argv[i];
    auto next = [&](const char* opt) -> std::string {
      if (i + 1 >= argc) {
        fprintf(stderr, "missing arg for %s\n", opt);
        std::exit(1);
      }
      return argv[++i];
    };
    if (k == "--file") a.file = next("--file");
    else if (k == "--target") a.target = next("--target");
    else if (k == "--port") a.port = static_cast<uint16_t>(atoi(next("--port").c_str()));
    else if (k == "--max-per-packet") a.maxPerPacket = static_cast<uint16_t>(atoi(next("--max-per-packet").c_str()));
    else if (k == "--rate") {
      std::string v = next("--rate");
      a.rateMsgPerSec = (v == "max") ? 0 : strtoull(v.c_str(), nullptr, 10);
    } else if (k == "--multicast") a.multicast = true;
    else {
      fprintf(stderr, "unknown arg: %s\n", argv[i]);
      std::exit(1);
    }
  }
  return a;
}

int main(int argc, char** argv) {
  Args args = parseArgs(argc, argv);

  // mmap the ITCH file. Same approach as the engine's file path.
  int fd = open(args.file.c_str(), O_RDONLY);
  if (fd < 0) { perror("open"); return 1; }
  struct stat sb;
  fstat(fd, &sb);
  size_t fileSize = sb.st_size;
  void* fileMap = mmap(nullptr, fileSize, PROT_READ, MAP_FILE | MAP_PRIVATE, fd, 0);
  if (fileMap == MAP_FAILED) { perror("mmap"); close(fd); return 1; }
  const uint8_t* data = static_cast<const uint8_t*>(fileMap);
  const uint8_t* end = data + fileSize;

  int sock = socket(AF_INET, SOCK_DGRAM, 0);
  if (sock < 0) { perror("socket"); return 1; }
  if (args.multicast) {
    unsigned char ttl = 1;
    setsockopt(sock, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl));
  }

  sockaddr_in dst{};
  dst.sin_family = AF_INET;
  dst.sin_port = htons(args.port);
  dst.sin_addr.s_addr = inet_addr(args.target.c_str());

  fprintf(stderr, "replay: sending %s -> %s:%u (max %u msgs/pkt, rate=%s)\n",
          args.file.c_str(), args.target.c_str(), args.port, args.maxPerPacket,
          args.rateMsgPerSec ? std::to_string(args.rateMsgPerSec).c_str()
                             : "max");

  char session[10] = {'O','R','D','B','K','S','E','S','1','\0'};
  uint64_t seq = 1;
  uint64_t totalMsgs = 0;
  uint64_t totalPkts = 0;

  uint8_t packet[2048];
  const uint8_t* batchPtrs[256];
  uint16_t batchLens[256];

  auto monoNs = []() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<uint64_t>(ts.tv_sec) * 1000000000ull + ts.tv_nsec;
  };
  uint64_t startNs = monoNs();
  uint64_t nsPerMsg = args.rateMsgPerSec
                         ? 1000000000ull / args.rateMsgPerSec
                         : 0;

  while (data < end) {
    uint16_t batchCount = 0;
    uint16_t packetUsed = kMoldHeaderBytes;
    const uint8_t* batchStart = data;

    while (data < end && batchCount < args.maxPerPacket) {
      uint16_t mlen = ntohs(*reinterpret_cast<const uint16_t*>(data));
      // Per-message overhead in the packet body: 2-byte length prefix + payload.
      if (packetUsed + 2 + mlen > sizeof(packet)) break;
      batchPtrs[batchCount] = data + 2;
      batchLens[batchCount] = mlen;
      packetUsed += 2 + mlen;
      data += 2 + mlen;
      batchCount++;
    }

    if (batchCount == 0) break;  // single message wouldn't fit; abort

    size_t pktBytes = buildMoldPacket(packet, sizeof(packet), session, seq,
                                      batchPtrs, batchLens, batchCount);
    if (pktBytes == 0) {
      fprintf(stderr, "replay: failed to build packet at seq=%llu\n",
              static_cast<unsigned long long>(seq));
      break;
    }

    // Send. If we're paced, wait until the target time before sending.
    if (nsPerMsg) {
      uint64_t targetNs = startNs + totalMsgs * nsPerMsg;
      uint64_t now;
      while ((now = monoNs()) < targetNs) {
        // tight loop; nsPerMsg is typically small enough that sleep adds jitter
      }
    }

    ssize_t sent = sendto(sock, packet, pktBytes, 0,
                          reinterpret_cast<sockaddr*>(&dst), sizeof(dst));
    if (sent < 0) {
      perror("sendto");
      break;
    }

    seq += batchCount;
    totalMsgs += batchCount;
    totalPkts++;
    if (totalPkts % 100000 == 0) {
      double progress = 100.0 * (batchStart - static_cast<const uint8_t*>(fileMap)) / fileSize;
      fprintf(stderr, "replay: %.1f%% (%llu msgs, %llu pkts)\n", progress,
              static_cast<unsigned long long>(totalMsgs),
              static_cast<unsigned long long>(totalPkts));
    }
  }

  // Send end-of-session marker (messageCount = 0xFFFF).
  size_t eosBytes = buildMoldPacket(packet, sizeof(packet), session, seq,
                                    nullptr, nullptr, 0);
  // Manually patch the messageCount field to the EOS sentinel.
  uint16_t eosBe = htons(kMoldEndOfSession);
  memcpy(packet + 18, &eosBe, 2);
  sendto(sock, packet, eosBytes, 0, reinterpret_cast<sockaddr*>(&dst),
         sizeof(dst));

  uint64_t endNs = monoNs();
  double seconds = (endNs - startNs) / 1e9;
  fprintf(stderr,
          "replay done: %llu msgs in %llu pkts, %.2fs, %.0f msg/s\n",
          static_cast<unsigned long long>(totalMsgs),
          static_cast<unsigned long long>(totalPkts), seconds,
          totalMsgs / seconds);

  munmap(fileMap, fileSize);
  close(fd);
  close(sock);
  return 0;
}

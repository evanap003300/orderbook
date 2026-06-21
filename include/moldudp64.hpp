#pragma once
#include <arpa/inet.h>
#include <stdint.h>

#include <cstring>

// NASDAQ MoldUDP64 packet framing.
//
// Wire layout (all multi-byte fields big-endian):
//
//   bytes 0-9   : Session   (10 ASCII characters identifying the feed
//                 instance; identical for all packets in one session)
//   bytes 10-17 : SequenceNumber (uint64) - the sequence number of the FIRST
//                 message in this packet. Subsequent messages in the same
//                 packet are implicitly numbered seq, seq+1, seq+2, ...
//   bytes 18-19 : MessageCount (uint16) - number of messages in this packet.
//                 Special value 0xFFFF = "end of session" marker.
//                 Value 0 = heartbeat (no payload).
//   bytes 20.. : zero or more messages, each prefixed by:
//                 - 2 bytes: MessageLength (uint16, payload bytes, NOT
//                   including the 2 length bytes themselves)
//                 - MessageLength bytes: the ITCH message payload (which
//                   itself begins with the 1-byte ITCH message type).
//
// Sequence semantics:
//   * Receiver tracks `nextExpected`. On each packet, compare the packet's
//     SequenceNumber to nextExpected.
//   * If equal: normal flow. Process messages, advance nextExpected by
//     MessageCount.
//   * If greater: gap detected. (In production, request retransmits from a
//     recovery feed; here we log and skip.)
//   * If less: duplicate or out-of-order. Ignore.

constexpr size_t kMoldHeaderBytes = 20;
constexpr uint16_t kMoldEndOfSession = 0xFFFF;
constexpr uint16_t kMoldHeartbeat = 0;

// Parsed view over a MoldUDP64 packet header. The packet bytes are NOT copied.
struct MoldHeader {
  char session[10];
  uint64_t sequenceNumber;
  uint16_t messageCount;
};

// Parse the 20-byte MoldUDP64 header. Returns true on success. `data` must
// point to at least kMoldHeaderBytes of valid memory.
inline bool parseMoldHeader(const uint8_t* data, MoldHeader& out) {
  std::memcpy(out.session, data, 10);
  uint64_t seqBe;
  std::memcpy(&seqBe, data + 10, 8);
  // big-endian to host. ntohll isn't standard; do it by parts.
  uint32_t hi = ntohl(static_cast<uint32_t>(seqBe & 0xFFFFFFFFu));
  uint32_t lo = ntohl(static_cast<uint32_t>(seqBe >> 32));
  out.sequenceNumber = (static_cast<uint64_t>(hi) << 32) | lo;
  uint16_t cntBe;
  std::memcpy(&cntBe, data + 18, 2);
  out.messageCount = ntohs(cntBe);
  return true;
}

// Iterates ITCH messages within a packet body (the bytes after kMoldHeaderBytes).
// Caller is responsible for matching message count and not stepping past the
// packet end.
class MoldMessageIterator {
 public:
  MoldMessageIterator(const uint8_t* body, size_t bodyBytes)
      : cursor(body), end(body + bodyBytes) {}

  // Returns nullptr when exhausted (or on truncation). `outLen` is set to
  // the message length on success.
  const uint8_t* next(uint16_t& outLen) {
    if (cursor + 2 > end) return nullptr;
    uint16_t lenBe;
    std::memcpy(&lenBe, cursor, 2);
    uint16_t len = ntohs(lenBe);
    cursor += 2;
    if (cursor + len > end) return nullptr;  // truncated
    const uint8_t* msg = cursor;
    cursor += len;
    outLen = len;
    return msg;
  }

 private:
  const uint8_t* cursor;
  const uint8_t* end;
};

// Builder helper used by the replay sender. Writes a complete MoldUDP64 packet
// (header + N length-prefixed messages) into `out`. Returns total bytes
// written, or 0 if out of space. `messages` is an array of (ptr, len) pairs.
// Caller must ensure the packet fits in `outCap` (typically 1500-byte MTU).
inline size_t buildMoldPacket(uint8_t* out, size_t outCap,
                              const char session[10], uint64_t sequenceNumber,
                              const uint8_t* const* messages,
                              const uint16_t* messageLengths,
                              uint16_t messageCount) {
  if (outCap < kMoldHeaderBytes) return 0;
  std::memcpy(out, session, 10);

  // Sequence number, big-endian (same pair-of-uint32 trick as the parser).
  uint32_t hi = htonl(static_cast<uint32_t>(sequenceNumber >> 32));
  uint32_t lo = htonl(static_cast<uint32_t>(sequenceNumber & 0xFFFFFFFFu));
  uint64_t seqBe = (static_cast<uint64_t>(lo) << 32) | hi;
  std::memcpy(out + 10, &seqBe, 8);

  uint16_t cntBe = htons(messageCount);
  std::memcpy(out + 18, &cntBe, 2);

  size_t off = kMoldHeaderBytes;
  for (uint16_t i = 0; i < messageCount; ++i) {
    if (off + 2 + messageLengths[i] > outCap) return 0;
    uint16_t lenBe = htons(messageLengths[i]);
    std::memcpy(out + off, &lenBe, 2);
    off += 2;
    std::memcpy(out + off, messages[i], messageLengths[i]);
    off += messageLengths[i];
  }
  return off;
}

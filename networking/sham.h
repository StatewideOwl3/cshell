#ifndef SHAM_H
#define SHAM_H

#include <stdint.h>

#define SHAM_FLAG_SYN 0x1
#define SHAM_FLAG_ACK 0x2
#define SHAM_FLAG_FIN 0x4

#define SHAM_MTU 1400
#define SHAM_DATA_LEN 1024

// Fixed sender window (packets)
#define SHAM_SENDER_WIN_PKTS 10
// Retransmission timeout in milliseconds
#define SHAM_RTO_MS 500

struct sham_header {
    uint32_t seq_num;      // First byte sequence number of this payload
    uint32_t ack_num;      // Next expected byte (cumulative ACK)
    uint16_t flags;        // SYN/ACK/FIN
    uint16_t window_size;  // Receiver advertised window (bytes)
} __attribute__((packed));

#endif // SHAM_H

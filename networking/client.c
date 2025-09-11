#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <time.h>

#include "sham.h"
#include "common.h"

#define ROLE "client"

typedef struct {
    uint32_t seq;      // starting byte seq of this chunk
    size_t len;        // data length
    uint64_t send_ts;  // last send time in ms
    int in_flight;     // 1 if sent and awaiting ack
    unsigned char data[SHAM_DATA_LEN];
} segment_t;

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <server_ip> <server_port> <input_file> <output_file_name> [loss_rate]\n", argv[0]);
        fprintf(stderr, "Chat: %s <server_ip> <server_port> --chat [loss_rate]\n", argv[0]);
        return 1;
    }

    // Collect info from input
    const char *server_ip = argv[1];
    int port = atoi(argv[2]);
    int chat_mode = 0;
    const char *infile = NULL;
    const char *outfile_name = NULL;
    double loss_rate = 0.0;

    if (argc >= 4 && strcmp(argv[3], "--chat") == 0) {
        chat_mode = 1;
        if (argc >= 5) loss_rate = atof(argv[4]);
    } else {
        if (argc < 5) {
            fprintf(stderr, "File mode usage: %s <server_ip> <server_port> <input_file> <output_file_name> [loss_rate]\n", argv[0]);
            return 1;
        }
        infile = argv[3];
        outfile_name = argv[4];
        if (argc >= 6) loss_rate = atof(argv[5]);
    }

    open_log_file(ROLE);
    srand((unsigned)time(NULL));

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) { perror("socket"); return 1; }

    struct sockaddr_in srv = {0};
    srv.sin_family = AF_INET;
    srv.sin_port = htons(port);
    // Convert string form of server IPv4 address to binary form and store in srv struct's in_addr field
    if (inet_pton(AF_INET, server_ip, &srv.sin_addr) != 1) {
        perror("inet_pton");
        return 1;
    }

    unsigned char buffer[sizeof(struct sham_header) + SHAM_DATA_LEN + 512];
    struct sham_header hdr;

    // 3-way handshake
    uint32_t isn_client = (uint32_t)(rand() & 0x7fffffff);

    // Send SYN
    struct sham_header syn = {0};
    syn.seq_num = htonl(isn_client);
    syn.ack_num = htonl(0);
    syn.flags = htons(SHAM_FLAG_SYN);
    syn.window_size = htons(SHAM_DATA_LEN * SHAM_SENDER_WIN_PKTS);
    memcpy(buffer, &syn, sizeof(syn));
    sendto(sock, buffer, sizeof(syn), 0, (struct sockaddr *)&srv, sizeof(srv));
    log_event("SND SYN SEQ=%u", isn_client);

    // Wait for SYN-ACK
    socklen_t slen = sizeof(srv);
    while (1) {
        ssize_t n = recvfrom(sock, buffer, sizeof(buffer), 0, (struct sockaddr *)&srv, &slen);
        if (n < 0) { if (errno==EINTR) continue; perror("recvfrom"); return 1; }
        if ((size_t)n < sizeof(struct sham_header)) continue;
        memcpy(&hdr, buffer, sizeof(hdr));
        hdr.seq_num = ntohl(hdr.seq_num);
        hdr.ack_num = ntohl(hdr.ack_num);
        hdr.flags = ntohs(hdr.flags);
        hdr.window_size = ntohs(hdr.window_size);
        if ((hdr.flags & SHAM_FLAG_SYN) && (hdr.flags & SHAM_FLAG_ACK)) {
            log_event("RCV SYN SEQ=%u", hdr.seq_num);
            log_event("RCV ACK=%u", hdr.ack_num);
            break;
        }
    }

    // Send ACK for SYN
    struct sham_header ack = {0};
    ack.seq_num = htonl(isn_client + 1);
    ack.ack_num = htonl(hdr.seq_num + 1);
    ack.flags = htons(SHAM_FLAG_ACK);
    ack.window_size = htons(SHAM_DATA_LEN * SHAM_SENDER_WIN_PKTS);
    memcpy(buffer, &ack, sizeof(ack));
    sendto(sock, buffer, sizeof(ack), 0, (struct sockaddr *)&srv, sizeof(srv));
    log_event("SND ACK FOR SYN");

    if (chat_mode) {
        // Enter chat mode
        fd_set rfds;
        int stdin_fd = 0;
        while (1) {
            FD_ZERO(&rfds);
            FD_SET(stdin_fd, &rfds);
            FD_SET(sock, &rfds);
            int maxfd = sock > stdin_fd ? sock : stdin_fd;
            if (select(maxfd + 1, &rfds, NULL, NULL, NULL) < 0) {
                if (errno == EINTR) continue; perror("select"); break;
            }
            if (FD_ISSET(stdin_fd, &rfds)) {
                char line[1024];
                if (!fgets(line, sizeof(line), stdin)) break;
                if (strncmp(line, "/quit", 5) == 0) {
                    // FIN handshake
                    struct sham_header fin = {0};
                    fin.seq_num = htonl(0);
                    fin.ack_num = htonl(0);
                    fin.flags = htons(SHAM_FLAG_FIN);
                    fin.window_size = htons(SHAM_DATA_LEN * SHAM_SENDER_WIN_PKTS);
                    memcpy(buffer, &fin, sizeof(fin));
                    sendto(sock, buffer, sizeof(fin), 0, (struct sockaddr *)&srv, sizeof(srv));
                    log_event("SND FIN SEQ=%u", 0u);
                    // Wait for ACK and FIN from server
                    while (1) {
                        ssize_t n = recvfrom(sock, buffer, sizeof(buffer), 0, (struct sockaddr *)&srv, &slen);
                        if (n <= 0) continue;
                        struct sham_header ih; memcpy(&ih, buffer, sizeof(ih));
                        ih.seq_num = ntohl(ih.seq_num); ih.ack_num = ntohl(ih.ack_num);
                        ih.flags = ntohs(ih.flags); ih.window_size = ntohs(ih.window_size);
                        if (ih.flags & SHAM_FLAG_ACK) {
                            log_event("RCV ACK=%u", ih.ack_num);
                        }
                        if (ih.flags & SHAM_FLAG_FIN) {
                            log_event("RCV FIN SEQ=%u", ih.seq_num);
                            // final ACK
                            struct sham_header fack = {0};
                            fack.seq_num = htonl(0);
                            fack.ack_num = htonl(ih.seq_num + 1);
                            fack.flags = htons(SHAM_FLAG_ACK);
                            fack.window_size = htons(SHAM_DATA_LEN * SHAM_SENDER_WIN_PKTS);
                            memcpy(buffer, &fack, sizeof(fack));
                            sendto(sock, buffer, sizeof(fack), 0, (struct sockaddr *)&srv, sizeof(srv));
                            log_event("SND ACK=%u", ih.seq_num + 1);
                            goto out_chat;
                        }
                    }
                }
                // send as data
                struct sham_header dh = {0};
                dh.seq_num = htonl(0);
                dh.ack_num = htonl(0);
                dh.flags = htons(SHAM_FLAG_ACK);
                dh.window_size = htons(SHAM_DATA_LEN * SHAM_SENDER_WIN_PKTS);
                size_t len = strnlen(line, sizeof(line));
                memcpy(buffer, &dh, sizeof(dh));
                memcpy(buffer + sizeof(dh), line, len);
                sendto(sock, buffer, sizeof(dh) + len, 0, (struct sockaddr *)&srv, sizeof(srv));
                log_event("SND DATA SEQ=%u LEN=%zu", 0u, len);
            }
            if (FD_ISSET(sock, &rfds)) {
                ssize_t n = recvfrom(sock, buffer, sizeof(buffer), 0, (struct sockaddr *)&srv, &slen);
                if (n <= 0) continue;
                if ((size_t)n < sizeof(struct sham_header)) continue;
                struct sham_header ih; memcpy(&ih, buffer, sizeof(ih));
                ih.seq_num = ntohl(ih.seq_num); ih.ack_num = ntohl(ih.ack_num);
                ih.flags = ntohs(ih.flags); ih.window_size = ntohs(ih.window_size);
                size_t dlen = n - sizeof(struct sham_header);
                if (ih.flags & SHAM_FLAG_FIN) {
                    log_event("RCV FIN SEQ=%u", ih.seq_num);
                    // ACK and FIN
                    struct sham_header a = {0};
                    a.seq_num = htonl(0); a.ack_num = htonl(ih.seq_num + 1); a.flags = htons(SHAM_FLAG_ACK); a.window_size = htons(SHAM_DATA_LEN * SHAM_SENDER_WIN_PKTS);
                    memcpy(buffer, &a, sizeof(a));
                    sendto(sock, buffer, sizeof(a), 0, (struct sockaddr *)&srv, sizeof(srv));
                    log_event("SND ACK FOR FIN");
                    struct sham_header fin = {0}; fin.seq_num=htonl(0); fin.ack_num=htonl(0); fin.flags=htons(SHAM_FLAG_FIN); fin.window_size=htons(SHAM_DATA_LEN * SHAM_SENDER_WIN_PKTS);
                    memcpy(buffer, &fin, sizeof(fin));
                    sendto(sock, buffer, sizeof(fin), 0, (struct sockaddr *)&srv, sizeof(srv));
                    log_event("SND FIN SEQ=%u", 0u);
                } else if (dlen > 0) {
                    if (should_drop(loss_rate)) { log_event("DROP DATA SEQ=%u", ih.seq_num); continue; }
                    log_event("RCV DATA SEQ=%u LEN=%zu", ih.seq_num, dlen);
                    fwrite(buffer + sizeof(struct sham_header), 1, dlen, stdout);
                    fflush(stdout);
                }
            }
        }
    out_chat:
        close_log_file();
        return 0;
    }

    // File transfer mode
    FILE *inf = fopen(infile, "rb");
    if (!inf) { perror("fopen input"); return 1; }

    // prepare segments
    const size_t MAX_SEGS = 65536 / SHAM_DATA_LEN + 4; // arbitrary upper bound
    segment_t *segs = NULL; size_t seg_count = 0; size_t seg_cap = 0;

    // We'll transmit a first pseudo segment carrying the output file name followed by \n
    {
        size_t name_len = strlen(outfile_name);
        size_t line_len = name_len + 1; // include \n
        seg_cap = 1024;
        segs = (segment_t *)calloc(seg_cap, sizeof(segment_t));
        seg_count = 1;
        segs[0].seq = 1; // data starts at 1
        segs[0].len = line_len;
        memset(segs[0].data, 0, sizeof(segs[0].data));
        memcpy(segs[0].data, outfile_name, name_len);
        segs[0].data[name_len] = '\n';
    }

    // Read file into segments of SHAM_DATA_LEN
    uint32_t next_seq = 1 + (uint32_t)segs[0].len;
    while (1) {
        if (seg_count == seg_cap) { seg_cap *= 2; segs = (segment_t *)realloc(segs, seg_cap * sizeof(segment_t)); }
        segment_t *s = &segs[seg_count];
        s->seq = next_seq;
        size_t n = fread(s->data, 1, SHAM_DATA_LEN, inf);
        s->len = n;
        if (n == 0) break;
        seg_count++;
        next_seq += (uint32_t)n;
        if (n < SHAM_DATA_LEN) break; // EOF
    }
    fclose(inf);

    uint32_t last_byte = next_seq - 1; // last byte number in stream

    size_t base_idx = 0; // first unacked segment index
    size_t next_idx = 0; // next segment to send
    uint32_t last_acked = 0; // last cumulatively acked byte

    // Flow control tracking
    uint32_t last_byte_sent = 0;    // last byte index sent (0 means none)
    uint32_t last_byte_acked = 0;   // last cumulatively acked byte index
    uint32_t remote_win_bytes = SHAM_DATA_LEN * SHAM_SENDER_WIN_PKTS; // default until ACKs arrive

    // send/recv loop with sliding window and RTO
    while (base_idx < seg_count) {
        // Send new segments within window, respect receiver's advertised window
        while (next_idx < seg_count && (next_idx - base_idx) < SHAM_SENDER_WIN_PKTS) {
            segment_t *s = &segs[next_idx];
            // Bytes in flight according to flow control
            uint32_t inflight = (last_byte_sent > last_byte_acked) ? (last_byte_sent - last_byte_acked) : 0;
            if (inflight + (uint32_t)s->len > remote_win_bytes) {
                // Cannot send more due to receiver window; break to wait for ACKs
                break;
            }
            struct sham_header dh = {0};
            dh.seq_num = htonl(s->seq);
            dh.ack_num = htonl(last_acked + 1);
            dh.flags = htons(0);
            dh.window_size = htons(SHAM_DATA_LEN * SHAM_SENDER_WIN_PKTS);
            memcpy(buffer, &dh, sizeof(dh));
            memcpy(buffer + sizeof(dh), s->data, s->len);
            sendto(sock, buffer, sizeof(dh) + s->len, 0, (struct sockaddr *)&srv, sizeof(srv));
            if (!s->in_flight) {
                log_event("SND DATA SEQ=%u LEN=%zu", s->seq, s->len);
            } else {
                log_event("RETX DATA SEQ=%u LEN=%zu", s->seq, s->len);
            }
            s->in_flight = 1;
            s->send_ts = now_ms();
            // Update last_byte_sent to end of this segment
            uint32_t endb = s->seq + (uint32_t)s->len - 1;
            if (endb > last_byte_sent) last_byte_sent = endb;
            next_idx++;
        }

        // Wait for ACK or timeout
        struct timeval tv; tv.tv_sec = 0; tv.tv_usec = SHAM_RTO_MS * 1000 / 2; // half RTO check
        fd_set rfds; FD_ZERO(&rfds); FD_SET(sock, &rfds);
        int rv = select(sock + 1, &rfds, NULL, NULL, &tv);
        if (rv < 0) {
            if (errno == EINTR) continue;
            perror("select");
            break;
        }

        if (rv == 1 && FD_ISSET(sock, &rfds)) {
            ssize_t n = recvfrom(sock, buffer, sizeof(buffer), 0, (struct sockaddr *)&srv, &slen);
            if (n >= (ssize_t)sizeof(struct sham_header)) {
                struct sham_header ah; memcpy(&ah, buffer, sizeof(ah));
                ah.seq_num = ntohl(ah.seq_num); ah.ack_num = ntohl(ah.ack_num);
                ah.flags = ntohs(ah.flags); ah.window_size = ntohs(ah.window_size);
                if (ah.flags & SHAM_FLAG_ACK) {
                    log_event("RCV ACK=%u", ah.ack_num);
                    // Update flow control state
                    if (ah.ack_num > 0) {
                        last_byte_acked = ah.ack_num - 1;
                    }
                    remote_win_bytes = ah.window_size;
                    // cumulative ack: mark segments with end <= ack_num as acked
                    while (base_idx < seg_count) {
                        segment_t *bs = &segs[base_idx];
                        uint32_t endb = bs->seq + (uint32_t)bs->len;
                        if (endb <= ah.ack_num) {
                            base_idx++;
                            last_acked = endb - 1;
                        } else break;
                    }
                }
            }
        }

        // Check for timeout
        uint64_t tnow = now_ms();
        for (size_t i = base_idx; i < next_idx; i++) {
            segment_t *s = &segs[i];
            if (s->in_flight && (tnow - s->send_ts) >= SHAM_RTO_MS) {
                // timeout -> retransmit this and all subsequent in-flight up to next_idx? We'll do selective by re-sending this now
                log_event("TIMEOUT SEQ=%u", s->seq);
                struct sham_header dh = {0};
                dh.seq_num = htonl(s->seq);
                dh.ack_num = htonl(last_acked + 1);
                dh.flags = htons(0);
                dh.window_size = htons(SHAM_DATA_LEN * SHAM_SENDER_WIN_PKTS);
                memcpy(buffer, &dh, sizeof(dh));
                memcpy(buffer + sizeof(dh), s->data, s->len);
                sendto(sock, buffer, sizeof(dh) + s->len, 0, (struct sockaddr *)&srv, sizeof(srv));
                log_event("RETX DATA SEQ=%u LEN=%zu", s->seq, s->len);
                s->send_ts = now_ms();
            }
        }
    }

    // After all data acked, start 4-way FIN
    struct sham_header fin = {0};
    fin.seq_num = htonl(last_byte + 1);
    fin.ack_num = htonl(0);
    fin.flags = htons(SHAM_FLAG_FIN);
    fin.window_size = htons(SHAM_DATA_LEN * SHAM_SENDER_WIN_PKTS);
    memcpy(buffer, &fin, sizeof(fin));
    sendto(sock, buffer, sizeof(fin), 0, (struct sockaddr *)&srv, sizeof(srv));
    log_event("SND FIN SEQ=%u", last_byte + 1);

    int got_ack = 0, got_fin = 0;
    while (!(got_ack && got_fin)) {
        ssize_t n = recvfrom(sock, buffer, sizeof(buffer), 0, (struct sockaddr *)&srv, &slen);
        if (n < (ssize_t)sizeof(struct sham_header)) continue;
        struct sham_header ih; memcpy(&ih, buffer, sizeof(ih));
        ih.seq_num = ntohl(ih.seq_num); ih.ack_num = ntohl(ih.ack_num);
        ih.flags = ntohs(ih.flags); ih.window_size = ntohs(ih.window_size);
        if (ih.flags & SHAM_FLAG_ACK) { got_ack = 1; log_event("RCV ACK=%u", ih.ack_num); }
        if (ih.flags & SHAM_FLAG_FIN) {
            got_fin = 1; log_event("RCV FIN SEQ=%u", ih.seq_num);
            struct sham_header a = {0};
            a.seq_num = htonl(last_byte + 2);
            a.ack_num = htonl(ih.seq_num + 1);
            a.flags = htons(SHAM_FLAG_ACK);
            a.window_size = htons(SHAM_DATA_LEN * SHAM_SENDER_WIN_PKTS);
            memcpy(buffer, &a, sizeof(a));
            sendto(sock, buffer, sizeof(a), 0, (struct sockaddr *)&srv, sizeof(srv));
            log_event("SND ACK=%u", ih.seq_num + 1);
        }
    }

    free(segs);
    close_log_file();
    return 0;
}

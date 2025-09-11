#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <time.h>
#include <openssl/md5.h>

#include "sham.h"
#include "common.h"

#define ROLE "server"

static void compute_md5_and_print(const char *filename) {
    unsigned char c[MD5_DIGEST_LENGTH];
    MD5_CTX mdContext;
    int bytes;
    unsigned char data[4096];

    FILE *inFile = fopen(filename, "rb");
    if (inFile == NULL) {
        perror("fopen");
        return;
    }

    MD5_Init(&mdContext);
    while ((bytes = fread(data, 1, 4096, inFile)) != 0) {
        MD5_Update(&mdContext, data, bytes);
    }
    MD5_Final(c, &mdContext);
    fclose(inFile);

    // Print MD5 in lowercase hex
    printf("MD5: ");
    for (int i = 0; i < MD5_DIGEST_LENGTH; i++)
        printf("%02x", c[i]);
    printf("\n");
    fflush(stdout);
}

static ssize_t recv_packet(int sock, void *buf, size_t len, int flags, struct sockaddr_in *peer, socklen_t *plen) {
    return recvfrom(sock, buf, len, flags, (struct sockaddr *)peer, plen);
}

static ssize_t send_packet(int sock, const void *buf, size_t len, int flags, const struct sockaddr_in *peer) {
    return sendto(sock, buf, len, flags, (const struct sockaddr *)peer, sizeof(*peer));
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <port> [--chat] [loss_rate]\n", argv[0]);
        return 1;
    }

    int port = atoi(argv[1]);
    int chat_mode = 0;
    double loss_rate = 0.0;
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--chat") == 0) chat_mode = 1;
        else loss_rate = atof(argv[i]);
    }

    open_log_file(ROLE);

    // Creating a socket and binding it - IPv4 with UDP
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) { perror("socket"); return 1; }

    struct sockaddr_in addr = {0}; // struct sockaddr_in has: internet address family, port, internet address
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port); // Byte order (Little Endian, Big Endian) - host to network
    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) { perror("bind"); return 1; }

    // Handshake
    struct sham_header hdr;
    unsigned char buffer[SHAM_DATA_LEN + sizeof(struct sham_header)];
    struct sockaddr_in peer; socklen_t peerlen = sizeof(peer);

    // Wait for SYN
    while (1) {
        ssize_t n = recv_packet(sock, buffer, sizeof(buffer), 0, &peer, &peerlen);
        if (n < 0) { if (errno==EINTR) continue; perror("recvfrom"); return 1; }
        if ((size_t)n < sizeof(struct sham_header)) continue;
        memcpy(&hdr, buffer, sizeof(hdr));
        hdr.seq_num = ntohl(hdr.seq_num);
        hdr.ack_num = ntohl(hdr.ack_num);
        hdr.flags = ntohs(hdr.flags);
        hdr.window_size = ntohs(hdr.window_size);
        if (hdr.flags & SHAM_FLAG_SYN) {
            log_event("RCV SYN SEQ=%u", hdr.seq_num);
            break;
        }
    }

    // Send SYN-ACK
    uint32_t isn_server = (uint32_t) (rand() & 0x7fffffff);
    struct sham_header out = {0};
    out.seq_num = htonl(isn_server);
    out.ack_num = htonl(hdr.seq_num + 1);
    out.flags = htons(SHAM_FLAG_SYN | SHAM_FLAG_ACK);
    out.window_size = htons(SHAM_DATA_LEN * SHAM_SENDER_WIN_PKTS);
    memcpy(buffer, &out, sizeof(out));
    send_packet(sock, buffer, sizeof(out), 0, &peer);
    log_event("SND SYN-ACK SEQ=%u ACK=%u", isn_server, hdr.seq_num + 1);

    // Wait for ACK
    while (1) {
        ssize_t n = recv_packet(sock, buffer, sizeof(buffer), 0, &peer, &peerlen);
        if (n < 0) { if (errno==EINTR) continue; perror("recvfrom"); return 1; }
        if ((size_t)n < sizeof(struct sham_header)) continue;
        struct sham_header h; memcpy(&h, buffer, sizeof(h));
        h.seq_num = ntohl(h.seq_num); h.ack_num = ntohl(h.ack_num);
        h.flags = ntohs(h.flags); h.window_size = ntohs(h.window_size);
        if (h.flags & SHAM_FLAG_ACK) {
            log_event("RCV ACK FOR SYN");
            break;
        }
    }

    // Connected
    uint32_t expected_seq = hdr.seq_num + 1; // used only for chat-mode ACKs below
    uint32_t recv_highest_inorder = expected_seq - 1;
    size_t recv_buf_cap = 1 << 20; // 1MB
    unsigned char *recv_buf = (unsigned char *)malloc(recv_buf_cap);
    size_t recv_size = 0;

    // For file mode, we need an output file name from client; protocol: first DATA packet contains a zero-terminated filename line? But spec says client provides output_file_name to server; we will accept that the first data bytes are the file name ending with '\n' and then file data follows. To keep it simple, we expect first data packet to begin with filename line.

    char out_filename[256] = {0};
    int got_filename = 0;

    FILE *outf = NULL;

    if (chat_mode) {
        // Chat mode: echo messages between stdin and network
        log_event("FLOW WIN UPDATE=%u", SHAM_DATA_LEN * SHAM_SENDER_WIN_PKTS);
        fd_set rfds;
        int stdin_fd = 0;
        while (1) {
            FD_ZERO(&rfds);
            FD_SET(stdin_fd, &rfds);
            FD_SET(sock, &rfds);
            int maxfd = sock > stdin_fd ? sock : stdin_fd;
            if (select(maxfd + 1, &rfds, NULL, NULL, NULL) < 0) {
                if (errno == EINTR) continue;
                perror("select");
                break;
            }
            if (FD_ISSET(stdin_fd, &rfds)) {
                char line[1024];
                if (!fgets(line, sizeof(line), stdin)) break;
                if (strncmp(line, "/quit", 5) == 0) {
                    // Start FIN handshake
                    struct sham_header fin = {0};
                    fin.seq_num = htonl(0);
                    fin.ack_num = htonl(0);
                    fin.flags = htons(SHAM_FLAG_FIN);
                    fin.window_size = htons(SHAM_DATA_LEN * SHAM_SENDER_WIN_PKTS);
                    memcpy(buffer, &fin, sizeof(fin));
                    send_packet(sock, buffer, sizeof(fin), 0, &peer);
                    log_event("SND FIN SEQ=%u", 0u);
                    break;
                }
                // Send as data
                struct sham_header dh = {0};
                dh.seq_num = htonl(0);
                dh.ack_num = htonl(expected_seq);
                dh.flags = htons(SHAM_FLAG_ACK);
                dh.window_size = htons(SHAM_DATA_LEN * SHAM_SENDER_WIN_PKTS);
                size_t len = strnlen(line, sizeof(line));
                memcpy(buffer, &dh, sizeof(dh));
                memcpy(buffer + sizeof(dh), line, len);
                send_packet(sock, buffer, sizeof(dh) + len, 0, &peer);
                log_event("SND DATA SEQ=%u LEN=%zu", 0u, len);
            }
            if (FD_ISSET(sock, &rfds)) {
                ssize_t n = recv_packet(sock, buffer, sizeof(buffer), 0, &peer, &peerlen);
                if (n <= 0) continue;
                if ((size_t)n < sizeof(struct sham_header)) continue;
                struct sham_header ih; memcpy(&ih, buffer, sizeof(ih));
                ih.seq_num = ntohl(ih.seq_num); ih.ack_num = ntohl(ih.ack_num);
                ih.flags = ntohs(ih.flags); ih.window_size = ntohs(ih.window_size);
                size_t dlen = n - sizeof(struct sham_header);
                if (ih.flags & SHAM_FLAG_FIN) {
                    log_event("RCV FIN SEQ=%u", ih.seq_num);
                    // ACK FIN
                    struct sham_header ack = {0};
                    ack.seq_num = htonl(0);
                    ack.ack_num = htonl(ih.seq_num + 1);
                    ack.flags = htons(SHAM_FLAG_ACK);
                    ack.window_size = htons(SHAM_DATA_LEN * SHAM_SENDER_WIN_PKTS);
                    memcpy(buffer, &ack, sizeof(ack));
                    send_packet(sock, buffer, sizeof(ack), 0, &peer);
                    log_event("SND ACK FOR FIN");
                    // Send own FIN
                    struct sham_header fin = {0};
                    fin.seq_num = htonl(0);
                    fin.ack_num = htonl(0);
                    fin.flags = htons(SHAM_FLAG_FIN);
                    fin.window_size = htons(SHAM_DATA_LEN * SHAM_SENDER_WIN_PKTS);
                    memcpy(buffer, &fin, sizeof(fin));
                    send_packet(sock, buffer, sizeof(fin), 0, &peer);
                    log_event("SND FIN SEQ=%u", 0u);
                    // Expect final ACK (optional)
                    break;
                } else if (dlen > 0) {
                    if (should_drop(loss_rate)) {
                        log_event("DROP DATA SEQ=%u", ih.seq_num);
                        continue;
                    }
                    log_event("RCV DATA SEQ=%u LEN=%zu", ih.seq_num, dlen);
                    fwrite(buffer + sizeof(struct sham_header), 1, dlen, stdout);
                    fflush(stdout);
                }
            }
        }
        close_log_file();
        return 0;
    }

    // File transfer mode
    // Receive data, send cumulative ACKs for highest in-order only
    // Maintain an out-of-order buffer (singly-linked list sorted by seq)
    typedef struct ooo_node {
        uint32_t seq;       // starting byte of this chunk
        size_t len;         // length of this chunk
        unsigned char *data;// payload buffer
        struct ooo_node *next;
    } ooo_node_t;
    ooo_node_t *ooo_head = NULL;

    // For file-mode stream, client starts at seq=1, so we expect 1 first
    uint32_t ft_expected_seq = 1;
    while (1) {
        ssize_t n = recv_packet(sock, buffer, sizeof(buffer), 0, &peer, &peerlen);
        if (n < 0) { if (errno==EINTR) continue; perror("recvfrom"); break; }
        if ((size_t)n < sizeof(struct sham_header)) continue;
        struct sham_header ih; memcpy(&ih, buffer, sizeof(ih));
        ih.seq_num = ntohl(ih.seq_num); ih.ack_num = ntohl(ih.ack_num);
        ih.flags = ntohs(ih.flags); ih.window_size = ntohs(ih.window_size);
        size_t dlen = n - sizeof(struct sham_header);

        if (ih.flags & SHAM_FLAG_FIN) {
            log_event("RCV FIN SEQ=%u", ih.seq_num);
            // ACK FIN
            struct sham_header ack = {0};
            ack.seq_num = htonl(isn_server);
            ack.ack_num = htonl(ih.seq_num + 1);
            ack.flags = htons(SHAM_FLAG_ACK);
            ack.window_size = htons((uint16_t)(recv_buf_cap - recv_size));
            memcpy(buffer, &ack, sizeof(ack));
            send_packet(sock, buffer, sizeof(ack), 0, &peer);
            log_event("SND ACK FOR FIN");
            // Send own FIN
            struct sham_header fin = {0};
            fin.seq_num = htonl(isn_server + 1);
            fin.ack_num = htonl(0);
            fin.flags = htons(SHAM_FLAG_FIN);
            fin.window_size = htons((uint16_t)(recv_buf_cap - recv_size));
            memcpy(buffer, &fin, sizeof(fin));
            send_packet(sock, buffer, sizeof(fin), 0, &peer);
            log_event("SND FIN SEQ=%u", isn_server + 1);
            // expect final ACK and then break
            break;
        }

        if (dlen == 0) continue;
        if (should_drop(loss_rate)) {
            log_event("DROP DATA SEQ=%u", ih.seq_num);
            // Do not ACK dropped packet to simulate loss
            continue;
        }
        log_event("RCV DATA SEQ=%u LEN=%zu", ih.seq_num, dlen);

        // Helper lambda-like macros to write a contiguous chunk to file handling filename-first logic
        // Write data pointed by ptr with length len, updating recv_size
        #define WRITE_DATA(ptr,len) do { \
            fwrite((ptr), 1, (len), outf); \
            recv_size += (len); \
        } while(0)

        // If this packet starts exactly at ft_expected_seq, consume it (and possibly buffered successors)
        unsigned char *payload = buffer + sizeof(struct sham_header);
        if (ih.seq_num == ft_expected_seq) {
            // Ensure output file is opened and filename parsed if not yet
            if (!got_filename) {
                char *nl = memchr(payload, '\n', dlen);
                if (nl) {
                    size_t namelen = nl - (char *)payload;
                    if (namelen >= sizeof(out_filename)) namelen = sizeof(out_filename)-1;
                    memcpy(out_filename, payload, namelen);
                    out_filename[namelen] = '\0';
                    got_filename = 1;
                    outf = fopen(out_filename, "wb");
                    if (!outf) { perror("fopen output"); break; }
                    size_t rem = dlen - (namelen + 1);
                    if (rem > 0) {
                        WRITE_DATA(nl + 1, rem);
                    }
                    ft_expected_seq += (uint32_t)dlen;
                } else {
                    // Protocol requires filename in first packet entirely
                    fprintf(stderr, "Protocol error: filename not provided in first packet.\n");
                    break;
                }
            } else {
                // normal file data
                if (recv_size + dlen > recv_buf_cap) {
                    recv_buf_cap = (recv_size + dlen) * 2;
                    recv_buf = (unsigned char *)realloc(recv_buf, recv_buf_cap);
                }
                WRITE_DATA(payload, dlen);
                ft_expected_seq += (uint32_t)dlen;
            }

            // After consuming this packet, try to flush any buffered in-order packets
            ooo_node_t *prev = NULL, *cur = ooo_head;
            while (cur) {
                if (cur->seq == ft_expected_seq) {
                    // consume
                    if (!got_filename) {
                        // Should never happen because filename must have been processed in the first in-order packet
                        ;
                    } else {
                        if (recv_size + cur->len > recv_buf_cap) {
                            recv_buf_cap = (recv_size + cur->len) * 2;
                            recv_buf = (unsigned char *)realloc(recv_buf, recv_buf_cap);
                        }
                        WRITE_DATA(cur->data, cur->len);
                    }
                    ft_expected_seq += (uint32_t)cur->len;
                    // remove cur from list
                    ooo_node_t *to_free = cur;
                    if (prev) prev->next = cur->next; else ooo_head = cur->next;
                    cur = (prev ? prev->next : ooo_head);
                    free(to_free->data);
                    free(to_free);
                    continue; // continue scanning from current position
                }
                // list sorted; if current seq > expected, cannot consume further
                if (cur->seq > ft_expected_seq) break;
                prev = cur;
                cur = cur->next;
            }
        } else if (ih.seq_num > ft_expected_seq) {
            // Out-of-order: buffer if not duplicate
            // Insert into sorted linked list if not already present
            ooo_node_t *node = (ooo_node_t *)malloc(sizeof(ooo_node_t));
            node->seq = ih.seq_num;
            node->len = dlen;
            node->data = (unsigned char *)malloc(dlen);
            memcpy(node->data, payload, dlen);
            node->next = NULL;
            // Insert sorted
            if (!ooo_head || ih.seq_num < ooo_head->seq) {
                node->next = ooo_head;
                ooo_head = node;
            } else {
                ooo_node_t *p = ooo_head;
                while (p->next && p->next->seq < ih.seq_num) p = p->next;
                // if exact duplicate (same seq), keep the first (ignore new)
                if (p->next && p->next->seq == ih.seq_num) {
                    free(node->data); free(node); // drop duplicate
                } else {
                    node->next = p->next;
                    p->next = node;
                }
            }
        } else {
            // Duplicate or already consumed; ignore
        }

        // Send cumulative ACK for the highest contiguous byte (ft_expected_seq-1)
        struct sham_header ack = {0};
        ack.seq_num = htonl(isn_server);
        ack.ack_num = htonl(ft_expected_seq);
        ack.flags = htons(SHAM_FLAG_ACK);
        uint16_t win = (uint16_t)((recv_buf_cap - recv_size) > 65535 ? 65535 : (recv_buf_cap - recv_size));
        ack.window_size = htons(win);
        memcpy(buffer, &ack, sizeof(ack));
        send_packet(sock, buffer, sizeof(ack), 0, &peer);
        log_event("SND ACK=%u WIN=%u", ft_expected_seq, win);
        log_event("FLOW WIN UPDATE=%u", win);
    }

    if (outf) fclose(outf);
    if (got_filename) compute_md5_and_print(out_filename);

    free(recv_buf);
    close_log_file();
    return 0;
}

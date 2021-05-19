// Copyright (C) 2018, Cloudflare, Inc.
// Copyright (C) 2018, Alessandro Ghedini
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//
//     * Redistributions in binary form must reproduce the above copyright
//       notice, this list of conditions and the following disclaimer in the
//       documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
// IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include <arpa/inet.h>
#include <errno.h>
#include <ev.h>
#include <fcntl.h>
#include <inttypes.h>
#include <memory.h>
#include <netdb.h>
#include <quiche.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "helper.h"

#define LOCAL_CONN_ID_LEN 16

#define MAX_DATAGRAM_SIZE 1350

#define MAX_BLOCK_SIZE 10000000  // 10Mbytes
#define TIME_SIZE 8
static uint64_t chunk_before_deadline = 0;
static uint64_t recv_num = 0;

struct conn_io {
    ev_timer timer;

    ev_io *watcher;
    ev_io *watcher2;

    int sock;
    int sock2;

    quiche_conn *conn;

    bool first_udp_packet;

    // uint64_t t_last;
    // ssize_t can_send;
    // bool done_writing;
    // ev_timer pace_timer;
};

// MP: PCID(default for second path)
uint8_t server_pcid[LOCAL_CONN_ID_LEN];
uint8_t client_pcid[LOCAL_CONN_ID_LEN];
// *********************************************

// MP: server->client one way delay
uint64_t one_way_delay = 0;

int send_packet(int sock, const uint8_t *out, size_t len) {
    uint8_t out_with_time[MAX_DATAGRAM_SIZE + 2 * TIME_SIZE];
    uint8_t *out_time;

    /**********************************************************/
        // send the timestamp
        uint64_t t = getCurrentTime_mic();
        out_time = (uint8_t *)&t;
        for (int i = 0; i < TIME_SIZE; i++) {
            out_with_time[i] = out_time[i];
        }

        // send the one_way_delay
        out_time = (uint8_t *)&one_way_delay;
        for (int i = TIME_SIZE, j = 0; j < TIME_SIZE; i++, j++) {
            out_with_time[i] = out_time[j];
        }

        for (int i = 2 * TIME_SIZE, j = 0; j < MAX_DATAGRAM_SIZE; i++, j++) {
            out_with_time[i] = out[j];
        }
        /**********************************************************/
        len += 2 * TIME_SIZE;

        ssize_t sent = send(sock, out_with_time, len, 0);

        // struct sockaddr_in listendAddr;
        // socklen_t listendAddrLen = sizeof(listendAddr);
        // getsockname(conn_io->sock, (struct sockaddr *)&listendAddr,
        // &listendAddrLen); fprintf(stderr, "listen address = %s:%d\n",
        // inet_ntoa(listendAddr.sin_addr), ntohs(listendAddr.sin_port));

        if (sent != len) {
            return -1;
        }
        fprintf(stderr, "Sent udp packet on the first path");

        fprintf(stderr, "sent %zd bytes\n", sent);

        return sent;
}

int flush_packets(struct conn_io *conn_io, int path) {
    uint8_t out[MAX_DATAGRAM_SIZE];
    while (1) {
        // uint64_t t_now = getCurrentTime_mic();
        // conn_io->can_send += rate * (t_now - conn_io->t_last) /
        //                      8000000;  //(bits/8)/s * s = bytes
        // // fprintf(stderr, "%ld us time went, %ld bytes can send\n",
        // //         t_now - conn_io->t_last, conn_io->can_send);
        // conn_io->t_last = t_now;
        // if (conn_io->can_send < 1350) {
        //     fprintf(stderr, "can_send < 1350\n");
        //     conn_io->pace_timer.repeat = 0.001;
        //     ev_timer_again(loop, &conn_io->pace_timer);
        //     break;
        // }
        uint64_t quiche_conn_send_begin_time = getCurrentTime_mic();
        ssize_t written = quiche_conn_send(conn_io->conn, out, sizeof(out), path);
        uint64_t quiche_conn_send_end_time = getCurrentTime_mic();
        fprintf(stderr, "First path: quiche_conn_send time: %" PRIu64 "\n",
                quiche_conn_send_end_time - quiche_conn_send_begin_time);

        fprintf(stderr, "written %ld\n", written);
        if (written == QUICHE_ERR_DONE) {
            fprintf(stderr, "done writing\n");
            // conn_io->done_writing = true;  // app_limited
            break;
        }

        if (written < 0) {
            fprintf(stderr, "failed to create packet: %zd\n", written);
            return -1;
        }
        
        int sock = path == 0 ? conn_io->sock : conn_io->sock2;
        int r = send_packet(sock, out, written);
        if (r < 0) {
            perror("failed to send");
            break;
        }
    }
    return 0;
}

static void debug_log(const char *line, void *argp) {
    fprintf(stderr, "%s\n", line);
}

static void flush_egress(struct ev_loop *loop, struct conn_io *conn_io, int path) {
    fprintf(stderr, "--flush egress-------\n");
    flush_packets(conn_io, path);

    double t = quiche_conn_timeout_as_nanos(conn_io->conn) / 1e9f;
    fprintf(stderr, "ts: %f\n", t);
    conn_io->timer.repeat = t;
    ev_timer_again(loop, &conn_io->timer);

    if (quiche_conn_is_established(conn_io->conn) &&
        !conn_io->first_udp_packet) {
        // connection建立起来之后，启动handshake of second path.
        fprintf(stderr, "Send first packet of second path\n");
        uint8_t out[MAX_DATAGRAM_SIZE] = "Second";
        int sent = send_packet(conn_io->sock2, out, sizeof("Second"));
        if (sent < 0) {
            perror("failed to send first udp packet.");
            return;
        }
        fprintf(stderr, "sent %d bytes: Second\n", sent);
        conn_io->first_udp_packet = true;
    }

    fprintf(stderr, "conn establish %d\n",
            quiche_conn_is_established(conn_io->conn));
}

// static void flush_egress_pace(EV_P_ ev_timer *pace_timer, int revents) {
//     struct conn_io *conn_io = pace_timer->data;
//     fprintf(stderr, "begin flush_egress_pace\n");
//     flush_egress(loop, conn_io, false);
// }

static void recv_cb(EV_P_ ev_io *w, int revents) {
    uint64_t recv_cb_start_time = getCurrentTime_mic();
    struct conn_io *conn_io = w->data;
    static uint8_t buf[MAX_BLOCK_SIZE];
    static uint8_t buf_with_time[MAX_BLOCK_SIZE];
    static uint8_t read_time[TIME_SIZE];
    fprintf(stderr, "-----------------recv_cb------------------\n");
    int recv_count_once = 0;
    while (1) {
        ssize_t read =
            recv(conn_io->sock, buf_with_time, sizeof(buf_with_time), 0);

        if (read < 0) {
            if ((errno == EWOULDBLOCK) || (errno == EAGAIN)) {
                fprintf(stderr, "recv would block\n");
                break;
            }

            perror("failed to read");
            return;
        }

        /**************************************************/
        for (int i = 0; i < TIME_SIZE; i++) {
            read_time[i] = buf_with_time[i];
        }
        uint64_t t1 = *(uint64_t *)read_time;
        uint64_t t2 = getCurrentTime_mic();
        one_way_delay = t2 - t1;
        fprintf(stderr, "first_path t1: %" PRIu64 "\n", t1);
        fprintf(stderr, "first_path t2: %" PRIu64 "\n", t2);
        fprintf(stderr,
                "first_path: server to client -> UDP trans time: %" PRIu64 "\n",
                t2 - t1);
        for (int i = TIME_SIZE, j = 0; j < read; i++, j++) {
            buf[j] = buf_with_time[i];
        }

        /**************************************************/
        read -= TIME_SIZE;

        uint64_t quiche_conn_recv_begin_time = getCurrentTime_mic();
        ssize_t done = quiche_conn_recv(conn_io->conn, buf, read, 0);
        uint64_t quiche_conn_recv_end_time = getCurrentTime_mic();
        fprintf(stderr, "First path: quiche_conn_recv time: %" PRIu64 "\n",
                quiche_conn_recv_end_time - quiche_conn_recv_begin_time);

        if (done == QUICHE_ERR_DONE) {
            fprintf(stderr, "done reading\n");
            break;
        }

        if (done < 0) {
            fprintf(stderr, "failed to process packet\n");
            return;
        }

        fprintf(stderr, "Client recv a packet on first path");

        fprintf(stderr, "recv %zd bytes\n", done);

        recv_count_once += 1;
    }
    fprintf(stderr, "recv_count_once:%d\n", recv_count_once);
    uint64_t recv_cb_while_time = getCurrentTime_mic();
    fprintf(stderr, "First path: after while time: %" PRIu64 "\n",
            recv_cb_while_time - recv_cb_start_time);

    if (quiche_conn_is_established(conn_io->conn)) {
        // fprintf(stderr, "----------connection established!-----------\n");
        uint64_t s = 0;

        quiche_stream_iter *readable = quiche_conn_readable(conn_io->conn);

        while (quiche_stream_iter_next(readable, &s)) {
            fprintf(stderr, "stream %" PRIu64 " is readable\n", s);

            bool fin = false;

            uint64_t quiche_conn_stream_recv_begin_time = getCurrentTime_mic();
            ssize_t recv_len = quiche_conn_stream_recv(conn_io->conn, s, buf,
                                                       sizeof(buf), &fin);
            uint64_t quiche_conn_stream_recv_end_time = getCurrentTime_mic();
            fprintf(stderr,
                    "First path: quiche_conn_stream_recv time: %" PRIu64 "\n",
                    quiche_conn_stream_recv_end_time -
                        quiche_conn_stream_recv_begin_time);

            if (recv_len < 0) {
                fprintf(stderr, "Stream recv wrong: %zd\n", recv_len);
                break;
                // continue;
            } else {
                fprintf(stderr, "Stream recv %zd bytes\n", recv_len);
            }

            if (fin) {
                uint64_t quiche_conn_get_bct_begin_time = getCurrentTime_mic();
                int64_t temp_time = quiche_conn_get_bct(conn_io->conn, s);
                uint64_t quiche_conn_get_bct_end_time = getCurrentTime_mic();
                fprintf(stderr,
                        "First path: quiche_conn_get_bct time: %" PRIu64 "\n",
                        quiche_conn_get_bct_end_time -
                            quiche_conn_get_bct_begin_time);

                fprintf(stderr,
                        "Statistics:stream:%" PRIu64
                        " Received completely by path1, "
                        "recv time is %" PRIu64 ", complete time is %" PRId64
                        "\n",
                        s, getCurrentTime_mic(), temp_time);
                fprintf(stderr, "recv_cb ID:%" PRIu64 "\n", s);
                fprintf(stderr, " %" PRId64 "\n", temp_time);
                recv_num += 1;
                fprintf(stderr, "recv_num:%" PRIu64 "\n", recv_num);
                if (temp_time <= 200) {
                    chunk_before_deadline += 1;
                    fprintf(stderr, "chunk_before_deadline:%" PRIu64 "\n",
                            chunk_before_deadline);
                }
                // fflush(stdout);
                // fflush(stdout);
            }

            static const uint8_t echo[] = "echo\n";
            if (quiche_conn_stream_send(conn_io->conn, s, echo, sizeof(echo),
                                        false) < sizeof(echo)) {
                fprintf(stderr, "fail to echo back.\n");
            }

            // ssize_t send_len = quiche_conn_stream_send(conn_io->conn, s, buf,
            // 1, fin); if (send_len == 1)
            // {
            //     fprintf(stderr, "client send 1 bytes\n");
            // }
            // else
            // {
            //     fprintf(stderr, "client send back fails.\n");
            // }
        }

        quiche_stream_iter_free(readable);
    }

    uint64_t recv_cb_if_time = getCurrentTime_mic();
    fprintf(stderr, "First path: after if time: %" PRIu64 "\n",
            recv_cb_if_time - recv_cb_while_time);

    if (quiche_conn_is_closed(conn_io->conn)) {
        fprintf(stderr, "connection closed\n");

        ev_break(EV_A_ EVBREAK_ONE);
        return;
    }

    uint64_t recv_cb_2if_time = getCurrentTime_mic();
    fprintf(stderr, "First path: after if2 time: %" PRIu64 "\n",
            recv_cb_2if_time - recv_cb_if_time);

    flush_egress(loop, conn_io, 0);

    uint64_t recv_cb_end_time = getCurrentTime_mic();
    fprintf(stderr, "First path: flush egress time: %" PRIu64 "\n",
            recv_cb_end_time - recv_cb_2if_time);

    fprintf(stderr, "First path: recv cb all time: %" PRIu64 "\n",
            recv_cb_end_time - recv_cb_start_time);
}

static void recv_cb2(EV_P_ ev_io *w, int revents) {
    struct conn_io *conn_io = w->data;
    static uint8_t buf2[MAX_BLOCK_SIZE];
    static uint8_t buf2_with_time[MAX_BLOCK_SIZE];
    static uint8_t read_time2[TIME_SIZE];
    fprintf(stderr, "-----------------recv_cb2------------------\n");

    int count2 = 0;

    while (1) {
        /*------------------------------------------------*/
        ssize_t read =
            recv(conn_io->sock2, buf2_with_time, sizeof(buf2_with_time), 0);
        /*------------------------------------------------*/

        if (read < 0) {
            if ((errno == EWOULDBLOCK) || (errno == EAGAIN)) {
                fprintf(stderr, "recv would block\n");
                break;
            }

            perror("failed to read");
            return;
        }

        /**************************************************/
        for (int i = 0; i < TIME_SIZE; i++) {
            read_time2[i] = buf2_with_time[i];
        }
        uint64_t t1 = *(uint64_t *)read_time2;
        uint64_t t2 = getCurrentTime_mic();
        one_way_delay = t2 - t1;
        fprintf(stderr, "second_path t1: %" PRIu64 "\n", t1);
        fprintf(stderr, "second_path t2: %" PRIu64 "\n", t2);
        fprintf(stderr,
                "second_path: server to client -> UDP trans time: %" PRIu64
                "\n",
                t2 - t1);
        for (int i = TIME_SIZE, j = 0; j < read; i++, j++) {
            buf2[j] = buf2_with_time[i];
        }
        /**************************************************/
        read -= TIME_SIZE;

        uint64_t quiche_conn_recv_begin_time = getCurrentTime_mic();
        ssize_t done = quiche_conn_recv(conn_io->conn, buf2, read, 1);
        uint64_t quiche_conn_recv_end_time = getCurrentTime_mic();
        fprintf(stderr, "Second path: quiche_conn_recv time: %" PRIu64 "\n",
                quiche_conn_recv_end_time - quiche_conn_recv_begin_time);

        if (done == QUICHE_ERR_DONE) {
            fprintf(stderr, "done reading2\n");
            break;
        }

        if (done < 0) {
            fprintf(stderr, "failed to process packet\n");
            return;
        }

        fprintf(stderr, "Client recv a packet on second path");

        fprintf(stderr, "recv %zd bytes\n", done);

        count2 += 1;
    }
    fprintf(stderr, "recv_count2_once:%d\n", count2);

    if (quiche_conn_is_established(conn_io->conn)) {
        fprintf(stderr, "----------connection established2!-----------\n");
        uint64_t s = 0;

        quiche_stream_iter *readable = quiche_conn_readable(conn_io->conn);

        while (quiche_stream_iter_next(readable, &s)) {
            fprintf(stderr, "stream %" PRIu64 " is readable\n", s);

            bool fin = false;
            ssize_t recv_len = quiche_conn_stream_recv(
                conn_io->conn, s, buf2, sizeof(buf2),  // APP copy finished
                &fin);
            if (recv_len < 0) {
                fprintf(stderr, "Stream recv wrong: %zd\n", recv_len);
                break;
            } else {
                fprintf(stderr, "Stream recv %zd bytes\n", recv_len);
            }

            if (fin) {
                int64_t temp_time = quiche_conn_get_bct(conn_io->conn, s);
                fprintf(stderr,
                        "Statistics:stream:%" PRIu64
                        " Received completely by path2, "
                        "recv time is %" PRIu64 ", complete time is %" PRId64
                        "\n",
                        s, getCurrentTime_mic(), temp_time);
                fprintf(stderr, "recv_cb2 ID:%" PRIu64 "\n", s);
                fprintf(stderr, " %" PRId64 "\n", temp_time);
                recv_num += 1;
                fprintf(stderr, "recv_num:%" PRIu64 "\n", recv_num);
                if (temp_time <= 200) {
                    chunk_before_deadline += 1;
                    fprintf(stderr, "chunk_before_deadline:%" PRIu64 "\n",
                            chunk_before_deadline);
                }
                // fflush(stdout);
                fflush(stdout);
            }

            // ssize_t send_len = quiche_conn_stream_send(conn_io->conn, s,
            // buf2, 1, fin); if (send_len == 1)
            // {
            //     fprintf(stderr, "client send 1 bytes\n");
            // }
            // else
            // {
            //     fprintf(stderr, "client send back fails.\n");
            // }

            // send back an ack in application layer.
            static const uint8_t echo[] = "echo\n";
            if (quiche_conn_stream_send(conn_io->conn, s, echo, sizeof(echo),
                                        false) < sizeof(echo)) {
                fprintf(stderr, "fail to echo back.\n");
            }
        }
        quiche_stream_iter_free(readable);
    }

    if (quiche_conn_is_closed(conn_io->conn)) {
        fprintf(stderr, "connection closed\n");

        ev_break(EV_A_ EVBREAK_ONE);
        return;
    }

    flush_egress(loop, conn_io, 1);
}

static void timeout_cb(EV_P_ ev_timer *w, int revents) {
    struct conn_io *conn_io = w->data;
    quiche_conn_on_timeout(conn_io->conn);

    fprintf(stderr, "-------timeout");

    flush_egress(loop, conn_io, 0);
    flush_egress(loop, conn_io, 1);

    if (quiche_conn_is_closed(conn_io->conn)) {
        fprintf(stderr, "chunk before deadline: %" PRIu64 "\n",
                chunk_before_deadline);
        quiche_stats stats;

        quiche_conn_stats(conn_io->conn, &stats);

        fprintf(stderr,
                "connection closed, recv=%zu sent=%zu lost_init=%zu "
                "lost_subseq=%zu rtt_init=%" PRIu64 "ns rtt_subseq=%" PRIu64
                "ns\n",
                stats.recv, stats.sent, stats.lost_init, stats.lost_subseq,
                stats.rtt_init, stats.rtt_subseq);

        ev_break(EV_A_ EVBREAK_ONE);
        return;
    }
}

int main(int argc, char *argv[]) {
    const char *host = argv[1];
    const char *port = argv[2];
    const char *local1 = argv[3];
    const char *local1_port = argv[4];
    const char *local2 = argv[5];
    const char *local2_port = argv[6];

    const struct addrinfo hints = {.ai_family = PF_UNSPEC,
                                   .ai_socktype = SOCK_DGRAM,
                                   .ai_protocol = IPPROTO_UDP};

    quiche_enable_debug_logging(debug_log, NULL);

    struct addrinfo *peer;
    if (getaddrinfo(host, port, &hints, &peer) != 0) {
        perror("failed to resolve host");
        return -1;
    }

    // socket1 of client
    int sock = socket(peer->ai_family, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("failed to create socket");
        return -1;
    }

    if (fcntl(sock, F_SETFL, O_NONBLOCK) != 0) {
        perror("failed to make socket non-blocking");
        return -1;
    }

    // MP: Bind address to socket1 --------------------------------------------
    if (sizeof(local1) != 1) {
        const struct addrinfo hints_sock = {.ai_family = PF_UNSPEC,
                                            .ai_socktype = SOCK_DGRAM,
                                            .ai_protocol = IPPROTO_UDP};
        struct addrinfo *local_sock;
        if (getaddrinfo(local1, local1_port, &hints_sock, &local_sock) != 0) {
            perror("failed to resolve host");
            return -1;
        }

        if (bind(sock, local_sock->ai_addr, local_sock->ai_addrlen) < 0) {
            perror("failed to bind sock");
            return -1;
        } else {
            freeaddrinfo(local_sock);
            printf("Bind successful\n");
        }
    }
    // MP: Bind address to socket1 END
    // --------------------------------------------

    if (connect(sock, peer->ai_addr, peer->ai_addrlen) < 0) {
        perror("failed to connect socket1");
        return -1;
    }

    // socket2 of client
    /*------------------------------------------------*/
    int sock2 = socket(peer->ai_family, SOCK_DGRAM, 0);
    if (sock2 < 0) {
        perror("failed to create socket2");
        return -1;
    }

    if (fcntl(sock2, F_SETFL, O_NONBLOCK) != 0) {
        perror("failed to make socket2 non-blocking");
        return -1;
    }

    // MP: Bind address to socket2 --------------------------------------------
    if (sizeof(local2) != 1) {
        const struct addrinfo hints_sock2 = {.ai_family = PF_UNSPEC,
                                             .ai_socktype = SOCK_DGRAM,
                                             .ai_protocol = IPPROTO_UDP};
        struct addrinfo *local_sock2;
        if (getaddrinfo(local2, local2_port, &hints_sock2, &local_sock2) != 0) {
            perror("failed to resolve host");
            return -1;
        }
        if (bind(sock2, local_sock2->ai_addr, local_sock2->ai_addrlen) < 0) {
            perror("failed to bind sock2");
            return -1;
        } else {
            freeaddrinfo(local_sock2);
        }
    }
    // MP: Bind address to socket2 END
    // --------------------------------------------

    if (connect(sock2, peer->ai_addr, peer->ai_addrlen) < 0) {
        perror("failed to connect socket2");
        return -1;
    }
    /*------------------------------------------------*/
    // fprintf(stderr, "%d\n", sock==sock2);

    quiche_config *config = quiche_config_new(0xbabababa);
    if (config == NULL) {
        fprintf(stderr, "failed to create config\n");
        return -1;
    }

    quiche_config_set_application_protos(
        config, (uint8_t *)"\x05hq-25\x05hq-24\x05hq-23\x08http/0.9", 15);

    quiche_config_set_max_idle_timeout(config, 15000);
    quiche_config_set_max_packet_size(config, MAX_DATAGRAM_SIZE);
    quiche_config_set_initial_max_data(config, 10000000000);
    quiche_config_set_initial_max_stream_data_bidi_local(config, 1000000000);
    quiche_config_set_initial_max_stream_data_bidi_remote(config, 1000000000);
    quiche_config_set_initial_max_stream_data_uni(config, 1000000000);
    quiche_config_set_initial_max_streams_bidi(config, 1000000000);
    quiche_config_set_initial_max_streams_uni(config, 1000000000);
    quiche_config_set_disable_active_migration(config, true);
    quiche_config_verify_peer(config, false);
    quiche_config_set_cc_algorithm(config, QUICHE_CC_RENO);

    if (getenv("SSLKEYLOGFILE")) {
        quiche_config_log_keys(config);
    }

    uint8_t scid[LOCAL_CONN_ID_LEN];
    int rng = open("/dev/urandom", O_RDONLY);
    if (rng < 0) {
        perror("failed to open /dev/urandom");
        return -1;
    }

    ssize_t rand_len = read(rng, &scid, sizeof(scid));
    if (rand_len < 0) {
        perror("failed to create connection ID");
        return -1;
    }

    // for(int i=0;i<LOCAL_CONN_ID_LEN; i++)
    // {
    //     fprintf(stderr, "%x", scid[i]);
    // }
    // fprintf(stderr, "\nclient scid\n");

    quiche_conn *conn =
        quiche_connect(host, (const uint8_t *)scid, sizeof(scid), config);

    if (conn == NULL) {
        fprintf(stderr, "failed to create connection\n");
        return -1;
    }

    struct conn_io *conn_io = malloc(sizeof(*conn_io));
    if (conn_io == NULL) {
        fprintf(stderr, "failed to allocate connection IO\n");
        return -1;
    }

    // int cfgs_len;
    // struct dtp_config *cfgs = NULL;
    // cfgs = parse_dtp_config(dtp_cfg_fname, &cfgs_len);
    // if (cfgs_len <= 0) {
    //     fprintf(stderr, "No valid DTP configuration\n");
    //     return -1;
    // }

    conn_io->sock = sock;
    conn_io->sock2 = sock2;
    conn_io->conn = conn;
    conn_io->first_udp_packet = false;
    // conn_io->t_last = getCurrentTime_mic();
    // conn_io->can_send = 1350;
    // conn_io->done_writing = false;

    // ************************* MP: Set PCID value
    // ************************************** set server_pcid, client_pcid
    memset(server_pcid, 0x22, sizeof(server_pcid));
    memset(client_pcid, 0x33, sizeof(client_pcid));
    // ***********************************************************************************

    // ************************* MP: Create Second Path(Change Connection
    // Status) *********************************
    initiate_second_path(conn_io->conn, client_pcid, LOCAL_CONN_ID_LEN,
                         server_pcid, LOCAL_CONN_ID_LEN);
    // **************************************************************************************************************

    struct ev_loop *loop = ev_default_loop(0);

    ev_io watcher;
    ev_io_init(&watcher, recv_cb, conn_io->sock, EV_READ);
    watcher.data = conn_io;
    ev_io_start(loop, &watcher);
    conn_io->watcher = &watcher;

    /*------------------------------------------------*/
    ev_io watcher2;
    ev_io_init(&watcher2, recv_cb2, conn_io->sock2, EV_READ);
    watcher2.data = conn_io;
    ev_io_start(loop, &watcher2);
    conn_io->watcher2 = &watcher2;
    /*------------------------------------------------*/

    ev_init(&conn_io->timer, timeout_cb);
    conn_io->timer.data = conn_io;

    // ev_init(&conn_io->pace_timer, flush_egress_pace);
    // conn_io->pace_timer.data = conn_io;

    flush_egress(loop, conn_io, 0);
    fprintf(stderr, "sock start\n");

    ev_loop(loop, 0);

    freeaddrinfo(peer);

    quiche_conn_free(conn);

    quiche_config_free(config);

    // To be confirmed. Whether to free conn_io
    free(conn_io);

    return 0;
}

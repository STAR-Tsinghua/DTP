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

// MP: One Connection with Two Paths

#include <arpa/inet.h>
#include <dtp_config.h>
#include <errno.h>
#include <ev.h>
#include <fcntl.h>
#include <inttypes.h>
#include <netdb.h>
#include <quiche.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "helper.h"
#include "uthash.h"

#define LOCAL_CONN_ID_LEN 16
#define MAX_DATAGRAM_SIZE 1350   // UDP
#define MAX_BLOCK_SIZE 10000000  // QUIC
#define TIME_SIZE 8
// #define SEND_INTERVAL 0.03
/* #define MAX_SEND_TIMES 600 */
// #define MAX_FLUSH_TIMES 3

int MAX_SEND_TIMES;

#define MAX_TOKEN_LEN                                        \
    sizeof("quiche") - 1 + sizeof(struct sockaddr_storage) + \
        QUICHE_MAX_CONN_ID_LEN

struct connections {
    int sock;

    ev_io *watcher;

    struct conn_io *h;

    int configs_len;
    dtp_config *configs;
};

struct conn_io {
    ev_timer timer;
    ev_timer sender;

    int sock;

    uint8_t cid[LOCAL_CONN_ID_LEN];

    quiche_conn *conn;

    int send_round;

    // ev_timer pace_timer;
    // uint64_t t_last;
    // ssize_t can_send;
    // bool done_writing;

    // pacing
    uint64_t first_t_last;
    uint64_t second_t_last;
    ssize_t first_can_send;
    ssize_t second_can_send;
    bool first_done_writing;
    bool second_done_writing;
    ev_timer first_pace_timer;
    ev_timer second_pace_timer;

    // MP: Initial Path address
    struct sockaddr_storage initial_peer_addr;
    socklen_t initial_peer_addr_len;
    // *****************************

    // MP: Another New Path address
    struct sockaddr_storage second_peer_addr;
    socklen_t second_peer_addr_len;
    // *****************************

    UT_hash_handle hh;

    int configs_len;
    dtp_config *configs;
};

static quiche_config *config = NULL;

static struct connections *conns = NULL;

static uint64_t iter = 0;

// MP: PCID(default for second path)
uint8_t server_pcid[LOCAL_CONN_ID_LEN];
uint8_t client_pcid[LOCAL_CONN_ID_LEN];
// *********************************************

struct sockaddr_storage temp_addr;
socklen_t temp_addr_len;

// MP: Sign whether second path address is known
bool second_udp_path_get_finished = false;
bool MP_conn_finished = false;
// *********************************************

struct conn_io *conn_io_outside = NULL;

static void timeout_cb(EV_P_ ev_timer *w, int revents);
static void debug_log(const char *line, void *argp) {
    fprintf(stderr, "%s\n", line);
}

static void flush_egress(struct ev_loop *loop, struct conn_io *conn_io) {
    fprintf(stderr, "------flush egress------\n");

    uint64_t flush_egress_begin_time = getCurrentTime_mic();
    fprintf(stderr, "flush start time: %" PRIu64 "\n", flush_egress_begin_time);

    static uint8_t out[MAX_DATAGRAM_SIZE];
    static uint8_t out_with_time[MAX_DATAGRAM_SIZE + TIME_SIZE];

    // pacing
    // uint64_t first_rate = 480 * 1024 * 1024;  // bits/s   6MB/s
    // uint64_t second_rate = 480 * 1024 * 1024;  // bits/s   1MB/s
    double first_rate =
        quiche_conn_get_pacing_rate(conn_io->conn, 0);  // bits/s
    fprintf(stderr, "first_rate: %lf\n", first_rate / (1024.0 * 1024.0 * 8.0));
    double second_rate =
        quiche_conn_get_pacing_rate(conn_io->conn, 1);  // bits/s
    fprintf(stderr, "second_rate: %lf\n",
            second_rate / (1024.0 * 1024.0 * 8.0));

    if (conn_io->first_done_writing) {
        conn_io->first_can_send = 1350;
        conn_io->first_t_last = getCurrentTime_mic();
        conn_io->first_done_writing = false;
        conn_io->first_pace_timer.repeat = 99999.0;
        ev_timer_again(loop, &conn_io->first_pace_timer);
    }
    if (conn_io->second_done_writing) {
        conn_io->second_can_send = 1350;
        conn_io->second_t_last = getCurrentTime_mic();
        conn_io->second_done_writing = false;
        conn_io->second_pace_timer.repeat = 99999.0;
        ev_timer_again(loop, &conn_io->second_pace_timer);
    }

    int flush_times_first = 0;
    int flush_times_second = 0;
    while (1) {
        uint64_t t_now = getCurrentTime_mic();
        uint64_t can_send_increase =
            first_rate * (t_now - conn_io->first_t_last) / 8000000.0;
        conn_io->first_can_send += can_send_increase;  //(bits/8)/s * s = bytes

        conn_io->first_t_last = t_now;
        if (conn_io->first_can_send < 1350) {
            fprintf(stderr, "first path can_send < 1350\n");
            conn_io->first_pace_timer.repeat = 0.001;
            ev_timer_again(loop, &conn_io->first_pace_timer);
            if (!MP_conn_finished) {
                break;
            }
        } else {
            uint64_t quiche_conn_send_begin_time = getCurrentTime_mic();
            ssize_t written_f =
                quiche_conn_send(conn_io->conn, out, sizeof(out), 0);
            uint64_t quiche_conn_send_end_time = getCurrentTime_mic();
            fprintf(stderr, "quiche_conn_send time: %" PRIu64 "\n",
                    quiche_conn_send_end_time - quiche_conn_send_begin_time);

            if (written_f > 0) {
                uint64_t t = getCurrentTime_mic();
                uint8_t *out_time;
                for (int i = TIME_SIZE, j = 0; j < MAX_DATAGRAM_SIZE;
                     i++, j++) {
                    out_with_time[i] = out[j];
                }
                out_time = (uint8_t *)&t;
                for (int i = 0; i < TIME_SIZE; i++) {
                    out_with_time[i] = out_time[i];
                }
                /**********************************************************/
                written_f += TIME_SIZE;

                fprintf(stderr, "before send conn_io==NULL %d\n",
                        conn_io == NULL);
                fprintf(stderr, "conn_io->sock %d\n", conn_io->sock);

                uint64_t first_before_send = getCurrentTime_mic();
                fprintf(stderr, "first_before_send: %" PRIu64 "\n",
                        first_before_send);
                ssize_t sent =
                    sendto(conn_io->sock, out_with_time, written_f, 0,
                           (struct sockaddr *)&conn_io->initial_peer_addr,
                           conn_io->initial_peer_addr_len);
                uint64_t first_after_send = getCurrentTime_mic();
                fprintf(stderr, "first_after_send: %" PRIu64 "\n",
                        first_after_send);
                // fprintf(stderr, "sent: %zd\n", sent);
                // fprintf(stderr, "written: %zd\n", written_f);

                if (sent != written_f) {
                    perror("failed to send");
                    return;
                }
                // ***********************************************************************

                fprintf(stderr, "sent on first path %zd bytes\n", sent);
                flush_times_first++;
                conn_io->first_can_send -= sent;
            }

            if (written_f < -1) {
                fprintf(stderr, "failed to create packet on first path: %zd\n",
                        written_f);
                return;
            }

            if (written_f == QUICHE_ERR_DONE) {
                // conn_io->can_send = 1350;  // app_limited
                fprintf(stderr, "first path done writing\n");
                conn_io->first_done_writing = true;  // app_limited
                // break;
                if (!MP_conn_finished) {
                    break;
                }
            }
        }

        // If 2 paths, following codes are needed.
        if (MP_conn_finished) {
            uint64_t t_now_second = getCurrentTime_mic();
            uint64_t can_send_increase_second =
                second_rate * (t_now_second - conn_io->second_t_last) /
                8000000.0;
            conn_io->second_can_send +=
                can_send_increase_second;  //(bits/8)/s * s = bytes

            conn_io->second_t_last = t_now_second;
            if (conn_io->second_can_send < 1350) {
                fprintf(stderr, "second path can_send < 1350\n");
                conn_io->second_pace_timer.repeat = 0.001;
                ev_timer_again(loop, &conn_io->second_pace_timer);
                if (conn_io->first_can_send < 1350 ||
                    conn_io->first_done_writing) {
                    break;
                }
            } else {
                uint64_t quiche_conn_send_begin_time = getCurrentTime_mic();
                ssize_t written_s =
                    quiche_conn_send(conn_io->conn, out, sizeof(out), 1);
                uint64_t quiche_conn_send_end_time = getCurrentTime_mic();
                fprintf(
                    stderr,
                    "Statistic time:quiche_conn_send time: %" PRIu64 "\n",
                    quiche_conn_send_end_time - quiche_conn_send_begin_time);

                // Done writing
                if (written_s == QUICHE_ERR_DONE) {
                    fprintf(stderr, "second path done writing\n");
                    conn_io->second_done_writing = true;  // app_limited
                    if (conn_io->first_done_writing ||
                        conn_io->first_can_send < 1350) {
                        break;
                    }
                }

                if (written_s > 0) {
                    uint64_t t = getCurrentTime_mic();
                    uint8_t *out_time;
                    for (int i = TIME_SIZE, j = 0; j < MAX_DATAGRAM_SIZE;
                         i++, j++) {
                        out_with_time[i] = out[j];
                    }
                    out_time = (uint8_t *)&t;
                    for (int i = 0; i < TIME_SIZE; i++) {
                        out_with_time[i] = out_time[i];
                    }
                    /**********************************************************/
                    written_s += TIME_SIZE;

                    fprintf(stderr, "before send conn_io==NULL %d\n",
                            conn_io == NULL);
                    fprintf(stderr, "conn_io->sock %d\n", conn_io->sock);

                    char *tmp_ip = inet_ntoa(
                        ((struct sockaddr_in *)(struct sockaddr *)&conn_io
                             ->second_peer_addr)
                            ->sin_addr);
                    int tmp_port =
                        ntohs(((struct sockaddr_in *)(struct sockaddr *)&conn_io
                                   ->second_peer_addr)
                                  ->sin_port);

                    fprintf(stderr, "ip port addr_len: %s, %d, %d\n", tmp_ip,
                            tmp_port, conn_io->second_peer_addr_len);

                    uint64_t second_before_send = getCurrentTime_mic();
                    fprintf(stderr, "second_before_send: %" PRIu64 "\n",
                            second_before_send);
                    ssize_t sent =
                        sendto(conn_io->sock, out_with_time, written_s, 0,
                               (struct sockaddr *)&conn_io->second_peer_addr,
                               conn_io->second_peer_addr_len);
                    uint64_t second_after_send = getCurrentTime_mic();
                    fprintf(stderr, "second_after_send: %" PRIu64 "\n",
                            second_after_send);

                    fprintf(stderr, "sent: %zd\n", sent);
                    fprintf(stderr, "written: %zd\n", written_s);

                    if (sent != written_s) {
                        perror("failed to send");
                        return;
                    }
                    // ***********************************************************************

                    fprintf(stderr, "sent on second path %zd bytes\n", sent);
                    conn_io->second_can_send -= sent;
                    flush_times_second++;
                }

                if (written_s < -1) {
                    fprintf(stderr,
                            "failed to create packet on second path: %zd\n",
                            written_s);
                    return;
                }
            }
        }

        // if (flush_times_first > MAX_FLUSH_TIMES || flush_times_second >
        // MAX_FLUSH_TIMES) {
        //     ev_feed_event(loop, conns->watcher, EV_READ);
        //     fprintf(stderr, "break the sending.\n");
        //     break;
        // }
    }
    fprintf(stderr, "first_flush_times: %d, second_flush_times: %d\n",
            flush_times_first, flush_times_second);

    uint64_t flush_egress_end_time = getCurrentTime_mic();
    fprintf(stderr, "flush time: %" PRIu64 "\n",
            flush_egress_end_time - flush_egress_begin_time);
    fprintf(stderr, "now time: %" PRIu64 "\n", flush_egress_end_time);

    double t = quiche_conn_timeout_as_nanos(conn_io->conn) / 1e9f;
    fprintf(stderr, "ts: %f", t);
    conn_io->timer.repeat = t;
    ev_timer_again(loop, &conn_io->timer);

    quiche_stats stats;
    quiche_conn_stats(conn_io->conn, &stats);
    fprintf(
        stderr,
        "-----recv=%zu sent=%zu lost_init=%zu lost_subseq=%zu rtt_init=%" PRIu64
        "ns rtt_subseq=%" PRIu64 "ns-----\n",
        stats.recv, stats.sent, stats.lost_init, stats.lost_subseq,
        stats.rtt_init, stats.rtt_subseq);
}

static void flush_egress_first_pace(EV_P_ ev_timer *first_pace_timer,
                                    int revents) {
    struct conn_io *conn_io = first_pace_timer->data;
    // fprintf(stderr, "begin flush_egress_pace\n");
    flush_egress(loop, conn_io);
}
static void flush_egress_second_pace(EV_P_ ev_timer *second_pace_timer,
                                     int revents) {
    struct conn_io *conn_io = second_pace_timer->data;
    // fprintf(stderr, "begin flush_egress_pace\n");
    flush_egress(loop, conn_io);
}

static void mint_token(const uint8_t *dcid, size_t dcid_len,
                       struct sockaddr_storage *addr, socklen_t addr_len,
                       uint8_t *token, size_t *token_len) {
    memcpy(token, "quiche", sizeof("quiche") - 1);
    memcpy(token + sizeof("quiche") - 1, addr, addr_len);
    memcpy(token + sizeof("quiche") - 1 + addr_len, dcid, dcid_len);

    *token_len = sizeof("quiche") - 1 + addr_len + dcid_len;
}

static bool validate_token(const uint8_t *token, size_t token_len,
                           struct sockaddr_storage *addr, socklen_t addr_len,
                           uint8_t *odcid, size_t *odcid_len) {
    if ((token_len < sizeof("quiche") - 1) ||
        memcmp(token, "quiche", sizeof("quiche") - 1)) {
        return false;
    }

    token += sizeof("quiche") - 1;
    token_len -= sizeof("quiche") - 1;

    if ((token_len < addr_len) || memcmp(token, addr, addr_len)) {
        return false;
    }

    token += addr_len;
    token_len -= addr_len;

    if (*odcid_len < token_len) {
        return false;
    }

    memcpy(odcid, token, token_len);
    *odcid_len = token_len;

    return true;
}

static void sender_cb(EV_P_ ev_timer *w, int revents) {
    struct conn_io *conn_io = w->data;

    fprintf(stderr, "------------sender_cb exe----------------");
    uint64_t sender_cb_exe_time = getCurrentTime_mic();
    fprintf(stderr, "sender_cb_exe_time: %" PRIu64 "\n", sender_cb_exe_time);

    if (quiche_conn_is_established(conn_io->conn)) {
        fprintf(stderr, "-----sender_cb  quiche_conn_is_established-----\n");
        int deadline = 0;
        int priority = 0;
        int block_size = 0;
        static uint8_t buf[MAX_BLOCK_SIZE];

        deadline = conn_io->configs[iter].deadline;
        priority = conn_io->configs[iter].priority;
        block_size = conn_io->configs[iter].block_size;
        iter++;
        if (block_size > MAX_BLOCK_SIZE) block_size = MAX_BLOCK_SIZE;

        // fprintf(stderr, "conn:%d stream_id:%d block_size:%d ddl:%d
        // priority:%d\n",
        //         conn_io->conn != NULL, 4 * (conn_io->send_round + 1) + 1,
        //         block_size, deadline, priority);

        ssize_t stream_send_written = quiche_conn_stream_send_full(
            conn_io->conn, 4 * (conn_io->send_round + 1) + 1, buf, block_size,
            true, deadline, priority);
        if (stream_send_written < 0) {
            fprintf(stderr, "failed to send data round %d\n",
                    conn_io->send_round);
        } else {
            // Decrease the frequency of printf.
            // if (conn_io->send_round % (10 * conn_io->configs_len) == 0)
            // {
            //     fprintf(stderr, "send round %d\n", conn_io->send_round);
            // }
            // fprintf(stderr, "stream_send_written %zd\n",
            // stream_send_written);
            fprintf(stderr, "send round %d\n", conn_io->send_round);
        }

        conn_io->send_round++;
        if (conn_io->send_round >= MAX_SEND_TIMES) {
            ev_timer_stop(loop, &conn_io->sender);
        }

        fprintf(stderr, "-------sender_cb");

        flush_egress(loop, conn_io);
    }
}

static struct conn_io *create_conn(uint8_t *odcid, size_t odcid_len) {
    struct conn_io *conn_io = malloc(sizeof(*conn_io));
    if (conn_io == NULL) {
        fprintf(stderr, "failed to allocate connection IO\n");
        return NULL;
    }

    int rng = open("/dev/urandom", O_RDONLY);
    if (rng < 0) {
        perror("failed to open /dev/urandom");
        return NULL;
    }

    ssize_t rand_len = read(rng, conn_io->cid, LOCAL_CONN_ID_LEN);
    if (rand_len < 0) {
        perror("failed to create connection ID");
        return NULL;
    }

    quiche_conn *conn = quiche_accept(conn_io->cid, LOCAL_CONN_ID_LEN, odcid,
                                      odcid_len, config);
    if (conn == NULL) {
        fprintf(stderr, "failed to create connection\n");
        return NULL;
    }

    conn_io->sock = conns->sock;
    conn_io->conn = conn;
    conn_io->send_round = 0;
    conn_io->first_t_last = getCurrentTime_mic();
    conn_io->second_t_last = getCurrentTime_mic();
    conn_io->first_can_send = 1350;
    conn_io->second_can_send = 1350;
    conn_io->first_done_writing = false;
    conn_io->second_done_writing = false;
    conn_io->configs_len = conns->configs_len;
    conn_io->configs = conns->configs;

    ev_init(&conn_io->timer, timeout_cb);
    conn_io->timer.data = conn_io;

    ev_init(&conn_io->first_pace_timer, flush_egress_first_pace);
    conn_io->first_pace_timer.data = conn_io;
    ev_init(&conn_io->second_pace_timer, flush_egress_second_pace);
    conn_io->second_pace_timer.data = conn_io;

    HASH_ADD(hh, conns->h, cid, LOCAL_CONN_ID_LEN, conn_io);

    fprintf(stderr, "new connection\n");

    return conn_io;
}

// Only one recv callback to receive packets from two paths.
static void recv_cb(EV_P_ ev_io *w, int revents) {
    struct conn_io *tmp, *conn_io = NULL;
    static uint8_t buf_with_time[MAX_BLOCK_SIZE];
    static uint8_t read_time[TIME_SIZE];
    static uint8_t buf[MAX_BLOCK_SIZE];
    static uint8_t out_process[MAX_DATAGRAM_SIZE];
    static uint8_t out_with_time_process[MAX_DATAGRAM_SIZE + TIME_SIZE];
    static uint8_t first_pkt_of_second_path[] = "Second";

    while (1) {
        uint64_t recvcb_begin_time = getCurrentTime_mic();
        // fprintf(stderr, "recvcb_begin_time: %" PRIu64 "\n",
        // recvcb_begin_time);

        struct sockaddr_storage peer_addr;
        socklen_t peer_addr_len = sizeof(peer_addr);
        memset(&peer_addr, 0, peer_addr_len);

        ssize_t read = recvfrom(conns->sock, buf_with_time,
                                sizeof(buf_with_time), 0,  // UDP?
                                (struct sockaddr *)&peer_addr, &peer_addr_len);

        char *client_ip = inet_ntoa(
            ((struct sockaddr_in *)(struct sockaddr *)&peer_addr)->sin_addr);
        int client_port = ntohs(
            ((struct sockaddr_in *)(struct sockaddr *)&peer_addr)->sin_port);

        fprintf(stderr, "recv from: %s, %d , %ld bytes\n", client_ip,
                client_port, read);

        if (read < 0) {
            if ((errno == EWOULDBLOCK) || (errno == EAGAIN)) {
                fprintf(stderr, "recv would block\n");
                break;
            }

            perror("failed to read");
            return;
        }

        /**************************************************/
        // get client send time
        uint64_t t2 = getCurrentTime_mic();
        for (int i = 0; i < TIME_SIZE; i++) {
            read_time[i] = buf_with_time[i];
        }
        uint64_t t1 = *(uint64_t *)read_time;
        fprintf(stderr, "t1: %" PRIu64 "\n", t1);
        fprintf(stderr, "t2: %" PRIu64 "\n", t2);
        fprintf(stderr, "debug!!!");
        fprintf(stderr, "client to server -> UDP trans time: %" PRIu64 "\n",
                t2 - t1);

        // get one way delay
        for (int i = TIME_SIZE, j = 0; j < TIME_SIZE; i++, j++) {
            read_time[j] = buf_with_time[i];
        }
        uint64_t owd = *(uint64_t *)read_time;
        fprintf(stderr, "OWD time: %" PRIu64 "\n", owd);

        for (int i = 2 * TIME_SIZE, j = 0; i < read; i++, j++) {
            buf[j] = buf_with_time[i];
        }
        /**************************************************/
        read -= 2 * TIME_SIZE;

        if (memcmp(buf, first_pkt_of_second_path,
                   sizeof(first_pkt_of_second_path)) == 0) {
            memcpy(&temp_addr, &peer_addr, peer_addr_len);
            temp_addr_len = peer_addr_len;
            fprintf(stderr,
                    "**********************recv udp pkt from second "
                    "address**********************\n");
            second_udp_path_get_finished = true;

            if (conn_io_outside != NULL && second_udp_path_get_finished &&
                !MP_conn_finished) {
                memcpy(&conn_io_outside->second_peer_addr, &temp_addr,
                       temp_addr_len);
                conn_io_outside->second_peer_addr_len = temp_addr_len;
                fprintf(stderr,
                        "**************second address get!***************\n");
                quiche_conn_second_path_is_established(conn_io_outside->conn);
                MP_conn_finished = true;

                // after two paths built, blocks are sent.
                // start sending immediately and repeat every 50ms.
                ev_timer_init(&conn_io_outside->sender, sender_cb, 0.1, 0.033);
                ev_timer_start(loop, &conn_io_outside->sender);
                conn_io_outside->sender.data = conn_io_outside;
            }

            continue;
        }

        uint8_t type;
        uint32_t version;

        uint8_t scid[QUICHE_MAX_CONN_ID_LEN];
        size_t scid_len = sizeof(scid);

        uint8_t dcid[QUICHE_MAX_CONN_ID_LEN];
        size_t dcid_len = sizeof(dcid);

        uint8_t odcid[QUICHE_MAX_CONN_ID_LEN];
        size_t odcid_len = sizeof(odcid);

        uint8_t token[MAX_TOKEN_LEN];
        size_t token_len = sizeof(token);

        int rc = quiche_header_info(buf, read, LOCAL_CONN_ID_LEN, &version,
                                    &type, scid, &scid_len, dcid, &dcid_len,
                                    token, &token_len);

        fprintf(stderr, "token_len : %ld\n", token_len);
        if (rc < 0) {
            fprintf(stderr, "failed to parse header: %d\n", rc);
            return;
        }

        // ****************** MP: Mapping PDCID(peer) to MSCID(host) and Find
        // conn_io *********************
        uint8_t mscid[QUICHE_MAX_CONN_ID_LEN];
        size_t mscid_len = sizeof(mscid);

        int found = 0;
        HASH_ITER(hh, conns->h, conn_io, tmp) {
            found = mp_mapping_pcid_to_mcid(conn_io->conn, dcid, dcid_len,
                                            mscid, &mscid_len);
            if (found) {  // Mapping is success
                fprintf(stderr, "FOUND\n");
                break;
            }
        }

        // conn_io = NULL;

        // fprintf(stderr, "version:%d\n", version);

        fprintf(stderr, "found:%d\n", found);
        fprintf(stderr, "conn_io==NULL %d\n", conn_io == NULL);

        if (found) {
            HASH_FIND(hh, conns->h, mscid, mscid_len, conn_io);
            fprintf(stderr, "conn_io==NULL %d\n", conn_io == NULL);
        }
        // *****************************************************************************************************************

        if (conn_io == NULL) {
            fprintf(stderr, "version: %d\n", version);
            if (!quiche_version_is_supported(version)) {
                fprintf(stderr, "version negotiation\n");
                ssize_t written =
                    quiche_negotiate_version(scid, scid_len, dcid, dcid_len,
                                             out_process, sizeof(out_process));

                fprintf(stderr, "written:%ld\n", written);
                if (written < 0) {
                    fprintf(stderr, "failed to create vneg packet: %zd\n",
                            written);
                    return;
                }

                /**********************************************************/
                uint64_t t = getCurrentTime_mic();
                uint8_t *out_time;
                for (int i = TIME_SIZE, j = 0; j < MAX_DATAGRAM_SIZE;
                     i++, j++) {
                    out_with_time_process[i] = out_process[j];
                }
                out_time = (uint8_t *)&t;
                for (int i = 0; i < TIME_SIZE; i++) {
                    out_with_time_process[i] = out_time[i];
                }
                /**********************************************************/
                written += TIME_SIZE;

                ssize_t sent =
                    sendto(conns->sock, out_with_time_process, written, 0,
                           (struct sockaddr *)&peer_addr, peer_addr_len);

                if (sent != written) {
                    perror("failed to send");
                    return;
                }

                fprintf(stderr, "sent %zd bytes\n", sent);
                return;
            }

            if (token_len == 0) {
                fprintf(stderr, "stateless retry\n");

                mint_token(dcid, dcid_len, &peer_addr, peer_addr_len, token,
                           &token_len);

                ssize_t written = quiche_retry(
                    scid, scid_len, dcid, dcid_len, dcid, dcid_len, token,
                    token_len, out_process, sizeof(out_process));

                if (written < 0) {
                    fprintf(stderr, "failed to create retry packet: %zd\n",
                            written);
                    return;
                }

                /**********************************************************/
                uint64_t t = getCurrentTime_mic();
                uint8_t *out_time;
                for (int i = TIME_SIZE, j = 0; j < MAX_DATAGRAM_SIZE;
                     i++, j++) {
                    out_with_time_process[i] = out_process[j];
                }
                out_time = (uint8_t *)&t;
                for (int i = 0; i < TIME_SIZE; i++) {
                    out_with_time_process[i] = out_time[i];
                }
                /**********************************************************/
                written += TIME_SIZE;

                ssize_t sent =
                    sendto(conns->sock, out_with_time_process, written, 0,
                           (struct sockaddr *)&peer_addr, peer_addr_len);
                if (sent != written) {
                    perror("failed to send");
                    return;
                }

                fprintf(stderr, "sent %zd bytes\n", sent);
                return;
            }

            if (!validate_token(token, token_len, &peer_addr, peer_addr_len,
                                odcid, &odcid_len)) {
                fprintf(stderr, "invalid address validation token\n");
                return;
            }

            conn_io = create_conn(odcid, odcid_len);
            conn_io_outside = conn_io;
            if (conn_io == NULL) {
                return;
            }
            fprintf(stderr, "conn created!\n");

            memcpy(&conn_io->initial_peer_addr, &peer_addr, peer_addr_len);
            conn_io->initial_peer_addr_len = peer_addr_len;

            // ************************* MP: Create Second Path(Change
            // Connection Status) *********************************
            initiate_second_path(conn_io->conn, server_pcid, LOCAL_CONN_ID_LEN,
                                 client_pcid, LOCAL_CONN_ID_LEN);
            // **************************************************************************************************************
        }

        // if (conn_io != NULL && second_udp_path_get_finished &&
        // !MP_conn_finished)
        // {
        //     memcpy(&conn_io->second_peer_addr, &temp_addr, temp_addr_len);
        //     conn_io->second_peer_addr_len = temp_addr_len;
        //     fprintf(stderr, "**************second address
        //     get!***************\n");
        //     quiche_conn_second_path_is_established(conn_io->conn);
        //     MP_conn_finished = true;

        //     // after two paths built, blocks are sent.
        //     // start sending immediately and repeat every 50ms.

        //     ev_timer_init(&conn_io->sender, sender_cb, 0., 0.1);
        //     ev_timer_start(loop, &conn_io->sender);
        //     conn_io->sender.data = conn_io;
        // }

        if (memcmp(&conn_io->initial_peer_addr, &peer_addr, peer_addr_len) ==
            0) {
            fprintf(stderr, "First path OWD\n");
            quiche_conn_get_path_one_way_delay_update(conn_io->conn,
                                                      owd / 1000.0, 0);
        } else if (memcmp(&conn_io->second_peer_addr, &peer_addr,
                          peer_addr_len) == 0) {
            fprintf(stderr, "Second path OWD\n");
            quiche_conn_get_path_one_way_delay_update(conn_io->conn,
                                                      owd / 1000.0, 1);
        }

        uint64_t quiche_conn_recv_begin_time = getCurrentTime_mic();
        fprintf(stderr, "recv->quiche_recv: %" PRIu64 "\n",
                quiche_conn_recv_begin_time - recvcb_begin_time);

        ssize_t done;
        if (memcmp(&conn_io->initial_peer_addr, &peer_addr, peer_addr_len) ==
            0) {
            done = quiche_conn_recv(conn_io->conn, buf, read, 0);
        } else if (memcmp(&conn_io->second_peer_addr, &peer_addr,
                          peer_addr_len) == 0) {
            done = quiche_conn_recv(conn_io->conn, buf, read, 1);
        } else {
            done = -1;
        }
        uint64_t quiche_conn_recv_end_time = getCurrentTime_mic();
        fprintf(stderr, "Statistic time:quiche_conn_recv time: %" PRIu64 "\n",
                quiche_conn_recv_end_time - quiche_conn_recv_begin_time);

        if (done == QUICHE_ERR_DONE) {
            fprintf(stderr, "done reading\n");
            break;
        }

        if (done < 0) {
            fprintf(stderr, "failed to process packet: %zd\n", done);
            return;
        }

        fprintf(stderr, "recv %zd bytes\n", done);

        quiche_stats stats;
        quiche_conn_stats(conn_io->conn, &stats);
        fprintf(stderr,
                "-----recv=%zu sent=%zu lost_init=%zu lost_subseq=%zu "
                "rtt_init=%" PRIu64 "ns rtt_subseq=%" PRIu64 "ns-----\n",
                stats.recv, stats.sent, stats.lost_init, stats.lost_subseq,
                stats.rtt_init, stats.rtt_subseq);

        // this block of codes won't exe
        /*
        if (quiche_conn_is_established(conn_io->conn)) {
            fprintf(stderr, "-------recv_cb quiche connection
        established!---------\n"); uint64_t s = 0; quiche_stream_iter *readable
        = quiche_conn_readable(conn_io->conn); while
        (quiche_stream_iter_next(readable, &s)) { fprintf(stderr, "stream %"
        PRIu64 " is readable\n", s); bool fin = false; ssize_t recv_len =
        quiche_conn_stream_recv(conn_io->conn, s, buf, sizeof(buf),// &fin); if
        (recv_len < 0) { fprintf(stderr, "Stream recv wrong: %zd\n", recv_len);
                    break;
                } else {
                    fprintf(stderr, "Stream recv %zd bytes\n", recv_len);
                }
                if (fin) {
                    fprintf(stdout, "%" PRIu64, s);
                    //fflush(stdout);
                    fflush(stdout);
                }
            }
            quiche_stream_iter_free(readable);
        }
        */
    }

    HASH_ITER(hh, conns->h, conn_io, tmp) {
        flush_egress(loop, conn_io);

        if (quiche_conn_is_closed(conn_io->conn)) {
            HASH_DELETE(hh, conns->h, conn_io);

            ev_timer_stop(loop, &conn_io->timer);
            quiche_conn_free(conn_io->conn);
            free(conn_io);
        }
    }
}

static void timeout_cb(EV_P_ ev_timer *w, int revents) {
    struct conn_io *conn_io = w->data;
    quiche_conn_on_timeout(conn_io->conn);

    fprintf(stderr, "timeout\n");

    flush_egress(loop, conn_io);

    if (quiche_conn_is_closed(conn_io->conn)) {
        fprintf(stdout, "connection closed\n");
        fflush(stdout);
        // exit(0);
        HASH_DELETE(hh, conns->h, conn_io);

        ev_timer_stop(loop, &conn_io->timer);
        quiche_conn_free(conn_io->conn);
        free(conn_io);

        return;
    }
}

int exit_addr(struct addrinfo* local) {
  freeaddrinfo(local);
  return -1;
}

int exit_sock(struct addrinfo* local, int sock) {
  close(sock);
  freeaddrinfo(local);
  return -1;
}


int exit_config(struct addrinfo* local, int sock, quiche_config* conf) {
  quiche_config_free(config);
  close(sock);
  freeaddrinfo(local);
  return -1;
}

int exit_all(struct addrinfo *local, int sock, quiche_config *conf,
             struct dtp_config *cfgs) {
  free(cfgs);
  quiche_config_free(config);
  close(sock);
  freeaddrinfo(local);
  return -1;
}

int main(int argc, char *argv[]) {
    const char *host = argv[1];
    const char *port = argv[2];
    const char *dtp_cfg_fname = argv[3];

    // ************************* MP: Set PCID value
    // **************************************
    memset(server_pcid, 0x22, sizeof(server_pcid));
    memset(client_pcid, 0x33, sizeof(client_pcid));
    // ***********************************************************************************

    const struct addrinfo hints = {.ai_family = PF_UNSPEC,
                                   .ai_socktype = SOCK_DGRAM,
                                   .ai_protocol = IPPROTO_UDP};

    /* quiche_enable_debug_logging(debug_log, NULL); */

    struct addrinfo *local;
    if (getaddrinfo(host, port, &hints, &local) != 0) {
        perror("failed to resolve host");
        return -1;
    }

    int sock = socket(local->ai_family, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("failed to create socket");
        return exit_addr(local);
    }

    if (fcntl(sock, F_SETFL, O_NONBLOCK) != 0) {
        perror("failed to make socket non-blocking");
        return exit_sock(local, sock);
    }

    if (bind(sock, local->ai_addr, local->ai_addrlen) < 0) {
        perror("failed to connect socket");
        return exit_sock(local, sock);
    }

    config = quiche_config_new(QUICHE_PROTOCOL_VERSION);
    if (config == NULL) {
        fprintf(stderr, "failed to create config\n");
        return exit_sock(local, sock);
    }

    quiche_config_load_cert_chain_from_pem_file(config, "./cert.crt");
    quiche_config_load_priv_key_from_pem_file(config, "./cert.key");

    quiche_config_set_application_protos(
        config, (uint8_t *)"\x05hq-25\x05hq-24\x05hq-23\x08http/0.9", 21);

    quiche_config_set_max_idle_timeout(config, 15000);
    quiche_config_set_max_packet_size(config, MAX_DATAGRAM_SIZE);
    quiche_config_set_initial_max_data(config, 10000000000);
    quiche_config_set_initial_max_stream_data_bidi_local(config, 1000000000);
    quiche_config_set_initial_max_stream_data_bidi_remote(config, 1000000000);
    quiche_config_set_initial_max_streams_bidi(config, 1000000000);
    quiche_config_set_initial_max_streams_uni(config, 1000000000);
    quiche_config_set_disable_active_migration(config, true);
    quiche_config_verify_peer(config, false);
    quiche_config_set_cc_algorithm(config, QUICHE_CC_RENO);

    int cfgs_len;
    struct dtp_config *cfgs = NULL;
    cfgs = parse_dtp_config(dtp_cfg_fname, &cfgs_len, &MAX_SEND_TIMES);
    if (cfgs == NULL) {
      fprintf(stderr, "No valid DTP configuration\n");
      return exit_config(local, sock, config);
    } else if(cfgs_len == 0) {
      fprintf(stderr, "Zero length DTP configuration\n");
      return exit_all(local, sock, config, cfgs);
    }

    struct connections c;
    c.sock = sock;
    c.h = NULL;
    c.configs_len = cfgs_len;
    c.configs = cfgs;

    conns = &c;

    ev_io watcher;

    struct ev_loop *loop = ev_default_loop(0);

    ev_io_init(&watcher, recv_cb, sock, EV_READ);
    ev_io_start(loop, &watcher);
    watcher.data = &c;
    conns->watcher = &watcher;

    ev_loop(loop, 0);

    exit_all(local, sock, config, cfgs);

    return 0;
}

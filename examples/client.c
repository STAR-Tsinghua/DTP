// Copyright (C) 2018-2019, Cloudflare, Inc.
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

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <errno.h>
#include <fcntl.h>

#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <ev.h>

#include <quiche.h>

#include <dtp_config.h>

#define LOCAL_CONN_ID_LEN 16

#define MAX_DATAGRAM_SIZE 1350

#define MAX_BLOCK_SIZE 1000000  // 1Mbytes

int MAX_SEND_TIMES;

struct conn_io {
    ev_timer timer;
    ev_timer sender;
    ev_timer pace_timer;

    int send_round;

    int sock;

    uint64_t t_last;
    ssize_t can_send;
    bool done_writing;

    quiche_conn *conn;
    int configs_len;
    dtp_config *configs;
};

static void debug_log(const char *line, void *argp) {
    fprintf(stderr, "%s\n", line);
}

static void flush_egress(struct ev_loop *loop, struct conn_io *conn_io) {
    static uint8_t out[MAX_DATAGRAM_SIZE];
    uint64_t rate = quiche_bbr_get_pacing_rate(conn_io->conn);  // bits/s
    // uint64_t rate = 48*1024*1024; //48Mbits/s
    if (conn_io->done_writing) {
        conn_io->can_send = 1350;
        conn_io->t_last = getCurrentUsec();
        conn_io->done_writing = false;
    }

    while (1) {
        uint64_t t_now = getCurrentUsec();
        conn_io->can_send += rate * (t_now - conn_io->t_last) /
                             8000000;  //(bits/8)/s * s = bytes
        // fprintf(stderr, "%ld us time went, %ld bytes can send\n",
        //         t_now - conn_io->t_last, conn_io->can_send);
        conn_io->t_last = t_now;
        if (conn_io->can_send < 1350) {
            fprintf(stderr, "can_send < 1350\n");
            conn_io->pace_timer.repeat = 0.001;
            ev_timer_again(loop, &conn_io->pace_timer);
            break;
        }
        ssize_t written = quiche_conn_send(conn_io->conn, out, sizeof(out));

        if (written == QUICHE_ERR_DONE) {
            fprintf(stderr, "done writing\n");
            conn_io->pace_timer.repeat = 99999.0;
            ev_timer_again(loop, &conn_io->pace_timer);
            conn_io->done_writing = true;  // app_limited
            break;
        }

        if (written < 0) {
            fprintf(stderr, "failed to create packet: %zd\n", written);
            return;
        }

        ssize_t sent = send(conn_io->sock, out, written, 0);
        if (sent != written) {
            perror("failed to send");
            return;
        }

        // fprintf(stderr, "sent %zd bytes\n", sent);
        conn_io->can_send -= sent;
    }
    double t = quiche_conn_timeout_as_nanos(conn_io->conn) / 1e9f;
    conn_io->timer.repeat = t;
    // fprintf(stderr, "timeout t = %lf\n", t);
    ev_timer_again(loop, &conn_io->timer);
}

static void flush_egress_pace(EV_P_ ev_timer *pace_timer, int revents) {
    struct conn_io *conn_io = pace_timer->data;
    fprintf(stderr, "begin flush_egress_pace\n");
    flush_egress(loop, conn_io);
}

static void sender_cb(EV_P_ ev_timer *w, int revents) {
    struct conn_io *conn_io = w->data;

    if (quiche_conn_is_established(conn_io->conn)) {
        int deadline = 0;
        int priority = 0;
        int block_size = 0;
        int depend_id = 0;
        int stream_id = 0;
        static uint8_t buf[MAX_BLOCK_SIZE];

        for (int i = 0; i < conn_io->configs_len; i++) {
            deadline = conn_io->configs[i].deadline;
            priority = conn_io->configs[i].priority;
            block_size = conn_io->configs[i].block_size;
            stream_id = 4 * (conn_io->send_round + 1);
            if (stream_id % 8 == 0)
                depend_id = stream_id - 4;
            else
                depend_id = stream_id;

            if (block_size > MAX_BLOCK_SIZE) block_size = MAX_BLOCK_SIZE;

            // rtt lab
            //////////////////////////////
            // if(conn_io->send_round<700){
            //     deadline = 100;
            // }
            //////////////////////////////
            if (quiche_conn_stream_send_full(conn_io->conn, stream_id, buf,
                                             block_size, true, deadline,
                                             priority, depend_id) < 0) {
                fprintf(stderr, "failed to send data round %d\n",
                        conn_io->send_round);
            } else {
                fprintf(stderr, "send round %d\n", conn_io->send_round);
            }

            conn_io->send_round++;
            if (conn_io->send_round >= MAX_SEND_TIMES) {
                ev_timer_stop(loop, &conn_io->sender);
                break;
            }
        }
    }
    flush_egress(loop, conn_io);
}

static void recv_cb(EV_P_ ev_io *w, int revents) {
    struct conn_io *conn_io = w->data;
    static uint8_t buf[MAX_BLOCK_SIZE];
    uint8_t i = 3;

    while (i--) {
        ssize_t read = recv(conn_io->sock, buf, sizeof(buf), 0);

        if (read < 0) {
            if ((errno == EWOULDBLOCK) || (errno == EAGAIN)) {
                fprintf(stderr, "recv would block\n");
                break;
            }

            perror("failed to read");
            return;
        }

        ssize_t done = quiche_conn_recv(conn_io->conn, buf, read);

        if (done == QUICHE_ERR_DONE) {
            fprintf(stderr, "done reading\n");
            break;
        }

        if (done < 0) {
            fprintf(stderr, "failed to process packet\n");
            return;
        }

        // fprintf(stderr, "recv %zd bytes\n", done);
    }

    if (quiche_conn_is_closed(conn_io->conn)) {
        fprintf(stderr, "connection closed\n");

        ev_break(EV_A_ EVBREAK_ONE);
        return;
    }

    if (quiche_conn_is_established(conn_io->conn)) {
        uint64_t s = 0;

        quiche_stream_iter *readable = quiche_conn_readable(conn_io->conn);

        while (quiche_stream_iter_next(readable, &s)) {
            // fprintf(stderr, "stream %" PRIu64 " is readable\n", s);

            bool fin = false;
            ssize_t recv_len = quiche_conn_stream_recv(conn_io->conn, s, buf,
                                                       sizeof(buf), &fin);
            if (recv_len < 0) {
                break;
            }

            // printf("%.*s", (int)recv_len, buf);

            // if (fin) {
            //     if (quiche_conn_close(conn_io->conn, true, 0, NULL, 0) < 0) {
            //         fprintf(stderr, "failed to close connection\n");
            //     }
            // }
        }

        quiche_stream_iter_free(readable);
    }

    flush_egress(loop, conn_io);
}

static void timeout_cb(EV_P_ ev_timer *w, int revents) {
    struct conn_io *conn_io = w->data;
    quiche_conn_on_timeout(conn_io->conn);

    fprintf(stderr, "timeout\n");

    flush_egress(loop, conn_io);

    if (quiche_conn_is_closed(conn_io->conn)) {
        quiche_stats stats;

        quiche_conn_stats(conn_io->conn, &stats);

        fprintf(stderr,
                "connection closed, recv=%zu sent=%zu lost=%zu rtt=%" PRIu64
                "ns\n",
                stats.recv, stats.sent, stats.lost, stats.rtt);

        ev_break(EV_A_ EVBREAK_ONE);
        return;
    }
}

int main(int argc, char *argv[]) {
    const char *host = argv[1];
    const char *port = argv[2];
    const char *dtp_cfg_fname = argv[3];
    const double priority_weight = atof(argv[4]);
    const uint64_t min_priority = atoi(argv[5]);

    const struct addrinfo hints = {.ai_family = PF_UNSPEC,
                                   .ai_socktype = SOCK_DGRAM,
                                   .ai_protocol = IPPROTO_UDP};

    quiche_enable_debug_logging(debug_log, NULL);

    struct addrinfo *peer;
    if (getaddrinfo(host, port, &hints, &peer) != 0) {
        perror("failed to resolve host");
        return -1;
    }

    int sock = socket(peer->ai_family, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("failed to create socket");
        return -1;
    }

    if (fcntl(sock, F_SETFL, O_NONBLOCK) != 0) {
        perror("failed to make socket non-blocking");
        return -1;
    }

    if (connect(sock, peer->ai_addr, peer->ai_addrlen) < 0) {
        perror("failed to connect socket");
        return -1;
    }

    quiche_config *config = quiche_config_new(0xbabababa);
    if (config == NULL) {
        fprintf(stderr, "failed to create config\n");
        return -1;
    }

    quiche_config_set_application_protos(
        config, (uint8_t *)"\x05hq-25\x05hq-24\x05hq-23\x08http/0.9", 15);

    quiche_config_set_max_idle_timeout(config, 5000);
    quiche_config_set_max_packet_size(config, MAX_DATAGRAM_SIZE);
    quiche_config_set_initial_max_data(config, 10000000000);
    quiche_config_set_initial_max_stream_data_bidi_local(config, 1000000000);
    quiche_config_set_initial_max_stream_data_uni(config, 1000000000);
    quiche_config_set_initial_max_streams_bidi(config, 10000);
    quiche_config_set_initial_max_streams_uni(config, 10000);
    quiche_config_set_disable_active_migration(config, true);
    quiche_config_set_cc_algorithm(config, QUICHE_CC_BBR);

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

    int cfgs_len;
    struct dtp_config *cfgs = NULL;
    cfgs = parse_dtp_config(dtp_cfg_fname, &cfgs_len, &MAX_SEND_TIMES);
    if (cfgs_len <= 0) {
        fprintf(stderr, "No valid DTP configuration\n");
        return -1;
    }

    conn_io->sock = sock;
    conn_io->conn = conn;
    conn_io->send_round = 0;
    conn_io->configs_len = cfgs_len;
    conn_io->configs = cfgs;
    conn_io->t_last = getCurrentUsec();
    conn_io->can_send = 1350;
    conn_io->done_writing = false;

    ev_io watcher;

    struct ev_loop *loop = ev_default_loop(0);

    ev_io_init(&watcher, recv_cb, conn_io->sock, EV_READ);
    ev_io_start(loop, &watcher);
    watcher.data = conn_io;

    ev_init(&conn_io->timer, timeout_cb);
    conn_io->timer.data = conn_io;

    quiche_conn_priority_weight(conn, priority_weight);
    quiche_conn_min_priority(conn, min_priority);
    // start sending immediately and repeat every 100ms.
    ev_timer_init(&conn_io->sender, sender_cb, 0., 0.1);
    ev_timer_start(loop, &conn_io->sender);
    conn_io->sender.data = conn_io;

    // ev_timer_init(&conn_io->pace_timer, flush_egress_pace, 99999.0, 99999.0);
    // ev_timer_start(loop, &conn_io->pace_timer);
    ev_init(&conn_io->pace_timer, flush_egress_pace);
    conn_io->pace_timer.data = conn_io;

    flush_egress(loop, conn_io);

    ev_loop(loop, 0);

    freeaddrinfo(peer);

    quiche_conn_free(conn);

    quiche_config_free(config);

    return 0;
}

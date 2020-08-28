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
#include <uthash.h>

#include <dtp_config.h>
#include <quiche.h>

#define LOCAL_CONN_ID_LEN 16

#define MAX_DATAGRAM_SIZE 1350

#define MAX_BLOCK_SIZE 1000000  // 1Mbytes

char *dtp_cfg_fname;
int cfgs_len;
struct dtp_config *cfgs = NULL;

#define MAX_TOKEN_LEN                                        \
    sizeof("quiche") - 1 + sizeof(struct sockaddr_storage) + \
        QUICHE_MAX_CONN_ID_LEN

struct connections {
    int sock;

    struct conn_io *h;
};

struct conn_io {
    ev_timer timer;
    ev_timer sender;
    int send_round;
    int configs_len;
    dtp_config *configs;

    int sock;

    uint64_t t_last;
    ssize_t can_send;
    bool done_writing;
    ev_timer pace_timer;

    uint8_t cid[LOCAL_CONN_ID_LEN];

    quiche_conn *conn;

    struct sockaddr_storage peer_addr;
    socklen_t peer_addr_len;

    UT_hash_handle hh;
};

static quiche_config *config = NULL;

static struct connections *conns = NULL;

static void timeout_cb(EV_P_ ev_timer *w, int revents);

// static void debug_log(const char *line, void *argp) {
//     fprintf(stderr, "%s\n", line);
// }

static void flush_egress(struct ev_loop *loop, struct conn_io *conn_io) {
    static uint8_t out[MAX_DATAGRAM_SIZE];
    uint64_t rate = quiche_bbr_get_pacing_rate(conn_io->conn);  // bits/s
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
            // fprintf(stderr, "can_send < 1350\n");
            conn_io->pace_timer.repeat = 0.001;
            ev_timer_again(loop, &conn_io->pace_timer);
            break;
        }
        ssize_t written = quiche_conn_send(conn_io->conn, out, sizeof(out));

        if (written == QUICHE_ERR_DONE) {
            // fprintf(stderr, "done writing\n");
            conn_io->done_writing = true;  // app_limited
            conn_io->pace_timer.repeat = 99999.0;
            ev_timer_again(loop, &conn_io->pace_timer);
            break;
        }

        if (written < 0) {
            // fprintf(stderr, "failed to create packet: %zd\n", written);
            return;
        }

        ssize_t sent = sendto(conn_io->sock, out, written, 0,
                              (struct sockaddr *)&conn_io->peer_addr,
                              conn_io->peer_addr_len);
        if (sent != written) {
            perror("failed to send");
            return;
        }

        // fprintf(stderr, "sent %zd bytes\n", sent);
        conn_io->can_send -= sent;
    }

    double t = quiche_conn_timeout_as_nanos(conn_io->conn) / 1e9f;
    conn_io->timer.repeat = t;
    ev_timer_again(loop, &conn_io->timer);
}

static void flush_egress_pace(EV_P_ ev_timer *pace_timer, int revents) {
    struct conn_io *conn_io = pace_timer->data;
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

    if (quiche_conn_is_established(conn_io->conn)) {
        int deadline = 0;
        int priority = 0;
        int block_size = 0;
        int depend_id = 0;
        int stream_id = 0;
        float send_time_gap = 0.0;
        static uint8_t buf[MAX_BLOCK_SIZE];

        for (int i = conn_io->send_round; i < conn_io->configs_len; i++) {
            send_time_gap = conn_io->configs[i].send_time_gap;
            deadline = conn_io->configs[i].deadline;
            priority = conn_io->configs[i].priority;
            block_size = conn_io->configs[i].block_size;
            stream_id = 4 * (conn_io->send_round + 1) + 1;
            // if ((stream_id + 1) % 8 == 0)
            //     depend_id = stream_id - 4;
            // else
            // depend_id = stream_id;
            depend_id = stream_id;
            if (block_size > MAX_BLOCK_SIZE) block_size = MAX_BLOCK_SIZE;

            if (quiche_conn_stream_send_full(conn_io->conn, stream_id, buf,
                                             block_size, true, deadline,
                                             priority, depend_id) < 0) {
                // fprintf(stderr, "failed to send data round %d\n",
                        // conn_io->send_round);
            } else {
                // fprintf(stderr, "send round %d\n", conn_io->send_round);
            }

            conn_io->send_round++;
            conn_io->sender.repeat = send_time_gap;
            ev_timer_again(loop, &conn_io->sender);
            // fprintf(stderr, "time gap: %f\n", send_time_gap);
            if (conn_io->send_round >= conn_io->configs_len) {
                ev_timer_stop(loop, &conn_io->sender);
                break;
            }
            break;  //每次只发一个block
        }
    } else {
        float send_time_gap = conn_io->configs[0].send_time_gap;
        conn_io->sender.repeat = send_time_gap;
        ev_timer_again(loop, &conn_io->sender);
        // fprintf(stderr, "try to send first block again\n");
    }
    flush_egress(loop, conn_io);
}

static struct conn_io *create_conn(struct ev_loop *loop, uint8_t *odcid,
                                   size_t odcid_len) {
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

    cfgs = parse_dtp_config(dtp_cfg_fname, &cfgs_len);
    if (cfgs_len <= 0) {
        fprintf(stderr, "No valid configuration\n");
    } else {
        conn_io->configs_len = cfgs_len;
        conn_io->configs = cfgs;
    }
    conn_io->configs_len = cfgs_len;
    conn_io->configs = cfgs;

    conn_io->t_last = getCurrentUsec();
    conn_io->can_send = 1350;
    conn_io->done_writing = false;

    // start sending immediately and repeat every 100ms.
    ev_timer_init(&conn_io->sender, sender_cb, cfgs[0].send_time_gap, 9999.0);
    ev_timer_start(loop, &conn_io->sender);
    conn_io->sender.data = conn_io;

    ev_init(&conn_io->timer, timeout_cb);
    conn_io->timer.data = conn_io;

    ev_init(&conn_io->pace_timer, flush_egress_pace);
    conn_io->pace_timer.data = conn_io;

    HASH_ADD(hh, conns->h, cid, LOCAL_CONN_ID_LEN, conn_io);

    fprintf(stderr, "new connection,  timestamp: %lu\n",
            getCurrentUsec() / 1000 / 1000);

    return conn_io;
}

static void recv_cb(EV_P_ ev_io *w, int revents) {
    struct conn_io *tmp, *conn_io = NULL;

    static uint8_t buf[MAX_BLOCK_SIZE];
    static uint8_t out[MAX_DATAGRAM_SIZE];

    uint8_t i = 3;

    while (i--) {
        struct sockaddr_storage peer_addr;
        socklen_t peer_addr_len = sizeof(peer_addr);
        memset(&peer_addr, 0, peer_addr_len);

        ssize_t read = recvfrom(conns->sock, buf, sizeof(buf), 0,
                                (struct sockaddr *)&peer_addr, &peer_addr_len);

        if (read < 0) {
            if ((errno == EWOULDBLOCK) || (errno == EAGAIN)) {
                // fprintf(stderr, "recv would block\n");
                break;
            }

            perror("failed to read");
            return;
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
        if (rc < 0) {
            fprintf(stderr, "failed to parse header: %d\n", rc);
            return;
        }

        HASH_FIND(hh, conns->h, dcid, dcid_len, conn_io);

        if (conn_io == NULL) {
            if (!quiche_version_is_supported(version)) {
                // fprintf(stderr, "version negotiation\n");

                ssize_t written = quiche_negotiate_version(
                    scid, scid_len, dcid, dcid_len, out, sizeof(out));

                if (written < 0) {
                    // fprintf(stderr, "failed to create vneg packet: %zd\n",
                            // written);
                    return;
                }

                ssize_t sent =
                    sendto(conns->sock, out, written, 0,
                           (struct sockaddr *)&peer_addr, peer_addr_len);
                if (sent != written) {
                    perror("failed to send");
                    return;
                }

                // fprintf(stderr, "sent %zd bytes\n", sent);
                return;
            }

            if (token_len == 0) {
                // fprintf(stderr, "stateless retry\n");

                mint_token(dcid, dcid_len, &peer_addr, peer_addr_len, token,
                           &token_len);

                ssize_t written =
                    quiche_retry(scid, scid_len, dcid, dcid_len, dcid, dcid_len,
                                 token, token_len, out, sizeof(out));

                if (written < 0) {
                    // fprintf(stderr, "failed to create retry packet: %zd\n",
                    //         written);
                    return;
                }

                ssize_t sent =
                    sendto(conns->sock, out, written, 0,
                           (struct sockaddr *)&peer_addr, peer_addr_len);
                if (sent != written) {
                    perror("failed to send");
                    return;
                }

                // fprintf(stderr, "sent %zd bytes\n", sent);
                return;
            }

            if (!validate_token(token, token_len, &peer_addr, peer_addr_len,
                                odcid, &odcid_len)) {
                // fprintf(stderr, "invalid address validation token\n");
                return;
            }

            conn_io = create_conn(loop, odcid, odcid_len);
            if (conn_io == NULL) {
                return;
            }

            memcpy(&conn_io->peer_addr, &peer_addr, peer_addr_len);
            conn_io->peer_addr_len = peer_addr_len;
        }

        ssize_t done = quiche_conn_recv(conn_io->conn, buf, read);

        if (done == QUICHE_ERR_DONE) {
            // fprintf(stderr, "done reading\n");
            break;
        }

        if (done < 0) {
            // fprintf(stderr, "failed to process packet: %zd\n", done);
            return;
        }

        // fprintf(stderr, "recv %zd bytes\n", done);

        if (quiche_conn_is_established(conn_io->conn)) {
            uint64_t s = 0;

            quiche_stream_iter *readable = quiche_conn_readable(conn_io->conn);

            while (quiche_stream_iter_next(readable, &s)) {
                // fprintf(stderr, "stream %" PRIu64 " is readable\n", s);

                bool fin = false;
                ssize_t recv_len = quiche_conn_stream_recv(
                    conn_io->conn, s, buf, sizeof(buf), &fin);
                if (recv_len < 0) {
                    break;
                }
            }

            quiche_stream_iter_free(readable);
        }
    }

    HASH_ITER(hh, conns->h, conn_io, tmp) {
        flush_egress(loop, conn_io);

        if (quiche_conn_is_closed(conn_io->conn)) {
            quiche_stats stats;

            quiche_conn_stats(conn_io->conn, &stats);
            fprintf(stderr,
                    "connection closed, recv=%zu sent=%zu lost=%zu rtt=%" PRIu64
                    "ns cwnd=%zu\n",
                    stats.recv, stats.sent, stats.lost, stats.rtt, stats.cwnd);

            HASH_DELETE(hh, conns->h, conn_io);

            ev_timer_stop(loop, &conn_io->timer);
            ev_timer_stop(loop, &conn_io->sender);
            quiche_conn_free(conn_io->conn);
            free(conn_io);
        }
    }
}

static void timeout_cb(EV_P_ ev_timer *w, int revents) {
    struct conn_io *conn_io = w->data;
    quiche_conn_on_timeout(conn_io->conn);

    // fprintf(stderr, "timeout\n");

    flush_egress(loop, conn_io);

    if (quiche_conn_is_closed(conn_io->conn)) {
        quiche_stats stats;

        quiche_conn_stats(conn_io->conn, &stats);
        // fprintf(stderr,
        //         "connection closed, recv=%zu sent=%zu lost=%zu rtt=%" PRIu64
        //         "ns cwnd=%zu\n",
        //         stats.recv, stats.sent, stats.lost, stats.rtt, stats.cwnd);
        fprintf(stderr,
                    "connection closed, you can see result in client.log\n");

        // fflush(stdout);

        HASH_DELETE(hh, conns->h, conn_io);

        ev_timer_stop(loop, &conn_io->timer);
        ev_timer_stop(loop, &conn_io->sender);
        quiche_conn_free(conn_io->conn);
        free(conn_io);

        return;
    }
}

int main(int argc, char *argv[]) {
    const char *host = argv[1];
    const char *port = argv[2];
    dtp_cfg_fname = argv[3];

    const struct addrinfo hints = {.ai_family = PF_UNSPEC,
                                   .ai_socktype = SOCK_DGRAM,
                                   .ai_protocol = IPPROTO_UDP};

    // quiche_enable_debug_logging(debug_log, NULL);

    struct addrinfo *local;
    if (getaddrinfo(host, port, &hints, &local) != 0) {
        perror("failed to resolve host");
        return -1;
    }

    int sock = socket(local->ai_family, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("failed to create socket");
        return -1;
    }

    if (fcntl(sock, F_SETFL, O_NONBLOCK) != 0) {
        perror("failed to make socket non-blocking");
        return -1;
    }

    if (bind(sock, local->ai_addr, local->ai_addrlen) < 0) {
        perror("failed to connect socket");
        return -1;
    }

    config = quiche_config_new(QUICHE_PROTOCOL_VERSION);
    if (config == NULL) {
        // fprintf(stderr, "failed to create config\n");
        return -1;
    }

    quiche_config_load_cert_chain_from_pem_file(config, "cert.crt");
    quiche_config_load_priv_key_from_pem_file(config, "cert.key");

    quiche_config_set_application_protos(
        config, (uint8_t *)"\x05hq-25\x05hq-24\x05hq-23\x08http/0.9", 21);

    quiche_config_set_max_idle_timeout(config, 5000);
    quiche_config_set_max_packet_size(config, MAX_DATAGRAM_SIZE);
    quiche_config_set_initial_max_data(config, 10000000000);
    quiche_config_set_initial_max_stream_data_bidi_local(config, 1000000000);
    quiche_config_set_initial_max_stream_data_bidi_remote(config, 1000000000);
    quiche_config_set_initial_max_streams_bidi(config, 10000);
    quiche_config_set_cc_algorithm(config, Aitrans_CC_TRIGGER);

    struct connections c;
    c.sock = sock;
    c.h = NULL;

    conns = &c;

    ev_io watcher;

    struct ev_loop *loop = ev_default_loop(0);

    ev_io_init(&watcher, recv_cb, sock, EV_READ);
    ev_io_start(loop, &watcher);
    watcher.data = &c;

    ev_loop(loop, 0);

    freeaddrinfo(local);

    quiche_config_free(config);

    return 0;
}

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
#include <inttypes.h>
#include <quiche.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <uv.h>

#include "helper.h"
#include "uthash.h"

#define LOCAL_CONN_ID_LEN 16
#define MAX_DATAGRAM_SIZE 1350   // UDP
#define MAX_BLOCK_SIZE 10000000  // QUIC
#define TIME_SIZE 8
// #define SEND_INTERVAL 0.03
// #define MAX_SEND_TIMES 600
// #define MAX_FLUSH_TIMES 3

int MAX_SEND_TIMES;

#define MAX_TOKEN_LEN                                        \
    sizeof("quiche") - 1 + sizeof(struct sockaddr_storage) + \
        QUICHE_MAX_CONN_ID_LEN

struct connections {
    uv_udp_t *paths[2];

    struct conn_io *h;

    int configs_len;
    dtp_config *configs;
};

struct pacer {
    struct sockaddr_storage peer_addr;

    socklen_t peer_addr_len;
    uint64_t t_last;
    ssize_t can_send;
    bool done_writing;
    uv_timer_t pacer_timer;
};

struct conn_io {
    uv_timer_t timer;
    uv_timer_t sender;

    struct pacer pacers[2];
    uint8_t cid[LOCAL_CONN_ID_LEN];

    quiche_conn *conn;

    int send_round;
    bool second_udp_path_get_finished;
    bool MP_conn_finished;

    UT_hash_handle hh;
    struct connections *conns;
};

static quiche_config *config = NULL;
uv_loop_t *loop = NULL;
struct connections *conns = NULL;

// MP: PCID(default for second path)
uint8_t server_pcid[LOCAL_CONN_ID_LEN];
uint8_t client_pcid[LOCAL_CONN_ID_LEN];

static void timeout_cb(uv_timer_t *timer);
static void debug_log(const char *line, void *argp) {
    fprintf(stderr, "%s\n", line);
}

// called after the data was sent
static void on_send(uv_udp_send_t *req, int status) {
    free(req);
    if (status) {
        fprintf(stderr, "uv_udp_send_cb error: %s\n", uv_strerror(status));
    } else {
    }
}

static void alloc_buffer(uv_handle_t *handle, size_t suggested_size,
                         uv_buf_t *buf) {
    (void)handle;
    buf->base = malloc(suggested_size);
    buf->len = suggested_size;
}

void send_packet(uv_udp_t *handle, const struct sockaddr *peer_addr,
                 uint8_t *out, size_t len) {
    char out_with_time[MAX_DATAGRAM_SIZE + TIME_SIZE];
    char *out_time;

    /**********************************************************/
    // send the timestamp
    uint64_t t = getCurrentTime_mic();
    out_time = (char *)&t;
    for (int i = 0; i < TIME_SIZE; i++) {
        out_with_time[i] = out_time[i];
    }

    for (int i = TIME_SIZE, j = 0; j < MAX_DATAGRAM_SIZE; i++, j++) {
        out_with_time[i] = out[j];
    }
    /**********************************************************/
    len += TIME_SIZE;

    uv_buf_t buf = uv_buf_init(out_with_time, len);
    uv_udp_send_t *req = malloc(sizeof(uv_udp_send_t));

    uv_udp_send(req, handle, &buf, 1, peer_addr, on_send);
}

int flush_packets_pacing(struct conn_io *conn_io, int path) {
    uint8_t out[MAX_DATAGRAM_SIZE];

    double pacing_rate =
        quiche_conn_get_pacing_rate(conn_io->conn, path);  // bits/s

    if (conn_io->pacers[path].done_writing) {
        conn_io->pacers[path].can_send = 1350;
        conn_io->pacers[path].t_last = getCurrentTime_mic();
        conn_io->pacers[path].done_writing = false;
        uv_timer_set_repeat(&conn_io->pacers[path].pacer_timer, 99999999);
        uv_timer_again(&conn_io->pacers[path].pacer_timer);
    }
    while (1) {
        uint64_t t_now = getCurrentTime_mic();
        uint64_t can_send_increase =
            pacing_rate * (t_now - conn_io->pacers[path].t_last) / 8000000.0;
        conn_io->pacers[path].can_send +=
            can_send_increase;  //(bits/8)/s * s = bytes
        conn_io->pacers[path].t_last = t_now;
        if (conn_io->pacers[path].can_send < 1350) {
            fprintf(stderr, "first path can_send < 1350\n");
            uv_timer_set_repeat(&conn_io->pacers[path].pacer_timer, 1);
            uv_timer_again(&conn_io->pacers[path].pacer_timer);
            break;
        } else {
            ssize_t written =
                quiche_conn_send(conn_io->conn, out, sizeof(out), path);

            if (written > 0) {
                send_packet(conn_io->conns->paths[path],
                            (const struct sockaddr*)&conn_io->pacers[path].peer_addr, out, written);
                conn_io->pacers[path].can_send -= written;
            } else if (written < -1) {
                fprintf(stderr, "failed to create packet on first path: %zd\n",
                        written);
                return -1;
            } else if (written == QUICHE_ERR_DONE) {
                // conn_io->can_send = 1350;  // app_limited
                fprintf(stderr, "first path done writing\n");
                conn_io->pacers[path].done_writing = true;  // app_limited
                break;
            }
        }
    }

    int t = quiche_conn_timeout_as_nanos(conn_io->conn) / 1e6f;
    fprintf(stderr, "ts: %d", t);
    uv_timer_set_repeat(&conn_io->timer, t);
    uv_timer_again(&conn_io->timer);

    quiche_stats stats;
    quiche_conn_stats(conn_io->conn, &stats);
    fprintf(
        stderr,
        "-----recv=%zu sent=%zu lost_init=%zu lost_subseq=%zu rtt_init=%" PRIu64
        "ns rtt_subseq=%" PRIu64 "ns-----\n",
        stats.recv, stats.sent, stats.lost_init, stats.lost_subseq,
        stats.rtt_init, stats.rtt_subseq);
    return 0;
}

static void flush_egress_first_pace(uv_timer_t *pacer_timer) {
    struct conn_io *conn_io = pacer_timer->data;
    flush_packets_pacing(conn_io, 0);
}

static void flush_egress_second_pace(uv_timer_t *pacer_timer) {
    struct conn_io *conn_io = pacer_timer->data;
    flush_packets_pacing(conn_io, 1);
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

static void sender_cb(uv_timer_t *sender_timer) {
    struct conn_io *conn_io = sender_timer->data;

    if (quiche_conn_is_established(conn_io->conn)) {
        fprintf(stderr, "-----sender_cb  quiche_conn_is_established-----\n");
        int deadline = 0;
        int priority = 0;
        int block_size = 0;
        static uint8_t buf[MAX_BLOCK_SIZE];

        deadline = conn_io->conns->configs[conn_io->send_round].deadline;
        priority = conn_io->conns->configs[conn_io->send_round].priority;
        block_size = conn_io->conns->configs[conn_io->send_round].block_size;
        if (block_size > MAX_BLOCK_SIZE) block_size = MAX_BLOCK_SIZE;

        ssize_t stream_send_written = quiche_conn_stream_send_full(
            conn_io->conn, 4 * (conn_io->send_round + 1) + 1, buf, block_size,
            true, deadline, priority);
        if (stream_send_written < 0) {
            fprintf(stderr, "failed to send data round %d\n",
                    conn_io->send_round);
        } else {
            fprintf(stderr, "send round %d\n", conn_io->send_round);
        }

        conn_io->send_round++;
        if (conn_io->send_round >= MAX_SEND_TIMES) {
            uv_timer_stop(sender_timer);
        }

        fprintf(stderr, "-------sender_cb");

        if (conn_io->MP_conn_finished) {
            for (int i = 0; i < 2; i++) {
                flush_packets_pacing(conn_io, i);
            }
        }
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

    conn_io->conns = conns;
    conn_io->conn = conn;
    conn_io->send_round = 0;
    for (int i = 0; i < 2; i++) {
        conn_io->pacers[i].t_last = getCurrentTime_mic();
        conn_io->pacers[i].can_send = 1350;
        conn_io->pacers[i].done_writing = false;
    }

    conn_io->timer.data = conn_io;
    uv_timer_init(loop, &conn_io->timer);
    uv_timer_start(&conn_io->timer, timeout_cb, 100000, 0);

    conn_io->pacers[0].pacer_timer.data = conn_io;
    uv_timer_init(loop, &conn_io->pacers[0].pacer_timer);
    uv_timer_init(loop, &conn_io->pacers[1].pacer_timer);

    uv_timer_init(loop, &conn_io->sender);

    HASH_ADD(hh, conns->h, cid, LOCAL_CONN_ID_LEN, conn_io);

    fprintf(stderr, "new connection\n");

    return conn_io;
}

static void on_read(uv_udp_t *req, ssize_t nread, const uv_buf_t *buf_with_time,
                    const struct sockaddr *addr, unsigned flags) {
    struct connections *conns = req->data;
    struct conn_io *tmp, *conn_io = NULL;
    uint8_t buf[MAX_BLOCK_SIZE];
    uint8_t read_time[TIME_SIZE];
    uint8_t out_process[MAX_DATAGRAM_SIZE];
    static uint8_t first_pkt_of_second_path[] = "Second";
    fprintf(stderr, "-----------------recv_cb------------------\n");

    for (int i = 0; i < TIME_SIZE; i++) {
        read_time[i] = buf_with_time->base[i];
    }
    // get one way delay
    for (int i = TIME_SIZE, j = 0; j < TIME_SIZE; i++, j++) {
        read_time[j] = buf_with_time->base[i];
    }
    uint64_t owd = *(uint64_t *)read_time;
    fprintf(stderr, "OWD time: %" PRIu64 "\n", owd);

    for (int i = 2 * TIME_SIZE, j = 0; i < nread; i++, j++) {
        buf[j] = buf_with_time->base[i];
    }
    nread -= 2 * TIME_SIZE;

    if (memcmp(buf, first_pkt_of_second_path,
               sizeof(first_pkt_of_second_path)) == 0) {
        fprintf(stderr,
                "**********************recv udp pkt from second "
                "address**********************\n");

        if (!conn_io->MP_conn_finished) {
            memcpy(&conn_io->pacers[1].peer_addr, addr,
                   sizeof(struct sockaddr));
            conn_io->pacers[1].peer_addr_len = sizeof(struct sockaddr);
            fprintf(stderr,
                    "**************second address get!***************\n");
            quiche_conn_second_path_is_established(conn_io->conn);
            conn_io->MP_conn_finished = true;

            // after two paths built, blocks are sent.
            // start sending immediately and repeat every 50ms.
            // TODO init sender timer first.
            uv_timer_start(&conn_io->sender, sender_cb, 0, 100);
            conn_io->sender.data = conn_io;
        }
        uv_timer_start(&conn_io->pacers[1].pacer_timer, flush_egress_second_pace, 0,
                    0);  // TODO do we need to start immediately?
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

    int rc =
        quiche_header_info(buf, nread, LOCAL_CONN_ID_LEN, &version, &type, scid,
                           &scid_len, dcid, &dcid_len, token, &token_len);

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
        found = mp_mapping_pcid_to_mcid(conn_io->conn, dcid, dcid_len, mscid,
                                        &mscid_len);
        if (found) {  // Mapping is success
            fprintf(stderr, "FOUND\n");
            break;
        }
    }

    fprintf(stderr, "found:%d\n", found);
    fprintf(stderr, "conn_io==NULL %d\n", conn_io == NULL);

    if (found) {
        HASH_FIND(hh, conns->h, mscid, mscid_len, conn_io);
        fprintf(stderr, "conn_io==NULL %d\n", conn_io == NULL);
    }

    if (conn_io == NULL) {
        fprintf(stderr, "version: %d\n", version);
        if (!quiche_version_is_supported(version)) {
            fprintf(stderr, "version negotiation\n");
            ssize_t written =
                quiche_negotiate_version(scid, scid_len, dcid, dcid_len,
                                         out_process, sizeof(out_process));

            fprintf(stderr, "written:%ld\n", written);
            if (written < 0) {
                fprintf(stderr, "failed to create vneg packet: %zd\n", written);
                return;
            } else {
                send_packet(req, addr, out_process, written);
                return;
            }
        }

        if (token_len == 0) {
            fprintf(stderr, "stateless retry\n");
            // TODO check type signature
            mint_token(dcid, dcid_len, (struct sockaddr_storage*)addr, sizeof(struct sockaddr), token,
                       &token_len);

            ssize_t written = quiche_retry(scid, scid_len, dcid, dcid_len, dcid,
                                           dcid_len, token, token_len,
                                           out_process, sizeof(out_process));

            if (written < 0) {
                fprintf(stderr, "failed to create retry packet: %zd\n",
                        written);
                return;
            } else {
                send_packet(req, addr, out_process, written);
                return;
            }
        }

        if (!validate_token(token, token_len, (struct sockaddr_storage*)addr, sizeof(struct sockaddr),
                            odcid, &odcid_len)) {
            fprintf(stderr, "invalid address validation token\n");
            return;
        }

        conn_io = create_conn(odcid, odcid_len);
        conns->h = conn_io;
        if (conn_io == NULL) {
            return;
        }
        fprintf(stderr, "conn created!\n");

        memcpy(&conn_io->pacers[0].peer_addr, addr, sizeof(struct sockaddr));
        conn_io->pacers[0].peer_addr_len = sizeof(struct sockaddr);

        // ************************* MP: Create Second Path(Change
        // Connection Status) *********************************
        initiate_second_path(conn_io->conn, server_pcid, LOCAL_CONN_ID_LEN,
                             client_pcid, LOCAL_CONN_ID_LEN);
        // **************************************************************************************************************
        uv_timer_start(&conn_io->pacers[0].pacer_timer, flush_egress_first_pace, 0,
                    0);  // TODO do we need to start immediately?
    }

    // TODO will this work?
    int path = req == conns->paths[0] ? 0 : 1;

    quiche_conn_get_path_one_way_delay_update(conn_io->conn, owd / 1000.0,
                                              path);
    ssize_t done = quiche_conn_recv(conn_io->conn, buf, nread, path);
    if (done == QUICHE_ERR_DONE) {
        fprintf(stderr, "done reading\n");
        return;
    } else if (done < 0) {
        fprintf(stderr, "failed to process packet: %zd\n", done);
        return;
    }
    quiche_stats stats;
    quiche_conn_stats(conn_io->conn, &stats);
    fprintf(stderr,
            "-----recv=%zu sent=%zu lost_init=%zu lost_subseq=%zu "
            "rtt_init=%" PRIu64 "ns rtt_subseq=%" PRIu64 "ns-----\n",
            stats.recv, stats.sent, stats.lost_init, stats.lost_subseq,
            stats.rtt_init, stats.rtt_subseq);
    flush_packets_pacing(conn_io, path);

    HASH_ITER(hh, conns->h, conn_io, tmp) {
        if (quiche_conn_is_closed(conn_io->conn)) {
            HASH_DELETE(hh, conns->h, conn_io);

            uv_timer_stop(&conn_io->timer);
            uv_timer_stop(&conn_io->sender);
            quiche_conn_free(conn_io->conn);
            free(conn_io);
        }
    }
}

static void timeout_cb(uv_timer_t *timer) {
    struct conn_io *conn_io = timer->data;
    quiche_conn_on_timeout(conn_io->conn);

    fprintf(stderr, "timeout\n");

    for (int path = 0; path < 2; path++) {
        flush_packets_pacing(conn_io, path);
    }

    if (quiche_conn_is_closed(conn_io->conn)) {
        fprintf(stdout, "connection closed\n");
        fflush(stdout);
        HASH_DELETE(hh, conns->h, conn_io);

        uv_timer_stop(timer);
        uv_timer_stop(&conn_io->sender);
        quiche_conn_free(conn_io->conn);
        free(conn_io);

        return;
    }
}

uv_udp_t *init_echo_udp_server(uv_loop_t *loop, const char *address,
                               uint16_t port) {
    uv_udp_t *recv_socket = malloc(sizeof(uv_udp_t));
    struct sockaddr_in recv_addr;
    uv_ip4_addr(address, port, &recv_addr);

    int r = uv_udp_init(loop, recv_socket);
    if (r < 0) {
        perror("uv_udp_init failed");
        exit(-1);
    }

    r = uv_udp_bind(recv_socket, (const struct sockaddr *)&recv_addr,
                    UV_UDP_REUSEADDR);
    if (r < 0) {
        perror("uv_udp_bind failed");
        exit(-1);
    }
    r = uv_udp_recv_start(recv_socket, alloc_buffer, on_read);
    if (r < 0) {
        perror("uv_udp_recv_start failed");
        exit(-1);
    }
    return recv_socket;
}

int main(int argc, char *argv[]) {
    loop = uv_default_loop();

    struct connections c;
    c.paths[0] = init_echo_udp_server(loop, argv[1], atoi(argv[2]));
    c.paths[1] = init_echo_udp_server(loop, argv[3], atoi(argv[4]));

    const char *dtp_cfg_fname = argv[5];
    int cfgs_len;
    struct dtp_config *cfgs = NULL;
    cfgs = parse_dtp_config(dtp_cfg_fname, &cfgs_len, &MAX_SEND_TIMES);
    if (cfgs_len <= 0) {
        fprintf(stderr, "No valid DTP configuration\n");
        return -1;
    }

    c.h = NULL;
    c.configs_len = cfgs_len;
    c.configs = cfgs;
    conns = &c;

    // ************************* MP: Set PCID value
    // **************************************
    memset(server_pcid, 0x22, sizeof(server_pcid));
    memset(client_pcid, 0x33, sizeof(client_pcid));
    // ***********************************************************************************

    quiche_enable_debug_logging(debug_log, NULL);

    config = quiche_config_new(QUICHE_PROTOCOL_VERSION);
    if (config == NULL) {
        fprintf(stderr, "failed to create config\n");
        return -1;
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

    uv_run(loop, UV_RUN_DEFAULT);
    uv_loop_close(loop);
    free(loop);
    quiche_config_free(config);

    return 0;
}

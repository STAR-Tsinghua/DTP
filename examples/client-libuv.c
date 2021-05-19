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
#include <inttypes.h>
#include <memory.h>
#include <netdb.h>
#include <quiche.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <uv.h>

#include "helper.h"

#define LOCAL_CONN_ID_LEN 16

#define MAX_DATAGRAM_SIZE 1350

#define MAX_BLOCK_SIZE 10000000  // 10Mbytes
#define TIME_SIZE 8
static uint64_t chunk_before_deadline = 0;
static uint64_t recv_num = 0;

struct conn_io {
    uv_timer_t timer;

    uv_udp_t *paths[2];
    quiche_conn *conn;

    bool first_udp_packet;
};

// MP: PCID(default for second path)
uint8_t server_pcid[LOCAL_CONN_ID_LEN];
uint8_t client_pcid[LOCAL_CONN_ID_LEN];
// *********************************************

// MP: server->client one way delay
uint64_t one_way_delay = 0;

// called after the data was sent
static void on_send(uv_udp_send_t *req, int status) {
    free(req);
    if (status) {
        fprintf(stderr, "uv_udp_send_cb error: %s\n", uv_strerror(status));
    } else {
    }
}

void send_packet(struct conn_io *conn_io, const uint8_t *out, size_t len,
                 int path) {
    char out_with_time[MAX_DATAGRAM_SIZE + 2 * TIME_SIZE];
    char *out_time;

    /**********************************************************/
    // send the timestamp
    uint64_t t = getCurrentTime_mic();
    out_time = (char *)&t;
    for (int i = 0; i < TIME_SIZE; i++) {
        out_with_time[i] = out_time[i];
    }

    // send the one_way_delay
    out_time = (char *)&one_way_delay;
    for (int i = TIME_SIZE, j = 0; j < TIME_SIZE; i++, j++) {
        out_with_time[i] = out_time[j];
    }

    for (int i = 2 * TIME_SIZE, j = 0; j < MAX_DATAGRAM_SIZE; i++, j++) {
        out_with_time[i] = out[j];
    }
    /**********************************************************/
    len += 2 * TIME_SIZE;

    uv_buf_t buf = uv_buf_init(out_with_time, len);
    uv_udp_send_t *req = malloc(sizeof(uv_udp_send_t));

    uv_udp_send(req, conn_io->paths[path], &buf, 1, NULL, on_send);
}

int flush_packets(struct conn_io *conn_io, int path) {
    uint8_t out[MAX_DATAGRAM_SIZE];
    while (1) {
        uint64_t quiche_conn_send_begin_time = getCurrentTime_mic();
        ssize_t written =
            quiche_conn_send(conn_io->conn, out, sizeof(out), path);
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
        send_packet(conn_io, out, written, path);
    }
    return 0;
}

static void debug_log(const char *line, void *argp) {
    fprintf(stderr, "%s\n", line);
}

static void flush_egress(struct conn_io *conn_io, int path) {
    fprintf(stderr, "--flush egress-------\n");
    flush_packets(conn_io, path);

    int t = quiche_conn_timeout_as_nanos(conn_io->conn) / 1e6f;
    fprintf(stderr, "ts: %d\n", t);
    uv_timer_set_repeat(&conn_io->timer, t);
    uv_timer_again(&conn_io->timer);

    if (quiche_conn_is_established(conn_io->conn) &&
        !conn_io->first_udp_packet) {
        // connection建立起来之后，启动handshake of second path.
        fprintf(stderr, "Send first packet of second path\n");
        uint8_t out[MAX_DATAGRAM_SIZE] = "Second";
        send_packet(conn_io, out, sizeof("Second"), 1);
        conn_io->first_udp_packet = true;
    }

    fprintf(stderr, "conn establish %d\n",
            quiche_conn_is_established(conn_io->conn));
}

static void on_read(uv_udp_t *req, ssize_t nread, const uv_buf_t *buf_with_time,
                    const struct sockaddr *addr, unsigned flags) {
    struct conn_io *conn_io = req->data;
    int path = req == conn_io->paths[0] ? 0 : 1;
    uint8_t buf[MAX_BLOCK_SIZE];
    uint8_t read_time[TIME_SIZE];
    fprintf(stderr, "-----------------recv_cb------------------\n");

    /**************************************************/
    for (int i = 0; i < TIME_SIZE; i++) {
        read_time[i] = buf_with_time->base[i];
    }
    uint64_t t1 = *(uint64_t *)read_time;
    uint64_t t2 = getCurrentTime_mic();
    one_way_delay = t2 - t1;
    fprintf(stderr, "first_path t1: %" PRIu64 "\n", t1);
    fprintf(stderr, "first_path t2: %" PRIu64 "\n", t2);
    fprintf(stderr,
            "first_path: server to client -> UDP trans time: %" PRIu64 "\n",
            t2 - t1);
    for (int i = TIME_SIZE, j = 0; j < nread; i++, j++) {
        buf[j] = buf_with_time->base[i];
    }

    /**************************************************/
    nread -= TIME_SIZE;

    uint64_t quiche_conn_recv_begin_time = getCurrentTime_mic();
    ssize_t done = quiche_conn_recv(conn_io->conn, buf, nread, path);
    uint64_t quiche_conn_recv_end_time = getCurrentTime_mic();
    fprintf(stderr, "First path: quiche_conn_recv time: %" PRIu64 "\n",
            quiche_conn_recv_end_time - quiche_conn_recv_begin_time);

    if (done == QUICHE_ERR_DONE) {
        fprintf(stderr, "done reading\n");
    }

    if (done < 0) {
        fprintf(stderr, "failed to process packet\n");
        free(buf_with_time->base);
        return;
    }

    fprintf(stderr, "Client recv a packet on path %d\n", path);
    fprintf(stderr, "recv %zd bytes\n", done);

    if (quiche_conn_is_established(conn_io->conn)) {
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
            }

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
        free(buf_with_time->base);
        return;
    }

    flush_egress(conn_io, path);
}

static void timeout_cb(uv_timer_t *timer) {
    struct conn_io *conn_io = timer->data;
    quiche_conn_on_timeout(conn_io->conn);

    fprintf(stderr, "-------timeout");

    flush_egress(conn_io, 0);
    flush_egress(conn_io, 1);

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

        uv_timer_stop(timer);
        // TODO stop all callback.
    }
}

static void alloc_buffer(uv_handle_t *handle, size_t suggested_size,
                         uv_buf_t *buf) {
    (void)handle;
    buf->base = malloc(suggested_size);
    buf->len = suggested_size;
}

uv_udp_t *init_udp_client_path(uv_loop_t *loop, void *data, const char *remote_ip,
                               uint16_t remote_port, const char *local_ip,
                               uint16_t local_port) {
    uv_udp_t *send_socket = malloc(sizeof(uv_udp_t));
    send_socket->data = data;
    struct sockaddr_in local_addr;
    uv_ip4_addr(local_ip, local_port, &local_addr);

    struct sockaddr_in remote_addr;
    uv_ip4_addr(remote_ip, remote_port, &remote_addr);

    uv_udp_init(loop, send_socket);

    uv_udp_bind(send_socket, (const struct sockaddr *)&local_addr,
                UV_UDP_REUSEADDR);
    uv_udp_connect(send_socket, (const struct sockaddr *)&remote_addr);
    uv_udp_recv_start(send_socket, alloc_buffer, on_read);
    return send_socket;
}

int main(int argc, char *argv[]) {
    // TODO use uv_interface_addresses() to get local IP.
    // remote ip1, remote port1, local ip1, local port1, remote ip2, remote
    // port2, local ip2, local port2.
    printf("endter the main\n");
    uv_loop_t *loop = uv_default_loop();
    struct conn_io conn_io;

    uv_udp_t *path1 = init_udp_client_path(
        loop, &conn_io, argv[1], atoi(argv[2]), argv[3], atoi(argv[4]));
    uv_udp_t *path2 = init_udp_client_path(
        loop, &conn_io, argv[5], atoi(argv[6]), argv[7], atoi(argv[8]));

    conn_io.paths[0] = path1;
    conn_io.paths[1] = path2;
    quiche_enable_debug_logging(debug_log, NULL);

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
        quiche_connect(argv[1], (const uint8_t *)scid, sizeof(scid), config);

    if (conn == NULL) {
        fprintf(stderr, "failed to create connection\n");
        return -1;
    }

    conn_io.conn = conn;
    conn_io.first_udp_packet = false;

    // ************************* MP: Set PCID value
    // ************************************** set server_pcid, client_pcid
    memset(server_pcid, 0x22, sizeof(server_pcid));
    memset(client_pcid, 0x33, sizeof(client_pcid));
    // ***********************************************************************************

    // ************************* MP: Create Second Path(Change Connection
    // Status) *********************************
    initiate_second_path(conn_io.conn, client_pcid, LOCAL_CONN_ID_LEN,
                         server_pcid, LOCAL_CONN_ID_LEN);
    // **************************************************************************************************************
    conn_io.timer.data = &conn_io;
    uv_timer_init(loop, &conn_io.timer);
    uv_timer_start(&conn_io.timer, timeout_cb, 100000, 0);

    flush_egress(&conn_io, 0);
    uv_run(loop, UV_RUN_DEFAULT);
    uv_loop_close(loop);
    free(loop);
    quiche_conn_free(conn);

    quiche_config_free(config);

    return 0;
}

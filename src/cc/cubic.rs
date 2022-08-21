// Copyright (C) 2019, Cloudflare, Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright notice,
//       this list of conditions and the following disclaimer.
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

//! CUBIC Congestion Control
//!
//! This implementation is based on the following RFC:
//! <https://tools.ietf.org/html/rfc8312>
//!
//! Note that Slow Start can use HyStart++ when enabled.

use std::cmp;

use std::time;
use std::time::Duration;
use std::time::Instant;

use crate::cc;
use crate::cc::hystart;
use crate::packet;
use crate::recovery::Sent;

/// CUBIC Constants.
///
/// These are recommended value in RFC8312.
const BETA_CUBIC: f64 = 0.7;

const C: f64 = 0.4;

/// The packet count threshold to restore to the prior state if the
/// lost packet count since the last checkpoint is less than the threshold.
const RESTORE_COUNT_THRESHOLD: usize = 10;

/// CUBIC State Variables.
///
/// We need to keep those variables across the connection.
/// k, w_max, w_last_max is described in the RFC.
#[derive(Debug, Default)]
pub struct State {
    pub k: f64,

    pub w_max: f64,

    pub w_last_max: f64,

    // Used in CUBIC fix (see on_packet_sent())
    pub last_sent_time: Option<Instant>,

    // Store cwnd increment during congestion avoidance.
    pub cwnd_inc: usize,

    // CUBIC state checkpoint preceding the last congestion event.
    pub prior: PriorState,
}

/// Stores the CUBIC state from before the last congestion event.
///
/// <https://tools.ietf.org/id/draft-ietf-tcpm-rfc8312bis-00.html#section-4.9>
#[derive(Debug, Default)]
pub struct PriorState {
    pub congestion_window: usize,

    pub ssthresh: usize,

    pub w_max: f64,

    pub w_last_max: f64,

    pub k: f64,

    pub epoch_start: Option<Instant>,

    pub lost_count: usize,
}

/// CUBIC Functions.
///
/// Note that these calculations are based on a count of cwnd as bytes,
/// not packets.
/// Unit of t (duration) and RTT are based on seconds (f64).
impl State {
    // K = cbrt(w_max * (1 - beta_cubic) / C) (Eq. 2)
    pub fn cubic_k(&self, max_datagram_size: usize) -> f64 {
        let w_max = self.w_max / max_datagram_size as f64;
        libm::cbrt(w_max * (1.0 - BETA_CUBIC) / C)
    }

    // W_cubic(t) = C * (t - K)^3 - w_max (Eq. 1)
    pub fn w_cubic(&self, t: Duration, max_datagram_size: usize) -> f64 {
        let w_max = self.w_max / max_datagram_size as f64;

        (C * (t.as_secs_f64() - self.k).powi(3) + w_max)
            * max_datagram_size as f64
    }

    // W_est(t) = w_max * beta_cubic + 3 * (1 - beta_cubic) / (1 + beta_cubic) *
    // (t / RTT) (Eq. 4)
    pub fn w_est(
        &self, t: Duration, rtt: Duration, max_datagram_size: usize,
    ) -> f64 {
        let w_max = self.w_max / max_datagram_size as f64;
        (w_max * BETA_CUBIC
            + 3.0 * (1.0 - BETA_CUBIC) / (1.0 + BETA_CUBIC) * t.as_secs_f64()
                / rtt.as_secs_f64())
            * max_datagram_size as f64
    }
}

pub struct Cubic {
    congestion_window: usize,

    bytes_in_flight: usize,

    congestion_recovery_start_time: Option<Instant>,

    ssthresh: usize,

    cubic_state: State,

    hystart: cc::hystart::Hystart,

    // for slow start and congestion avoidance
    bytes_acked_sl: usize,
}

impl cc::CongestionControl for Cubic {
    fn new(_init_cwnd: u64, _init_pacing_rate: u64) -> Self
    where
        Self: Sized,
    {
        Cubic {
            congestion_window: cc::INITIAL_WINDOW,

            bytes_in_flight: 0,

            congestion_recovery_start_time: None,

            ssthresh: std::usize::MAX,

            cubic_state: State::default(),

            hystart: hystart::Hystart::new(true),

            bytes_acked_sl: 0,
        }
    }

    fn cwnd(&self) -> usize {
        self.congestion_window
    }

    fn collapse_cwnd(&mut self) {
        let r = self;
        let cubic = &mut r.cubic_state;

        r.congestion_recovery_start_time = None;

        cubic.w_last_max = r.congestion_window as f64;
        cubic.w_max = cubic.w_last_max;

        // 4.7 Timeout - reduce ssthresh based on BETA_CUBIC
        r.ssthresh = (r.congestion_window as f64 * BETA_CUBIC) as usize;
        r.ssthresh = cmp::max(r.ssthresh, cc::MINIMUM_WINDOW);

        cubic.cwnd_inc = 0;

        // reno::collapse_window
        r.congestion_window = cc::MINIMUM_WINDOW;
        r.bytes_acked_sl = 0;
    }

    fn bytes_in_flight(&self) -> usize {
        self.bytes_in_flight
    }

    fn decrease_bytes_in_flight(&mut self, bytes_in_flight: usize) {
        self.bytes_in_flight =
            self.bytes_in_flight.saturating_sub(bytes_in_flight);
    }

    fn congestion_recovery_start_time(&self) -> Option<Instant> {
        self.congestion_recovery_start_time
    }

    fn on_packet_sent_cc(
        &mut self, _pkt: &Sent, bytes_sent: usize, _trace_id: &str,
    ) {
        // See https://github.com/torvalds/linux/commit/30927520dbae297182990bb21d08762bcc35ce1d
        // First transmit when no packets in flight
        let r = self;
        let cubic = &mut r.cubic_state;
        let now = Instant::now();

        if let Some(last_sent_time) = cubic.last_sent_time {
            if r.bytes_in_flight == 0 {
                let delta = now - last_sent_time;

                // We were application limited (idle) for a while.
                // Shift epoch start to keep cwnd growth to cubic curve.
                if let Some(recovery_start_time) =
                    r.congestion_recovery_start_time
                {
                    if delta.as_nanos() > 0 {
                        r.congestion_recovery_start_time =
                            Some(recovery_start_time + delta);
                    }
                }
            }
        }

        cubic.last_sent_time = Some(now);

        // reno::on_packet_sent
        r.bytes_in_flight += bytes_sent;
    }

    fn on_packet_acked_cc(
        &mut self, packet: &Sent, _srtt: Duration, min_rtt: Duration,
        latest_rtt: Duration, app_limited: bool, _trace_id: &str,
        epoch: packet::Epoch, lost_count: usize,
    ) {
        let mut r = self;
        let now = Instant::now();
        let in_congestion_recovery = r.in_congestion_recovery(packet.time);

        r.bytes_in_flight = r.bytes_in_flight.saturating_sub(packet.size);

        if in_congestion_recovery {
            return;
        }

        if app_limited {
            return;
        }

        // Detecting spurious congestion events.
        // <https://tools.ietf.org/id/draft-ietf-tcpm-rfc8312bis-00.html#section-4.9>
        //
        // When the recovery episode ends with recovering
        // a few packets (less than RESTORE_COUNT_THRESHOLD), it's considered
        // as spurious and restore to the previous state.
        if r.congestion_recovery_start_time.is_some() {
            let new_lost = lost_count - r.cubic_state.prior.lost_count;

            if r.congestion_window < r.cubic_state.prior.congestion_window
                && new_lost < RESTORE_COUNT_THRESHOLD
            {
                r.rollback();
                return;
            }
        }

        if r.congestion_window < r.ssthresh {
            // Slow start.
            r.congestion_window += packet.size;

            if r.hystart.enabled()
                && epoch == packet::EPOCH_APPLICATION
                && r.hystart.try_enter_lss(
                    packet,
                    latest_rtt,
                    r.congestion_window,
                    now,
                    cc::MAX_DATAGRAM_SIZE,
                )
            {
                r.ssthresh = r.congestion_window;
            }
        } else {
            // Congestion avoidance.
            let ca_start_time;

            // In LSS, use lss_start_time instead of congestion_recovery_start_time.
            if r.hystart.in_lss(epoch) {
                ca_start_time = r.hystart.lss_start_time().unwrap();

                // Reset w_max and k when LSS started.
                if r.cubic_state.w_max == 0.0 {
                    r.cubic_state.w_max = r.congestion_window as f64;
                    r.cubic_state.k = 0.0;
                }
            } else {
                match r.congestion_recovery_start_time {
                    Some(t) => ca_start_time = t,
                    None => {
                        // When we come here without congestion_event() triggered,
                        // initialize congestion_recovery_start_time, w_max and k.
                        ca_start_time = now;
                        r.congestion_recovery_start_time = Some(now);

                        r.cubic_state.w_max = r.congestion_window as f64;
                        r.cubic_state.k = 0.0;
                    },
                }
            }

            let t = now - ca_start_time;

            // w_cubic(t + rtt)
            let w_cubic =
                r.cubic_state.w_cubic(t + min_rtt, cc::MAX_DATAGRAM_SIZE);

            // w_est(t)
            let w_est = r.cubic_state.w_est(t, min_rtt, cc::MAX_DATAGRAM_SIZE);

            let mut cubic_cwnd = r.congestion_window;

            if w_cubic < w_est {
                // TCP friendly region.
                cubic_cwnd = cmp::max(cubic_cwnd, w_est as usize);
            } else if cubic_cwnd < w_cubic as usize {
                // Concave region or convex region use same increment.
                let cubic_inc = (w_cubic - cubic_cwnd as f64) / cubic_cwnd as f64
                    * cc::MAX_DATAGRAM_SIZE as f64;

                cubic_cwnd += cubic_inc as usize;
            }

            // When in Limited Slow Start, take the max of CA cwnd and
            // LSS cwnd.
            if r.hystart.in_lss(epoch) {
                let lss_cwnd = r.hystart.lss_cwnd(
                    packet.size,
                    r.bytes_acked_sl,
                    r.congestion_window,
                    r.ssthresh,
                    cc::MAX_DATAGRAM_SIZE,
                );

                r.bytes_acked_sl += packet.size;

                cubic_cwnd = cmp::max(cubic_cwnd, lss_cwnd);
            }

            // Update the increment and increase cwnd by MSS.
            r.cubic_state.cwnd_inc += cubic_cwnd - r.congestion_window;

            // cwnd_inc can be more than 1 MSS in the late stage of max probing.
            // however QUIC recovery draft 7.4 (Congestion Avoidance) limits
            // the increase of cwnd to 1 max_datagram_size per cwnd acknowledged.
            if r.cubic_state.cwnd_inc >= cc::MAX_DATAGRAM_SIZE {
                r.congestion_window += cc::MAX_DATAGRAM_SIZE;
                r.cubic_state.cwnd_inc = 0;
            }
        }
        r.bytes_acked_sl = 0;
        debug!(
            "timestamp= {:?} {:?}",
            time::SystemTime::now()
                .duration_since(time::SystemTime::UNIX_EPOCH)
                .unwrap()
                .as_millis(),
            r
        );
    }

    fn congestion_event(
        &mut self, _srtt: Duration, time_sent: Instant, now: Instant,
        _trace_id: &str, _packet_id: u64, epoch: packet::Epoch,
        lost_count: usize,
    ) {
        // Start a new congestion event if packet was sent after the
        // start of the previous congestion recovery period.
        if !self.in_congestion_recovery(time_sent) {
            self.checkpoint(lost_count);

            let r = self;

            r.congestion_recovery_start_time = Some(now);
            // Fast convergence
            if r.cubic_state.w_max < r.cubic_state.w_last_max {
                r.cubic_state.w_last_max = r.cubic_state.w_max;
                r.cubic_state.w_max =
                    r.cubic_state.w_max as f64 * (1.0 + BETA_CUBIC) / 2.0;
            } else {
                r.cubic_state.w_last_max = r.cubic_state.w_max;
            }

            r.cubic_state.w_max = r.congestion_window as f64;
            r.ssthresh = (r.cubic_state.w_max * BETA_CUBIC) as usize;
            r.ssthresh = cmp::max(r.ssthresh, cc::MINIMUM_WINDOW);
            r.congestion_window = r.ssthresh;
            r.cubic_state.k = r.cubic_state.cubic_k(cc::MAX_DATAGRAM_SIZE);

            r.cubic_state.cwnd_inc =
                (r.cubic_state.cwnd_inc as f64 * BETA_CUBIC) as usize;

            if r.hystart.in_lss(epoch) {
                r.hystart.congestion_event();
            }
            debug!(
                "timestamp= {:?} {:?}",
                time::SystemTime::now()
                    .duration_since(time::SystemTime::UNIX_EPOCH)
                    .unwrap()
                    .as_millis(),
                r
            );
        }
    }

    // unused
    fn pacing_rate(&self) -> u64 {
        u64::max_value()
    }

    fn cc_bbr_begin_ack(&mut self, _ack_time: Instant) {}

    fn cc_bbr_end_ack(&mut self) {}

    fn bbr_min_rtt(&mut self) -> Duration {
        Duration::new(0, 0)
    }

    fn cc_trigger(
        &mut self, _event_type: char, _rtt: u64, _bytes_in_flight: u64,
        _packet_id: u64,
    ) {
    }
}

impl Cubic {
    fn checkpoint(&mut self, lost_count: usize) {
        let r = self;
        r.cubic_state.prior.congestion_window = r.congestion_window;
        r.cubic_state.prior.ssthresh = r.ssthresh;
        r.cubic_state.prior.w_max = r.cubic_state.w_max;
        r.cubic_state.prior.w_last_max = r.cubic_state.w_last_max;
        r.cubic_state.prior.k = r.cubic_state.k;
        r.cubic_state.prior.epoch_start = r.congestion_recovery_start_time;
        r.cubic_state.prior.lost_count = lost_count;
    }
    fn rollback(&mut self) {
        let r = self;
        r.congestion_window = r.cubic_state.prior.congestion_window;
        r.ssthresh = r.cubic_state.prior.ssthresh;
        r.cubic_state.w_max = r.cubic_state.prior.w_max;
        r.cubic_state.w_last_max = r.cubic_state.prior.w_last_max;
        r.cubic_state.k = r.cubic_state.prior.k;
        r.congestion_recovery_start_time = r.cubic_state.prior.epoch_start;
    }
}

impl std::fmt::Debug for Cubic {
    fn fmt(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
        write!(
            f,
            "cwnd= {} ssthresh= {} bytes_in_flight= {}",
            self.congestion_window, self.ssthresh, self.bytes_in_flight,
        )
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::cc::hystart;
    use crate::recovery;

    #[test]
    fn cubic_init() {
        let mut cfg = crate::Config::new(crate::PROTOCOL_VERSION).unwrap();
        cfg.set_cc_algorithm(cc::Algorithm::CUBIC);

        let r = recovery::Recovery::new(&cfg);

        assert!(r.cc.cwnd() > 0);
        assert_eq!(r.cc.bytes_in_flight(), 0);
    }

    #[test]
    fn cubic_send() {
        let mut cfg = crate::Config::new(crate::PROTOCOL_VERSION).unwrap();
        cfg.set_cc_algorithm(cc::Algorithm::CUBIC);

        let mut r = recovery::Recovery::new(&cfg);

        let dummy_pkt = Sent {
            pkt_num: 0,
            frames: vec![],
            time: Instant::now() - Duration::from_secs(1),
            size: 1000,
            ack_eliciting: true,
            in_flight: false,
            fec_info: crate::fec::FecStatus::default(),
        };
        r.cc.on_packet_sent_cc(&dummy_pkt, 1000, "cubic_send");

        assert_eq!(r.cc.bytes_in_flight(), 1000);
    }

    // #[test]
    // fn cubic_slow_start() {
    //     let mut cfg = crate::Config::new(crate::PROTOCOL_VERSION).unwrap();
    //     cfg.set_cc_algorithm(cc::Algorithm::CUBIC);

    //     let r = recovery::Recovery::new(&cfg);
    //     let now = Instant::now();

    //     let p = recovery::Sent {
    //         pkt_num: 0,
    //         frames: vec![],
    //         time: now,
    //         size: cc::MAX_DATAGRAM_SIZE,
    //         ack_eliciting: true,
    //         in_flight: true,
    //         fec_info: crate::fec::FecStatus::default()
    //     };

    //     // Send initcwnd full MSS packets to become no longer app limited
    //     for _ in 0..cc::INITIAL_WINDOW_PACKETS {
    //         r.on_packet_sent(p, packet::EPOCH_APPLICATION, true,
    //             now,  "cubic_slow_start");
    //     }

    //     let cwnd_prev = r.cc.cwnd();

    //     r.on_packet_acked(
    //         &p,
    //         packet::EPOCH_APPLICATION,
    //         r.rtt(),
    //         r.min_rtt,
    //         r.latest_rtt,
    //         r.app_limited,
    //         trace_id,
    //         r.lost_count
    //     );

    //     // Check if cwnd increased by packet size (slow start)
    //     assert_eq!(r.cc.cwnd(), cwnd_prev + p.size);
    // }

    // #[test]
    // fn cubic_slow_start_abc_l() {
    //     let mut cfg = crate::Config::new(crate::PROTOCOL_VERSION).unwrap();
    //     cfg.set_cc_algorithm(recovery::CongestionControlAlgorithm::CUBIC);

    //     let mut r = Recovery::new(&cfg);
    //     let now = Instant::now();

    //     let p = recovery::Sent {
    //         pkt_num: 0,
    //         frames: vec![],
    //         time_sent: now,
    //         time_acked: None,
    //         time_lost: None,
    //         size: r.max_datagram_size,
    //         ack_eliciting: true,
    //         in_flight: true,
    //         delivered: 0,
    //         delivered_time: now,
    //         recent_delivered_packet_sent_time: now,
    //         is_app_limited: false,
    //         has_data: false,
    //     };

    //     // Send initcwnd full MSS packets to become no longer app limited
    //     for _ in 0..recovery::INITIAL_WINDOW_PACKETS {
    //         r.on_packet_sent_cc(p.size, now);
    //     }

    //     let cwnd_prev = r.cwnd();

    //     let acked = vec![
    //         Acked {
    //             pkt_num: p.pkt_num,
    //             time_sent: p.time_sent,
    //             size: p.size * 3,
    //         },
    //         Acked {
    //             pkt_num: p.pkt_num,
    //             time_sent: p.time_sent,
    //             size: p.size * 3,
    //         },
    //         Acked {
    //             pkt_num: p.pkt_num,
    //             time_sent: p.time_sent,
    //             size: p.size * 3,
    //         },
    //     ];

    //     r.on_packets_acked(acked, packet::EPOCH_APPLICATION, now);

    //     // Acked 3 packets, but cwnd will increase 2 x mss.
    //     assert_eq!(r.cwnd(), cwnd_prev + p.size * recovery::ABC_L);
    // }

    // #[test]
    // fn cubic_congestion_event() {
    //     let mut cfg = crate::Config::new(crate::PROTOCOL_VERSION).unwrap();
    //     cfg.set_cc_algorithm(recovery::CongestionControlAlgorithm::CUBIC);

    //     let mut r = Recovery::new(&cfg);
    //     let now = Instant::now();
    //     let prev_cwnd = r.cwnd();

    //     r.congestion_event(now, packet::EPOCH_APPLICATION, now);

    //     // In CUBIC, after congestion event, cwnd will be reduced by (1 -
    //     // CUBIC_BETA)
    //     assert_eq!(prev_cwnd as f64 * BETA_CUBIC, r.cwnd() as f64);
    // }

    // #[test]
    // fn cubic_congestion_avoidance() {
    //     let mut cfg = crate::Config::new(crate::PROTOCOL_VERSION).unwrap();
    //     cfg.set_cc_algorithm(recovery::CongestionControlAlgorithm::CUBIC);

    //     let mut r = Recovery::new(&cfg);
    //     let now = Instant::now();
    //     let prev_cwnd = r.cwnd();

    //     // Send initcwnd full MSS packets to become no longer app limited
    //     for _ in 0..recovery::INITIAL_WINDOW_PACKETS {
    //         r.on_packet_sent_cc(r.max_datagram_size, now);
    //     }

    //     // Trigger congestion event to update ssthresh
    //     r.congestion_event(now, packet::EPOCH_APPLICATION, now);

    //     // After congestion event, cwnd will be reduced.
    //     let cur_cwnd = (prev_cwnd as f64 * BETA_CUBIC) as usize;
    //     assert_eq!(r.cwnd(), cur_cwnd);

    //     let rtt = Duration::from_millis(100);

    //     let acked = vec![Acked {
    //         pkt_num: 0,
    //         // To exit from recovery
    //         time_sent: now + rtt,
    //         size: r.max_datagram_size,
    //     }];

    //     // Ack more than cwnd bytes with rtt=100ms
    //     r.update_rtt(rtt, Duration::from_millis(0), now);

    //     // To avoid rollback
    //     r.lost_count += RESTORE_COUNT_THRESHOLD;

    //     r.on_packets_acked(acked, packet::EPOCH_APPLICATION, now + rtt * 3);

    //     // After acking more than cwnd, expect cwnd increased by MSS
    //     assert_eq!(r.cwnd(), cur_cwnd + r.max_datagram_size);
    // }

    // #[test]
    // fn cubic_collapse_cwnd_and_restart() {
    //     let mut cfg = crate::Config::new(crate::PROTOCOL_VERSION).unwrap();
    //     cfg.set_cc_algorithm(recovery::CongestionControlAlgorithm::CUBIC);

    //     let mut r = Recovery::new(&cfg);
    //     let now = Instant::now();

    //     // Fill up bytes_in_flight to avoid app_limited=true
    //     r.on_packet_sent_cc(30000, now);

    //     // Trigger congestion event to update ssthresh
    //     r.congestion_event(now, packet::EPOCH_APPLICATION, now);

    //     // After persistent congestion, cwnd should be the minimum window
    //     r.collapse_cwnd();
    //     assert_eq!(
    //         r.cwnd(),
    //         r.max_datagram_size * recovery::MINIMUM_WINDOW_PACKETS
    //     );

    //     let acked = vec![Acked {
    //         pkt_num: 0,
    //         // To exit from recovery
    //         time_sent: now + Duration::from_millis(1),
    //         size: r.max_datagram_size,
    //     }];

    //     r.on_packets_acked(acked, packet::EPOCH_APPLICATION, now);

    //     // Slow start again - cwnd will be increased by 1 MSS
    //     assert_eq!(
    //         r.cwnd(),
    //         r.max_datagram_size * (recovery::MINIMUM_WINDOW_PACKETS + 1)
    //     );
    // }

    // #[test]
    // fn cubic_hystart_limited_slow_start() {
    //     let mut cfg = crate::Config::new(crate::PROTOCOL_VERSION).unwrap();
    //     cfg.set_cc_algorithm(recovery::CongestionControlAlgorithm::CUBIC);
    //     cfg.enable_hystart(true);

    //     let mut r = Recovery::new(&cfg);
    //     let now = Instant::now();
    //     let pkt_num = 0;
    //     let epoch = packet::EPOCH_APPLICATION;

    //     let p = recovery::Sent {
    //         pkt_num: 0,
    //         frames: vec![],
    //         time_sent: now,
    //         time_acked: None,
    //         time_lost: None,
    //         size: r.max_datagram_size,
    //         ack_eliciting: true,
    //         in_flight: true,
    //         delivered: 0,
    //         delivered_time: now,
    //         recent_delivered_packet_sent_time: now,
    //         is_app_limited: false,
    //         has_data: false,
    //     };

    //     // 1st round.
    //     let n_rtt_sample = hystart::N_RTT_SAMPLE;
    //     let pkts_1st_round = n_rtt_sample as u64;
    //     r.hystart.start_round(pkt_num);

    //     let rtt_1st = 50;

    //     // Send 1st round packets.
    //     for _ in 0..n_rtt_sample {
    //         r.on_packet_sent_cc(p.size, now);
    //     }

    //     // Receving Acks.
    //     let now = now + Duration::from_millis(rtt_1st);
    //     for _ in 0..n_rtt_sample {
    //         r.update_rtt(
    //             Duration::from_millis(rtt_1st),
    //             Duration::from_millis(0),
    //             now,
    //         );

    //         let acked = vec![Acked {
    //             pkt_num: p.pkt_num,
    //             time_sent: p.time_sent,
    //             size: p.size,
    //         }];

    //         r.on_packets_acked(acked, epoch, now);
    //     }

    //     // Not in LSS yet.
    //     assert_eq!(r.hystart.lss_start_time().is_some(), false);

    //     // 2nd round.
    //     r.hystart.start_round(pkts_1st_round * 2);

    //     let mut rtt_2nd = 100;
    //     let now = now + Duration::from_millis(rtt_2nd);

    //     // Send 2nd round packets.
    //     for _ in 0..n_rtt_sample {
    //         r.on_packet_sent_cc(p.size, now);
    //     }

    //     // Receving Acks.
    //     // Last ack will cause to exit to LSS.
    //     let mut cwnd_prev = r.cwnd();

    //     for _ in 0..n_rtt_sample {
    //         cwnd_prev = r.cwnd();
    //         r.update_rtt(
    //             Duration::from_millis(rtt_2nd),
    //             Duration::from_millis(0),
    //             now,
    //         );

    //         let acked = vec![Acked {
    //             pkt_num: p.pkt_num,
    //             time_sent: p.time_sent,
    //             size: p.size,
    //         }];

    //         r.on_packets_acked(acked, epoch, now);

    //         // Keep increasing RTT so that hystart exits to LSS.
    //         rtt_2nd += 4;
    //     }

    //     // Now we are in LSS.
    //     assert_eq!(r.hystart.lss_start_time().is_some(), true);
    //     assert_eq!(r.cwnd(), cwnd_prev + r.max_datagram_size);

    //     // Send a full cwnd.
    //     r.on_packet_sent_cc(r.cwnd(), now);

    //     // Ack'ing 4 packets to increase cwnd by 1 MSS during LSS
    //     cwnd_prev = r.cwnd();
    //     for _ in 0..4 {
    //         let acked = vec![Acked {
    //             pkt_num: p.pkt_num,
    //             time_sent: p.time_sent,
    //             size: p.size,
    //         }];
    //         r.on_packets_acked(acked, epoch, now);
    //     }

    //     // During LSS cwnd will be increased less than usual slow start.
    //     assert_eq!(r.cwnd(), cwnd_prev + r.max_datagram_size);
    // }

    // #[test]
    // fn cubic_spurious_congestion_event() {
    //     let mut cfg = crate::Config::new(crate::PROTOCOL_VERSION).unwrap();
    //     cfg.set_cc_algorithm(recovery::CongestionControlAlgorithm::CUBIC);

    //     let mut r = Recovery::new(&cfg);
    //     let now = Instant::now();
    //     let prev_cwnd = r.cwnd();

    //     // Send initcwnd full MSS packets to become no longer app limited
    //     for _ in 0..recovery::INITIAL_WINDOW_PACKETS {
    //         r.on_packet_sent_cc(r.max_datagram_size, now);
    //     }

    //     // Trigger congestion event to update ssthresh
    //     r.congestion_event(now, packet::EPOCH_APPLICATION, now);

    //     // After congestion event, cwnd will be reduced.
    //     let cur_cwnd = (prev_cwnd as f64 * BETA_CUBIC) as usize;
    //     assert_eq!(r.cwnd(), cur_cwnd);

    //     let rtt = Duration::from_millis(100);

    //     let acked = vec![Acked {
    //         pkt_num: 0,
    //         // To exit from recovery
    //         time_sent: now + rtt,
    //         size: r.max_datagram_size,
    //     }];

    //     // Ack more than cwnd bytes with rtt=100ms
    //     r.update_rtt(rtt, Duration::from_millis(0), now);

    //     // Trigger detecting sprurious congestion event
    //     r.on_packets_acked(
    //         acked,
    //         packet::EPOCH_APPLICATION,
    //         now + rtt + Duration::from_millis(5),
    //     );

    //     // cwnd is restored to the previous one.
    //     assert_eq!(r.cwnd(), prev_cwnd);
    // }
}

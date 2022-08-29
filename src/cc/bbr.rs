// Copyright (C) 2018, Cloudflare, Inc.
// Copyright (C) 2018, Alessandro Ghedini
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

#![allow(non_camel_case_types, non_upper_case_globals)]
extern crate bitflags;
extern crate rand;
use rand::Rng;
use std::time;

use crate::recovery::Sent;

// use std::convert::TryInto;

use std::cmp;

use std::time::Duration;
use std::time::Instant;

use std::collections::BTreeMap;
use std::collections::VecDeque;

// use crate::frame;
use crate::packet;
// use crate::ranges;

use crate::cc;

// // Loss Recovery
// const PACKET_THRESHOLD: u64 = 3;

// const GRANULARITY: Duration = Duration::from_millis(1);

// const INITIAL_RTT: Duration = Duration::from_millis(500);

// // Congestion Control
// pub const INITIAL_WINDOW_PACKETS: usize = 10;

// const MAX_DATAGRAM_SIZE: usize = 1452;

// const INITIAL_WINDOW: usize = INITIAL_WINDOW_PACKETS * MAX_DATAGRAM_SIZE;
// const MINIMUM_WINDOW: usize = 2 * MAX_DATAGRAM_SIZE;

// const PERSISTENT_CONGESTION_THRESHOLD: u32 = 3;

// #[derive(Debug)]
// pub struct Sent {
//     pub pkt_num: u64,

//     pub frames: Vec<frame::Frame>,

//     pub time: Instant,

//     pub size: usize,

//     pub ack_eliciting: bool,

//     pub in_flight: bool,

//     pub is_crypto: bool,
// }

// pub struct Recovery {
//     loss_detection_timer: Option<Instant>, // connection

//     crypto_count: u32, // connection - Loss detection

//     pto_count: u32, // connection - Loss detection

//     time_of_last_sent_ack_eliciting_pkt: Instant,

//     time_of_last_sent_crypto_pkt: Instant,

//     largest_acked_pkt: [u64; packet::EPOCH_COUNT],

//     latest_rtt: Duration,

//     smoothed_rtt: Option<Duration>,

//     rttvar: Duration,

//     min_rtt: Duration,

//     pub max_ack_delay: Duration,

//     loss_time: [Option<Instant>; packet::EPOCH_COUNT], /* connection - Loss
//                                                         * detection */
//     sent: [BTreeMap<u64, Sent>; packet::EPOCH_COUNT], // connection

//     pub lost: [Vec<frame::Frame>; packet::EPOCH_COUNT], // connection

//     pub acked: [Vec<frame::Frame>; packet::EPOCH_COUNT], // connection

//     pub lost_count: usize, // connection - Loss detection

//     bytes_in_flight: usize,

//     crypto_bytes_in_flight: usize,

//     pub cwnd: usize,

//     recovery_start_time: Option<Instant>,

//     ssthresh: usize,

//     pub probes: usize,

//     pub bbr: BBR,
// }

// impl Default for Recovery {
//     fn default() -> Recovery {
//         let now = Instant::now();

//         Recovery {
//             loss_detection_timer: None,

//             crypto_count: 0,

//             pto_count: 0,

//             time_of_last_sent_crypto_pkt: now,

//             time_of_last_sent_ack_eliciting_pkt: now,

//             largest_acked_pkt: [std::u64::MAX; packet::EPOCH_COUNT],

//             latest_rtt: Duration::new(0, 0),

//             smoothed_rtt: None,

//             min_rtt: Duration::new(0, 0),

//             rttvar: Duration::new(0, 0),

//             max_ack_delay: Duration::from_millis(25),

//             loss_time: [None; packet::EPOCH_COUNT],

//             sent: [BTreeMap::new(), BTreeMap::new(), BTreeMap::new()], /*
// Connection */
//             lost: [Vec::new(), Vec::new(), Vec::new()], // Connection

//             acked: [Vec::new(), Vec::new(), Vec::new()], //  Connection

//             lost_count: 0,

//             bytes_in_flight: 0,

//             crypto_bytes_in_flight: 0,

//             cwnd: INITIAL_WINDOW,

//             recovery_start_time: None,

//             ssthresh: std::usize::MAX,

//             probes: 0,

//             bbr: BBR::default(),
//         }
//     }
// }

// impl Recovery {
//     pub fn on_packet_sent(
//         &mut self, pkt: Sent, epoch: packet::Epoch, now: Instant, trace_id:
// &str,     ) {
//         let pkt_num = pkt.pkt_num;
//         let ack_eliciting = pkt.ack_eliciting;
//         let in_flight = pkt.in_flight;
//         // let is_crypto = pkt.is_crypto;
//         let sent_bytes = pkt.size;

//         let mut packet_out = PacketOut {
//             po_sent_time: pkt.time,
//             po_packno: pkt.pkt_num,
//             size: pkt.size as u64,
//             // sent: &pkt,
//             po_bwp_state: None,
//             po_sent: pkt.time,
//         };
//         let app_limit = false;

//         self.bbr.bbr_sent(
//             &mut packet_out,
//             self.bytes_in_flight as u64, // supposed to be number of packets
// in flight. But it only use to initialize. Only compared to zero.
// app_limit,             pkt_num,
//         );

//         self.bbr.packet_out_que.insert(pkt_num, packet_out);
//         self.sent[epoch].insert(pkt_num, pkt);

//         if in_flight {
//             if is_crypto {
//                 self.time_of_last_sent_crypto_pkt = now;

//                 self.crypto_bytes_in_flight += sent_bytes;
//             }

//             if ack_eliciting {
//                 self.time_of_last_sent_ack_eliciting_pkt = now;
//             }

//             // OnPacketSentCC
//             self.bytes_in_flight += sent_bytes;

//             self.set_loss_detection_timer();
//         }

//         trace!("{} {:?}", trace_id, self);
//     }

//     pub fn on_ack_received(
//         &mut self, ranges: &ranges::RangeSet, ack_delay: u64,
//         epoch: packet::Epoch, now: Instant, trace_id: &str,
//     ) {
//         self.bbr
//             .bbr_begin_ack(now, self.bytes_in_flight.try_into().unwrap());
//         let largest_acked = ranges.largest().unwrap();

//         if self.largest_acked_pkt[epoch] == std::u64::MAX {
//             self.largest_acked_pkt[epoch] = largest_acked;
//         } else {
//             self.largest_acked_pkt[epoch] =
//                 cmp::max(self.largest_acked_pkt[epoch], largest_acked);
//         }

//         if let Some(pkt) =
// self.sent[epoch].get(&self.largest_acked_pkt[epoch]) {             if
// pkt.ack_eliciting {                 let latest_rtt = now - pkt.time;

//                 let ack_delay = if epoch == packet::EPOCH_APPLICATION {
//                     Duration::from_micros(ack_delay)
//                 } else {
//                     Duration::from_micros(0)
//                 };

//                 self.update_rtt(latest_rtt, ack_delay);
//             }
//         }

//         let mut has_newly_acked = false;

//         // Processing ACKed packets in reverse order (from largest to
// smallest)         // appears to be faster, possibly due to the BTreeMap
// implementation.         // changed it to match with BBR implementation.
//         for pn in ranges.flatten() {
//             let newly_acked = self.on_packet_acked(pn, epoch);
//             has_newly_acked = cmp::max(has_newly_acked, newly_acked);
//             if newly_acked {
//                 trace!("{} packet newly acked {}", trace_id, pn);
//             }
//         }

//         if !has_newly_acked {
//             return;
//         }

//         self.bbr
//             .bbr_end_ack(self.bytes_in_flight.try_into().unwrap());
//         self.detect_lost_packets(epoch, now, trace_id);

//         self.crypto_count = 0;
//         self.pto_count = 0;

//         self.set_loss_detection_timer();

//         trace!("{} {:?}", trace_id, self);
//     }

//     pub fn on_loss_detection_timeout(&mut self, now: Instant, trace_id: &str)
// {         let (loss_time, epoch) = self.earliest_loss_time();

//         if loss_time.is_some() {
//             self.detect_lost_packets(epoch, now, trace_id);
//         } else if self.crypto_bytes_in_flight > 0 {
//             // Retransmit unacked data from all packet number spaces.
//             for e in packet::EPOCH_INITIAL..packet::EPOCH_COUNT {
//                 for p in self.sent[e].values().filter(|p| p.is_crypto) {
//                     self.lost[e].extend_from_slice(&p.frames);
//                 }
//             }

//             trace!("{} resend unacked crypto data ({:?})", trace_id, self);

//             self.crypto_count += 1;
//         } else {
//             self.pto_count += 1;
//             self.probes = 2;
//         }

//         self.set_loss_detection_timer();

//         trace!("{} {:?}", trace_id, self);
//     }

//     pub fn drop_unacked_data(&mut self, epoch: packet::Epoch) {
//         let mut unacked_bytes = 0;
//         let mut crypto_unacked_bytes = 0;

//         for p in self.sent[epoch].values_mut().filter(|p| p.in_flight) {
//             unacked_bytes += p.size;

//             if p.is_crypto {
//                 crypto_unacked_bytes += p.size;
//             }
//         }

//         self.crypto_bytes_in_flight -= crypto_unacked_bytes;
//         self.bytes_in_flight -= unacked_bytes;

//         self.sent[epoch].clear();
//         self.lost[epoch].clear();
//         self.acked[epoch].clear();
//     }

//     pub fn loss_detection_timer(&self) -> Option<Instant> {
//         self.loss_detection_timer
//     }

//     #[allow(dead_code)]
//     pub fn cwnd(&self) -> usize {
//         // Ignore cwnd when sending probe packets.
//         if self.probes > 0 {
//             return std::usize::MAX;
//         }

//         if self.bytes_in_flight > self.bbr.bbr_get_cwnd().try_into().unwrap()
// {             return 0;
//         }

//         self.bbr.bbr_get_cwnd() as usize - self.bytes_in_flight
//     }

//     pub fn rtt(&self) -> Duration {
//         self.smoothed_rtt.unwrap_or(INITIAL_RTT)
//     }

//     pub fn pto(&self) -> Duration {
//         self.rtt() + cmp::max(self.rttvar * 4, GRANULARITY) +
// self.max_ack_delay     }

//     fn update_rtt(&mut self, latest_rtt: Duration, ack_delay: Duration) {
//         self.latest_rtt = latest_rtt;

//         match self.smoothed_rtt {
//             // First RTT sample.
//             None => {
//                 self.min_rtt = latest_rtt;

//                 self.smoothed_rtt = Some(latest_rtt);

//                 self.rttvar = latest_rtt / 2;
//             },

//             Some(srtt) => {
//                 self.min_rtt = cmp::min(self.min_rtt, latest_rtt);

//                 let ack_delay = cmp::min(self.max_ack_delay, ack_delay);

//                 // Adjust for ack delay if plausible.
//                 let adjusted_rtt = if latest_rtt > self.min_rtt + ack_delay {
//                     latest_rtt - ack_delay
//                 } else {
//                     latest_rtt
//                 };

//                 self.rttvar = (self.rttvar * 3 + sub_abs(srtt, adjusted_rtt))
// / 4;

//                 self.smoothed_rtt = Some((srtt * 7 + adjusted_rtt) / 8);
//             },
//         }
//     }

//     fn earliest_loss_time(&mut self) -> (Option<Instant>, packet::Epoch) {
//         let mut epoch = packet::EPOCH_INITIAL;
//         let mut time = self.loss_time[epoch];

//         for e in packet::EPOCH_HANDSHAKE..packet::EPOCH_COUNT {
//             let new_time = self.loss_time[e];

//             if new_time.is_some() && (time.is_none() || new_time < time) {
//                 time = new_time;
//                 epoch = e;
//             }
//         }

//         (time, epoch)
//     }

//     fn set_loss_detection_timer(&mut self) {
//         let (loss_time, _) = self.earliest_loss_time();
//         if loss_time.is_some() {
//             // Time threshold loss detection.
//             self.loss_detection_timer = loss_time;
//             return;
//         }

//         if self.crypto_bytes_in_flight > 0 {
//             // Crypto retransmission timer.
//             let mut timeout = self.rtt() * 2;

//             timeout = cmp::max(timeout, GRANULARITY);
//             timeout *= 2_u32.pow(self.crypto_count);

//             self.loss_detection_timer =
//                 Some(self.time_of_last_sent_crypto_pkt + timeout);

//             return;
//         }

//         if self.bytes_in_flight == 0 {
//             self.loss_detection_timer = None;
//             return;
//         }

//         // PTO timer.
//         let timeout = self.pto() * 2_u32.pow(self.pto_count);

//         self.loss_detection_timer =
//             Some(self.time_of_last_sent_ack_eliciting_pkt + timeout);
//     }

//     fn detect_lost_packets(
//         &mut self, epoch: packet::Epoch, now: Instant, trace_id: &str,
//     ) {
//         let mut lost_pkt: Vec<u64> = Vec::new();

//         let largest_acked = self.largest_acked_pkt[epoch];

//         let loss_delay = (cmp::max(self.latest_rtt, self.rtt()) * 9) / 8;
//         let loss_delay = cmp::max(loss_delay, GRANULARITY);

//         let lost_send_time = now - loss_delay;

//         self.loss_time[epoch] = None;

//         for (_, unacked) in self.sent[epoch].range(..=largest_acked) {
//             if unacked.time <= lost_send_time ||
//                 largest_acked >= unacked.pkt_num + PACKET_THRESHOLD
//             {
//                 if unacked.time <= lost_send_time {
//                     trace!("judge lost by time");
//                 }
//                 if largest_acked >= unacked.pkt_num + PACKET_THRESHOLD {
//                     trace!("judge lost by 3 ack");
//                 }
//                 trace!(
//                     "largest_acked={},unacked.pkt_num={}",
//                     largest_acked,
//                     unacked.pkt_num
//                 );
//                 trace!(
//                     "loss_delay={},unack_duration={}",
//                     loss_delay.as_millis(),
//                     now.duration_since(unacked.time).as_millis()
//                 );
//                 if unacked.in_flight {
//                     trace!(
//                         "{} packet {} lost on epoch {}",
//                         trace_id,
//                         unacked.pkt_num,
//                         epoch
//                     );
//                 }

//                 // We can't remove the lost packet from |self.sent| here, so
//                 // simply keep track of the number so it can be removed
// later.                 lost_pkt.push(unacked.pkt_num);
//             } else {
//                 let loss_time = match self.loss_time[epoch] {
//                     None => unacked.time + loss_delay,

//                     Some(loss_time) =>
//                         cmp::min(loss_time, unacked.time + loss_delay),
//                 };

//                 self.loss_time[epoch] = Some(loss_time);
//             }
//         }

//         if !lost_pkt.is_empty() {
//             self.on_packets_lost(lost_pkt, epoch, now);
//         }
//     }

//     fn in_recovery(&self, sent_time: Instant) -> bool {
//         match self.recovery_start_time {
//             Some(recovery_start_time) => sent_time <= recovery_start_time,

//             None => false,
//         }
//     }

//     fn on_packet_acked(&mut self, pkt_num: u64, epoch: packet::Epoch) -> bool
// {         // Check if packet is newly acked.
//         if let Some(mut p) = self.sent[epoch].remove(&pkt_num) {
//             self.acked[epoch].append(&mut p.frames);

//             if p.in_flight {
//                 // OnPacketAckedCC
//                 let mut packet_out =
//                     match self.bbr.packet_out_que.remove(&pkt_num) {
//                         Some(v) => v,
//                         None => return false,
//                     };
//                 self.bbr.bbr_ack(
//                     &mut packet_out,
//                     p.size as u64,
//                     Instant::now(),
//                     false,
//                 );
//                 self.bytes_in_flight -= p.size;

//                 if p.is_crypto {
//                     self.crypto_bytes_in_flight -= p.size;
//                 }

//                 if self.in_recovery(p.time) {
//                     return true;
//                 }

//                 if self.cwnd < self.ssthresh {
//                     // Slow start.
//                     self.cwnd += p.size;
//                 } else {
//                     // Congestion avoidance.
//                     self.cwnd += (MAX_DATAGRAM_SIZE * p.size) / self.cwnd;
//                 }
//             }

//             return true;
//         }

//         // Is not newly acked.
//         false
//     }

//     fn in_persistent_congestion(&mut self, _largest_lost_pkt: &Sent) -> bool
// {         let _congestion_period = self.pto() *
// PERSISTENT_CONGESTION_THRESHOLD;

//         // TODO: properly detect persistent congestion
//         false
//     }

//     fn on_packets_lost(
//         &mut self, lost_pkt: Vec<u64>, epoch: packet::Epoch, now: Instant,
//     ) {
//         // Differently from OnPacketsLost(), we need to handle both
//         // in-flight and non-in-flight packets, so need to keep track
//         // of whether we saw any lost in-flight packet to trigger the
//         // congestion event later.
//         let mut largest_lost_pkt: Option<Sent> = None;

//         for lost in lost_pkt {
//             let mut p = self.sent[epoch].remove(&lost).unwrap();

//             self.lost_count += 1;

//             if !p.in_flight {
//                 continue;
//             }

//             self.bytes_in_flight -= p.size;

//             if p.is_crypto {
//                 self.crypto_bytes_in_flight -= p.size;
//             }

//             self.lost[epoch].append(&mut p.frames);

//             largest_lost_pkt = Some(p);
//         }

//         if let Some(largest_lost_pkt) = largest_lost_pkt {
//             // CongestionEvent
//             if !self.in_recovery(largest_lost_pkt.time) {
//                 self.recovery_start_time = Some(now);

//                 self.cwnd /= 2;
//                 self.cwnd = cmp::max(self.cwnd, MINIMUM_WINDOW);
//                 self.ssthresh = self.cwnd;
//             }

//             if self.in_persistent_congestion(&largest_lost_pkt) {
//                 self.cwnd = MINIMUM_WINDOW;
//             }
//         }
//     }
// }

// impl std::fmt::Debug for Recovery {
//     fn fmt(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
//         match self.loss_detection_timer {
//             Some(v) => {
//                 let now = Instant::now();

//                 if v > now {
//                     let d = v.duration_since(now);
//                     write!(f, "timer={:?} ", d)?;
//                 } else {
//                     write!(f, "timer=exp ")?;
//                 }
//             },

//             None => {
//                 write!(f, "timer=none ")?;
//             },
//         };

//         write!(f, "crypto={} ", self.crypto_bytes_in_flight)?;
//         write!(f, "inflight={} ", self.bytes_in_flight)?;
//         write!(f, "cwnd={} ", self.cwnd)?;
//         write!(f, "latest_rtt={:?} ", self.latest_rtt)?;
//         write!(f, "srtt={:?} ", self.smoothed_rtt)?;
//         write!(f, "min_rtt={:?} ", self.min_rtt)?;
//         write!(f, "rttvar={:?} ", self.rttvar)?;
//         write!(f, "probes={} ", self.probes)?;

//         Ok(())
//     }
// }

// fn sub_abs(lhs: Duration, rhs: Duration) -> Duration {
//     if lhs > rhs {
//         lhs - rhs
//     } else {
//         rhs - lhs
//     }
// }

// bbr
const kDefaultTCPMSS: u64 = 1460;
const kMaxSegmentSize: u64 = kDefaultTCPMSS;

const kDefaultMinimumCongestionWindow: u64 = 4 * kDefaultTCPMSS;

const kDefaultHighGain: f64 = 2.885; // 2/ln2

// never used
// const kDerivedHighGain: f64 = 2.773; // 4 * ln2

// The newly derived CWND gain for STARTUP, 2
// never used
// const kDerivedHighCWNDGain: f64 = 2.0;

// The gain used in STARTUP after loss has been detected.
// 1.5 is enough to allow for 25% exogenous loss and still observe a 25% growth
// in measured bandwidth.
const kStartupAfterLossGain: f64 = 1.5;

// We match SPDY's use of 32 (since we'd compete with SPDY).
const kInitialCongestionWindow: u64 = 32;

// Taken from send_algorithm_interface.h
const kDefaultMaxCongestionWindowPackets: u64 = 2000;

// The time after which the current min_rtt value expires.
const kMinRttExpiry: Duration = Duration::from_secs(10); // origin 10

// Coefficient to determine if a new RTT is sufficiently similar to min_rtt that
// we don't need to enter PROBE_RTT.
const kSimilarMinRttThreshold: f64 = 1.125;

// If the bandwidth does not increase by the factor of |kStartupGrowthTarget|
// within |kRoundTripsWithoutGrowthBeforeExitingStartup| rounds, the connection
// will exit the STARTUP mode.
const kStartupGrowthTarget: f64 = 1.25;

const kRoundTripsWithoutGrowthBeforeExitingStartup: u64 = 3;

const startup_rate_reduction_multiplier_: u64 = 0;

// The cycle of gains used during the PROBE_BW stage.
// Thel length of the gain cycle
const kGainCycleLength: usize = 8;

const kPacingGain: [f64; kGainCycleLength] =
    [1.25, 0.75, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0];

// Coefficient of target congestion window to use when basing PROBE_RTT on BDP.
const kModerateProbeRttMultiplier: f64 = 0.75;

// The maximum packet size of any QUIC packet over IPv6, based on ethernet's max
// size, minus the IP and UDP headers. IPv6 has a 40 byte header, UDP adds an
// additional 8 bytes.  This is a total overhead of 48 bytes.  Ethernet's
// max packet size is 1500 bytes,  1500 - 48 = 1452.
const kMaxV6PacketSize: u64 = 1452;
// The maximum packet size of any QUIC packet over IPv4.
// 1500(Ethernet) - 20(IPv4 header) - 8(UDP header) = 1472.
// const kMaxV4PacketSize: u64 = 1472;//never used
// The maximum incoming packet size allowed.
// const kMaxIncomingPacketSize: u64 = kMaxV4PacketSize;// never used
// The maximum outgoing packet size allowed.
const kMaxOutgoingPacketSize: u64 = kMaxV6PacketSize;

// The minimum time the connection can spend in PROBE_RTT mode.
const kProbeRttTime: Duration = Duration::from_millis(200);

// /* FLAG* are from net/quic/quic_flags_list.h */
// When in STARTUP and recovery, do not add bytes_acked to QUIC BBR's CWND in
// CalculateCongestionWindow()
const FLAGS_quic_bbr_no_bytes_acked_in_startup_recovery: bool = false;

// When true, ensure BBR allows at least one MSS to be sent in response to an
// ACK in packet conservation.
// const FLAG_quic_bbr_one_mss_conservation: u64 = 0;//never used

// From net/quic/quic_flags_list.h
const kCwndGain: f64 = 2.0;

#[derive(Copy, Clone, Default)]
pub struct MinmaxSample {
    pub time: u64,
    pub value: u64,
}

pub struct Minmax {
    pub window: u64,
    pub samples: [MinmaxSample; 3],
}

impl Minmax {
    pub fn new(window: u64) -> Minmax {
        Minmax {
            window,
            samples: Default::default(),
        }
    }

    pub fn minmax_get_idx(&self, idx: usize) -> u64 {
        self.samples[idx].value
    }

    pub fn minmax_get(&self) -> u64 {
        return self.minmax_get_idx(0);
    }

    pub fn minmax_reset(&mut self, sample: MinmaxSample) {
        self.samples = [sample; 3];
    }

    pub fn minmax_upmax(&mut self, now: u64, meas: u64) {
        let sample = MinmaxSample {
            time: now,
            value: meas,
        };

        if self.samples[0].value == 0
            || sample.value >= self.samples[0].value
            || sample.time - self.samples[2].time > self.window
        {
            self.minmax_reset(sample);
            return;
        }

        if sample.value >= self.samples[1].value {
            self.samples[2] = sample;
            self.samples[1] = sample;
        } else if sample.value >= self.samples[2].value {
            self.samples[2] = sample;
        }

        self.minmax_subwin_update(sample);
    }

    pub fn minmax_subwin_update(&mut self, sample: MinmaxSample) {
        let dt = sample.time - self.samples[0].time;

        if dt > self.window {
            // Passed entire window without a new sample so make 2nd
            // choice the new sample & 3rd choice the new 2nd choice.
            // we may have to iterate this since our 2nd choice
            // may also be outside the window (we checked on entry
            // that the third choice was in the window).
            self.samples[0] = self.samples[1];
            self.samples[1] = self.samples[2];
            self.samples[2] = sample;
            if sample.time - self.samples[0].time > self.window {
                self.samples[0] = self.samples[1];
                self.samples[1] = self.samples[2];
                self.samples[2] = sample;
            }
        } else if self.samples[1].time == self.samples[0].time
            && dt > self.window / 4
        {
            // We've passed a quarter of the window without a new sample
            // so take a 2nd choice from the 2nd quarter of the window.
            self.samples[2] = sample;
            self.samples[1] = sample;
        } else if self.samples[2].time == self.samples[1].time
            && dt > self.window / 2
        {
            // We've passed half the window without finding a new sample
            // so take a 3rd choice from the last half of the window
            self.samples[2] = sample;
        }
    }
}

pub struct RttStatus {
    // srtt: Duration,
    // rttvar: Duration,
    min_rtt: Duration,
}

impl Default for RttStatus {
    fn default() -> RttStatus {
        RttStatus {
            // srtt: Duration::new(0, 0),
            // rttvar: Duration::new(0, 0),
            min_rtt: Duration::new(0, 0),
        }
    }
}

impl RttStatus {
    // fn rtt_stats_get_srtt(&mut self) -> Duration {
    //     return self.srtt;
    // }

    // fn rtt_stats_get_rtt_var(&mut self) -> Duration {
    //     return self.rttvar;
    // }

    fn rtt_stats_get_min_rtt(&self) -> Duration {
        return self.min_rtt;
    }

    // fn rtt_stats_update(send_delta: Duration, lack_delta: Duration) {
    // noop
    // }
}

pub struct BbrAckState {
    samples: VecDeque<BwSample>,
    ack_time: Instant,
    max_packno: u64,
    ack_bytes: u64,
    lost_bytes: u64,
    total_bytes_acked_before: u64,
    in_flight: u64,
    has_losses: bool,
}

impl Default for BbrAckState {
    fn default() -> BbrAckState {
        BbrAckState {
            samples: VecDeque::new(),
            ack_time: Instant::now(),
            max_packno: 0,
            ack_bytes: 0,
            lost_bytes: 0,
            in_flight: 0,
            total_bytes_acked_before: 0,
            has_losses: false,
        }
    }
}

impl BbrAckState {
    // maybe todo
    fn bbr_ack_state_init(&mut self) {
        self.samples = VecDeque::new();
        self.ack_time = Instant::now();
        self.max_packno = 0;
        self.ack_bytes = 0;
        self.lost_bytes = 0;
        self.total_bytes_acked_before = 0;
        self.has_losses = false;
    }
}

#[derive(PartialEq, Debug)]
enum BbrMode {
    StartUp,
    Drain,
    ProbeBw,
    ProbeRtt,
}

#[derive(PartialEq)]
enum Bbr_recovery_state {
    BBR_RS_NOT_IN_RECOVERY,
    BBR_RS_CONSERVATION,
    BBR_RS_GROWTH,
}

bitflags::bitflags! {
struct BbrFlags: u64 {
    const BBR_FLAG_IN_ACK = 0b1 << 0; // cci_begin_ack() has been called
    const BBR_FLAG_LAST_SAMPLE_APP_LIMITED = 0b1 << 1;
    const BBR_FLAG_HAS_NON_APP_LIMITED = 0b1 << 2;
    const BBR_FLAG_APP_LIMITED_SINCE_LAST_PROBE_RTT = 0b1 << 3;
    const BBR_FLAG_PROBE_RTT_DISABLED_IF_APP_LIMITED = 0b1 << 4;
    const BBR_FLAG_PROBE_RTT_SKIPPED_IF_SIMILAR_RTT = 0b1 << 5;
    const BBR_FLAG_EXIT_STARTUP_ON_LOSS = 0b1 << 6;
    const BBR_FLAG_IS_AT_FULL_BANDWIDTH = 0b1 << 7;
    const BBR_FLAG_EXITING_QUIESCENCE = 0b1 << 8;
    const BBR_FLAG_PROBE_RTT_ROUND_PASSED = 0b1 << 9;
    const BBR_FLAG_FLEXIBLE_APP_LIMITED = 0b1 << 10;
    // If true; will not exit low gain mode until bytes_in_flight drops
    // below BDP or it's time for high gain mode.
    const BBR_FLAG_Drain_TO_TARGET = 0b1 << 11;
    // When true; expire the windowed ack aggregation values in STARTUP
    // when bandwidth increases more than 25%.
    const BBR_FLAG_EXPIRE_ACK_AGG_IN_STARTUP = 0b1 << 12;
    // If true; use a CWND of 0.75*BDP during probe_rtt instead of 4
    // packets.
    const BBR_FLAG_PROBE_RTT_BASED_ON_BDP = 0b1 << 13;
    // When true; pace at 1.5x and disable packet conservation in STARTUP.
    const BBR_FLAG_SLOWER_STARTUP = 0b1 << 14;
    // When true; add the most recent ack aggregation measurement during
    // STARTUP.
    const BBR_FLAG_ENABLE_ACK_AGG_IN_STARTUP = 0b1 << 15;
    // When true; disables packet conservation in STARTUP.
    const BBR_FLAG_RATE_BASED_STARTUP = 0b1 << 16;
}
}

#[allow(non_snake_case)]
pub struct BBR {
    bbr_bw_sampler: BwSampler,

    bbr_ack_state: BbrAckState,

    bbr_rtt_stats: RttStatus,

    bbr_mode: BbrMode,

    bbr_bytes_in_flight: u64,

    // bbr_rtt: Duration,
    bbr_round_count: u64,

    bbr_max_bandwidth: Minmax,

    bbr_max_ack_height: Minmax,

    bbr_aggregation_epoch_bytes: u64,

    bbr_aggregation_epoch_start_time: Instant,

    bbr_min_rtt: Duration,

    bbr_min_rtt_timestamp: Instant,

    bbr_init_cwnd: u64,

    bbr_cwnd: u64,

    bbr_max_cwnd: u64,

    bbr_min_cwnd: u64,

    bbr_high_gain: f64,

    bbr_high_cwnd_gain: f64,

    bbr_drain_gain: f64,

    bbr_pacing_rate: Bandwidth,

    bbr_pacing_gain: f64,

    bbr_cwnd_gain: f64,

    bbr_num_startup_rtts: u64,

    bbr_cycle_current_offset: usize,

    bbr_round_wo_bw_gain: u64,

    bbr_bw_at_last_round: Bandwidth,

    bbr_exit_probe_rtt_at: Option<Instant>,

    bbr_flags: BbrFlags,

    bbr_recovery_window: u64,

    bbr_last_sent_packno: u64,

    bbr_recovery_state: Bbr_recovery_state,

    bbr_current_round_trip_end: u64,

    bbr_end_recovery_at: u64,

    bbr_min_rtt_since_last_probe: Duration,

    bbr_last_cycle_start: Instant,

    // bbr_num_startup_rtts: u64,
    bbr_startup_bytes_lost: u64,

    packet_out_que: BTreeMap<u64, PacketOut>,
}

impl Default for BBR {
    fn default() -> BBR {
        let now = Instant::now();

        BBR {
            // bbr
            bbr_bw_sampler: BwSampler::default(),

            bbr_rtt_stats: RttStatus::default(),

            bbr_ack_state: BbrAckState::default(),

            bbr_mode: BbrMode::StartUp,

            bbr_bytes_in_flight: 0,

            bbr_round_count: 0,

            bbr_max_bandwidth: Minmax::new(10),

            bbr_max_ack_height: Minmax::new(10),

            bbr_aggregation_epoch_bytes: 0,

            bbr_aggregation_epoch_start_time: now, // maybe need Option::None

            bbr_min_rtt: Duration::from_micros(0),

            bbr_min_rtt_timestamp: now, // maybe need Option::None

            bbr_init_cwnd: kInitialCongestionWindow * kDefaultTCPMSS,

            bbr_cwnd: kInitialCongestionWindow * kDefaultTCPMSS,

            bbr_max_cwnd: kDefaultMaxCongestionWindowPackets * kDefaultTCPMSS,

            bbr_min_cwnd: kDefaultMinimumCongestionWindow,

            bbr_high_gain: kDefaultHighGain,

            bbr_high_cwnd_gain: kDefaultHighGain,

            bbr_drain_gain: 1.0 / kDefaultHighGain,

            bbr_pacing_rate: Bandwidth { value: 0 },

            bbr_pacing_gain: kDefaultHighGain, // 1.0->set_startup_values

            bbr_cwnd_gain: kDefaultHighGain, // 1.0->set_startup_values

            bbr_num_startup_rtts: kRoundTripsWithoutGrowthBeforeExitingStartup,

            // bbr_flags init?
            bbr_cycle_current_offset: 0,

            bbr_last_cycle_start: now, // maybe need Option::None

            bbr_round_wo_bw_gain: 0,

            bbr_bw_at_last_round: Bandwidth { value: 0 },

            bbr_exit_probe_rtt_at: None,

            bbr_flags: BbrFlags::empty(),

            // others
            bbr_recovery_window: 0,

            bbr_last_sent_packno: 0,

            bbr_recovery_state: Bbr_recovery_state::BBR_RS_NOT_IN_RECOVERY,

            bbr_current_round_trip_end: 0,

            bbr_end_recovery_at: 0,

            bbr_min_rtt_since_last_probe: Duration::new(0, 0),

            bbr_startup_bytes_lost: 0,

            packet_out_que: BTreeMap::new(),
        }
    }
}

#[allow(non_snake_case)]
impl BBR {
    // bbr

    fn set_mode(&mut self, mode: BbrMode) {
        let now_time_ms = match time::SystemTime::now()
            .duration_since(time::SystemTime::UNIX_EPOCH)
        {
            Ok(n) => n.as_millis(),
            Err(_) => panic!("SystemTime before UNIX EPOCH!"),
        };

        if self.bbr_mode != mode {
            debug!(
                "timestamp: {} ms; BBR :: mode change {:?} -> {:?}",
                now_time_ms, self.bbr_mode, mode
            );
            self.bbr_mode = mode;
        } else {
            debug!(
                "timestamp: {} ms; BBR :: mode remains {:?}",
                now_time_ms, mode
            );
        }
    }

    fn set_startup_values(&mut self) {
        self.bbr_pacing_gain = self.bbr_high_gain;
        self.bbr_cwnd_gain = self.bbr_high_cwnd_gain;
    }

    fn get_min_rtt(&self) -> Duration {
        if self.bbr_min_rtt > Duration::from_micros(0) {
            return self.bbr_min_rtt;
        } else {
            let mut min_rtt = self.bbr_rtt_stats.rtt_stats_get_min_rtt();
            if min_rtt == Duration::from_micros(0) {
                min_rtt = Duration::from_micros(25000);
            }
            return min_rtt;
        }
    }

    pub fn bbr_pacing_rate(&self) -> u64 {
        let now_time_ms = match time::SystemTime::now()
            .duration_since(time::SystemTime::UNIX_EPOCH)
        {
            Ok(n) => n.as_millis(),
            Err(_) => panic!("SystemTime before UNIX EPOCH!"),
        };
        if !self.bbr_pacing_rate.bw_is_zero() {
            debug!(
                "timestamp: {} ms, self.bbr_pacing_rate.value= {}",
                now_time_ms, self.bbr_pacing_rate.value
            );
            // println!("pacing:{}", self.bbr_pacing_rate.value);
            return self.bbr_pacing_rate.value;
        } else {
            let min_rtt = self.get_min_rtt();
            let mut bw = Bandwidth::bw_from_bytes_and_delta(
                self.bbr_init_cwnd,
                min_rtt.as_micros() as u64,
            );
            bw = bw.bw_times(self.bbr_high_cwnd_gain);
            debug!(
                "timestamp: {} ms, bw.bw_to_bytes_per_sec()= {}",
                now_time_ms,
                bw.bw_value()
            );
            // println!("pacing:{}", bw.bw_value());
            return bw.bw_value();
        }
    }

    fn get_target_cwnd(&self, gain: f64) -> u64 {
        let bw = Bandwidth {
            value: self.bbr_max_bandwidth.minmax_get(),
        };
        let bdp =
            bw.bw_to_bytes_per_sec() as f64 * self.get_min_rtt().as_secs_f64();
        let mut cwnd = gain * bdp;

        if cwnd as u64 == 0 {
            cwnd = gain * self.bbr_init_cwnd as f64;
        }

        return cmp::max(cwnd as u64, self.bbr_min_cwnd as u64);
    }

    fn is_pipe_sufficiently_full(&mut self, bytes_in_flight: u64) -> bool {
        if self.bbr_mode == BbrMode::StartUp {
            return bytes_in_flight >= self.get_target_cwnd(1.5);
        } else if self.bbr_pacing_gain > 1.0 {
            return bytes_in_flight >= self.get_target_cwnd(self.bbr_pacing_gain);
        } else {
            return bytes_in_flight >= self.get_target_cwnd(1.1);
        }
    }

    // lsquic_bbr_was_quiet is useless?

    fn bbr_app_limited(&mut self, bytes_in_flight: u64) {
        let cwnd = self.bbr_get_cwnd();
        if bytes_in_flight >= cwnd {
            return;
        }

        if self
            .bbr_flags
            .contains(BbrFlags::BBR_FLAG_FLEXIBLE_APP_LIMITED)
            && self.is_pipe_sufficiently_full(bytes_in_flight)
        {
            return;
        }

        self.bbr_flags =
            self.bbr_flags | BbrFlags::BBR_FLAG_APP_LIMITED_SINCE_LAST_PROBE_RTT;
        self.bbr_bw_sampler.lsquic_bw_sampler_app_limited();
        debug!(
            "becoming application-limited.  Last sent packet: {};
        CWND: {}",
            self.bbr_last_sent_packno, cwnd
        );
    }

    pub fn bbr_ack(
        &mut self, packet_out: &mut PacketOut, packet_sz: u64,
        _now_time: Instant, _app_limit: bool,
    ) {
        assert!(self.bbr_flags.contains(BbrFlags::BBR_FLAG_IN_ACK));
        match self.bbr_bw_sampler.lsquic_bw_sampler_packet_acked(
            packet_out,
            self.bbr_ack_state.ack_time,
        ) {
            Some(v) => {
                debug!(
                    "bbr_ack_state.samples.push_back,v.bandwidth={};v.rtt={};",
                    v.bandwidth.bw_value(),
                    v.rtt.as_micros()
                );
                self.bbr_ack_state.samples.push_back(v);
            },
            _ => (),
        }

        // IQUIC_INVALID_PACKNO; IQUIC_MAX_PACKNO=(1ULL << 62) - 1
        // debug!(
        //     "bbr_ack:
        // packet_out.po_packno={};self.bbr_ack_state.max_packno={}",
        //     packet_out.po_packno, self.bbr_ack_state.max_packno
        // );
        // quiche's packet numbers not ordered(in fact, reverse every time)
        if self.bbr_ack_state.max_packno <= (1 << 62 - 1) {
            // We assume packet numbers are ordered
            debug!(
                "po_packno: {}, max_packno: {}\n",
                packet_out.po_packno, self.bbr_ack_state.max_packno
            );
            // assert!(packet_out.po_packno > self.bbr_ack_state.max_packno);
        }

        if packet_out.po_packno > self.bbr_ack_state.max_packno {
            self.bbr_ack_state.max_packno = packet_out.po_packno;
        }
        self.bbr_ack_state.ack_bytes += packet_sz;
    }

    pub fn bbr_sent(
        &mut self, packet_out: &mut PacketOut, in_flight: u64, app_limit: bool,
        po_packno: u64,
    ) {
        // not need po_flags?
        self.bbr_bw_sampler
            .lsquic_bw_sampler_packet_sent(packet_out, in_flight);

        self.bbr_last_sent_packno = po_packno;

        // add bytes
        self.bbr_bytes_in_flight += packet_out.size;

        if app_limit {
            self.bbr_app_limited(in_flight);
        }
    }

    #[allow(dead_code)]
    pub fn bbr_lost(&mut self, packet_out: &mut PacketOut, packet_sz: u64) {
        self.bbr_bw_sampler
            .lsquic_bw_sampler_packet_lost(packet_out);

        self.bbr_ack_state.has_losses = true;
        self.bbr_ack_state.lost_bytes += packet_sz;
    }

    pub fn bbr_begin_ack(&mut self, ack_time: Instant, bytes_in_flight: u64) {
        assert!(!self.bbr_flags.contains(BbrFlags::BBR_FLAG_IN_ACK));
        self.bbr_flags |= BbrFlags::BBR_FLAG_IN_ACK;

        self.bbr_ack_state.bbr_ack_state_init();

        self.bbr_ack_state.ack_time = ack_time;
        self.bbr_ack_state.max_packno = std::u64::MAX;
        self.bbr_ack_state.in_flight = bytes_in_flight;
        self.bbr_ack_state.total_bytes_acked_before =
            self.bbr_bw_sampler.bw_sampler_total_acked();
    }

    fn should_extend_min_rtt_expiry(&mut self) -> bool {
        if (self.bbr_flags
            & (BbrFlags::BBR_FLAG_APP_LIMITED_SINCE_LAST_PROBE_RTT
                | BbrFlags::BBR_FLAG_PROBE_RTT_DISABLED_IF_APP_LIMITED))
            == (BbrFlags::BBR_FLAG_APP_LIMITED_SINCE_LAST_PROBE_RTT
                | BbrFlags::BBR_FLAG_PROBE_RTT_DISABLED_IF_APP_LIMITED)
        {
            // Extend the current min_rtt if we've been app limited recently.
            return true;
        }

        let increased_since_last_probe =
            self.bbr_min_rtt_since_last_probe.as_micros() as f64
                > self.bbr_min_rtt.as_micros() as f64 * kSimilarMinRttThreshold;

        if (self.bbr_flags
            & (BbrFlags::BBR_FLAG_APP_LIMITED_SINCE_LAST_PROBE_RTT
                | BbrFlags::BBR_FLAG_PROBE_RTT_SKIPPED_IF_SIMILAR_RTT))
            == (BbrFlags::BBR_FLAG_APP_LIMITED_SINCE_LAST_PROBE_RTT
                | BbrFlags::BBR_FLAG_PROBE_RTT_SKIPPED_IF_SIMILAR_RTT)
            && !increased_since_last_probe
        {
            // Extend the current min_rtt if we've been app limited recently and
            // an rtt has been measured in that time that's less than
            // 12.5% more than the current min_rtt.
            return true;
        }
        return false;
    }

    fn update_bandwidth_and_min_rtt(&mut self) -> bool {
        let mut sample_min_rtt = Duration::from_micros(std::u64::MAX);
        for sample in self.bbr_ack_state.samples.iter() {
            if sample.is_app_limited {
                self.bbr_flags =
                    self.bbr_flags | BbrFlags::BBR_FLAG_LAST_SAMPLE_APP_LIMITED;
            } else {
                self.bbr_flags = self.bbr_flags
                    & (!BbrFlags::BBR_FLAG_LAST_SAMPLE_APP_LIMITED);
                self.bbr_flags =
                    self.bbr_flags | BbrFlags::BBR_FLAG_HAS_NON_APP_LIMITED;
            }

            debug!("sample.rtt : {} ms", sample.rtt.as_millis());
            if sample_min_rtt == Duration::from_micros(std::u64::MAX)
                || sample.rtt < sample_min_rtt
            {
                sample_min_rtt = sample.rtt;
            }

            if !sample.is_app_limited
                || sample.bandwidth.value > self.bbr_max_bandwidth.minmax_get()
            {
                debug!(
                    "minmax_upmax:sample.bandwidth.value={}",
                    sample.bandwidth.value
                );
                self.bbr_max_bandwidth
                    .minmax_upmax(self.bbr_round_count, sample.bandwidth.value)
            }
        }

        if sample_min_rtt == Duration::from_micros(std::u64::MAX) {
            return false;
        }

        self.bbr_min_rtt_since_last_probe =
            cmp::min(self.bbr_min_rtt_since_last_probe, sample_min_rtt);

        let mut min_rtt_expired = self.bbr_min_rtt != Duration::from_micros(0)
            && (self.bbr_ack_state.ack_time
                > self.bbr_min_rtt_timestamp + kMinRttExpiry);
        // println!("min_rtt_expired: {}", min_rtt_expired);

        if min_rtt_expired
            || sample_min_rtt < self.bbr_min_rtt
            || Duration::from_micros(0) == self.bbr_min_rtt
        {
            if min_rtt_expired && self.should_extend_min_rtt_expiry() {
                debug!(
                    "min rtt expiration extended, stay at: {} us",
                    self.bbr_min_rtt.as_micros()
                );
                min_rtt_expired = false;
            } else {
                debug!(
                    "min rtt updated: {} -> {}",
                    self.bbr_min_rtt.as_micros(),
                    sample_min_rtt.as_micros()
                );
                self.bbr_min_rtt = sample_min_rtt;
            }
            self.bbr_min_rtt_timestamp = self.bbr_ack_state.ack_time;
            self.bbr_min_rtt_since_last_probe =
                Duration::from_micros(std::u64::MAX);
            self.bbr_flags = self.bbr_flags
                & (!BbrFlags::BBR_FLAG_APP_LIMITED_SINCE_LAST_PROBE_RTT);
        }

        return min_rtt_expired;
    }

    fn update_recovery_state(&mut self, is_round_start: bool) {
        // Exit recovery when there are no losses for a round.
        if self.bbr_ack_state.has_losses {
            self.bbr_end_recovery_at = self.bbr_last_sent_packno;
        }

        match self.bbr_recovery_state {
            Bbr_recovery_state::BBR_RS_NOT_IN_RECOVERY => {
                if self.bbr_ack_state.has_losses {
                    self.bbr_recovery_state =
                        Bbr_recovery_state::BBR_RS_CONSERVATION;
                    self.bbr_recovery_window = 0;
                    self.bbr_current_round_trip_end = self.bbr_last_sent_packno;
                }
            },
            Bbr_recovery_state::BBR_RS_CONSERVATION => {
                if is_round_start {
                    self.bbr_recovery_state = Bbr_recovery_state::BBR_RS_GROWTH;
                }
            },
            Bbr_recovery_state::BBR_RS_GROWTH => {
                // Exit recovery if appropriate.
                if !self.bbr_ack_state.has_losses
                    && self.bbr_ack_state.max_packno > self.bbr_end_recovery_at
                {
                    self.bbr_recovery_state =
                        Bbr_recovery_state::BBR_RS_NOT_IN_RECOVERY;
                }
            },
        }
    }

    fn update_ack_aggregation_bytes(&mut self, newly_acked_bytes: u64) -> u64 {
        let ack_time = self.bbr_ack_state.ack_time;

        // Compute how many bytes are expected to be delivered, assuming max
        // bandwidth is correct.
        let bytes_acked_interval =
            ack_time - self.bbr_aggregation_epoch_start_time;
        let expected_bytes_acked = (self.bbr_max_bandwidth.minmax_get() as f64
            * bytes_acked_interval.as_micros() as f64)
            as u64;

        // Reset the current aggregation epoch as soon as the ack arrival rate is
        // less than or equal to the max bandwidth.
        if self.bbr_aggregation_epoch_bytes <= expected_bytes_acked {
            // Reset to start measuring a new aggregation epoch.
            self.bbr_aggregation_epoch_bytes = newly_acked_bytes;
            self.bbr_aggregation_epoch_start_time = ack_time;
            return 0;
        }

        // Compute how many extra bytes were delivered vs max bandwidth.
        // Include the bytes most recently acknowledged to account for stretch
        // acks.
        self.bbr_aggregation_epoch_bytes += newly_acked_bytes;
        let diff = self.bbr_aggregation_epoch_bytes - expected_bytes_acked;
        self.bbr_max_ack_height
            .minmax_upmax(self.bbr_round_count, diff);
        return diff;
    }

    fn update_gain_cycle_phase(&mut self, bytes_in_flight: u64) {
        let prior_in_flight = self.bbr_ack_state.in_flight;
        let now = self.bbr_ack_state.ack_time;

        // In most cases, the cycle is advanced after an RTT passes.
        let mut should_advance_gain_cycling =
            now - self.bbr_last_cycle_start > self.get_min_rtt();

        // If the pacing gain is above 1.0, the connection is trying to probe the
        // bandwidth by increasing the number of bytes in flight to at least
        // pacing_gain * BDP.  Make sure that it actually reaches the target, as
        // long as there are no losses suggesting that the buffers are not able to
        // hold that much.
        if self.bbr_pacing_gain > 1.0
            && !self.bbr_ack_state.has_losses
            && prior_in_flight < self.get_target_cwnd(self.bbr_pacing_gain)
        {
            should_advance_gain_cycling = false;
        }
        // Several optimizations are possible here: "else if" instead of "if", as
        // well as not calling get_target_cwnd() if `should_advance_gain_cycling'
        // is already set to the target value.

        // If pacing gain is below 1.0, the connection is trying to drain the
        // extra queue which could have been incurred by probing prior to
        // it.  If the number of bytes in flight falls down to the
        // estimated BDP value earlier, conclude that the queue has been
        // successfully drained and exit this cycle early.

        if self.bbr_pacing_gain < 1.0
            && bytes_in_flight <= self.get_target_cwnd(1.0)
        {
            should_advance_gain_cycling = true;
        }

        if should_advance_gain_cycling {
            self.bbr_cycle_current_offset =
                (self.bbr_cycle_current_offset + 1) % kGainCycleLength;
            self.bbr_last_cycle_start = now;
            // Stay in low gain mode until the target BDP is hit.  Low gain mode
            // will be exited immediately when the target BDP is achieved.
            if self.bbr_flags.contains(BbrFlags::BBR_FLAG_Drain_TO_TARGET)
                && self.bbr_pacing_gain < 1.0
                && kPacingGain[self.bbr_cycle_current_offset] == 1.0
                && bytes_in_flight > self.get_target_cwnd(1.0)
            {
                return;
            }
            self.bbr_pacing_gain = kPacingGain[self.bbr_cycle_current_offset];
            debug!(
                "advanced gain cycle, pacing gain set to {} ",
                self.bbr_pacing_gain
            );
        }
    }

    fn in_recovery(&self) -> bool {
        self.bbr_recovery_state != Bbr_recovery_state::BBR_RS_NOT_IN_RECOVERY
    }

    fn check_if_full_bw_reached(&mut self) {
        if self
            .bbr_flags
            .contains(BbrFlags::BBR_FLAG_LAST_SAMPLE_APP_LIMITED)
        {
            debug!("last sample app limited: full BW not reached");
            return;
        }

        let target = self.bbr_bw_at_last_round.bw_times(kStartupGrowthTarget);
        let bw = Bandwidth {
            value: self.bbr_max_bandwidth.minmax_get(),
        };

        if bw.bw_value() >= target.bw_value() {
            self.bbr_bw_at_last_round = bw;
            self.bbr_round_wo_bw_gain = 0;
            if self
                .bbr_flags
                .contains(BbrFlags::BBR_FLAG_EXPIRE_ACK_AGG_IN_STARTUP)
            {
                // Expire old excess delivery measurements now that bandwidth
                // increased.
                self.bbr_max_ack_height.minmax_reset(MinmaxSample {
                    time: self.bbr_round_count,
                    value: 0,
                });
                debug!("BW estimate {} bps greater than or equal to target {} bps: full BW not reached",
                bw.bw_value(), target.bw_value());
                return;
            }
        }

        self.bbr_round_wo_bw_gain += 1;
        if (self.bbr_round_wo_bw_gain >= self.bbr_num_startup_rtts)
            || (self
                .bbr_flags
                .contains(BbrFlags::BBR_FLAG_EXIT_STARTUP_ON_LOSS)
                && self.in_recovery())
        {
            assert!(self
                .bbr_flags
                .contains(BbrFlags::BBR_FLAG_HAS_NON_APP_LIMITED));
            self.bbr_flags |= BbrFlags::BBR_FLAG_IS_AT_FULL_BANDWIDTH;
            debug!("reached full BW");
        } else {
            debug!(
                "rounds w/o gain: {}, full BW not reached",
                self.bbr_round_wo_bw_gain
            );
        }
    }

    fn on_exit_startup(&mut self, _now: Instant) {
        assert!(self.bbr_mode == BbrMode::StartUp);
        // Apparently this method is just to update stats, something that we
        // don't do yet.
    }

    fn enter_probe_bw_mode(&mut self, now: Instant) {
        self.set_mode(BbrMode::ProbeBw);
        self.bbr_cwnd_gain = kCwndGain;

        // Pick a random offset for the gain cycle out of {0, 2..7} range. 1 is
        // excluded because in that case increased gain and decreased gain would
        // not follow each other.
        let mut rng = rand::thread_rng();
        let random: usize = rng.gen::<usize>();
        self.bbr_cycle_current_offset = random % (kGainCycleLength - 1);
        if self.bbr_cycle_current_offset >= 1 {
            self.bbr_cycle_current_offset += 1;
        }

        self.bbr_last_cycle_start = now;
        self.bbr_pacing_gain = kPacingGain[self.bbr_cycle_current_offset];
    }

    fn enter_startup_mode(&mut self, _now: Instant) {
        self.set_mode(BbrMode::StartUp);
        self.set_startup_values();
    }

    fn maybe_exit_startup_or_drain(
        &mut self, now: Instant, bytes_in_flight: u64,
    ) {
        if self.bbr_mode == BbrMode::StartUp
            && (self
                .bbr_flags
                .contains(BbrFlags::BBR_FLAG_IS_AT_FULL_BANDWIDTH))
        {
            self.on_exit_startup(now);
            self.set_mode(BbrMode::Drain);
            self.bbr_pacing_gain = self.bbr_drain_gain;
            self.bbr_cwnd_gain = self.bbr_high_cwnd_gain;
        }

        if self.bbr_mode == BbrMode::Drain {
            let target_cwnd = self.get_target_cwnd(1.0);
            // println!(
            //     "bytes in flight: {}; target cwnd: {}",
            //     bytes_in_flight, target_cwnd
            // );
            if bytes_in_flight <= target_cwnd {
                self.enter_probe_bw_mode(now);
            }
        }
    }

    fn in_slow_start(&mut self) -> bool {
        self.bbr_mode == BbrMode::StartUp
    }

    fn get_probe_rtt_cwnd(&self) -> u64 {
        if self
            .bbr_flags
            .contains(BbrFlags::BBR_FLAG_PROBE_RTT_BASED_ON_BDP)
        {
            return self.get_target_cwnd(kModerateProbeRttMultiplier);
        } else {
            return self.bbr_min_cwnd;
        }
    }

    fn bbr_get_cwnd(&self) -> u64 {
        let cwnd: u64;
        if self.bbr_mode == BbrMode::ProbeRtt {
            cwnd = self.get_probe_rtt_cwnd();
        } else if self.in_recovery()
            && !(self
                .bbr_flags
                .contains(BbrFlags::BBR_FLAG_RATE_BASED_STARTUP)
                && self.bbr_mode == BbrMode::StartUp)
        {
            cwnd = cmp::min(self.bbr_cwnd, self.bbr_recovery_window);
        } else {
            cwnd = self.bbr_cwnd;
        }
        // println!("BBR cwnd:{}", cwnd);
        return cwnd;
    }

    fn maybe_enter_or_exit_probe_rtt(
        &mut self, now: Instant, is_round_start: bool, min_rtt_expired: bool,
        bytes_in_flight: u64,
    ) {
        // println!(
        //     "min_rtt_expired: {}, flag: {}, mode: {}",
        //     min_rtt_expired,
        //     self.bbr_flags
        //         .contains(BbrFlags::BBR_FLAG_EXITING_QUIESCENCE),
        //     self.bbr_mode != BbrMode::ProbeRtt
        // );
        if min_rtt_expired
            && !(self
                .bbr_flags
                .contains(BbrFlags::BBR_FLAG_EXITING_QUIESCENCE))
            && self.bbr_mode != BbrMode::ProbeRtt
        {
            if self.in_slow_start() {
                self.on_exit_startup(now);
            }
            self.set_mode(BbrMode::ProbeRtt);
            self.bbr_pacing_gain = 1.0;
            // Do not decide on the time to exit ProbeRtt until the
            // |bytes_in_flight| is at the target small value.
            self.bbr_exit_probe_rtt_at = None;
        }

        if self.bbr_mode == BbrMode::ProbeRtt {
            self.bbr_bw_sampler.lsquic_bw_sampler_app_limited();

            if self.bbr_exit_probe_rtt_at.is_none() {
                // If the window has reached the appropriate size, schedule
                // exiting PROBE_RTT.  The CWND during PROBE_RTT
                // is kMinimumCongestionWindow, but we allow an
                // extra packet since QUIC checks CWND before
                // sending a packet.
                if bytes_in_flight
                    < self.get_probe_rtt_cwnd() + kMaxOutgoingPacketSize
                // kMaxOutgoingPackets = 1452
                {
                    self.bbr_exit_probe_rtt_at = Some(now + kProbeRttTime);
                    self.bbr_flags = self.bbr_flags
                        & (!BbrFlags::BBR_FLAG_PROBE_RTT_ROUND_PASSED);
                }
            } else {
                if is_round_start {
                    self.bbr_flags |= BbrFlags::BBR_FLAG_PROBE_RTT_ROUND_PASSED;
                }
                if now >= self.bbr_exit_probe_rtt_at.unwrap()
                    && (self
                        .bbr_flags
                        .contains(BbrFlags::BBR_FLAG_PROBE_RTT_ROUND_PASSED))
                {
                    self.bbr_min_rtt_timestamp = now;
                    if !(self
                        .bbr_flags
                        .contains(BbrFlags::BBR_FLAG_IS_AT_FULL_BANDWIDTH))
                    {
                        self.enter_startup_mode(now);
                    } else {
                        self.enter_probe_bw_mode(now);
                    }
                }
            }
        }

        self.bbr_flags &= !BbrFlags::BBR_FLAG_EXITING_QUIESCENCE;
    }

    fn calculate_pacing_rate(&mut self) {
        let bw = Bandwidth {
            value: self.bbr_max_bandwidth.minmax_get(),
        };
        if bw.bw_is_zero() {
            return;
        }
        let target_rate = bw.bw_times(self.bbr_pacing_gain);
        debug!(
            "target_rate: {} bps = bw_value: {} bps * gain: {}",
            target_rate.value, bw.value, self.bbr_pacing_gain
        );
        if self
            .bbr_flags
            .contains(BbrFlags::BBR_FLAG_IS_AT_FULL_BANDWIDTH)
        {
            debug!("is at full bandwidth.");
            self.bbr_pacing_rate = target_rate;
            return;
        }

        // Pace at the rate of initial_window / RTT as soon as RTT measurements
        // are available.
        if self.bbr_pacing_rate.bw_is_zero()
            && Duration::from_micros(0)
                != self.bbr_rtt_stats.rtt_stats_get_min_rtt()
        {
            debug!(
                "bw is 0, init_win: {} * 8000_000 / self.min_rtt: {} = rate",
                self.bbr_init_cwnd,
                self.bbr_rtt_stats.rtt_stats_get_min_rtt().as_micros()
            );
            self.bbr_pacing_rate = Bandwidth::bw_from_bytes_and_delta(
                self.bbr_init_cwnd,
                self.bbr_rtt_stats.rtt_stats_get_min_rtt().as_micros() as u64,
            );
            return;
        }

        // Slow the pacing rate in STARTUP once loss has ever been detected.
        let has_ever_detected_loss: bool = self.bbr_end_recovery_at != 0;
        if has_ever_detected_loss
            && (self.bbr_flags
                & (BbrFlags::BBR_FLAG_SLOWER_STARTUP
                    | BbrFlags::BBR_FLAG_HAS_NON_APP_LIMITED))
                == (BbrFlags::BBR_FLAG_SLOWER_STARTUP
                    | BbrFlags::BBR_FLAG_HAS_NON_APP_LIMITED)
        {
            debug!("lost, bw_value * 1.5 = rate");
            self.bbr_pacing_rate = bw.bw_times(kStartupAfterLossGain);
            return;
        }

        // Slow the pacing rate in STARTUP by the bytes_lost / CWND.
        if startup_rate_reduction_multiplier_ != 0
            && has_ever_detected_loss
            && (self
                .bbr_flags
                .contains(BbrFlags::BBR_FLAG_HAS_NON_APP_LIMITED))
        {
            debug!("slow down target_rate: {} * ", target_rate.value);
            self.bbr_pacing_rate = target_rate.bw_times(
                1.0 - (self.bbr_startup_bytes_lost as f64
                    * startup_rate_reduction_multiplier_ as f64
                    * 1.0
                    / self.bbr_cwnd_gain),
            );
            // Ensure the pacing rate doesn't drop below the startup growth target
            // times  the bandwidth estimate.
            if self.bbr_pacing_rate.value
                < bw.bw_times(kStartupGrowthTarget).value
            {
                debug!("starup growth rate 1.25, bw_value: {}", bw.value);
                self.bbr_pacing_rate = bw.bw_times(kStartupGrowthTarget);
            }
            return;
        }

        // Do not decrease the pacing rate during startup.
        if self.bbr_pacing_rate.value < target_rate.value {
            self.bbr_pacing_rate = target_rate;
        }
    }

    fn calculate_cwnd(&mut self, bytes_acked: u64, excess_acked: u64) {
        if self.bbr_mode == BbrMode::ProbeRtt {
            return;
        }

        let now_time_ms = match time::SystemTime::now()
            .duration_since(time::SystemTime::UNIX_EPOCH)
        {
            Ok(n) => n.as_millis(),
            Err(_) => panic!("SystemTime before UNIX EPOCH!"),
        };

        let mut target_window = self.get_target_cwnd(self.bbr_cwnd_gain);
        debug!(
            "timestamp: {} ms, target_window : {}",
            now_time_ms, target_window
        );
        // let bw = Bandwidth {
        //     value: self.bbr_max_bandwidth.minmax_get(),
        // };
        // debug!("timestamp:{}, target_window = max(bw : {} bytes/s * min rtt :
        // {} s * gain: {}, bbr_min_cwnd: {})", now_time_ms,
        // bw.bw_to_bytes_per_sec(), self.get_min_rtt().as_secs_f64(),
        // self.bbr_cwnd_gain, self.bbr_min_cwnd);
        if self
            .bbr_flags
            .contains(BbrFlags::BBR_FLAG_IS_AT_FULL_BANDWIDTH)
        {
            debug!("timestamp: {} ms, BBR_FLAG_IS_AT_FULL_BANDWIDTH, target window += bbr_max_ack_height: {} ", now_time_ms,  self.bbr_max_ack_height.minmax_get());

            // Add the max recently measured ack aggregation to CWND.
            target_window += self.bbr_max_ack_height.minmax_get();
            debug!(
                "timestamp: {} ms, target_window: {}",
                now_time_ms, target_window
            );
        } else if self
            .bbr_flags
            .contains(BbrFlags::BBR_FLAG_ENABLE_ACK_AGG_IN_STARTUP)
        {
            debug!("timestamp: {} ms, BBR_FLAG_ENABLE_ACK_AGG_IN_STARTUP, target_window += excess_acked: {}", now_time_ms,  excess_acked);
            // Add the most recent excess acked.  Because CWND never decreases in
            // STARTUP, this will automatically create a very localized max
            // filter.
            target_window += excess_acked;
            debug!(
                "timestamp: {} ms, target_window: {}",
                now_time_ms, target_window
            );
        }

        // Instead of immediately setting the target CWND as the new one, BBR
        // grows the CWND towards |target_window| by only increasing it
        // |bytes_acked| at a time.

        let add_bytes_acked = !FLAGS_quic_bbr_no_bytes_acked_in_startup_recovery
            || !self.in_recovery();
        if self
            .bbr_flags
            .contains(BbrFlags::BBR_FLAG_IS_AT_FULL_BANDWIDTH)
        {
            debug!("timestamp: {} ms, BBR_FLAG_IS_AT_FULL_BANDWIDTH, self.bbr_cwnd = min(target_window: {}, self.bbr_cwnd: {} + bytes_acked: {})",
            now_time_ms, target_window, self.bbr_cwnd, bytes_acked);
            self.bbr_cwnd = cmp::min(target_window, self.bbr_cwnd + bytes_acked);
        } else if add_bytes_acked
            && (self.bbr_cwnd_gain < target_window as f64
                || self.bbr_bw_sampler.bw_sampler_total_acked()
                    < self.bbr_init_cwnd)
        {
            // If the connection is not yet out of startup phase, do not decrease
            // the window.
            debug!(
                "timestamp: {} ms, In start up, self.bbr_cwnd: {} += byte_acked: {}",
                now_time_ms, self.bbr_cwnd, bytes_acked
            );

            self.bbr_cwnd += bytes_acked;
            debug!(
                "timestamp: {} ms, self.bbr_cwnd: {}",
                now_time_ms, self.bbr_cwnd
            );
        }

        // Enforce the limits on the congestion window.
        if self.bbr_cwnd < self.bbr_min_cwnd {
            self.bbr_cwnd = self.bbr_min_cwnd;
        } else if self.bbr_cwnd > self.bbr_max_cwnd {
            self.bbr_cwnd = self.bbr_max_cwnd;
        }
        debug!(
            "timestamp: {} ms, fix cwnd in range({}, {}), self.bbr_cwnd: {}",
            now_time_ms, self.bbr_min_cwnd, self.bbr_max_cwnd, self.bbr_cwnd
        );
    }

    fn calculate_recovery_window(
        &mut self, bytes_acked: u64, bytes_lost: u64, bytes_in_flight: u64,
    ) {
        if (self
            .bbr_flags
            .contains(BbrFlags::BBR_FLAG_RATE_BASED_STARTUP))
            && self.bbr_mode == BbrMode::StartUp
        {
            return;
        }

        if self.bbr_recovery_state == Bbr_recovery_state::BBR_RS_NOT_IN_RECOVERY {
            return;
        }

        // Set up the initial recovery window.
        if self.bbr_recovery_window == 0 {
            self.bbr_recovery_window = bytes_in_flight + bytes_acked;
            self.bbr_recovery_window =
                cmp::max(self.bbr_min_cwnd, self.bbr_recovery_window);
            return;
        }

        // Remove losses from the recovery window, while accounting for a
        // potential integer underflow.

        if self.bbr_recovery_window >= bytes_lost {
            self.bbr_recovery_window -= bytes_lost;
        } else {
            self.bbr_recovery_window = kMaxSegmentSize; // /*kMaxSegmentSize*/
        }

        // In CONSERVATION mode, just subtracting losses is sufficient.  In
        // GROWTH, release additional |bytes_acked| to achieve a
        // slow-start-like behavior.
        if self.bbr_recovery_state == Bbr_recovery_state::BBR_RS_GROWTH {
            self.bbr_recovery_window += bytes_acked;
        }

        // Sanity checks.  Ensure that we always allow to send at least an MSS or
        // |bytes_acked| in response, whichever is larger.
        self.bbr_recovery_window =
            cmp::max(self.bbr_recovery_window, bytes_in_flight + bytes_acked);

        if false
        // FLAG_quic_bbr_one_mss_conservation = 0
        {
            self.bbr_recovery_window = cmp::max(
                self.bbr_recovery_window,
                bytes_in_flight + kMaxSegmentSize, // kMaxSegmentSize
            )
        }

        self.bbr_recovery_window =
            cmp::max(self.bbr_recovery_window, self.bbr_min_cwnd);
    }

    pub fn bbr_end_ack(&mut self, bytes_in_flight: u64) {
        let is_round_start: bool;
        let min_rtt_expired: bool;
        let bytes_acked: u64;
        let excess_acked: u64;
        let bytes_lost: u64;

        assert!(self.bbr_flags.contains(BbrFlags::BBR_FLAG_IN_ACK));
        self.bbr_flags &= !BbrFlags::BBR_FLAG_IN_ACK;

        bytes_acked = self.bbr_bw_sampler.bw_sampler_total_acked()
            - self.bbr_ack_state.total_bytes_acked_before;

        if self.bbr_ack_state.ack_bytes > 0 {
            // println!("enter ack bytes");
            is_round_start = self.bbr_ack_state.max_packno
                > self.bbr_current_round_trip_end
                || !(self.bbr_current_round_trip_end < 0b1 << (62 - 1)); // MAXIQUIC packet number

            if is_round_start {
                self.bbr_round_count += 1;
                self.bbr_current_round_trip_end = self.bbr_last_sent_packno;
            }

            min_rtt_expired = self.update_bandwidth_and_min_rtt();
            self.update_recovery_state(is_round_start);
            excess_acked = self.update_ack_aggregation_bytes(bytes_acked);
        } else {
            is_round_start = false;
            min_rtt_expired = false;
            excess_acked = 0;
        }

        if self.bbr_mode == BbrMode::ProbeBw {
            self.update_gain_cycle_phase(bytes_in_flight);
        }

        if is_round_start
            && !(self
                .bbr_flags
                .contains(BbrFlags::BBR_FLAG_IS_AT_FULL_BANDWIDTH))
        {
            self.check_if_full_bw_reached();
        }

        self.maybe_exit_startup_or_drain(
            self.bbr_ack_state.ack_time,
            bytes_in_flight,
        );
        self.maybe_enter_or_exit_probe_rtt(
            self.bbr_ack_state.ack_time,
            is_round_start,
            min_rtt_expired,
            bytes_in_flight,
        );

        // Calculate number of packets acked and lost.
        bytes_lost = self.bbr_ack_state.lost_bytes;

        // After the model is updated, recalculate the pacing rate and congestion
        // window.

        self.calculate_pacing_rate();
        self.calculate_cwnd(bytes_acked, excess_acked);
        self.calculate_recovery_window(bytes_acked, bytes_lost, bytes_in_flight);
    }

    // fn lsquic_bbr_loss (&mut self) {   /* Noop */   }

    // fn lsquic_bbr_timeout (&mut self) {   /* Noop */   }

    // bbr end
}

#[derive(Copy, Clone)]
pub struct PacketOut {
    pub po_packno: u64,
    // pub po_ack2ed: u64,
    pub po_sent_time: Instant,
    pub size: u64,
    pub po_bwp_state: Option<BwpState>,
    pub po_sent: Instant,
}

/// sampler of BBR
#[derive(Debug, Clone, Copy)]
pub struct Bandwidth {
    value: u64,
}
impl Bandwidth {
    pub fn bw_infinite() -> Bandwidth {
        Bandwidth {
            value: u64::max_value(),
        }
    }

    // usecs' type:lsquic_time_t
    pub fn bw_from_bytes_and_delta(bytes: u64, usecs: u64) -> Bandwidth {
        Bandwidth {
            value: bytes * 8 * 1000_000 / usecs,
        }
    }

    pub fn bw_is_zero(&self) -> bool {
        self.value == 0
    }

    pub fn bw_to_bytes_per_sec(&self) -> u64 {
        self.value / 8
    }

    pub fn bw_times(&self, factor: f64) -> Bandwidth {
        Bandwidth {
            value: (self.value as f64 * factor) as u64,
        }
    }

    pub fn bw_value(&self) -> u64 {
        self.value
    }
}

enum BwsFlags {
    // BwsConnAborted = 1 << 0,
    BwsWarned = 1 << 1,
    BwsAppLimited = 1 << 2,
}

pub struct BwSampler {
    bws_total_sent: u64,
    bws_total_acked: u64,
    bws_total_lost: u64,
    bws_last_acked_total_sent: u64,
    bws_last_acked_sent_time: Instant, /* 原类型名：lsquic_time_t，
                                        * 实际上也是通过uint64 define的 */
    bws_last_acked_packet_time: Instant, // 同上
    bws_last_sent_packno: u64,           /* 原类型名：lsquic_packno_t，
                                          * 实际上也是通过uint64 define的 */
    bws_end_of_app_limited_phase: u64, // 同上

    // bws_ops_state: Vec<BwpState>, // 原代码：struct malo        *bws_malo;
    // 原注释：/* For struct osp_state objects */
    // osp可能是on send pakcet的意思
    // 猜想：使用这个malo的目的是存放on packet send的objects
    // bws_malo in lsquic: created in "lsquic_bw_sampler_init", destroy in
    // "lsquic_bw_sampler_cleanup"
    bws_flag: u64, // BwsFlags
}

impl Default for BwSampler {
    fn default() -> BwSampler {
        BwSampler {
            bws_total_sent: 0,
            bws_total_acked: 0,
            bws_total_lost: 0,
            bws_last_acked_total_sent: 0,
            bws_last_acked_sent_time: Instant::now(),
            bws_last_acked_packet_time: Instant::now(),
            bws_last_sent_packno: 0,
            bws_end_of_app_limited_phase: 0,

            // bws_ops_state: vec![],
            bws_flag: 0,
        }
    }
}

impl BwSampler {
    /// deleted functions, because useless:
    /// lsquic_bw_sampler_init, lsquic_bw_sampler_cleanup, bw_sampler_abort_conn
    pub fn bw_sampler_total_acked(&mut self) -> u64 {
        self.bws_total_acked
    }

    pub fn lsquic_bw_sampler_app_limited(&mut self) {
        self.bws_flag |= BwsFlags::BwsAppLimited as u64;
        self.bws_end_of_app_limited_phase = self.bws_last_sent_packno;
        debug!(
            "app limited, end of limited phase is {}",
            self.bws_end_of_app_limited_phase
        );
    }

    pub fn bw_warn_once(&mut self, msg: String) {
        if (self.bws_flag & BwsFlags::BwsWarned as u64) == 0 {
            self.bws_flag |= BwsFlags::BwsWarned as u64;
            warn!("{}", msg);
        }
    }

    pub fn lsquic_bw_sampler_packet_sent(
        &mut self, packet_out: &mut PacketOut, in_flight: u64,
    ) {
        match packet_out.po_bwp_state {
            None => {
                self.bw_warn_once(
                    packet_out.po_packno.to_string() + "packet already has state",
                );
            },
            _ => (),
        }
        self.bws_last_sent_packno = packet_out.po_packno;

        let sent_sz = packet_out.size;
        self.bws_total_sent += sent_sz;

        if in_flight == 0 {
            self.bws_last_acked_packet_time = packet_out.po_sent_time;
            self.bws_last_acked_total_sent = self.bws_total_sent;
            self.bws_last_acked_sent_time = packet_out.po_sent_time;
        }
        // malo
        let bwps_send_state = BwpsSendState {
            total_bytes_sent: self.bws_total_sent,
            total_bytes_acked: self.bws_total_acked,
            total_bytes_lost: self.bws_total_lost,
            is_app_limited: !(self.bws_flag & BwsFlags::BwsAppLimited as u64
                == 0),
        };
        let state = BwpState {
            bwps_send_state,
            bwps_sent_at_last_ack: self.bws_last_acked_total_sent,
            bwps_last_ack_sent_time: Some(self.bws_last_acked_sent_time),
            bwps_last_ack_ack_time: self.bws_last_acked_packet_time,
            bwps_packet_size: sent_sz,
        };
        packet_out.po_bwp_state = Some(state);
        debug!("add info for packet {}", packet_out.po_packno);
    }

    // total lost is useless.
    pub fn lsquic_bw_sampler_packet_lost(&mut self, packet_out: &mut PacketOut) {
        let po_bwp_state = match packet_out.po_bwp_state {
            None => return,
            Some(v) => v,
        };
        self.bws_total_lost += po_bwp_state.bwps_packet_size;
        // self.bws_ops_state.push(po_bwp_state);//malo
        packet_out.po_bwp_state = None;
        debug!(
            "packet {} lost, total_lost goes to {}",
            packet_out.po_packno, self.bws_total_lost
        );
    }

    pub fn lsquic_bw_sampler_packet_acked(
        &mut self, packet_out: &mut PacketOut, ack_time: Instant,
    ) -> Option<BwSample> {
        let state = match packet_out.po_bwp_state {
            None => return None,
            Some(v) => v,
        };
        let sent_sz = packet_out.size;

        self.bws_total_acked += sent_sz;
        self.bws_last_acked_total_sent = state.bwps_send_state.total_bytes_sent;
        self.bws_last_acked_sent_time = packet_out.po_sent_time;
        self.bws_last_acked_packet_time = ack_time;

        // Exit app-limited phase once a packet that was sent while the connection
        // is not app-limited is acknowledged.
        if (self.bws_flag & BwsFlags::BwsAppLimited as u64) != 0
            && packet_out.po_packno > self.bws_end_of_app_limited_phase
        {
            self.bws_flag &= !(BwsFlags::BwsAppLimited as u64);
            debug!(
                "exit app-limited phase due to packet {} being acked",
                packet_out.po_packno
            );
        }

        // There might have been no packets acknowledged at the moment when the
        // current packet was sent. In that case, there is no bandwidth sample to
        // make.
        let bwps_last_ack_sent_time = match state.bwps_last_ack_sent_time {
            None => {
                // self.bws_ops_state.push(state);
                packet_out.po_bwp_state = None;
                return None;
            },
            Some(v) => v,
        };

        // Infinite rate indicates that the sampler is supposed to discard the
        // current send rate sample and use only the ack rate.
        let send_rate;
        if packet_out.po_sent > bwps_last_ack_sent_time {
            let bytes = state.bwps_send_state.total_bytes_sent
                - state.bwps_sent_at_last_ack;
            let usecs = packet_out.po_sent - bwps_last_ack_sent_time;
            send_rate = Bandwidth::bw_from_bytes_and_delta(
                bytes,
                usecs.as_micros() as u64,
            ); // bps
        } else {
            send_rate = Bandwidth::bw_infinite();
        }

        // During the slope calculation, ensure that ack time of the current
        // packet is always larger than the time of the previous packet,
        // otherwise division by zero or integer underflow can occur.
        if ack_time <= state.bwps_last_ack_ack_time {
            self.bw_warn_once(
                "Time of the previously acked packet (".to_owned()
                    + &state
                        .bwps_last_ack_ack_time
                        .elapsed()
                        .as_micros()
                        .to_string()
                    + ") is larger than the ack time of the current packet ("
                    + &ack_time.elapsed().as_micros().to_string()
                    + ")",
            );
            // self.bws_ops_state.push(state);
            packet_out.po_bwp_state = None;
            return None;
        }

        let ack_rate = Bandwidth::bw_from_bytes_and_delta(
            self.bws_total_acked - state.bwps_send_state.total_bytes_acked,
            (ack_time - state.bwps_last_ack_ack_time).as_micros() as u64,
        ); // bps

        let now_time_ms = match time::SystemTime::now()
            .duration_since(time::SystemTime::UNIX_EPOCH)
        {
            Ok(n) => n.as_millis(),
            Err(_) => panic!("SystemTime before UNIX EPOCH!"),
        };
        debug!(
            "timestamp: {} ms; send rate {} bps; ack rate {} bps; ",
            now_time_ms, send_rate.value, ack_rate.value
        );
        // Note: this sample does not account for delayed acknowledgement time.
        // This means that the RTT measurements here can be artificially high,
        // especially on low bandwidth connections.

        let rtt = ack_time - packet_out.po_sent;
        let is_app_limited = state.bwps_send_state.is_app_limited;

        // After this point, we switch `sample' to point to `state' and don't
        // reference `state' anymore.
        let bandwidth;
        packet_out.po_bwp_state = None;
        if send_rate.bw_value() < ack_rate.bw_value() {
            bandwidth = send_rate;
        } else {
            bandwidth = ack_rate;
        }
        debug!(
            "packet {} acked, bandwidth: {} bps",
            packet_out.po_packno,
            bandwidth.bw_value()
        );
        let sample = BwSample {
            bandwidth,
            rtt,
            is_app_limited,
        };
        Some(sample)
    }
}

pub struct BwSample {
    // TAILQ_ENTRY(bw_sample)      next;//没看到过这个量被使用，故略。
    pub bandwidth: Bandwidth,
    pub rtt: Duration, // 原类型：lsquic_time_t
    pub is_app_limited: bool,
}

#[derive(Copy, Clone)]
pub struct BwpsSendState {
    pub total_bytes_sent: u64,
    pub total_bytes_acked: u64,
    pub total_bytes_lost: u64,
    pub is_app_limited: bool,
}
impl Default for BwpsSendState {
    fn default() -> BwpsSendState {
        BwpsSendState {
            total_bytes_sent: 0,
            total_bytes_acked: 0,
            total_bytes_lost: 0,
            is_app_limited: false,
        }
    }
}

#[derive(Copy, Clone)]
pub struct BwpState {
    pub bwps_send_state: BwpsSendState,
    pub bwps_sent_at_last_ack: u64,
    pub bwps_last_ack_sent_time: Option<Instant>, // lsquic_time_t
    pub bwps_last_ack_ack_time: Instant,          // lsquic_time_t
    pub bwps_packet_size: u64,                    // unsigned short
}
impl Default for BwpState {
    fn default() -> BwpState {
        BwpState {
            bwps_send_state: Default::default(),
            bwps_sent_at_last_ack: 0,
            bwps_last_ack_sent_time: None,
            bwps_last_ack_ack_time: Instant::now(),
            bwps_packet_size: 0,
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    // test of Bandwidth
    #[test]
    fn bandwidth_test() {
        assert_eq!(Bandwidth::bw_infinite().value, 18446744073709551615);
        // assert_eq!(Bandwidth::bw_zero().value, 0);

        let mut bw = Bandwidth { value: 0 };
        assert!(bw.bw_is_zero());
        bw.value = 800;
        assert!(!bw.bw_is_zero());
        assert_eq!(bw.bw_to_bytes_per_sec(), 100);
        // assert_eq!(Bandwidth::new(999).value, 999);

        assert_eq!(bw.bw_times(10 as f64).value, 8000);
    }

    // test of BwSampler
    #[test]
    fn bw_sampler_test() {
        let _state = BwpState {
            bwps_send_state: BwpsSendState {
                total_bytes_sent: 1,
                total_bytes_acked: 1,
                total_bytes_lost: 1,
                is_app_limited: true,
            },
            bwps_sent_at_last_ack: 1,
            bwps_last_ack_sent_time: None,
            bwps_last_ack_ack_time: Instant::now(),
            bwps_packet_size: 1,
        };
    }
}

// module_bbr
impl cc::CongestionControl for BBR {
    fn new(_init_cwnd: u64, _init_pacing_rate: u64) -> Self
    where
        Self: Sized,
    {
        BBR::default()
    }

    fn cwnd(&self) -> usize {
        let now_time_ms = match time::SystemTime::now()
            .duration_since(time::SystemTime::UNIX_EPOCH)
        {
            Ok(n) => n.as_millis(),
            Err(_) => panic!("SystemTime before UNIX EPOCH!"),
        };
        debug!(
            "timestamp: {} ms, bbr_cwnd: {}",
            now_time_ms,
            self.bbr_get_cwnd()
        );
        self.bbr_get_cwnd() as usize
    }

    fn collapse_cwnd(&mut self) {
        self.bbr_cwnd = kInitialCongestionWindow * kDefaultTCPMSS;
    }

    fn bytes_in_flight(&self) -> usize {
        self.bbr_bytes_in_flight as usize
    }

    fn decrease_bytes_in_flight(&mut self, bytes_in_flight: usize) {
        self.bbr_bytes_in_flight =
            // self.bbr_bytes_in_flight.saturating_sub(bytes_in_flight);
            self.bbr_bytes_in_flight - bytes_in_flight as u64;
    }

    fn congestion_recovery_start_time(&self) -> Option<Instant> {
        None
    }

    fn on_packet_sent_cc(
        &mut self, pkt: &Sent, _bytes_sent: usize, _trace_id: &str,
    ) {
        let mut packet_out = PacketOut {
            po_sent_time: pkt.time,
            po_packno: pkt.pkt_num,
            size: pkt.size as u64,
            // sent: &pkt,
            po_bwp_state: None,
            po_sent: pkt.time,
        };
        let app_limit = false;
        let pkt_num = pkt.pkt_num;

        self.bbr_sent(
            &mut packet_out,
            self.bbr_bytes_in_flight as u64,
            app_limit,
            pkt_num,
        );

        self.packet_out_que.insert(pkt_num, packet_out);

        let now_time_ms = match time::SystemTime::now()
            .duration_since(time::SystemTime::UNIX_EPOCH)
        {
            Ok(n) => n.as_millis(),
            Err(_) => panic!("SystemTime before UNIX EPOCH!"),
        };
        debug!(
            "timestamp: {} pkt {} has inserted to packet_out_queue",
            now_time_ms, pkt_num
        );
        // self.bbr_sent(packet_out: &mut PacketOut, in_flight: u64, app_limit:
        // bool, po_packno: u64)
    }

    fn on_packet_acked_cc(
        &mut self, packet: &Sent, _srtt: Duration, _min_rtt: Duration,
        _latest_rtt: Duration, app_limited: bool, _trace_id: &str,
        _epoch: packet::Epoch, _lost_count: usize,
    ) {
        self.bbr_bytes_in_flight -= packet.size as u64; // TODO

        // if self.in_congestion_recovery(packet.time) {
        //     return;
        // } // TODO

        // if app_limited {
        //     return;
        // }

        // let packet_out_zero = PacketOut {
        //     po_sent_time: Instant::now(),
        //     po_packno: 0,
        //     size: 0,
        //     // sent: &pkt,
        //     po_bwp_state: None,
        //     po_sent: Instant::now(),
        // }; // TODO

        let mut packet_out = match self.packet_out_que.remove(&packet.pkt_num) {
            Some(v) => v,
            None => {
                debug!("cannot find pkt {} in packet_out_queue", packet.pkt_num);
                return;
            },
        };
        let now_time_ms = match time::SystemTime::now()
            .duration_since(time::SystemTime::UNIX_EPOCH)
        {
            Ok(n) => n.as_millis(),
            Err(_) => panic!("SystemTime before UNIX EPOCH!"),
        };
        debug!(
            "timestamp: {} packet {} has removed from packet_out_que",
            now_time_ms, packet_out.po_packno
        );

        debug!("enter bbr_ack");
        self.bbr_ack(
            &mut packet_out,
            packet.size as u64,
            Instant::now(),
            app_limited,
        );
    }

    fn congestion_event(
        &mut self, _srtt: Duration, _time_sent: Instant, _now: Instant,
        _trace_id: &str, _packet_id: u64, _epoch: packet::Epoch,
        _lost_count: usize,
    ) {
        // Start a new congestion event if packet was sent after the
        // start of the previous congestion recovery period.
        // if !self.in_congestion_recovery(time_sent) {
        // self.congestion_recovery_start_time = Some(now);

        // self.congestion_window = (self.congestion_window as f64 *
        //     cc::LOSS_REDUCTION_FACTOR)
        //     as usize;
        // self.congestion_window =
        //     std::cmp::max(self.congestion_window, cc::MINIMUM_WINDOW);
        // self.ssthresh = self.congestion_window;
        // }
        debug!(
            "timestamp= {:?} {:?}",
            time::SystemTime::now()
                .duration_since(time::SystemTime::UNIX_EPOCH)
                .unwrap()
                .as_millis(),
            self
        );
    }

    fn cc_bbr_begin_ack(&mut self, ack_time: Instant) {
        debug!("enter bbr_begin_ack");
        self.bbr_begin_ack(ack_time, self.bbr_bytes_in_flight);
    }

    fn cc_bbr_end_ack(&mut self) {
        debug!("enter bbr_end_ack");
        self.bbr_end_ack(self.bbr_bytes_in_flight);
        debug!(
            "timestamp= {:?} {:?}",
            time::SystemTime::now()
                .duration_since(time::SystemTime::UNIX_EPOCH)
                .unwrap()
                .as_millis(),
            self
        );
    }

    fn pacing_rate(&self) -> u64 {
        self.bbr_pacing_rate()
    }

    fn bbr_min_rtt(&mut self) -> Duration {
        self.get_min_rtt()
    }

    // unused
    fn cc_trigger(
        &mut self, _event_type: char, _rtt: u64, _bytes_in_flight: u64,
        _packet_id: u64,
    ) {
    }
}

impl std::fmt::Debug for BBR {
    fn fmt(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
        write!(
            f,
            "cwnd= {} pacing= {} bytes_in_flight= {}",
            self.bbr_get_cwnd(),
            self.bbr_pacing_rate.value,
            self.bbr_bytes_in_flight,
        )
    }
}

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

use std::time::Duration;
use std::time::Instant;
use std::time::SystemTime;

use crate::cc;
use crate::packet;
use crate::recovery::Sent;

// use std::thread;
use std::sync::mpsc;
use threadpool::ThreadPool;

use std::mem;
use std::os::raw::c_char;

#[repr(C)]
#[derive(Debug, Copy, Clone)]
pub struct CcInfo {
    pub event_type: c_char,
    pub event_time: u64,
    pub rtt: u64,
    pub bytes_in_flight: u64,
    pub packet_id: u64,
}

#[cfg(feature = "interface")]
extern {
    fn SolutionCcTrigger(
        cc_infos: *mut CcInfo, cc_num: u64, cwnd: *mut u64, pacing_rate: *mut u64,
    );
}

#[allow(unused_variables)]
fn cc_trigger(
    cc_infos: *mut CcInfo, cc_num: u64, cwnd: *mut u64, pacing_rate: *mut u64,
) {
    #[cfg(feature = "interface")]
    unsafe {
        SolutionCcTrigger(cc_infos, cc_num, cwnd, pacing_rate)
    }
}

pub fn cc_trigger_async(
    cc_infos: &mut Vec<CcInfo>, cwnd: u64, pacing_rate: u64,
    tx: mpsc::Sender<(u64, u64)>,
) {
    cc_infos.shrink_to_fit();
    let ack_ptr = cc_infos.as_mut_ptr();
    let cc_num = cc_infos.len() as u64;
    mem::forget(cc_infos);
    let mut cwnd = cwnd;
    let mut pacing_rate = pacing_rate;
    cc_trigger(ack_ptr, cc_num, &mut cwnd, &mut pacing_rate);
    // todo use ack info to get new cwnd
    match tx.send((cwnd, pacing_rate)) {
        Err(v) => println!("{}", v),
        _ => (),
    };
}

/// cc trigger of aitrans implementation.
pub struct CCTrigger {
    congestion_window: u64,

    pacing_rate: u64, // bps

    bytes_in_flight: usize,

    congestion_recovery_start_time: Option<Instant>,

    // channel for  multi thread
    cwnd_rx: Option<mpsc::Receiver<(u64, u64)>>,
    cwnd_tx: Option<mpsc::Sender<(u64, u64)>>,
    // threadpool for  multi thread
    pool: ThreadPool,
    cc_infos: Vec<CcInfo>,
}

impl cc::CongestionControl for CCTrigger {
    fn new(init_cwnd: u64, init_pacing_rate: u64) -> Self
    where
        Self: Sized,
    {
        CCTrigger {
            congestion_window: init_cwnd,

            pacing_rate: init_pacing_rate, // bps

            bytes_in_flight: 0,

            congestion_recovery_start_time: None,

            cwnd_rx: None,
            cwnd_tx: None,
            pool: ThreadPool::new(2),
            cc_infos: vec![],
        }
    }

    fn cwnd(&self) -> usize {
        self.congestion_window as usize
    }

    fn collapse_cwnd(&mut self) {
        self.congestion_window = cc::MINIMUM_WINDOW as u64;
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
        self.bytes_in_flight += bytes_sent;
    }

    fn on_packet_acked_cc(
        &mut self, packet: &Sent, srtt: Duration, _min_rtt: Duration,
        _latest_rtt: Duration, _app_limited: bool, _trace_id: &str,
        _epoch: packet::Epoch, _lost_count: usize,
    ) {
        self.bytes_in_flight -= packet.size;
        self.cc_trigger(
            'F',
            srtt.as_millis() as u64,
            self.bytes_in_flight as u64,
            packet.pkt_num, // packet_id
        );
        trace!("pacing_rate:{}", self.pacing_rate());
    }

    fn congestion_event(
        &mut self, srtt: Duration, _time_sent: Instant, _now: Instant,
        _trace_id: &str, packet_id: u64, _epoch: packet::Epoch,
        _lost_count: usize,
    ) {
        self.cc_trigger(
            'D',
            srtt.as_millis() as u64,
            self.bytes_in_flight as u64,
            packet_id, // packet_id
        );
    }

    // todo:add ack info to self
    fn cc_trigger(
        &mut self, event_type: char, rtt: u64, bytes_in_flight: u64,
        packet_id: u64,
    ) {
        // get the cwnd from the channel
        // if there are no thread are running, spawn a new thread
        // move the ack info to the spawned thread
        let event_type = event_type as c_char;
        let event_time =
            match SystemTime::now().duration_since(SystemTime::UNIX_EPOCH) {
                Ok(n) => n.as_millis() as u64,
                Err(_) => panic!("SystemTime before UNIX EPOCH!"),
            }; // ms
        let ack_info = CcInfo {
            event_type,
            event_time,
            rtt,
            bytes_in_flight,
            packet_id,
        };
        self.cc_infos.push(ack_info);
        let cwnd = self.congestion_window;
        let pacing_rate = self.pacing_rate();
        // init cwnd_tx and cwnd_rx
        if self.cwnd_rx.is_none() {
            let (tx, rx) = mpsc::channel();
            let tx1 = mpsc::Sender::clone(&tx);
            let mut cc_infos = vec![];
            mem::swap(&mut self.cc_infos, &mut cc_infos);
            self.pool.execute(move || {
                cc_trigger_async(&mut cc_infos, cwnd, pacing_rate, tx1)
            });

            self.cwnd_rx = Some(rx);
            self.cwnd_tx = Some(tx);
        }
        // check the channel and if there are some ack to be tackle
        // let t1 = Instant::now();
        let recved = self.cwnd_rx.as_ref().unwrap().try_recv();
        // println!("channel recv use : {} us",t1.elapsed().as_micros());
        match recved {
            Ok((cwnd, pacing_rate)) => {
                self.congestion_window = cwnd; // last thread finished
                self.pacing_rate = pacing_rate;
                if !self.cc_infos.is_empty() {
                    // there are some ack to be tackle
                    let tx1 =
                        mpsc::Sender::clone(&self.cwnd_tx.as_ref().unwrap());
                    // let t1 = Instant::now();
                    let mut cc_infos = vec![];
                    mem::swap(&mut self.cc_infos, &mut cc_infos);
                    self.pool.execute(move || {
                        cc_trigger_async(&mut cc_infos, cwnd, pacing_rate, tx1)
                    });
                }
            },
            _ => (),
        };
    }

    // unused
    fn pacing_rate(&self) -> u64 {
        self.pacing_rate
    }

    fn cc_bbr_begin_ack(&mut self, _ack_time: Instant) {}

    fn cc_bbr_end_ack(&mut self) {}

    fn bbr_min_rtt(&mut self) -> Duration {
        Duration::new(0, 0)
    }
}

impl std::fmt::Debug for CCTrigger {
    fn fmt(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
        write!(
            f,
            "cwnd={} bytes_in_flight={}",
            self.congestion_window, self.bytes_in_flight,
        )
    }
}

// #[cfg(test)]
// mod tests {
//     use super::*;

//     const TRACE_ID: &str = "test_id";

//     fn init() {
//         let _ = env_logger::builder().is_test(true).try_init();
//     }

//     #[test]
//     fn cc_trigger_init() {
//         let cc = cc::new_congestion_control(cc::Algorithm::CcTrigger);

//         assert!(cc.cwnd() > 0);
//         assert_eq!(cc.bytes_in_flight(), 0);
//     }

//     #[test]
//     fn cc_trigger_send() {
//         init();

//         let mut cc = cc::new_congestion_control(cc::Algorithm::CcTrigger);

//        let p = Sent {
//            pkt_num: 0,
//            frames: vec![],
//            time: std::time::Instant::now(),
//            size: 1000,
//            ack_eliciting: true,
//            in_flight: true,
//            fec_info: Default::default(),
//        };

//         cc.on_packet_sent_cc(&p, p.size, TRACE_ID);

//         assert_eq!(cc.bytes_in_flight(), p.size);
//     }

//     #[test]
//     fn cc_trigger_congestion_event() {
//         init();

//         let mut cc = cc::new_congestion_control(cc::Algorithm::CcTrigger);
//         let prev_cwnd = cc.cwnd();

//         cc.congestion_event(
//             std::time::Duration::from_millis(0),
//             std::time::Instant::now(),
//             std::time::Instant::now(),
//             TRACE_ID,
//         );

//         // In Reno, after congestion event, cwnd will be cut in half.
//         assert_eq!(prev_cwnd / 2, cc.cwnd());
//     }

//     #[test]
//     fn cc_trigger_collapse_cwnd() {
//         init();

//         let mut cc = cc::new_congestion_control(cc::Algorithm::CcTrigger);

//         // cwnd will be reset
//         cc.collapse_cwnd();
//         assert_eq!(cc.cwnd(), cc::MINIMUM_WINDOW);
//     }
// }

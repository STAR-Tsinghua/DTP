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

use crate::cc;
// use crate::packet;
use crate::recovery::Sent;
use crate::stream::cc_trigger;

use std::ffi::CString;
use std::os::raw::c_char;

/// cc trigger of aitrans implementation.
pub struct CCTrigger {
    congestion_window: u64,

    bytes_in_flight: usize,

    congestion_recovery_start_time: Option<Instant>,
}

impl cc::CongestionControl for CCTrigger {
    fn new() -> Self
    where
        Self: Sized,
    {
        CCTrigger {
            congestion_window: cc::INITIAL_WINDOW as u64,

            bytes_in_flight: 0,

            congestion_recovery_start_time: None,
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
        &mut self, packet: &Sent, _srtt: Duration, _min_rtt: Duration,
        _app_limited: bool, _trace_id: &str,
    ) {
        self.bytes_in_flight -= packet.size;
        let c_str = CString::new("EVENT_TYPE_FINISHED").unwrap();
        // obtain a pointer to a valid zero-terminated string
        let c_ptr: *const c_char = c_str.as_ptr();
        let rtt = 0;
        self.congestion_window = cc_trigger(
            c_ptr,
            rtt,
            self.bytes_in_flight as u64,
            self.congestion_window,
        );
    }

    fn congestion_event(
        &mut self, _time_sent: Instant, _now: Instant, _trace_id: &str,
    ) {
        let c_str = CString::new("EVENT_TYPE_DROP").unwrap();
        // obtain a pointer to a valid zero-terminated string
        let c_ptr: *const c_char = c_str.as_ptr();
        let rtt = 0;
        self.congestion_window = cc_trigger(
            c_ptr,
            rtt,
            self.bytes_in_flight as u64,
            self.congestion_window,
        );
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

#[cfg(test)]
mod tests {
    use super::*;

    const TRACE_ID: &str = "test_id";

    fn init() {
        let _ = env_logger::builder().is_test(true).try_init();
    }

    #[test]
    fn cc_trigger_init() {
        let cc = cc::new_congestion_control(cc::Algorithm::CcTrigger);

        assert!(cc.cwnd() > 0);
        assert_eq!(cc.bytes_in_flight(), 0);
    }

    #[test]
    fn cc_trigger_send() {
        init();

        let mut cc = cc::new_congestion_control(cc::Algorithm::CcTrigger);

        let p = Sent {
            pkt_num: 0,
            frames: vec![],
            time: std::time::Instant::now(),
            size: 1000,
            ack_eliciting: true,
            in_flight: true,
        };

        cc.on_packet_sent_cc(&p, p.size, TRACE_ID);

        assert_eq!(cc.bytes_in_flight(), p.size);
    }

    #[test]
    fn cc_trigger_congestion_event() {
        init();

        let mut cc = cc::new_congestion_control(cc::Algorithm::CcTrigger);
        let prev_cwnd = cc.cwnd();

        cc.congestion_event(
            std::time::Instant::now(),
            std::time::Instant::now(),
            TRACE_ID,
        );

        // In Reno, after congestion event, cwnd will be cut in half.
        assert_eq!(prev_cwnd / 2, cc.cwnd());
    }

    #[test]
    fn cc_trigger_collapse_cwnd() {
        init();

        let mut cc = cc::new_congestion_control(cc::Algorithm::CcTrigger);

        // cwnd will be reset
        cc.collapse_cwnd();
        assert_eq!(cc.cwnd(), cc::MINIMUM_WINDOW);
    }
}

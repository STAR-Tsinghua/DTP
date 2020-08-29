use std::time::{
    Instant,
    SystemTime,
    UNIX_EPOCH,
};
extern crate time;

/// use nums of ntp_single_offset and compute mean of this offsets, to get a
/// stabler offset.
pub fn ntp_offset(server_address: &str) -> (i64, String) {
    let mut ntp_nums = 20;
    let mut sum_offset = 0;
    for _ in 0..ntp_nums {
        let offset = ntp_single_offset(server_address);
        if offset == i64::max_value() {
            ntp_nums -= 1;
        } else {
            sum_offset += offset;
        }
    }
    if ntp_nums == 0 {
        let s = format!("get ntp offset failed\n");
        return (0, s);
    } else {
        let s = format!(
            "get ntp offset success, ntp nums:{}, offset:{}\n",
            ntp_nums,
            sum_offset / ntp_nums
        );
        (sum_offset / ntp_nums, s)
    }
}

/// get offset between local time and server time. if failed, return
/// i64::max_value().
fn ntp_single_offset(server_address: &str) -> i64 {
    let t1 = Instant::now();
    let response = match ntp::request(server_address) {
        Ok(v) => v,
        _ => return i64::max_value(),
    };
    let t2 = Instant::now();

    let start = SystemTime::now();
    let since_the_epoch = start
        .duration_since(UNIX_EPOCH)
        .expect("Time went backwards");
    let local_sec = since_the_epoch.as_secs();
    let local_nsec = since_the_epoch.subsec_nanos();

    let ntp_time = response.transmit_time;
    let ntp_time: time::Timespec = time::Timespec::from(ntp_time);
    let offset = (local_sec as i64 - ntp_time.sec) * 1000 +
        (local_nsec as i64 - ntp_time.nsec as i64) / 1000_000;
    return offset - t2.duration_since(t1).as_millis() as i64 / 2;
}

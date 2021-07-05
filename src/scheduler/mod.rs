use crate::stream::Block;

pub trait Scheduler {
    fn new() -> Self
    where
        Self: Sized;
    /// Schedulor implementation
    ///
    /// Decide which block to send next in blocks_vec
    fn select_block(
        &mut self, 
        blocks_vec: &mut Vec<Block>, 
        _pacing_rate: f64, _rtt: f64,
        _next_packet_id: u64, _current_time: u64
    ) -> u64;

    /// Drop block discriminative function
    ///
    /// Decide whether a block should be dropped by the sender
    fn should_drop_block(
        &mut self, 
        _block: &Block, 
        _pacing_rate: f64, _rtt:f64, 
        _next_packet_id: u64, _current_time: u64
    ) -> bool {
        false
    }

    fn peek_through_flushable(
        &mut self, 
        _blocks_vec: &mut Vec<Block>, 
        _pacing_rate: f64, _rtt: f64,
        _next_packet_id: u64, _current_time: u64
    ) {
        () // do nothing
    }
}

#[derive(Default)]
pub struct BasicScheduler {
    last_block_id: Option<u64>
}

impl Scheduler for BasicScheduler {
    fn new() -> Self {
        info!("Create BasicScheduler");
        Default::default()
    }
    fn select_block(
        &mut self, 
        blocks_vec: &mut Vec<Block>, 
        _pacing_rate: f64, _rtt: f64,
        _next_packet_id: u64, _current_time: u64
    ) -> u64 {
        let mut len = -1;
        if self.last_block_id.is_some() {
            for block in blocks_vec.iter() {
                if Some(block.block_id) == self.last_block_id {
                    len = block.remaining_size as i128;
                    break;
                }
            }
        }

        if len <= 0 {
            // begin new block transmittion
            self.last_block_id = Some(blocks_vec[0].block_id);
            return blocks_vec[0].block_id;
        } else {
            // tranport the last block
            return self.last_block_id.clone().unwrap();
        }
    }
}

pub struct DynScheduler {
    scheduler: Box<dyn Scheduler>
}

impl Default for DynScheduler {
    fn default() -> Self {
        if cfg!(feature = "interface") {
            DynScheduler { 
                scheduler: Box::new(c_scheduler::CScheduler::new())
            }
        } else {
            DynScheduler {
                scheduler: Box::new(dtp_scheduler::DtpScheduler::new())
            }
        }
   }
}

impl Scheduler for DynScheduler {
    fn new() -> Self {
        Default::default()
    }

    fn select_block(
        &mut self, 
        blocks_vec: &mut Vec<Block>, 
        pacing_rate: f64, rtt: f64,
        next_packet_id: u64, current_time: u64
    ) -> u64 {
        self.scheduler.select_block(blocks_vec, pacing_rate, rtt, next_packet_id, current_time)
    }

    fn should_drop_block(
        &mut self, 
        block: &Block, 
        pacing_rate: f64, rtt:f64, 
        next_packet_id: u64, current_time: u64
    ) -> bool {
        self.scheduler.should_drop_block(block, pacing_rate, rtt, next_packet_id, current_time)
    } 

    fn peek_through_flushable(
        &mut self, 
        blocks_vec: &mut Vec<Block>, 
        pacing_rate: f64, rtt: f64,
        next_packet_id: u64, current_time: u64
    ) {
        self.scheduler.peek_through_flushable(blocks_vec, pacing_rate, rtt, next_packet_id, current_time)
    }
}

mod c_scheduler;
mod dtp_scheduler;

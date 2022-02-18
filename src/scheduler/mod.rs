use std::str::FromStr;

use crate::stream::Block;

/// Available scheduler types
#[repr(C)]
#[derive(Debug, Copy, Clone, PartialEq)]
pub enum SchedulerType {
    /// Basic FIFO scheduler
    Basic   = 0,
    /// Scheduler introduced by DTP
    DTP     = 1,
    /// Implement C scheduler using interface function
    /// `SolutionSelectBlock` and `SolutionShouldDropBlock` 
    C       = 2,
    /// Dynamic scheduler change with Rust feature
    Dynamic = 3,
}

impl FromStr for SchedulerType {
    type Err = crate::Error;

    fn from_str(name: &str) -> Result<Self, Self::Err> {
        match name {
            "Basic" | "basic" => Ok(SchedulerType::Basic),
            "DTP" | "Dtp" | "dtp" => Ok(SchedulerType::DTP),
            "C" | "c" => Ok(SchedulerType::C),
            "Dynamic" | "dynamic" | "Dyn" | "dyn" => Ok(SchedulerType::Dynamic),
            _ => Err(crate::Error::SchedulerType)
        }
    }
}

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

/// Instantiate a scheduler implementation basing on the given SchedulerType
pub fn new_scheduler(sche: SchedulerType) -> Box<dyn Scheduler> {
    eprintln!("Creating scheduler: {:?}", sche);
    match sche {
        SchedulerType::Basic =>
            Box::new(BasicScheduler::new()),
        SchedulerType::DTP => Box::new(dtp_scheduler::DtpScheduler::new()),
        SchedulerType::C=>
            Box::new(c_scheduler::CScheduler::new()),
        SchedulerType::Dynamic=>
            Box::new(DynScheduler::new()),
        _ => {
            warn!("Invalid scheduler type! Change to default scheduler");
            Box::new(DynScheduler::new())
        }
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
        } 
        else if cfg!(feature = "basic-scheduler") {
            DynScheduler {
                scheduler: Box::new(BasicScheduler::new())
            }
        }
        else {
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

impl DynScheduler {
    pub fn init(stype: SchedulerType) -> Self {
        let mut dyn_scheduler: DynScheduler = Default::default();
        if stype != SchedulerType::Dynamic {
            dyn_scheduler.scheduler = new_scheduler(stype);
        }
        eprintln!("Finish creating dyn scheduler");
        return dyn_scheduler;
    }
}

mod c_scheduler;
mod dtp_scheduler;

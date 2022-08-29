use crate::scheduler::Block;
use crate::scheduler::Scheduler;

#[cfg(feature = "interface")]
extern {
    fn SolutionSelectBlock(
        blocks: *const Block, block_num: u64, next_packet_id: u64,
        current_time: u64,
    ) -> u64;

    fn SolutionShouldDropBlock(
        block: *const Block, bandwidth: libc::c_double, rtt: libc::c_double,
        next_packet_id: u64, current_time: u64,
    ) -> bool;
}

#[allow(unused_variables)]
fn solution_select_block(
    blocks: *const Block, block_num: u64, next_packet_id: u64, current_time: u64,
) -> u64 {
    #[cfg(feature = "interface")]
    return unsafe {
        SolutionSelectBlock(blocks, block_num, next_packet_id, current_time)
    };
    #[cfg(not(feature = "interface"))]
    return 0;
}

#[allow(unused_variables)]
fn solution_should_drop_block(
    block: *const Block, bandwidth: libc::c_double, rtt: libc::c_double,
    next_packet_id: u64, current_time: u64,
) -> bool {
    #[cfg(feature = "interface")]
    return unsafe {
        SolutionShouldDropBlock(
            block,
            bandwidth,
            rtt,
            next_packet_id,
            current_time,
        )
    };
    #[cfg(not(feature = "interface"))]
    return false;
}

pub struct CScheduler;

impl Scheduler for CScheduler {
    fn new() -> Self {
        Self
    }

    fn select_block(
        &mut self, blocks_vec: &mut Vec<Block>, _pacing_rate: f64, _rtt: f64,
        next_packet_id: u64, current_time: u64,
    ) -> u64 {
        blocks_vec.shrink_to_fit();
        let blocks = blocks_vec.as_ptr();
        let block_num = blocks_vec.len() as u64;
        return solution_select_block(
            blocks,
            block_num,
            next_packet_id,
            current_time,
        );
    }

    fn should_drop_block(
        &mut self, block: &Block, pacing_rate: f64, rtt: f64,
        next_packet_id: u64, current_time: u64,
    ) -> bool {
        return solution_should_drop_block(
            block,
            pacing_rate,
            rtt,
            next_packet_id,
            current_time,
        );
    }
}

impl Default for CScheduler {
    fn default() -> Self {
        CScheduler::new()
    }
}

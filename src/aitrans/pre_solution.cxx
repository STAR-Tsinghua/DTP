#include "pre_solution.hxx"
#include "solution.hxx"

uint64_t CSelectBlock(Block *blocks, uint64_t block_num, uint64_t next_packet_id,
                      uint64_t current_time) {
    return SolutionSelectBlock(blocks, block_num, next_packet_id, current_time);
}

void Ccc_trigger(CcInfo *cc_infos, uint64_t cc_num, uint64_t *congestion_window, uint64_t *pacing_rate) {
    SolutionCcTrigger(cc_infos, cc_num, congestion_window, pacing_rate);
}

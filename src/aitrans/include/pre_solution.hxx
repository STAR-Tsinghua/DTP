#ifndef PRE_H_
#define PRE_H_
#include "solution.hxx"
#include <stdint.h>

extern "C" {
    uint64_t CSelectBlock(Block *blocks, uint64_t block_num, uint64_t next_packet_id,
                        uint64_t current_time);
    void Ccc_trigger(CcInfo *cc_infos, uint64_t cc_num, uint64_t *congestion_window, uint64_t *pacing_rate);
}

#endif /* PRE_H_ */
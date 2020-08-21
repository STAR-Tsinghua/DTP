#ifndef PRE_H_
#define PRE_H_
#include <stdint.h>

extern "C" {
    uint64_t CSelectBlock(char *blocks_string, uint64_t block_num,
                        uint64_t current_time);
    void Ccc_trigger(char *event_type, uint64_t rtt, uint64_t bytes_in_flight, uint64_t *congestion_window, uint64_t *pacing_rate);
}

#endif /* PRE_H_ */
#ifndef AITRANS_H_
#define AITRANS_H_
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <map>
using namespace std;
static map<string, uint64_t> your_parameter;
extern "C" {
    struct Blocks {
        uint64_t *blocks_id, *blocks_deadline, *blocks_priority,
            *blocks_create_time, *blocks_size;
        uint64_t block_num;
    };
    void SolutionInit();
    uint64_t SolutionSelectPacket(struct Blocks blocks, uint64_t current_time);
    uint64_t SolutionCcTrigger(char *event_type, uint64_t rtt, uint64_t bytes_in_flight, uint64_t cwnd);
}

#endif /* AITRANS_H_ */
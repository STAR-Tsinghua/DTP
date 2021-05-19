#ifndef SOLUTION_H_
#define SOLUTION_H_
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <unordered_map>
using namespace std;
static unordered_map<string, uint64_t> your_parameter;
static unordered_map<string, double> float_parameter;
extern "C" {
    struct CcInfo {
        char event_type; // 'D': packet is dropped in congestion; 'F': packet is acked
        uint64_t event_time; // ms
        uint64_t rtt; // ms
        uint64_t bytes_in_flight;
        uint64_t packet_id;
    };
    struct Block {
        uint64_t block_id;
        uint64_t block_deadline; // ms
        uint64_t block_priority;
        uint64_t block_create_time; // ms 
        uint64_t block_size; // Bytes
        uint64_t remaining_size; //Bytes
    };
    /** Returns the ratio of Data packets and ACK packets 
     * 
     *  An ACK packet will be sent each N packets
     *  where N is the return value. like: pk_num % N == 0
     * 
     * @return The ratio of Data packets and ACK packets 
     * */
    uint64_t SolutionAckRatio();
    /** Returns the rate of redundancy
     * 
     * Assume the rate of redundancy is a, the number of valid data packets is M 
     * then the number of parity packets is N = aM.
     * 
     * The data can be retrieved when at least M packets are received among M+N packets
     * 
     * @return The ratio of Data packets and ACK packets 
     * */
    float SolutionRedundancy();
    /** Decide whether a block should be dropped by sender
     * 
     * */
    bool SolutionShouldDropBlock(Block *block, double bandwidth, double rtt,uint64_t next_packet_id, uint64_t current_time); 
    /** 
     * Initialize self-parameters in Solution C codes
     * 
     * This function is called once in `set_cc_algorithm`
     * */
    void SolutionInit(uint64_t *init_congestion_window, uint64_t *init_pacing_rate);
    /** 
     * Select the next block to send in a block vector
     * 
     * @return The block id of selected block
     * */
    uint64_t SolutionSelectBlock(Block* blocks, uint64_t block_num, uint64_t next_packet_id, uint64_t current_time);
    /**
     * Implement Congestion Control by changing *congestion_window and *pacing_rate
     * */
    void SolutionCcTrigger(CcInfo* cc_infos, uint64_t cc_num, uint64_t *congestion_window, uint64_t *pacing_rate);
}

#endif /* SOLUTION_H_ */
#include <iostream>
#include <memory>
#include <algorithm>
#include <vector>
#include <cmath>
#include <ctime>
#include <cstring>

#include "include/solution.hxx"
// parameter.py
const static int MAX_P = 3;
const static int time_interval = 2000;
const static double MAX_BW = 200.0 * 1000 * 1000 * 8;
const static double MAX_DDL = 0.2;
const static int BYTES_PER_PACKET = 1500;

uint64_t SolutionAckRatio() {
    cout << "Solution: hello from SolutionAckRatio" << endl;
    return 4;
}

bool SolutionShouldDropBlock(struct Block *block, double bandwidth, double rtt,uint64_t next_packet_id, uint64_t current_time) {
    cout << "Solution: hello from SolutionShouldDropBlock" << endl;
    return current_time - block->block_create_time > block->block_deadline;
}

float SolutionRedundancy(){
    if(your_parameter["current_block_remainning_time"] < float_parameter["rtt"] * 2){
        cout  << "Solution: " << "redundancy func if1:" << float_parameter["redundancy"] << endl;
        return float_parameter["redundancy"];
    }
    else if (((your_parameter["current_block_remainning_size"] * 1.0 )/ your_parameter["current_block_remainning_time"]) * (1 + ((your_parameter["lost_pkt_num"] * 1.0) / your_parameter["total_pkt_num"])) * 8 < float_parameter["current_rate"]){
        cout  << "Solution: " << "redundancy func if2:" << float_parameter["redundancy"] << endl;
        return float_parameter["redundancy"];
    }
    else{
        cout  << "Solution: " << "redundancy func:" << 1.0 << endl;
        return 1.0;
    }
}

void SolutionInit(uint64_t *init_congestion_window, uint64_t *init_pacing_rate)
{
    cout << "Solution: hello from SolutionInit" << endl;

    for(int i = 0;i < 30;i ++){
        your_parameter[std::to_string(i)] = 0;
    }
    your_parameter["index"] = 0;

    your_parameter["total_pkt_num"] = 0;
    your_parameter["lost_pkt_num"] = 0;

    your_parameter["interval_pkt_num"] = 0;
    your_parameter["interval_start_time"] = 0;
    your_parameter["interval_send_num"] = 0;

    float_parameter["rtt"] = -10000;

    your_parameter["ddl"] = 0;
    your_parameter["size"] = 0;
    your_parameter["prio"] = 0;

    your_parameter["rtt_update_time"] = 0;
    your_parameter["last_block_id"] = -1;

    float_parameter["current_rate"] = MAX_BW;
    float_parameter["redundancy"] = 0.5;

    your_parameter["current_block_remainning_time"] = 2100000000;
    your_parameter["current_block_remainning_size"] = 0;

    *init_pacing_rate = MAX_BW;
    *init_congestion_window = 12345678;
}

uint64_t SolutionSelectBlock(Block* blocks, uint64_t block_num, uint64_t next_packet_id, uint64_t current_time)
{
    cout << "Solution: hello from SolutionSelectBlock" << endl;
    /************** START CODE HERE ***************/
    // return the id of block you want to send, for example:
    your_parameter["interval_send_num"] += 1;

    if (float_parameter["rtt"] > 0.0 && your_parameter["rtt_update_time"] + float_parameter["rtt"] < current_time){
        your_parameter["rtt_update_time"] = current_time;
        float_parameter["rtt"] = -10000.0;
    }
        
    double Min_weight = 10000000.0;
    int Min_weight_block_id = -1;

    int len = -1;
    int ddl, size, prio;
    for(int i = 0;i < block_num;i ++){
        if(blocks[i].block_id == your_parameter["last_block_id"]){
            len = blocks[i].remaining_size;
            ddl = blocks[i].block_deadline;
            size = blocks[i].remaining_size;
            prio = blocks[i].block_priority;
        }
    }

    if (len <= 0){

        for(int i = 0;i < block_num;i ++){
            if(blocks[i].remaining_size > 0){
                int tempddl = blocks[i].block_deadline;
                int Passedtime = current_time - blocks[i].block_create_time;
                double One_way_delay = float_parameter["rtt"];
                int tempsize = blocks[i].remaining_size;
                double Current_sending_rate = float_parameter["current_rate"];

                double Remaining_time = tempddl - Passedtime - One_way_delay - ((tempsize * 8.0) / Current_sending_rate) * 1000;

                if(Remaining_time >= 0.0){
                    int tempprio = blocks[i].block_priority;
                    double weight = (1.0 * Remaining_time / ddl) / (1 - 1.0 * tempprio / MAX_P);
                    if(Min_weight_block_id == -1){
                        Min_weight_block_id = i;
                        Min_weight = weight;
                        ddl = blocks[i].block_deadline;
                        size = blocks[i].remaining_size;
                        prio = blocks[i].block_priority;
                    }
                    else if(Min_weight > weight){
                        Min_weight_block_id = i;
                        Min_weight = weight;
                        ddl = blocks[i].block_deadline;
                        size = blocks[i].remaining_size;
                        prio = blocks[i].block_priority;
                    }
                    else if(Min_weight == weight && blocks[i].remaining_size < blocks[Min_weight_block_id].remaining_size){
                        Min_weight_block_id = i;
                        Min_weight = weight;
                        ddl = blocks[i].block_deadline;
                        size = blocks[i].remaining_size;
                        prio = blocks[i].block_priority;
                    }
                }
            }
        }

        your_parameter["ddl"] = ddl;
        your_parameter["size"] = size;
        your_parameter["prio"] = prio;

        your_parameter["last_block_id"] = Min_weight_block_id;

        if(Min_weight_block_id != -1){
            your_parameter["last_block_id"] = blocks[Min_weight_block_id].block_id;
            return your_parameter["last_block_id"];
        }   
        else {
            return blocks[0].block_id;
        }
    }
    else {
        your_parameter["ddl"] = ddl;
        your_parameter["size"] = size;
        your_parameter["prio"] = prio;

        return your_parameter["last_block_id"];
    }
    /************** END CODE HERE ***************/
}

void SolutionCcTrigger(CcInfo *cc_infos, uint64_t cc_num, uint64_t *congestion_window, uint64_t *pacing_rate)
{

    cout << "Solution: hello from SolutionCcTrigger" << endl;
    /************** START CODE HERE ***************/
    uint64_t cwnd = *congestion_window;
    for(uint64_t i=0;i<cc_num;i++){
        char event_type = cc_infos[i].event_type;
        // fprintf(stderr, "event_type=%c\n", event_type);
        const uint64_t max_packet_size = 1350;
        const uint64_t init_ssthresh = 2 * max_packet_size;
        if (your_parameter.count("ssthresh") <= 0)
            your_parameter["ssthresh"] = init_ssthresh;
        // return new cwnd, for example:
        uint64_t ssthresh = your_parameter["ssthresh"];
        if (event_type == 'F') {  // EVENT_TYPE_FINISHED
            if (cwnd > ssthresh)
                cwnd += max_packet_size / 2;
            else {
                cwnd += max_packet_size;
            }
        }
        if (event_type == 'D') {
            cwnd = cwnd / 2;  // EVENT_TYPE_DROP
            your_parameter["ssthresh"] = cwnd;
        }
        // fprintf(stderr,"new cwnd: %lu, ssthresh = %lu\n", cwnd, your_parameter["ssthresh"]);
    }
    // fprintf(stderr,"new cwnd: %lu, ssthresh = %lu\n", cwnd, your_parameter["ssthresh"]);
    *pacing_rate = 123456789;
    *congestion_window = cwnd;
    /************** END CODE HERE ***************/
}

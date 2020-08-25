#include "solution.hxx"

void SolutionInit(){
    your_parameter["max_packet_size"] = 1350;
    your_parameter["init_ssthresh"] = 2 * your_parameter["max_packet_size"];
    your_parameter["last_time"]=0;
    your_parameter["ssthresh"] = your_parameter["init_ssthresh"];
}

uint64_t SolutionSelectPacket(struct Blocks blocks, uint64_t current_time) {
    /************** START CODE HERE ***************/
    // return the id of block you want to send, for example:
    uint64_t last_time = your_parameter["last_time"];
    fprintf(stderr,"last_time = %lu, current_time = %lu\n", last_time, current_time);
    
    your_parameter["last_time"] = current_time;
    return blocks.blocks_id[0];
    /************** END CODE HERE ***************/
}

void SolutionCcTrigger(AckInfo *ack_infos, uint64_t ack_num, uint64_t *congestion_window, uint64_t *pacing_rate) {
    /************** START CODE HERE ***************/
    uint64_t cwnd = *congestion_window;
    for(uint64_t i=0;i<ack_num;i++){
        char* event_type = ack_infos[i].event_type;
        fprintf(stderr, "event_type=%s\n", event_type);
        const uint64_t max_packet_size = 1350;
        const uint64_t init_ssthresh = 2 * max_packet_size;
        if (your_parameter.count("ssthresh") <= 0)
            your_parameter["ssthresh"] = init_ssthresh;
        // return new cwnd, for example:
        uint64_t ssthresh = your_parameter["ssthresh"];
        if (event_type[11] == 'F') {  // EVENT_TYPE_FINISHED
            if (cwnd > ssthresh)
                cwnd += max_packet_size / 2;
            else {
                cwnd += max_packet_size;
            }
        }
        if (event_type[11] == 'D') {
            cwnd = cwnd / 2;  // EVENT_TYPE_DROP
            your_parameter["ssthresh"] = cwnd;
        }
        fprintf(stderr,"new cwnd: %lu, ssthresh = %lu\n", cwnd, your_parameter["ssthresh"]);
    }
    fprintf(stderr,"new cwnd: %lu, ssthresh = %lu\n", cwnd, your_parameter["ssthresh"]);
    *pacing_rate = 123456789;
    *congestion_window = cwnd;
    /************** END CODE HERE ***************/
}

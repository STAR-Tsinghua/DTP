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

uint64_t SolutionCcTrigger(char *event_type, uint64_t rtt, uint64_t bytes_in_flight, uint64_t cwnd) {
    /************** START CODE HERE ***************/
    fprintf(stderr, "rtt: %lu, bytes_in_flight: %lu\n", rtt, bytes_in_flight);
    // return new cwnd, for example:
    uint64_t ssthresh = your_parameter["ssthresh"];
    if (event_type[11] == 'F') {  // EVENT_TYPE_FINISHED
        if (cwnd > ssthresh)
            cwnd += your_parameter["max_packet_size"] / 2;
        else {
            cwnd += your_parameter["max_packet_size"];
        }
    }
    if (event_type[11] == 'D') {
        cwnd = cwnd / 2;  // EVENT_TYPE_DROP
        your_parameter["ssthresh"] = cwnd;
    }
    fprintf(stderr,"new cwnd: %lu, ssthresh = %lu\n", cwnd, your_parameter["ssthresh"]);
    return cwnd;
    /************** END CODE HERE ***************/
}

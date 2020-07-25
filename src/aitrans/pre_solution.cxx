#include "pre_solution.hxx"
#include "solution.hxx"

uint64_t CSelectBlock(char *blocks_string, uint64_t block_num,
                      uint64_t current_time) {
    fprintf(stderr, "blocks_string: %s\n", blocks_string);
    struct Blocks blocks;
    blocks.block_num = block_num;
    blocks.blocks_id = (uint64_t *)malloc(block_num * sizeof(uint64_t));
    blocks.blocks_deadline = (uint64_t *)malloc(block_num * sizeof(uint64_t));
    blocks.blocks_priority = (uint64_t *)malloc(block_num * sizeof(uint64_t));
    blocks.blocks_create_time =
        (uint64_t *)malloc(block_num * sizeof(uint64_t));
    blocks.blocks_size = (uint64_t *)malloc(block_num * sizeof(uint64_t));;
    for (uint64_t i = 0; i < block_num; i++) {
        blocks.blocks_id[i] = strtoull(blocks_string, &blocks_string, 10);
        blocks.blocks_deadline[i] = strtoull(blocks_string, &blocks_string, 10);
        blocks.blocks_priority[i] = strtoull(blocks_string, &blocks_string, 10);
        blocks.blocks_create_time[i] =
            strtoull(blocks_string, &blocks_string, 10);
        blocks.blocks_size[i] = strtoull(blocks_string, &blocks_string, 10);
        fprintf(stderr,"block_id: %lu, deadline: %lu, priority: %lu, create_time: %lu, size: %lu\n",blocks.blocks_id[i], blocks.blocks_deadline[i],
            blocks.blocks_priority[i], blocks.blocks_create_time[i], blocks.blocks_size[i]);
    }
    /*********** Player's Code Here*************/
    return SolutionSelectPacket(blocks, current_time);
    /*********** Player's Code End*************/
}

uint64_t Ccc_trigger(char *event_type, uint64_t rtt, uint64_t bytes_in_flight, uint64_t cwnd) {
    fprintf(stderr, "event_type=%s\n", event_type);
    return SolutionCcTrigger(event_type, rtt, bytes_in_flight, cwnd);
}

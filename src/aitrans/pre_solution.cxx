#include "pre_solution.hxx"
#include "solution.hxx"

uint64_t CSelectBlock(char *blocks_string, uint64_t block_num,
                      uint64_t current_time) {
    // fprintf(stderr, "blocks_string: %s\n", blocks_string);
    Block* blocks=(Block *)malloc(block_num * sizeof(Block));
    for (uint64_t i = 0; i < block_num; i++) {
        blocks[i].block_id = strtoull(blocks_string, &blocks_string, 10);
        blocks[i].block_deadline = strtoull(blocks_string, &blocks_string, 10);
        blocks[i].block_priority = strtoull(blocks_string, &blocks_string, 10);
        blocks[i].block_create_time =
            strtoull(blocks_string, &blocks_string, 10);
        blocks[i].block_size = strtoull(blocks_string, &blocks_string, 10);
        // fprintf(stderr,"block_id: %lu, deadline: %lu, priority: %lu, create_time: %lu, size: %lu\n",blocks.blocks_id[i], blocks.blocks_deadline[i],
            // blocks.blocks_priority[i], blocks.blocks_create_time[i], blocks.blocks_size[i]);
    }
    /*********** Player's Code Here*************/
    return SolutionSelectBlock(blocks, block_num, current_time);
    /*********** Player's Code End*************/
}

void Ccc_trigger(CcInfo *cc_infos, uint64_t cc_num, uint64_t *congestion_window, uint64_t *pacing_rate) {
    SolutionCcTrigger(cc_infos, cc_num, congestion_window, pacing_rate);
}

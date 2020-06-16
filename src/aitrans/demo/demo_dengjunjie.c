#include <stdint.h>
#include <stdio.h>

struct Blocks {
  uint64_t *blocks_id, *blocks_deadline, *blocks_priority, *blocks_create_time;
  uint64_t block_num;
};

uint64_t SolutionSelectPacket(uint64_t current_time, struct Blocks blocks) {
  /************** START CODE HERE ***************/
  int i;
  uint64_t best_block_index = -1;
  for (i = 0; i < blocks.block_num; i++) {
    if (best_block_index == -1 ||
        current_time - blocks.blocks_create_time[best_block_index] >=
            blocks.blocks_deadline[best_block_index] ||
        blocks.blocks_create_time[best_block_index] >
            blocks.blocks_create_time[i] ||
        (current_time - blocks.blocks_create_time[best_block_index]) *
                blocks.blocks_deadline[best_block_index] >
            (current_time - blocks.blocks_create_time[i]) *
                blocks.blocks_deadline[i])
      best_block_index = i;
  }
  return blocks.blocks_id[best_block_index];
  /************** END CODE HERE ***************/
}
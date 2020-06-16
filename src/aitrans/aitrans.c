#include "aitrans.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

// uint64_t SolutionSelectPacket(struct Blocks blocks, uint64_t current_time) {
//   fprintf(stderr, "current time = %ld \n", current_time);
//   return blocks.blocks_id[0];
// }

uint64_t CSelectBlock(char *blocks_string, uint64_t block_num,
                      uint64_t current_time) {
  fprintf(stderr, "%s\n", blocks_string);
  struct Blocks blocks;
  blocks.block_num = block_num;
  blocks.blocks_id = (uint64_t *)malloc(block_num * sizeof(uint64_t));
  blocks.blocks_deadline = (uint64_t *)malloc(block_num * sizeof(uint64_t));
  blocks.blocks_priority = (uint64_t *)malloc(block_num * sizeof(uint64_t));
  blocks.blocks_create_time = (uint64_t *)malloc(block_num * sizeof(uint64_t));
  for (uint64_t i = 0; i < block_num; i++) {
    blocks.blocks_id[i] = strtoull(blocks_string, &blocks_string, 10);
    blocks.blocks_deadline[i] = strtoull(blocks_string, &blocks_string, 10);
    blocks.blocks_priority[i] = strtoull(blocks_string, &blocks_string, 10);
    blocks.blocks_create_time[i] = strtoull(blocks_string, &blocks_string, 10);
  }
  /*********** Player's Code Here*************/
  return SolutionSelectPacket(blocks, current_time);
  /*********** Player's Code End*************/
}

// uint64_t SolutionCcTrigger(char *event_type, uint64_t cwnd) {
//   uint64_t new_cwnd;
//   if (event_type[11] == 'F')
//     new_cwnd = cwnd + 1500; // EVENT_TYPE_FINISHED
//   if (event_type[11] == 'D')
//     new_cwnd = cwnd / 2; // EVENT_TYPE_DROP
//   return new_cwnd;
// }

uint64_t Ccc_trigger(char *event_type, uint64_t cwnd) {
  fprintf(stderr, "event_type=%s\n", event_type);
  return SolutionCcTrigger(event_type, cwnd);
}

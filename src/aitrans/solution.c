#include <stdint.h>

struct Blocks {
  uint64_t *blocks_id, *blocks_deadline, *blocks_priority, *blocks_create_time;
  uint64_t block_num;
};

uint64_t SolutionSelectPacket(struct Blocks blocks) {
  /************** START CODE HERE ***************/
  return best_block_id_you_select;
  /************** END CODE HERE ***************/
}
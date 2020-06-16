#include "aitrans.h"
#include <stdint.h>

// struct Blocks {
//   uint64_t *blocks_id, *blocks_deadline, *blocks_priority,
//   *blocks_create_time; uint64_t block_num;
// };

uint64_t SolutionSelectPacket(struct Blocks blocks, uint64_t current_time) {
  /************** START CODE HERE ***************/
  // return the id of block you want to send, for example:
  return blocks.blocks_id[0];
  /************** END CODE HERE ***************/
}

uint64_t SolutionCcTrigger(char *event_type, uint64_t cwnd) {
  /************** START CODE HERE ***************/
  // return new cwnd, for example:
  uint64_t new_cwnd;
  if (event_type[11] == 'F')
    new_cwnd = cwnd + 1350; // EVENT_TYPE_FINISHED
  if (event_type[11] == 'D')
    new_cwnd = cwnd / 2; // EVENT_TYPE_DROP
  return new_cwnd;
  /************** END CODE HERE ***************/
}

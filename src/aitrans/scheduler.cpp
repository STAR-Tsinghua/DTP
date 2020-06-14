#include <cstdio>
#include <cstdint>
#include <cstdlib>


struct Blocks
{
    uint64_t *blocks_id, *blocks_deadline, *blocks_priority, *blocks_create_time;
    uint64_t block_num;
};

uint64_t SolutionSelectPacket(Blocks blocks){
    return blocks.blocks_id[0];
}

extern "C" uint64_t CSelectBlock(char *blocks_string, uint64_t block_num)
{
    fprintf(stderr, "%s\n", blocks_string);
    Blocks blocks;
    blocks.block_num = block_num;
    blocks.blocks_id = (uint64_t *)malloc(block_num * sizeof(uint64_t));
    blocks.blocks_deadline = (uint64_t *)malloc(block_num * sizeof(uint64_t));
    blocks.blocks_priority = (uint64_t *)malloc(block_num * sizeof(uint64_t));
    blocks.blocks_create_time = (uint64_t *)malloc(block_num * sizeof(uint64_t));
    for (uint64_t i = 0; i < block_num; i++)
    {
        blocks.blocks_id[i] = strtoull(blocks_string, &blocks_string, 10);
        blocks.blocks_deadline[i] = strtoull(blocks_string, &blocks_string, 10);
        blocks.blocks_priority[i] = strtoull(blocks_string, &blocks_string, 10);
        blocks.blocks_create_time[i] = strtoull(blocks_string, &blocks_string, 10);
    }
    /*********** Player's Code Here*************/
    return SolutionSelectPacket(blocks);
    /*********** Player's Code End*************/
}
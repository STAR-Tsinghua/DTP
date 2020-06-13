#include <cstdio>
#include <cstdint>
#include <cstdlib>

extern "C" uint64_t CselectBlock(char* blocks_string, uint64_t block_num) {
    fprintf(stderr, "%s\n",blocks_string);
    uint64_t* block_id=(uint64_t*)malloc(block_num*sizeof(uint64_t));
    for(uint64_t i=0;i<block_num;i++){
        uint64_t id = strtoull(blocks_string, &blocks_string, 10);
        // uint64_t deadline = strtoull(blocks_string, &blocks_string, 10);
        // uint64_t priority = strtoull(blocks_string, &blocks_string, 10);
        // uint64_t create_time = strtoull(blocks_string, &blocks_string, 10);
        block_id[i] = id;
    }    
    return block_id[0];
}
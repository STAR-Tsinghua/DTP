/// A dtp config refer to a set of dtp sending configuration
/// Such configurations are always saved in 'aitrans_block.txt' file
/// An exmaple:
/// | send_time_gap (s) | deadline (ms) | block_size (B) | priority (1, 2, 3)|
/// | -- | -- | -- | -- | 
/// |0.015025005 |  200 |   1235 |   1 |
/// | the gap between two adjasent sending | the deadline goal | the block size | the priority. Higher the number is, higher the priority is.| 

#ifndef CONFIG_PD_H
#define CONFIG_PD_H

#include <ctype.h>
#include <string.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>

__uint64_t getCurrentUsec()  //usec
{
    struct timeval tv;
    gettimeofday(&tv, NULL);  //该函数在sys/time.h头文件中
    return tv.tv_sec * 1000*1000 + tv.tv_usec;
}

struct dtp_config {
    int deadline;   // ms
    int priority;   //
    int block_size; // byte
    float send_time_gap;//s
};

typedef struct dtp_config dtp_config;



// return: config array, you need to release it manually
// number: return the number of parsed dtp_config (MAX=10).
struct dtp_config* parse_dtp_config(const char *filename, int *number)
{
    FILE *fd = NULL;

    float send_time_gap;
    int deadline;
    int priority;
    int block_size;

    int cfgs_len = 0;
    static int max_cfgs_len = 10000;
    dtp_config *cfgs = malloc(sizeof(*cfgs) * max_cfgs_len);


    fd = fopen(filename, "r");
    if (fd == NULL) {
        printf("fail to open config file.");
        *number = 0;
        return NULL;
    }

    while (fscanf(fd, "%f %d %d %d", &send_time_gap, &deadline, &block_size, &priority) == 4) {
        cfgs[cfgs_len].send_time_gap = send_time_gap;
        cfgs[cfgs_len].deadline = deadline;
        cfgs[cfgs_len].block_size = block_size;
        cfgs[cfgs_len].priority = priority;

        cfgs_len ++;
    }

    printf("block num: %d\n", cfgs_len);

    *number = cfgs_len;
    fclose(fd);
    
    return cfgs;
}


#endif // CONFIG_PD_H






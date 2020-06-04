#ifndef CONFIG_PD_H
#define CONFIG_PD_H

#include <ctype.h>
#include <string.h>
#include <sys/time.h>

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
};

typedef struct dtp_config dtp_config;



// return: config array
// number: return the number of parsed dtp_config (MAX=10).
struct dtp_config* parse_dtp_config(const char *filename, int *number,int *MAX_SEND_TIMES)
{
    FILE *fd = NULL;

    int deadline;
    int priority;
    int block_size;

    int cfgs_len = 0;
    static int max_cfgs_len = 1000;
    dtp_config *cfgs = malloc(sizeof(*cfgs) * max_cfgs_len);


    fd = fopen(filename, "r");
    if (fd == NULL) {
        printf("fail to open config file.");
        *number = 0;
        return NULL;
    }

    fscanf(fd,"%d",MAX_SEND_TIMES);
    while (fscanf(fd, "%d %d %d", &deadline, &priority, &block_size) == 3) {
        cfgs[cfgs_len].deadline = deadline;
        cfgs[cfgs_len].priority = priority;
        cfgs[cfgs_len].block_size = block_size;

        cfgs_len ++;
    }
    *number = cfgs_len;
    fclose(fd);
    
    return cfgs;
}


#endif // CONFIG_PD_H






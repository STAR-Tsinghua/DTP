#ifndef CONFIG_PD_H
#define CONFIG_PD_H

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct dtp_config {
  int deadline;    // ms
  int priority;    //
  int block_size;  // byte
};

typedef struct dtp_config dtp_config;

// return: config array, need to be released
// number: return the number of parsed dtp_config (MAX=10).
// MAX_SEND_TIMES: given as the first integer in the config file
// config format: (deadline, priority, block_size)
// ```txt
//   100
//   200 1 1401
//   500 2 2401
// ```
struct dtp_config *parse_dtp_config(const char *filename, int *number,
                                    int *MAX_SEND_TIMES) {
  FILE *fd = NULL;

  int deadline;
  int priority;
  int block_size;

  int cfgs_len = 0;
  static int max_cfgs_len = 2000;
  dtp_config *cfgs = (dtp_config *) malloc(sizeof(struct dtp_config) * max_cfgs_len);

  fd = fopen(filename, "r");
  if (fd == NULL) {
    fprintf(stderr, "fail to open config file.");
    *number = 0;
    free(cfgs);
    return NULL;
  }

  fscanf(fd, "%d", MAX_SEND_TIMES);
  while (fscanf(fd, "%d %d %d", &deadline, &priority, &block_size) == 3) {
    cfgs[cfgs_len].deadline = deadline;
    cfgs[cfgs_len].priority = priority;
    cfgs[cfgs_len].block_size = block_size;

    cfgs_len++;
  }
  *number = cfgs_len;
  fclose(fd);

  return cfgs;
}

#endif  // CONFIG_PD_H

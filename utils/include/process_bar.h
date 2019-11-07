#ifndef _PROCESS_BAR_H
#define _PROCESS_BAR_H
#include "type.h"
#include <time.h>
#include <sys/time.h>
#define MAX_BLOCK_NUMS 51
struct BAR{
    char buf[MAX_BLOCK_NUMS];
    uint64 total_nums;
    uint8 block_nums;
    uint32 block_size;
    timeval start;
    BAR(uint64 size, uint8 block);
    void print(uint64 cnt);
    void fini();
};
#endif
#include <stdio.h>
#include <time.h>
#include "process_bar.h"
#define GET_TIME_GAP(lastTime,nowTime)\
        ((nowTime.tv_sec-lastTime.tv_sec)*1000.0 + (nowTime.tv_usec-lastTime.tv_usec)/1000.0)

BAR::BAR(uint64 size, uint8 block){
    this->total_nums = size;
    this->block_nums = block;
    this->block_size = size / block;
    gettimeofday(&this->start, NULL);
    this->buf[this->block_nums] = 0;
    for(uint8 i = 0; i < this->block_nums; ++i){
        this->buf[i] = ' ';
    }
}

void BAR::print(uint64 cnt){
    uint32 ind = 0;
    uint64 s = cnt > 0 ? cnt : 1;
    uint64 val = cnt;
    if(cnt != 0){
        --cnt;
        ind = cnt / this->block_size;
    }
    ++ind;
    ind = ind >= this->block_nums? this->block_nums:ind;

    while(ind--){
        this->buf[ind] = '=';
    }
    timeval now;
    gettimeofday(&now, NULL);
    double time_consume = now.tv_sec - this->start.tv_sec;
    int time_consume_min = int(time_consume / 60);
    int time_consume_sec = int(time_consume) - 60 * time_consume_min;

    double time_last = (double)(this->total_nums - s) / (double)s * time_consume;
    int time_last_min = int(time_last / 60);
    int time_last_sec = int(time_last) - 60 * time_last_min;
    printf("\r[%d/%d %.2f\%][%s] use: %dm %02ds remain: %dm %02ds\n\b", val, this->total_nums, 100.0 * s/(double)this->total_nums
            , this->buf, time_consume_min, time_consume_sec, time_last_min, time_last_sec);
}

void BAR::fini(){
    timeval now;
    gettimeofday(&now, NULL);
    double time_consume = now.tv_sec - this->start.tv_sec;
    int time_consume_min = int(time_consume / 60);
    int time_consume_sec = int(time_consume) - 60 * time_consume_min;
    this->print(this->total_nums);
    printf("\ntotal time consume: %dm %02ds\n", time_consume_min, time_consume_sec);
}
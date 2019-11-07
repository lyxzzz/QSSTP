#include "rtt.h"
#include <math.h>
#include <sys/time.h>
#include <time.h>

#define ABS(x, y)\
        (x > y ? x - y : y - x)

#define GET_TIME_GAP(lastTime,nowTime)\
        ((nowTime.tv_sec-lastTime.tv_sec)*1000.0 + (nowTime.tv_usec-lastTime.tv_usec)/1000.0)

void RTT::estimate(void* lastack){
    timeval now;
    gettimeofday(&now, NULL);
    double SampleRTT = GET_TIME_GAP((*(timeval*)lastack), now);
    SampleRTT *= 2;

    if(firsttime){
        firsttime = false;
        this->EstimatedRTT = SampleRTT;
        this->Deviation = SampleRTT / 2;
    }else{
        this->Deviation = (1 - RTT_B) * this->Deviation + RTT_B * ABS(SampleRTT, this->EstimatedRTT);
        this->EstimatedRTT = (1 - RTT_X) * this->EstimatedRTT + RTT_X * SampleRTT;
    }
}
uint32 RTT::get_timeout(){
    return ceil(this->EstimatedRTT + this->gamma * this->Deviation);
}
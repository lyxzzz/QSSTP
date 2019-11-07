#ifndef _RTT_H
#define _RTT_H
#include "type.h"
#define RTT_X 0.125
#define RTT_B 0.25
struct RTT{
    double EstimatedRTT;
    double Deviation;
    uint32 gamma;
    bool firsttime;
    RTT(uint32 gamma):firsttime(true), gamma(gamma), EstimatedRTT(0), Deviation(0){}
    void estimate(void* lastack);
    uint32 get_timeout();
};
#endif
#ifndef _TIMER_H
#define _TIMER_H
#include "type.h"
#include "stp.h"

#define CALLBACK_MAX_ID 100

typedef void(STP_DATA::*TIMEOUT_FUNC)(void*);
typedef unsigned long long timeid_t;

enum TIMINGWHEEL_ERROR{
    WHEEL_NOERROR=0,
    WHEEL_INITMANAGER_POOL_ERROR=-1,
    WHEEL_INITNODE_POOL_ERROR=-2,
    WHEEL_INITMANAGER_ERROR=-3,
    WHEEL_INITNODE_ERROR=-4,
    WHEEL_TOOMUCHEVENTS_ERROR=-5,
    WHEEL_TOOLONGTIME_ERROR=-6
};

#pragma pack(4)
struct TIMER_EVENT{
    int32 timeout;
    void* data;
    int8 callbackID;
    TIMER_EVENT(int32 timeout, void* data, int8 callbackid);
};

struct TIMINGWHEEL{
    TIMINGWHEEL(uint32 max_events, uint32 interval, STP_DATA* stp, TIMEOUT_FUNC default_func);
    int setTimer(const TIMER_EVENT& event,timeid_t& timingID);
    void removeTimer(timeid_t timingID);
    void roll();
    void register_callback(int8 callbackid,TIMEOUT_FUNC func);
    char* gettime();
};
#pragma pack()

#endif
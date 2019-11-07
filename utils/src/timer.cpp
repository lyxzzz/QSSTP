#include "timer.h"
#include <sys/time.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SET_PRINTF 0
#define TICK_PRINTF 0
#define BOTTOM_CYCLE_SIZE 256
#define UPPER_CYCLE_SIZE 64
#define UPPER_WHEEL_SIZE 2
#define GET_TIME_GAP(lastTime,nowTime)\
        ((nowTime.tv_sec-lastTime.tv_sec)*1000.0 + (nowTime.tv_usec-lastTime.tv_usec)/1000.0)
#pragma pack(4)
struct WHEELNODE{
    TIMER_EVENT event;
    WHEELNODE* prev;
    WHEELNODE* next;
    bool del;
    WHEELNODE():event(0, 0, 0), prev(NULL), next(NULL), del(false){}
};
typedef struct WHEELNODE *LPWHEELNODE;
struct WHEEL{
    LPWHEELNODE* cycle;
    LPWHEELNODE* tail;
    uint32 unit_length;
    uint16 wheel_size;
    uint16 now_ptr;
};
struct TIMINGWHEEL_DATA{
    WHEEL wheels[UPPER_WHEEL_SIZE + 1];
    uint32 maxTime[UPPER_WHEEL_SIZE + 1];
    uint32 event_cnt;
    uint32 max_events;
    uint32 interval;
    uint64 tick;

    timeval sys_time[2];
    bool time_ptr;

    timeval start_time;
    double time_accumlation;
    double last_time;

    char now_time[60];

    STP_DATA* stp;
};
#pragma pack()

static bool __init = false;
static TIMEOUT_FUNC __callback[CALLBACK_MAX_ID];
static TIMINGWHEEL_DATA __stimerwheel;

void defaultTimerFunc(uint32 id)
{
    double time = __stimerwheel.last_time / 1000.0;
    printf("time[%.2fs]tick[%d] ID[%d]\n", time, __stimerwheel.tick, id);
}

TIMER_EVENT::TIMER_EVENT(int32 timeout, void* data, int8 callbackid){
    this->timeout = timeout;
    this->data = data;
    this->callbackID = callbackid;
}

TIMINGWHEEL::TIMINGWHEEL(uint32 max_events, uint32 interval, STP_DATA* stp, TIMEOUT_FUNC default_func){
    if(!__init){
        memset(&__stimerwheel, 0, sizeof(__stimerwheel));
        __stimerwheel.stp = stp;
        __stimerwheel.event_cnt = 0;
        __stimerwheel.max_events = max_events;
        __stimerwheel.interval = interval;
        __stimerwheel.tick = 0;
        __stimerwheel.time_ptr = true;

        uint32 unit_len = 1;
        uint16 wheel_size = BOTTOM_CYCLE_SIZE;
        for(int i = 0; i < UPPER_WHEEL_SIZE + 1; ++i){
            WHEEL& w = __stimerwheel.wheels[i];
            w.now_ptr = 0;
            w.wheel_size = wheel_size;
            w.unit_length = unit_len;
            w.cycle = new LPWHEELNODE[w.wheel_size];
            w.tail = new LPWHEELNODE[w.wheel_size];
            for(int j = 0; j < w.wheel_size; ++j){
                w.cycle[j] = new WHEELNODE;
                w.tail[j] = new WHEELNODE;
                w.cycle[j]->next = w.tail[j];
                w.tail[j]->prev = w.cycle[j];
            }
            __stimerwheel.maxTime[i] = w.wheel_size * w.unit_length;
            wheel_size = UPPER_CYCLE_SIZE;
            unit_len = __stimerwheel.maxTime[i];
        }

        for(int i=0;i<CALLBACK_MAX_ID;++i)
        {
            __callback[i]=default_func;
        }
        gettimeofday(&__stimerwheel.sys_time[0],NULL);
        gettimeofday(&__stimerwheel.start_time, NULL);
        __stimerwheel.time_accumlation = 0.0;
        __stimerwheel.last_time = 0.0;

        __init = true;
    }
}

void TIMINGWHEEL::register_callback(int8 callbackid, TIMEOUT_FUNC func){
    if(callbackid >= CALLBACK_MAX_ID){
        return;
    }
    __callback[callbackid]=func;
}

void __insert(LPWHEELNODE tail, LPWHEELNODE val){
    val->prev = tail->prev;
    if(val->prev){
        val->prev->next = val;
    }
    tail->prev = val;
    val->next = tail;
}

LPWHEELNODE __removeNext(LPWHEELNODE head){
    if(head->next){
        LPWHEELNODE r = head->next;
        head->next = r->next;
        r->prev = NULL;
        if(r->next){
            r->next->prev = head;
        }
        r->next = NULL;
        return r;
    }else{
        return NULL;
    }
}

void __delete(LPWHEELNODE node){
    LPWHEELNODE p = node->prev;
    LPWHEELNODE q = node->next;
    if(p){
        p->next = q;
    }
    if(q){
        q->prev = p;
    }
    node->prev = NULL;
    node->next = NULL;
    __stimerwheel.event_cnt = __stimerwheel.event_cnt - (node->del?0:1);
    delete node;
}

int TIMINGWHEEL::setTimer(const TIMER_EVENT& event,timeid_t& timingID){
    if(__stimerwheel.event_cnt >= __stimerwheel.max_events){
        return WHEEL_TOOMUCHEVENTS_ERROR;
    }
    if(event.timeout == 0){
        (__stimerwheel.stp->*__callback[event.callbackID])(event.data);
        return WHEEL_NOERROR;
    }
    if(event.timeout>=__stimerwheel.maxTime[UPPER_WHEEL_SIZE])
    {
        return WHEEL_TOOLONGTIME_ERROR;
    }

    uint32 time_stamp=event.timeout;
    uint32 last_ptr = 0;
    for(uint8 wheel_ind = 0; wheel_ind < UPPER_WHEEL_SIZE + 1; ++wheel_ind){
        WHEEL& w = __stimerwheel.wheels[wheel_ind];
        if(event.timeout < __stimerwheel.maxTime[wheel_ind]){
            uint16 box = (time_stamp/w.unit_length - 1 + w.now_ptr) % w.wheel_size;
            LPWHEELNODE p = new WHEELNODE;
            p->event = event;
            p->event.timeout = time_stamp;

            __insert(w.tail[box], p);
            
            timingID = (timeid_t)p;
            ++__stimerwheel.event_cnt;
            return WHEEL_NOERROR;
        }else{
            time_stamp += (w.now_ptr * w.unit_length);
        }
    }
    return WHEEL_TOOLONGTIME_ERROR;
}

void TIMINGWHEEL::removeTimer(timeid_t timingID){
    LPWHEELNODE p = (LPWHEELNODE)timingID;
    // p->prev->next = p->next;
    // if(p->next){
    //     p->next->prev = p->prev;
    // }
    // p->next = NULL;
    // p->prev = NULL;
    // delete p;
    p->del = true;
    --__stimerwheel.event_cnt;
}

void __deliverNode(int layer){
    if(layer >= UPPER_WHEEL_SIZE + 1){
        return;
    }
    WHEEL& w = __stimerwheel.wheels[layer];
    if(w.now_ptr == w.wheel_size - 1){
        __deliverNode(layer+1);
    }
    if(layer == 0){
        return;
    }
    WHEEL& lower_w = __stimerwheel.wheels[layer-1];
    LPWHEELNODE head = w.cycle[w.now_ptr];
    LPWHEELNODE tail = w.tail[w.now_ptr];
    LPWHEELNODE node = __removeNext(head);
    while(node != tail){
        if(node->del){
            
        }else{
            node->event.timeout %= w.unit_length;
            uint16 box = (lower_w.now_ptr + node->event.timeout / lower_w.unit_length) % lower_w.wheel_size;
            __insert(lower_w.tail[box], node);
        }
        node = __removeNext(head);
    }
    head->next = tail;
    tail->prev = head;

    ++w.now_ptr;
    if(w.now_ptr == w.wheel_size){
        w.now_ptr = 0;
    }
}

void __tick(){
    ++__stimerwheel.tick;
    __deliverNode(0);

    WHEEL& bot = __stimerwheel.wheels[0];
    LPWHEELNODE head = bot.cycle[bot.now_ptr];
    LPWHEELNODE tail = bot.tail[bot.now_ptr];
    LPWHEELNODE node = __removeNext(head);
    LPWHEELNODE prev;
    bool istick = false;
    while(node != tail){
        if(!node->del){
            istick = true;
            (__stimerwheel.stp->*__callback[node->event.callbackID])(node->event.data);
        }
        __delete(node);
        node = __removeNext(head);
    }
    if(istick){
        // printf("tick[%d]\n", __stimerwheel.tick);
    }
    head->next = tail;
    tail->prev = head;

    ++bot.now_ptr;
    if(bot.now_ptr == bot.wheel_size){
        bot.now_ptr = 0;
    }
}

void TIMINGWHEEL::roll(){
    timeval* sys_time = __stimerwheel.sys_time;
    gettimeofday(&sys_time[__stimerwheel.time_ptr], NULL);
	__stimerwheel.time_accumlation += GET_TIME_GAP(sys_time[!__stimerwheel.time_ptr], sys_time[__stimerwheel.time_ptr]);
    __stimerwheel.last_time += GET_TIME_GAP(sys_time[!__stimerwheel.time_ptr], sys_time[__stimerwheel.time_ptr]);
	if(__stimerwheel.time_accumlation >= __stimerwheel.interval)
	{
		uint32 times = uint32(__stimerwheel.time_accumlation/__stimerwheel.interval);
		__stimerwheel.time_accumlation = __stimerwheel.time_accumlation - times * __stimerwheel.interval;
		while(times--)
		{
			__tick();
		}
	}
	__stimerwheel.time_ptr = !__stimerwheel.time_ptr;
}

char* TIMINGWHEEL::gettime(){
    timeval now_time;
    gettimeofday(&now_time, NULL);
    double time = GET_TIME_GAP(__stimerwheel.start_time, now_time);
    sprintf(__stimerwheel.now_time, "time[%.2fms]", time);
    return __stimerwheel.now_time;
}
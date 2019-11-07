#ifndef _FSM_H
#define _FSM_H
#include "type.h"
#define MAX_FSM_STATE_SIZE 30
#define MAX_FSM_SIMULATE_SIZE 30

typedef void(*FSM_FUNC)(void* data);
struct FSM_ELE{
    FSM_FUNC func;
    uint8 new_state;
};
struct FSM_FUNC_SET{
    uint8 state;
    uint8 new_state;
    uint16 simulate;
    FSM_FUNC func;
};
struct FSM{
    FSM_ELE fsmfunc[MAX_FSM_STATE_SIZE][MAX_FSM_SIMULATE_SIZE];
    void set_default(FSM_ELE func);
    void set_func(FSM_FUNC_SET* func_set, uint16 size);
    void run(uint8& state, uint16 simulate, void* data);
};
#endif
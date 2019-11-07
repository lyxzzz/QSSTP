#include "fsm.h"

void FSM::set_default(FSM_ELE func){
    for(uint8 i = 0; i < MAX_FSM_STATE_SIZE; ++i){
        for(uint16 j = 0; j < MAX_FSM_SIMULATE_SIZE; ++j){
            this->fsmfunc[i][j] = {func.func, -1};
        }
    }
}

void FSM::set_func(FSM_FUNC_SET* func_set, uint16 size){
    for(uint16 i = 0; i < size; ++i){
        FSM_FUNC_SET& s = func_set[i];
        if(s.state < MAX_FSM_STATE_SIZE && s.simulate < MAX_FSM_SIMULATE_SIZE){
            this->fsmfunc[s.state][s.simulate] = {s.func, s.new_state};
        }
    }
}

void FSM::run(uint8& state, uint16 simulate, void* data){
    if(state < MAX_FSM_STATE_SIZE && simulate < MAX_FSM_SIMULATE_SIZE){
        uint8 oldstate = state;
        if(this->fsmfunc[oldstate][simulate].new_state != 0xff){
            state = this->fsmfunc[oldstate][simulate].new_state;
        }
        this->fsmfunc[oldstate][simulate].func(data);
    }
}
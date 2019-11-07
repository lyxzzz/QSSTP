#include <stdio.h>
#include <stdlib.h>
#include "PLD.h"
#include "stp.h"

PLD::PLD(double pdrop, double pdelay, uint32 MaxDelay, int seed){
    this->pdrop = pdrop;
    this->pdelay = pdelay;
    this->MaxDelay = MaxDelay;
    srand(seed);
}

bool PLD::random_drop(int state){
    if(state == NORMAL){
        double p = rand()/((double)(RAND_MAX)+1);
        return p <= this->pdrop;
    }else{
        return true;
    }
}

uint32 PLD::random_delay(int state){
    if(state != NORMAL){
        return 0;
    }
    double p = rand()/((double)(RAND_MAX)+1);
    if(p <= this->pdelay){
        p = rand()/((double)(RAND_MAX)+1);
        uint32 r = this->MaxDelay * p;
        return r;
    }else{
        return 0;
    }
}
#ifndef _PLD_H
#define _PLD_H
#include "type.h"
struct PLD{
    double pdrop;
    double pdelay;
    uint32 MaxDelay;
    PLD(double pdrop, double pdelay, uint32 MaxDelay, int seed);
    bool random_drop(int state);
    uint32 random_delay(int state);
};
#endif
#ifndef _STP_H
#define _STP_H
#include "type.h"
#include "transport.h"

#define OVER_ACK_TIMES 3

enum STP_STATE{
    SYN_SENT,
    SYN_RCVD,
    ESTABLISHED,
    FIN_WAIT_1,
    CLOSE_WAIT,
    FIN_WAIT_2,
    LAST_ACK,
    TIME_WAIT,
    CLOSE
};
enum STP_PKG_STATE{
    ACTIVE_CONNECT,
    PASSIVE_CONNECT,
    SYN,
    NORMAL,
    S_ACTIVE_FINI,
    FINI,
    FINI_ACK,
    OVERTIME,
    DROP
};
struct UUID{
    long a;
    long b;
};

struct STP_HEADER{
    bool SYN;
    bool FIN;
    bool ACK;
    uint8 type;
    uint8 ord;
    uint64 seq;
    uint64 ack;
    uint64 len;
    uint32 mws;
    UUID uuid;
};

struct STP_DATA;
struct STP{
    STP_DATA* stpdata;
    STP(bool listener, uint16 port, int loglevel, const char* logpath, const char* ip=ANY_IP);
    void setmws(uint32 mws);
    void setmss(uint32 mss);
    void settimeout(uint32 timeout);
    void usePLD(double pdrop, double pdelay, uint32 MaxDelay, int seed);
    void setgamma(uint32 gamma);
    int send(void* data, uint32 len);
    int recv(void* data, uint32 len);
    int init();
    int fini();
};
#endif
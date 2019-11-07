#include "stp.h"
#include "transport.h"
#include "timer.h"
#include "list.h"
#include "fsm.h"
#include "logger.h"
#include "PLD.h"
#include "rtt.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <map>
#include <sys/time.h>
using std::map;

#define MAX_PKG_SIZE 16384

enum STP_TIMER{
    TIMER_RESEND,
    TIMER_WAIT_CLOSE,
    TIMER_DELAY,
    TIMER_NULL
};

struct TMP_DATA{
    STP_HEADER header;
    char* buf;
    uint8 callbackid;
    uint32 timeout;
    uint32 send_times;
    uint32 ack_times;
    uint32 offset;
};

struct RECORD{
    uint32 times;
    uint32 drops;
    uint32 len;
    uint32 delay;
    RECORD():times(0), drops(0), len(0), delay(0){}
    RECORD(uint32 a, uint32 b, uint32 c, uint32 d):times(a), drops(b), len(c), delay(d){}
};

void STP_ERROR_FUNC(void* data);
void CONNECTING(void* data);
void ACCEPTING(void* data);
void SYN_SENT_SUCCESS(void* data);
void SYN_RCVD_SUCCESS(void* data);
void NORMAL_RECV(void* data);
void ACTIVE_FINI(void* data);
void PASSIVE_FINI(void* data);
void TIME_WAIT_CLOSE(void* data);
void CLOSING(void* data);
FSM_FUNC_SET fsm_func_set[9]={
    {CLOSE,         SYN_SENT,       ACTIVE_CONNECT,     CONNECTING},
    {CLOSE,         SYN_RCVD,       PASSIVE_CONNECT,    ACCEPTING},
    {SYN_SENT,      ESTABLISHED,    SYN,                SYN_SENT_SUCCESS},
    {SYN_RCVD,      ESTABLISHED,    SYN,                SYN_RCVD_SUCCESS},
    {ESTABLISHED,   ESTABLISHED,    NORMAL,             NORMAL_RECV},
    {ESTABLISHED,   FIN_WAIT_1,     S_ACTIVE_FINI,      ACTIVE_FINI},
    {ESTABLISHED,   LAST_ACK,       FINI,               PASSIVE_FINI},
    {FIN_WAIT_1,    TIME_WAIT,      FINI_ACK,           TIME_WAIT_CLOSE},
    {LAST_ACK,      CLOSE,          FINI_ACK,           CLOSING}
};

static const char* __stpstate_str[9] = {
    "SYN_SENT", "SYN_RCVD", "ESTABLISHED", "FIN_WAIT_1",
    "CLOSE_WAIT", "FIN_WAIT_2", "LAST_ACK", "TIME_WAIT", "CLOSE"
};
static const char* __stppkgstate_str[9] = {
    "ACTIVE_CONNECT", "PASSIVE_CONNECT", "SYN", "NORMAL", "S_ACTIVE_FINI", "FINI", "FINI_ACK", "OVERTIME", "DROP"
};
static char __pkg_state_buf[5];
char* getpkg_state(STP_HEADER& header){
    uint8 ptr = 0;
    if(header.SYN){
        __pkg_state_buf[ptr++] = 'S';
    }else if(header.FIN){
        __pkg_state_buf[ptr++] = 'F';
    }
    if(header.ACK){
        __pkg_state_buf[ptr++] = 'A';
    }
    if(header.len != 0){
        __pkg_state_buf[ptr++] = 'D';
    }
    __pkg_state_buf[ptr] = 0;
    return __pkg_state_buf;
}

struct STP_DATA{
    char sendbuf[MAX_PKG_SIZE];
    char recvbuf[MAX_PKG_SIZE];

    TRANS trans;
    LIST send_list;
    LIST recv_list;
    uint32 mws;
    uint32 mss;

    uint32 timeout;

    bool usePLD;
    PLD pld;

    uint64 nowseq;
    uint64 nowack;
    uint8 state;

    RTT rtt;

    TIMINGWHEEL timer;

    timeid_t fini_timer;

    bool rto_use;
    timeid_t rto_timer;

    FSM fsm;
    LOGGER logger;

    map<uint32, RECORD> send_times;
    map<uint32, RECORD> recv_times;

    void __log_stp_header(STP_HEADER& header, const char* title);
    void __log_pkg_times_info(bool send, int level=INFO);

    void __wrap_header(STP_HEADER& header, bool syn, bool fin, bool ack, uint8 type);
    void __wrap_timer(TMP_DATA& pkg, uint8 callid, uint32 timeout);

    void __ack_send(uint64 ack, UUID lastack);
    void __check_recv();

    void __rto_timer_set(uint32 timeout);

    void __send_ack(uint16 header_type);
    void __send_pkg(void* data, bool set_rto=true);

    void __timer_error(void* data);
    void __timer_resend_pkg(void* data);
    void __timer_time_fini(void* data);
    void __timer_delay_send(void* data);

    

    void simulate(uint16 simulate);
    int send(void* data, uint32 size);
    void recv();
    


    STP_DATA(bool listener, uint16 port, int loglevel, const char* logpath, const char* ip):
            trans(listener, port, 1, ip), mws(4096), mss(512), timeout(100), 
            usePLD(false), nowseq(0), nowack(0), state(CLOSE), rtt(4),
            timer(1024*16, 1, this, &STP_DATA::__timer_error), logger(logpath, loglevel), pld(0.0, 0.0, 0, 50){
        srand(time(NULL));
        double p = rand()/((double)(RAND_MAX)+1);
        this->nowseq = uint32(p * 1000);

        // bool extra_set[LOGGER_LEVEL_SIZE] = {false};
        // extra_set[INFO] = true;
        // this->logger.add_log_file("log/info.log", extra_set);

        this->timer.register_callback(TIMER_RESEND, &STP_DATA::__timer_resend_pkg);
        this->timer.register_callback(TIMER_WAIT_CLOSE, &STP_DATA::__timer_time_fini);
        this->timer.register_callback(TIMER_DELAY, &STP_DATA::__timer_delay_send);
        

        this->fsm.set_default({STP_ERROR_FUNC, (uint8)-1});
        this->fsm.set_func(fsm_func_set, sizeof(fsm_func_set));

        this->fini_timer = 0;
        
        this->rto_timer = 0;
        this->rto_use = false;
    }
};

void STP_DATA::__log_stp_header(STP_HEADER& header, const char* title){
    tlog_trace(this->logger, "==============%s==============\n", title);
    tlog_trace(this->logger, "\t\tSTN[%d]\tFIN[%d]\tACK[%d]\n", header.SYN, header.FIN, header.ACK);
    tlog_trace(this->logger, "\t\ttype[%s]\tord[%d]\tuuid[%d%d]\n", __stppkgstate_str[header.type], header.ord, header.uuid.a, header.uuid.b);
    tlog_trace(this->logger, "\t\tseq[%d]\tack[%d]\tlen[%d]\n", header.seq, header.ack, header.len);
    tlog_trace(this->logger, "\t\tstp state[%s]\n", __stpstate_str[this->state]);
    tlog_trace(this->logger, "====================================\n", title);
}

void STP_DATA::__log_pkg_times_info(bool send, int level){
    if(level != REQUIRE){
        if(send){
            tlog_trace(this->logger, "*******************send_pkg_times_info*******************\n");
            for(auto it = this->send_times.begin(); it != this->send_times.end(); ++it){
                tlog_trace(this->logger, "\t\t%d:%d(%d)\tlen:%d\n", it->first, it->second.times, it->second.drops, it->second.len);
            }
        }else{
            tlog_trace(this->logger, "*******************recv_pkg_times_info*******************\n");
            for(auto it = this->recv_times.begin(); it != this->recv_times.end(); ++it){
                 tlog_trace(this->logger, "\t\t%d:%d\tlen:%d\n", it->first, it->second.times, it->second.len);
            }
        }
        tlog_trace(this->logger, "**********************************************************\n");
    }else{
        if(send){
            uint64 total_send = 0;
            uint64 total_pkg_send = 0;
            uint64 total_drop = 0;
            uint64 total_delay = 0;
            uint64 need_ack = 0;
            tlog_require(this->logger, "*******************send_pkg_times_info*******************\n");
            for(auto it = this->send_times.begin(); it != this->send_times.end(); ++it){
                tlog_require(this->logger, "\t\t%d:%d(%d,%d)\tlen:%d\n", it->first, it->second.times, it->second.drops, it->second.delay, it->second.len);
                total_send += (it->second.times * it->second.len);
                total_pkg_send += it->second.times;
                total_drop += it->second.drops;
                total_delay += it->second.delay;
                ++need_ack;
            }
            tlog_require(this->logger, "total_send:%-12dtotal_pkg_send:%-8dtotal_drop:%-8total_delay:%-8ddneed_ack:%-5d\n", 
                                total_send, total_pkg_send, total_drop, total_delay, need_ack);
        }else{
            uint64 duplicate_ack = 0;
            uint64 duplicate_times = 0;
            uint64 total_pkg_recv = 0;
            tlog_require(this->logger, "*******************recv_pkg_times_info*******************\n");
            for(auto it = this->recv_times.begin(); it != this->recv_times.end(); ++it){
                tlog_require(this->logger, "\t\t%d:%d\tlen:%d\n", it->first, it->second.times, it->second.len);
                duplicate_ack += bool((it->second.times - 1) > 0);
                total_pkg_recv += it->second.times;
                duplicate_times += (it->second.times - 1);
            }
            tlog_require(this->logger, "total_pkg_recv:%-8dduplicate_times:%-8dduplicat_ack:%-d\n", total_pkg_recv, duplicate_times, duplicate_ack);
        }
        tlog_require(this->logger, "**********************************************************\n");

    }
}

void STP_DATA::__wrap_header(STP_HEADER& header, bool syn, bool fin, bool ack, uint8 type){
    header.SYN = syn;
    header.FIN = fin;
    header.ACK = ack;
    header.type = type;
    header.seq = this->nowseq;
    header.ack = this->nowack;
}

void STP_DATA::__check_recv(){
    NODE* head = this->recv_list.head;
    NODE* p =this->recv_list.getNext(head);
    tlog_debug(this->logger, "check_recv: seq num equal to %d will be recv\n", this->nowack);
    while(p){
        TMP_DATA* data = (TMP_DATA*)this->recv_list.getvalue(p);
        this->__log_stp_header(data->header, "check_recv");
        if(data->header.seq == this->nowack){
            this->nowack += data->header.len;
            tlog_debug(this->logger, "recv a pkg and num become %d\n", this->nowack);
        }else if(data->header.seq > this->nowack){
            tlog_debug(this->logger, "check_recv fini: seq num is %d\n", data->header.seq);
            break;
        }
        p = (NODE*)this->recv_list.getNext(p);
    }
}

void STP_DATA::__ack_send(uint64 ack, UUID lastack){
    NODE* head = this->send_list.head;
    NODE* p = this->send_list.getNext(head);
    tlog_debug(this->logger, "ACK_SEN: seq num less than %d will be ack\n", ack);
    bool ack_data = false;
    bool over_ack = false;
    this->rtt.estimate(&lastack);
    tlog_require(this->logger, "change timeout from %d to %d\n", this->timeout, this->rtt.get_timeout());
    this->timeout = this->rtt.get_timeout();

    while(p){
        TMP_DATA* data = (TMP_DATA*)this->send_list.getvalue(p);
        this->__log_stp_header(data->header, "ack_send");
        if(data->header.seq + data->header.len <= ack){
            ack_data = true;
            this->send_list.remove(p);
            this->mws += data->header.len;
            if(data->header.len != 0){
                delete data->buf;
            }
            tlog_debug(this->logger, "del a pkg because it seq+len %d is acked by %d\n", 
                    data->header.seq + data->header.len, ack);
            delete data;
            p = (NODE*)this->send_list.getNext(head);
        }else{
            if(ack == data->header.seq){
                ++data->ack_times;
            }
            if(data->ack_times >= OVER_ACK_TIMES){
                over_ack = true;
            }
            tlog_debug(this->logger, "ACK_SEN FIN: data seq is %d\n", data->header.seq);
            break;
        }
    }
    if(this->rto_use){
        if(ack_data){
            this->timer.removeTimer(this->rto_timer);
            this->rto_use = false;
            if(this->send_list.size != 0){
                this->__rto_timer_set(this->timeout);
            }
        }else if(over_ack){
            p = this->send_list.getNext(head);
            TMP_DATA* over_ack_pkg = (TMP_DATA*)this->send_list.getvalue(p);
            tlog_info(this->logger, "over ack: data seq is %d\n", over_ack_pkg->header.seq);
            this->__send_pkg(over_ack_pkg);
        }
    }
}

void STP_DATA::__rto_timer_set(uint32 timeout){
    if(!this->rto_use){
        TIMER_EVENT e(timeout, this, TIMER_RESEND);
        this->timer.setTimer(e, this->rto_timer);
        this->rto_use = true;
    }
}

void STP_DATA::__send_pkg(void* data, bool set_rto){
    TMP_DATA* pkg = (TMP_DATA*)data;
    uint32 size = pkg->header.len;
    
    ++pkg->send_times;
    gettimeofday((timeval*)&pkg->header.uuid, NULL);

    const char* type_str = "snd";
    bool isdrop = false;
    bool isdelay = false;

    if(size == 0){
        pkg->header.ord = 0;
        this->trans.send(&pkg->header, sizeof(pkg->header));
    }else{
        bool flag = this->pld.random_drop(pkg->header.type);

        if(this->usePLD && flag){
            type_str = "drop";
            isdrop = true;
        }else{
            uint32 delay_time = this->pld.random_delay(pkg->header.type);
            if(delay_time == 0){
                uint32 max_size = MAX_PKG_SIZE - sizeof(pkg->header);
                int32 ord = (size - 1) / max_size;
                uint32 index = 0;

                while(ord >= 0){
                    pkg->header.ord = ord;
                    memcpy(this->sendbuf, &pkg->header, sizeof(pkg->header));

                    uint32 sendsize = size < max_size? size:max_size;
                    memcpy(&this->sendbuf[sizeof(pkg->header)], &pkg->buf[index], sendsize);
                    
                    this->trans.send(this->sendbuf, sendsize + sizeof(pkg->header));
                    
                    index += sendsize;
                    size -= sendsize;
                    --ord;
                }

            }else{
                isdelay = true;
                tlog_require(this->logger, "delay pkg seq[%d] %d ms\n", pkg->header.seq, delay_time);
                TMP_DATA* delay_data = new TMP_DATA;
                memcpy(delay_data, pkg, sizeof(*delay_data));
                char* delay_buf = new char[pkg->header.len];
                delay_data->buf = delay_buf;
                memcpy(delay_buf, pkg->buf, pkg->header.len);
                TIMER_EVENT e(delay_time, delay_data, TIMER_DELAY);
                timeid_t tid;
                this->timer.setTimer(e, tid);
            }
        }
    }

    if(!pkg->header.FIN && !pkg->header.SYN && !pkg->header.ACK){
        if(this->send_times.find(pkg->header.seq) != this->send_times.end()){
            ++this->send_times[pkg->header.seq].times;
            if(isdrop){
                ++this->send_times[pkg->header.seq].drops;
            }
            if(isdelay){
                ++this->send_times[pkg->header.seq].delay;
            }
        }else{
            this->send_times[pkg->header.seq] = RECORD(1, (uint32)isdrop, pkg->header.len, (uint32)isdelay);
        }
    }
    if(set_rto){
        this->__rto_timer_set(this->timeout);
    }

    
    this->__log_pkg_times_info(true);
    this->__log_stp_header(pkg->header, "send");
    
    tlog_require(this->logger, "%s\t[%s]\t\tflag:%-5s\tseq:%-5d\tlen:%-5d\tack:%-5d\tid[%d%d]\n", 
                this->timer.gettime(), type_str, getpkg_state(pkg->header), 
                pkg->header.seq, pkg->header.len, pkg->header.ack,
                pkg->header.uuid.a, pkg->header.uuid.b);
}

void STP_DATA::__send_ack(uint16 header_type){
    TMP_DATA ack_pkg;
    memset(&ack_pkg, 0, sizeof(ack_pkg));
    this->__wrap_header(ack_pkg.header, false, false, true, header_type);
    
    this->__send_pkg(&ack_pkg, false);
}

void STP_DATA::__timer_delay_send(void* data){
    TMP_DATA* pkg = (TMP_DATA*)data;
    uint32 size = pkg->header.len;

    uint32 max_size = MAX_PKG_SIZE - sizeof(pkg->header);
    int32 ord = (size - 1) / max_size;
    uint32 index = 0;

    while(ord >= 0){
        pkg->header.ord = ord;
        memcpy(this->sendbuf, &pkg->header, sizeof(pkg->header));

        uint32 sendsize = size < max_size? size:max_size;
        memcpy(&this->sendbuf[sizeof(pkg->header)], &pkg->buf[index], sendsize);
        
        this->trans.send(this->sendbuf, sendsize + sizeof(pkg->header));
        
        index += sendsize;
        size -= sendsize;
        --ord;
    }
    tlog_require(this->logger, "%s real send pkg %d\n", this->timer.gettime(), pkg->header.seq);
    delete pkg->buf;
    delete pkg;
}

void STP_DATA::__timer_resend_pkg(void* data){
    STP_DATA* stp = (STP_DATA*)data;
    this->rto_use = false;
    stp->recv();
    if(this->send_list.size == 0){
        return;
    }

    NODE* head = this->send_list.head;
    NODE* p = this->send_list.getNext(head);

    TMP_DATA* resend_pkg = (TMP_DATA*)this->send_list.getvalue(p);

    this->__rto_timer_set(2 * this->timeout);
    this->__send_pkg(resend_pkg);
}

void STP_DATA::__timer_error(void* data){
    tlog_fatal(this->logger, "ERROR TIMER!\n");
    // this->trans.fini();
}

void STP_DATA::__timer_time_fini(void* data){
    tlog_debug(this->logger, "CLOSING!\n");
    this->state = CLOSE;
    // this->trans.fini();
}

void STP_DATA::simulate(uint16 simulate){
    this->recv();
    STP_HEADER* header = (STP_HEADER*)this->recvbuf;
    header->type = simulate;
    fsm.run(this->state, header->type, this);
}

void STP_DATA::recv(){
    uint32 size = MAX_PKG_SIZE;
    this->trans.recv(this->recvbuf, size);
    while(size != 0){
        STP_HEADER* header = (STP_HEADER*)this->recvbuf;
        this->__log_stp_header(*header, "recv");
        tlog_require(this->logger, "%s\t[recv]\t\tflag:%-5s\tseq:%-5d\tlen:%-5d\tack:%-5d\tid[%d%d]\n", 
                    this->timer.gettime(), getpkg_state(*header), 
                    header->seq, header->len, header->ack,
                    header->uuid.a, header->uuid.b);
        fsm.run(this->state, header->type, this);
        
        size = MAX_PKG_SIZE;
        this->trans.recv(this->recvbuf, size);

        this->timer.roll();
    }
    this->timer.roll();
}

int STP_DATA::send(void* data, uint32 size){
    this->recv();
    if(this->mws == 0){
        return 0;
    }else{
        uint32 send_size = size < this->mws? size:this->mws;
        this->mws -= send_size;

        for(uint32 s = 0; s < send_size; s+=this->mss){
            uint32 pkg_size = (send_size - s) < this->mss ? (send_size - s):this->mss;
            TMP_DATA* pkg = new TMP_DATA;
            memset(pkg, 0, sizeof(*pkg));
            this->__wrap_header(pkg->header, false, false, false, NORMAL);

            pkg->header.len = pkg_size;

            pkg->buf = new char[pkg_size];
            memcpy(pkg->buf, &((char*)data)[s], pkg_size);

            this->send_list.push_pri(pkg, pkg->header.seq);
            
            this->__send_pkg(pkg);
            this->nowseq += pkg_size;
        }
        return send_size;
    }
}

STP::STP(bool listener, uint16 port, int loglevel, const char* logpath, const char* ip){
    this->stpdata = new STP_DATA(listener, port, loglevel, logpath, ip);
}

int STP::init(){
    if(sizeof(timeval) != sizeof(STP_HEADER::uuid)){
        printf("bit diff %d %d\n", sizeof(timeval), sizeof(STP_HEADER::uuid));
        return -1;
    }
    stpdata->trans.init();
    if(stpdata->trans.listener){
        uint32 trytimes = 1000;
        while(stpdata->state != ESTABLISHED){
            stpdata->recv();
            usleep(1000 * 10);
            --trytimes;
            if(trytimes < 0){
                tlog_fatal(stpdata->logger, "can't connect to client\n");
                return -1;
            }
        }
        return 0;
    }else{
        uint32 trytimes = 1000;
        stpdata->simulate(ACTIVE_CONNECT);
        while(stpdata->state != ESTABLISHED){
            stpdata->recv();
            usleep(1000 * 10);
            --trytimes;
            if(trytimes < 0){
                tlog_fatal(stpdata->logger, "can't connect to server\n");
                return -1;
            }
        }
        return 0;
    }
}

int STP::fini(){
    stpdata->__log_pkg_times_info(true, REQUIRE);
    stpdata->__log_pkg_times_info(false, REQUIRE);
    if(stpdata->trans.listener){
        uint32 trytimes = 100;
        while(stpdata->state != CLOSE){
            stpdata->recv();
            usleep(1000 * 1);
            --trytimes;
            if(trytimes < 0){
                tlog_fatal(stpdata->logger, "can't fini to client\n");
                return -1;
            }
        }
    }else{
        while(stpdata->send_list.size != 0){
            stpdata->recv();
            usleep(1000 * 1);
        }
        uint32 trytimes = 100;
        stpdata->simulate(S_ACTIVE_FINI);
        while(stpdata->state != CLOSE){
            stpdata->recv();
            usleep(1000 * 10);
            --trytimes;
            if(trytimes < 0){
                tlog_fatal(stpdata->logger, "can't fini to server\n");
                return -1;
            }
        }
    }
    stpdata->__log_pkg_times_info(true, REQUIRE);
    stpdata->__log_pkg_times_info(false, REQUIRE);
    stpdata->trans.fini();
    stpdata->logger.fini();
    return 0;
}

void STP::setgamma(uint32 gamma){
    stpdata->rtt.gamma = gamma;
}
void STP::setmws(uint32 mws){
    stpdata->mws = mws;
}
void STP::setmss(uint32 mss){
    stpdata->mss = mss;
}
void STP::settimeout(uint32 timeout){
    stpdata->timeout = timeout;
}
void STP::usePLD(double pdrop, double pdelay, uint32 MaxDelay, int seed){
    stpdata->usePLD = true;
    if(pdrop >= 1.0){
        pdrop -= int(pdrop);
    }
    if(pdrop < 0.0){
        pdrop = 0.0;
    }
    if(pdelay >= 1.0){
        pdelay -= int(pdelay);
    }
    if(pdelay < 0.0){
        pdelay = 0.0;
    }
    stpdata->pld = PLD(pdrop, pdelay, MaxDelay, seed);
}

int STP::send(void* data, uint32 len){
    stpdata->timer.roll();
    int size = stpdata->send(data, len);
    return size;
}

int STP::recv(void* buf, uint32 len){
    if(stpdata->state != ESTABLISHED){
        return -1;
    }
    this->stpdata->recv();
    NODE* head = stpdata->recv_list.head;
    NODE* p = stpdata->recv_list.getNext(head);
    uint32 result = 0;
    while(p){
        TMP_DATA* data = (TMP_DATA*)stpdata->recv_list.getvalue(p);
        STP_HEADER& header = data->header;
        if(header.seq < stpdata->nowack){
            tlog_info(stpdata->logger, "recv_list_size[%d] seq[%d] len[%d] nowseq[%d]\n", stpdata->recv_list.size, header.seq, header.len, stpdata->nowack);
            uint32 leftsize = header.len - data->offset;
            uint32 readsize = leftsize < len ? leftsize:len;
            memcpy(&((char*)buf)[result], &data->buf[data->offset], readsize);
            result += readsize;
            len -= readsize;
            data->offset += readsize;
            tlog_info(stpdata->logger, "result[%d] len[%d] offset[%d]\n", result, len, data->offset);
            if(data->offset == header.len){
                stpdata->recv_list.remove(p);
                if(header.len != 0){
                    delete data->buf;
                }
                delete data;
            }else{
                break;
            }
        }else{
            break;
        }
        p = (NODE*)stpdata->recv_list.getNext(head);
    }
    if(result != 0){
        tlog_info(stpdata->logger, "*******total recv %d*******\n", result);
    }
    this->stpdata->timer.roll();
    return result;
}

void STP_ERROR_FUNC(void* ele){
    STP_DATA* stp = (STP_DATA*)ele;
    STP_HEADER* header = (STP_HEADER*)stp->recvbuf;
    tlog_fatal(stp->logger, "error! state[%s] simulate[%s]\n", __stpstate_str[stp->state], __stppkgstate_str[header->type]);
}

void CONNECTING(void* ele){
    STP_DATA* stp = (STP_DATA*)ele;
    STP_HEADER* header = (STP_HEADER*)stp->recvbuf;
    tlog_debug(stp->logger, "CONNECTING!\n");

    TMP_DATA* pkg = new TMP_DATA;
    memset(pkg, 0, sizeof(*pkg));
    stp->__wrap_header(pkg->header, true, false, false, PASSIVE_CONNECT);
    stp->send_list.push_pri(pkg, pkg->header.seq);
    stp->__send_pkg(pkg);
}
void ACCEPTING(void* ele){
    STP_DATA* stp = (STP_DATA*)ele;
    STP_HEADER* header = (STP_HEADER*)stp->recvbuf;
    tlog_debug(stp->logger, "ACCEPTING!\n");

    stp->nowack = header->seq+1;

    TMP_DATA* pkg = new TMP_DATA;
    memset(pkg, 0, sizeof(*pkg));
    stp->__wrap_header(pkg->header, true, false, true, SYN);

    stp->send_list.push_pri(pkg, pkg->header.seq);
    
    stp->__send_pkg(pkg);
}
void SYN_SENT_SUCCESS(void* ele){
    STP_DATA* stp = (STP_DATA*)ele;
    STP_HEADER* header = (STP_HEADER*)stp->recvbuf;
    tlog_debug(stp->logger, "SYN_SENT!\n");
    stp->nowseq = header->ack;
    stp->nowack = header->seq+1;

    stp->__ack_send(header->ack, header->uuid);

    stp->__send_ack(SYN);
}
void SYN_RCVD_SUCCESS(void* ele){
    STP_DATA* stp = (STP_DATA*)ele;
    STP_HEADER* header = (STP_HEADER*)stp->recvbuf;
    tlog_debug(stp->logger, "SYN_RCVD!\n");
    stp->nowseq = header->ack;
    if(stp->nowack != header->seq){
        tlog_fatal(stp->logger, "error ack and seq\n");
        stp->state = CLOSE;
    }else{
        stp->__ack_send(header->ack, header->uuid);
    }
}

//can't process pkg size > MAX_PKG_SIZE
void NORMAL_RECV(void* ele){
    STP_DATA* stp = (STP_DATA*)ele;
    STP_HEADER* header = (STP_HEADER*)stp->recvbuf;
    char* recvdata = &stp->recvbuf[sizeof(STP_HEADER)];
    tlog_debug(stp->logger, "NORMAL_RECV!\n");

    if(stp->recv_times.find(header->ack) != stp->recv_times.end()){
        ++stp->recv_times[header->ack].times;
    }else{
        stp->recv_times[header->ack] = RECORD(1, 0, header->len, 0);
    }

    stp->__ack_send(header->ack, header->uuid);
    
    stp->__log_pkg_times_info(false);
    if(header->len != 0){
        if(header->seq >= stp->nowack){
            NODE* node = stp->recv_list.find(header->seq);
            if(node == NULL){
                char* copydata = new char[header->len];
                TMP_DATA* recvpkg = new TMP_DATA;
                recvpkg->header = *header;
                recvpkg->offset = 0;
                recvpkg->buf = copydata;
                memcpy(recvpkg->buf, recvdata, header->len);
                stp->recv_list.push_pri(recvpkg, recvpkg->header.seq);
                stp->__check_recv();
            }
        }
        stp->__send_ack(NORMAL);
    }
}

void ACTIVE_FINI(void* ele){
    STP_DATA* stp = (STP_DATA*)ele;
    STP_HEADER* header = (STP_HEADER*)stp->recvbuf;
    tlog_debug(stp->logger, "ACTIVE_FINI!\n");

    TMP_DATA* pkg = new TMP_DATA;
    memset(pkg, 0, sizeof(*pkg));
    stp->__wrap_header(pkg->header, false, true, false, FINI);

    stp->send_list.push_pri(pkg, pkg->header.seq);
    stp->__send_pkg(pkg);
}
void PASSIVE_FINI(void* ele){
    STP_DATA* stp = (STP_DATA*)ele;
    STP_HEADER* header = (STP_HEADER*)stp->recvbuf;
    tlog_debug(stp->logger, "PASSIVE_FINI!\n");
    stp->__ack_send(header->ack, header->uuid);

    TMP_DATA* pkg = new TMP_DATA;
    memset(pkg, 0, sizeof(*pkg));

    stp->nowack = header->seq + 1;
    stp->__wrap_header(pkg->header, false, true, true, FINI_ACK);

    stp->send_list.push_pri(pkg, pkg->header.seq);
    stp->__send_pkg(pkg);
}
void TIME_WAIT_CLOSE(void* ele){
    STP_DATA* stp = (STP_DATA*)ele;
    STP_HEADER* header = (STP_HEADER*)stp->recvbuf;
    tlog_debug(stp->logger, "TIME_WAITING!\n");
    stp->__ack_send(header->ack, header->uuid);

    TMP_DATA* pkg = new TMP_DATA;
    memset(pkg, 0, sizeof(*pkg));

    stp->nowseq = header->ack;
    stp->nowack = header->seq + 1;

    stp->__send_ack(FINI_ACK);
    TIMER_EVENT e(2*stp->timeout, NULL, TIMER_WAIT_CLOSE);
    stp->timer.setTimer(e, stp->fini_timer);
}
void CLOSING(void* data){
    STP_DATA* stp = (STP_DATA*)data;
    STP_HEADER* header = (STP_HEADER*)stp->recvbuf;
    tlog_debug(stp->logger, "CLOSING!\n");
    stp->__ack_send(header->ack, header->uuid);
    // stp->logger.fini();
    // stp->trans.fini();
}
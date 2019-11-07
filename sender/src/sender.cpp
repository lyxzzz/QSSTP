#include <iostream>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include "stp.h"
#include "logger.h"
#include "process_bar.h"
#include <net/if.h>
using std::cin;
using std::cout;
using std::endl;
using std::ifstream;


int main(int argc, char** argv){

    // TRANS t(false, 8008, 1, "127.0.0.1");
    // LOGGER l("log/sender.log", 1);
    // char buf[64];
    // int i = 1000;
    // t.init();
    // while(true){
    //     uint32 size = 4;
    //     int* p = (int*)buf;
    //     *p = i;
    //     // cin >> *p;
    //     int q = t.send(buf, size);
    //     tlog_fatal(l, "%d:%d\n", q, *p);
    //     --i;
    //     if(*p == 0){
    //         break;
    //     }
    // }
    if(argc < 10){
        printf("need more para\n");
    }
    else{
        char* ip = argv[1];
        uint16 port = atoi(argv[2]);
        char* file_path = argv[3];
        uint32 mws = atoi(argv[4]);
        uint32 mss = atoi(argv[5]);
        uint32 timeout = atoi(argv[6]);
        double pdrop = atof(argv[7]);
        double pdelay = atof(argv[8]);
        uint32 MaxDelay = atoi(argv[9]);
        uint32 gamma = atoi(argv[10]);
        int seed = atoi(argv[11]);
        int loglevel = atoi(argv[12]);
        STP stp(false, port, loglevel, "log/sender.log", ip);
        stp.setmws(mws);
        stp.setmss(mss);
        stp.settimeout(timeout);
        stp.usePLD(pdrop, pdelay, MaxDelay, seed);
        stp.setgamma(gamma);
        const uint32 READ_FILE_SIZE = 4096;
        if(0 == stp.init()){
            char* buf = new char[READ_FILE_SIZE];
            uint32 size = READ_FILE_SIZE;

            printf("connect success\n");
            int fd = open(file_path, O_RDONLY);
            uint64 filesize = lseek(fd, 0, SEEK_END);
            lseek(fd, 0, SEEK_SET);
            printf("transport file:%s size:%d", file_path, filesize);

            BAR bar(filesize, 50);
            uint64 total_send = 0;
            uint64* int_buf = (uint64*)buf;
            *int_buf = filesize;
            uint32 buf_start = sizeof(uint64);
            while((size = read(fd, &buf[buf_start], size)) > 0){
                uint32 sendsize = 0;
                size = buf_start + size;
                while(size > 0){
                    bar.print(total_send);
                    int len = stp.send(&buf[sendsize], size);
                    if(len == 0){
                        usleep(1000 * 10);
                    }else{
                        sendsize += len;
                        size -= len;
                        total_send += len;
                        if(buf_start != 0){
                            if(len >= buf_start){
                                total_send -= buf_start;
                                buf_start = 0;
                            }else{
                                total_send = 0;
                                buf_start -= len;
                            }
                        }
                    }
                }
                size = READ_FILE_SIZE;
            }
            bar.fini();

            stp.fini();
            close(fd);
        }
    }
    return 0;
}
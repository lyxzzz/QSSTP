#include <iostream>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include "transport.h"
#include "stp.h"
#include "logger.h"
#include "process_bar.h"
using std::cin;
using std::cout;
using std::endl;
using std::ifstream;

uint64 getsize(STP& stp){
    char sizebuf[20];
    char* ptr = sizebuf;
    uint32 readsize = sizeof(uint64);
    int size;
    while((size = stp.recv(ptr, readsize)) >= 0){
        if(size > 0){
            readsize -= size;
            if(readsize == 0){
                break;
            }else{
                ptr += size;
            }
        }
    }
    uint64 filesize = *(uint64*)sizebuf;
    return filesize;
}

int main(int argc, char** argv){
    if(argc < 4){
        printf("need more para\n");
    }
    else{
        uint16 port = atoi(argv[1]);
        char* file_path = argv[2];
        int loglevel = atoi(argv[3]);
        STP stp(true, port, loglevel, "log/receiver.log");
        const uint32 WRITE_FILE_SIZE = 4096;
        if(0 == stp.init()){
            printf("accept connect success\n");
            int fd = open(file_path, O_WRONLY|O_CREAT, 0666);
            ftruncate(fd, 0);
            lseek(fd, 0, SEEK_SET);
            uint64 filesize = getsize(stp);
            printf("start to recv file, size is %d\n", filesize);

            BAR bar(filesize, 50);
            
            char* buf = new char[WRITE_FILE_SIZE];
            int32 size = WRITE_FILE_SIZE;
            uint32 total_recv = 0;
            while((size = stp.recv(buf, size)) >= 0){
                bar.print(total_recv);
                if(size > 0){
                    total_recv += size;
                    int len = write(fd, buf, size);
                    if(len != size){
                        // printf("error!");
                    }
                }else{
                    usleep(1000 * 10);
                }
                size = WRITE_FILE_SIZE;
            }
            bar.fini();
            stp.fini();
            close(fd);
        }
    }
    return 0;
}
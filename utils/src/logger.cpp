#include "logger.h"
#include "string.h"
#include "type.h"
#include "fcntl.h"
#include "stdio.h"
#include "unistd.h"
#include "stdarg.h"
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#define SET_LOG_HEADER(len, file, line, TITLE)\
        time_t nSeconds;\
        struct tm * pTM;\
        time(&nSeconds);\
        pTM = localtime(&nSeconds);\
        len1 = sprintf(__buf, "[%04d-%02d-%02d %02d:%02d:%02d][%s:%d][%s]",\
                pTM->tm_year + 1900, pTM->tm_mon + 1, pTM->tm_mday,\
                pTM->tm_hour, pTM->tm_min, pTM->tm_sec, file, line, TITLE)

static char __buf[1024];
static const char* __trans_int_level[5] = {
    "REQUIRE", "FATAL", "INFO", "DEBUG", "TRACE"
};

LOGGER::LOGGER(const char* filepath, int level){
    mkdir("log", 0666);
    this->file = open(filepath, O_WRONLY|O_APPEND|O_CREAT, 0666);

    this->level = level;
}
void LOGGER::fini(){
    close(this->file);
    for(uint8 i = 0; i < this->extra_size; ++i){
        close(this->extra_fd[i]);
    }
}

bool LOGGER::__judge(int l){
    bool r = false;
    for(uint8 i = 0; i < this->extra_size; ++i){
        if(this->extra_set[i][l]){
            r = true;
        }
    }
    return r;
}
void LOGGER::log(int level, const char* file, int line, const char* pat, ...){
    bool r = this->__judge(level);
    if(this->level >= level || r){
        int len1;
        SET_LOG_HEADER(len1, file, line, __trans_int_level[level]);

        va_list argptr;
        va_start(argptr, pat);
        int len2 = vsprintf(&__buf[len1], pat, argptr);
        va_end(argptr);

        if(this->level >= level){
            write(this->file, __buf, len1+len2);
        }
        if(r){
            for(uint8 i = 0; i < this->extra_size; ++i){
                if(this->extra_set[i][level]){
                    write(this->extra_fd[i], __buf, len1+len2);
                }
            }
        }
    }
}

void LOGGER::add_log_file(const char* file, bool level_set[]){
    this->extra_fd[this->extra_size] = open(file, O_WRONLY|O_APPEND|O_CREAT, 0666);
    for(uint8 i = 0; i < LOGGER_LEVEL_SIZE; ++i){
        this->extra_set[this->extra_size][i] = level_set[i];
    }
    ++this->extra_size;
}
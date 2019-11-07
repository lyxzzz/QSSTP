#ifndef _LOGGER_H
#define _LOGGER_H
enum LOGGER_LEVEL{
    REQUIRE,
    FATAL,
    INFO,
    DEBUG,
    TRACE
};

#define LOGGER_LEVEL_SIZE 5

#define tlog_require(LOGGER, PAT, args...)\
    LOGGER.log(REQUIRE, __FILE__, __LINE__, PAT, ##args)

#define tlog_fatal(LOGGER, PAT, args...)\
    LOGGER.log(FATAL, __FILE__, __LINE__, PAT, ##args)

#define tlog_info(LOGGER, PAT, args...)\
    LOGGER.log(INFO, __FILE__, __LINE__, PAT, ##args)

#define tlog_debug(LOGGER, PAT, args...)\
    LOGGER.log(DEBUG, __FILE__, __LINE__, PAT, ##args)

#define tlog_trace(LOGGER, PAT, args...)\
    LOGGER.log(TRACE, __FILE__, __LINE__, PAT, ##args)


struct LOGGER{
private:
    int file;
    int level;
    int extra_fd[LOGGER_LEVEL_SIZE];
    bool extra_set[LOGGER_LEVEL_SIZE][LOGGER_LEVEL_SIZE];
    int extra_size;
    bool __judge(int l);
public:
    LOGGER(const char* filepath, int level);
    void log(int level, const char* file, int line, const char* pat, ...);
    void add_log_file(const char* file, bool level_set[]);
    void fini();
};
#endif
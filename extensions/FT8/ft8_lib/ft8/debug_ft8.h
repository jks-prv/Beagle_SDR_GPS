#ifndef _DEBUG_H_INCLUDED_
#define _DEBUG_H_INCLUDED_

#define LOG_DEBUG 0
#define LOG_INFO  1
#define LOG_WARN  2
#define LOG_ERROR 3
#define LOG_FATAL 4

#ifdef LOG_LEVEL
    #ifndef LOG_PRINTF
        #include <stdio.h>
        #ifdef FT8_KIWI
            #define LOG_PRINTF(...) printf(__VA_ARGS__)
        #else
            #define LOG_PRINTF(...) fprintf(stderr, __VA_ARGS__)
        #endif
    #endif
    
    #define LOG(level, ...)     \
        if (level >= LOG_LEVEL) \
        LOG_PRINTF(__VA_ARGS__)
#else // ifdef LOG_LEVEL
    #define LOG(level, ...)
#endif

int ext_send_msg_encoded(int rx_chan, bool debug, const char *dst, const char *cmd, const char *fmt, ...);

#endif // _DEBUG_H_INCLUDED_

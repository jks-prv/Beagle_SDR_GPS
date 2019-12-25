
#include "DRM_main.h"
#include "../util/Pacer.h"

#include <sys/time.h>
#include <csignal>
#include <unistd.h>
#include <cstdio>
#include <iostream>

CPacer::CPacer(uint64_t ns)
{
    timespec now;

#if _POSIX_TIMERS>0
    int r = clock_gettime(CLOCK_REALTIME, &now);
#else
    timeval t;
    int r = gettimeofday(&t, nullptr);
    now.tv_sec = t.tv_sec;
    now.tv_nsec = 1000*t.tv_usec;
#endif

    if (r<0)
        perror("time");
    interval = ns;
    //printf("CPacer::CPacer interval=%lld\n", interval);
    timekeeper = 1000000000LL*now.tv_sec + now.tv_nsec;
    timekeeper += interval;
}

CPacer::~CPacer()
{
}

void CPacer::wait()
{
    uint64_t delay_ns = nstogo();
    
    #ifdef DRM_SHMEM_DISABLE
        //if (delay_ns > 100000ULL)       // min time to wait 100 usec
        //if (delay_ns > 10000000ULL)     // min time to wait 10 msec
        if (delay_ns > 20000000ULL)     // min time to wait 20 msec
        {
            u4_t usec = (u4_t) (delay_ns / 1000ULL);
            //printf("[%d] ", usec); fflush(stdout);
            TaskSleepUsec(usec);
        }
    #else
        if (delay_ns > 20000000ULL)     // min time to wait 20 msec
        {
            timespec delay;
            delay.tv_sec = delay_ns / 1000000000ULL;
            delay.tv_nsec = delay_ns % 1000000000ULL;
            //printf("(%ld,%ld) ", delay.tv_sec, delay.tv_nsec/1000); fflush(stdout);
            nanosleep(&delay, nullptr);
        }
    #endif

    timekeeper += interval;
}

uint64_t CPacer::nstogo()
{
    timespec now;

#if _POSIX_TIMERS>0
    (void) clock_gettime(CLOCK_REALTIME, &now);
#else
    timeval t;
    (void) gettimeofday(&t, nullptr);
    now.tv_sec = t.tv_sec;
    now.tv_nsec = 1000*t.tv_usec;
#endif

    uint64_t now_ns = 1000000000LL*now.tv_sec + now.tv_nsec;
    if (timekeeper <= now_ns)
        return 0;
    return timekeeper - now_ns;
}

#pragma once

#include "types.h"

#include <time.h>

// not all development systems have clock_gettime()
#ifdef DEVSYS
    #ifdef __MACH__
        #define clock_gettime(clk_id, tp) \
            (tp)->tv_sec = 0; \
            (tp)->tv_nsec = 0;
        #define clock_settime(clk_id, tp) 0
        #ifndef CLOCK_MONOTONIC
            #define CLOCK_MONOTONIC 0
            #define CLOCK_REALTIME 0
        #endif
    #endif
#else
	#include <time.h>
#endif

#define CTIME_R_BUFSIZE     (25 + SPACE_FOR_NULL)
#define CTIME_R_NL          24      // offset of '\n' in ctime_r return buffer

u4_t timer_epoch_sec();
void timer_set(struct timespec *ts);
time_t timer_server_build_unix_time();
time_t timer_server_start_unix_time();
u4_t timer_sec();           // overflows 136 years after timer epoch
u4_t timer_ms();            // overflows 49.7 days after timer epoch
u64_t timer_ms64_1970();    // never overflows (effectively)
u4_t timer_us();            // overflows 1.2 hours after timer epoch
u64_t timer_us64();         // never overflows (effectively)

time_t utc_time();
void utc_hour_min_sec(int *hour, int *min DEF_NULL, int *sec DEF_NULL);

time_t local_time(bool *returning_local_time DEF_NULL);
bool /* returning_local_time */ local_hour_min_sec(int *hour, int *min DEF_NULL, int *sec DEF_NULL);

void time_hour_min_sec(time_t t, int *hour, int *min DEF_NULL, int *sec DEF_NULL);
void utc_year_month_day(int *year, int *month DEF_NULL, int *day DEF_NULL, int *dow DEF_NULL, int *doy DEF_NULL);
char *var_ctime_static(time_t *t);
char *utc_ctime_static();
void var_ctime_r(time_t *t, char *tb);
void utc_ctime_r(char *tb);
int utc_time_since_2018();

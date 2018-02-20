#pragma once

#include "types.h"

// not all development systems have clock_gettime()
#ifdef DEVSYS
	#define clock_gettime(clk_id, tp)
	#define clock_settime(clk_id, tp) 0
	#define CLOCK_MONOTONIC 0
	#define CLOCK_REALTIME 0
#else
	#include <time.h>
#endif

#define CTIME_R_BUFSIZE     (25 + SPACE_FOR_NULL)
#define CTIME_R_NL          24      // offset of '\n' in ctime_r return buffer

u4_t timer_epoch_sec();
u4_t timer_server_build_unix_time();
u4_t timer_server_start_unix_time();
u4_t timer_sec();
u4_t timer_ms();
u4_t timer_us();
u64_t timer_us64();

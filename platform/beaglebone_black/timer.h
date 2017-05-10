#pragma once

#include "types.h"

// not all development systems have clock_gettime()
//#ifdef DEVSYS
//	#define clock_gettime(clk_id, tp)
//	#define CLOCK_MONOTONIC 0
//#else
	#include <time.h>
//#endif

u4_t timer_sec();
u4_t timer_ms();
u4_t timer_us();
u64_t timer_us64();

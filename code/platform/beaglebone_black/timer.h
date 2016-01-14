#ifndef _TIMER_H_
#define _TIMER_H_

#include "types.h"

u4_t timer_sec();
u4_t timer_ms();
u4_t timer_us();
u64_t timer_us64();

unsigned Microseconds(void);
unsigned nonSim_Microseconds(void);

#endif

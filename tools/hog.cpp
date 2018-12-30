#include "../types.h"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>

// not all development systems have clock_gettime()
#ifdef DEVSYS
	#define clock_gettime(clk_id, tp)
	#define CLOCK_MONOTONIC 0
#else
	#include <time.h>
#endif

u4_t timer_ms(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	int msec = ts.tv_nsec/1000000;
    return ts.tv_sec*1000 + msec;
}

int main()
{
	u4_t start = timer_ms(), diff;
	do {
		diff = timer_ms() - start;
	} while (diff < 60000);
	
	printf("UPDATE: hog DONE");

	return 0;
}

#include "../types.h"
#include "kiwi_assert.h"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>

void _sys_panic(const char *str, const char *file, int line)
{
	char *buf;
	
	// errno might be overwritten if the malloc inside asprintf fails
	asprintf(&buf, "SYS_PANIC: \"%s\" (%s, line %d)", str, file, line);
	perror(buf);
	exit(-1);
}

#define SRATE 12000.0
#define CW_DECODER_BLOCKSIZE_DEFAULT 88
//#define N_ELEM_PARIS 46
#define N_ELEM_PARIS 50

int main()
{
    float wpm = 29.0;
    float blk_per_ms = SRATE / 1e3 / CW_DECODER_BLOCKSIZE_DEFAULT;
    float ms_per_elem = 60000.0 / (wpm * N_ELEM_PARIS);
    float dot_avg = blk_per_ms * ms_per_elem;
    float dash_avg = dot_avg * 3;
    float cwspace_avg = dot_avg * 4.2;
    float symspace_avg = dot_avg * 0.93;
    float pulse_avg = (dot_avg / 4 + dash_avg) / 2.0;

    float spdcalc = 10.0 * dot_avg + 4.0 * dash_avg + 9.0 * symspace_avg + 5.0 * cwspace_avg;
    float speed_ms_per_word = spdcalc * 1000.0 / (SRATE / CW_DECODER_BLOCKSIZE_DEFAULT);
    float speed_wpm = (0.5 + 60000.0 / speed_ms_per_word);
    printf("CW WPM %.1f PARIS %d spdcalc %.3f MSPW %.3f pulse %.1f dot %.1f dash %.1f space %.1f sym %.1f\n",
        speed_wpm, N_ELEM_PARIS, spdcalc, speed_ms_per_word, pulse_avg, dot_avg, dash_avg, cwspace_avg, symspace_avg);

    return 0;
}

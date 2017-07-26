#include "types.h"
#include "datatypes.h"
#include "printf.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <math.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>

#define WSPR_DEMO_NSAMPS 45000

TYPECPX wspr_demo_samps[WSPR_DEMO_NSAMPS] = {
	#include "wspr.wav.h"
};

void _sys_panic(const char *str, const char *file, int line)
{
	char *buf;
	
	// errno might be overwritten if the malloc inside asprintf fails
	asprintf(&buf, "SYS_PANIC: \"%s\" (%s, line %d)", str, file, line);
	perror(buf);
	exit(-1);
}

/*

This reads our fs = 375 Hz text file of WSPR demo samples and makes an fs = 12 kHz binary s16 file for use with openwebrx.
We use csdr to do the re-sampling.

This produces a file full of zeros!
cat wspr.fs375.cf7040100.iq.s16.dat | csdr convert_s16_f | csdr fir_interpolate_cc 32 0.001 > wspr.fs12k.cf7040100.iq.f.dat

This works, but there is unexpected data at the EOF we chop off.
There is still a period of silence at the EOF unless we append extra samples below to cover the pipeline gap of the interpolator.
cat wspr.fs375.cf7040100.iq.s16.dat | csdr convert_s16_f | csdr plain_interpolate_cc 32 | csdr bandpass_fir_fft_cc -0.015625 0.015625 0.002 | dd bs=8 count=1440000 > wspr.fs12k.cf7040100.iq.f.dat
*/

#define GAIN 300
#define MIN_MAX 0

int main()
{
    int i, j, fd;
    TYPECPX *iq;
    short re, im;
    
    scall("open", (fd = open("wspr.fs375.cf7040100.iq.s16.dat", O_WRONLY | O_CREAT, 0666)));
    
    TYPEREAL max = 0, min = 0;

    for (i = 0; i < WSPR_DEMO_NSAMPS; i++) {
        iq = &wspr_demo_samps[i];
        #if MIN_MAX
            if (iq->re > max) max = iq->re;
            if (iq->re < min) min = iq->re;
            if (iq->im > max) max = iq->im;
            if (iq->im < min) min = iq->im;
        #endif
        re = iq->re * GAIN;
        im = iq->im * GAIN;
        write(fd, &re, 2);
        write(fd, &im, 2);
    }

    // append some extra samples to prevent interpolator pipeline gap
    for (i = 0; i < WSPR_DEMO_NSAMPS/10; i++) {
        iq = &wspr_demo_samps[i];
        re = iq->re * GAIN;
        im = iq->im * GAIN;
        write(fd, &re, 2);
        write(fd, &im, 2);
    }

    if (MIN_MAX) printf("max %.3f min %.3f\n", max, min);
    close(fd);
    
	return 0;
}

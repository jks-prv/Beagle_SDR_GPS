#include "types.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>

#include "wspr/file.c"

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
There is still a period of silence at the EOF.
cat wspr.fs375.cf7040100.iq.s16.dat | csdr convert_s16_f | csdr plain_interpolate_cc 32 | csdr bandpass_fir_fft_cc -0.015625 0.015625 0.001 | dd bs=8 count=1440000 > wspr.fs12k.cf7040100.iq.f.dat
*/

int main()
{
    int i, j, fd;
    
    scall("open", (fd = open("wspr.fs375.cf7040100.iq.s16.dat", O_WRONLY | O_CREAT, 0666)));
    
    TYPEREAL max = 0, min = 0;
    for (i = 0; i < WSPR_DEMO_NSAMPS; i++) {
        TYPECPX *iq = &wspr_demo_samps[i];
        if (iq->re > max) max = iq->re;
        if (iq->re < min) min = iq->re;
        if (iq->im > max) max = iq->im;
        if (iq->im < min) min = iq->im;
        #define GAIN 300
        short re = iq->re * GAIN;
        short im = iq->im * GAIN;
        write(fd, &re, 2);
        write(fd, &im, 2);
    }
    printf("max %.3f min %.3f\n", max, min);
    close(fd);
    
	return 0;
}

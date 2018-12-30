// creates samples in .au files suitable for use with Baudline

#include "../types.h"
#include "../support/printf.h"

#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

#define NSIGNALS    2
#define FSCALE      32767
//#define SRATE       9600.0
//#define DEC_SRATE   600.0
#define SRATE       12000.0
#define DEC_SRATE   375.0
#define NSAMPS      (1024*1024)
#define I           0
#define Q           1

double fsamps_1ch[NSAMPS][NIQ];
u2_t samps_1ch[NSAMPS][NIQ];
u2_t samps_2ch[NSAMPS][2*NIQ];

u4_t hdr[8];

int wfile(const char *fn, int nchans, double srate)
{
    int fd = open(fn, O_WRONLY|O_CREAT|O_TRUNC, 0644);
    if (fd < 0) {
        perror(fn);
        exit(-1);
    }
    int srate_i = round(srate);
    hdr[0] = FLIP32(0x2e736e64);    // ".snd"
    hdr[1] = FLIP32(sizeof(hdr));   // header size
    hdr[2] = FLIP32(0xffffffff);    // unknown filesize
    hdr[3] = FLIP32(3);             // 16-bit linear PCM
    hdr[4] = FLIP32(srate_i);       // sample rate
    hdr[5] = FLIP32(nchans*NIQ);    // number of channels
    hdr[6] = FLIP32(0);
    hdr[7] = FLIP32(0);
    write(fd, hdr, sizeof(hdr));
    return fd;
}

enum Bandwidth {NARROW, MIDDLE, WIDE};

struct firfilter {
    enum Bandwidth bandwidth;
    double buffer[17];
    int current;
};

double firfilter(struct firfilter *filter, double sample)
{
// Narrow, middle and wide fir low pass filter from ACfax
     const int buffer_count = 17;
     const double lpfcoeff[3][17]={
          { -7,-18,-15, 11, 56,116,177,223,240,223,177,116, 56, 11,-15,-18, -7},
          {  0,-18,-38,-39,  0, 83,191,284,320,284,191, 83,  0,-39,-38,-18,  0},
          {  6, 20,  7,-42,-74,-12,159,353,440,353,159,-12,-74,-42,  7, 20,  6}};

     const double *c = lpfcoeff[filter->bandwidth];
     const double *c_end=lpfcoeff[filter->bandwidth] + ((sizeof *lpfcoeff) / (sizeof **lpfcoeff));
     double* const b_begin=filter->buffer;
     double* const b_end=filter->buffer + buffer_count;
     double sum=0;

     double *current = filter->buffer + filter->current;
     // replace oldest value with current
     *current=sample;

     // convolution
     while(current!=b_end)
          sum+=(*current++)*(*c++);
     current=b_begin;
     while(c!=c_end)
          sum+=(*current++)*(*c++);

     // point again to oldest value
     if(--current<b_begin)
          current=b_end-1;

     filter->current = current - filter->buffer;
     return sum;
}

int main()
{
    int s;
    double fcar1 = 100, acar1 = 0, icar1 = fcar1 / SRATE;
    double fcar2 = -150, acar2 = 0, icar2 = fcar2 / SRATE;
    double i, i1, i2, q, q1, q2;
    double maxi=0, mini=0;
    
    int fd1 = wfile("/Users/jks/new.dec1.au", 1, SRATE);
    
    for (s = 0; s < NSAMPS; s++) {
        i1 = cos(K_2PI * acar1);
        q1 = -sin(K_2PI * acar1);
        acar1 += icar1; if (acar1 > 1.0) acar1 -= 1.0;

        i2 = cos(K_2PI * acar2);
        q2 = -sin(K_2PI * acar2);
        acar2 += icar2; if (acar2 > 1.0) acar2 -= 1.0;
        
	    // recall: pwr(dB) = 20 * log10(mag[ampl])
	    // so ampl/10 = -20 dB pwr, ampl/5 = -14 dB, ampl/2 = -6 dB etc.
        #if 1
            i = (i1 + i2/5) / NSIGNALS;     // keep bounded
            q = (q1 + q2/5) / NSIGNALS;
        #endif
        #if 0
            i = (i1 + i2);
            q = (q1 + q2);
            double mag = (i*i + q*q);
            i /= mag;
            q /= mag;
        #endif
        #if 0
            i = i1;
            q = q1;
        #endif
        if (i > maxi) maxi = i;
        if (i < mini) mini = i;
        fsamps_1ch[s][I] = i;
        fsamps_1ch[s][Q] = q;

        samps_1ch[s][I] = FLIP16((u2_t) (i * FSCALE));
        samps_1ch[s][Q] = FLIP16((u2_t) (q * FSCALE));
    }
    printf("maxi=%f mini=%f\n", maxi, mini);
    
    printf("dec1: ns=%d size=%luk\n", NSAMPS, (sizeof(hdr) + sizeof(u2_t) * NIQ * NSAMPS)/1024);

    write(fd1, samps_1ch, sizeof(u2_t) * NIQ * NSAMPS);
    close(fd1);

    #define _2CHAN  2
    #define CH(ch, iq)  ((ch)*NIQ + (iq))

    int fd2 = wfile("/Users/jks/new.dec2.au", _2CHAN, DEC_SRATE);
    //int fd2 = wfile("/Users/jks/new.dec2.au", _2CHAN, SRATE);

    double decimate = SRATE / DEC_SRATE;
    //double decimate = 1.0;
    double fsp;     // fractional sample position
    int ns;
    
    struct firfilter firfilters[NIQ] = {{NARROW}, {NARROW}};

    for (fsp = 0, s = 0, ns = 0; s < NSAMPS; ns++) {
        i = fsamps_1ch[s][I];
        q = fsamps_1ch[s][Q];
        samps_2ch[ns][CH(0,I)] = FLIP16((u2_t) (i * FSCALE));
        samps_2ch[ns][CH(0,Q)] = FLIP16((u2_t) (q * FSCALE));

        i = firfilter(firfilters+I, i);
        q = firfilter(firfilters+Q, q);
        double mag = sqrt(i*i + q*q);
        samps_2ch[ns][CH(1,I)] = FLIP16((u2_t) (i/mag * FSCALE));
        samps_2ch[ns][CH(1,Q)] = FLIP16((u2_t) (q/mag * FSCALE));
        

        fsp += decimate;
        //s = trunc(fsp);
        s = round(fsp);
    }

    printf("dec2: decimate = %f ns=%d size=%luk\n", decimate, ns, (sizeof(hdr) + sizeof(u2_t) * _2CHAN*NIQ * ns)/1024);
    
    write(fd2, samps_2ch, sizeof(u2_t) * _2CHAN*NIQ * ns);
	close(fd2);
	return 0;
}

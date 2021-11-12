/*
this software is part of libcsdr, a set of simple dsp routines for
software defined radio.

copyright (c) 2014, andras retzler <randras@sdr.hu>
all rights reserved.

redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * neither the name of the copyright holder nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

this software is provided by the copyright holders and contributors "as is" and
any express or implied warranties, including, but not limited to, the implied
warranties of merchantability and fitness for a particular purpose are
disclaimed. in no event shall andras retzler be liable for any
direct, indirect, incidental, special, exemplary, or consequential damages
(including, but not limited to, procurement of substitute goods or services;
loss of use, data, or profits; or business interruption) however caused and
on any theory of liability, whether in contract, strict liability, or tort
(including negligence or otherwise) arising in any way out of the use of this
software, even if advised of the possibility of such damage.
*/

#include <math.h>           // cos, M_PI
#include <complex.h>
#include "complex2.h"
#include "libcsdr.h"        // window_t
#include "util.h"           // XCALLOC

int32_t next_pow2(int32_t x)
{
    int32_t pow2;
    //portability? (31 is the problem)
    for(int32_t i=0;i<31;i++)
    {
        if(x<(pow2=1<<i)) return pow2; //@@next_pow2
    }
    return -1;
}

int32_t firdes_filter_len(float transition_bw)
{
    int32_t result=4.0/transition_bw;
    if (result%2==0) result++; //number of symmetric FIR filter taps should be odd
    return result;
}

static float firdes_wkernel_blackman(float rate)
{
    //Explanation at Chapter 16 of dspguide.com, page 2
    //Blackman window has better stopband attentuation and passband ripple than Hamming, but it has slower rolloff.
    rate=0.5+rate/2;
    return 0.42-0.5*cos(2*M_PI*rate)+0.08*cos(4*M_PI*rate);
}

static float firdes_wkernel_hamming(float rate)
{
    //Explanation at Chapter 16 of dspguide.com, page 2
    //Hamming window has worse stopband attentuation and passband ripple than Blackman, but it has faster rolloff.
    rate=0.5+rate/2;
    return 0.54-0.46*cos(2*M_PI*rate);
}

static float firdes_wkernel_boxcar(float rate)
{   //"Dummy" window kernel, do not use; an unwindowed FIR filter may have bad frequency response
	UNUSED(rate);
    return 1.0;
}

static float (*firdes_get_window_kernel(window_t window))(float)
{
    if(window==WINDOW_HAMMING) return firdes_wkernel_hamming;
    else if(window==WINDOW_BLACKMAN) return firdes_wkernel_blackman;
    else if(window==WINDOW_BOXCAR) return firdes_wkernel_boxcar;
    else return firdes_get_window_kernel(WINDOW_DEFAULT);
}

static void normalize_fir_f(float* input, float* output, int32_t length)
{
    //Normalize filter kernel
    float sum=0;
    for(int32_t i=0;i<length;i++) //@normalize_fir_f: normalize pass 1
        sum+=input[i];
    for(int32_t i=0;i<length;i++) //@normalize_fir_f: normalize pass 2
        output[i]=input[i]/sum;
}

static void firdes_lowpass_f(float *output, int32_t length, float cutoff_rate, window_t window)
{   //Generates symmetric windowed sinc FIR filter real taps
    //  length should be odd
    //  cutoff_rate is (cutoff frequency/sampling frequency)
    //Explanation at Chapter 16 of dspguide.com
    int32_t middle=length/2;
    float (*window_function)(float)  = firdes_get_window_kernel(window);
    output[middle]=2*M_PI*cutoff_rate*window_function(0);
    for(int32_t i=1; i<=middle; i++) //@@firdes_lowpass_f: calculate taps
    {
        output[middle-i]=output[middle+i]=(sin(2*M_PI*cutoff_rate*i)/i)*window_function((float)i/middle);
        //printf("%g %d %d %d %d | %g\n",output[middle-i],i,middle,middle+i,middle-i,sin(2*M_PI*cutoff_rate*i));
    }
    normalize_fir_f(output,output,length);
}

void firdes_bandpass_c(float complex *output, int32_t length, float lowcut, float highcut, window_t window)
{
    //To generate a complex filter:
    //  1. we generate a real lowpass filter with a bandwidth of highcut-lowcut
    //  2. we shift the filter taps spectrally by multiplying with e^(j*w), so we get complex taps
    //(tnx HA5FT)
    float* realtaps = XCALLOC(length, sizeof(float));

    firdes_lowpass_f(realtaps, length, (highcut-lowcut)/2, window);
    float filter_center=(highcut+lowcut)/2;

    float phase=0, sinval, cosval;
    for(int32_t i=0; i<length; i++) //@@firdes_bandpass_c
    {
        cosval=cos(phase);
        sinval=sin(phase);
        phase+=2*M_PI*filter_center;
        while(phase>2*M_PI) phase-=2*M_PI; //@@firdes_bandpass_c
        while(phase<0) phase+=2*M_PI;
        output[i] = CMPLXF(cosval*realtaps[i], sinval*realtaps[i]);
        //output[i] := realtaps[i] * e^j*w
    }
    XFREE(realtaps);
}

float compute_filter_relative_transition_bw(int32_t sample_rate, int32_t transition_bw_Hz) {
	ASSERT(sample_rate != 0);
	return (float)transition_bw_Hz / (float)sample_rate;
}

int32_t compute_fft_decimation_rate(int32_t sample_rate, int32_t target_rate) {
	ASSERT(sample_rate > 0);
	int32_t decimation_rate_int = floorf((float)sample_rate / (float)(target_rate));
	return next_pow2(decimation_rate_int) / 2;
}


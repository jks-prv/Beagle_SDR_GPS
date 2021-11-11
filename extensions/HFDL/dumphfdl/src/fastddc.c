/*
This software is part of libcsdr, a set of simple DSP routines for
Software Defined Radio.

Copyright (c) 2014, Andras Retzler <randras@sdr.hu>
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the copyright holder nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL ANDRAS RETZLER BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

//#define FASTDDC_DEBUG

#include <stdio.h>
#include <string.h>             // memset
#include <complex.h>
#include "complex2.h"
#include "hfdl_config.h"             // FASTDDC_DEBUG
#include "fastddc.h"
#include "hfdl_fft.h"
#include "libcsdr.h"
#include "libcsdr_gpl.h"
#include "util.h"               // debug_print, XCALLOC, NEW, XFREE

//DDC implementation based on:
//http://www.3db-labs.com/01598092_MultibandFilterbank.pdf

// Kiwi: the "inline" causes is_integer to be undefined to the clang loader for some reason
#ifdef KIWI
    int32_t is_integer(float a) { return floorf(a) == a; }
#else
    inline int32_t is_integer(float a) { return floorf(a) == a; }
#endif

int32_t fastddc_init(fastddc_t* ddc, float transition_bw, int32_t decimation, float shift_rate)
{
	ddc->pre_decimation = 1; //this will be done in the frequency domain
	ddc->post_decimation = decimation; //this will be done in the time domain
	while( is_integer((float)ddc->post_decimation/2) && ddc->post_decimation/2 != 1)
	{
		ddc->post_decimation/=2;
		ddc->pre_decimation*=2;
	}
	ddc->taps_min_length = firdes_filter_len(transition_bw); //his is the minimal number of taps to achieve the given transition_bw; we are likely to have more taps than this number.
	ddc->taps_length = next_pow2(ceil(ddc->taps_min_length/(float)ddc->pre_decimation) * ddc->pre_decimation) + 1; //the number of taps must be a multiple of the decimation factor
	ddc->fft_size = next_pow2(ddc->taps_length * 4); //it is a good rule of thumb for performance (based on the article), but we should do benchmarks
	while (ddc->fft_size<ddc->pre_decimation) ddc->fft_size*=2; //fft_size should be a multiple of pre_decimation.
	ddc->overlap_length = ddc->taps_length - 1;
	ddc->input_size = ddc->fft_size - ddc->overlap_length;
	ddc->fft_inv_size = ddc->fft_size / ddc->pre_decimation;

	//Shift operation in the frequency domain: we can shift by a multiple of v.
	ddc->v = ddc->fft_size/ddc->overlap_length; //overlap factor | +-1 ? (or maybe ceil() this?)
	int32_t middlebin=ddc->fft_size / 2;
	ddc->startbin = middlebin + middlebin * (-shift_rate) * 2;
	//fprintf(stderr, "ddc->startbin=%g\n",(float)ddc->startbin);
	ddc->startbin = ddc->v * round( ddc->startbin / (float)ddc->v );
	//fprintf(stderr, "ddc->startbin=%g\n",(float)ddc->startbin);
	ddc->offsetbin = ddc->startbin - middlebin;
	ddc->post_shift = (ddc->pre_decimation)*(shift_rate+((float)ddc->offsetbin/ddc->fft_size));
	ddc->pre_shift = ddc->offsetbin/(float)ddc->fft_size;
	ddc->dsadata = decimating_shift_addition_init(ddc->post_shift, ddc->post_decimation);

	//Overlap is scrapped, not added
	ddc->scrap=ddc->overlap_length/ddc->pre_decimation; //TODO this is problematic sometimes! overlap_length = 401 :: scrap = 200
	ddc->post_input_size=ddc->fft_inv_size-ddc->scrap;

	return ddc->fft_size<=2; //returns true on error
}

void fastddc_print(fastddc_t* ddc, char* source)
{
#ifndef DEBUG
	UNUSED(ddc);
	UNUSED(source);
#endif
	debug_print(D_DSP,
		"%s: (fft_size = %d) = (taps_length = %d) + (input_size = %d) - 1\n"
		"  overlap     ::  (overlap_length = %d) = taps_length - 1, taps_min_length = %d\n"
		"  decimation  ::  decimation = (pre_decimation = %d) * (post_decimation = %d), fft_inv_size = %d\n"
		"  shift       ::  startbin = %d, offsetbin = %d, v = %d, pre_shift = %g, post_shift = %g\n"
		"  o&s         ::  post_input_size = %d, scrap = %d\n"
		,
		source, ddc->fft_size, ddc->taps_length, ddc->input_size,
		ddc->overlap_length, ddc->taps_min_length,
		ddc->pre_decimation, ddc->post_decimation, ddc->fft_inv_size,
		ddc->startbin, ddc->offsetbin, ddc->v, ddc->pre_shift, ddc->post_shift,
		ddc->post_input_size, ddc->scrap);
}

void fft_swap_sides(float complex *io, int32_t fft_size)
{
	int32_t middle = fft_size / 2;
	float complex temp;
	for(int32_t i = 0; i < middle; i++)
	{
		temp = io[i];
		io[i] = io[i+middle];
		io[i+middle] = temp;
	}
}

decimating_shift_addition_status_t fastddc_inv_cc(float complex *input, float complex *output, fastddc_t* ddc, FFT_PLAN_T *plan_inverse, float complex *taps_fft, decimating_shift_addition_status_t shift_stat)
{
	//implements DDC by using the overlap & scrap method
	//TODO: +/-1s on overlap_size et al
	//input shoud have ddc->fft_size number of elements

	float complex *inv_input = plan_inverse->input;
	float complex *inv_output = plan_inverse->output;

	memset(inv_input, 0, plan_inverse->size * sizeof(float complex));

	//Alias & shift & filter at once

	//fprintf(stderr, " === fastddc_inv_cc() ===\n");
	//The problem is, we have to say that the output_index should be the _center_ of the spectrum when i is at startbin! (startbin is at the _center_ of the input to downconvert, not at its first bin!)
#ifdef FASTDDC_DEBUG
	static int32_t first = 1;
#ifdef FASTDDC_DEBUG
	if(first) {
		fprintf(stderr, "taps = [];\n");
		for(int32_t i = 0 ; i < ddc->fft_size; i++) {
			fprintf(stderr, "taps(%d)=%f+%f*i;\n", i+1, crealf(taps_fft[i]), cimagf(taps_fft[i]));
		}
	}
#endif
#endif
	for(int32_t i=0;i<ddc->fft_size;i++)
	{
		int32_t output_index = (ddc->fft_size+i-ddc->offsetbin+(ddc->fft_inv_size/2))%plan_inverse->size;
		//fprintf(stderr, "output_index = %d , tap_index = %d, input index = %d\n", output_index, tap_index, i);
		//cmultadd(inv_input+output_index, input+i, taps_fft+tap_index); //cmultadd(output, input1, input2):   complex output += complex input1 * complex input 2
		// (a+b*i)*(c+d*i) = (ac-bd)+(ad+bc)*i
		// a = iof(input,i)
		// b = qof(input,i)
		// c = iof(taps_fft,i)
		// d = qof(taps_fft,i)
		inv_input[output_index] += input[i] * taps_fft[i];
#ifdef FASTDDC_DEBUG
		if(first) {
			debug_print(D_DSP, "%d -> %d: %f, %f\n", i, output_index, crealf(inv_input[output_index]), cimagf(inv_input[output_index]));
		}
#endif
		//iof(inv_input,output_index) += iof(input,i); //no filter
		//qof(inv_input,output_index) += qof(input,i);
	}
#ifdef FASTDDC_DEBUG
	static int32_t second = 2;
	float in_scale = 100.0f;
	float out_scale = 1e3f;
	if(second == 1) {
		fprintf(stderr, "ddc_input = [];\n");
		for(int32_t i=0;i<ddc->fft_size;i++) {
			//fprintf(stderr, "ddc_input(%4d) %9.6f %9.6f*i\n", i+1, crealf(input[i]), cimagf(input[i]));
			fprintf(stderr, "ddc_input(%4d) %6.0f %6.0f*i\n", i+1,
			    fabsf(crealf(input[i]))*in_scale, fabsf(cimagf(input[i]))*in_scale);
		}
		fprintf(stderr, "fft_input = [];\n");
		for(int32_t i = 0; i < plan_inverse->size; i++) {
			//fprintf(stderr, "fft_input(%4d) %9.6f %9.6f*i\n", i+1, crealf(inv_input[i]), cimagf(inv_input[i]));
			fprintf(stderr, "fft_input(%4d) %6.0f %6.0f*i\n", i+1,
			    fabsf(crealf(inv_input[i]))*in_scale, fabsf(cimagf(inv_input[i]))*in_scale);
		}
	}
#endif

	//Normalize inv fft bins (now our output level is not higher than the input... but we may optimize this into the later loop when we normalize by size)
	for(int32_t i=0;i<plan_inverse->size;i++)
	{
		inv_input[i] /= ddc->pre_decimation;
	}

	fft_swap_sides(inv_input,plan_inverse->size);
	csdr_fft_execute(plan_inverse);

	//Normalize data
	for(int32_t i=0;i<plan_inverse->size;i++) //@fastddc_inv_cc: normalize by size
	{
		inv_output[i] /= plan_inverse->size;
	}
#ifdef FASTDDC_DEBUG
	if(second==1) {
		fprintf(stderr, "fft_output = [];\n");
		for(int32_t i=0;i<plan_inverse->size;i++) {
			//fprintf(stderr, "fft_output(%4d) %9.6f %9.6f*i\n", i+1, crealf(inv_output[i]), cimagf(inv_output[i]));
			fprintf(stderr, "fft_output(%4d) %6.0f %6.0f*i\n", i+1,
			    fabsf(crealf(inv_output[i]))*out_scale, fabsf(cimagf(inv_output[i]))*out_scale);
		}
	}
	first = 0;
	second--;
#endif

	//Overlap is scrapped, not added
	//Shift correction
	shift_stat = decimating_shift_addition_cc(inv_output+ddc->scrap, output, ddc->post_input_size, ddc->dsadata, ddc->post_decimation, shift_stat);
	//shift_stat.output_size = ddc->post_input_size; //bypass shift correction
	//memcpy(output, inv_output+ddc->scrap, sizeof(float complex)*ddc->post_input_size);
	return shift_stat;
}

fft_channelizer fft_channelizer_create(int32_t decimation, float transition_bw, float freq_shift) {
	window_t window = WINDOW_HAMMING;

	NEW(fft_channelizer_s, c);
	NEW(fastddc_t, ddc);
	c->ddc = ddc;
	if(fastddc_init(c->ddc, transition_bw, decimation, freq_shift)) {
		goto fail;
	}
	fastddc_print(c->ddc,"fastddc_inv_cc");

	//prepare making the filter and doing FFT on it
	float complex *taps = XCALLOC(c->ddc->fft_size, sizeof(float complex));
	c->filtertaps_fft = XCALLOC(c->ddc->fft_size, sizeof(float complex));
	FFT_PLAN_T *filter_taps_plan = csdr_make_fft_c2c(c->ddc->fft_size, taps, c->filtertaps_fft, 1, 0);

	//make the filter
	float filter_half_bw = 0.5f / decimation;
	debug_print(D_DSP, "preparing a bandpass filter of [%g, %g] cutoff rates. Real transition bandwidth is: %g\n",
			(-freq_shift) - filter_half_bw, (-freq_shift) + filter_half_bw, 4.0 / c->ddc->taps_length);
	firdes_bandpass_c(taps, c->ddc->taps_length, (-freq_shift) - filter_half_bw, (-freq_shift) + filter_half_bw, window);
	csdr_fft_execute(filter_taps_plan);
	fft_swap_sides(c->filtertaps_fft, c->ddc->fft_size);
	csdr_destroy_fft_c2c(filter_taps_plan);
	XFREE(taps);

	//make FFT plan
	c->inv_input = XCALLOC(c->ddc->fft_size, sizeof(float complex));
	c->inv_output = XCALLOC(c->ddc->fft_size, sizeof(float complex));
	c->inv_plan = csdr_make_fft_c2c(c->ddc->fft_inv_size, c->inv_input, c->inv_output, 0, 0);

	return c;
fail:
	XFREE(c);
	return NULL;
}

void fft_channelizer_destroy(fft_channelizer c) {
	if(c == NULL) {
		return;
	}
	csdr_destroy_fft_c2c(c->inv_plan);
	XFREE(c->inv_output);
	XFREE(c->inv_input);
	XFREE(c->filtertaps_fft);
	XFREE(c->ddc);
	XFREE(c);
}

/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include <math.h>
#include <complex.h>
#include "hfdl_fft.h"                // FFT_PLAN_T
#include "libcsdr_gpl.h"        // shift_addition_data_t, decimating_shift_addition_status_t

typedef struct fastddc_s
{
	int32_t pre_decimation;
	int32_t post_decimation;
	int32_t taps_length;
	int32_t taps_min_length;
	int32_t overlap_length; //it is taps_length - 1
	int32_t fft_size;
	int32_t fft_inv_size;
	int32_t input_size;
	int32_t post_input_size;
	float pre_shift;
	int32_t startbin; //for pre_shift
	int32_t v; //step for pre_shift
	int32_t offsetbin;
	float post_shift;
	int32_t output_scrape;
	int32_t scrap;
	shift_addition_data_t dsadata;
} fastddc_t;

typedef struct {
	fastddc_t *ddc;
	FFT_PLAN_T *inv_plan;
	float complex *inv_input, *inv_output;
	float complex *filtertaps_fft;
	decimating_shift_addition_status_t shift_status;
} fft_channelizer_s;
typedef fft_channelizer_s *fft_channelizer;

int32_t fastddc_init(fastddc_t *ddc, float transition_bw, int32_t decimation, float shift_rate);
decimating_shift_addition_status_t fastddc_inv_cc(float complex *input, float complex *output, fastddc_t *ddc, FFT_PLAN_T *plan_inverse, float complex *taps_fft, decimating_shift_addition_status_t shift_stat);
void fastddc_print(fastddc_t *ddc, char *source);
void fft_swap_sides(float complex *io, int32_t fft_size);
fft_channelizer fft_channelizer_create(int32_t decimation, float transition_bw, float freq_shift);
void fft_channelizer_destroy(fft_channelizer c);

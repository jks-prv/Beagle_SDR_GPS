/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include <stdint.h>
#include <complex.h>

// FIXME: this should be hidden.
// Need to provide getter function for input pointer (fastddc needs this)
struct fft_plan_s {
	int32_t size;
	void *input;
	void *output;
	void *plan;
};

// FIXME: typedef
#define FFT_PLAN_T struct fft_plan_s

typedef struct fft_thread_ctx_s *fft_thread_ctx_t;

// hfdl_fft_fftw.c
void csdr_fft_init();
void csdr_fft_destroy();
FFT_PLAN_T* csdr_make_fft_c2c(int32_t size, float complex *input,
		float complex *output, int32_t forward, int32_t benchmark);
void csdr_destroy_fft_c2c(FFT_PLAN_T *plan);
void csdr_fft_execute(FFT_PLAN_T* plan);

// hfdl_fft.c
struct block *fft_create(int32_t decimation, float transition_bw);
void fft_destroy(struct block *fft_block);

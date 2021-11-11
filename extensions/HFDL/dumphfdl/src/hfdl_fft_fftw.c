/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <complex.h>
#include <fftw3.h>
#include "hfdl_fft.h"
#include "util.h"           // NEW
#include "hfdl_config.h"         // WITH_FFTW3F_THREADS

#define FFT_THREAD_CNT 4

void csdr_fft_init() {
#ifdef WITH_FFTW3F_THREADS
	fftwf_init_threads();
	fftwf_plan_with_nthreads(FFT_THREAD_CNT);
#endif
}

void csdr_fft_destroy() {
#ifdef WITH_FFTW3F_THREADS
	fftwf_cleanup_threads();
#endif
}

FFT_PLAN_T* csdr_make_fft_c2c(int32_t size, float complex* input, float complex* output, int32_t forward, int32_t benchmark) {
	NEW(FFT_PLAN_T, plan);
	// fftwf_complex is binary compatible with float complex
	plan->plan = fftwf_plan_dft_1d(size, (fftwf_complex *)input, (fftwf_complex *)output, forward ? FFTW_FORWARD : FFTW_BACKWARD, benchmark ? FFTW_MEASURE : FFTW_ESTIMATE);
	plan->size = size;
	plan->input = input;
	plan->output = output;
	return plan;
}

void csdr_destroy_fft_c2c(FFT_PLAN_T *plan) {
	if(plan) {
		fftwf_destroy_plan(plan->plan);
		XFREE(plan);
	}
}

void csdr_fft_execute(FFT_PLAN_T* plan) {
	fftwf_execute(plan->plan);
}

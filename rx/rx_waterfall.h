/*
--------------------------------------------------------------------------------
This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Library General Public
License as published by the Free Software Foundation; either
version 2 of the License, or (at your option) any later version.
This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Library General Public License for more details.
You should have received a copy of the GNU Library General Public
License along with this library; if not, write to the
Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
Boston, MA  02110-1301, USA.
--------------------------------------------------------------------------------
*/

// Copyright (c) 2019 John Seamons, ZL4VO/KF6VO

#pragma once

#include "types.h"
#include "config.h"
#include "kiwi.h"
#include "clk.h"
#include "misc.h"
#include "nbuf.h"
#include "web.h"
#include "spi.h"
#include "gps.h"
#include "coroutines.h"
#include "debug.h"
#include "data_pump.h"
#include "cfg.h"
#include "datatypes.h"
#include "ext_int.h"
#include "rx.h"
#include "rx_sound.h"
#include "dx.h"
#include "non_block.h"

#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <sched.h>
#include <math.h>
#include <fftw3.h>

#ifdef USE_WF_NEW
#define	WF_USING_HALF_FFT	1	// the result is contained in the first half of the complex FFT
#define	WF_USING_HALF_CIC	1	// only use half of the remaining FFT after a CIC
#define	WF_BETTER_LOOKING	1	// increase in FFT size for better looking display
#else
#define	WF_USING_HALF_FFT	2	// the result is contained in the first half of the complex FFT
#define	WF_USING_HALF_CIC	2	// only use half of the remaining FFT after a CIC
#define	WF_BETTER_LOOKING	2	// increase in FFT size for better looking display
#endif

#define WF_OUTPUT	1024	// conceptually same as WF_WIDTH although not required
#define WF_C_NFFT	(WF_OUTPUT * WF_USING_HALF_FFT * WF_USING_HALF_CIC * WF_BETTER_LOOKING)	// worst case FFT size needed
#define WF_C_NSAMPS	WF_C_NFFT

#define	WF_WIDTH		1024	// width of waterfall display

struct fft_t {
	fftwf_plan hw_dft_plan;
	fftwf_complex hw_c_samps[sizeof(fftwf_complex) * (WF_C_NSAMPS)];
	fftwf_complex hw_fft[sizeof(fftwf_complex) * (WF_C_NFFT)];
};

struct wf_pkt_t {
	char id4[4];
	u4_t x_bin_server;
	#define WF_FLAGS                0xffff0000
	#define WF_FLAGS_COMPRESSION    0x00010000
	#define WF_FLAGS_NO_SYNC        0x00020000
	u4_t flags_x_zoom_server;
	u4_t seq;
	union {
		u1_t buf[WF_WIDTH];
		struct {
			#define ADPCM_PAD 10
			u1_t adpcm_pad[ADPCM_PAD];
			u1_t buf2[WF_WIDTH];
		};
	} un;
} __attribute__((packed));


// Use odd values so periodic signals like radars running at even-Hz rates don't
// beat against update rate and produce artifacts or blanking.

#define	WF_SPEED_MAX		23
#define	WEB_SERVER_POLL_US	(1000000 / WF_SPEED_MAX / 2)

#define WF_SPEED_OFF        0
#define	WF_SPEED_1FPS		1
#define	WF_SPEED_SLOW		5
#define	WF_SPEED_MED		13
#define	WF_SPEED_FAST		WF_SPEED_MAX

enum { WF_SELECT_OFF = 0, WF_SELECT_1FPS = 1, WF_SELECT_SLOW = 2, WF_SELECT_MED = 3, WF_SELECT_FAST = 4 };

#define WF_ZOOM_MIN     0
#define WF_ZOOM_MAX     15

#define WF_COMP_OFF     0
#define WF_COMP_ON      1

enum aper_t { MAN=0, AUTO };
enum aper_algo_t { IIR=0, MMA, EMA, OFF };

#define WF_CIC_COMP 10
enum wf_interp_t { WF_MAX=0, WF_MIN, WF_LAST, WF_DROP, WF_CMA };

struct wf_inst_t {
	conn_t *conn;
	int rx_chan;
	int fft_used, plot_width, plot_width_clamped;
	int maxdb, mindb, send_dB;
	float fft_scale[WF_WIDTH], fft_scale_div2[WF_WIDTH], fft_offset;
	u2_t fft2wf_map[WF_C_NFFT / WF_USING_HALF_FFT];		// map is 1:1 with fft
	u2_t wf2fft_map[WF_WIDTH];							// map is 1:1 with plot
	u2_t drop_sample[WF_WIDTH];
	int start, prev_start, zoom, prev_zoom;
	u4_t mark;
	int speed, fft_used_limit;
	bool new_map, new_map2, compression, no_sync, isWF, isFFT;
	int flush_wf_pipe;
	bool cic_comp;
	wf_interp_t interp;
	int window_func;
	bool trigger;

	// NB: matches rx_noise.h which is not included here to prevent re-compile cascade
    #define NOISE_TYPES 4
    #define NOISE_PARAMS 8
	int nb_enable[NOISE_TYPES];
	float nb_param[NOISE_TYPES][NOISE_PARAMS];
	bool nb_param_change[NOISE_TYPES], nb_setup;
	u4_t last_noise_pulse;

	snd_t *snd;
	u4_t snd_seq;
	wf_pkt_t out;
	int out_bytes;
	bool check_overlapped_sampling, overlapped_sampling;
	int samp_wait_ms, chunk_wait_us;
	
	u4_t last_frames_ms, waterfall_frames;
	
	int aper, aper_algo;
	float aper_param;
	int need_autoscale, done_autoscale, sent_autoscale, avg_clear, signal, noise;
    #define APER_PWR_LEN WF_OUTPUT
    float avg_pwr[APER_PWR_LEN];
    u4_t report_sec;
    int last_noise, last_signal;
};

#define WINF_WF_HANNING          0
#define WINF_WF_HAMMING          1
#define WINF_WF_BLACKMAN_HARRIS  2
#define WINF_WF_NONE             3
#define N_WF_WINF               4

struct wf_shmem_t {
    wf_inst_t wf_inst[MAX_RX_CHANS];        // NB: MAX_RX_CHANS even though there may be fewer MAX_WF_CHANS
    fft_t fft_inst[MAX_WF_CHANS];           // NB: MAX_WF_CHANS not MAX_RX_CHANS
    float window_function[N_WF_WINF][WF_C_NSAMPS];
    float CIC_comp[WF_C_NSAMPS];
    int n_chunks;
};     

#include "shmem_config.h"

#ifdef MULTI_CORE
    //#define WF_SHMEM_DISABLE_TEST
    #ifdef WF_SHMEM_DISABLE_TEST
        #warning dont forget to remove WF_SHMEM_DISABLE_TEST
        #define WF_SHMEM_DISABLE
    #else
        // shared memory enabled
    #endif
#else
    #define WF_SHMEM_DISABLE
#endif

#include "shmem.h"

#ifdef WF_SHMEM_DISABLE
    extern wf_shmem_t *wf_shmem_p;
    #define WF_SHMEM wf_shmem_p
#else
    #define WF_SHMEM (&shmem->wf_shmem)
#endif


enum wf_cmd_key_e {
    CMD_SET_ZOOM=1, CMD_SET_MAX_MIN_DB, CMD_SET_CMAP, CMD_SET_APER, CMD_SET_BAND,
    CMD_SET_SCALE, CMD_SET_WF_SPEED, CMD_SEND_DB, CMD_EXT_BLUR, CMD_INTERPOLATE, CMD_WF_WINDOW_FUNC
};

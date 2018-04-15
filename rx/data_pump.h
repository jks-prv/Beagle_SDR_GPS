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

// Copyright (c) 2015 John Seamons, ZL/KF6VO

#ifndef _DATA_PUMP_H_
#define _DATA_PUMP_H_

#include "types.h"
#include "spi.h"
#include "cuteSDR.h"
#include "ima_adpcm.h"

#include <fftw3.h>

struct rx_iq_t {
	u2_t i, q;
	u1_t q3, i3;	// NB: endian swap
} __attribute__((packed));
			
struct wf_iq_t {
	u2_t i, q;
} __attribute__((packed));

#define N_DPBUF	16

struct rx_dpump_t {
	struct {
		u4_t wr_pos, rd_pos;
		// array size really NRX_SAMPS but made pow2 FASTFIR_OUTBUF_SIZE for indexing efficiency
		TYPECPX in_samps[N_DPBUF][FASTFIR_OUTBUF_SIZE];
		u64_t ticks[N_DPBUF];
		#ifdef SND_SEQ_CHECK
		    u4_t in_seq[N_DPBUF];
		#endif
		
		u4_t iq_wr_pos, iq_rd_pos;
		u4_t iq_seq, iq_seqnum[N_DPBUF];
		TYPECPX iq_samples[N_DPBUF][FASTFIR_OUTBUF_SIZE];
		
		TYPECPX agc_samples[FASTFIR_OUTBUF_SIZE];

		TYPEREAL demod_samples[FASTFIR_OUTBUF_SIZE];

		u4_t real_wr_pos, real_rd_pos;
		u4_t real_seq, real_seqnum[N_DPBUF];
		TYPEMONO16 real_samples[N_DPBUF][FASTFIR_OUTBUF_SIZE];
	};
	
	struct {
	    int rx_chan;
		u64_t gen, proc;
		fftwf_complex *wf_c_samps;
		u4_t desired;
		float chunk_wait_us;
		int zoom, samp_wait_ms;
		bool overlapped_sampling;
		ima_adpcm_state_t adpcm_snd;
	};
};

extern rx_dpump_t rx_dpump[RX_CHANS];
extern u4_t dpump_resets, dpump_hist[NRX_BUFS];

extern int rx_adc_ovfl;

void data_pump_start_stop();
void data_pump_init();

#endif

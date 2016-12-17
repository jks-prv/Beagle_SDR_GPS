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

#define	SBUF_FIR	0
#define	SBUF_AGC	1
#define	SBUF_N		2
#define SBUF_SLOP	512

struct rx_dpump_t {
	struct {
		u4_t wr_pos, rd_pos;
		TYPECPX in_samps[N_DPBUF][FASTFIR_OUTBUF_SIZE + SBUF_SLOP];
		TYPECPX cpx_samples[SBUF_N][FASTFIR_OUTBUF_SIZE + SBUF_SLOP];
		TYPEREAL real_samples[FASTFIR_OUTBUF_SIZE + SBUF_SLOP];
		TYPEMONO16 mono16_samples[FASTFIR_OUTBUF_SIZE + SBUF_SLOP];
	};
	struct {
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

extern int rx_adc_ovfl;

enum rx_chan_action_e {RX_CHAN_ENABLE, RX_CHAN_DISABLE, RX_CHAN_FREE };
	
void data_pump_init();
void rx_enable(int chan, rx_chan_action_e action);
int rx_chan_free(int *idx);

#endif

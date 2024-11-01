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

// Copyright (c) 2019-2023 John Seamons, ZL4VO/KF6VO

#pragma once

#include "types.h"
#include "kiwi.h"
#include "options.h"
#include "cuteSDR.h"
#include "ima_adpcm.h"
#include "rx_noise.h"
#include "ext.h"


#define TR_SND_CMDS     0
#define SM_SND_DEBUG	false

#define SND_INSTANCE_FFT_PASSBAND   0
#define SND_INSTANCE_FFT_CHAN_NULL  1

#define CLIPPER_NBFM_VAL 8192

#define MODE_FLAGS_SAM  0x0000000f
#define MODE_FLAGS_SCAN 0x00000010


typedef struct {
	struct {
		char id[3];
		u1_t flags;
		u1_t seq[4];            // waterfall syncs to this sequence number on the client-side
		u1_t smeter[2];
	} __attribute__((packed)) h;
	union {
	    u1_t u1[FASTFIR_OUTBUF_SIZE * sizeof(s2_t)];
	    s2_t s2[FASTFIR_OUTBUF_SIZE];
	};
} __attribute__((packed)) snd_pkt_real_t;

typedef struct {
	struct {
		char id[3];
		u1_t flags;
		u1_t seq[4];            // waterfall syncs to this sequence number on the client-side
		u1_t smeter[2];
		u1_t last_gps_solution; // time difference to last gps solution in seconds
		u1_t dummy;
		u4_t gpssec;            // GPS time stamp (GPS seconds)
		u4_t gpsnsec;           // GPS time stamp (fractional seconds in units of ns)
	} __attribute__((packed)) h;
	union {
	    u1_t u1[FASTFIR_OUTBUF_SIZE * NIQ * sizeof(s2_t)];
	    s2_t s2[FASTFIR_OUTBUF_SIZE * NIQ];
	};
} __attribute__((packed)) snd_pkt_iq_t;

typedef struct {
	struct {
		char id[3];
		u1_t flags;
		u1_t seq[4];            // waterfall syncs to this sequence number on the client-side
		u1_t smeter[2];
		u1_t last_gps_solution; // time difference to last gps solution in seconds
		u1_t dummy;
		u4_t gpssec;            // GPS time stamp (GPS seconds)
		u4_t gpsnsec;           // GPS time stamp (fractional seconds in units of ns)
	} __attribute__((packed)) h;
	union {
	    u1_t u1[MAX_WB_SAMPS * NIQ * sizeof(s2_t)];
	    s2_t s2[MAX_WB_SAMPS * NIQ];
	};
} __attribute__((packed)) snd_pkt_wb_t;

extern snd_pkt_wb_t out_pkt_wb;

#define WINF_SND_BLACKMAN_NUTTALL   0
#define WINF_SND_BLACKMAN_HARRIS    1
#define WINF_SND_NUTTALL            2
#define WINF_SND_HANNING            3
#define WINF_SND_HAMMING            4
#define N_SND_WINF                  5

#define SPEC_SND_NONE   0
#define SPEC_SND_RF     1
#define SPEC_SND_AF     2
#define N_SND_SPEC      3

typedef struct {
    snd_pkt_real_t out_pkt_real;
    snd_pkt_iq_t   out_pkt_iq;

    u4_t firewall[32];
	u4_t seq;
	bool isSAM;
	float norm_locut, norm_hicut, norm_pbc;
    int window_func;
	ima_adpcm_state_t adpcm_snd;

	ext_receive_FFT_samps_t specAF_FFT;
	int specAF_instance;
	u4_t specAF_last_ms;
    bool isChanNull;
	
    #ifdef SND_SEQ_CHECK
        bool snd_seq_ck_init;
	    u4_t snd_seq_ck;
    #endif

	ext_receive_FFT_samps_t rsid_FFT;

	double freq, gen, locut, hicut;
	int mode, genattn, mute, test, deemp, deemp_nfm;
	bool gen_enable;
	u4_t mparam, SAM_mparam;
	bool spectral_inversion;
    u4_t cmd_recv;
	int tr_cmds;
	bool change_LPF, change_freq_mode, restart;
	bool check_masked;
	int compression;
	bool little_endian;
	int mute_overload;      // activate the muting when overloaded
	float rf_attn_dB;

    bool   gps_init;
	double gpssec;       // current gps timestamp
	double last_gpssec;  // last gps timestamp

	int agc, _agc, hang, _hang;
	int thresh, _thresh, manGain, _manGain, slope, _slope, decay, _decay;

	int squelch, squelch_on_seq, tail_delay;
	bool sq_changed, squelched;

	int nb_algo, nr_algo;
    int nb_enable[NOISE_TYPES], nr_enable[NOISE_TYPES];
	float nb_param[NOISE_TYPES][NOISE_PARAMS], nr_param[NOISE_TYPES][NOISE_PARAMS];
} snd_t;

extern snd_t snd_inst[MAX_RX_CHANS];

enum snd_cmd_key_e {
    CMD_AUDIO_START=1, CMD_TUNE, CMD_COMPRESSION, CMD_REINIT, CMD_LITTLE_ENDIAN,
    CMD_GEN_FREQ, CMD_GEN_ATTN, CMD_SET_AGC, CMD_SQUELCH, CMD_NB_ALGO, CMD_NR_ALGO, CMD_NB_TYPE,
    CMD_NR_TYPE, CMD_MUTE, CMD_OVLD_MUTE, CMD_DE_EMP, CMD_TEST, CMD_UAR, CMD_AR_OKAY, CMD_UNDERRUN,
    CMD_SEQ, CMD_LMS_AUTONOTCH, CMD_SAM_PLL, CMD_SND_WINDOW_FUNC, CMD_SPEC, CMD_RF_ATTN
};

bool specAF_FFT(int rx_chan, int instance, int flags, int ratio, int ns_out, TYPECPX *samps);

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
#include "agc.h"
#include "fir.h"
#include "iir.h"
#include "squelch.h"
#include "fastfir.h"
#include "noiseproc.h"
#include "ima_adpcm.h"
#include "ext.h"

#define CMD_FREQ		0x01
#define CMD_MODE		0x02
#define CMD_PASSBAND	0x04
#define CMD_AGC			0x08
#define	CMD_AR_OK		0x10
#define	CMD_ALL			(CMD_FREQ | CMD_MODE | CMD_PASSBAND | CMD_AGC | CMD_AR_OK)

#define LOOP_BC 1024

extern str_hash_t snd_cmd_hash;
extern CAgc m_Agc[MAX_RX_CHANS];
extern CNoiseProc m_NoiseProc_snd[MAX_RX_CHANS];
extern CSquelch m_Squelch[MAX_RX_CHANS];

extern CFastFIR m_PassbandFIR[MAX_RX_CHANS];
extern CFastFIR m_chan_null_FIR[MAX_RX_CHANS];

extern CFir m_AM_FIR[MAX_RX_CHANS];

extern CFir m_nfm_deemp_FIR[MAX_RX_CHANS];     // see: tools/FIR.m
extern CFir m_am_ssb_deemp_FIR[MAX_RX_CHANS];

extern CIir m_deemp_Biquad[MAX_RX_CHANS];      // see: tools/biquad.MZT.m

void rx_sound_set_freq(conn_t *conn, double freq, bool spectral_inversion);
void rx_gen_set_freq(conn_t *conn, snd_t *s);
void rx_sound_cmd(conn_t *conn, double frate, int n, char *cmd);

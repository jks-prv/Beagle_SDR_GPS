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

// Copyright (c) 2019 John Seamons, ZL/KF6VO

#pragma once

#include "types.h"
#include "kiwi.h"
#include "cuteSDR.h"

typedef struct {
	struct {
		char id[3];
		u1_t flags;
		u1_t seq[4];            // waterfall syncs to this sequence number on the client-side
		char smeter[2];
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
		char smeter[2];
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
    snd_pkt_real_t out_pkt_real;
    snd_pkt_iq_t   out_pkt_iq;

    u4_t firewall[32];
	u4_t seq;
	float locut, hicut, norm_locut, norm_hicut;
	
    #ifdef SND_SEQ_CHECK
        bool snd_seq_ck_init;
	    u4_t snd_seq_ck;
    #endif
} snd_t;

extern snd_t snd_inst[MAX_RX_CHANS];

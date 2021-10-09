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

// Copyright (c) 2016 John Seamons, ZL/KF6VO

#pragma once

#include "types.h"

//#define DEVL_EiBi

// DX list

#define DX_HIDDEN_SLOT 1

typedef struct {
	float freq;				// must be first for qsort_floatcomp()
    u2_t time_begin, time_end;
	u4_t flags;
    u2_t ident_idx;
    char country[4], lang[4], target[4];

	int idx;
	const char *ident, *ident_s;
	char *notes, *notes_s;
	const char *params;
	int low_cut;
	int high_cut;
	int offset;
	int timestamp;
	int tag;
} dx_t;

typedef enum { DB_STORED = 0, DB_EiBi } dx_db_e;

typedef struct {
    dx_db_e db;
    dx_t *cur_list;
    int cur_len;    // malloc'd length is always len + DX_HIDDEN_SLOT
} dx_rx_t;

typedef struct {
	int masked_lo, masked_hi;   // Hz
} dx_mask_t;

typedef struct {
    dx_rx_t dx_rx[MAX_RX_CHANS];
	dx_t *stored_list, *eibi_list;
	int stored_len, eibi_len;
	bool eibi_init;
	bool hidden_used;
	bool json_up_to_date;
	dx_mask_t *masked_list;
	int masked_len, masked_seq;
	char *json;
} dxlist_t;

extern dxlist_t dx;

extern const int eibi_counts[];

#define	DX_MODE	    0x000f

#define	DX_TYPE	    0x01f0  // 32 types
#define DX_TYPE_SFT 4
#define DX_T2I(type) ( (((type) & DX_TYPE) - DX_FIRST) >> DX_TYPE_SFT )

#define	DX_ACTIVE   0x0000
#define	DX_WL		0x0010	// on watchlist, i.e. not actually heard yet, marked as a signal to watch for
#define	DX_SB		0x0020	// a sub-band, not a station
#define	DX_DG		0x0030	// DGPS
#define	DX_SE		0x0040	// special event
#define	DX_XX		0x0050	// interference
#define	DX_MK		0x0060	// masked

#define	DX_FIRST	0x0080
#define	DX_BCAST	0x0080
#define	DX_HFDL		0x0090
#define	DX_CW		0x00a0
#define	DX_FSK		0x00b0
#define	DX_TIME		0x00c0
#define	DX_UTIL		0x00d0
#define	DX_ALE		0x00e0
#define	DX_SPY      0x00f0
#define	DX_MODEM    0x0100
#define	DX_FAX		0x0110
#define	DX_AERO		0x0120
#define	DX_MARINE   0x0130
#define	DX_LAST 	0x0130

#define	DX_DOW      0xfe00
#define DX_SUN      0x8000
#define DX_MON      0x4000
#define DX_TUE      0x2000
#define DX_WED      0x1000
#define DX_THU      0x0800
#define DX_FRI      0x0400
#define DX_SAT      0x0200

void dx_reload();
void dx_save_as_json();
void dx_prep_list(bool need_sort, dx_t *_dx_list, int _dx_list_len, int _dx_list_len_new);
void dx_eibi_init();

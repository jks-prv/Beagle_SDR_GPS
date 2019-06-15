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

// DX list

#define DX_HIDDEN_SLOT 1

typedef struct {
	float freq;				// must be first for qsort_floatcomp()
	int idx;
	const char *ident, *ident_s;
	const char *notes, *notes_s;
	const char *params;
	int flags;
	int low_cut;
	int high_cut;
	int offset;
	int timestamp;
	int tag;
	int masked_lo, masked_hi;   // Hz
} dx_t;

typedef struct {
	dx_t *list;
	int len;                // malloc'd length is always len + DX_HIDDEN_SLOT
	bool hidden_used;
	bool json_up_to_date;
	int *masked_idx;
	int masked_len, masked_seq;
} dxlist_t;

extern dxlist_t dx;

#define	DX_MODE	    0x000f

#define	DX_TYPE	    0x00f0
#define	DX_WL		0x0010	// on watchlist, i.e. not actually heard yet, marked as a signal to watch for
#define	DX_SB		0x0020	// a sub-band, not a station
#define	DX_DG		0x0030	// DGPS
#define	DX_SE		0x0040	// special event
#define	DX_XX		0x0050	// interference
#define	DX_MK		0x0060	// masked

#define	DX_FLAG     0xff00

void dx_reload();
void dx_save_as_json();
void dx_prep_list(bool need_sort, dx_t *_dx_list, int _dx_list_len, int _dx_list_len_new);

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

struct dx_t {
	float freq;
	const char *ident;
	const char *notes;

	int flags;
	union { float offset; float low_cut; };
	float high_cut;
};

struct dxlist_t {
	dx_t *list;
	int len;
	bool hidden_used;
};

extern dxlist_t dx;

#define	DX_MODE	0x00f
#define	AM		0x000
#define	AMN		0x001
#define	USB		0x002
#define	LSB		0x003
#define	CW		0x004
#define	CWN		0x005
#define	DATA	0x006
#define	RESV	0x007

#define	DX_TYPE	0x0f0
#define	WL		0x010	// on watchlist, i.e. not actually heard yet, marked as a signal to watch for
#define	SB		0x020	// a sub-band, not a station
#define	DG		0x030	// DGPS
#define	NoN		0x040	// MHRS NoN
#define	XX		0x050	// interference

#define	DX_FLAG	0xf00
#define	PB		0x100	// passband specified

void dx_reload();
void dx_save_as_json();

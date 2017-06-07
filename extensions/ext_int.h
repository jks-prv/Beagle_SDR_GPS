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
#include "kiwi.h"
#include "web.h"
#include "coroutines.h"
#include "ext.h"

#define	N_EXT	16		// max # of different extensions

struct conn_t;

// extension information when active on a particular RX_CHAN
struct ext_users_t {
    bool valid;
	ext_t *ext;
	conn_t *conn_ext;                       // used by ext_send_* routines
	ext_receive_iq_samps_t receive_iq;		// server-side routine for receiving IQ data
	tid_t receive_iq_tid;
	ext_receive_real_samps_t receive_real;	// server-side routine for receiving real data
	tid_t receive_real_tid;
	ext_receive_FFT_samps_t receive_FFT;	// server-side routine for receiving FFT data
	bool postFiltered;						// FFT data is post-FIR filtered
	ext_receive_S_meter_t receive_S_meter;	// server-side routine for receiving S-meter data
};

extern ext_users_t ext_users[RX_CHANS];

// internal use
void extint_setup();
void extint_init();
void extint_send_extlist(conn_t *conn);
char *extint_list_js();
void extint_load_extension_configs(conn_t *conn);
void extint_ext_users_init(int rx_chan);
void extint_setup_c2s(void *param);
void extint_c2s(void *param);

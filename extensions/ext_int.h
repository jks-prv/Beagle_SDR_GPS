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

// Copyright (c) 2016 John Seamons, ZL4VO/KF6VO

#pragma once

#include "types.h"
#include "config.h"
#include "kiwi.h"
#include "conn.h"
#include "web.h"
#include "coroutines.h"
#include "ext.h"

typedef struct {
    int notify_chan;
    u4_t notify_seq;
    #define N_NOTIFY 256
    char notify_msg[N_NOTIFY];
} extint_t;

extern extint_t extint;

// extension information when active on a particular RX_CHAN
typedef struct {
    bool valid;
	ext_t *ext;
	conn_t *conn_ext;                       // used by ext_send_* routines

    // server-side routine for receiving FFT data
	int FFT_flags;
	ext_receive_FFT_samps_t receive_FFT;

    // server-side routine for receiving IQ data
	ext_receive_iq_samps_t receive_iq_pre_fir, receive_iq_pre_agc, receive_iq_post_agc;
	tid_t receive_iq_pre_agc_tid, receive_iq_post_agc_tid;

	// server-side routine for receiving real data
    ext_receive_real_samps_t receive_real;
	tid_t receive_real_tid;

	// server-side routine for receiving S-meter data
	ext_receive_S_meter_t receive_S_meter;
} ext_users_t;

extern ext_users_t ext_users[MAX_RX_CHANS];

// internal use
void extint_setup();
void extint_init();
void extint_send_extlist(conn_t *conn);
char *extint_list_js();
void extint_load_extension_configs(conn_t *conn);
void extint_ext_users_init(int rx_chan);
void extint_setup_c2s(void *param);
void extint_c2s(void *param);
void extint_shutdown_c2s(void *param);

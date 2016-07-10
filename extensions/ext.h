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
#include "datatypes.h"

// extensions to include
#define	EXT_WSPR
#define	EXT_EXAMPLE
#define	EXT_LORAN_C

typedef void (*ext_main_t)();
typedef bool (*ext_receive_msgs_t)(char *msg, int rx_chan);

struct ext_t {
	const char *name;
	ext_main_t main;
	ext_receive_msgs_t msgs;
};

void ext_register(ext_t *ext);

typedef void (*ext_receive_iq_samps_t)(int rx_chan, int ns_out, TYPECPX *samps);
extern ext_receive_iq_samps_t ext_receive_iq_samps[RX_CHANS];
void ext_register_receive_iq_samps(ext_receive_iq_samps_t func, int rx_chan);
void ext_unregister_receive_iq_samps(int rx_chan);

typedef void (*ext_receive_real_samps_t)(int rx_chan, int ns_out, TYPEMONO16 *samps);
extern ext_receive_real_samps_t ext_receive_real_samps[RX_CHANS];
void ext_register_receive_real_samps(ext_receive_real_samps_t func, int rx_chan);
void ext_unregister_receive_real_samps(int rx_chan);

int ext_send_msg(int rx_chan, bool debug, const char *msg, ...);
int ext_send_data_msg(int rx_chan, bool debug, u1_t cmd, u1_t *bytes, int nbytes);
int ext_send_encoded_msg(int rx_chan, bool debug, const char *dst, const char *cmd, const char *fmt, ...);

double ext_get_sample_rateHz();

// internal use
void extint_setup();
void extint_init();
void extint_send_extlist(conn_t *conn);
char *extint_list_js();
void extint_load_extension_configs(conn_t *conn);
void extint_w2a(void *param);

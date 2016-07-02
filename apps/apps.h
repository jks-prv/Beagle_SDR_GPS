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

// applications
#define	APP_WSPR

void w2a_apps(void *param);

typedef void (*app_receive_iq_samps_t)(int rx_chan, int ns_out, TYPECPX *f_samps);
extern app_receive_iq_samps_t app_receive_iq_samps[RX_CHANS];
void app_register_receive_iq_samps(app_receive_iq_samps_t func, int rx_chan);
void app_unregister_receive_iq_samps(int rx_chan);

typedef bool (*app_receive_msgs_t)(char *msg, int rx_chan);
void app_register_receive_msgs(const char *name, app_receive_msgs_t func);

int app_send_msg(int rx_chan, bool debug, const char *msg, ...);
int app_send_data_msg(int rx_chan, bool debug, u1_t cmd, u1_t *bytes, int nbytes);
int app_send_encoded_msg(int rx_chan, bool debug, const char *dst, const char *cmd, const char *fmt, ...);

double app_get_sample_rateHz();

// private
void apps_setup();
void apps_init();

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
#include "coroutines.h"
#include "datatypes.h"

// extensions to exclude
//#define EXT_S4285

typedef void (*ext_main_t)();
typedef void (*ext_close_conn_t)(int rx_chan);
typedef bool (*ext_receive_msgs_t)(char *msg, int rx_chan);
typedef bool (*ext_receive_cmds_t)(u2_t key, char *cmd, int rx_chan);
typedef bool (*ext_receive_FFT_samps_t)(int rx_chan, int instance, int flags, int ratio, int ns_out, TYPECPX *samps);
typedef void (*ext_receive_iq_samps_t)(int rx_chan, int instance, int ns_out, TYPECPX *samps);
typedef void (*ext_receive_real_samps_t)(int rx_chan, int instance, int ns_out, TYPEMONO16 *samps, int freqHz);
typedef void (*ext_receive_S_meter_t)(int rx_chan, float S_meter_dBm);
typedef void (*ext_poll_t)(int rx_chan);

#define EXT_NEW_VERSION     0xcafebeef
#define EXT_NO_FLAGS        0x00
#define EXT_FLAGS_HEAVY     0x01

// used by extension server-part to describe itself
typedef struct {
	const char *name;					// name of extension, short, no whitespace
	ext_main_t main_unused;             // unused, ext_main_t routines are called via ext_init.c:extint_init()
	ext_close_conn_t close_conn;		// routine to cleanup when connection closed
	ext_receive_msgs_t receive_msgs;	// routine to receive messages from client-part

    u4_t version;                       // for backward compatibility with external extensions (e.g. antenna switch)
	u4_t flags;
	ext_poll_t poll_cb;                 // periodic callback that cal be used for polling (e.g. shared mem comm)
} ext_t;

void ext_register(ext_t *ext);

// call to start/stop receiving raw audio channel IQ samples, pre-passband FIR filter
void ext_register_receive_iq_samps_raw(ext_receive_iq_samps_t func, int rx_chan);
void ext_unregister_receive_iq_samps_raw(int rx_chan);

// call to start/stop receiving audio channel FFT samples, pre- or post-passband FIR filter, detection & AGC
typedef enum { PRE_FILTERED = 1, POST_FILTERED = 2 } ext_FFT_flags_e;
void ext_register_receive_FFT_samps(ext_receive_FFT_samps_t func, int rx_chan, int flags);
void ext_unregister_receive_FFT_samps(int rx_chan);

// call to start/stop receiving audio channel IQ samples, post-passband FIR filter, but pre- detector & AGC
typedef enum { PRE_AGC = 0, POST_AGC = 1 } ext_IQ_flags_e;
void ext_register_receive_iq_samps(ext_receive_iq_samps_t func, int rx_chan, int flags = PRE_AGC);
void ext_register_receive_iq_samps_task(tid_t tid, int rx_chan, int flags = PRE_AGC);
void ext_unregister_receive_iq_samps(int rx_chan);
void ext_unregister_receive_iq_samps_task(int rx_chan);

// call to start/stop receiving audio channel real samples, post-passband FIR filter, detection & AGC
void ext_register_receive_real_samps(ext_receive_real_samps_t func, int rx_chan);
void ext_register_receive_real_samps_task(tid_t tid, int rx_chan);
void ext_unregister_receive_real_samps(int rx_chan);
void ext_unregister_receive_real_samps_task(int rx_chan);

// call to start/stop receiving S-meter data
void ext_register_receive_S_meter(ext_receive_S_meter_t func, int rx_chan);
void ext_unregister_receive_S_meter(int rx_chan);

// call to start/stop receiving cmds
void ext_register_receive_cmds(ext_receive_cmds_t func, int rx_chan);
void ext_unregister_receive_cmds(int rx_chan);

// general routines
enum { ADC_CLK_SYS = -1, ADC_CLK_TYP = -2, RX_CHAN_CUR = -3 };
double ext_update_get_sample_rateHz(int rx_chan);   // return sample rate of audio channel
typedef enum { AUTH_USER = 0, AUTH_LOCAL = 1, AUTH_PASSWORD = 2 } ext_auth_e;
ext_auth_e ext_auth(int rx_chan);
void ext_notify_connected(int rx_chan, u4_t seq, char *msg);
void ext_kick(int rx_chan);
double ext_get_displayed_freq_kHz(int rx_chan);
int ext_get_mode(int rx_chan);

// routines to send messages to extension client-part
C_LINKAGE(int ext_send_msg(int rx_chan, bool debug, const char *msg, ...));
int ext_send_msg_data(int rx_chan, bool debug, u1_t cmd, u1_t *bytes, int nbytes);
int ext_send_msg_data2(int rx_chan, bool debug, u1_t cmd, u1_t data2, u1_t *bytes, int nbytes);
C_LINKAGE(int ext_send_msg_encoded(int rx_chan, bool debug, const char *dst, const char *cmd, const char *fmt, ...));

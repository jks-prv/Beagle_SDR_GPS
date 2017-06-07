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

// Copyright (c) 2015 John Seamons, ZL/KF6VO

#pragma once

#include "types.h"
#include "kiwi.gen.h"
#include "datatypes.h"
#include "fastfir.h"
#include "web.h"
#include "coroutines.h"
#include "misc.h"

#define	I	0
#define	Q	1
#define	NIQ	2

// The hardware returns RXO_BITS (typically 24-bits) and scaling down by RXOUT_SCALE
// will convert this to a +/- 1.0 float.
// But the CuteSDR code assumes a scaling of +/- 32.0k, so we scale up by CUTESDR_SCALE.
#define	RXOUT_SCALE	(RXO_BITS-1)	// s24 -> +/- 1.0
#define	CUTESDR_SCALE	15			// +/- 1.0 -> +/- 32.0k (s16 equivalent)
#define CUTESDR_MAX_VAL ((float) ((1 << CUTESDR_SCALE) - 1))
#define CUTESDR_MAX_PWR (CUTESDR_MAX_VAL * CUTESDR_MAX_VAL)

// Use odd values so periodic signals like radars running at even-Hz rates don't
// beat against update rate and produce artifacts or blanking.
#define	WF_SPEED_MAX		23
#define	WF_SPEED_SLOW		1
#define	WF_SPEED_MED		17
#define	WF_SPEED_FAST		WF_SPEED_MAX

#define	WEB_SERVER_POLL_US	(1000000 / WF_SPEED_MAX / 2)

extern int version_maj, version_min;
extern rx_chan_t rx_channels[];
extern conn_t conns[];
extern bool background_mode, adc_clock_enable, need_hardware, no_net, test_flag, gps_always_acq,
	DUC_enable_start, web_nocache, web_caching_debug;
extern int p0, p1, p2, wf_sim, wf_real, wf_time, ev_dump, wf_flip, wf_exit, wf_start, tone, down, navg,
	rx_cordic, rx_cic, rx_cic2, rx_dump, wf_cordic, wf_cic, wf_mult, wf_mult_gen, meas, do_dyn_dns,
	rx_yield, gps_chans, spi_clkg, spi_speed, wf_max, rx_num, wf_num, do_slice, do_gps, do_sdr, wf_olap,
	spi_delay, do_fft, noisePwr, unwrap, rev_iq, ineg, qneg, fft_file, fftsize, fftuse, bg, alt_port,
	color_map, port, print_stats, ecpu_cmds, ecpu_tcmds, serial_number,
	use_spidev, inactivity_timeout_mins, S_meter_cal, current_nusers, debug_v, debian_ver,
	utc_offset, dst_offset;
extern float g_genfreq, g_genampl, g_mixfreq;
extern double ui_srate;
extern double DC_offset_I, DC_offset_Q;
extern char *cpu_stats_buf, *tzone_id, *tzone_name;

extern lock_t spi_lock;
extern volatile float audio_kbps, waterfall_kbps, waterfall_fps[RX_CHANS+1], http_kbps;
extern volatile int audio_bytes, waterfall_bytes, waterfall_frames[], http_bytes;

// sound
struct snd_t {
	u4_t seq;
    #ifdef SND_SEQ_CHECK
        bool snd_seq_init;
	    u4_t snd_seq;
    #endif
};

extern snd_t snd_inst[RX_CHANS];

struct snd_pkt_t {
	struct {
		char id[4];
		u4_t seq;           // waterfall syncs to this sequence number on the client-side
		char smeter[2];
	} __attribute__((packed)) h;
	union {
        u1_t buf_iq[FASTFIR_OUTBUF_SIZE * 2 * sizeof(u2_t)];
        u1_t buf_real[FASTFIR_OUTBUF_SIZE * sizeof(u2_t)];
    };
} __attribute__((packed));

#define N_MODE 8
extern const char *mode_s[N_MODE], *modu_s[N_MODE];	// = { "am", "amn", "usb", "lsb", "cw", "cwn", "nbfm", "iq" };
enum mode_e { MODE_AM, MODE_AMN, MODE_USB, MODE_LSB, MODE_CW, MODE_CWN, MODE_NBFM, MODE_IQ };

#define	KEEPALIVE_SEC		60

void fpga_init();

void rx_server_init();
void rx_server_remove(conn_t *c);
int rx_server_users();
void rx_server_user_kick();
void rx_server_send_config(conn_t *conn);

void update_vars_from_config();
void cfg_adm_transition();
bool rx_common_cmd(const char *stream_name, conn_t *conn, char *cmd);
void dump();

enum websocket_mode_e { WS_MODE_ALLOC, WS_MODE_LOOKUP, WS_MODE_CLOSE };
conn_t *rx_server_websocket(struct mg_connection *mc, websocket_mode_e);

void c2s_sound_init();
void c2s_sound_setup(void *param);
void c2s_sound(void *param);

void c2s_waterfall_init();
void c2s_waterfall_compression(int rx_chan, bool compression);
void c2s_waterfall_setup(void *param);
void c2s_waterfall(void *param);

void c2s_admin_setup(void *param);
void c2s_admin(void *param);

void c2s_mfg_setup(void *param);
void c2s_mfg(void *param);

extern bool update_pending, update_in_progress, backup_in_progress;
extern int pending_maj, pending_min;

void check_for_update(update_check_e type, conn_t *conn);
void schedule_update(int hour, int min);

extern bool sd_copy_in_progress;

enum logtype_e { LOG_ARRIVED, LOG_UPDATE, LOG_UPDATE_NC, LOG_LEAVING };
void loguser(conn_t *c, logtype_e type);
void webserver_collect_print_stats(int print);
void stat_task(void *param);

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
#include "printf.h"
#include "datatypes.h"
#include "coroutines.h"
#include "cfg.h"
#include "non_block.h"

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

extern int version_maj, version_min;

extern bool background_mode, need_hardware, no_net, test_flag,
	DUC_enable_start, rev_enable_start, web_nocache, web_caching_debug, auth_su, sdr_hu_debug,
	have_ant_switch_ext, gps_e1b_only, disable_led_task;

extern int p0, p1, p2, wf_sim, wf_real, wf_time, ev_dump, wf_flip, wf_exit, wf_start, tone, down, navg,
	rx_cordic, rx_cic, rx_cic2, rx_dump, wf_cordic, wf_cic, wf_mult, wf_mult_gen, meas, do_dyn_dns,
	rx_yield, gps_chans, spi_clkg, spi_speed, wf_max, rx_num, wf_num, do_slice, do_gps, do_sdr, wf_olap,
	spi_delay, do_fft, noisePwr, unwrap, rev_iq, ineg, qneg, fft_file, fftsize, fftuse, bg, alt_port,
	color_map, port, print_stats, ecpu_cmds, ecpu_tcmds, serial_number, ip_limit_mins,
	use_spidev, inactivity_timeout_mins, S_meter_cal, current_nusers, debug_v, debian_ver,
	utc_offset, dst_offset, reg_kiwisdr_com_status, sdr_hu_lo_kHz, sdr_hu_hi_kHz,
	debian_maj, debian_min, gps_debug, gps_var, gps_lo_gain, gps_cg_gain;

extern char **main_argv;

extern float g_genfreq, g_genampl, g_mixfreq;
extern double ui_srate, freq_offset;
extern TYPEREAL DC_offset_I, DC_offset_Q;
extern char *cpu_stats_buf, *tzone_id, *tzone_name;
extern char auth_su_remote_ip[NET_ADDRSTRLEN];
extern cfg_t cfg_ipl;

extern lock_t spi_lock;
extern volatile float audio_kbps, waterfall_kbps, waterfall_fps[RX_CHANS+1], http_kbps;
extern volatile int audio_bytes, waterfall_bytes, waterfall_frames[], http_bytes;

#define N_MODE 8
extern const char *mode_s[N_MODE], *modu_s[N_MODE];	// = { "am", "amn", "usb", "lsb", "cw", "cwn", "nbfm", "iq" };
typedef enum { MODE_AM, MODE_AMN, MODE_USB, MODE_LSB, MODE_CW, MODE_CWN, MODE_NBFM, MODE_IQ } mode_e;

typedef enum { DOM_SEL_NAM=0, DOM_SEL_DUC=1, DOM_SEL_PUB=2, DOM_SEL_SIP=3, DOM_SEL_REV=4 } sdr_hu_dom_sel_e;

#define	KEEPALIVE_SEC		60

// print_stats
#define STATS_GPS       0x01
#define STATS_GPS_SOLN  0x02
#define STATS_TASK      0x04

void fpga_init();

void update_vars_from_config();
void cfg_adm_transition();
void dump();

void c2s_sound_init();
void c2s_sound_setup(void *param);
void c2s_sound(void *param);

void c2s_waterfall_init();
void c2s_waterfall_compression(int rx_chan, bool compression);
void c2s_waterfall_setup(void *param);
void c2s_waterfall(void *param);

void c2s_admin_setup(void *param);
void c2s_admin_shutdown(void *param);
void c2s_admin(void *param);

void c2s_mfg_setup(void *param);
void c2s_mfg(void *param);

extern bool update_pending, update_in_progress, backup_in_progress;
extern int pending_maj, pending_min;

extern bool sd_copy_in_progress;

void webserver_collect_print_stats(int print);
void stat_task(void *param);

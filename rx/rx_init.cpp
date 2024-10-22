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

// Copyright (c) 2014-2021 John Seamons, ZL4VO/KF6VO

#include "types.h"
#include "config.h"
#include "kiwi.h"
#include "mode.h"
#include "rx.h"
#include "rx_util.h"
#include "mem.h"
#include "misc.h"
#include "str.h"
#include "printf.h"
#include "timer.h"
#include "web.h"
#include "spi.h"
#include "gps.h"
#include "cfg.h"
#include "coroutines.h"
#include "net.h"
#include "clk.h"
#include "wspr.h"
#include "FT8.h"
#include "ext_int.h"
#include "rx_noise.h"
#include "wdsp.h"
#include "security.h"
#include "options.h"
#include "ant_switch.h"

#ifdef DRM
 #include "DRM.h"
#else
 #define DRM_NREG_CHANS_DEFAULT 3
#endif

#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <sched.h>
#include <math.h>
#include <signal.h>

// copy admin-related configuration from kiwi.json to new admin.json file
void cfg_adm_transition()
{
	int i;
	bool b;
	const char *s;

	s = cfg_string("user_password", NULL, CFG_REQUIRED);
	admcfg_set_string("user_password", s);
	cfg_string_free(s);
	b = cfg_bool("user_auto_login", NULL, CFG_REQUIRED);
	admcfg_set_bool("user_auto_login", b);
	s = cfg_string("admin_password", NULL, CFG_REQUIRED);
	admcfg_set_string("admin_password", s);
	cfg_string_free(s);
	b = cfg_bool("admin_auto_login", NULL, CFG_REQUIRED);
	admcfg_set_bool("admin_auto_login", b);
	
	i = cfg_int("port", NULL, CFG_REQUIRED);
	admcfg_set_int("port", i);
	
	b = cfg_bool("enable_gps", NULL, CFG_REQUIRED);
	admcfg_set_bool("enable_gps", b);

	b = cfg_bool("update_check", NULL, CFG_REQUIRED);
	admcfg_set_bool("update_check", b);
	b = cfg_bool("update_install", NULL, CFG_REQUIRED);
	admcfg_set_bool("update_install", b);
	
	b = cfg_bool("sdr_hu_register", NULL, CFG_REQUIRED);
	admcfg_set_bool("sdr_hu_register", b);
	s = cfg_string("api_key", NULL, CFG_REQUIRED);
	admcfg_set_string("api_key", s);
	cfg_string_free(s);


	// remove from kiwi.json file
	cfg_rem_string("user_password");
	cfg_rem_bool("user_auto_login");
	cfg_rem_string("admin_password");
	cfg_rem_bool("admin_auto_login");

	cfg_rem_int("port");

	cfg_rem_bool("enable_gps");

	cfg_rem_bool("update_check");
	cfg_rem_bool("update_install");

	cfg_rem_bool("sdr_hu_register");
	cfg_rem_string("api_key");


	// won't be present first time after upgrading from v1.2
	// first admin page connection will create
	if ((s = cfg_object("ip_address", NULL, CFG_OPTIONAL)) != NULL) {
		admcfg_set_object("ip_address", s);
		cfg_object_free(s);
		cfg_rem_object("ip_address");
	}


	// update JSON files
	admcfg_save_json(cfg_adm.json);     // during init doesn't conflict with admin cfg
	cfg_save_json(cfg_cfg.json);        // during init doesn't conflict with admin cfg
}

int inactivity_timeout_mins, ip_limit_mins;
int S_meter_cal, waterfall_cal;
double ui_srate, ui_srate_kHz;
int kiwi_reg_lo_kHz, kiwi_reg_hi_kHz;
float max_thr;
int n_camp;
bool log_local_ip, DRM_enable, admin_keepalive, any_preempt_autorun;

#define DC_OFFSET_DEFAULT -0.02F
#define DC_OFFSET_DEFAULT_PREV 0.05F
#define DC_OFFSET_DEFAULT_20kHz -0.034F
TYPEREAL DC_offset_I, DC_offset_Q;

#define WATERFALL_CALIBRATION_DEFAULT -13
#define SMETER_CALIBRATION_DEFAULT -13

#define N_MTU 3
static int mtu_v[N_MTU] = { 1500, 1440, 1400 };

static int snr_interval[] = { 0, 1, 4, 6, 24 };

void update_freqs(bool *update_cfg)
{
    int srate_idx = cfg_default_int("max_freq", 0, update_cfg);
	ui_srate = srate_idx? 32*MHz : 30*MHz;
	ui_srate_kHz = round(ui_srate/kHz);
    double foff_kHz = cfg_default_float("freq_offset", 0, update_cfg);
    rx_set_freq_offset_kHz(foff_kHz);
    //printf("foff: INIT %.3f\n", foff_kHz);
	//printf("ui_srate=%.3f ui_srate_kHz=%.3f freq.offset_kHz=%.3f freq.offmax_kHz=%.3f\n",
	//    ui_srate, ui_srate_kHz, freq.offset_kHz, freq.offmax_kHz);
}

void update_vars_from_config(bool called_at_init)
{
    int n;
	bool update_cfg = false;
	bool up_cfg = false;
	bool update_admcfg = false;
	const char *s;
    bool err;

    // When called by client-side "SET save_cfg/save_adm=":
	//  Makes C copies of vars that must be updated when configuration saved from js.
	//
	// When called by server-side rx_server_init():
	//  Makes C copies of vars that must be updated when configuration loaded from cfg files.
	//  Creates configuration parameters with default values that must exist for client connections.

    inactivity_timeout_mins = cfg_default_int("inactivity_timeout_mins", 0, &up_cfg);
    ip_limit_mins = cfg_default_int("ip_limit_mins", 0, &up_cfg);
    
	double prev_freq_offset_kHz = freq.offset_kHz;
    update_freqs(&up_cfg);
	if (freq.offset_kHz != prev_freq_offset_kHz) {
	    update_masked_freqs();
	}

    // force DC offsets to the default value if not configured
    // also if set to the previous default value
    int firmware_sel = admcfg_default_int("firmware_sel", 0, &update_admcfg);   // needed below
    int mode_20kHz = (firmware_sel == RX3_WF3)? 1:0;
    TYPEREAL Ioff, Ioff_20kHz, Qoff, Qoff_20kHz;
    //printf("mode_20kHz=%d\n", mode_20kHz);
    admcfg_default_int("wb_sel", 0, &update_admcfg);

    Ioff = cfg_float("DC_offset_I", &err, CFG_OPTIONAL);
    if (err || Ioff == DC_OFFSET_DEFAULT_PREV) {
        Ioff = DC_OFFSET_DEFAULT;
        cfg_set_float("DC_offset_I", Ioff);
        lprintf("DC_offset_I: no cfg or prev default, setting to default value\n");
        update_cfg = cfg_gdb_break(true);
    }

    Qoff = cfg_float("DC_offset_Q", &err, CFG_OPTIONAL);
    if (err || Qoff == DC_OFFSET_DEFAULT_PREV) {
        Qoff = DC_OFFSET_DEFAULT;
        cfg_set_float("DC_offset_Q", Ioff);
        lprintf("DC_offset_Q: no cfg or prev default, setting to default value\n");
        update_cfg = cfg_gdb_break(true);
    }

    Ioff_20kHz = cfg_float("DC_offset_20kHz_I", &err, CFG_OPTIONAL);
    if (err) {
        Ioff_20kHz = DC_OFFSET_DEFAULT_20kHz;
        cfg_set_float("DC_offset_20kHz_I", Ioff_20kHz);
        lprintf("DC_offset_20kHz_I: no cfg or prev default, setting to default value\n");
        update_cfg = cfg_gdb_break(true);
    }

    Qoff_20kHz = cfg_float("DC_offset_20kHz_Q", &err, CFG_OPTIONAL);
    if (err) {
        Qoff_20kHz = DC_OFFSET_DEFAULT_20kHz;
        cfg_set_float("DC_offset_20kHz_Q", Qoff_20kHz);
        lprintf("DC_offset_20kHz_Q: no cfg or prev default, setting to default value\n");
        update_cfg = cfg_gdb_break(true);
    }

    DC_offset_I = mode_20kHz? Ioff_20kHz : Ioff;
    DC_offset_Q = mode_20kHz? Qoff_20kHz : Qoff;
    static bool dc_off_msg;
    if (!dc_off_msg) {
        lprintf("using DC_offsets: I %.6f Q %.6f\n", DC_offset_I, DC_offset_Q);
        dc_off_msg = true;
    }

    // DRM extension related
    cfg_default_object("DRM", "{}", &up_cfg);
    DRM_enable = cfg_default_bool("DRM.enable", true, &up_cfg);
    drm_nreg_chans = cfg_default_int("DRM.nreg_chans", DRM_NREG_CHANS_DEFAULT, &up_cfg);

    s = cfg_string("DRM.test_file1", NULL, CFG_OPTIONAL);
	if (!s || strcmp(s, "Kuwait.15110.1.12k.iq.au") == 0) {
	    cfg_set_string("DRM.test_file1", "DRM.BBC.Journaline.au");
	    update_cfg = cfg_gdb_break(true);
    }
    cfg_string_free(s);

    s = cfg_string("DRM.test_file2", NULL, CFG_OPTIONAL);
	if (!s || strcmp(s, "Delhi.828.1.12k.iq.au") == 0) {
	    cfg_set_string("DRM.test_file2", "DRM.KTWR.slideshow.au");
	    update_cfg = cfg_gdb_break(true);
    }
    cfg_string_free(s);
    
    
    // TDoA extension related
    cfg_default_object("tdoa", "{}", &up_cfg);
    // FIXME: switch to using new SSL version of TDoA service at some point: https://tdoa2.kiwisdr.com
    // workaround to prevent collision with 1st-level "server_url" until we can fix cfg code
	if ((s = cfg_string("tdoa.server_url", NULL, CFG_OPTIONAL)) != NULL) {
		cfg_set_string("tdoa.server", s);
	    cfg_string_free(s);
	    cfg_rem_string("tdoa.server_url");
	    update_cfg = cfg_gdb_break(true);
	} else {
        cfg_default_string("tdoa.server", "http://tdoa.kiwisdr.com", &update_admcfg);
    }


    // fix any broken UTF-8 sequences via cfg_default_string()
    cfg_default_object("index_html_params", "{}", &up_cfg);
    cfg_default_string("index_html_params.HTML_HEAD", "", &up_cfg);
    cfg_default_string("index_html_params.USER_LOGIN", "", &up_cfg);
    cfg_default_string("index_html_params.PAGE_TITLE", "", &up_cfg);
    cfg_default_string("index_html_params.RX_PHOTO_TITLE", "", &up_cfg);
    cfg_default_string("index_html_params.RX_PHOTO_DESC", "", &up_cfg);
    cfg_default_string("index_html_params.RX_TITLE", "", &up_cfg);
    cfg_default_string("index_html_params.RX_LOC", "", &up_cfg);
    cfg_default_bool("index_html_params.RX_PHOTO_LEFT_MARGIN", true, &up_cfg);
    cfg_default_bool("index_html_params.RX_PHOTO_CENTERED", false, &up_cfg);

    cfg_default_string("status_msg", "", &up_cfg);
    cfg_default_string("rx_name", "", &up_cfg);
    cfg_default_string("rx_device", "", &up_cfg);
    cfg_default_string("rx_location", "", &up_cfg);
    cfg_default_string("rx_antenna", "", &up_cfg);
    cfg_default_string("owner_info", "", &up_cfg);
    cfg_default_string("reason_disabled", "", &up_cfg);
    cfg_default_string("panel_readme", "", &up_cfg);
    
    // pcb.jpg => pcb.png since new pcb photo has alpha channel that only .png supports.
    // Won't disturb an RX_PHOTO_FILE set to kiwi.config/photo.upload by admin photo upload process.
	if ((s = cfg_string("index_html_params.RX_PHOTO_FILE", NULL, CFG_OPTIONAL)) != NULL) {
	    if (strcmp(s, "kiwi/pcb.jpg") == 0) {
		    cfg_set_string("index_html_params.RX_PHOTO_FILE", "kiwi/pcb.png");
	        update_cfg = cfg_gdb_break(true);
	    }
	    cfg_string_free(s);
	}

	if ((s = cfg_string("index_html_params.RX_PHOTO_DESC", NULL, CFG_OPTIONAL)) != NULL) {
	    if (strcmp(s, "First production PCB") == 0) {
		    cfg_set_string("index_html_params.RX_PHOTO_DESC", "");
	        update_cfg = cfg_gdb_break(true);
	    }
	    cfg_string_free(s);
	}

    S_meter_cal = cfg_default_int("S_meter_cal", SMETER_CALIBRATION_DEFAULT, &up_cfg);
    waterfall_cal = cfg_default_int("waterfall_cal", WATERFALL_CALIBRATION_DEFAULT, &up_cfg);
    cfg_default_bool("no_zoom_corr", false, &up_cfg);
    cfg_default_bool("contact_admin", true, &up_cfg);
    cfg_default_int("chan_no_pwd", 0, &up_cfg);
    cfg_default_int("clk_adj", 0, &up_cfg);
    kiwi_reg_lo_kHz = cfg_default_int("sdr_hu_lo_kHz", 0, &up_cfg);
    kiwi_reg_hi_kHz = cfg_default_int("sdr_hu_hi_kHz", 30000, &up_cfg);

    cfg_default_bool("ext_ADC_clk", false, &up_cfg);
    cfg_default_int("ext_ADC_freq", (int) round(ADC_CLOCK_TYP), &up_cfg);
    bool ADC_clk_corr = cfg_bool("ADC_clk_corr", &err, CFG_OPTIONAL);
    if (!err) {     // convert from yes/no switch to multiple-entry menu
        int ADC_clk2_corr = ADC_clk_corr? ADC_CLK_CORR_CONTINUOUS : ADC_CLK_CORR_DISABLED;
        cfg_default_int("ADC_clk2_corr", ADC_clk2_corr, &up_cfg);
        cfg_rem_bool("ADC_clk_corr");
    } else {
        cfg_default_int("ADC_clk2_corr", ADC_CLK_CORR_CONTINUOUS, &up_cfg);
    }

    cfg_default_string("tdoa_id", "", &up_cfg);
    cfg_default_int("tdoa_nchans", -1, &up_cfg);
    cfg_default_int("ext_api_nchans", -1, &up_cfg);
    cfg_default_bool("no_wf", false, &up_cfg);
    cfg_default_bool("test_webserver_prio", false, &up_cfg);
    cfg_default_bool("test_deadline_update", false, &up_cfg);
    cfg_default_bool("disable_recent_changes", false, &up_cfg);
    cfg_default_int("rf_attn_allow", 1, &up_cfg);
    cfg_default_int("S_meter_OV_counts", 10, &up_cfg);
    cfg_default_bool("webserver_caching", true, &up_cfg);
    max_thr = (float) cfg_default_int("overload_mute", -15, &up_cfg);
    cfg_default_bool("agc_thresh_smeter", true, &up_cfg);
    n_camp = cfg_default_int("n_camp", N_CAMP, &up_cfg);
    snr_meas_interval_hrs = snr_interval[cfg_default_int("snr_meas_interval_hrs", 1, &up_cfg)];
    snr_local_time = cfg_default_bool("snr_local_time", true, &up_cfg);
    any_preempt_autorun = cfg_default_bool("any_preempt_autorun", true, &up_cfg);
    cfg_default_int("ident_len", IDENT_LEN_MIN, &up_cfg);
    cfg_default_bool("show_geo", true, &up_cfg);
    cfg_default_bool("show_geo_city", true, &up_cfg);
    cfg_default_bool("show_user", true, &up_cfg);
    cfg_default_bool("show_1Hz", false, &up_cfg);
    cfg_default_int("dx_default_db", 0, &up_cfg);
    cfg_default_int("spec_min_range", 50, &up_cfg);

    cfg_default_object("init", "{}", &up_cfg);
    cfg_default_int("init.cw_offset", 500, &up_cfg);
    cfg_default_int("init.colormap", 0, &up_cfg);
    cfg_default_int("init.aperture", 1, &up_cfg);
    cfg_default_int("init.comp", 0, &up_cfg);
    cfg_default_int("init.setup", 0, &up_cfg);
    cfg_default_int("init.tab", 0, &up_cfg);
    cfg_default_float("init.rf_attn", 0, &up_cfg);

    cfg_default_object("ant_switch", "{}", &up_cfg);
    if (ant_switch_cfg(called_at_init)) update_cfg = cfg_gdb_break(true);

    cfg_default_int("nb_algo", 0, &up_cfg);
    cfg_default_int("nb_wf", 1, &up_cfg);
	// NB_STD
    cfg_default_int("nb_gate", 100, &up_cfg);
    cfg_default_int("nb_thresh", 50, &up_cfg);
	// NB_WILD
    cfg_default_float("nb_thresh2", 0.95, &up_cfg);
    cfg_default_int("nb_taps", 10, &up_cfg);
    cfg_default_int("nb_samps", 7, &up_cfg);

    cfg_default_int("nr_algo", 0, &up_cfg);
    cfg_default_int("nr_de", 1, &up_cfg);
    cfg_default_int("nr_an", 0, &up_cfg);
   // NR_WDSP
    cfg_default_int("nr_wdspDeTaps", 64, &up_cfg);
    cfg_default_int("nr_wdspDeDelay",16, &up_cfg);
    cfg_default_int("nr_wdspDeGain", 10, &up_cfg);
    cfg_default_int("nr_wdspDeLeak", 7, &up_cfg);
    cfg_default_int("nr_wdspAnTaps", 64, &up_cfg);
    cfg_default_int("nr_wdspAnDelay",16, &up_cfg);
    cfg_default_int("nr_wdspAnGain", 10, &up_cfg);
    cfg_default_int("nr_wdspAnLeak", 7, &up_cfg);
   // NR_ORIG
    cfg_default_int("nr_origDeDelay",1, &up_cfg);
    cfg_default_float("nr_origDeBeta", 0.05, &up_cfg);
    cfg_default_float("nr_origDeDecay", 0.98, &up_cfg);
    cfg_default_int("nr_origAnDelay",48, &up_cfg);
    cfg_default_float("nr_origAnBeta", 0.125, &up_cfg);
    cfg_default_float("nr_origAnDecay", 0.99915, &up_cfg);
   // NR_SPECTRAL
    cfg_default_int("nr_specGain", 0, &up_cfg);
    cfg_default_float("nr_specAlpha", 0.95, &up_cfg);
    cfg_default_int("nr_specSNR", 30, &up_cfg);

    // Change default to false. Only do this once.
    if (!cfg_true("require_id_setup")) {
        kiwi.require_id = cfg_set_bool("require_id", false);
        cfg_set_bool("require_id_setup", true);
    } else {
        kiwi.require_id = cfg_default_bool("require_id", false, &up_cfg);
    }

    // Only handle forced speed changes here as ESPEED_AUTO is always in effect after a reboot.
    // ethtool doesn't seem to have a way to go back to auto speed once a forced speed is set?
    int espeed = cfg_default_int("ethernet_speed", 0, &up_cfg);
    static int current_espeed;
    if (espeed != current_espeed || (kiwi.platform == PLATFORM_BB_AI64 && current_espeed == ESPEED_10M)) {
        if (kiwi.platform == PLATFORM_BB_AI64 && (espeed == ESPEED_10M || current_espeed == ESPEED_10M)) {
            lprintf("ETH0 CAUTION: BBAI-64 doesn't support 10 mbps. Reverting to auto speed.\n");
            espeed = ESPEED_AUTO;
            if (called_at_init)
                cfg_set_int("ethernet_speed", espeed);
        } else {
            lprintf("ETH0 ethernet speed %s\n", (espeed == ESPEED_AUTO)? "auto" : ((espeed == ESPEED_10M)? "10M" : "100M"));
            if (espeed) {
                non_blocking_cmd_system_child("kiwi.ethtool",
                    stprintf("ethtool -s eth0 speed %d duplex full", (espeed == ESPEED_10M)? 10:100), NO_WAIT);
            }
        }
        current_espeed = espeed;
    }
    
    int mtu = cfg_default_int("ethernet_mtu", 0, &up_cfg);
    if (mtu < 0 || mtu >= N_MTU) mtu = 0;
    static int current_mtu;
    if (mtu != current_mtu) {
        int mtu_val = mtu_v[mtu];
        printf("ETH0 ifconfig eth0 mtu %d\n", mtu_val);
        non_blocking_cmd_system_child("kiwi.ifconfig", stprintf("ifconfig eth0 mtu %d", mtu_val), NO_WAIT);
        current_mtu = mtu;
    }
    
    #ifdef USE_SDR
        if (wspr_update_vars_from_config(called_at_init)) update_cfg = cfg_gdb_break(true);

        // fix corruption left by v1.131 dotdot bug
        // i.e. "WSPR.autorun": N instead of "WSPR": { "autorun": N ... }"
        _cfg_int(&cfg_cfg, "WSPR.autorun", &err, CFG_OPTIONAL|CFG_NO_DOT);
        if (!err) {
            _cfg_set_int(&cfg_cfg, "WSPR.autorun", 0, CFG_REMOVE|CFG_NO_DOT, 0);
            _cfg_set_bool(&cfg_cfg, "index_html_params.RX_PHOTO_LEFT_MARGIN", 0, CFG_REMOVE|CFG_NO_DOT, 0);
            printf("removed v1.131 dotdot bug corruption\n");
            update_cfg = cfg_gdb_break(true);
        }

        if (ft8_update_vars_from_config(called_at_init)) update_cfg = cfg_gdb_break(true);
    #endif
    
    // enforce waterfall min_dB < max_dB
    int min_dB = cfg_default_int("init.min_dB", -110, &up_cfg);
    int max_dB = cfg_default_int("init.max_dB", -10, &up_cfg);
    if (min_dB >= max_dB) {
        cfg_set_int("init.min_dB", -110);
        cfg_set_int("init.max_dB", -10);
        update_cfg = cfg_gdb_break(true);
    }
    cfg_default_int("init.floor_dB", 0, &up_cfg);
    cfg_default_int("init.ceil_dB", 5, &up_cfg);

    int _dom_sel = cfg_default_int("sdr_hu_dom_sel", DOM_SEL_NAM, &up_cfg);

    #if 0
        // try and get this Kiwi working with the proxy
        //printf("serno=%d dom_sel=%d\n", serial_number, _dom_sel);
	    if (serial_number == 1006 && _dom_sel == DOM_SEL_NAM) {
            cfg_set_int("sdr_hu_dom_sel", DOM_SEL_REV);
            update_cfg = cfg_gdb_break(true);
            lprintf("######## FORCE DOM_SEL_REV serno=%d ########\n", serial_number);
	    }
    #endif
    
    // remove old kiwisdr.example.com default
    cfg_default_string("server_url", "", &up_cfg);
    const char *server_url = cfg_string("server_url", NULL, CFG_REQUIRED);
	if (strcmp(server_url, "kiwisdr.example.com") == 0) {
	    cfg_set_string("server_url", "");
	    update_cfg = cfg_gdb_break(true);
	}
    
    // not sure I want to do this yet..
    #if 0
        // Strange problem where cfg.sdr_hu_dom_sel seems to get changed occasionally between modes
        // DOM_SEL_NAM=0 and DOM_SEL_PUB=2. This can result in DOM_SEL_NAM selected but the corresponding
        // domain field blank which has bad consequences (e.g. TDoA host file corrupted).
        // So do some consistency checking here.
        if (dom_sel == DOM_SEL_NAM && (*server_url == '\0' || strcmp(server_url, "kiwisdr.example.com") == 0)) {
            lprintf("### DOM_SEL check: DOM_SEL_NAM but server_url=\"%s\"\n", server_url);
            lprintf("### DOM_SEL check: forcing change to DOM_SEL_PUB\n");
            cfg_set_int("sdr_hu_dom_sel", DOM_SEL_PUB);
            // FIXME: but then server_url needs to be set when pub ip is detected
            update_cfg = cfg_gdb_break(true);
        }
	#endif
    cfg_string_free(server_url); server_url = NULL;
    
    // move kiwi.json tlimit_exempt_pwd to admin.json
	if ((s = cfg_string("tlimit_exempt_pwd", NULL, CFG_OPTIONAL)) != NULL) {
		admcfg_set_string("tlimit_exempt_pwd", s);
	    cfg_string_free(s);
	    cfg_rem_string("tlimit_exempt_pwd");
	    update_cfg = update_admcfg = true;
	} else {
        admcfg_default_string("tlimit_exempt_pwd", "", &update_admcfg);
    }
    
    // sdr.hu => rx.kiwisdr.com in status msg
    char *status_msg = (char *) cfg_string("status_msg", NULL, CFG_REQUIRED);
    bool caller_must_free;
	char *nsm = kiwi_str_replace(status_msg, "sdr.hu", "rx.kiwisdr.com", &caller_must_free);
	if (nsm) {
	    nsm = kiwi_str_replace(nsm, "/?top=kiwi", "");  // shrinking, so nsm same memory space
	    cfg_set_string("status_msg", nsm);
	    if (caller_must_free) kiwi_ifree(nsm, "update_vars_from_config nsm");
	    update_cfg = cfg_gdb_break(true);
    } else {
        // convert from old URL
        if (strcmp(status_msg, "Try other KiwiSDRs world-wide at <a href='http://kiwisdr.com/public/' target='_blank'>http://kiwisdr.com/public/</a>") == 0) {
            cfg_set_string("status_msg", "Try other KiwiSDRs world-wide at <a href='http://rx.kiwisdr.com' target='_blank'>rx.kiwisdr.com</a>");
            update_cfg = cfg_gdb_break(true);
        }
    }
    cfg_string_free(status_msg); status_msg = NULL;

    char *rx_name = (char *) cfg_string("rx_name", NULL, CFG_REQUIRED);
    // shrinking, so same memory space
	nsm = kiwi_str_replace(rx_name, ", ZL/KF6VO, New Zealand", "");
	if (nsm) {
        cfg_set_string("rx_name", nsm);
        update_cfg = cfg_gdb_break(true);
    }
    cfg_string_free(rx_name); rx_name = NULL;

    char *rx_title = (char *) cfg_string("index_html_params.RX_TITLE", &err, CFG_OPTIONAL);
    if (!err) {
        // shrinking, so same memory space
        nsm = kiwi_str_replace(rx_title, " at <a href='http://kiwisdr.com' target='_blank' onclick='dont_toggle_rx_photo()'>ZL/KF6VO</a>", "");
        if (nsm) {
            cfg_set_string("index_html_params.RX_TITLE", nsm);
            update_cfg = cfg_gdb_break(true);
        }
        cfg_string_free(rx_title); rx_title = NULL;
    }

    // change init.mode from mode menu idx to a mode string
	n = cfg_int("init.mode", &err, CFG_OPTIONAL);
	if (err) {
	    s = cfg_string("init.mode", &err, CFG_OPTIONAL);
	    if (err) {
	        cfg_set_string("init.mode", "lsb");     // init.mode never existed?
	        update_cfg = cfg_gdb_break(true);
	    }
        cfg_string_free(s);
	} else {
	    cfg_rem_int("init.mode");
	    cfg_set_string("init.mode", mode_lc[n]);
	    update_cfg = cfg_gdb_break(true);
	}

    if (up_cfg) update_cfg = cfg_gdb_break(true);
	if (update_cfg) {
        //printf("_cfg_save_json update_cfg\n");
		cfg_save_json(cfg_cfg.json);    // during init doesn't conflict with admin cfg
	}


	// same, but for admin config
	// currently just default values that need to exist
	
    admcfg_default_bool("server_enabled", true, &update_admcfg);
    admcfg_default_bool("auto_add_nat", false, &update_admcfg);
    admcfg_default_bool("duc_enable", false, &update_admcfg);
    admcfg_default_string("duc_user", "", &update_admcfg);
    admcfg_default_string("duc_pass", "", &update_admcfg);
    admcfg_default_string("duc_host", "", &update_admcfg);
    admcfg_default_int("duc_update", 3, &update_admcfg);
    admcfg_default_int("restart_update", 1, &update_admcfg);    // by default don't update after a restart
    admcfg_default_int("update_restart", 0, &update_admcfg);
    admcfg_default_bool("update_major_only", false, &update_admcfg);
    admcfg_default_string("ip_address.dns1", "1.1.1.1", &update_admcfg);
    admcfg_default_string("ip_address.dns2", "8.8.8.8", &update_admcfg);
    admcfg_default_string("url_redirect", "", &update_admcfg);

    admcfg_default_bool("ip_blacklist_auto_download", true, &update_admcfg);
    admcfg_default_string("ip_blacklist", "47.88.219.24/24", &update_admcfg);
    admcfg_default_string("ip_blacklist_local", "", &update_admcfg);
    admcfg_default_int("ip_blacklist_mtime", 0, &update_admcfg);
    net.ip_blacklist_port_only = admcfg_default_int("ip_blacklist_port", 0, &update_admcfg);

    admcfg_default_bool("no_dup_ip", false, &update_admcfg);
    admcfg_default_bool("my_kiwi", true, &update_admcfg);
    admcfg_default_bool("onetime_password_check", false, &update_admcfg);
    admcfg_default_bool("dx_labels_converted", false, &update_admcfg);
    admcfg_default_string("proxy_server", PROXY_SERVER_HOST, &update_admcfg);
    admin_keepalive = admcfg_default_bool("admin_keepalive", true, &update_admcfg);
    log_local_ip = admcfg_default_bool("log_local_ip", true, &update_admcfg);
    admcfg_default_bool("dx_comm_auto_download", true, &update_admcfg);
    kiwi.restart_delay = admcfg_default_int("restart_delay", RESTART_DELAY_30_SEC, &update_admcfg);
    
    // convert daily_restart switch bool => menu int
    bool daily_restart_bool = admcfg_bool("daily_restart", &err, CFG_OPTIONAL);
    daily_restart_e daily_restart;
    if (!err) {
        daily_restart = daily_restart_bool? DAILY_RESTART : DAILY_RESTART_NO;
        admcfg_rem_bool("daily_restart");
    } else {
        daily_restart = DAILY_RESTART_NO;   // default if it doesn't already exist
    }
    kiwi.daily_restart = (daily_restart_e) admcfg_default_int("daily_restart", daily_restart, &update_admcfg);

    // decouple rx.kiwisdr.com and sdr.hu registration
    bool sdr_hu_register = admcfg_bool("sdr_hu_register", NULL, CFG_REQUIRED);
	admcfg_bool("kiwisdr_com_register", &err, CFG_OPTIONAL);
    // never set or incorrectly set to false by v1.365,366
	if (err || (VERSION_MAJ == 1 && VERSION_MIN <= 369)) {
        admcfg_set_bool("kiwisdr_com_register", sdr_hu_register);
        update_admcfg = true;
    }

    // disable public registration if all the channels are full of WSPR/FT8 autorun
	bool isPublic = admcfg_bool("kiwisdr_com_register", NULL, CFG_REQUIRED);
	int wspr_autorun = cfg_int("WSPR.autorun", NULL, CFG_REQUIRED);
	int ft8_autorun = cfg_int("ft8.autorun", NULL, CFG_REQUIRED);
	if (isPublic && (wspr_autorun + ft8_autorun) >= rx_chans) {
	    lprintf("REG: WSPR.autorun(%d) + ft8.autorun(%d) >= rx_chans(%d) -- DISABLING PUBLIC REGISTRATION\n", wspr_autorun, ft8_autorun, rx_chans);
        admcfg_set_bool("kiwisdr_com_register", false);
        update_admcfg = true;
	}

    // historical uses of options parameter:
    //int new_find_local = admcfg_int("options", NULL, CFG_REQUIRED) & 1;
    admcfg_default_int("options", 0, &update_admcfg);

    #ifdef USE_GPS
        admcfg_default_bool("GPS_tstamp", true, &update_admcfg);
        admcfg_default_bool("use_kalman_position_solver", true, &update_admcfg);
        admcfg_default_int("rssi_azel_iq", 0, &update_admcfg);

        admcfg_default_bool("always_acq_gps", false, &update_admcfg);
        gps.set_date = admcfg_default_bool("gps_set_date", false, &update_admcfg);
        gps.include_alert_gps = admcfg_default_bool("include_alert_gps", false, &update_admcfg);
        //real_printf("gps.include_alert_gps=%d\n", gps.include_alert_gps);
        gps.include_E1B = admcfg_default_bool("include_E1B", true, &update_admcfg);
        //real_printf("gps.include_E1B=%d\n", gps.include_E1B);
        admcfg_default_int("E1B_offset", 4, &update_admcfg);

        gps.acq_Navstar = admcfg_default_bool("acq_Navstar", true, &update_admcfg);
        if (!gps.acq_Navstar) ChanRemove(Navstar);
        gps.acq_QZSS = admcfg_default_bool("acq_QZSS", true, &update_admcfg);
        if (!gps.acq_QZSS) ChanRemove(QZSS);
        gps.QZSS_prio = admcfg_default_bool("QZSS_prio", false, &update_admcfg);
        gps.acq_Galileo = admcfg_default_bool("acq_Galileo", true, &update_admcfg);
        if (!gps.acq_Galileo) ChanRemove(E1B);
        //real_printf("Navstar=%d QZSS=%d Galileo=%d\n", gps.acq_Navstar, gps.acq_QZSS, gps.acq_Galileo);

        // force plot_E1B true because there is no longer an option switch in the admin interface (to make room for new ones)
        bool plot_E1B = admcfg_default_bool("plot_E1B", true, &update_admcfg);
        if (!plot_E1B) {
            admcfg_set_bool("plot_E1B", true);
            update_admcfg = true;
        }
    #endif
    
    #ifdef CRYPT_PW
    
        // Either:
        // 1) Transitioning on startup from passwords stored in admin.json file from a prior version
        //      (.eup/.eap files will not exist).
        // or
        // 2) In response to a password change from the admin UI (sequence number will increment).
        //
        // Generates and saves a new salt/hash in either case.
        // The old {user,admin}_password fields are kept because other code needs to know when
        // the password is blank/empty. So it is either set to the empty string or "(encrypted)".
        // This does not preclude the user from using the string "(encrypted)" as an actual password.
        
        bool ok;
        const char *key;
        char *encrypted;
        static u4_t user_pwd_seq, admin_pwd_seq;
        
        if (called_at_init) {
            user_pwd_seq = admcfg_default_int("user_pwd_seq", 0, &update_admcfg);
            admin_pwd_seq = admcfg_default_int("admin_pwd_seq", 0, &update_admcfg);
        }
        u4_t updated_user_pwd_seq = admcfg_int("user_pwd_seq", NULL, CFG_REQUIRED);
        u4_t updated_admin_pwd_seq = admcfg_int("admin_pwd_seq", NULL, CFG_REQUIRED);

        bool eup_exists = kiwi_file_exists(DIR_CFG "/.eup");
        bool user_seq_diff = (user_pwd_seq != updated_user_pwd_seq);
        if (!eup_exists || user_seq_diff) {
            user_pwd_seq = updated_user_pwd_seq;
    	    key = admcfg_string("user_password", NULL, CFG_REQUIRED);
    	    
    	    if (!eup_exists) {
    	        // if hash file is missing, but key indicates encryption previously used, make key empty
    	        if (key && strcmp(key, "(encrypted)") == 0) {
                    cfg_string_free(key);
                    key = NULL;
                }
    	    }

            encrypted = kiwi_crypt_generate(key, user_pwd_seq);
            printf("### user crypt ok=%d seq=%d key=<%s> => %s\n", ok, updated_user_pwd_seq, key, encrypted);
            n = kiwi_file_write("eup", DIR_CFG "/.eup", encrypted, strlen(encrypted), /* add_nl */ true);
            free(encrypted);

            if (n) {
                admcfg_set_string("user_password", (key == NULL || *key == '\0')? "" : "(encrypted)");
                update_admcfg = true;
            }

            cfg_string_free(key);
        }

        bool eap_exists = kiwi_file_exists(DIR_CFG "/.eap");
        bool admin_seq_diff = (admin_pwd_seq != updated_admin_pwd_seq);
        if (!eap_exists || admin_seq_diff) {
            admin_pwd_seq = updated_admin_pwd_seq;
    	    key = admcfg_string("admin_password", NULL, CFG_REQUIRED);

    	    if (!eap_exists) {
    	        // if hash file is missing, but key indicates encryption previously used, make key empty
    	        if (key && strcmp(key, "(encrypted)") == 0) {
                    cfg_string_free(key);
                    key = NULL;
                }
    	    }

            encrypted = kiwi_crypt_generate(key, admin_pwd_seq);
            printf("### admin crypt ok=%d seq=%d key=<%s> => %s\n", ok, updated_admin_pwd_seq, key, encrypted);
            n = kiwi_file_write("eap", DIR_CFG "/.eap", encrypted, strlen(encrypted), /* add_nl */ true);
            free(encrypted);

            if (n) {
                admcfg_set_string("admin_password", (key == NULL || *key == '\0')? "" : "(encrypted)");
                update_admcfg = true;
            }

            cfg_string_free(key);
        }
    #endif
    
    // FIXME: resolve problem of ip_address.xxx vs ip_address:{xxx} in .json files
    //admcfg_default_bool("ip_address.use_static", false, &update_admcfg);

	if (update_admcfg)
		admcfg_save_json(cfg_adm.json);     // during init doesn't conflict with admin cfg


    // one-time-per-run initializations
    
    static bool initial_clk_adj;
    if (!initial_clk_adj) {
        int clk_adj = cfg_int("clk_adj", &err, CFG_OPTIONAL);
        if (err == false) {
            printf("INITIAL clk_adj=%d\n", clk_adj);
            if (clk_adj != 0) {
                clock_manual_adj(clk_adj);
            }
        }
        initial_clk_adj = true;
    }
}

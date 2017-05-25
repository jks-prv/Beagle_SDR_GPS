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

// Copyright (c) 2014-2016 John Seamons, ZL/KF6VO

#include "types.h"
#include "config.h"
#include "kiwi.h"
#include "misc.h"
#include "str.h"
#include "printf.h"
#include "timer.h"
#include "web.h"
#include "peri.h"
#include "spi.h"
#include "gps.h"
#include "cfg.h"
#include "dx.h"
#include "coroutines.h"
#include "data_pump.h"
#include "ext_int.h"
#include "net.h"

#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <sched.h>
#include <math.h>
#include <signal.h>
#include <fftw3.h>

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
	admcfg_save_json(cfg_adm.json);
	cfg_save_json(cfg_cfg.json);
}

int inactivity_timeout_mins;
int S_meter_cal;
double ui_srate;

#define DC_OFFSET_DEFAULT 0.05
double DC_offset_I, DC_offset_Q;

#define WATERFALL_CALIBRATION_DEFAULT -13
#define SMETER_CALIBRATION_DEFAULT -13

void update_vars_from_config()
{
	bool update_cfg = false;

	// C copies of vars that must be updated when configuration loaded or saved from file
	// or configuration parameters that must exist for client connections
	// (i.e. have default values assigned).

    inactivity_timeout_mins = cfg_default_int("inactivity_timeout_mins", 0, &update_cfg);

    int srate_idx = cfg_default_int("max_freq", 0, &update_cfg);
	ui_srate = srate_idx? 32*MHz : 30*MHz;

    // force DC offsets to the default value
    bool err;
    DC_offset_I = cfg_float("DC_offset_I", &err, CFG_OPTIONAL);
    if (err || DC_offset_I != DC_OFFSET_DEFAULT) {
        cfg_set_float("DC_offset_I", DC_OFFSET_DEFAULT);
        DC_offset_I = DC_OFFSET_DEFAULT;
        update_cfg = true;
    }
    DC_offset_Q = cfg_float("DC_offset_Q", &err, CFG_OPTIONAL);
    if (err || DC_offset_Q != DC_OFFSET_DEFAULT) {
        cfg_set_float("DC_offset_Q", DC_OFFSET_DEFAULT);
        DC_offset_Q = DC_OFFSET_DEFAULT;
        update_cfg = true;
    }

    S_meter_cal = cfg_default_int("S_meter_cal", SMETER_CALIBRATION_DEFAULT, &update_cfg);
    cfg_default_int("waterfall_cal", WATERFALL_CALIBRATION_DEFAULT, &update_cfg);
    cfg_default_bool("contact_admin", true, &update_cfg);
    cfg_default_int("chan_no_pwd", 0, &update_cfg);
    cfg_default_string("owner_info", "", &update_cfg);
    cfg_default_int("WSPR.autorun", 0, &update_cfg);

	if (update_cfg)
		cfg_save_json(cfg_cfg.json);


	// same, but for admin config
	
	bool update_admcfg = false;
	
    admcfg_default_bool("server_enabled", true, &update_admcfg);
    admcfg_default_bool("auto_add_nat", false, &update_admcfg);
    admcfg_default_bool("duc_enable", false, &update_admcfg);
    admcfg_default_string("duc_user", "", &update_admcfg);
    admcfg_default_string("duc_pass", "", &update_admcfg);
    admcfg_default_string("duc_host", "", &update_admcfg);
    admcfg_default_int("duc_update", 3, &update_admcfg);

	if (update_admcfg)
		admcfg_save_json(cfg_adm.json);
}

static void geoloc_task(void *param)
{
	conn_t *conn = (conn_t *) param;
	int n, stat;
	char buf[512], *cmd_p;

    asprintf(&cmd_p, "curl -s \"freegeoip.net/json/%s\" 2>&1", conn->remote_ip);
    //asprintf(&cmd_p, "curl -s \"freegeoip.net/json/::ffff:103.26.16.225\" 2>&1");
    cprintf(conn, "GEOLOC: <%s>\n", cmd_p);
    n = non_blocking_cmd(cmd_p, buf, sizeof(buf), &stat);
    free(cmd_p);
    if (stat < 0 || WEXITSTATUS(stat) != 0 || n <= 0) {
        clprintf(conn, "GEOLOC: failed for %s\n", conn->remote_ip);
        return;
    }
    cprintf(conn, "GEOLOC: returned <%s>\n", buf);

	cfg_t cfg_geo;
    json_init(&cfg_geo, buf);
    char *country_code = (char *) json_string(&cfg_geo, "country_code", NULL, CFG_OPTIONAL);
    char *country_name = (char *) json_string(&cfg_geo, "country_name", NULL, CFG_OPTIONAL);
    char *region_name = (char *) json_string(&cfg_geo, "region_name", NULL, CFG_OPTIONAL);
    char *city = (char *) json_string(&cfg_geo, "city", NULL, CFG_OPTIONAL);
    
    char *country;
	if (country_code && strcmp(country_code, "US") == 0 && region_name && *region_name) {
		country = kstr_cat(region_name, ", USA");
	} else {
		country = kstr_cat(country_name, NULL);
	}

	char *geo = NULL;
	bool has_city = (city && *city);
	if (has_city)
		geo = kstr_cat(geo, city);
    geo = kstr_cat(geo, has_city? ", " : "");
    geo = kstr_cat(geo, country);   // NB: country freed here

    clprintf(conn, "GEOLOC: %s <%s>\n", conn->remote_ip, kstr_sp(geo));
    conn->geo = strdup(kstr_sp(geo));
    kstr_free(geo);

    json_string_free(&cfg_geo, country_code);
    json_string_free(&cfg_geo, country_name);
    json_string_free(&cfg_geo, region_name);
    json_string_free(&cfg_geo, city);
}

int current_nusers;
static int last_hour = -1, last_min = -1;

// called periodically
void webserver_collect_print_stats(int print)
{
	int i, nusers=0;
	conn_t *c;
	
	// print / log connections
	for (c=conns; c < &conns[N_CONNS]; c++) {
		if (!(c->valid && c->type == STREAM_SOUND && c->arrived)) continue;
		
		u4_t now = timer_sec();
		if (c->freqHz != c->last_freqHz || c->mode != c->last_mode || c->zoom != c->last_zoom) {
			if (print) loguser(c, LOG_UPDATE);
			c->last_tune_time = now;
		} else {
			u4_t diff = now - c->last_log_time;
			if (diff > MINUTES_TO_SEC(5)) {
				if (print) loguser(c, LOG_UPDATE_NC);
			}
			
			if (!c->inactivity_timeout_override && (inactivity_timeout_mins != 0) && !c->isLocal) {
				diff = now - c->last_tune_time;
				if (diff > MINUTES_TO_SEC(inactivity_timeout_mins) && !c->inactivity_msg_sent) {
					send_msg(c, false, "MSG inactivity_timeout_msg=%d", inactivity_timeout_mins);
					c->inactivity_msg_sent = true;
				}
				if (diff > (MINUTES_TO_SEC(inactivity_timeout_mins) + INACTIVITY_WARNING_SECS)) {
					c->inactivity_timeout = true;
				}
			}
		}
		
		if (!c->geo && !c->try_geoloc && (now - c->arrival) > 10) {
		    clprintf(c, "GEOLOC: %s sent no geoloc info, trying from here\n", c->remote_ip);
		    CreateTask(geoloc_task, (void *) c, WEBSERVER_PRIORITY);
		    c->try_geoloc = true;
		}
		
		nusers++;
	}
	current_nusers = nusers;

	// construct cpu stats response
	int n, user, sys, idle;
	static int last_user, last_sys, last_idle;
	user = sys = 0;
	u4_t now = timer_ms();
	static u4_t last_now;
	float secs = (float)(now - last_now) / 1000;
	last_now = now;
	
	float del_user = 0;
	float del_sys = 0;
	float del_idle = 0;
	
	char buf[256];
	n = non_blocking_cmd("cat /proc/stat", buf, sizeof(buf), NULL);
	if (n > 0) {
		n = sscanf(buf, "cpu %d %*d %d %d", &user, &sys, &idle);
		//long clk_tick = sysconf(_SC_CLK_TCK);
		del_user = (float)(user - last_user) / secs;
		del_sys = (float)(sys - last_sys) / secs;
		del_idle = (float)(idle - last_idle) / secs;
		//printf("CPU %.1fs u=%.1f%% s=%.1f%% i=%.1f%%\n", secs, del_user, del_sys, del_idle);
		
		// ecpu_use() below can thread block, so cpu_stats_buf must be properly set NULL for reading thread
		if (cpu_stats_buf) {
			char *s = cpu_stats_buf;
			cpu_stats_buf = NULL;
			free(s);
		}
		asprintf(&cpu_stats_buf, "\"ct\":%d,\"cu\":%.0f,\"cs\":%.0f,\"ci\":%.0f,\"ce\":%.0f",
			timer_sec(), del_user, del_sys, del_idle, ecpu_use());
		last_user = user;
		last_sys = sys;
		last_idle = idle;
	}

	// collect network i/o stats
	static const float k = 1.0/1000.0/10.0;		// kbytes/sec every 10 secs
	audio_kbps = audio_bytes*k;
	waterfall_kbps = waterfall_bytes*k;
	
	for (i=0; i <= RX_CHANS; i++) {
		waterfall_fps[i] = waterfall_frames[i]/10.0;
		waterfall_frames[i] = 0;
	}
	http_kbps = http_bytes*k;
	audio_bytes = waterfall_bytes = http_bytes = 0;

	// on the hour: report number of connected users & schedule updates
	time_t t;
	time(&t);
	struct tm tm;
	localtime_r(&t, &tm);
	
	if (tm.tm_hour != last_hour) {
		if (print) lprintf("(%d %s)\n", nusers, (nusers==1)? "user":"users");
		last_hour = tm.tm_hour;
	}

	if (tm.tm_min != last_min) {
		schedule_update(tm.tm_hour, tm.tm_min);
		last_min = tm.tm_min;
	}
}

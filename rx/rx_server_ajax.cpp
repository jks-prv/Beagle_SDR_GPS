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

// Copyright (c) 2014-2017 John Seamons, ZL/KF6VO

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

// process non-websocket connections
char *rx_server_ajax(struct mg_connection *mc)
{
	int i, j, n;
	char *sb, *sb2;
	stream_t *st;
	
	for (st=streams; st->uri; st++) {
		if (strcmp(mc->uri, st->uri) == 0)
			break;
	}

	if (!st->uri) return NULL;

	// these are okay to process while were down or updating
	if ((down || update_in_progress || backup_in_progress)
		&& st->type != AJAX_VERSION
		&& st->type != AJAX_STATUS
		&& st->type != AJAX_DISCOVERY
		&& st->type != AJAX_DUMP
		)
			return NULL;

	//printf("rx_server_ajax: uri=<%s> qs=<%s>\n", mc->uri, mc->query_string);
	
	// these don't require a query string
	if (mc->query_string == NULL
		&& st->type != AJAX_VERSION
		&& st->type != AJAX_STATUS
		&& st->type != AJAX_DISCOVERY
		&& st->type != AJAX_PHOTO
		) {
		lprintf("rx_server_ajax: missing query string! uri=<%s>\n", mc->uri);
		return NULL;
	}
	
	switch (st->type) {
	
	// SECURITY:
	//	Returns JSON
	//	Done as an AJAX because needed for .js file version checking long before any websocket available
	case AJAX_VERSION:
		asprintf(&sb, "{\"maj\":%d,\"min\":%d}", VERSION_MAJ, VERSION_MIN);
		break;

	// SECURITY:
	//	Okay, requires a matching auth key generated from a previously authenticated admin web socket connection
	//	MITM vulnerable
	//	Returns JSON
	case AJAX_PHOTO: {
		char vname[64], fname[64];		// mg_parse_multipart() checks size of these
		const char *data;
		int data_len, rc = 0;
		
		printf("PHOTO UPLOAD REQUESTED from %s len=%d\n", mc->remote_ip, mc->content_len);
		//printf("PHOTO UPLOAD REQUESTED key=%s ckey=%s\n", mc->query_string, current_authkey);
		
		int key_cmp = -1;
		if (mc->query_string && current_authkey) {
			key_cmp = strcmp(mc->query_string, current_authkey);
			free(current_authkey);
			current_authkey = NULL;
		}
		if (key_cmp != 0)
			rc = 1;
		
		mg_parse_multipart(mc->content, mc->content_len,
			vname, sizeof(vname), fname, sizeof(fname), &data, &data_len);
		
		if (data_len < PHOTO_UPLOAD_MAX_SIZE) {
			FILE *fp;
			scallz("fopen photo", (fp = fopen(DIR_CFG "/photo.upload.tmp", "w")));
			scall("fwrite photo", (n = fwrite(data, 1, data_len, fp)));
			fclose(fp);
			
			// do some server-side checking
			char reply[256];
			int status;
			n = non_blocking_cmd("file " DIR_CFG "/photo.upload.tmp" , reply, sizeof(reply), &status);
			if (n > 0) {
				if (strstr(reply, "image data") == 0)
					rc = 2;
			} else {
				rc = 3;
			}
		} else {
			rc = 4;
		}
		
		// only clobber the old file if the checks pass
		if (rc == 0)
			system("mv " DIR_CFG "/photo.upload.tmp " DIR_CFG "/photo.upload");
		
		printf("AJAX_PHOTO: data=%p data_len=%d \"%s\" rc=%d\n", data, data_len, fname, rc);
		asprintf(&sb, "{\"r\":%d}", rc);
		break;
	}

	// SECURITY:
	//	FIXME: restrict delivery to the local network?
	//	Used by kiwisdr.com/scan -- the KiwiSDR auto-discovery scanner
	case AJAX_DISCOVERY:
		asprintf(&sb, "%d %s %s %d %d %s",
			ddns.serno, ddns.ip_pub, ddns.ip_pvt, ddns.port, ddns.nm_bits, ddns.mac);
		printf("DISCOVERY REQUESTED from %s: <%s>\n", mc->remote_ip, sb);
		break;

	// SECURITY:
	//	OKAY, used by sdr.hu and Priyom Pavlova at the moment
	//	Returns '\n' delimited keyword=value pairs
	case AJAX_STATUS: {
	
		int sdr_hu_reg = (admcfg_bool("sdr_hu_register", NULL, CFG_OPTIONAL) == 1)? 1:0;
		static int reg_silent_period_run, reg_silent_period_end;
		
		// If registration off, create a silence period so sdr.hu de-lists our entry.
		// Can't simply ignore the first /status query to arrive because it's not guaranteed
		// to be from sdr.hu
		if (!sdr_hu_reg) {
			if (reg_silent_period_run == 0) {
				reg_silent_period_run = 1;
				#define SDR_HU_POLL_PERIOD_MIN 3
				reg_silent_period_end = timer_sec() + MINUTES_TO_SEC(SDR_HU_POLL_PERIOD_MIN) + 5;
				//printf("-------- silent period start\n");
				sb = NULL;
				break;
			}
			if (timer_sec() < reg_silent_period_end) {
				//printf("-------- silent period\n");
				sb = NULL;
				break;
			}
			//printf("-------- /status OKAY\n");
		} else {
			reg_silent_period_run = 0;
		}
		
		static time_t avatar_ctime;
		// the avatar file is in the in-memory store, so it's not going to be changing after server start
		if (avatar_ctime == 0) time(&avatar_ctime);
		const char *s1, *s2, *s3, *s4, *s5, *s6;
		
		// if location hasn't been changed from the default try using DDNS lat/log
		// or, failing that, put us in Antarctica to be noticed
		s4 = cfg_string("rx_gps", NULL, CFG_OPTIONAL);
		const char *gps_loc;
		bool gps_default = false;
		char *ddns_lat_lon = NULL;
		if (strcmp(s4, "(-37.631120, 176.172210)") == 0) {
			if (ddns.lat_lon_valid) {
				asprintf(&ddns_lat_lon, "(%lf, %lf)", ddns.lat, ddns.lon);
				gps_loc = ddns_lat_lon;
			} else {
				gps_loc = "(-69.0, 90.0)";		// Antarctica
				gps_default = true;
			}
		} else {
			gps_loc = s4;
		}
		
		// append location to name if none of the keywords in location appear in name
		s1 = cfg_string("rx_name", NULL, CFG_OPTIONAL);
		char *name;
		name = strdup(s1);
		cfg_string_free(s1);

		s5 = cfg_string("rx_location", NULL, CFG_OPTIONAL);
		if (name && s5) {
		
			// hack to include location description in name
			#define NKWDS 8
			char *kwds[NKWDS], *loc;
			loc = strdup(s5);
			n = kiwi_split((char *) loc, ",;-:/()[]{}<>| \t\n", kwds, NKWDS);
			for (i=0; i < n; i++) {
				//printf("KW%d: <%s>\n", i, kwds[i]);
				if (strcasestr(name, kwds[i]))
					break;
			}
			free(loc);
			if (i == n) {
				char *name2;
				asprintf(&name2, "%s | %s", name, s5);
				free(name);
				name = name2;
				//printf("KW <%s>\n", name);
			}
		}
		
		s2 = cfg_string("rx_device", NULL, CFG_OPTIONAL);
		char *cp;
		if ((cp = strstr((char *) s2, " on BeagleBone")) != NULL)
			*cp = '\0';
		
		// if this Kiwi doesn't have any open access (no password required)
		// prevent it from being listed on sdr.hu
		const char *pwd_s = admcfg_string("user_password", NULL, CFG_REQUIRED);
		int chan_no_pwd = cfg_int("chan_no_pwd", NULL, CFG_REQUIRED);
		bool no_open_access = (*pwd_s != '\0' && chan_no_pwd == 0);
		//printf("STATUS user_pwd=%d chan_no_pwd=%d no_open_access=%d\n", *pwd_s != '\0', chan_no_pwd, no_open_access);

		asprintf(&sb, "status=active\nname=%s\nsdr_hw=%s v%d.%d%s\nop_email=%s\nbands=0-%.0f\nusers=%d\nusers_max=%d\navatar_ctime=%ld\ngps=%s\nasl=%d\nloc=%s\nsw_version=%s%d.%d\nantenna=%s\n%suptime=%d\n",
			name,
			s2, VERSION_MAJ, VERSION_MIN, gps_default? " [default location set]" : "",
			(s3 = cfg_string("admin_email", NULL, CFG_OPTIONAL)),
			ui_srate, current_nusers,
			(pwd_s != NULL && *pwd_s != '\0')? chan_no_pwd : RX_CHANS,
			avatar_ctime, gps_loc,
			cfg_int("rx_asl", NULL, CFG_OPTIONAL),
			s5,
			"KiwiSDR_v", VERSION_MAJ, VERSION_MIN,
			(s6 = cfg_string("rx_antenna", NULL, CFG_OPTIONAL)),
			no_open_access? "auth=password\n" : "",
			timer_sec()
			);

		if (name) free(name);
		if (ddns_lat_lon) free(ddns_lat_lon);
		cfg_string_free(s2);
		cfg_string_free(s3);
		cfg_string_free(s4);
		cfg_string_free(s5);
		cfg_string_free(s6);

		//printf("STATUS REQUESTED from %s: <%s>\n", mc->remote_ip, sb);
		break;
	}

	// SECURITY: FIXME: security through obscurity is weak
	case AJAX_DUMP: {
		printf("\n");
		lprintf("DUMP REQUESTED from %s\n", mc->remote_ip);
		if (strcmp(mc->query_string, "b3f5ca67159c3bfb6dc150bd1a2064f50b8367ee") != 0)
			return NULL;
		dump();
		asprintf(&sb, "--- LOG DUMP ---\n");
		log_save_t *ls = log_save_p;
		int first = MIN(ls->idx, N_LOG_SAVE/2);
		for (i = 0; i < first; i++) {
			sb = kstr_cat(sb, (char *) ls->arr[i]);
		}
		if (ls->not_shown) {
			asprintf(&sb2, "\n--- %d lines not shown ---\n\n", ls->not_shown);
			sb = kstr_cat(sb, kstr_wrap(sb2));
		}
		for (; i < ls->idx; i++) {
			sb = kstr_cat(sb, (char *) ls->arr[i]);
		}
		break;
	}

	default:
		return NULL;
		break;
	}

	//printf("AJAX: RTN <%s>\n", kstr_sp(sb));
	return sb;		// NB: sb is free()'d by caller
}

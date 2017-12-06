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
#include "clk.h"
#include "gps.h"
#include "cfg.h"
#include "coroutines.h"
#include "net.h"

#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <sched.h>
#include <math.h>
#include <signal.h>
#include <fftw3.h>
#include <stdlib.h>
#include <sys/time.h>

// process non-websocket connections
char *rx_server_ajax(struct mg_connection *mc)
{
	int i, j, n;
	char *sb, *sb2;
	stream_t *st;
	char *uri = (char *) mc->uri;
	
	if (*uri == '/') uri++;
	
	for (st=streams; st->uri; st++) {
		if (strcmp(uri, st->uri) == 0)
			break;
	}

	if (!st->uri) return NULL;

	// these are okay to process while we're down or updating
	if ((down || update_in_progress || backup_in_progress)
		&& st->type != AJAX_VERSION
		&& st->type != AJAX_STATUS
		&& st->type != AJAX_DISCOVERY
		)
			return NULL;

	//printf("rx_server_ajax: uri=<%s> qs=<%s>\n", uri, mc->query_string);
	
	// these don't require a query string
	if (mc->query_string == NULL
		&& st->type != AJAX_VERSION
		&& st->type != AJAX_STATUS
		&& st->type != AJAX_DISCOVERY
		&& st->type != AJAX_PHOTO
		) {
		lprintf("rx_server_ajax: missing query string! uri=<%s>\n", uri);
		return NULL;
	}
	
	switch (st->type) {
	
	// SECURITY:
	//	Returns JSON
	//	Done as an AJAX because needed for .js file version checking long before any websocket available
	case AJAX_VERSION:
		asprintf(&sb, "{\"maj\":%d,\"min\":%d}", version_maj, version_min);
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
		
		if (rc == 0) {
			mg_parse_multipart(mc->content, mc->content_len,
				vname, sizeof(vname), fname, sizeof(fname), &data, &data_len);
			
			if (data_len < PHOTO_UPLOAD_MAX_SIZE) {
				FILE *fp;
				scallz("fopen photo", (fp = fopen(DIR_CFG "/photo.upload.tmp", "w")));
				scall("fwrite photo", (n = fwrite(data, 1, data_len, fp)));
				fclose(fp);
				
				// do some server-side checking
				char *reply;
				int status;
				reply = non_blocking_cmd("file " DIR_CFG "/photo.upload.tmp", &status);
				if (reply != NULL) {
					if (strstr(kstr_sp(reply), "image data") == 0)
						rc = 2;
					kstr_free(reply);
				} else {
					rc = 3;
				}
			} else {
				rc = 4;
			}
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
	//	OKAY, used by sdr.hu, kiwisdr.com and Priyom Pavlova at the moment
	//	Returns '\n' delimited keyword=value pairs
	case AJAX_STATUS: {
	
		// determine real client ip if proxied
		char remote_ip[NET_ADDRSTRLEN];
		check_if_forwarded(sdr_hu_debug? "/status" : NULL, mc, remote_ip);
		
		int sdr_hu_reg = (admcfg_bool("sdr_hu_register", NULL, CFG_OPTIONAL) == 1)? 1:0;
		
		// If sdr.hu registration is off then don't reply to sdr.hu, but reply to others.
		// But don't reply to anyone until ddns.ips_sdr_hu is valid.
		if (!sdr_hu_reg && (!ddns.ips_sdr_hu.valid || ip_match(remote_ip, &ddns.ips_sdr_hu))) {
			if (sdr_hu_debug)
				printf("/status: sdr.hu reg disabled, not replying to sdr.hu (%s)\n", remote_ip);
			return (char *) "NO-REPLY";
		}
		if (sdr_hu_debug)
			printf("/status: replying to %s\n", remote_ip);
		
		const char *s1, *s3, *s4, *s5, *s6;
		
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
			char *kwds[NKWDS], *loc, *r_loc;
			loc = strdup(s5);
			n = kiwi_split((char *) loc, &r_loc, ",;-:/()[]{}<>| \t\n", kwds, NKWDS);
			for (i=0; i < n; i++) {
				//printf("KW%d: <%s>\n", i, kwds[i]);
				if (strcasestr(name, kwds[i]))
					break;
			}
			free(loc); free(r_loc);
			if (i == n) {
				char *name2;
				asprintf(&name2, "%s | %s", name, s5);
				free(name);
				name = name2;
				//printf("KW <%s>\n", name);
			}
		}
		
		// if this Kiwi doesn't have any open access (no password required)
		// prevent it from being listed on sdr.hu
		const char *pwd_s = admcfg_string("user_password", NULL, CFG_REQUIRED);
		int chan_no_pwd = cfg_int("chan_no_pwd", NULL, CFG_REQUIRED);
		bool no_open_access = (*pwd_s != '\0' && chan_no_pwd == 0);
		//printf("STATUS user_pwd=%d chan_no_pwd=%d no_open_access=%d\n", *pwd_s != '\0', chan_no_pwd, no_open_access);

		// Advertise whether Kiwi can be publicly listed,
		// and is available for use
		const char *status;
		if (! sdr_hu_reg)
			// Make sure to always keep set to private when private
			status = "private";
		else if (down || update_in_progress || backup_in_progress)
			status = "offline";
		else
			status = "active";

		// the avatar file is in the in-memory store, so it's not going to be changing after server start
		u4_t avatar_ctime = timer_server_build_unix_time();
		
		asprintf(&sb, "status=%s\nname=%s\nsdr_hw=KiwiSDR v%d.%d"
			"%s%s ⁣\n"
			"op_email=%s\nbands=%.0f-%.0f\nusers=%d\nusers_max=%d\n"
			"avatar_ctime=%u\ngps=%s\nasl=%d\nloc=%s\nsw_version=%s%d.%d\nantenna=%s\n%suptime=%d\n",
			status, name, version_maj, version_min,
			// "nbsp;nbsp;" can't be used here because HTML can't be sent.
			// So a Unicode "invisible separator" #x2063 surrounded by spaces gets the desired double spacing.
			(clk.adc_gps_clk_corrections > 8)? " ⁣ 📡 GPS" : "",
			have_ant_switch_ext?               " ⁣ 📶 ANT-SWITCH" : "",
			//gps_default? " [default location set]" : "",
			(s3 = cfg_string("admin_email", NULL, CFG_OPTIONAL)),
			(float) sdr_hu_lo_kHz * kHz, (float) sdr_hu_hi_kHz * kHz,
			current_nusers,
			(pwd_s != NULL && *pwd_s != '\0')? chan_no_pwd : RX_CHANS,
			avatar_ctime, gps_loc,
			cfg_int("rx_asl", NULL, CFG_OPTIONAL),
			s5,
			"KiwiSDR_v", version_maj, version_min,
			(s6 = cfg_string("rx_antenna", NULL, CFG_OPTIONAL)),
			no_open_access? "auth=password\n" : "",
			timer_sec()
			);

		if (name) free(name);
		if (ddns_lat_lon) free(ddns_lat_lon);
		cfg_string_free(s3);
		cfg_string_free(s4);
		cfg_string_free(s5);
		cfg_string_free(s6);

		//printf("STATUS REQUESTED from %s: <%s>\n", mc->remote_ip, sb);
		break;
	}

	default:
		return NULL;
		break;
	}

	//printf("AJAX: RTN <%s>\n", kstr_sp(sb));
	return sb;		// NB: sb is free()'d by caller
}

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
#include "clk.h"
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

char *cpu_stats_buf;
volatile float audio_kbps, waterfall_kbps, waterfall_fps[RX_CHANS+1], http_kbps;
volatile int audio_bytes, waterfall_bytes, waterfall_frames[RX_CHANS+1], http_bytes;
char *current_authkey;
int debug_v;

//#define FORCE_ADMIN_PWD_CHECK

bool rx_common_cmd(const char *stream_name, conn_t *conn, char *cmd)
{
	int i, j, n;
	struct mg_connection *mc = conn->mc;
	char *sb, *sb2;
	int slen;
	char name[1024];
	
	if (mc == NULL) return false;
	
	// SECURITY: auth command here is the only one allowed before auth check below
	if (strncmp(cmd, "SET auth", 8) == 0) {
		const char *pwd_s = NULL;
		int cfg_auto_login;
		char *type_m = NULL, *pwd_m = NULL;
		
		n = sscanf(cmd, "SET auth t=%ms p=%ms", &type_m, &pwd_m);
		//cprintf(conn, "n=%d typem=%s pwd=%s\n", n, type_m, pwd_m);
		if ((n != 1 && n != 2) || type_m == NULL) {
			send_msg(conn, false, "MSG badp=1");
			return true;
		}
		
		str_decode_inplace(pwd_m);
		//printf("PWD %s pwd %d \"%s\" from %s\n", type_m, slen, pwd_m, mc->remote_ip);
		
		bool allow = false, cant_determine = false;
		bool is_kiwi = (type_m != NULL && strcmp(type_m, "kiwi") == 0);
		bool is_admin = (type_m != NULL && strcmp(type_m, "admin") == 0);
		
		bool bad_type = (conn->type != STREAM_SOUND && conn->type != STREAM_WATERFALL && conn->type != STREAM_EXT &&
			conn->type != STREAM_ADMIN && conn->type != STREAM_MFG);
		
		if ((!is_kiwi && !is_admin) || bad_type) {
			clprintf(conn, "PWD BAD REQ type_m=\"%s\" conn_type=%d from %s\n", type_m, conn->type, mc->remote_ip);
			send_msg(conn, false, "MSG badp=1");
			return true;
		}
		
		bool log_auth_attempt = (conn->type == STREAM_ADMIN || conn->type == STREAM_MFG || (conn->type == STREAM_EXT && is_admin));
		isLocal_t isLocal = isLocal_IP(conn, log_auth_attempt);
		bool is_local = (isLocal == IS_LOCAL);

		#ifdef FORCE_ADMIN_PWD_CHECK
			is_local = false;
		#endif
		
		//cprintf(conn, "PWD %s log_auth_attempt %d conn_type %d [%s] isLocal %d is_local %d from %s\n",
		//	type_m, log_auth_attempt, conn->type, streams[conn->type].uri, isLocal, is_local, mc->remote_ip);
		
		int chan_no_pwd = cfg_int("chan_no_pwd", NULL, CFG_REQUIRED);
		int chan_need_pwd = RX_CHANS - chan_no_pwd;

		if (is_kiwi) {
			pwd_s = admcfg_string("user_password", NULL, CFG_REQUIRED);
			bool no_pwd = (pwd_s == NULL || *pwd_s == '\0');
			cfg_auto_login = admcfg_bool("user_auto_login", NULL, CFG_REQUIRED);
			
			// if no user password set allow unrestricted connection
			if (no_pwd) {
				cprintf(conn, "PWD kiwi: no config pwd set, allow any\n");
				allow = true;
			} else
			
			// config pwd set, but auto_login for local subnet is true
			if (cfg_auto_login && is_local) {
				cprintf(conn, "PWD kiwi: config pwd set, but is_local and auto-login set\n");
				allow = true;
			} else {
			
				int rx_free = rx_chan_free(NULL);
				
				// allow with no password if minimum number of channels needing password remains
				// if no password has been set at all we've already allowed access above
				if (rx_free >= chan_need_pwd) {
					allow = true;
					//cprintf(conn, "PWD rx_free=%d >= chan_need_pwd=%d %s\n", rx_free, chan_need_pwd, allow? "TRUE":"FALSE");
				}
			}
		} else
		
		if (is_admin) {
			pwd_s = admcfg_string("admin_password", NULL, CFG_REQUIRED);
			bool no_pwd = (pwd_s == NULL || *pwd_s == '\0');
			cfg_auto_login = admcfg_bool("admin_auto_login", NULL, CFG_REQUIRED);
			clprintf(conn, "PWD %s: config pwd set %s, auto-login %s\n", type_m,
				no_pwd? "FALSE":"TRUE", cfg_auto_login? "TRUE":"FALSE");

			// can't determine local network status (yet)
			if (no_pwd && isLocal == NO_LOCAL_IF) {
				clprintf(conn, "PWD %s: no local network interface information\n", type_m);
				cant_determine = true;
			} else

			// no config pwd set (e.g. initial setup) -- allow if connection is from local network
			if (no_pwd && is_local) {
				clprintf(conn, "PWD %s: no config pwd set, but is_local\n", type_m);
				allow = true;
			} else
			
			// config pwd set, but auto_login for local subnet is true
			if (cfg_auto_login && is_local) {
				clprintf(conn, "PWD %s: config pwd set, but is_local and auto-login set\n", type_m);
				allow = true;
			}
		} else {
			cprintf(conn, "PWD bad type=%s\n", type_m);
			pwd_s = NULL;
		}
		
		// FIXME: remove at some point
		#ifndef FORCE_ADMIN_PWD_CHECK
			if (!allow && (strcmp(mc->remote_ip, "103.26.16.225") == 0 || strcmp(mc->remote_ip, "::ffff:103.26.16.225") == 0)) {
				allow = true;
			}
		#endif
		
		int badp = 1;

		if (cant_determine) {
		    badp = 2;
		} else

		if (allow) {
			if (log_auth_attempt)
				clprintf(conn, "PWD %s allow override: sent from %s\n", type_m, mc->remote_ip);
			badp = 0;
		} else
		
		if ((pwd_s == NULL || *pwd_s == '\0')) {
			clprintf(conn, "PWD %s rejected: no config pwd set, sent from %s\n", type_m, mc->remote_ip);
			badp = 1;
		} else {
			if (pwd_m == NULL || pwd_s == NULL)
				badp = 1;
			else {
				//cprintf(conn, "PWD CMP %s pwd_s \"%s\" pwd_m \"%s\" from %s\n", type_m, pwd_s, pwd_m, mc->remote_ip);
				badp = strcasecmp(pwd_m, pwd_s)? 1:0;
			}
			//clprintf(conn, "PWD %s %s: sent from %s\n", type_m, badp? "rejected":"accepted", mc->remote_ip);
		}
		
		send_msg(conn, false, "MSG rx_chans=%d", RX_CHANS);
		send_msg(conn, false, "MSG chan_no_pwd=%d", chan_no_pwd);
		send_msg(conn, false, "MSG badp=%d", badp);

		if (type_m) free(type_m);
		if (pwd_m) free(pwd_m);
		cfg_string_free(pwd_s);
		
		// only when the auth validates do we setup the handler
		if (badp == 0) {
			if (is_kiwi) conn->auth_kiwi = true;
			if (is_admin) conn->auth_admin = true;

			if (conn->auth == false) {
				conn->auth = true;
				conn->isLocal = is_local;
				
				// send cfg once to javascript
				if (conn->type == STREAM_SOUND || conn->type == STREAM_ADMIN || conn->type == STREAM_MFG)
					rx_server_send_config(conn);
				
				// setup stream task first time it's authenticated
				stream_t *st = &streams[conn->type];
				if (st->setup) (st->setup)((void *) conn);
			}
		}
		
		return true;
	}

	// SECURITY: we accept no incoming command besides auth above until auth is successful
	if (conn->auth == false) {
		clprintf(conn, "### SECURITY: NO AUTH YET: %s %d %s <%s>\n", stream_name, conn->type, mc->remote_ip, cmd);
		return true;	// fake that we accepted command so it won't be further processed
	}

	if (strcmp(cmd, "SET get_authkey") == 0) {
		if (current_authkey)
			free(current_authkey);
		current_authkey = kiwi_authkey();
		send_msg(conn, false, "MSG authkey_cb=%s", current_authkey);
		return true;
	}

	if (strcmp(cmd, "SET keepalive") == 0) {
		conn->keepalive_count++;
		return true;
	}

	n = strncmp(cmd, "SET save_cfg=", 13);
	if (n == 0) {
		if (conn->type != STREAM_ADMIN) {
			lprintf("** attempt to save kiwi config from non-STREAM_ADMIN! IP %s\n", mc->remote_ip);
			return true;	// fake that we accepted command so it won't be further processed
		}
	
		char *json = cfg_realloc_json(strlen(cmd), CFG_NONE);	// a little bigger than necessary
		n = sscanf(cmd, "SET save_cfg=%s", json);
		assert(n == 1);
		//printf("SET save_cfg=...\n");
		str_decode_inplace(json);
		cfg_save_json(json);
		update_vars_from_config();      // update C copies of vars

		return true;
	}

	n = strncmp(cmd, "SET save_adm=", 13);
	if (n == 0) {
		if (conn->type != STREAM_ADMIN) {
			lprintf("** attempt to save admin config from non-STREAM_ADMIN! IP %s\n", mc->remote_ip);
			return true;	// fake that we accepted command so it won't be further processed
		}
	
		char *json = admcfg_realloc_json(strlen(cmd), CFG_NONE);	// a little bigger than necessary
		n = sscanf(cmd, "SET save_adm=%s", json);
		assert(n == 1);
		//printf("SET save_adm=...\n");
		str_decode_inplace(json);
		admcfg_save_json(json);
		//update_vars_from_config();    // no admin vars need to be updated on save currently
		
		return true;
	}

	if (strcmp(cmd, "SET GET_USERS") == 0) {
		rx_chan_t *rx;
		bool need_comma = false;
		sb = kstr_cat(NULL, "[");
		bool isAdmin = (conn->type == STREAM_ADMIN);
		
		for (rx = rx_channels, i=0; rx < &rx_channels[RX_CHANS]; rx++, i++) {
			n = 0;
			if (rx->busy) {
				conn_t *c = rx->conn_snd;
				if (c && c->valid && c->arrived && c->user != NULL) {
					assert(c->type == STREAM_SOUND);
					u4_t now = timer_sec();
					u4_t t = now - c->arrival;
					u4_t sec = t % 60; t /= 60;
					u4_t min = t % 60; t /= 60;
					u4_t hr = t;
					char *user = c->isUserIP? NULL : str_encode(c->user);
					char *geo = c->geo? str_encode(c->geo) : NULL;
					char *ext = ext_users[i].ext? str_encode((char *) ext_users[i].ext->name) : NULL;
					const char *ip = isAdmin? c->remote_ip : "";
					asprintf(&sb2, "%s{\"i\":%d,\"n\":\"%s\",\"g\":\"%s\",\"f\":%d,\"m\":\"%s\",\"z\":%d,\"t\":\"%d:%02d:%02d\",\"e\":\"%s\",\"a\":\"%s\"}",
						need_comma? ",":"", i, user? user:"", geo? geo:"", c->freqHz,
						enum2str(c->mode, mode_s, ARRAY_LEN(mode_s)), c->zoom, hr, min, sec, ext? ext:"", ip);
					if (user) free(user);
					if (geo) free(geo);
					if (ext) free(ext);
					n = 1;
				}
			}
			if (n == 0) {
				asprintf(&sb2, "%s{\"i\":%d}", need_comma? ",":"", i);
			}
			sb = kstr_cat(sb, kstr_wrap(sb2));
			need_comma = true;
		}

		sb = kstr_cat(sb, "]");
		send_msg(conn, false, "MSG user_cb=%s", kstr_sp(sb));
		kstr_free(sb);
		return true;
	}

#define DX_SPACING_ZOOM_THRESHOLD	5
#define DX_SPACING_THRESHOLD_PX		10
		dx_t *dp, *ldp, *upd;

	// SECURITY: should be okay: checks for conn->auth_admin first
	if (strncmp(cmd, "SET DX_UPD", 10) == 0) {
		if (conn->auth_admin == false) {
			cprintf(conn, "DX_UPD NO AUTH %s\n", conn->mc->remote_ip);
			return true;
		}
		
		if (dx.len == 0) {
			return true;
		}
		
		float freq;
		int gid, mkr_off, flags, new_len;
		flags = 0;
		char *text_m, *notes_m;
		text_m = notes_m = NULL;
		n = sscanf(cmd, "SET DX_UPD g=%d f=%f o=%d m=%d i=%ms n=%ms", &gid, &freq, &mkr_off, &flags, &text_m, &notes_m);
		//printf("DX_UPD #%d %8.2f 0x%x text=<%s> notes=<%s>\n", gid, freq, flags, text_m, notes_m);

		if (n != 2 && n != 6) {
			printf("DX_UPD n=%d\n", n);
			return true;
		}
		
		dx_t *dxp;
		if (gid >= -1 && gid < dx.len) {
			if (gid != -1 && freq == -1) {
				// delete entry by forcing to top of list, then decreasing size by one before save
				cprintf(conn, "DX_UPD %s delete entry #%d\n", conn->mc->remote_ip, gid);
				dxp = &dx.list[gid];
				dxp->freq = 999999;
				new_len = dx.len - 1;
			} else {
				if (gid == -1) {
					// new entry: add to end of list (in hidden slot), then sort will insert it properly
					cprintf(conn, "DX_UPD %s adding new entry\n", conn->mc->remote_ip);
					assert(dx.hidden_used == false);		// FIXME need better serialization
					dxp = &dx.list[dx.len];
					dx.hidden_used = true;
					dx.len++;
					new_len = dx.len;
				} else {
					// modify entry
					cprintf(conn, "DX_UPD %s modify entry #%d\n", conn->mc->remote_ip, gid);
					dxp = &dx.list[gid];
					new_len = dx.len;
				}
				dxp->freq = freq;
				dxp->offset = mkr_off;
				dxp->flags = flags;
				
				// remove trailing 'x' transmitted with text and notes fields
				text_m[strlen(text_m)-1] = '\0';
				notes_m[strlen(notes_m)-1] = '\0';
				
				// can't use kiwi_strdup because free() must be used later on
				dxp->ident = strdup(text_m);
				free(text_m);
				dxp->notes = strdup(notes_m);
				free(notes_m);
			}
		} else {
			printf("DX_UPD: gid %d >= dx.len %d ?\n", gid, dx.len);
		}
		
		qsort(dx.list, dx.len, sizeof(dx_t), qsort_floatcomp);
		//printf("DX_UPD after qsort dx.len %d new_len %d top elem f=%.2f\n",
		//	dx.len, new_len, dx.list[dx.len-1].freq);
		dx.len = new_len;
		dx_save_as_json();		// FIXME need better serialization
		dx_reload();
		send_msg(conn, false, "MSG request_dx_update");	// get client to request updated dx list
		return true;
	}

	if (strncmp(cmd, "SET MKR", 7) == 0) {
		float min, max;
		int zoom, width;
		n = sscanf(cmd, "SET MKR min=%f max=%f zoom=%d width=%d", &min, &max, &zoom, &width);
		if (n != 4) return true;
		float bw;
		bw = max - min;
		static bool first = true;
		static int dx_lastx;
		dx_lastx = 0;
		time_t t; time(&t);
		
		if (dx.len == 0) {
			return true;
		}
		
		asprintf(&sb, "[{\"t\":%ld}", t);		// reset appending
		sb = kstr_wrap(sb);

		for (dp = dx.list, i=j=0; i < dx.len; dp++, i++) {
			float freq = dp->freq + (dp->offset / 1000.0);		// carrier plus offset

			// when zoomed far-in need to look at wider window since we don't know PB center here
			#define DX_SEARCH_WINDOW 10.0
			if (freq < min - DX_SEARCH_WINDOW) continue;
			if (freq > max + DX_SEARCH_WINDOW) break;
			
			// reduce dx label clutter
			if (zoom <= DX_SPACING_ZOOM_THRESHOLD) {
				int x = ((dp->freq - min) / bw) * width;
				int diff = x - dx_lastx;
				//printf("DX spacing %d %d %d %s\n", dx_lastx, x, diff, dp->ident);
				if (!first && diff < DX_SPACING_THRESHOLD_PX) continue;
				dx_lastx = x;
				first = false;
			}
			
			// NB: ident and notes are already stored URL encoded
			float f = dp->freq + (dp->offset / 1000.0);
			asprintf(&sb2, ",{\"g\":%d,\"f\":%.3f,\"o\":%.0f,\"b\":%d,\"i\":\"%s\"%s%s%s}",
				i, freq, dp->offset, dp->flags, dp->ident,
				dp->notes? ",\"n\":\"":"", dp->notes? dp->notes:"", dp->notes? "\"":"");
			//printf("dx(%d,%.3f,%.0f,%d,\'%s\'%s%s%s)\n", i, f, dp->offset, dp->flags, dp->ident,
			//	dp->notes? ",\'":"", dp->notes? dp->notes:"", dp->notes? "\'":"");
			sb = kstr_cat(sb, kstr_wrap(sb2));
		}
		
		sb = kstr_cat(sb, "]");
		send_msg(conn, false, "MSG mkr=%s", kstr_sp(sb));
		kstr_free(sb);
		return true;
	}

	if (strcmp(cmd, "SET GET_CONFIG") == 0) {
		asprintf(&sb, "{\"r\":%d,\"g\":%d,\"s\":%d,\"pu\":\"%s\",\"pe\":%d,\"pv\":\"%s\",\"pi\":%d,\"n\":%d,\"m\":\"%s\",\"v1\":%d,\"v2\":%d}",
			RX_CHANS, GPS_CHANS, ddns.serno, ddns.ip_pub, ddns.port_ext, ddns.ip_pvt, ddns.port, ddns.nm_bits, ddns.mac, version_maj, version_min);
		send_msg(conn, false, "MSG config_cb=%s", sb);
		free(sb);
		return true;
	}
	
	if (strncmp(cmd, "SET STATS_UPD", 13) == 0) {
		int ch;
		n = sscanf(cmd, "SET STATS_UPD ch=%d", &ch);
		if (n != 1 || ch < 0 || ch > RX_CHANS) return true;

		rx_chan_t *rx;
		int underruns = 0, seq_errors = 0;
		n = 0;
		//n = snprintf(oc, rem, "{\"a\":["); oc += n; rem -= n;
		
		for (rx = rx_channels, i=0; rx < &rx_channels[RX_CHANS]; rx++, i++) {
			if (rx->busy) {
				conn_t *c = rx->conn_snd;
				if (c && c->valid && c->arrived && c->user != NULL) {
					underruns += c->audio_underrun;
					seq_errors += c->sequence_errors;
				}
			}
		}
		
		if (cpu_stats_buf != NULL) {
			asprintf(&sb, "{%s", cpu_stats_buf);
		} else {
			asprintf(&sb, "");
		}
		sb = kstr_wrap(sb);

		float sum_kbps = audio_kbps + waterfall_kbps + http_kbps;
		asprintf(&sb2, ",\"aa\":%.0f,\"aw\":%.0f,\"af\":%.0f,\"at\":%.0f,\"ah\":%.0f,\"as\":%.0f",
			audio_kbps, waterfall_kbps, waterfall_fps[ch], waterfall_fps[RX_CHANS], http_kbps, sum_kbps);
		sb = kstr_cat(sb, kstr_wrap(sb2));

		asprintf(&sb2, ",\"ga\":%d,\"gt\":%d,\"gg\":%d,\"gf\":%d,\"gc\":%.6f,\"go\":%d",
			gps.acquiring, gps.tracking, gps.good, gps.fixes, clk.adc_clock_system/1000000, clk.adc_clk_corrections);
		sb = kstr_cat(sb, kstr_wrap(sb2));

		extern int audio_dropped;
		asprintf(&sb2, ",\"ad\":%d,\"au\":%d,\"ae\":%d,\"ar\":%d,\"an\":%d,\"ap\":[",
			audio_dropped, underruns, seq_errors, dpump_resets, NRX_BUFS);
		sb = kstr_cat(sb, kstr_wrap(sb2));
		for (i = 0; i < NRX_BUFS; i++) {
		    asprintf(&sb2, "%s%d", (i != 0)? ",":"", dpump_hist[i]);
		    sb = kstr_cat(sb, kstr_wrap(sb2));
		}
        sb = kstr_cat(sb, "]");

		char *s, utc_s[32], local_s[32];
		time_t utc; time(&utc);
		s = asctime(gmtime(&utc));
		strncpy(utc_s, &s[11], 5);
		utc_s[5] = '\0';
		if (utc_offset != -1 && dst_offset != -1) {
			time_t local = utc + utc_offset + dst_offset;
			s = asctime(gmtime(&local));
			strncpy(local_s, &s[11], 5);
			local_s[5] = '\0';
		} else {
			strcpy(local_s, "");
		}
		asprintf(&sb2, ",\"tu\":\"%s\",\"tl\":\"%s\",\"ti\":\"%s\",\"tn\":\"%s\"",
			utc_s, local_s, tzone_id, tzone_name);
		sb = kstr_cat(sb, kstr_wrap(sb2));

		asprintf(&sb2, "}");
		sb = kstr_cat(sb, kstr_wrap(sb2));

		send_msg(conn, false, "MSG stats_cb=%s", kstr_sp(sb));
		kstr_free(sb);
		return true;
	}

	n = strcmp(cmd, "SET gps_update");
	if (n == 0) {
		gps_stats_t::gps_chan_t *c;
		
		asprintf(&sb, "{\"FFTch\":%d,\"ch\":[", gps.FFTch);
		sb = kstr_wrap(sb);
		
		for (i=0; i < gps_chans; i++) {
			c = &gps.ch[i];
			int un = c->ca_unlocked;
			asprintf(&sb2, "%s{ \"ch\":%d,\"prn\":%d,\"snr\":%d,\"rssi\":%d,\"gain\":%d,\"hold\":%d,\"wdog\":%d"
				",\"unlock\":%d,\"parity\":%d,\"sub\":%d,\"sub_renew\":%d,\"novfl\":%d}",
				i? ", ":"", i, c->prn, c->snr, c->rssi, c->gain, c->hold, c->wdog,
				un, c->parity, c->sub, c->sub_renew, c->novfl);
			sb = kstr_cat(sb, kstr_wrap(sb2));
			c->parity = 0;
			for (j = 0; j < SUBFRAMES; j++) {
				if (c->sub_renew & (1<<j)) {
					c->sub |= 1<<j;
					c->sub_renew &= ~(1<<j);
				}
			}
		}

		UMS hms(gps.StatSec/60/60);
		
		unsigned r = (timer_ms() - gps.start)/1000;
		if (r >= 3600) {
			asprintf(&sb2, "],\"run\":\"%d:%02d:%02d\"", r / 3600, (r / 60) % 60, r % 60);
		} else {
			asprintf(&sb2, "],\"run\":\"%d:%02d\"", (r / 60) % 60, r % 60);
		}
		sb = kstr_cat(sb, kstr_wrap(sb2));

		if (gps.ttff) {
			asprintf(&sb2, ",\"ttff\":\"%d:%02d\"", gps.ttff / 60, gps.ttff % 60);
		} else {
			asprintf(&sb2, ",\"ttff\":null");
		}
		sb = kstr_cat(sb, kstr_wrap(sb2));

		if (gps.StatDay != -1) {
			asprintf(&sb2, ",\"gpstime\":\"%s %02d:%02d:%02.0f\"", Week[gps.StatDay], hms.u, hms.m, hms.s);
		} else {
			asprintf(&sb2, ",\"gpstime\":null");
		}
		sb = kstr_cat(sb, kstr_wrap(sb2));

		if (gps.tLS_valid) {
			asprintf(&sb2, ",\"utc_offset\":\"%+d sec\"", gps.delta_tLS);
		} else {
			asprintf(&sb2, ",\"utc_offset\":null");
		}
		sb = kstr_cat(sb, kstr_wrap(sb2));

		if (gps.StatLat) {
			asprintf(&sb2, ",\"lat\":\"%8.6f %c\"", gps.StatLat, gps.StatNS);
			sb = kstr_cat(sb, kstr_wrap(sb2));
			asprintf(&sb2, ",\"lon\":\"%8.6f %c\"", gps.StatLon, gps.StatEW);
			sb = kstr_cat(sb, kstr_wrap(sb2));
			asprintf(&sb2, ",\"alt\":\"%1.0f m\"", gps.StatAlt);
			sb = kstr_cat(sb, kstr_wrap(sb2));
			asprintf(&sb2, ",\"map\":\"<a href='http://wikimapia.org/#lang=en&lat=%8.6f&lon=%8.6f&z=18&m=b' target='_blank'>wikimapia.org</a>\"",
				gps.sgnLat, gps.sgnLon);
		} else {
			asprintf(&sb2, ",\"lat\":null");
		}
		sb = kstr_cat(sb, kstr_wrap(sb2));
			
		asprintf(&sb2, ",\"acq\":%d,\"track\":%d,\"good\":%d,\"fixes\":%d,\"adc_clk\":%.6f,\"adc_corr\":%d}",
			gps.acquiring? 1:0, gps.tracking, gps.good, gps.fixes, clk.adc_clock_system/1e6, clk.adc_clk_corrections);
		sb = kstr_cat(sb, kstr_wrap(sb2));

		send_msg_encoded(conn, "MSG", "gps_update_cb", "%s", kstr_sp(sb));
		kstr_free(sb);
		return true;
	}

	// SECURITY: FIXME: get rid of this?
	int wf_comp;
	n = sscanf(cmd, "SET wf_comp=%d", &wf_comp);
	if (n == 1) {
		c2s_waterfall_compression(conn->rx_channel, wf_comp? true:false);
		printf("### SET wf_comp=%d\n", wf_comp);
		return true;
	}

	// SECURITY: should be okay: checks for conn->auth_admin first
	double dc_off_I, dc_off_Q;
	n = sscanf(cmd, "SET DC_offset I=%lf Q=%lf", &dc_off_I, &dc_off_Q);
	if (n == 2) {
	#if 0
		if (conn->auth_admin == false) {
			lprintf("SET DC_offset: NO AUTH\n");
			return true;
		}
		
		DC_offset_I += dc_off_I;
		DC_offset_Q += dc_off_Q;
		printf("DC_offset: I %.4lg/%.4lg Q %.4lg/%.4lg\n", dc_off_I, DC_offset_I, dc_off_Q, DC_offset_Q);

		cfg_set_float("DC_offset_I", DC_offset_I);
		cfg_set_float("DC_offset_Q", DC_offset_Q);
		cfg_save_json(cfg_cfg.json);
		return true;
	#else
		// FIXME: Too many people are screwing themselves by pushing the button without understanding what it does
		// and then complaining that there is carrier leak in AM mode. So disable this for now.
		return true;
	#endif
	}
	
	if (strncmp(cmd, "SET pref_export", 15) == 0) {
		if (conn->pref_id) free(conn->pref_id);
		if (conn->pref) free(conn->pref);
		n = sscanf(cmd, "SET pref_export id=%64ms pref=%4096ms", &conn->pref_id, &conn->pref);
		if (n != 2) {
			cprintf(conn, "pref_export n=%d\n", n);
			return true;
		}
		cprintf(conn, "pref_export id=<%s> pref= %d <%s>\n",
			conn->pref_id, strlen(conn->pref), conn->pref);

		// remove prior exports from other channels
		conn_t *c;
		for (c = conns; c < &conns[N_CONNS]; c++) {
			if (c == conn) continue;
			if (c->pref_id && c->pref && strcmp(c->pref_id, conn->pref_id) == 0) {
			if (c->pref_id) { free(c->pref_id); c->pref_id = NULL; }
			if (c->pref) { free(c->pref); c->pref = NULL; }
			}
		}
		
		return true;
	}
	
	if (strncmp(cmd, "SET pref_import", 15) == 0) {
		if (conn->pref_id) free(conn->pref_id);
		n = sscanf(cmd, "SET pref_import id=%64ms", &conn->pref_id);
		if (n != 1) {
			cprintf(conn, "pref_import n=%d\n", n);
			return true;
		}
		cprintf(conn, "pref_import id=<%s>\n", conn->pref_id);

		conn_t *c;
		for (c = conns; c < &conns[N_CONNS]; c++) {
			// allow self-match if (c == conn) continue;
			if (c->pref_id && c->pref && strcmp(c->pref_id, conn->pref_id) == 0) {
				cprintf(conn, "pref_import ch%d MATCH ch%d\n", conn->rx_channel, c->rx_channel);
				send_msg(conn, false, "MSG pref_import_ch=%d pref_import=%s", c->rx_channel, c->pref);
				break;
			}
		}
		if (c == &conns[N_CONNS]) {
			cprintf(conn, "pref_import NOT FOUND\n", conn->pref_id);
			send_msg(conn, false, "MSG pref_import=null");
		}

		free(conn->pref_id); conn->pref_id = NULL;
		return true;
	}

	strcpy(name, &cmd[9]);		// can't use sscanf because name might have embedded spaces
	n = (strncmp(cmd, "SET name=", 9) == 0);
	bool noname = (strcmp(cmd, "SET name=") == 0 || strcmp(cmd, "SET name=(null)") == 0);
	if (n || noname) {
		if (conn->mc == NULL) return true;	// we've seen this
		bool setUserIP = false;
		if (noname && !conn->user) setUserIP = true;
		if (noname && conn->user && strcmp(conn->user, conn->mc->remote_ip)) setUserIP = true;
		if (setUserIP) {
			kiwi_str_redup(&conn->user, "user", conn->mc->remote_ip);
			conn->isUserIP = TRUE;
			//printf(">>> isUserIP TRUE: %s:%05d setUserIP=%d noname=%d user=%s <%s>\n",
			//	conn->mc->remote_ip, conn->mc->remote_port, setUserIP, noname, conn->user, cmd);
		}
		if (!noname) {
			str_decode_inplace(name);
			kiwi_str_redup(&conn->user, "user", name);
			conn->isUserIP = FALSE;
			//printf(">>> isUserIP FALSE: %s:%05d setUserIP=%d noname=%d user=%s <%s>\n",
			//	conn->mc->remote_ip, conn->mc->remote_port, setUserIP, noname, conn->user, cmd);
		}
		
		//clprintf(conn, "SND name: <%s>\n", cmd);
		if (!conn->arrived) {
			loguser(conn, LOG_ARRIVED);
			conn->arrived = TRUE;
		}
		return true;
	}

	n = sscanf(cmd, "SET need_status=%d", &j);
	if (n == 1) {
		if (conn->mc == NULL) return true;	// we've seen this
		char *status = (char*) cfg_string("status_msg", NULL, CFG_REQUIRED);
		send_msg_encoded(conn, "MSG", "status_msg_html", "\f%s", status);
		cfg_string_free(status);
		return true;
	}
	
	n = sscanf(cmd, "SET geo=%127s", name);
	if (n == 1) {
		str_decode_inplace(name);
		//cprintf(conn, "ch%d recv geoloc from client: %s\n", conn->rx_channel, name);
		if (conn->geo) free(conn->geo);
		conn->geo = strdup(name);
		return true;
	}

	sb = (char *) "SET geojson=";
	slen = strlen(sb);
	n = strncmp(cmd, sb, slen);
	if (n == 0) {
		// FIXME: user str_decode_inplace()
		int len = strlen(cmd) - slen;
		mg_url_decode(cmd+slen, len, name, len + SPACE_FOR_NULL, 0);
		//clprintf(conn, "SND geo: <%s>\n", name);
		return true;
	}
	
	sb = (char *) "SET browser=";
	slen = strlen(sb);
	n = strncmp(cmd, sb, slen);
	if (n == 0) {
		// FIXME: user str_decode_inplace()
		int len = strlen(cmd) - slen;
		mg_url_decode(cmd+slen, len, name, len + SPACE_FOR_NULL, 0);
		//clprintf(conn, "SND browser: <%s>\n", name);
		return true;
	}

	int inactivity_timeout;
	n = sscanf(cmd, "SET OVERRIDE inactivity_timeout=%d", &inactivity_timeout);
	if (n == 1) {
		clprintf(conn, "SET OVERRIDE inactivity_timeout=%d\n", inactivity_timeout);
		if (inactivity_timeout == 0)
			conn->inactivity_timeout_override = true;
		return true;
	}

	n = sscanf(cmd, "SET spi_delay=%d", &j);
	if (n == 1) {
		assert(j == 1 || j == -1);
		spi_delay += j;
		return true;
	}
			
	// SECURITY: only used during debugging
	n = sscanf(cmd, "SET nocache=%d", &i);
	if (n == 1) {
		web_nocache = i? true : false;
		printf("SET nocache=%d\n", web_nocache);
		return true;
	}

	// SECURITY: only used during debugging
	n = sscanf(cmd, "SET ctrace=%d", &i);
	if (n == 1) {
		web_caching_debug = i? true : false;
		printf("SET ctrace=%d\n", web_caching_debug);
		return true;
	}

	// SECURITY: only used during debugging
	n = sscanf(cmd, "SET debug_v=%d", &i);
	if (n == 1) {
		debug_v = i;
		printf("SET debug_v=%d\n", debug_v);
		return true;
	}

	// SECURITY: only used during debugging
	sb = (char *) "SET debug_msg=";
	slen = strlen(sb);
	n = strncmp(cmd, sb, slen);
	if (n == 0) {
		str_decode_inplace(cmd);
		clprintf(conn, "### DEBUG MSG: <%s>\n", &cmd[slen]);
		return true;
	}

	if (strncmp(cmd, "SERVER DE CLIENT", 16) == 0) return true;
	
	// we see these sometimes; not part of our protocol
	if (strcmp(cmd, "PING") == 0) return true;

	// we see these at the close of a connection sometimes; not part of our protocol
    #define ASCII_ETX 3
	if (strlen(cmd) == 2 && cmd[0] == ASCII_ETX) return true;

	return false;
}

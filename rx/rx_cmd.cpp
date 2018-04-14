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
#include "coroutines.h"
#include "net.h"
#include "debug.h"

#if RX_CHANS
 #include "data_pump.h"
 #include "dx.h"
#endif

#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <sched.h>
#include <math.h>
#include <limits.h>
#include <signal.h>
#include <fftw3.h>

char *cpu_stats_buf;
volatile float audio_kbps, waterfall_kbps, waterfall_fps[RX_CHANS+1], http_kbps;
volatile int audio_bytes, waterfall_bytes, waterfall_frames[RX_CHANS+1], http_bytes;
char *current_authkey;
int debug_v;
bool auth_su;
char auth_su_remote_ip[NET_ADDRSTRLEN];

const char *mode_s[N_MODE] = { "am", "amn", "usb", "lsb", "cw", "cwn", "nbfm", "iq" };
const char *modu_s[N_MODE] = { "AM", "AMN", "USB", "LSB", "CW", "CWN", "NBFM", "IQ" };

static struct dx_t *dx_list_first, *dx_list_last;

int bsearch_freqcomp(const void *key, const void *elem)
{
	dx_t *dx_key = (dx_t *) key, *dx_elem = (dx_t *) elem;
	float key_freq = dx_key->freq;
    float elem_freq = dx_elem->freq + (dx_elem->offset / 1000.0);		// carrier plus offset
    
    if (key_freq == elem_freq) return 0;
    if (key_freq < elem_freq) {
        if (dx_elem == dx_list_first) return 0;     // key < first in array so lower is first
        return -1;
    }
    
    // implicit key_freq > elem_freq
    // but because there may never be an exact match must do a range test
    dx_t *dx_elem2 = dx_elem+1;
    if (dx_elem2 < dx_list_last) {
        // there is a following array element to range check with
        float elem2_freq = dx_elem2->freq + (dx_elem2->offset / 1000.0);		// carrier plus offset
        if (key_freq < elem2_freq) return 0;    // key was between without exact match
        return 1;   // key_freq > elem2_freq -> keep looking
    }
    return 0;   // key > last in array so lower is last (degenerate case)
}

bool rx_common_cmd(const char *stream_name, conn_t *conn, char *cmd)
{
	int i, j, k, n, first;
	struct mg_connection *mc = conn->mc;
	char *sb, *sb2;
	int slen;
	
	if (mc == NULL) return false;
	
	NextTask("rx_common_cmd");      // breakup long runs of sequential commands -- sometimes happens at startup
    evLatency(EC_EVENT, EV_RX, 0, "rx_common_cmd", evprintf("%s", cmd));
	
	// SECURITY: auth command here is the only one allowed before auth check below
	if (kiwi_str_begins_with(cmd, "SET auth")) {
	
		const char *pwd_s = NULL;
		int cfg_auto_login;

		char *type_m = NULL, *pwd_m = NULL, *ipl_m = NULL;
		n = sscanf(cmd, "SET auth t=%16ms p=%256ms ipl=%256ms", &type_m, &pwd_m, &ipl_m);

		if (pwd_m != NULL && strcmp(pwd_m, "#") == 0) {
		    free(pwd_m);
		    pwd_m = NULL;       // equivalent to NULL so ipl= can be sscanf()'d properly
		}
		
		//cprintf(conn, "n=%d typem=%s pwd=%s ipl=%s <%s>\n", n, type_m, pwd_m, ipl_m, cmd);
		if ((n != 1 && n != 2 && n != 3) || type_m == NULL) {
			send_msg(conn, false, "MSG badp=1");
            free(type_m); free(pwd_m); free(ipl_m);     // free(NULL) is okay
			return true;
		}
		
		kiwi_str_decode_inplace(pwd_m);
		kiwi_str_decode_inplace(ipl_m);
		//printf("PWD %s pwd %d \"%s\" from %s\n", type_m, slen, pwd_m, conn->remote_ip);
		
		bool allow = false, cant_determine = false;
		bool type_kiwi = (type_m != NULL && strcmp(type_m, "kiwi") == 0);
		bool type_admin = (type_m != NULL && strcmp(type_m, "admin") == 0);
		
		bool stream_wf = (conn->type == STREAM_WATERFALL);
		bool stream_snd = (conn->type == STREAM_SOUND);
		bool stream_snd_or_wf = (stream_snd || stream_wf);
		bool stream_admin_or_mfg = (conn->type == STREAM_ADMIN || conn->type == STREAM_MFG);
		bool stream_ext = (conn->type == STREAM_EXT);
		
		bool bad_type = (stream_snd_or_wf || stream_ext || stream_admin_or_mfg)? false : true;
		
		if ((!type_kiwi && !type_admin) || bad_type) {
			clprintf(conn, "PWD BAD REQ type_m=\"%s\" conn_type=%d from %s\n", type_m, conn->type, conn->remote_ip);
			send_msg(conn, false, "MSG badp=1");
            free(type_m); free(pwd_m); free(ipl_m);
			return true;
		}
		
		// opened admin/mfg url, but then tried type kiwi auth!
		if (stream_admin_or_mfg && !type_admin) {
			clprintf(conn, "PWD BAD TYPE MIX type_m=\"%s\" conn_type=%d from %s\n", type_m, conn->type, conn->remote_ip);
			send_msg(conn, false, "MSG badp=1");
            free(type_m); free(pwd_m); free(ipl_m);
			return true;
		}
		
		bool check_ip_against_interfaces = true;
		isLocal_t isLocal = IS_NOT_LOCAL;
		bool is_local = false;
        bool log_auth_attempt, pwd_debug;

        #define PWD_DEBUG 0
        if (PWD_DEBUG) {
            log_auth_attempt = true;
            pwd_debug = true;
        } else {
            log_auth_attempt = (stream_admin_or_mfg || (stream_ext && type_admin));
            pwd_debug = false;
        }

		int fd;
		if (type_admin && (fd = open(DIR_CFG "/opt.admin_ip", O_RDONLY)) != 0) {
		    char admin_ip[NET_ADDRSTRLEN];
		    n = read(fd, admin_ip, NET_ADDRSTRLEN);
		    if (n >= 1) {
		        admin_ip[n-1] = '\0';
		        if (admin_ip[n-2] == '\n') admin_ip[n-2] = '\0';
		        if (isLocal_ip(admin_ip) && strcmp(ip_remote(mc), admin_ip) == 0) {
		            isLocal = IS_LOCAL;
		            is_local = true;
		        }
		        cprintf(conn, "PWD opt.admin_ip %s ip=%s\n", is_local? "TRUE":"FALSE", ip_remote(mc));
		        check_ip_against_interfaces = false;
		    }
		    close(fd);
		}
		
		if (check_ip_against_interfaces) {
            
            // SECURITY: call isLocal_if_ip() using mc->remote_ip NOT conn->remote_ip because the latter
            // could be spoofed via X-Real-IP / X-Forwarded-For to look like a local address.
            // For a non-local connection mc->remote_ip is 127.0.0.1 when the frp proxy is used
            // so it will never be considered a local connection.
            isLocal = isLocal_if_ip(conn, ip_remote(mc), (log_auth_attempt || pwd_debug)? "PWD":NULL);
            is_local = (isLocal == IS_LOCAL);
        }

		if (pwd_debug) cprintf(conn, "PWD %s conn #%d %s isLocal=%d is_local=%d auth=%d auth_kiwi=%d auth_admin=%d from %s\n",
			type_m, conn->self_idx, streams[conn->type].uri, isLocal, is_local,
			conn->auth, conn->auth_kiwi, conn->auth_admin, conn->remote_ip);
		
        if ((inactivity_timeout_mins || ip_limit_mins) && stream_snd) {
            const char *tlimit_exempt_pwd = cfg_string("tlimit_exempt_pwd", NULL, CFG_OPTIONAL);
            if (is_local) {
                conn->tlimit_exempt = true;
                cprintf(conn, "TLIMIT exempt local connection from %s\n", conn->remote_ip);
            } else
            if (ipl_m != NULL && tlimit_exempt_pwd != NULL && strcasecmp(ipl_m, tlimit_exempt_pwd) == 0) {
                conn->tlimit_exempt = true;
                cprintf(conn, "TLIMIT exempt password from %s\n", conn->remote_ip);
            }
            cfg_string_free(tlimit_exempt_pwd);
        }

        // enforce 24hr ip address connect time limit
        if (ip_limit_mins && stream_snd && !conn->tlimit_exempt) {
            int ipl_cur_mins = json_default_int(&cfg_ipl, conn->remote_ip, 0, NULL);
            if (ipl_cur_mins >= ip_limit_mins) {
                cprintf(conn, "TLIMIT-IP connecting LIMIT EXCEEDED cur:%d >= lim:%d for %s\n", ipl_cur_mins, ip_limit_mins, conn->remote_ip);
                send_msg_mc_encoded(mc, "MSG", "ip_limit", "%d,%s", ip_limit_mins, conn->remote_ip);
                free(type_m); free(pwd_m); free(ipl_m);
                return true;
            } else {
                conn->ipl_cur_secs = MINUTES_TO_SEC(ipl_cur_mins);
                cprintf(conn, "TLIMIT-IP connecting LIMIT OKAY cur:%d < lim:%d for %s\n", ipl_cur_mins, ip_limit_mins, conn->remote_ip);
            }
        }

	    // Let client know who we think they are.
		// Use public ip of Kiwi server when client connection is on local subnet.
		// This distinction is for the benefit of setting the user's geolocation at short-wave.info
        if (!stream_wf) {
            char *client_public_ip = (is_local && ddns.pub_valid)? ddns.ip_pub : conn->remote_ip;
            send_msg(conn, false, "MSG client_public_ip=%s", client_public_ip);
            //cprintf(conn, "client_public_ip %s\n", client_public_ip);
        }

		int chan_no_pwd = cfg_int("chan_no_pwd", NULL, CFG_REQUIRED);
		int chan_need_pwd = RX_CHANS - chan_no_pwd;

		if (type_kiwi) {
			pwd_s = admcfg_string("user_password", NULL, CFG_REQUIRED);
			bool no_pwd = (pwd_s == NULL || *pwd_s == '\0');
			cfg_auto_login = admcfg_bool("user_auto_login", NULL, CFG_REQUIRED);
			
			// if no user password set allow unrestricted connection
			if (no_pwd) {
			    if (!stream_wf)
				    cprintf(conn, "PWD kiwi ALLOWED: no config pwd set, allow any\n");
				allow = true;
			} else
			
			// config pwd set, but auto_login for local subnet is true
			if (cfg_auto_login && is_local) {
			    if (!stream_wf)
				    cprintf(conn, "PWD kiwi ALLOWED: config pwd set, but is_local and auto-login set\n");
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
		
		if (type_admin) {
			pwd_s = admcfg_string("admin_password", NULL, CFG_REQUIRED);
			bool no_pwd = (pwd_s == NULL || *pwd_s == '\0');
			bool prior_auth = (conn->auth && conn->auth_admin);
			cfg_auto_login = admcfg_bool("admin_auto_login", NULL, CFG_REQUIRED);
			clfprintf(conn, prior_auth? PRINTF_REG : PRINTF_LOG, "PWD %s config pwd set %s, auto-login %s\n",
			    type_m, no_pwd? "FALSE":"TRUE", cfg_auto_login? "TRUE":"FALSE");
			
			// Since we no longer cookie save the admin password get session persistence by checking for prior auth.
			// This is important so the admin pwd isn't asked for repeatedly during e.g. dx list editing.
			if (prior_auth) {
				cprintf(conn, "PWD %s prior auth\n", type_m);
				allow = true;
			} else

			// can't determine local network status (yet)
			if (isLocal == NO_LOCAL_IF) {
				clprintf(conn, "PWD %s CAN'T DETERMINE: no local network interface information\n", type_m);
				cant_determine = true;
			} else

			// no config pwd set (e.g. initial setup) -- allow if connection is from local network
			if (no_pwd && is_local) {
				clprintf(conn, "PWD %s ALLOWED: no config pwd set, but is_local\n", type_m);
				allow = true;
			} else
			
			// config pwd set, but auto_login for local subnet is true
			if (cfg_auto_login && is_local) {
				clprintf(conn, "PWD %s ALLOWED: config pwd set, but is_local and auto-login set\n", type_m);
				allow = true;
			}
		} else {
			cprintf(conn, "PWD bad type=%s\n", type_m);
			pwd_s = NULL;
		}
		
        if (type_admin && auth_su && strcmp(conn->remote_ip, auth_su_remote_ip) == 0) {
            bool no_console = false;
            struct stat st;
            if (stat(DIR_CFG "/opt.no_console", &st) == 0)
                no_console = true;
            if (no_console == false) {
                printf("PWD %s ALLOWED: by su\n", type_m);
                allow = true;
                is_local = true;
            }
            auth_su = false;        // be certain to reset the global immediately
            memset(auth_su_remote_ip, 0, sizeof(auth_su_remote_ip));
        }
		
		int badp = 1;

		//clprintf(conn, "PWD %s RESULT pwd_s=<%s> pwd_m=<%s> badp=%d cant_determine=%d allow=%d is_local=%d isLocal(enum)=%d %s\n",
		//    type_m, pwd_s, pwd_m, badp, cant_determine, allow, is_local, isLocal, conn->remote_ip);

		if (cant_determine) {
		    badp = 2;
		} else

		if (allow) {
			if (log_auth_attempt || pwd_debug)
				clprintf(conn, "PWD %s ALLOWED: from %s\n", type_m, conn->remote_ip);
			badp = 0;
		} else
		
		if ((pwd_s == NULL || *pwd_s == '\0')) {
			clprintf(conn, "PWD %s REJECTED: no config pwd set, from %s\n", type_m, conn->remote_ip);
			badp = 1;
		} else {
			if (pwd_m == NULL || pwd_s == NULL) {
                if (pwd_debug)
                    cprintf(conn, "PWD %s probably auto-login check, from %s\n", type_m, conn->remote_ip);
				badp = 1;
			} else {
				//cprintf(conn, "PWD CMP %s pwd_s \"%s\" pwd_m \"%s\" from %s\n", type_m, pwd_s, pwd_m, conn->remote_ip);
				badp = strcasecmp(pwd_m, pwd_s)? 1:0;
                if (pwd_debug)
                    cprintf(conn, "PWD %s %s: from %s\n", type_m, badp? "REJECTED":"ACCEPTED", conn->remote_ip);
			}
		}
		
		send_msg(conn, false, "MSG rx_chans=%d", RX_CHANS);
		send_msg(conn, false, "MSG chan_no_pwd=%d", chan_no_pwd);
		send_msg(conn, false, "MSG badp=%d", badp);

        free(type_m); free(pwd_m); free(ipl_m);
		cfg_string_free(pwd_s);
		
		// only when the auth validates do we setup the handler
		if (badp == 0) {
		
		    // It's possible for both to be set e.g. auth_kiwi set on initial user connection
		    // then correct admin pwd given later for label edit.
		    
			if (type_kiwi) conn->auth_kiwi = true;

			if (type_admin) {
			    conn->auth_admin = true;
			    int chan = conn->rx_channel;

                // give admin auth to all associated conns
			    if (!stream_admin_or_mfg && chan != -1) {
                    conn_t *c = conns;
                    for (int i=0; i < N_CONNS; i++, c++) {
                        if (!c->valid || !(c->type == STREAM_SOUND || c->type == STREAM_WATERFALL || c->type == STREAM_EXT))
                            continue;
                        if (c->rx_channel == chan || (c->type == STREAM_EXT && c->ext_rx_chan == chan)) {
                            c->auth_admin = true;
                        }
                    }
			    }
			}

			if (conn->auth == false) {
				conn->auth = true;
				conn->isLocal = is_local;
				
				// send cfg once to javascript
				if (stream_snd || stream_admin_or_mfg)
					rx_server_send_config(conn);
				
				// setup stream task first time it's authenticated
				stream_t *st = &streams[conn->type];
				if (st->setup) (st->setup)((void *) conn);
			}
		}

		return true;
	}

	if (strcmp(cmd, "SET keepalive") == 0) {
		conn->keepalive_count++;
		return true;
	}

	// SECURITY: we accept no incoming commands besides auth and keepalive until auth is successful
	if (conn->auth == false) {
		clprintf(conn, "### SECURITY: NO AUTH YET: %s %d %s <%s>\n", stream_name, conn->type, conn->remote_ip, cmd);
		return true;	// fake that we accepted command so it won't be further processed
	}

	if (strcmp(cmd, "SET is_admin") == 0) {
	    assert(conn->auth == true);
		send_msg(conn, false, "MSG is_admin=%d", conn->auth_admin);
		return true;
	}

	if (strcmp(cmd, "SET get_authkey") == 0) {
		if (conn->auth_admin == false) {
			cprintf(conn, "get_authkey NO ADMIN AUTH %s\n", conn->remote_ip);
			return true;
		}
		
		free(current_authkey);
		current_authkey = kiwi_authkey();
		send_msg(conn, false, "MSG authkey_cb=%s", current_authkey);
		return true;
	}

	if (kiwi_str_begins_with(cmd, "SET save_cfg=")) {
		if (conn->auth_admin == FALSE) {
			lprintf("** attempt to save kiwi config with auth_admin == FALSE! IP %s\n", conn->remote_ip);
			return true;	// fake that we accepted command so it won't be further processed
		}

		char *json = cfg_realloc_json(strlen(cmd), CFG_NONE);	// a little bigger than necessary
		n = sscanf(cmd, "SET save_cfg=%s", json);
		assert(n == 1);
		//printf("SET save_cfg=...\n");
		kiwi_str_decode_inplace(json);
		cfg_save_json(json);
		update_vars_from_config();      // update C copies of vars

		return true;
	}

	if (kiwi_str_begins_with(cmd, "SET save_adm=")) {
		if (conn->type != STREAM_ADMIN) {
			lprintf("** attempt to save admin config from non-STREAM_ADMIN! IP %s\n", conn->remote_ip);
			return true;	// fake that we accepted command so it won't be further processed
		}
	
		if (conn->auth_admin == FALSE) {
			lprintf("** attempt to save admin config with auth_admin == FALSE! IP %s\n", conn->remote_ip);
			return true;	// fake that we accepted command so it won't be further processed
		}

		char *json = admcfg_realloc_json(strlen(cmd), CFG_NONE);	// a little bigger than necessary
		n = sscanf(cmd, "SET save_adm=%s", json);
		assert(n == 1);
		//printf("SET save_adm=...\n");
		kiwi_str_decode_inplace(json);
		admcfg_save_json(json);
		//update_vars_from_config();    // no admin vars need to be updated on save currently
		
		return true;
	}

#if RX_CHANS

	if (strcmp(cmd, "SET GET_USERS") == 0) {
		rx_chan_t *rx;
		bool need_comma = false;
		sb = (char *) "[";
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
					char *user = c->isUserIP? NULL : kiwi_str_encode(c->user);
					char *geo = c->geo? kiwi_str_encode(c->geo) : NULL;
					char *ext = ext_users[i].ext? kiwi_str_encode((char *) ext_users[i].ext->name) : NULL;
					const char *ip = isAdmin? c->remote_ip : "";
					asprintf(&sb2, "%s{\"i\":%d,\"n\":\"%s\",\"g\":\"%s\",\"f\":%d,\"m\":\"%s\",\"z\":%d,\"t\":\"%d:%02d:%02d\",\"e\":\"%s\",\"a\":\"%s\"}",
						need_comma? ",":"", i, user? user:"", geo? geo:"", c->freqHz,
						kiwi_enum2str(c->mode, mode_s, ARRAY_LEN(mode_s)), c->zoom, hr, min, sec, ext? ext:"", ip);
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
	if (kiwi_str_begins_with(cmd, "SET DX_UPD")) {
		if (conn->auth_admin == false) {
			cprintf(conn, "DX_UPD NO ADMIN AUTH %s\n", conn->remote_ip);
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
		n = sscanf(cmd, "SET DX_UPD g=%d f=%f o=%d m=%d i=%1024ms n=%1024ms", &gid, &freq, &mkr_off, &flags, &text_m, &notes_m);
        //printf("DX_UPD [%s]\n", cmd);
		//printf("DX_UPD n=%d #%d %8.2f 0x%x text=<%s> notes=<%s>\n", n, gid, freq, flags, text_m, notes_m);

		if (n != 2 && n != 6) {
			printf("DX_UPD n=%d\n", n);
            free(text_m); free(notes_m);
			return true;
		}
		
		//  gid freq    action
		//  !-1 -1      delete
		//  !-1 !-1     modify
		//  -1  x       add new
		
		dx_t *dxp;
		if (gid >= -1 && gid < dx.len) {
			if (gid != -1 && freq == -1) {
				// delete entry by forcing to top of list, then decreasing size by one before save
				cprintf(conn, "DX_UPD %s delete entry #%d\n", conn->remote_ip, gid);
				dxp = &dx.list[gid];
				dxp->freq = 999999;
				new_len = dx.len - 1;
			} else {
				if (gid == -1) {
					// new entry: add to end of list (in hidden slot), then sort will insert it properly
					cprintf(conn, "DX_UPD %s adding new entry\n", conn->remote_ip);
					assert(dx.hidden_used == false);		// FIXME need better serialization
					dxp = &dx.list[dx.len];
					dx.hidden_used = true;
					dx.len++;
					new_len = dx.len;
				} else {
					// modify entry
					cprintf(conn, "DX_UPD %s modify entry #%d\n", conn->remote_ip, gid);
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
				dxp->notes = strdup(notes_m);
			}
		} else {
			printf("DX_UPD: gid %d >= dx.len %d ?\n", gid, dx.len);
		}
		
		// NB: dx.len here, NOT new_len
		qsort(dx.list, dx.len, sizeof(dx_t), qsort_floatcomp);
		//printf("DX_UPD after qsort dx.len %d new_len %d top elem f=%.2f\n",
		//	dx.len, new_len, dx.list[dx.len-1].freq);
		dx.len = new_len;
		//NextTask("DX_UPD 1");
		for (i = 0; i < dx.len; i++) dx.list[i].idx = i;
		//NextTask("DX_UPD 2");
		dx_save_as_json();		// FIXME need better serialization
		//NextTask("DX_UPD 3");
		dx_reload();
		send_msg(conn, false, "MSG request_dx_update");	// get client to request updated dx list

        free(text_m); free(notes_m);
		return true;
	}

	if (kiwi_str_begins_with(cmd, "SET MKR")) {
		float min, max;
		int zoom, width;
		n = sscanf(cmd, "SET MKR min=%f max=%f zoom=%d width=%d", &min, &max, &zoom, &width);
		if (n != 4) return true;
		float bw;
		bw = max - min;
		static bool first = true;
		static int dx_lastx;
		dx_lastx = 0;
		
		if (dx.len == 0) {
			return true;
		}
		
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        u4_t msec = ts.tv_nsec/1000000;
		asprintf(&sb, "[{\"s\":%ld,\"m\":%d}", ts.tv_sec, msec);   // reset appending
		sb = kstr_wrap(sb);
		int send = 0;
		
		// bsearch the lower bound for speed with large lists
		dx_t dx_min;
		#define DX_SEARCH_WINDOW 10.0
		dx_min.freq = min - DX_SEARCH_WINDOW;
		dx_list_first = &dx.list[0];
		dx_list_last = &dx.list[dx.len-1];      // NB: addr of LAST in list
		dx_t *dp = (dx_t *) bsearch(&dx_min, dx.list, dx.len, sizeof(dx_t), bsearch_freqcomp);
		if (dp == NULL) panic("DX bsearch");
		//printf("DX MKR key=%.2f bsearch=%.2f(%d/%d) min=%.2f max=%.2f\n",
		//    dx_min.freq, dp->freq + (dp->offset / 1000.0), dp->idx, dx.len, min, max);

		for (; dp < &dx.list[dx.len]; dp++) {
			float freq = dp->freq + (dp->offset / 1000.0);		// carrier plus offset

			// when zoomed far-in need to look at wider window since we don't know PB center here
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
				dp->idx, freq, dp->offset, dp->flags, dp->ident,
				dp->notes? ",\"n\":\"":"", dp->notes? dp->notes:"", dp->notes? "\"":"");
			//printf("dx(%d,%.3f,%.0f,%d,\'%s\'%s%s%s)\n", dp->idx, f, dp->offset, dp->flags, dp->ident,
			//	dp->notes? ",\'":"", dp->notes? dp->notes:"", dp->notes? "\'":"");
			sb = kstr_cat(sb, kstr_wrap(sb2));
		    //printf("DX %d: %.2f(%d)\n", send, freq, dp->idx);
			send++;
		}
		
		sb = kstr_cat(sb, "]");
		send_msg(conn, false, "MSG mkr=%s", kstr_sp(sb));
		kstr_free(sb);
		//printf("DX send=%d\n", send);
		return true;
	}

#endif

	if (strcmp(cmd, "SET GET_CONFIG") == 0) {
		asprintf(&sb, "{\"r\":%d,\"g\":%d,\"s\":%d,\"pu\":\"%s\",\"pe\":%d,\"pv\":\"%s\",\"pi\":%d,\"n\":%d,\"m\":\"%s\",\"v1\":%d,\"v2\":%d}",
			RX_CHANS, GPS_CHANS, ddns.serno, ddns.ip_pub, ddns.port_ext, ddns.ip_pvt, ddns.port, ddns.nm_bits, ddns.mac, version_maj, version_min);
		send_msg(conn, false, "MSG config_cb=%s", sb);
		free(sb);
		return true;
	}
	
	if (kiwi_str_begins_with(cmd, "SET STATS_UPD")) {
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
			asprintf(&sb, "{%s,", cpu_stats_buf);
		} else {
			asprintf(&sb, "{");
		}
		sb = kstr_wrap(sb);

		float sum_kbps = audio_kbps + waterfall_kbps + http_kbps;
		asprintf(&sb2, "\"aa\":%.0f,\"aw\":%.0f,\"af\":%.0f,\"at\":%.0f,\"ah\":%.0f,\"as\":%.0f",
			audio_kbps, waterfall_kbps, waterfall_fps[ch], waterfall_fps[RX_CHANS], http_kbps, sum_kbps);
		sb = kstr_cat(sb, kstr_wrap(sb2));

		asprintf(&sb2, ",\"ga\":%d,\"gt\":%d,\"gg\":%d,\"gf\":%d,\"gc\":%.6f,\"go\":%d",
			gps.acquiring, gps.tracking, gps.good, gps.fixes, adc_clock_system()/1e6, clk.adc_gps_clk_corrections);
		sb = kstr_cat(sb, kstr_wrap(sb2));

#if RX_CHANS
		extern int audio_dropped;
		asprintf(&sb2, ",\"ad\":%d,\"au\":%d,\"ae\":%d,\"ar\":%d,\"an\":%d,\"ap\":[",
			audio_dropped, underruns, seq_errors, dpump_resets, NRX_BUFS);
		sb = kstr_cat(sb, kstr_wrap(sb2));
		for (i = 0; i < NRX_BUFS; i++) {
		    asprintf(&sb2, "%s%d", (i != 0)? ",":"", dpump_hist[i]);
		    sb = kstr_cat(sb, kstr_wrap(sb2));
		}
        sb = kstr_cat(sb, "]");
#endif

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
		if (conn->auth_admin == false) {
			cprintf(conn, "SET gps_update: NO ADMIN AUTH %s\n", conn->remote_ip);
			return true;
		}

		if (gps.IQ_seq_w != gps.IQ_seq_r) {
		    asprintf(&sb, "{\"ch\":%d,\"IQ\":[", 0);
		    sb = kstr_wrap(sb);
		    s4_t iq;
            for (j = 0; j < GPS_IQ_SAMPS*NIQ; j++) {
                #if GPS_INTEG_BITS == 16
                    iq = S4(S2(gps.IQ_data[j*2+1]));
                #else
                    iq = S32_16_16(gps.IQ_data[j*2], gps.IQ_data[j*2+1]);
                #endif
                asprintf(&sb2, "%s%d", j? ",":"", iq);
                sb = kstr_cat(sb, kstr_wrap(sb2));
            }
            sb = kstr_cat(sb, "]}");
            send_msg_encoded(conn, "MSG", "gps_IQ_data_cb", "%s", kstr_sp(sb));
            kstr_free(sb);
		    gps.IQ_seq_r = gps.IQ_seq_w;
		    NextTask("gps_update1");
		}

        // sends a list of the last gps.POS_len entries per query
		if (gps.POS_seq_w != gps.POS_seq_r) {
		    asprintf(&sb, "{\"ref_lat\":%.6f,\"ref_lon\":%.6f,\"POS\":[", gps.ref_lat, gps.ref_lon);
		    sb = kstr_wrap(sb);
		    int xmax[GPS_NPOS], xmin[GPS_NPOS], ymax[GPS_NPOS], ymin[GPS_NPOS];
		    xmax[0] = xmax[1] = ymax[0] = ymax[1] = INT_MIN;
		    xmin[0] = xmin[1] = ymin[0] = ymin[1] = INT_MAX;
            for (j = 0; j < GPS_NPOS; j++) {
                for (k = 0; k < gps.POS_len; k++) {
                    asprintf(&sb2, "%s%.6f,%.6f", (j||k)? ",":"", gps.POS_data[j][k].lat, gps.POS_data[j][k].lon);
                    sb = kstr_cat(sb, kstr_wrap(sb2));
                    if (gps.POS_data[j][k].lat != 0) {
                        int x = gps.POS_data[j][k].x;
                        if (x > xmax[j]) xmax[j] = x; else if (x < xmin[j]) xmin[j] = x;
                        int y = gps.POS_data[j][k].y;
                        if (y > ymax[j]) ymax[j] = y; else if (y < ymin[j]) ymin[j] = y;
                    }
                }
            }
            asprintf(&sb2, "],\"x0span\":%d,\"y0span\":%d,\"x1span\":%d,\"y1span\":%d}",
                xmax[0]-xmin[0], ymax[0]-ymin[0], xmax[1]-xmin[1], ymax[1]-ymin[1]);
            sb = kstr_cat(sb, kstr_wrap(sb2));
            send_msg_encoded(conn, "MSG", "gps_POS_data_cb", "%s", kstr_sp(sb));
            kstr_free(sb);
		    gps.POS_seq_r = gps.POS_seq_w;
		    NextTask("gps_update2");
		}

        // sends a list of the newest, non-duplicate entries per query
		if (gps.MAP_seq_w != gps.MAP_seq_r) {
		    asprintf(&sb, "{\"ref_lat\":%.6f,\"ref_lon\":%.6f,\"MAP\":[", gps.ref_lat, gps.ref_lon);
		    sb = kstr_wrap(sb);
            int any_new = 0;
            for (j = 0; j < GPS_NMAP; j++) {
                for (k = 0; k < gps.MAP_len; k++) {
                    u4_t seq = gps.MAP_data[0][k].seq;
                    if (seq <= gps.MAP_seq_r || gps.MAP_data[j][k].lat == 0) continue;
                    asprintf(&sb2, "%s{\"seq\":%d,\"nmap\":%d,\"lat\":%.6f,\"lon\":%.6f}", (any_new)? ",":"",
                        seq, j, gps.MAP_data[j][k].lat, gps.MAP_data[j][k].lon);
                    sb = kstr_cat(sb, kstr_wrap(sb2));
                    any_new++;
                }
            }
            sb = kstr_cat(sb, "]}");
            if (any_new)
                send_msg_encoded(conn, "MSG", "gps_MAP_data_cb", "%s", kstr_sp(sb));
            kstr_free(sb);
		    gps.MAP_seq_r = gps.MAP_seq_w;
		    NextTask("gps_update3");
		}

		gps_stats_t::gps_chan_t *c;
		
		asprintf(&sb, "{\"FFTch\":%d,\"ch\":[", gps.FFTch);
		sb = kstr_wrap(sb);
		
		for (i=0; i < gps_chans; i++) {
			c = &gps.ch[i];
			int prn = -1;
			char prn_s = 'x';
			if (c->sat >= 0) {
			    prn_s = sat_s[Sats[c->sat].type];
			    prn = Sats[c->sat].prn;
			}
			asprintf(&sb2, "%s{\"ch\":%d,\"prn_s\":\"%c\",\"prn\":%d,\"snr\":%d,\"rssi\":%d,\"gain\":%d,\"hold\":%d,\"wdog\":%d"
				",\"unlock\":%d,\"parity\":%d,\"alert\":%d,\"sub\":%d,\"sub_renew\":%d,\"soln\":%d,\"ACF\":%d,\"novfl\":%d,\"az\":%d,\"el\":%d}",
				i? ", ":"", i, prn_s, prn, c->snr, c->rssi, c->gain, c->hold, c->wdog,
				c->ca_unlocked, c->parity, c->alert, c->sub, c->sub_renew, c->soln, c->ACF_mode, c->novfl, c->az, c->el);
//jks2
//if(i==3)printf("%s\n", sb2);
			sb = kstr_cat(sb, kstr_wrap(sb2));
			c->parity = 0;
			c->soln = 0;
			for (j = 0; j < SUBFRAMES; j++) {
				if (c->sub_renew & (1<<j)) {
					c->sub |= 1<<j;
					c->sub_renew &= ~(1<<j);
				}
			}
		    NextTask("gps_update4");
		}

        asprintf(&sb2, "],\"soln\":%d,\"sep\":%d", gps.soln, gps.E1B_plot_separately);
		sb = kstr_cat(sb, kstr_wrap(sb2));

		UMS hms(gps.StatSec/60/60);
		
		unsigned r = (timer_ms() - gps.start)/1000;
		if (r >= 3600) {
			asprintf(&sb2, ",\"run\":\"%d:%02d:%02d\"", r / 3600, (r / 60) % 60, r % 60);
		} else {
			asprintf(&sb2, ",\"run\":\"%d:%02d\"", (r / 60) % 60, r % 60);
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
			//asprintf(&sb2, ",\"lat\":\"%8.6f %c\"", gps.StatLat, gps.StatNS);
			asprintf(&sb2, ",\"lat\":%.6f", gps.sgnLat);
			sb = kstr_cat(sb, kstr_wrap(sb2));
			//asprintf(&sb2, ",\"lon\":\"%8.6f %c\"", gps.StatLon, gps.StatEW);
			asprintf(&sb2, ",\"lon\":%.6f", gps.sgnLon);
			sb = kstr_cat(sb, kstr_wrap(sb2));
			asprintf(&sb2, ",\"alt\":\"%1.0f m\"", gps.StatAlt);
			sb = kstr_cat(sb, kstr_wrap(sb2));
			asprintf(&sb2, ",\"map\":\"<a href='http://wikimapia.org/#lang=en&lat=%8.6f&lon=%8.6f&z=18&m=b' target='_blank'>wikimapia.org</a>\"",
				gps.sgnLat, gps.sgnLon);
		} else {
			//asprintf(&sb2, ",\"lat\":null");
			asprintf(&sb2, ",\"lat\":0");
		}
		sb = kstr_cat(sb, kstr_wrap(sb2));
			
		asprintf(&sb2, ",\"acq\":%d,\"track\":%d,\"good\":%d,\"fixes\":%d,\"adc_clk\":%.6f,\"adc_corr\":%d}",
			gps.acquiring? 1:0, gps.tracking, gps.good, gps.fixes, adc_clock_system()/1e6, clk.adc_gps_clk_corrections);
		sb = kstr_cat(sb, kstr_wrap(sb2));

		send_msg_encoded(conn, "MSG", "gps_update_cb", "%s", kstr_sp(sb));
		kstr_free(sb);
        NextTask("gps_update5");

		return true;
	}

	n = strcmp(cmd, "SET gps_az_el_history");
	if (n == 0) {
		if (conn->auth_admin == false) {
			cprintf(conn, "SET gps_az_el_history: NO ADMIN AUTH %s\n", conn->remote_ip);
			return true;
		}

        time_t t; time(&t);
        struct tm tm; gmtime_r(&t, &tm);
        //int now = (tm.tm_hour & 3)*60 + tm.tm_min;
        int now = tm.tm_min;
        
        int az, el;
        int sat_seen[MAX_SATS], prn_seen[MAX_SATS], samp_seen[AZEL_NSAMP];
        memset(&sat_seen, 0, sizeof(sat_seen));
        memset(&prn_seen, 0, sizeof(prn_seen));
        memset(&samp_seen, 0, sizeof(samp_seen));

        // sat/prn seen during any sample period
        for (int sat = 0; sat < MAX_SATS; sat++) {
		    for (int samp = 0; samp < AZEL_NSAMP; samp++) {
		        if (gps.el[samp][sat] != 0) {
		            sat_seen[sat] = sat+1;     // +1 bias
		            prn_seen[sat] = sat+1;     // +1 bias
		            break;
		        }
		    }
		}

        #if 0
        if (gps_debug) {
            // any sat/prn seen during specific sample period
            for (int samp = 0; samp < AZEL_NSAMP; samp++) {
                for (int sat = 0; sat < MAX_SATS; sat++) {
                    if (gps.el[samp][sat] != 0) {
                        samp_seen[samp] = 1;
                        break;
                    }
                }
            }

            real_printf("-----------------------------------------------------------------------------\n");
            for (int samp = 0; samp < AZEL_NSAMP; samp++) {
                if (!samp_seen[samp] && samp != now) continue;
                for (int sat = 0; sat < MAX_SATS; sat++) {
                    if (!sat_seen[sat]) continue;
                    real_printf("%s     ", PRN(prn_seen[sat]-1));
                }
                real_printf("SAMP %2d %s\n", samp, (samp == now)? "==== NOW ====":"");
                for (int sat = 0; sat < MAX_SATS; sat++) {
                    if (!sat_seen[sat]) continue;
                    az = gps.az[samp][sat];
                    el = gps.el[samp][sat];
                    if (az == 0 && el == 0)
                        real_printf("         ");
                    else
                        real_printf("%3d|%2d   ", az, el);
                }
                real_printf("\n");
            }
        }
        #endif

        // send history only for sats seen
        asprintf(&sb, "{\"n_sats\":%d,\"n_samp\":%d,\"now\":%d,\"sat_seen\":[", MAX_SATS, AZEL_NSAMP, now);
        sb = kstr_wrap(sb);

		first = 1;
        for (int sat = 0; sat < MAX_SATS; sat++) {
            if (!sat_seen[sat]) continue;
            asprintf(&sb2, "%s%d", first? "":",", sat_seen[sat]-1);   // -1 bias
            sb = kstr_cat(sb, kstr_wrap(sb2));
            first = 0;
        }
		
        sb = kstr_cat(sb, "],\"prn_seen\":[");
		first = 1;
        for (int sat = 0; sat < MAX_SATS; sat++) {
            if (!sat_seen[sat]) continue;
            char *prn_s = PRN(prn_seen[sat]-1);
            if (*prn_s == 'N') prn_s++;
            asprintf(&sb2, "%s\"%s\"", first? "":",", prn_s);
            sb = kstr_cat(sb, kstr_wrap(sb2));
            first = 0;
        }

        sb = kstr_cat(sb, "],\"az\":[");
		first = 1;
		for (int samp = 0; samp < AZEL_NSAMP; samp++) {
            for (int sat = 0; sat < MAX_SATS; sat++) {
                if (!sat_seen[sat]) continue;
                asprintf(&sb2, "%s%d", first? "":",", gps.az[samp][sat]);
                sb = kstr_cat(sb, kstr_wrap(sb2));
                first = 0;
            }
        }

        NextTask("gps_az_el_history1");
        sb = kstr_cat(sb, "],\"el\":[");
		first = 1;
		for (int samp = 0; samp < AZEL_NSAMP; samp++) {
            for (int sat = 0; sat < MAX_SATS; sat++) {
                if (!sat_seen[sat]) continue;
                asprintf(&sb2, "%s%d", first? "":",", gps.el[samp][sat]);
                sb = kstr_cat(sb, kstr_wrap(sb2));
                first = 0;
            }
        }

        asprintf(&sb2, "],\"qzs3\":{\"az\":%d,\"el\":%d},\"shadow_map\":[", gps.qzs_3.az, gps.qzs_3.el);
        sb = kstr_cat(sb, kstr_wrap(sb2));
		first = 1;
        for (int az = 0; az < 360; az++) {
            asprintf(&sb2, "%s%u", first? "":",", gps.shadow_map[az]);
            sb = kstr_cat(sb, kstr_wrap(sb2));
            first = 0;
        }

        sb = kstr_cat(sb, "]}");
		send_msg_encoded(conn, "MSG", "gps_az_el_history_cb", "%s", kstr_sp(sb));
		kstr_free(sb);
        NextTask("gps_az_el_history2");
		return true;
	}
	
	n = sscanf(cmd, "SET gps_IQ_data_ch=%d", &j);
	if (n == 1) {
		if (conn->auth_admin == false) {
			cprintf(conn, "SET gps_IQ_data_ch: NO ADMIN AUTH %s\n", conn->remote_ip);
			return true;
		}

	    gps.IQ_data_ch = j;
		return true;
	}

	n = sscanf(cmd, "SET gps_kick_pll_ch=%d", &j);
	if (n == 1) {
		if (conn->auth_admin == false) {
			cprintf(conn, "SET gps_kick_pll_ch: NO ADMIN AUTH %s\n", conn->remote_ip);
			return true;
		}

	    gps.kick_lo_pll_ch = j+1;
	    printf("gps_kick_pll_ch=%d\n", gps.kick_lo_pll_ch);
		return true;
	}

	n = sscanf(cmd, "SET gps_gain=%d", &j);
	if (n == 1) {
		if (conn->auth_admin == false) {
			cprintf(conn, "SET gps_gain: NO ADMIN AUTH %s\n", conn->remote_ip);
			return true;
		}

	    gps.gps_gain = j;
	    printf("gps_gain=%d\n", gps.gps_gain);
		return true;
	}

	if (kiwi_str_begins_with(cmd, "SET pref_export")) {
		free(conn->pref_id);
		free(conn->pref);
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
			    free(c->pref_id); c->pref_id = NULL;
			    free(c->pref); c->pref = NULL;
			}
		}
		
		return true;
	}
	
	if (kiwi_str_begins_with(cmd, "SET pref_import")) {
		free(conn->pref_id);
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

	if (kiwi_str_begins_with(cmd, "SET ident_user=")) {
        char *ident_user_m = NULL;
	    sscanf(cmd, "SET ident_user=%256ms", &ident_user_m);
		bool noname = (ident_user_m == NULL || ident_user_m[0] == '\0');
		bool setUserIP = false;
		
		if (conn->mc == NULL) return true;	// we've seen this
		if (noname && !conn->user) setUserIP = true;
		if (noname && conn->user && strcmp(conn->user, conn->remote_ip)) setUserIP = true;

		if (setUserIP) {
			kiwi_str_redup(&conn->user, "user", conn->remote_ip);
			conn->isUserIP = TRUE;
			// printf(">>> isUserIP TRUE: %s:%05d setUserIP=%d noname=%d user=%s <%s>\n",
			// 	conn->remote_ip, conn->remote_port, setUserIP, noname, conn->user, cmd);
		}

		if (!noname) {
			kiwi_str_decode_inplace(ident_user_m);
			kiwi_str_redup(&conn->user, "user", ident_user_m);
			conn->isUserIP = FALSE;
			// printf(">>> isUserIP FALSE: %s:%05d setUserIP=%d noname=%d user=%s <%s>\n",
			// 	conn->remote_ip, conn->remote_port, setUserIP, noname, conn->user, cmd);
		}
		
		//clprintf(conn, "SND user: <%s>\n", cmd);
		if (!conn->arrived) {
			loguser(conn, LOG_ARRIVED);
			conn->arrived = TRUE;
		}
		
		free(ident_user_m);
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
	
	char *geo_m = NULL;
	n = sscanf(cmd, "SET geo=%127ms", &geo_m);
	if (n == 1) {
		kiwi_str_decode_inplace(geo_m);
		//cprintf(conn, "ch%d recv geoloc from client: %s\n", conn->rx_channel, geo_m);
		free(conn->geo);
		conn->geo = geo_m;
		return true;
	}

	char *geojson_m = NULL;
	n = sscanf(cmd, "SET geojson=%256ms", &geojson_m);
	if (n == 1) {
		kiwi_str_decode_inplace(geojson_m);
		//clprintf(conn, "SND geo: <%s>\n", geojson_m);
		free(geojson_m);
		return true;
	}
	
	char *browser_m = NULL;
	n = sscanf(cmd, "SET browser=%256ms", &browser_m);
	if (n == 1) {
		kiwi_str_decode_inplace(browser_m);
		//clprintf(conn, "SND browser: <%s>\n", browser_m);
		free(browser_m);
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

    int clk_adj;
    n = sscanf(cmd, "SET clk_adj=%d", &clk_adj);
    if (n == 1) {
		if (conn->auth_admin == false) {
			cprintf(conn, "clk_adj NO ADMIN AUTH %s\n", conn->remote_ip);
			return true;
		}
		
        int hz_limit = PPM_TO_HZ(ADC_CLOCK_NOM, ADC_CLOCK_PPM_LIMIT);
        if (clk_adj < -hz_limit || clk_adj > hz_limit) {
			cprintf(conn, "clk_adj TOO LARGE = %d %d %d %f\n", clk_adj, -hz_limit, hz_limit, PPM_TO_HZ(ADC_CLOCK_NOM, ADC_CLOCK_PPM_LIMIT));
			return true;
		}
		
        clock_manual_adj(clk_adj);
        printf("MANUAL clk_adj = %d\n", clk_adj);
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
		//printf("SET debug_v=%d\n", debug_v);
		return true;
	}

	// SECURITY: only used during debugging
	sb = (char *) "SET debug_msg=";
	slen = strlen(sb);
	n = strncmp(cmd, sb, slen);
	if (n == 0) {
		kiwi_str_decode_inplace(cmd);
		clprintf(conn, "### DEBUG MSG: <%s>\n", &cmd[slen]);
		return true;
	}

#if RX_CHANS
	// SECURITY: FIXME: get rid of this?
	int wf_comp;
	n = sscanf(cmd, "SET wf_comp=%d", &wf_comp);
	if (n == 1) {
		c2s_waterfall_compression(conn->rx_channel, wf_comp? true:false);
		printf("### SET wf_comp=%d\n", wf_comp);
		return true;
	}
#endif

	if (kiwi_str_begins_with(cmd, "SERVER DE CLIENT")) return true;
	
	// we see these sometimes; not part of our protocol
	if (strcmp(cmd, "PING") == 0) return true;

	// we see these at the close of a connection sometimes; not part of our protocol
    #define ASCII_ETX 3
	if (strlen(cmd) == 2 && cmd[0] == ASCII_ETX) return true;

	return false;
}

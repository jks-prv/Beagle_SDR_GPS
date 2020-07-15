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
#include "rx.h"
#include "clk.h"
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
#include "debug.h"
#include "ext_int.h"
#include "wspr.h"

#ifndef CFG_GPS_ONLY
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

kstr_t *cpu_stats_buf;
volatile float audio_kbps[MAX_RX_CHANS+1], waterfall_kbps[MAX_RX_CHANS+1], waterfall_fps[MAX_RX_CHANS+1], http_kbps;
volatile u4_t audio_bytes[MAX_RX_CHANS+1], waterfall_bytes[MAX_RX_CHANS+1], waterfall_frames[MAX_RX_CHANS+1], http_bytes;
char *current_authkey;
int debug_v;
bool auth_su;
char auth_su_remote_ip[NET_ADDRSTRLEN];
bool conn_nolocal;

const char *mode_s[N_MODE] = {
    "am", "amn", "usb", "lsb", "cw", "cwn", "nbfm", "iq", "drm", "usn", "lsn", "sam", "sau", "sal", "sas"
};
const char *modu_s[N_MODE] = {
    "AM", "AMN", "USB", "LSB", "CW", "CWN", "NBFM", "IQ", "DRM", "USN", "LSN", "SAM", "SAU", "SAL", "SAS"
};
const int mode_hbw[N_MODE] = {
    9800/2, 5000/2, 2400/2, 2400/2, 400/2, 60/2, 12000/2, 10000/2, 10000/2, 2100/2, 2100/2, 9800/2, 9800/2, 9800/2, 9800/2
};
const int mode_offset[N_MODE] = {
    0, 0, 1500, -1500, 0, 0, 0, 0, 0, 1350, -1350, 0, 0, 0, 0
};

#ifndef CFG_GPS_ONLY

static dx_t *dx_list_first, *dx_list_last;

int bsearch_freqcomp(const void *key, const void *elem)
{
	dx_t *dx_key = (dx_t *) key, *dx_elem = (dx_t *) elem;
	float key_freq = dx_key->freq;
    float elem_freq = dx_elem->freq + ((float) dx_elem->offset / 1000.0);		// carrier plus offset
    
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
        float elem2_freq = dx_elem2->freq + ((float) dx_elem2->offset / 1000.0);		// carrier plus offset
        if (key_freq < elem2_freq) return 0;    // key was between without exact match
        return 1;   // key_freq > elem2_freq -> keep looking
    }
    return 0;   // key > last in array so lower is last (degenerate case)
}

#endif

void rx_common_init(conn_t *conn)
{
	conn->keepalive_time = timer_sec();

    if (is_multi_core)
	    send_msg(conn, true, "MSG is_multi_core");
}

bool rx_common_cmd(const char *stream_name, conn_t *conn, char *cmd)
{
	int i, j, k, n, first;
	struct mg_connection *mc = conn->mc;
	char *sb, *sb2;
	int slen;
	
	if (mc == NULL) {
	    cprintf(conn, "### cmd but mc is null <%s>\n", cmd);
	    return false;
	}
	
	NextTask("rx_common_cmd");      // breakup long runs of sequential commands -- sometimes happens at startup
    evLatency(EC_EVENT, EV_RX, 0, "rx_common_cmd", evprintf("%s", cmd));
	
	if (strcmp(cmd, "SET keepalive") == 0) {
	    conn->keepalive_time = timer_sec();

        // for STREAM_EXT send a roundtrip keepalive
	    if (conn->type == STREAM_EXT) {
            if (conn->ext_rx_chan == -1) return true;
            ext_send_msg(conn->ext_rx_chan, false, "MSG keepalive");
	    }
		conn->keepalive_count++;
		return true;
	}

	// SECURITY: auth command here is the only one allowed before auth check below (excluding keepalive above)
	if (kiwi_str_begins_with(cmd, "SET auth")) {
	
		const char *pwd_s = NULL;
		int cfg_auto_login;
		const char *uri = rx_streams[conn->type].uri;

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
		
		bool allow = false, cant_determine = false, cant_login = false, skip_dup_ip_check = false;
		bool type_kiwi = (type_m != NULL && strcmp(type_m, "kiwi") == 0);
		bool type_prot = (type_m != NULL && strcmp(type_m, "prot") == 0);
		bool type_admin = (type_m != NULL && strcmp(type_m, "admin") == 0);
		
		bool stream_wf = (conn->type == STREAM_WATERFALL);
		bool stream_snd = (conn->type == STREAM_SOUND);
		bool stream_snd_or_wf = (stream_snd || stream_wf);
		bool stream_admin_or_mfg = (conn->type == STREAM_ADMIN || conn->type == STREAM_MFG);
		bool stream_ext = (conn->type == STREAM_EXT);
		
		bool bad_type = (stream_snd_or_wf || stream_ext || stream_admin_or_mfg)? false : true;
		
		if ((!type_kiwi && !type_prot && !type_admin) || bad_type) {
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
		
		bool check_ip_against_restricted = true;
		bool check_ip_against_interfaces = true;
		bool restricted_ip_match = false;
		isLocal_t isLocal = IS_NOT_LOCAL;
		bool is_local = false;
        bool log_auth_attempt, pwd_debug;

	    #define nwf_cprintf(conn, fmt, ...) \
		    cprintf(conn, fmt, ## __VA_ARGS__)
		    //if (!stream_wf) cprintf(conn, fmt, ## __VA_ARGS__)

	    #define pdbug_cprintf(conn, fmt, ...) \
		    if (pwd_debug) cprintf(conn, fmt, ## __VA_ARGS__)
        if (debug_printfs) {
            log_auth_attempt = true;
            pwd_debug = true;
        } else {
            log_auth_attempt = (stream_admin_or_mfg || (stream_ext && type_admin));
            pwd_debug = false;
        }
        
        if (conn->internal_connection) {
            isLocal = IS_LOCAL;
            is_local = true;
            check_ip_against_interfaces = check_ip_against_restricted = false;
        }

		int fd;
		if (type_admin && check_ip_against_restricted && (fd = open(DIR_CFG "/opt.admin_ip", O_RDONLY)) != -1) {
		    char admin_ip[NET_ADDRSTRLEN];
		    n = read(fd, admin_ip, NET_ADDRSTRLEN);
		    if (n >= 1) {
		        admin_ip[n] = '\0';
		        if (admin_ip[n-1] == '\n') admin_ip[n-1] = '\0';
		        if (isLocal_ip(admin_ip) && strcmp(ip_remote(mc), admin_ip) == 0) {
		            isLocal = IS_LOCAL;
		            is_local = true;
		            restricted_ip_match = true;
		        }
		        cprintf(conn, "PWD opt.admin_ip %s\n", restricted_ip_match? "MATCH":"FAIL");
		        check_ip_against_interfaces = false;
		    }
		    close(fd);
		}
		
		if (check_ip_against_interfaces) {
            
            // SECURITY: call isLocal_if_ip() using mc->remote_ip NOT conn->remote_ip because the latter
            // could be spoofed via X-Real-IP / X-Forwarded-For to look like a local address.
            // For a non-local connection mc->remote_ip is 127.0.0.1 when the frp proxy is used
            // so it will never be considered a local connection.
            isLocal = isLocal_if_ip(conn, ip_remote(mc), (log_auth_attempt || pwd_debug)? "PWD" : NULL);

            if (conn_nolocal) {
                isLocal = IS_NOT_LOCAL;
                pwd_debug = true;
            }

            //#define TEST_NO_LOCAL_IF
            #ifdef TEST_NO_LOCAL_IF
                isLocal = NO_LOCAL_IF;
                pwd_debug = true;
            #endif
            
            is_local = (isLocal == IS_LOCAL);
            check_ip_against_restricted = false;
        }

		pdbug_cprintf(conn, "PWD %s %s conn #%d isLocal=%d is_local=%d auth=%d auth_kiwi=%d auth_prot=%d auth_admin=%d check_ip_against_restricted=%d restricted_ip_match=%d from %s\n",
			type_m, uri, conn->self_idx, isLocal, is_local,
			conn->auth, conn->auth_kiwi, conn->auth_prot, conn->auth_admin, check_ip_against_restricted, restricted_ip_match, conn->remote_ip);
		
        const char *tlimit_exempt_pwd = admcfg_string("tlimit_exempt_pwd", NULL, CFG_OPTIONAL);
        //#define TEST_TLIMIT_LOCAL
        #ifndef TEST_TLIMIT_LOCAL
            if (is_local) {
                conn->tlimit_exempt = true;
                cprintf(conn, "TLIMIT exempt local connection from %s\n", conn->remote_ip);
            }
        #endif
        pdbug_cprintf(conn, "PWD TLIMIT exempt password check: ipl=<%s> tlimit_exempt_pwd=<%s>\n",
            ipl_m, tlimit_exempt_pwd);
        if (ipl_m != NULL && tlimit_exempt_pwd != NULL && strcasecmp(ipl_m, tlimit_exempt_pwd) == 0) {
            conn->tlimit_exempt = true;
            conn->tlimit_exempt_by_pwd = true;
            skip_dup_ip_check = true;
            cprintf(conn, "TLIMIT exempt password from %s\n", conn->remote_ip);
        }
        cfg_string_free(tlimit_exempt_pwd);

        // enforce 24hr ip address connect time limit
        if (ip_limit_mins && stream_snd && !conn->tlimit_exempt) {
            int ipl_cur_secs = json_default_int(&cfg_ipl, conn->remote_ip, 0, NULL);
            int ipl_cur_mins = SEC_TO_MINUTES(ipl_cur_secs);
            //cprintf(conn, "TLIMIT-IP getting database sec:%d for %s\n", ipl_cur_secs, conn->remote_ip);
            if (ipl_cur_mins >= ip_limit_mins) {
                cprintf(conn, "TLIMIT-IP connecting LIMIT EXCEEDED cur:%d >= lim:%d for %s\n", ipl_cur_mins, ip_limit_mins, conn->remote_ip);
                send_msg_mc_encoded(mc, "MSG", "ip_limit", "%d,%s", ip_limit_mins, conn->remote_ip);
                free(type_m); free(pwd_m); free(ipl_m);
                conn->tlimit_zombie = true;
                return true;
            } else {
                cprintf(conn, "TLIMIT-IP connecting LIMIT OKAY cur:%d < lim:%d for %s\n", ipl_cur_mins, ip_limit_mins, conn->remote_ip);
            }
        }

	    // Let client know who we think they are.
		// Use public ip of Kiwi server when client connection is on local subnet.
		// This distinction is for the benefit of setting the user's geolocation at short-wave.info
        if (!stream_wf) {
            char *client_public_ip = (is_local && net.pub_valid)? net.ip_pub : conn->remote_ip;
            send_msg(conn, false, "MSG client_public_ip=%s", client_public_ip);
            //cprintf(conn, "client_public_ip %s\n", client_public_ip);
        }

		int chan_no_pwd = cfg_int("chan_no_pwd", NULL, CFG_REQUIRED);
		if (chan_no_pwd >= rx_chans) chan_no_pwd = rx_chans - 1;
		int chan_need_pwd = rx_chans - chan_no_pwd;

		if (type_kiwi || type_prot) {
			pwd_s = admcfg_string("user_password", NULL, CFG_REQUIRED);
			bool no_pwd = (pwd_s == NULL || *pwd_s == '\0');
			cfg_auto_login = admcfg_bool("user_auto_login", NULL, CFG_REQUIRED);
			
			// config pwd set, but auto_login for local subnet is true
			if (cfg_auto_login && is_local) {
                nwf_cprintf(conn, "PWD %s %s ALLOWED: config pwd set, but is_local and auto-login set\n",
				    type_m, uri);
				allow = true;
				skip_dup_ip_check = true;
			} else

			// if no user password set allow unrestricted connection
			if (no_pwd) {
				nwf_cprintf(conn, "PWD %s %s ALLOWED: no config pwd set, allow any (%s)\n",
				    type_m, uri, conn->remote_ip);
				allow = true;
			} else
			
			// internal connections always allowed
			if (conn->internal_connection) {
				nwf_cprintf(conn, "PWD %s %s ALLOWED: internal connection\n",
				    type_m, uri);
				allow = true;
				skip_dup_ip_check = true;
			} else
			
			if (!type_prot) {       // consider only if protected mode not requested
			
				int rx_free = rx_chan_free_count(RX_COUNT_ALL);
				
				// Allow with no password if minimum number of channels needing password remains.
				// This minimum number is reduced by the number of already privileged connections.
				// If no password has been set at all we've already allowed access above.
				int already_privileged_conns = rx_count_server_conns(LOCAL_OR_PWD_PROTECTED_USERS, conn);
				chan_need_pwd -= already_privileged_conns;
				if (chan_need_pwd < 0) chan_need_pwd = 0;
				pdbug_cprintf(conn, "PWD %s %s rx_free=%d >= chan_need_pwd=%d %s already_privileged_conns=%d\n", 
				    type_m, uri, rx_free, chan_need_pwd,
				    (rx_free >= chan_need_pwd)? "TRUE":"FALSE", already_privileged_conns);
				
				if (rx_free >= chan_need_pwd) {
					allow = true;
			        //nwf_cprintf(conn, "PWD rx_free=%d >= chan_need_pwd=%d %s\n", rx_free, chan_need_pwd, allow? "TRUE":"FALSE");
				}
			} else {
				nwf_cprintf(conn, "PWD %s %s NEED PWD: protected mode requested\n", type_m, uri);
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
			} else
			
			// when no admin pwd set, display msg when not on same subnet to aid in debugging
			if (no_pwd && !is_local) {
				clprintf(conn, "PWD %s CANT LOGIN: no config pwd set, not is_local\n", type_m);
				cant_login = true;
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
                cant_login = false;
            }
            auth_su = false;        // be certain to reset the global immediately
            memset(auth_su_remote_ip, 0, sizeof(auth_su_remote_ip));
        }
		
		int badp = 1;

		pdbug_cprintf(conn, "PWD %s %s RESULT allow=%d pwd_s=<%s> pwd_m=<%s> cant_determine=%d is_local=%d isLocal(enum)=%d %s\n",
		    type_m, uri, allow, pwd_s, pwd_m, cant_determine, is_local, isLocal, conn->remote_ip);

		if (check_ip_against_restricted && !allow && !restricted_ip_match) {
		    badp = 3;
		} else

		if (cant_determine) {
		    badp = 2;
		} else

		if (cant_login) {
		    badp = 4;
		} else

		if (allow) {
			if (log_auth_attempt || pwd_debug)
				clprintf(conn, "PWD %s %s ALLOWED: from %s\n", type_m, uri, conn->remote_ip);
			badp = 0;
		} else
		
		if ((pwd_s == NULL || *pwd_s == '\0')) {
			clprintf(conn, "PWD %s %s REJECTED: no config pwd set, from %s\n", type_m, uri, conn->remote_ip);
			badp = 1;
		} else {
			if (pwd_m == NULL || pwd_s == NULL) {
                pdbug_cprintf(conn, "PWD %s %s probably auto-login check, from %s\n", type_m, uri, conn->remote_ip);
				badp = 1;
			} else {
				//cprintf(conn, "PWD CMP %s pwd_s \"%s\" pwd_m \"%s\" from %s\n", type_m, pwd_s, pwd_m, conn->remote_ip);
				badp = strcasecmp(pwd_m, pwd_s)? 1:0;
                pdbug_cprintf(conn, "PWD %s %s %s:%s from %s\n", type_m, uri, badp? "REJECTED":"ACCEPTED",
                    type_prot? " (protected login)":"", conn->remote_ip);
                if (!badp) skip_dup_ip_check = true;    // pwd needed and given correctly so allow dup ip
			}
		}
		
        //cprintf(conn, "DUP_IP badp=%d skip_dup_ip_check=%d stream_snd_or_wf=%d\n", badp, skip_dup_ip_check, stream_snd_or_wf);
        //cprintf(conn, "DUP_IP rx%d %s %s\n", conn->rx_channel, uri, conn->remote_ip);
        
        // only SND connection has tlimit_exempt_by_pwd
        if (stream_wf && conn->other && conn->other->tlimit_exempt_by_pwd) skip_dup_ip_check = true;
        
        bool no_dup_ip = admcfg_bool("no_dup_ip", NULL, CFG_REQUIRED);
        bool skip_admin = (type_admin && allow);
		if (no_dup_ip && !skip_admin && badp == 0 && !skip_dup_ip_check && stream_snd_or_wf) {
            conn_t *c = conns;
            //cprintf(conn, "DUP_IP NEW rx%d %s %s\n", conn->rx_channel, uri, conn->remote_ip);
            for (i=0; i < N_CONNS; i++, c++) {
                bool snd_wf = (c->type == STREAM_SOUND || c->type == STREAM_WATERFALL);
                if (!c->valid || c->internal_connection || !snd_wf) continue;
                if (c->rx_channel == conn->rx_channel) continue;    // skip our own entry
                //cprintf(conn, "DUP_IP CHK rx%d %s %s\n", c->rx_channel, rx_streams[c->type].uri, c->remote_ip);
                if (strcmp(conn->remote_ip, c->remote_ip) == 0) {
                    //cprintf(conn, "DUP_IP MATCH\n");
				    badp = 5;
				    break;
                }
            }
        }

		send_msg(conn, false, "MSG rx_chans=%d", rx_chans);
		send_msg(conn, false, "MSG chan_no_pwd=%d", chan_no_pwd);
		if (badp == 0 && (conn->type == STREAM_SOUND || conn->type == STREAM_ADMIN)) {
		    send_msg(conn, false, "MSG is_local=%d,%d", conn->rx_channel, is_local? 1:0);
            conn_nolocal = false;
		    //pdbug_cprintf(conn, "PWD %s %s resetting conn_nolocal\n", type_m, uri);
		}
		send_msg(conn, false, "MSG badp=%d", badp);

        free(pwd_m); free(ipl_m);
		cfg_string_free(pwd_s);
		
		if (badp == 5) conn->kick = true;
		
		// only when the auth validates do we setup the handler
		if (badp == 0) {
		
		    // It's possible for both to be set e.g. auth_kiwi set on initial user connection
		    // then correct admin pwd given later for label edit.
		    
			if (type_kiwi || type_prot) conn->auth_kiwi = true;
			if (type_prot) conn->auth_prot = true;

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
				
				if (stream_snd_or_wf || stream_admin_or_mfg) {
		            send_msg(conn, SM_NO_DEBUG, "MSG version_maj=%d version_min=%d", version_maj, version_min);
		        }

				// send cfg once to javascript
				if (stream_snd || stream_admin_or_mfg)
					rx_server_send_config(conn);
				
				// setup stream task first time it's authenticated
				rx_stream_t *st = &rx_streams[conn->type];
				if (st->setup) (st->setup)((void *) conn);
			}
		}
		pdbug_cprintf(conn, "PWD %s %s AUTH=%d badp=%d auth_kiwi=%d auth_prot=%d auth_admin=%d\n", type_m, uri,
		     conn->auth, badp, conn->auth_kiwi, conn->auth_prot, conn->auth_admin);
        free(type_m);
        
		return true;
	}

	// SECURITY: we accept no incoming commands besides auth and keepalive until auth is successful
	if (conn->auth == false) {
		clprintf(conn, "### SECURITY: NO AUTH YET: %s %d %s <%s>\n", stream_name, conn->type, conn->remote_ip, cmd);
		return true;	// fake that we accepted command so it won't be further processed
	}


////////////////////////////////
// saved config
////////////////////////////////

	if (kiwi_str_begins_with(cmd, "SET save_cfg=")) {
		if (conn->auth_admin == FALSE) {
			lprintf("** attempt to save kiwi config with auth_admin == FALSE! IP %s\n", conn->remote_ip);
			return true;	// fake that we accepted command so it won't be further processed
		}

		char *json = (char *) malloc(strlen(cmd) + SPACE_FOR_NULL); // a little bigger than necessary
		n = sscanf(cmd, "SET save_cfg=%s", json);
		assert(n == 1);
		//printf("SET save_cfg=...\n");
		kiwi_str_decode_inplace(json);
		cfg_save_json(json);
		free(json);
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

		char *json = (char *) malloc(strlen(cmd) + SPACE_FOR_NULL); // a little bigger than necessary
		n = sscanf(cmd, "SET save_adm=%s", json);
		assert(n == 1);
		//printf("SET save_adm=...\n");
		kiwi_str_decode_inplace(json);
		admcfg_save_json(json);
		free(json);
		update_vars_from_config();      // update C copies of vars
		
		return true;
	}


////////////////////////////////
// dx
////////////////////////////////

#ifndef CFG_GPS_ONLY

#define DX_SPACING_ZOOM_THRESHOLD	5
#define DX_SPACING_THRESHOLD_PX		10
		dx_t *dp, *ldp, *upd;

	// SECURITY: should be okay: checks for conn->auth_admin first
	if (kiwi_str_begins_with(cmd, "SET DX_UPD")) {
		if (conn->auth_admin == false) {
			cprintf(conn, "DX_UPD NO ADMIN AUTH %s\n", conn->remote_ip);
			return true;
		}
		
		float freq = 0;
		int gid = -999;
		int low_cut, high_cut, mkr_off, flags, new_len;
		bool need_sort = false;
		flags = 0;

		char *text_m, *notes_m, *params_m;
		text_m = notes_m = params_m = NULL;
		n = sscanf(cmd, "SET DX_UPD g=%d f=%f lo=%d hi=%d o=%d m=%d i=%1024ms n=%1024ms p=%1024ms",
		    &gid, &freq, &low_cut, &high_cut, &mkr_off, &flags, &text_m, &notes_m, &params_m);
        printf("DX_UPD [%s]\n", cmd);
		printf("DX_UPD n=%d #%d %8.2f 0x%x text=<%s> notes=<%s> params=<%s>\n", n, gid, freq, flags, text_m, notes_m, params_m);

		if (n != 2 && n != 9) {
			printf("DX_UPD n=%d ?\n", n);
            free(text_m); free(notes_m); free(params_m);
			return true;
		}
		
		//  gid freq    action
		//  !-1 -1      delete
		//  !-1 !-1     modify
		//  -1  x       add new
		
		// dx.len == 0 only applies when adding first entry to empty list
		if (gid != -1 && dx.len == 0) return true;
		
		bool err = false;
		dx_t *dxp;
		if (gid >= -1 && gid < dx.len) {
			if (n == 2 && gid != -1 && freq == -1) {
				// delete entry by forcing to top of list, then decreasing size by one before save
				cprintf(conn, "DX_UPD %s delete entry #%d\n", conn->remote_ip, gid);
				dxp = &dx.list[gid];
				dxp->freq = 999999;
				new_len = dx.len - 1;
				need_sort = true;
			} else
			if (n != 2) {
				if (gid == -1) {
					// new entry: add to end of list (in hidden slot), then sort will insert it properly
					cprintf(conn, "DX_UPD %s adding new entry\n", conn->remote_ip);
					assert(dx.hidden_used == false);		// FIXME need better serialization
					dxp = &dx.list[dx.len];
					dx.hidden_used = true;
					dx.len++;
					new_len = dx.len;
				    need_sort = true;
				} else {
					// modify entry
					cprintf(conn, "DX_UPD %s modify entry #%d\n", conn->remote_ip, gid);
					dxp = &dx.list[gid];
					new_len = dx.len;
				    if (dxp->freq != freq) {
                        need_sort = true;
                    }
                    else printf("DX_UPD modify but no freq change, so no sort required %f\n", freq);

				}
				dxp->freq = freq;
				dxp->low_cut = low_cut;
				dxp->high_cut = high_cut;
				dxp->offset = mkr_off;
				dxp->flags = flags;
		        dxp->timestamp = utc_time_since_2018() / 60;
				
				// remove trailing 'x' transmitted with text, notes and params fields
				text_m[strlen(text_m)-1] = '\0';
				notes_m[strlen(notes_m)-1] = '\0';
				params_m[strlen(params_m)-1] = '\0';
				
				// can't use kiwi_strdup because free() must be used later on
				free((void *) dxp->ident); dxp->ident = strdup(text_m);
				free((void *) dxp->ident_s); dxp->ident_s = kiwi_str_decode_inplace(strdup(text_m));
				free((void *) dxp->notes); dxp->notes = strdup(notes_m);
				free((void *) dxp->notes_s); dxp->notes_s = kiwi_str_decode_inplace(strdup(notes_m));
				free((void *) dxp->params); dxp->params = strdup(params_m);
			} else {
			    err = true;
			}
		} else {
			printf("DX_UPD: gid %d <> dx.len %d ?\n", gid, dx.len);
			err = true;
		}
		
        //#define TMEAS(x) x
        #define TMEAS(x)

		if (!err) {
            // NB: difference between dx.len and new_len
            dx_prep_list(need_sort, dx.list, dx.len, new_len);
            //printf("DX_UPD after qsort dx.len %d new_len %d top elem f=%.2f\n",
            //	dx.len, new_len, dx.list[dx.len - DX_HIDDEN_SLOT].freq);
                        
            dx.len = new_len;
            TMEAS(u4_t start = timer_ms(); printf("DX_UPD START\n");)
            dx_save_as_json();		// FIXME need better serialization
            TMEAS(u4_t split = timer_ms(); printf("DX_UPD struct -> json, file write in %.3f sec\n", TIME_DIFF_MS(split, start));)

            #if 1
                // NB: Don't need to do the time consuming json re-parse since there are no dxcfg_* write routines that
                // incrementally alter the json struct. The DX_UPD code modifies the dx struct and then regenerates the entire json string
                // and writes it to the file. In this way the dx struct is acting as a proxy for the json struct, except at server start-up
                // when the json struct is walked to construct the initial dx struct.
                // But do need to grow to include a new hidden slot if it was just used by an add.
                //dx_reload();
                if (dx.hidden_used) {
	                dx.list = (dx_t *) kiwi_realloc("dx_list", dx.list, (dx.len + DX_HIDDEN_SLOT) * sizeof(dx_t));
	                memset(&dx.list[dx.len], 0, sizeof(dx_t));
	                dx.hidden_used = false;
                }
                
	            dx.json_up_to_date = false;
            #else
                dx_reload();
            #endif

            TMEAS(u4_t now = timer_ms(); printf("DX_UPD DONE reloaded in %.3f/%.3f sec\n", TIME_DIFF_MS(now, split), TIME_DIFF_MS(now, start));)
            send_msg(conn, false, "MSG request_dx_update");	// get client to request updated dx list
        }

        free(text_m); free(notes_m); free(params_m);
		return true;
	}

	if (kiwi_str_begins_with(cmd, "SET DX_FILTER")) {
	    char *filter_ident_m, *filter_notes_m;
		n = sscanf(cmd, "SET DX_FILTER i=%256ms n=%256ms c=%d w=%d g=%d",
		    &filter_ident_m, &filter_notes_m, &conn->dx_filter_case, &conn->dx_filter_wild, &conn->dx_filter_grep);
		if (n != 5) return true;
        // remove trailing 'x' appended to text strings
        filter_ident_m[strlen(filter_ident_m)-1] = '\0';
        filter_notes_m[strlen(filter_notes_m)-1] = '\0';
        free(conn->dx_filter_ident); conn->dx_filter_ident = kiwi_str_decode_inplace(strdup(filter_ident_m));
        free(conn->dx_filter_notes); conn->dx_filter_notes = kiwi_str_decode_inplace(strdup(filter_notes_m));
        free(filter_ident_m);
        free(filter_notes_m);
        conn->dx_err_preg_ident = conn->dx_err_preg_notes = 0;
        
        // compile regexp
        if (conn->dx_filter_grep) {
            int cflags = conn->dx_filter_case? REG_NOSUB : (REG_ICASE|REG_NOSUB);
            if (conn->dx_has_preg_ident) { regfree(&conn->dx_preg_ident); conn->dx_has_preg_ident = false; }
            if (conn->dx_has_preg_notes) { regfree(&conn->dx_preg_notes); conn->dx_has_preg_notes = false; }
            if (conn->dx_filter_ident && conn->dx_filter_ident[0] != '\0') {
                conn->dx_err_preg_ident = regcomp(&conn->dx_preg_ident, conn->dx_filter_ident, cflags);
                conn->dx_has_preg_ident = !conn->dx_err_preg_ident;
                if (conn->dx_err_preg_ident) printf("regcomp ident %d\n", conn->dx_err_preg_ident);
            }
            if (conn->dx_filter_notes && conn->dx_filter_notes[0] != '\0') {
                conn->dx_err_preg_notes = regcomp(&conn->dx_preg_notes, conn->dx_filter_notes, cflags);
                conn->dx_has_preg_notes = !conn->dx_err_preg_notes;
                if (conn->dx_err_preg_notes) printf("regcomp notes %d\n", conn->dx_err_preg_notes);
            }
        }
        
        //printf("DX_FILTER setup <%s> <%s> case=%d wild=%d grep=%d\n",
        //    conn->dx_filter_ident, conn->dx_filter_notes, conn->dx_filter_case, conn->dx_filter_wild, conn->dx_filter_grep);
        //show_conn("DX FILTER ", conn);
        send_msg(conn, false, "MSG request_dx_update");	// get client to request updated dx list
		return true;
	}


    // Called in two different ways:
    // With 4 params to search for and return labels in the visible area defined by max/min.
    // With 2 params to search for the next label above or below the visible area when label stepping.
    // The search criteria applies in both cases.
    
	if (kiwi_str_begins_with(cmd, "SET MKR")) {
		float min, max, bw;
		int zoom, width, dir = 1;
		int type = sscanf(cmd, "SET MKR min=%f max=%f zoom=%d width=%d", &min, &max, &zoom, &width);
		if (type != 4) {
		    type = sscanf(cmd, "SET MKR dir=%d freq=%f", &dir, &min);
		        if (type != 2) return true;
		} else {
		    bw = max - min;
		}
		
		static bool first = true;
		static int dx_lastx;
		dx_lastx = 0;
		
		if (dx.len == 0) {
            send_msg(conn, false, "MSG mkr=[{\"t\":4}]");    // otherwise last marker won't get cleared
			return true;
		}
		
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        u4_t msec = ts.tv_nsec/1000000;
        // reset appending
		sb = kstr_asprintf(NULL, "[{\"t\":%d,\"s\":%ld,\"m\":%d,\"f\":%d}",
		    type, ts.tv_sec, msec, (conn->dx_err_preg_ident? 1:0) + (conn->dx_err_preg_notes? 2:0));   
		int send = 0;
		
		// bsearch the lower bound for speed with large lists
		dx_t dx_min;

        // DX_SEARCH_WINDOW: when zoomed far-in need to look at wider window since we don't know PB center here
		#define DX_SEARCH_WINDOW 10.0
		dx_min.freq = min + ((type == 2 && dir == 1)? DX_SEARCH_WINDOW : -DX_SEARCH_WINDOW);
		dx_list_first = &dx.list[0];
		dx_list_last = &dx.list[dx.len - DX_HIDDEN_SLOT];   // NB: addr of LAST in list

		dx_t *dp = (dx_t *) bsearch(&dx_min, dx.list, dx.len, sizeof(dx_t), bsearch_freqcomp);
		if (dp == NULL) panic("DX bsearch");
		//printf("DX MKR key=%.2f bsearch=%.2f(%d/%d) min=%.2f max=%.2f\n",
		//    dx_min.freq, dp->freq + ((float) dp->offset / 1000.0), dp->idx, dx.len, min, max);
		
		int dx_filter = 0, fn_flags = 0;
        if (conn->dx_filter_ident || conn->dx_filter_notes) {
            dx_filter = 1;
            fn_flags = conn->dx_filter_case? 0 : FNM_CASEFOLD;
            //printf("DX FILTERING on <%s> <%s> case=%d wild=%d grep=%d\n",
            //    conn->dx_filter_ident, conn->dx_filter_notes, conn->dx_filter_case, conn->dx_filter_wild, conn->dx_filter_grep);
            //show_conn("DX FILTER ", conn);
        }
        
		for (; dp < &dx.list[dx.len] && dp >= dx.list; dp += dir) {
			float freq = dp->freq + ((float) dp->offset / 1000.0);		// carrier plus offset

			if (type == 4 && freq > max + DX_SEARCH_WINDOW) break;    // get extra one above for label stepping
			
			if (dx_filter) {
			    if (conn->dx_filter_grep) {
			        if (conn->dx_has_preg_ident) {
                        if (regexec(&conn->dx_preg_ident, dp->ident_s, 0, NULL, 0) == REG_NOMATCH) continue;
                    }
			        if (conn->dx_has_preg_notes) {
                        if (regexec(&conn->dx_preg_notes, dp->notes_s, 0, NULL, 0) == REG_NOMATCH) continue;
                    }
			        //printf("DX FILTER MATCHED-grep %s<%s> %s<%s>\n",
			        //    conn->dx_has_preg_ident? "*":"", dp->ident_s, conn->dx_has_preg_notes? "*":"", dp->notes_s);
			    } else
			    if (conn->dx_filter_wild) {
                    if (fnmatch(conn->dx_filter_ident, dp->ident_s, fn_flags) != 0) continue;
                    if (conn->dx_filter_notes && conn->dx_filter_notes[0] != '\0' &&
                        fnmatch(conn->dx_filter_notes, dp->notes_s, fn_flags) != 0) continue;
			        //printf("DX FILTER MATCHED-wild <%s> <%s>\n", dp->ident_s, dp->notes_s);
			    } else {
                    if (conn->dx_filter_case) {
                        if (strstr(dp->ident_s, conn->dx_filter_ident) == NULL) continue;
                        if (conn->dx_filter_notes && strstr(dp->notes_s, conn->dx_filter_notes) == NULL) continue;
                    } else {
                        if (strcasestr(dp->ident_s, conn->dx_filter_ident) == NULL) continue;
                        if (conn->dx_filter_notes && strcasestr(dp->notes_s, conn->dx_filter_notes) == NULL) continue;
                    }
			        //printf("DX FILTER MATCHED-no-grep <%s> <%s>\n", dp->ident_s, dp->notes_s);
                }
			}
			
			// reduce dx label clutter
			if (type == 4 && zoom <= DX_SPACING_ZOOM_THRESHOLD) {
				int x = ((dp->freq - min) / bw) * width;
				int diff = x - dx_lastx;
				//printf("DX spacing %d %d %d %s\n", dx_lastx, x, diff, dp->ident);
				if (!first && diff < DX_SPACING_THRESHOLD_PX) continue;
				dx_lastx = x;
				first = false;
			}
			
			// NB: ident, notes and params are already stored URL encoded
			if (type == 4 || dp->freq != min) {
                sb = kstr_asprintf(sb, ",{\"g\":%d,\"f\":%.3f,\"lo\":%d,\"hi\":%d,\"o\":%d,\"b\":%d,\"ts\":%d,\"tg\":%d,\"i\":\"%s\"%s%s%s%s%s%s}",
                    dp->idx, freq, dp->low_cut, dp->high_cut, dp->offset, dp->flags, dp->timestamp, dp->tag, dp->ident,
                    dp->notes? ",\"n\":\"":"", dp->notes? dp->notes:"", dp->notes? "\"":"",
                    dp->params? ",\"p\":\"":"", dp->params? dp->params:"", dp->params? "\"":"");
                //printf("DX %d: %.2f(%d)\n", send, freq, dp->idx);
                send++;
            }
			
			// return the very first we hit that passed the filtering criteria above
			if (type == 2 && send) break;
		}
		
		sb = kstr_cat(sb, "]");
		send_msg(conn, false, "MSG mkr=%s", kstr_sp(sb));
		kstr_free(sb);
		//printf("DX send=%d\n", send);
		return true;
	}
	
	// send the whole database as json
	if (strcmp(cmd, "SET GET_DX_JSON") == 0) {
	    dxcfg_update_json();    // update (re-parse) json if necessary

        // NB: ident, notes and params are already stored URL encoded
        printf("GET_DX_JSON len=%d\n", strlen(cfg_dx.json));
		send_msg(conn, false, "MSG dx_json=%s", cfg_dx.json);
		free(sb);
		return true;
	}

#endif


////////////////////////////////
// status and config
////////////////////////////////

	if (strcmp(cmd, "SET GET_CONFIG") == 0) {
		asprintf(&sb, "{\"r\":%d,\"g\":%d,\"s\":%d,\"pu\":\"%s\",\"pe\":%d,\"pv\":\"%s\",\"pi\":%d,\"n\":%d,\"m\":\"%s\",\"v1\":%d,\"v2\":%d}",
			rx_chans, GPS_CHANS, net.serno, net.ip_pub, net.port_ext, net.ip_pvt, net.port, net.nm_bits, net.mac, version_maj, version_min);
		send_msg(conn, false, "MSG config_cb=%s", sb);
		free(sb);
		return true;
	}
	
	if (kiwi_str_begins_with(cmd, "SET STATS_UPD")) {
		int ch;
		n = sscanf(cmd, "SET STATS_UPD ch=%d", &ch);
		//printf("STATS_UPD ch=%d\n", ch);
		// ch == rx_chans for admin connections (e.g. 4 when ch = 0..3 for user connections)
		if (n != 1 || ch < 0 || ch > rx_chans) return true;

		rx_chan_t *rx;
		int underruns = 0, seq_errors = 0;
		
		for (rx = rx_channels, i=0; rx < &rx_channels[rx_chans]; rx++, i++) {
			if (rx->busy) {
				conn_t *c = rx->conn;
				if (c && c->valid && c->arrived && c->type == STREAM_SOUND && c->user != NULL) {
					underruns += c->audio_underrun;
					seq_errors += c->sequence_errors;
				}
			}
		}
		
		sb = kstr_asprintf(NULL, cpu_stats_buf? "{%s," : "{", kstr_sp(cpu_stats_buf));

		float sum_kbps = audio_kbps[rx_chans] + waterfall_kbps[rx_chans] + http_kbps;
		sb = kstr_asprintf(sb, "\"ac\":%.0f,\"wc\":%.0f,\"fc\":%.0f,\"ah\":%.0f,\"as\":%.0f,\"sr\":%.6f",
			audio_kbps[ch], waterfall_kbps[ch], waterfall_fps[ch], http_kbps, sum_kbps,
			ext_update_get_sample_rateHz(-1));

		sb = kstr_asprintf(sb, ",\"ga\":%d,\"gt\":%d,\"gg\":%d,\"gf\":%d,\"gc\":%.6f,\"go\":%d",
			gps.acquiring, gps.tracking, gps.good, gps.fixes, adc_clock_system()/1e6, clk.adc_gps_clk_corrections);

#ifndef CFG_GPS_ONLY
        //printf("ch=%d ug=%d lat=%d\n", ch, wspr_c.GPS_update_grid, (gps.StatLat != 0));
        if (wspr_c.GPS_update_grid && gps.StatLat) {
            latLon_t loc;
            loc.lat = gps.sgnLat;
            loc.lon = gps.sgnLon;
            char grid6[LEN_GRID];
            if (latLon_to_grid6(&loc, grid6) == 0) {
                //#define TEST_GPS_GRID
                #ifdef TEST_GPS_GRID
                    static int grid_flip;
                    grid6[5] = grid_flip? 'j':'i';
                    if (ch == rx_chans) {
                        grid_flip = grid_flip ^ 1;
                        printf("TEST_GPS_GRID %s\n", grid6);
                    }
                #endif
	            kiwi_strncpy(wspr_c.rgrid, grid6, LEN_GRID);
            }
        }
        
        // Always send WSPR grid. Won't reveal location if grid not set on WSPR admin page
        // and update-from-GPS turned off.
        sb = kstr_asprintf(sb, ",\"gr\":\"%s\"", kiwi_str_encode_static(wspr_c.rgrid));
        //printf("status sending wspr_c.rgrid=<%s>\n", wspr_c.rgrid);
        
		sb = kstr_asprintf(sb, ",\"ad\":%d,\"au\":%d,\"ae\":%d,\"ar\":%d,\"an\":%d,\"an2\":%d,",
			dpump.audio_dropped, underruns, seq_errors, dpump.resets, nrx_bufs, N_DPBUF);
		sb = kstr_cat(sb, kstr_list_int("\"ap\":[", "%u", "],", (int *) dpump.hist, nrx_bufs));
		sb = kstr_cat(sb, kstr_list_int("\"ai\":[", "%u", "]", (int *) dpump.in_hist, N_DPBUF));
#endif

		char utc_s[32], local_s[32];
		time_t utc = utc_time();
		strncpy(utc_s, &utc_ctime_static()[11], 5);
		utc_s[5] = '\0';
		if (utc_offset != -1 && dst_offset != -1) {
			time_t local = utc + utc_offset + dst_offset;
			strncpy(local_s, &var_ctime_static(&local)[11], 5);
			local_s[5] = '\0';
		} else {
			strcpy(local_s, "");
		}
		sb = kstr_asprintf(sb, ",\"tu\":\"%s\",\"tl\":\"%s\",\"ti\":\"%s\",\"tn\":\"%s\"}",
			utc_s, local_s, tzone_id, tzone_name);

		send_msg(conn, false, "MSG stats_cb=%s", kstr_sp(sb));
        //printf("MSG stats_cb=<%s>\n", kstr_sp(sb));
		kstr_free(sb);
		return true;
	}

#ifndef CFG_GPS_ONLY

	if (strcmp(cmd, "SET GET_USERS") == 0) {
		bool include_ip = (conn->type == STREAM_ADMIN);
		sb = rx_users(include_ip);
		send_msg(conn, false, "MSG user_cb=%s", kstr_sp(sb));
		kstr_free(sb);
		return true;
	}

#endif


////////////////////////////////
// UI
////////////////////////////////

	if (kiwi_str_begins_with(cmd, "SET ident_user=")) {
        char *ident_user_m = NULL;
	    sscanf(cmd, "SET ident_user=%256ms", &ident_user_m);
		bool noname = (ident_user_m == NULL || ident_user_m[0] == '\0');
		bool setUserIP = false;
		
		if (conn->mc == NULL) return true;	// we've seen this

		// no name sent, no previous name set: use ip as name
		if (noname && !conn->user) setUserIP = true;

		// no name sent, but ip changed: use ip as name
		if (noname && conn->user && strcmp(conn->user, conn->remote_ip)) setUserIP = true;
		
		// else no name sent, but ip didn't change: do nothing
		
		//cprintf(conn, "SET ident_user=<%s> noname=%d setUserIP=%d\n", ident_user_m, noname, setUserIP);
		
		if (setUserIP) {
			kiwi_str_redup(&conn->user, "user", conn->remote_ip);
			conn->isUserIP = TRUE;
			// printf(">>> isUserIP TRUE: %s:%05d setUserIP=%d noname=%d user=%s <%s>\n",
			// 	conn->remote_ip, conn->remote_port, setUserIP, noname, conn->user, cmd);
		}

        // name sent: save new, replace previous (if any)
		if (!noname) {
			kiwi_str_decode_inplace(ident_user_m);
            char *esc = kiwi_str_escape_HTML(ident_user_m);
            if (esc) {
                free(ident_user_m);
                ident_user_m = esc;
            }
			kiwi_str_redup(&conn->user, "user", ident_user_m);
			conn->isUserIP = FALSE;
			// printf(">>> isUserIP FALSE: %s:%05d setUserIP=%d noname=%d user=%s <%s>\n",
			// 	conn->remote_ip, conn->remote_port, setUserIP, noname, conn->user, cmd);
		}
		
		// ext_api_nchans, if exceeded, overrides tdoa_nchans
		if (conn->ext_api) {
		    int ext_api_ch = cfg_int("ext_api_nchans", NULL, CFG_REQUIRED);
		    if (ext_api_ch == -1) ext_api_ch = rx_chans;      // has never been set
		    int ext_api_users = rx_count_server_conns(EXT_API_USERS);
		    //cprintf(conn, "EXT_API ext_api_users=%d >? ext_api_ch=%d\n", ext_api_users, ext_api_ch);
		    if (ext_api_users > ext_api_ch) {
		        //cprintf(conn, "EXT_API TOO_BUSY %s\n", conn->remote_ip);
		        clprintf(conn, "Non-Kiwi API denied connection: %d/%d %s \"%s\"\n",
		            ext_api_users, ext_api_ch, conn->remote_ip, conn->user);
			    send_msg(conn, SM_NO_DEBUG, "MSG too_busy=%d", ext_api_ch);
		        conn->kick = true;
		    }
		}

		// Can only distinguish the TDoA service at the time the kiwirecorder identifies itself.
		// If a match and the limit is exceeded then kick the connection off immediately.
		// This identification is typically sent right after initial connection is made.
		if (!conn->kick && kiwi_str_begins_with(conn->user, "TDoA_service")) {
		    int tdoa_ch = cfg_int("tdoa_nchans", NULL, CFG_REQUIRED);
		    if (tdoa_ch == -1) tdoa_ch = rx_chans;      // has never been set
		    int tdoa_users = rx_count_server_conns(TDOA_USERS);
		    //cprintf(conn, "TDoA_service tdoa_users=%d >? tdoa_ch=%d\n", tdoa_users, tdoa_ch);
		    if (tdoa_users > tdoa_ch) {
		        //cprintf(conn, "TDoA_service TOO_BUSY\n");
			    send_msg(conn, SM_NO_DEBUG, "MSG too_busy=%d", tdoa_ch);
		        conn->kick = true;
		    }
		}

		free(ident_user_m);
		conn->ident = true;
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
		char *esc = kiwi_str_escape_HTML(geo_m);
		if (esc) {
		    free(geo_m);
		    geo_m = esc;
		}
		free(conn->geo);
		conn->geo = geo_m;
		return true;
	}

	char *geojson_m = NULL;
	n = sscanf(cmd, "SET geojson=%256ms", &geojson_m);
	if (n == 1) {
		kiwi_str_decode_inplace(geojson_m);
		//clprintf(conn, "SET geojson<%s>\n", geojson_m);
		free(geojson_m);
		return true;
	}
	
	char *browser_m = NULL;
	n = sscanf(cmd, "SET browser=%256ms", &browser_m);
	if (n == 1) {
		kiwi_str_decode_inplace(browser_m);
		//clprintf(conn, "SET browser=<%s>\n", browser_m);
		free(browser_m);
		return true;
	}
	
#ifndef CFG_GPS_ONLY
    // used by signal generator etc.
	int wf_comp;
	n = sscanf(cmd, "SET wf_comp=%d", &wf_comp);
	if (n == 1) {
		c2s_waterfall_compression(conn->rx_channel, wf_comp? true:false);
		//printf("### SET wf_comp=%d\n", wf_comp);
		return true;
	}
#endif

    if (strcmp(cmd, "SET inactivity_ack") == 0) {
        conn->last_tune_time = timer_sec();
        return true;
    }


////////////////////////////////
// preferences
////////////////////////////////

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


////////////////////////////////
// kiwiclient
////////////////////////////////

    // DEPRECATED
    // Replaced by "--tlimit-pw" option that specifies an actual time limit exemption password.
    // But silently accept for backward compatibility with old versions of kiwiclient still in use.
	int inactivity_timeout;
	n = sscanf(cmd, "SET OVERRIDE inactivity_timeout=%d", &inactivity_timeout);
	if (n == 1) {
		return true;
	}


////////////////////////////////
// debugging
////////////////////////////////

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


////////////////////////////////
// misc
////////////////////////////////

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

	if (kiwi_str_begins_with(cmd, "SERVER DE CLIENT")) return true;
	
	// we see these sometimes; not part of our protocol
	if (strcmp(cmd, "PING") == 0) return true;

	// we see these at the close of a connection sometimes; not part of our protocol
    #define ASCII_ETX 3
	if (strlen(cmd) == 2 && cmd[0] == ASCII_ETX) return true;
	
	// we see these timestamps sometimes; not part of our protocol
	int y, d, h, m, s;
	if (sscanf(cmd, "%d/%d/%d %d:%d:%d", &y, &n, &d, &h, &m, &s) == 6) {
	    conn->spurious_timestamps_recvd++;
	    return true;
	}

	if (kiwi_str_begins_with(cmd, "SET x-DEBUG")) {
	    cprintf(conn, "x-DEBUG %s \"%s\"\n", conn->remote_ip, &cmd[12]);
	    return true;
	}
	
	return false;
}

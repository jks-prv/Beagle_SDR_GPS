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
#include "rx_cmd.h"
#include "clk.h"
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
#include "debug.h"
#include "ext_int.h"
#include "wspr.h"
#include "security.h"

#ifdef USE_SDR
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
#include <ctype.h>

kstr_t *cpu_stats_buf;
volatile float audio_kbps[MAX_RX_CHANS+1], waterfall_kbps[MAX_RX_CHANS+1], waterfall_fps[MAX_RX_CHANS+1], http_kbps;
volatile u4_t audio_bytes[MAX_RX_CHANS+1], waterfall_bytes[MAX_RX_CHANS+1], waterfall_frames[MAX_RX_CHANS+1], http_bytes;
int debug_v;

const char *mode_s[N_MODE] = {
    "am", "amn", "usb", "lsb", "cw", "cwn", "nbfm", "iq", "drm", "usn", "lsn", "sam", "sau", "sal", "sas", "qam"
};
const char *modu_s[N_MODE] = {
    "AM", "AMN", "USB", "LSB", "CW", "CWN", "NBFM", "IQ", "DRM", "USN", "LSN", "SAM", "SAU", "SAL", "SAS", "QAM"
};
const int mode_hbw[N_MODE] = {
    9800/2, 5000/2, 2400/2, 2400/2, 400/2, 60/2, 12000/2, 10000/2, 10000/2, 2100/2, 2100/2, 9800/2, 9800/2, 9800/2, 9800/2, 9800/2
};
const int mode_offset[N_MODE] = {
    0, 0, 1500, -1500, 0, 0, 0, 0, 0, 1350, -1350, 0, 0, 0, 0, 0
};

#ifdef USE_SDR

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

// if entries here are ordered by rx_common_cmd_key_e then the reverse lookup (str_hash_t *)->hashes[key].name
// will work as a debugging aid
static str_hashes_t rx_common_cmd_hashes[] = {
    { "~~~~~~~~~~", STR_HASH_MISS },
    { "SET keepal", CMD_KEEPALIVE },
    { "SET auth t", CMD_AUTH },
    { "SET option", CMD_OPTIONS },
    { "SET save_c", CMD_SAVE_CFG },
    { "SET save_a", CMD_SAVE_ADM },
    { "SET DX_UPD", CMD_DX_UPD },
    { "SET DX_FIL", CMD_DX_FILTER },
    { "SET MARKER", CMD_MARKER },
    { "SET GET_DX", CMD_GET_DX },
    { "SET DX_SET", CMD_DX_SET },
    { "SET GET_CO", CMD_GET_CONFIG },
    { "SET STATS_", CMD_STATS_UPD },
    { "SET GET_US", CMD_GET_USERS },
    { "SET ident_", CMD_IDENT_USER },
    { "SET need_s", CMD_NEED_STATUS },
    { "SET geoloc", CMD_GEO_LOC },        // kiwiclient still uses "geo="
    { "SET geojso", CMD_GEO_JSON },
    { "SET browse", CMD_BROWSER },
    { "SET wf_com", CMD_WF_COMP },
    { "SET inacti", CMD_INACTIVITY_ACK },
    { "SET notify", CMD_NOTIFY_MSG },
    { "SET pref_e", CMD_PREF_EXPORT },
    { "SET pref_i", CMD_PREF_IMPORT },
    { "SET OVERRI", CMD_OVERRIDE },
    { "SET nocach", CMD_NOCACHE },
    { "SET ctrace", CMD_CTRACE },
    { "SET dbug_v", CMD_DEBUG_VAL },
    { "SET dbug_m", CMD_DEBUG_MSG },
    { "SET is_adm", CMD_IS_ADMIN },
    { "SET get_au", CMD_GET_AUTHKEY },
    { "SET clk_ad", CMD_CLK_ADJ },
    { "SERVER DE ", CMD_SERVER_DE_CLIENT },
    { "SET x-DEBU", CMD_X_DEBUG },
    { 0 }
};

static str_hash_t rx_common_cmd_hash;

void rx_common_init(conn_t *conn)
{
    str_hash_init("rx_common_cmd", &rx_common_cmd_hash, rx_common_cmd_hashes);
	conn->keepalive_time = timer_sec();

    if (is_multi_core)
	    send_msg(conn, false, "MSG is_multi_core");
}

bool rx_common_cmd(const char *stream_name, conn_t *conn, char *cmd)
{
	int i, j, k, n;
	struct mg_connection *mc = conn->mc;
	char *sb;
	int slen;
	bool ok, err;
	
	if (mc == NULL) {
	    //cprintf(conn, "### cmd but mc is null <%s>\n", cmd);
	    return false;
	}
	
	NextTask("rx_common_cmd");      // breakup long runs of sequential commands -- sometimes happens at startup
    evLatency(EC_EVENT, EV_RX, 0, "rx_common_cmd", evprintf("%s", cmd));
	
	// trim trailing '\n' e.g. for commands coming from files etc.
    slen = strlen(cmd);
    if (cmd[slen-1] == '\n') {
        cmd[slen-1] = '\0';
        //printf("--> CMD trim \\n %d<%s>\n", slen, cmd);
    }
    
	// SECURITY: we accept no incoming commands besides auth and keepalive until auth is successful
	if (conn->auth == false &&
	    strcmp(cmd, "SET keepalive") != 0 &&
	    kiwi_str_begins_with(cmd, "SET options") == false &&    // options needed before CMD_AUTH
	    kiwi_str_begins_with(cmd, "SET auth") == false) {
		    clprintf(conn, "### SECURITY: NO AUTH YET: %s %s %s %d <%.64s>\n",
		        stream_name, rx_streams[conn->type].uri, conn->remote_ip, strlen(cmd), cmd);
            conn->kick = true;
		    return true;	// fake that we accepted command so it won't be further processed
	}

    // CAUTION: because rx_common_cmd() can pass cmds it doesn't match there can be false hash matches
    // because not all possible commands are in the rx_common_cmd_hashes[] list.
    // So all cases below still need to do a full string match check.
    
    u2_t key = str_hash_lookup(&rx_common_cmd_hash, cmd);
    if (key == 0) {
        //printf("#### rx_common_cmd(%s): key=0 hash=0x%04x \"%s\"\n",
        //    stream_name, rx_common_cmd_hash.cur_hash, cmd);
        return false;     // signal we didn't handle command
    }
    
	switch (key) {
	
	case CMD_KEEPALIVE:
        if (strcmp(cmd, "SET keepalive") == 0) {
            conn->keepalive_time = timer_sec();

            // for STREAM_EXT send a roundtrip keepalive
            if (conn->type == STREAM_EXT) {
                if (conn->ext_rx_chan == -1) return true;
                ext_send_msg(conn->ext_rx_chan, false, "MSG keepalive");
            }
            
            // for STREAM_ADMIN send a roundtrip keepalive so admin client can tell
            // when server has gone away
            if (conn->type == STREAM_ADMIN) {
                // conn->mc checked for == NULL above
                send_msg(conn, false, "MSG keepalive");
            }

            conn->keepalive_count++;
            return true;
        }
	    break;
	
	case CMD_OPTIONS:
	    u4_t options;
        n = sscanf(cmd, "SET options=%d", &options);
        if (n == 1) {
            //printf("CMD_OPTIONS 0x%x\n", options);
            #define OPT_NOLOCAL 1
            if (options & OPT_NOLOCAL) conn->force_notLocal = true;
            return true;
        }
	    break;
	
	// SECURITY: auth command here is the only one allowed before auth check below (excluding keepalive above)
	case CMD_AUTH:
        if (kiwi_str_begins_with(cmd, "SET auth")) {
    
            const char *pwd_s = NULL;
            bool no_pwd = false;;
            int cfg_auto_login;
            const char *uri = rx_streams[conn->type].uri;
            
            conn->awaitingPassword = true;

            char *type_m = NULL, *pwd_m = NULL, *ipl_m = NULL;
            n = sscanf(cmd, "SET auth t=%16ms p=%256ms ipl=%256ms", &type_m, &pwd_m, &ipl_m);

            if (pwd_m != NULL && strcmp(pwd_m, "#") == 0) {
                kiwi_ifree(pwd_m);
                pwd_m = NULL;       // equivalent to NULL so ipl= can be sscanf()'d properly
            }
        
            //cprintf(conn, "n=%d typem=%s pwd=%s ipl=%s <%s>\n", n, type_m, pwd_m, ipl_m, cmd);
            if ((n != 1 && n != 2 && n != 3) || type_m == NULL) {
                send_msg(conn, false, "MSG badp=1");
                kiwi_ifree(type_m); kiwi_ifree(pwd_m); kiwi_ifree(ipl_m);     // kiwi_ifree(NULL) is okay
                return true;
            }
        
            kiwi_str_decode_inplace(pwd_m);
            kiwi_str_decode_inplace(ipl_m);
            //printf("PWD %s pwd \"%s\" from %s\n", type_m, pwd_m, conn->remote_ip);
        
            bool allow = false, cant_determine = false, cant_login = false, skip_dup_ip_check = false;
            bool type_kiwi = (type_m != NULL && strcmp(type_m, "kiwi") == 0);
            bool type_prot = (type_m != NULL && strcmp(type_m, "prot") == 0);
            bool type_kiwi_prot = (type_kiwi || type_prot);
            bool type_admin = (type_m != NULL && strcmp(type_m, "admin") == 0);
        
            bool stream_wf = (conn->type == STREAM_WATERFALL);
            bool stream_snd = (conn->type == STREAM_SOUND);
            bool stream_snd_or_wf = (stream_snd || stream_wf);
            bool stream_admin_or_mfg = (conn->type == STREAM_ADMIN || conn->type == STREAM_MFG);
            bool stream_mon = (conn->type == STREAM_MONITOR);
            bool stream_ext = (conn->type == STREAM_EXT);
        
            bool bad_type = (stream_snd_or_wf || stream_mon || stream_ext || stream_admin_or_mfg)? false : true;
        
            if ((!type_kiwi && !type_prot && !type_admin) || bad_type) {
                clprintf(conn, "PWD BAD REQ type_m=\"%s\" conn_type=%d from %s\n", type_m, conn->type, conn->remote_ip);
                send_msg(conn, false, "MSG badp=1");
                kiwi_ifree(type_m); kiwi_ifree(pwd_m); kiwi_ifree(ipl_m);
                return true;
            }
        
            // opened admin/mfg url, but then tried type kiwi auth!
            if (stream_admin_or_mfg && !type_admin) {
                clprintf(conn, "PWD BAD TYPE MIX type_m=\"%s\" conn_type=%d from %s\n", type_m, conn->type, conn->remote_ip);
                send_msg(conn, false, "MSG badp=1");
                kiwi_ifree(type_m); kiwi_ifree(pwd_m); kiwi_ifree(ipl_m);
                return true;
            }
        
            bool check_ip_against_restricted = true;
            bool check_ip_against_interfaces = true;
            bool restricted_ip_match = false;
            isLocal_t isLocal = IS_NOT_LOCAL;
            bool is_local = false;
            bool is_password = false;
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
                log_auth_attempt = (stream_admin_or_mfg || stream_mon || (stream_ext && type_admin));
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
            
                // SECURITY: call isLocal_if_ip() using mc->remote_ip [via ip_remote(mc)] NOT conn->remote_ip
                // because the latter could be spoofed via X-Real-IP / X-Forwarded-For to look like a local address.
                // For a non-local connection mc->remote_ip is 127.0.0.1 when the frp proxy is used
                // so it will never be considered a local connection.
                isLocal = isLocal_if_ip(conn, ip_remote(mc), (log_auth_attempt || pwd_debug)? "PWD" : NULL);

                if (conn->force_notLocal) {
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

            pdbug_cprintf(conn, "PWD %s %s conn #%d force_notLocal=%d isLocal=%d is_local=%d auth=%d auth_kiwi=%d auth_prot=%d auth_admin=%d check_ip_against_restricted=%d restricted_ip_match=%d from %s\n",
                type_m, uri, conn->self_idx, conn->force_notLocal, isLocal, is_local,
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
                    cprintf(conn, "TLIMIT-IP connecting 24hr LIMIT EXCEEDED cur:%d >= lim:%d for %s\n", ipl_cur_mins, ip_limit_mins, conn->remote_ip);
                    send_msg_mc_encoded(mc, "MSG", "ip_limit", "%d,%s", ip_limit_mins, conn->remote_ip);
                    kiwi_ifree(type_m); kiwi_ifree(pwd_m); kiwi_ifree(ipl_m);
                    conn->tlimit_zombie = true;
                    
                    #define TOO_MANY_ATTEMPTS 2
                    #define WITHIN_THIS_MANY_MINUTES 30
                    u4_t last = json_default_int(&cfg_ipl, stprintf("%s_last", conn->remote_ip), 0, NULL);
                    
                    #if 0
                        u4_t last_time = last >> 3;
                        u4_t retries = last & 7;
                        u4_t now = timer_sec();
                        u4_t since_last_time = now - last_time;
                        bool too_frequent = (since_last_time < MINUTES_TO_SEC(WITHIN_THIS_MANY_MINUTES) && last_time != 0);
                        if (too_frequent) retries = CLAMP(retries+1, 0, TOO_MANY_ATTEMPTS);
                        last = (now << 3) | retries;
                        cprintf(conn, "TLIMIT-IP retries=%d since_last_time=%d too_frequent=%d\n", retries, since_last_time, too_frequent);
                    #else
                        u4_t retries = last;
                        retries++;
                        last = retries;
                        cprintf(conn, "TLIMIT-IP retries=%d\n", retries);
                    #endif
                    
                    json_set_int(&cfg_ipl, stprintf("%s_last", conn->remote_ip), last);
                    if (retries == TOO_MANY_ATTEMPTS) {
                        cprintf(conn, "TLIMIT-IP RETRIES EXCEEDED, adding to blacklist: %s\n", conn->remote_ip);
                        ip_blacklist_add_iptables(conn->remote_ip);
                    }
                    
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

            #ifdef CRYPT_PW
                int seq;
                char *salt = NULL, *hash = NULL;
            #endif

            if (type_kiwi_prot) {
                pwd_s = admcfg_string("user_password", NULL, CFG_REQUIRED);
                no_pwd = (pwd_s == NULL || *pwd_s == '\0');

                #ifdef CRYPT_PW
                    if (!kiwi_crypt_file_read(DIR_CFG "/.eup", &seq, &salt, &hash)) {
                        printf("### DANGER: User password file not found or corrupted! Defaulting to no user password set.\n");
                        admcfg_set_string("user_password", "");
		                admcfg_save_json(cfg_adm.json);     // DANGER: this can collide with admin cfg write!
		                system("rm -f " DIR_CFG "/.eup");
                        no_pwd = true;
                    }
                #endif

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
                    int chan_need_pwd = rx_chans - rx_chan_no_pwd() - already_privileged_conns;
                    if (chan_need_pwd < 0) chan_need_pwd = 0;
                    
                    // NB: ">=" not ">" because channel already allocated (freed if this check fails)
                    pdbug_cprintf(conn, "PWD %s %s rx_free=%d >= chan_need_pwd=%d %s rx_chans=%d rx_chan_no_pwd()=%d already_privileged_conns=%d stream_mon=%d\n", 
                        type_m, uri, rx_free, chan_need_pwd,
                        (rx_free >= chan_need_pwd)? "TRUE":"FALSE", rx_chans, rx_chan_no_pwd(), already_privileged_conns, stream_mon);

                    // always allow STREAM_MONITOR for the case when channels could become available
                    if (rx_free >= chan_need_pwd || stream_mon) {
                        allow = true;
                        //nwf_cprintf(conn, "PWD rx_free=%d >= chan_need_pwd=%d %s\n", rx_free, chan_need_pwd, allow? "TRUE":"FALSE");
                    }
                } else {
                    nwf_cprintf(conn, "PWD %s %s NEED PWD: protected mode requested\n", type_m, uri);
                }
            } else
        
            if (type_admin) {
                pwd_s = admcfg_string("admin_password", NULL, CFG_OPTIONAL);
                no_pwd = (pwd_s == NULL || *pwd_s == '\0');

                #ifdef CRYPT_PW
                    if (!kiwi_crypt_file_read(DIR_CFG "/.eap", &seq, &salt, &hash)) {
                        printf("### DANGER: Admin password file not found or corrupted! Defaulting to no admin password set.\n");
                        admcfg_set_string("admin_password", "");
		                admcfg_save_json(cfg_adm.json);     // DANGER: this can collide with admin cfg write!
		                system("rm -f " DIR_CFG "/.eap");
                        no_pwd = true;
                    }
                #endif

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
        
            int badp = 1;

            pdbug_cprintf(conn, "PWD %s %s RESULT: allow=%d pwd_s=<%s> pwd_m=<%s> cant_determine=%d is_local=%d isLocal(enum)=%d %s\n",
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

                    #ifdef CRYPT_PW
                        badp = kiwi_crypt_validate(pwd_m, salt, hash)? 0:1;
                    #else
                        badp = strcasecmp(pwd_m, pwd_s)? 1:0;
                    #endif

                    pdbug_cprintf(conn, "PWD %s %s %s:%s from %s\n", type_m, uri, badp? "REJECTED":"ACCEPTED",
                        type_prot? " (protected login)":"", conn->remote_ip);
                    if (!badp) {
                        if (type_kiwi_prot) is_password = true;     // password used to get in
                        skip_dup_ip_check = true;    // pwd needed and given correctly so allow dup ip
                    }
                }
            }
        
            #ifdef CRYPT_PW
                kiwi_ifree(salt); kiwi_ifree(hash);
            #endif
            
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
            send_msg(conn, false, "MSG chan_no_pwd=%d", rx_chan_no_pwd());  // potentially corrected from cfg.chan_no_pwd
            send_msg(conn, false, "MSG chan_no_pwd_true=%d", rx_chan_no_pwd(PWD_CHECK_YES));
            if (badp == 0 && (stream_snd || conn->type == STREAM_ADMIN)) {
                send_msg(conn, false, "MSG is_local=%d,%d", conn->rx_channel, is_local? 1:0);
                //pdbug_cprintf(conn, "PWD %s %s\n", type_m, uri);
            }
            send_msg(conn, false, "MSG max_camp=%d", N_CAMP);
            send_msg(conn, false, "MSG badp=%d", badp);

            kiwi_ifree(pwd_m); kiwi_ifree(ipl_m);
            cfg_string_free(pwd_s);
        
            if (badp == 5) conn->kick = true;
        
            // only when the auth validates do we setup the handler
            if (badp == 0) {
        
                // It's possible for both to be set e.g. auth_kiwi set on initial user connection
                // then correct admin pwd given later for label edit.
            
                if (type_kiwi_prot) conn->auth_kiwi = true;
                if (type_prot) conn->auth_prot = true;

                if (type_admin) {
                    conn->auth_admin = true;
                    int chan = conn->rx_channel;

                    // give admin auth to all associated conns of this rx_channel
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
                    conn->isPassword = is_password;
                
                    if (stream_snd_or_wf || stream_admin_or_mfg) {
                        send_msg(conn, SM_NO_DEBUG, "MSG version_maj=%d version_min=%d debian_ver=%d", version_maj, version_min, debian_ver);
                    }

                    // send cfg once to javascript
                    if (stream_snd || stream_mon || stream_admin_or_mfg)
                        rx_server_send_config(conn);
                
                    // setup stream task first time it's authenticated
                    rx_stream_t *st = &rx_streams[conn->type];
                    if (st->setup) (st->setup)((void *) conn);
                }

                conn->awaitingPassword = false;
            }

            pdbug_cprintf(conn, "PWD %s %s AUTH=%d badp=%d auth_kiwi=%d auth_prot=%d auth_admin=%d\n", type_m, uri,
                 conn->auth, badp, conn->auth_kiwi, conn->auth_prot, conn->auth_admin);
            kiwi_ifree(type_m);
        
            return true;
	    }
	    break;


////////////////////////////////
// saved config
////////////////////////////////

    case CMD_SAVE_CFG:
        if (kiwi_str_begins_with(cmd, "SET save_cfg=")) {
            if (conn->auth_admin == FALSE) {
                lprintf("** attempt to save kiwi config with auth_admin == FALSE! IP %s\n", conn->remote_ip);
                return true;	// fake that we accepted command so it won't be further processed
            }
            
            // To prevent cfg database multi-writer data loss, enforce no admin connections (a source of writes)
            // on any non-admin/mfg (i.e. user) connection cfg save.
            if (conn->type != STREAM_ADMIN && conn->type != STREAM_MFG && (n = rx_count_server_conns(ADMIN_USERS)) != 0) {
                //cprintf(conn, "CMD_SAVE_CFG: abort because admin_users=%d\n", n);
                send_msg(conn, false, "MSG no_admin_conns");    // tell user their cfg save was rejected
                rx_server_send_config(conn);    // and reload last saved config to flush bad values
                return true;
            }

            char *json = (char *) kiwi_imalloc("CMD_SAVE_CFG", strlen(cmd) + SPACE_FOR_NULL); // a little bigger than necessary
            n = sscanf(cmd, "SET save_cfg=%s", json);
            assert(n == 1);
            
            // For cfg strings double URI encoding is effectively used since they are stored encoded and
            // another encoding is done for transmission.
            // So decode the transmission encoding with kiwi_str_decode_inplace()
            // and then decode the cfg string encoding with kiwi_str_decode_selective_inplace().
            //cprintf(conn, "SET save_cfg=...\n");
            kiwi_str_decode_inplace(json);
            kiwi_str_decode_selective_inplace(json);
            cfg_save_json(json);
            kiwi_ifree(json);
            update_vars_from_config();      // update C copies of vars

            return true;
        }
	    break;

    case CMD_SAVE_ADM:
        if (kiwi_str_begins_with(cmd, "SET save_adm=")) {
            if (conn->type != STREAM_ADMIN) {
                lprintf("** attempt to save admin config from non-STREAM_ADMIN! IP %s\n", conn->remote_ip);
                return true;	// fake that we accepted command so it won't be further processed
            }
    
            if (conn->auth_admin == FALSE) {
                lprintf("** attempt to save admin config with auth_admin == FALSE! IP %s\n", conn->remote_ip);
                return true;	// fake that we accepted command so it won't be further processed
            }

            char *json = (char *) kiwi_imalloc("CMD_SAVE_ADM", strlen(cmd) + SPACE_FOR_NULL); // a little bigger than necessary
            n = sscanf(cmd, "SET save_adm=%s", json);
            assert(n == 1);

            // For cfg strings double URI encoding is effectively used since they are stored encoded and
            // another encoding is done for transmission.
            // So decode the transmission encoding with kiwi_str_decode_inplace()
            // and then decode the cfg string encoding with kiwi_str_decode_selective_inplace().
            //printf("SET save_adm=...\n");
            kiwi_str_decode_inplace(json);
            kiwi_str_decode_selective_inplace(json);
            admcfg_save_json(json);
            kiwi_ifree(json);
            update_vars_from_config();      // update C copies of vars
        
            return true;
        }
	    break;


////////////////////////////////
// dx
////////////////////////////////

#ifdef USE_SDR

#define DX_SPACING_ZOOM_THRESHOLD	5
#define DX_SPACING_THRESHOLD_PX		10

#define DX_PRINT
#ifdef DX_PRINT
	#define DX_PRINT_DOW_TIME 1
	#define dx_print_dow_time(cond, fmt, ...) \
		if ((dx_print & DX_PRINT_DOW_TIME) && (cond)) cprintf(conn, fmt, ## __VA_ARGS__)

	#define DX_PRINT_MKRS 2
	#define dx_print_mkrs(cond, fmt, ...) \
		if ((dx_print & DX_PRINT_MKRS) && (cond)) cprintf(conn, fmt, ## __VA_ARGS__)

	#define DX_PRINT_ADM_MKRS 4
	#define dx_print_adm_mkrs(fmt, ...) \
		if (dx_print & DX_PRINT_ADM_MKRS) cprintf(conn, fmt, ## __VA_ARGS__)

	#define DX_PRINT_UPD 8
	#define dx_print_upd(fmt, ...) \
		if (dx_print & DX_PRINT_UPD) cprintf(conn, fmt, ## __VA_ARGS__)

	#define DX_PRINT_SEARCH 16
	#define dx_print_search(cond, fmt, ...) \
		if ((dx_print & DX_PRINT_SEARCH) && (cond)) cprintf(conn, fmt, ## __VA_ARGS__)
#else
	#define dx_print_dow_time(cond, fmt, ...)
	#define dx_print_mkrs(cond, fmt, ...)
	#define dx_print_adm_mkrs(fmt, ...)
	#define dx_print_upd(fmt, ...)
	#define dx_print_search(cond, fmt, ...)
#endif

    dx_t *dp, *ldp, *upd;

	// SECURITY: should be okay: checks for conn->auth_admin first
    case CMD_DX_UPD:
        if (kiwi_str_begins_with(cmd, "SET DX_UPD")) {
            if (conn->auth_admin == false) {
                cprintf(conn, "DX_UPD NO ADMIN AUTH %s\n", conn->remote_ip);
                return true;
            }
        
            int chan = (conn->type == STREAM_ADMIN)? MAX_RX_CHANS : conn->rx_channel;
            dx_rx_t *drx = &dx.dx_rx[chan];
            float freq = 0;
            int gid = -999;
            int low_cut, high_cut, mkr_off, flags, begin, end, new_len;
            flags = 0;

            char *text_m, *notes_m, *params_m;
            text_m = notes_m = params_m = NULL;
            n = sscanf(cmd, "SET DX_UPD g=%d f=%f lo=%d hi=%d o=%d fl=%d b=%d e=%d i=%1024ms n=%1024ms p=%1024ms",
                &gid, &freq, &low_cut, &high_cut, &mkr_off, &flags, &begin, &end, &text_m, &notes_m, &params_m);
            enum { DX_MOD_ADD = 11, DX_DEL = 2 };
            
            #if 0
                cprintf(conn, "DX_UPD [%s]\n", cmd);
                int type;
                if (n == DX_MOD_ADD) {
                    const char * dx_mod_add_s[] = { "MOD", "ADD", "???" };
                    if (gid != -1 && freq != -1) type = 0;
                    else
                    if (gid == -1) type = 1; else type = 2;
                    cprintf(conn, "DX_UPD %s: n=%d #%d %8.2f %s lo=%d hi=%d off=%d flags=0x%x b=%d e=%d text=<%s> notes=<%s> params=<%s>\n",
                        dx_mod_add_s[type], n, gid, freq, mode_s[flags & DX_MODE], low_cut, high_cut, mkr_off, flags, begin, end,
                        text_m, notes_m, params_m);
                } else {
                    const char * dx_del_s[] = { "DEL", "???" };
                    if (gid != -1 && freq == -1) type = 0; else type = 1;
                    cprintf(conn, "DX_UPD %s: n=%d #%d\n", dx_del_s[type], n, gid);
                }
            #endif

            if (n != DX_MOD_ADD && n != DX_DEL) {
                cprintf(conn, "DX_UPD n=%d ?\n", n);
                kiwi_ifree(text_m); kiwi_ifree(notes_m); kiwi_ifree(params_m);
                return true;
            }
            int func = n;
            
            if (drx->db != DB_STORED) {
                cprintf(conn, "#### DANGER: CMD_DX_UPD with drx->db != DB_STORED, IGNORED!!!\n");
                return true;
            }
        
            // To prevent dx database multi-writer data loss, enforce no admin connections (a source of writes)
            // on any user connection (with admin pwd given) dx save.
            if (conn->type != STREAM_ADMIN && conn->type != STREAM_MFG && (n = rx_count_server_conns(ADMIN_USERS)) != 0) {
                dx_print_upd("CMD_DX_UPD: abort because admin_users=%d\n", n);
                send_msg(conn, false, "MSG no_admin_conns");    // tell user their dx save was rejected
                send_msg(conn, false, "MSG request_dx_update");	// get client to request updated dx list to flush bad values
                return true;
            }

            //  gid freq    action
            //  !-1 -1      delete
            //  !-1 !-1     modify
            //  -1  x       add new
        
            // dx.stored_len == 0 only applies when adding first entry to empty list
            if (gid != -1 && dx.stored_len == 0) return true;
        
            err = false;
            bool need_sort = false;
            dx_t *dxp;
            if (gid >= -1 && gid < dx.stored_len) {
                if (func == DX_DEL && gid != -1 && freq == -1) {
                    // delete entry by forcing to top of list, then decreasing size by one before sort and save
                    dx_print_upd("DX_UPD %s delete entry #%d\n", conn->remote_ip, gid);
                    dxp = &dx.stored_list[gid];
                    dxp->freq = 999999;
                    new_len = dx.stored_len - 1;
                    need_sort = true;
                } else
                if (func == DX_MOD_ADD) {
                    if (gid == -1) {
                        // new entry: add to end of list (in hidden slot), then sort will insert it properly
                        dx_print_upd("DX_UPD %s adding new entry, freq=%.2f\n", conn->remote_ip, freq);
                        assert(dx.hidden_used == false);		// FIXME need better serialization
                        dxp = &dx.stored_list[dx.stored_len];
                        dx.hidden_used = true;
                        dx.stored_len++;
                        new_len = dx.stored_len;
                        need_sort = true;
                    } else {
                        // modify entry
                        dx_print_upd("DX_UPD %s modify entry #%d, freq=%.2f\n", conn->remote_ip, gid, freq);
                        dxp = &dx.stored_list[gid];
                        new_len = dx.stored_len;
                        if (dxp->freq != freq) {
                            need_sort = true;
                            dx_print_upd("DX_UPD modify, freq change, sort required (old freq=%.2f)\n", dxp->freq);
                        } else {
                            dx_print_upd("DX_UPD modify no freq change, no sort required\n");
                        }
                        if (begin == 0 && end == 0) {
                            dx_print_upd("$DX_UPD why is b=e=0? idx=%d f=%.2f\n", gid, freq);
                            end = 2400;
                        }
                    }
                    dxp->freq = freq;
                    dxp->low_cut = low_cut;
                    dxp->high_cut = high_cut;
                    dxp->offset = mkr_off;
                    dxp->flags = flags;
                    dxp->time_begin = begin;
                    dxp->time_end = end;
                    //dxp->timestamp = utc_time_since_2018() / 60;
                
                    // remove trailing 'x' transmitted with text, notes and params fields
                    text_m[strlen(text_m)-1] = '\0';
                    notes_m[strlen(notes_m)-1] = '\0';
                    params_m[strlen(params_m)-1] = '\0';
                
                    // can't use kiwi_strdup because kiwi_ifree() must be used later on
                    kiwi_ifree((void *) dxp->ident); dxp->ident = strdup(text_m);
                    kiwi_ifree((void *) dxp->ident_s); dxp->ident_s = kiwi_str_decode_inplace(strdup(text_m));
                    kiwi_ifree((void *) dxp->notes); dxp->notes = strdup(notes_m);
                    kiwi_ifree((void *) dxp->notes_s); dxp->notes_s = kiwi_str_decode_inplace(strdup(notes_m));
                    kiwi_ifree((void *) dxp->params); dxp->params = strdup(params_m);
                } else {
                    err = true;
                }
            } else {
                dx_print_upd("DX_UPD: gid %d <> dx.stored_len %d ?\n", gid, dx.stored_len);
                err = true;
            }
        
            //#define TMEAS(x) x
            #define TMEAS(x)

            if (!err) {
                // NB: difference between dx.stored_len and new_len
                dx_prep_list(need_sort, dx.stored_list, dx.stored_len, new_len);
                dx_print_upd("DX_UPD after qsort dx.stored_len=%d new_len=%d diff=%d last entry f=%.2f\n",
                	dx.stored_len, new_len, new_len - dx.stored_len, dx.stored_list[dx.stored_len - DX_HIDDEN_SLOT].freq);
                        
                dx.stored_len = new_len;
                TMEAS(u4_t start = timer_ms(); cprintf(conn, "DX_UPD START\n");)
                dx_save_as_json();		// FIXME need better serialization
                TMEAS(u4_t split = timer_ms(); cprintf(conn, "DX_UPD struct -> json, file write in %.3f sec\n", TIME_DIFF_MS(split, start));)

                #define DX_NO_REPARSE_JSON
                #ifdef DX_NO_REPARSE_JSON
                    // NB: Don't need to do the time consuming json re-parse since there are no dxcfg_* write routines that
                    // incrementally alter the json struct. The DX_UPD code modifies the dx struct and then regenerates the entire json string
                    // and writes it to the file. In this way the dx struct is acting as a proxy for the json struct, except at server start-up
                    // when the json struct is walked to construct the initial dx struct.
                    // But do need to grow to include a new hidden slot if it was just used by an add.
                    if (dx.hidden_used) {
                        //dx.stored_list = (dx_t *) kiwi_realloc("dx_list", dx.stored_list, (dx.stored_len + DX_HIDDEN_SLOT) * sizeof(dx_t));
                        //memset(&dx.stored_list[dx.stored_len], 0, sizeof(dx_t));
                        bool need_more = (dx.stored_len + DX_HIDDEN_SLOT > dx.stored_alloc_size);
                        dx_print_upd("DX_UPD hidden_used stored_len=%d stored_alloc_size=%d realloc=%s\n",
                            dx.stored_len, dx.stored_alloc_size, need_more? "T":"F");
                        if (need_more) {
                            dx.stored_list = (dx_t *) kiwi_table_realloc("dx_list", dx.stored_list, dx.stored_alloc_size, DX_LIST_ALLOC_CHUNK, sizeof(dx_t));
                            dx.stored_alloc_size += DX_LIST_ALLOC_CHUNK;
                        }
                        dx.hidden_used = false;
                    }
                
                    dx.json_up_to_date = false;
                #else
                    //dx_reload();
                #endif

                TMEAS(u4_t now = timer_ms(); cprintf(conn, "DX_UPD DONE reloaded in %.3f(%.3f total) sec\n", TIME_DIFF_MS(now, split), TIME_DIFF_MS(now, start));)
                send_msg(conn, false, "MSG request_dx_update");	// get client to request updated dx list
            }

            kiwi_ifree(text_m); kiwi_ifree(notes_m); kiwi_ifree(params_m);
            return true;
        }
	    break;

    case CMD_DX_FILTER:
        if (kiwi_str_begins_with(cmd, "SET DX_FILTER")) {
            char *filter_ident_m, *filter_notes_m;
            n = sscanf(cmd, "SET DX_FILTER i=%256ms n=%256ms c=%d w=%d g=%d",
                &filter_ident_m, &filter_notes_m, &conn->dx_filter_case, &conn->dx_filter_wild, &conn->dx_filter_grep);
            if (n != 5) return true;

            // remove trailing 'x' appended to text strings
            filter_ident_m[strlen(filter_ident_m)-1] = '\0';
            filter_notes_m[strlen(filter_notes_m)-1] = '\0';
            kiwi_ifree(conn->dx_filter_ident); conn->dx_filter_ident = kiwi_str_decode_inplace(strdup(filter_ident_m));
            kiwi_ifree(conn->dx_filter_notes); conn->dx_filter_notes = kiwi_str_decode_inplace(strdup(filter_notes_m));
            kiwi_ifree(filter_ident_m);
            kiwi_ifree(filter_notes_m);
            conn->dx_err_preg_ident = conn->dx_err_preg_notes = 0;
        
            // compile regexp
            if (conn->dx_filter_grep) {
                int cflags = conn->dx_filter_case? REG_NOSUB : (REG_ICASE|REG_NOSUB);
                if (conn->dx_has_preg_ident) { regfree(&conn->dx_preg_ident); conn->dx_has_preg_ident = false; }
                if (conn->dx_has_preg_notes) { regfree(&conn->dx_preg_notes); conn->dx_has_preg_notes = false; }
                if (conn->dx_filter_ident && conn->dx_filter_ident[0] != '\0') {
                    conn->dx_err_preg_ident = regcomp(&conn->dx_preg_ident, conn->dx_filter_ident, cflags);
                    conn->dx_has_preg_ident = !conn->dx_err_preg_ident;
                    if (conn->dx_err_preg_ident) cprintf(conn, "regcomp ident %d\n", conn->dx_err_preg_ident);
                }
                if (conn->dx_filter_notes && conn->dx_filter_notes[0] != '\0') {
                    conn->dx_err_preg_notes = regcomp(&conn->dx_preg_notes, conn->dx_filter_notes, cflags);
                    conn->dx_has_preg_notes = !conn->dx_err_preg_notes;
                    if (conn->dx_err_preg_notes) cprintf(conn, "regcomp notes %d\n", conn->dx_err_preg_notes);
                }
            }
        
            //cprintf(conn, "DX_FILTER setup <%s> <%s> case=%d wild=%d grep=%d\n",
            //    conn->dx_filter_ident, conn->dx_filter_notes, conn->dx_filter_case, conn->dx_filter_wild, conn->dx_filter_grep);
            //show_conn("DX FILTER ", conn);
            send_msg(conn, false, "MSG request_dx_update");	// get client to request updated dx list
            return true;
        }
	    break;


    // Called in many different ways:
    // With 8 params to search for and return labels in the visible area defined by max/min freq.
    // With 3 params to search for the next label above or below the visible area when label stepping.
    // With 2 params giving the min/max index of the dx table to supply to the admin DX tab.
    // With params associated with dx list searching.
    // Returning counts of the number of types used.
    
    case CMD_MARKER:
        if (kiwi_str_begins_with(cmd, "SET MARKER")) {
            int chan = (conn->type == STREAM_ADMIN)? MAX_RX_CHANS : conn->rx_channel;
            dx_rx_t *drx = &dx.dx_rx[chan];
            float freq, min, max, bw;
            int db, zoom, width, dir = 1, filter_tod, anti_clutter;
            int idx1, idx2;
            u4_t eibi_types_mask;
            char *ident = NULL, *notes = NULL;
            bool db_changed = false;
            //if (drx->db == DB_EiBi) cprintf(conn, "DX_MKR [%s]\n", cmd);

            // values for compatibility with client side
            enum { DX_ADM_MKRS = 0, DX_ADM_SEARCH_F = 1, DX_STEP = 2, DX_ADM_SEARCH_IDENT = 3, DX_MKRS = 4, DX_ADM_SEARCH_NOTES = 5 };
            int func;

            if (sscanf(cmd, "SET MARKER db=%d min=%f max=%f zoom=%d width=%d eibi_types_mask=0x%x filter_tod=%d anti_clutter=%d",
                &db, &min, &max, &zoom, &width, &eibi_types_mask, &filter_tod, &anti_clutter) == 8) {
                bw = max - min;
                func = DX_MKRS;
            } else
            if (sscanf(cmd, "SET MARKER db=%d dir=%d freq=%f", &db, &dir, &min) == 3) {
                func = DX_STEP;
            } else
            if (sscanf(cmd, "SET MARKER idx1=%d idx2=%d", &idx1, &idx2) == 2) {
                func = DX_ADM_MKRS;
                db = DB_STORED;
            } else
            if (sscanf(cmd, "SET MARKER search_freq=%f", &freq) == 1) {
                func = DX_ADM_SEARCH_F;
                db = DB_STORED;
            } else
            if (sscanf(cmd, "SET MARKER idx=%d search_ident=%64ms", &idx1, &ident) == 2) {
                func = DX_ADM_SEARCH_IDENT;
                db = DB_STORED;
            } else
            if (sscanf(cmd, "SET MARKER idx=%d search_notes=%64ms", &idx1, &notes) == 2) {
                func = DX_ADM_SEARCH_NOTES;
                db = DB_STORED;
            } else {
                cprintf(conn, "CMD_MARKER: unknown varient [%s]\n", cmd);
                return true;
            }
            
            bool db_stored = (db == DB_STORED);
            bool db_eibi = (db == DB_EiBi);
            static bool first = true;
            int dx_lastx;
            dx_lastx = 0;
        
            if (db != drx->db) {
                dx_db_e new_db = db_stored? DB_STORED : (db_eibi? DB_EiBi : DB_STORED);
                drx->db = new_db;
                if (drx->db == DB_EiBi && !dx.eibi_init) {
                    dx_eibi_init();
                    dx.eibi_init = true;
                }
                first = true;
                //cprintf(conn, "DX_MKR: db SWITCHED to %s\n", db_stored? "stored" : "EiBi");
            }
            //cprintf(conn, "DX_MKR: SET MARKER db=%s func=%d admin=%d\n",
            //    db_stored? "stored" : "EiBi", func, conn->type == STREAM_ADMIN);
        
            if (func == DX_ADM_SEARCH_F) {
                dx_t dx_min;
                dx_min.freq = freq;
                dx_t *cur_list = dx.stored_list;
                int cur_len = dx.stored_len;
                dx_list_first = &cur_list[0];
                dx_list_last = &cur_list[cur_len];      // NB: addr of LAST+1 in list
                dx_t *dp = (dx_t *) bsearch(&dx_min, cur_list, cur_len, sizeof(dx_t), bsearch_freqcomp);
                dx_print_search(true, "DX_ADM_SEARCH_F %.2f found #%d %.2f\n", freq, dp->idx, dp->freq);
                send_msg(conn, false, "MSG mkr_search_pos=0,%d", dp->idx);
                return true;
            } else
            
            if (func == DX_ADM_SEARCH_IDENT) {
                int pos = -1;
                if (ident != NULL && ident[0] != '\0') {
                    kiwi_str_decode_inplace(ident);
                    dx_t *cur_list = dx.stored_list;
                    int cur_len = dx.stored_len;
                    idx1 = CLAMP(idx1, 0, cur_len - 1);

                    // search starting from given index
                    dx_t *dp = &cur_list[idx1];
                    for (i = idx1; dp < &cur_list[cur_len]; i++, dp++) {
                        dx_print_search(true, "DX_ADM_SEARCH_IDENT LOOP1 <%s> <%s> #%d\n", dp->ident_s, ident, i);
                        if (strcasestr(dp->ident_s, ident) != NULL) {
                            pos = i;
                            break;
                        }
                    }

                    // if not found, try searching full list
                    if (pos == -1) {
                        dx_t *dp = &cur_list[0];
                        for (i = 0; dp < &cur_list[cur_len]; i++, dp++) {
                            dx_print_search(true, "DX_ADM_SEARCH_IDENT LOOP2 <%s> <%s> #%d\n", dp->ident_s, ident, i);
                            if (strcasestr(dp->ident_s, ident) != NULL) {
                                pos = i;
                                break;
                            }
                        }
                    }
                }
                dx_print_search(true, "DX_ADM_SEARCH_IDENT <%s> found #%d\n", ident, pos);
                send_msg(conn, false, "MSG mkr_search_pos=1,%d", pos);
                return true;
            } else

            if (func == DX_ADM_SEARCH_NOTES) {
                int pos = -1;
                if (notes != NULL && notes[0] != '\0') {
                    kiwi_str_decode_inplace(notes);
                    dx_t *cur_list = dx.stored_list;
                    int cur_len = dx.stored_len;
                    idx1 = CLAMP(idx1, 0, cur_len - 1);

                    // search starting from given index
                    dx_t *dp = &cur_list[idx1];
                    for (i = idx1; dp < &cur_list[cur_len]; i++, dp++) {
                        dx_print_search(dp->notes, "DX_ADM_SEARCH_NOTES LOOP1 <%s> <%s> #%d\n", dp->notes_s, notes, i);
                        if (dp->notes && strcasestr(dp->notes_s, notes) != NULL) {
                            pos = i;
                            break;
                        }
                    }

                    // if not found, try searching full list
                    if (pos == -1) {
                        dx_t *dp = &cur_list[0];
                        for (i = 0; dp < &cur_list[cur_len]; i++, dp++) {
                            dx_print_search(dp->notes, "DX_ADM_SEARCH_NOTES LOOP2 <%s> <%s> #%d\n", dp->notes_s, notes, i);
                            if (dp->notes && strcasestr(dp->notes_s, notes) != NULL) {
                                pos = i;
                                break;
                            }
                        }
                    }
                }
                dx_print_search(true, "DX_ADM_SEARCH_NOTES <%s> found #%d\n", notes, pos);
                send_msg(conn, false, "MSG mkr_search_pos=2,%d", pos);
                return true;
            }
            
            if (db_stored && dx.stored_len == 0 && func != DX_ADM_MKRS) {
                send_msg(conn, false, "MSG mkr=[{\"t\":%d}]", DX_MKRS);     // otherwise last marker won't get cleared
                return true;
            }
        
            int today_Su_0;
            utc_year_month_day(NULL, NULL, NULL, &today_Su_0);

            //#define TEST_DX_TIME_REV
            // search for TEST_DX_TIME_REV on js side to enable dx update to see debug effect once per minute 
            #ifdef TEST_DX_TIME_REV
                int hr_min = 23*60 + 58 + timer_sec()/60;
                hr_min = hr_min/60*100 + hr_min % 60;
                if (hr_min >= 2400) {
                    hr_min -= 2400;
                    today_Su_0 = (today_Su_0 == 6)? 0 : (today_Su_0 + 1);
                }
                static int hr_min_last;
                if (hr_min != hr_min_last) {
                    printf("TICK hr_min=%04d\n", hr_min);
                    hr_min_last = hr_min;
                }
            #else
                int _hr, _min;
                utc_hour_min_sec(&_hr, &_min);
                int hr_min = _hr*100 + _min;    // NB: not *60 because value is stored base 100, e.g. "12:34" = 1234(1200 + 34)
            #endif
            
            int today_Mo_0 = (today_Su_0 == 0)? 6 : (today_Su_0 - 1);
            int yesterday_Mo_0 = (today_Mo_0 == 0)? 6 : (today_Mo_0 - 1);
            struct timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);
            u4_t msec = ts.tv_nsec/1000000;
            int send = 0;

            // reset appending
            sb = kstr_asprintf(NULL, "[{\"t\":%d,\"n\":%d,\"s\":%ld,\"m\":%d,\"f\":%d,\"pe\":%d,\"fe\":%d",
                func, dx.stored_len, ts.tv_sec, msec, (conn->dx_err_preg_ident? 1:0) + (conn->dx_err_preg_notes? 2:0),
                dx.json_parse_errors, dx.dx_format_errors);
            if (drx->db == DB_EiBi) {
                sb = kstr_cat(sb, ",\"ec\":[");
                for (i = 0; i < DX_N_EiBi; i++)
                    sb = kstr_asprintf(sb, "%d%s", eibi_counts[i], (i < (DX_N_EiBi-1))? "," : "");
                sb = kstr_cat(sb, "]");
            }

            // count of label entries using each type
            sb = kstr_cat(sb, ",\"tc\":[");
            int types_n[DX_N_STORED];
            memset(types_n, 0, sizeof(types_n));
            dx_t *dp = dx.stored_list;
            for (i = 0; i < dx.stored_len; i++, dp++) {
                types_n[DX_STORED_TYPE2IDX(dp->flags)]++;
            }
            for (i = 0; i < DX_N_STORED; i++) {
                sb = kstr_asprintf(sb, "%d%s", types_n[i], (i < (DX_N_STORED-1))? "," : "");
            }
            sb = kstr_cat(sb, "]}");

            if (func == DX_ADM_MKRS) {
                dx_t *cur_list = dx.stored_list;
                int cur_len = dx.stored_len;
                dp = &cur_list[idx1];
                for (i = idx1; i <= idx2 && dp < &cur_list[cur_len]; i++, dp++) {
                    u2_t dow = (dp->flags & DX_DOW) >> DX_DOW_SFT;
                    if (dow == 0) {
                        cprintf(conn, "$DX_ADM_MKRS why is dow=0? idx=%d f=%.2f\n", i, dp->freq);
                        dp->flags |= DX_DOW;
                    }
                    
                    if (dp->time_begin == 0 && dp->time_end == 0) {
                        cprintf(conn, "$DX_ADM_MKRS why is b=e=0? idx=%d f=%.2f\n", i, dp->freq);
                        dp->time_end = 2400;
                    }
                    
                    //float freq = dp->freq + ((float) dp->offset / 1000.0);		// carrier plus offset
                    sb = kstr_asprintf(sb, ",{\"g\":%d,\"f\":%.3f,\"lo\":%d,\"hi\":%d,\"o\":%d,\"fl\":%d,\"b\":%d,\"e\":%d,"
                        "\"i\":\"%s\"%s%s%s%s%s%s}",
                        dp->idx, dp->freq, dp->low_cut, dp->high_cut, dp->offset, dp->flags, dp->time_begin, dp->time_end,
                        dp->ident,
                        dp->notes? ",\"n\":\"":"", dp->notes? dp->notes:"", dp->notes? "\"":"",
                        dp->params? ",\"p\":\"":"", dp->params? dp->params:"", dp->params? "\"":"");
                    
                        dx_print_adm_mkrs("DX_ADM_MKRS %d|#%d(%d) %.2f(%d) %s\n", i, dp->idx, dx.stored_len, dp->freq, dp->offset, dp->ident_s);
                }
                sb = kstr_cat(sb, "]");
                send_msg(conn, false, "MSG admin_mkr=%s", kstr_sp(sb));
                kstr_free(sb);
                dx_print_adm_mkrs("DX_ADM_MKRS DONE\n");
                return true;
            } else {
                // func == DX_MKRS, DX_STEP
                
                // bsearch the lower bound for speed with large lists
                dx_t dx_min;

                // DX_SEARCH_WINDOW: when zoomed far-in need to look at wider window since we don't know PB center here
                #define DX_SEARCH_WINDOW 10.0
                dx_min.freq = min + ((func == DX_STEP && dir == 1)? DX_SEARCH_WINDOW : -DX_SEARCH_WINDOW);
                dx_t *cur_list = db_stored? dx.stored_list : dx.eibi_list;
                int cur_len = db_stored? dx.stored_len : dx.eibi_len;
                dx_list_first = &cur_list[0];
                dx_list_last = &cur_list[cur_len - DX_HIDDEN_SLOT];     // NB: addr of LAST in list

                dx_t *dp = (dx_t *) bsearch(&dx_min, cur_list, cur_len, sizeof(dx_t), bsearch_freqcomp);
                if (dp == NULL) panic("DX bsearch");
                //cprintf(conn, "DX_MKR key=%.2f bsearch=%.2f(%d/%d) min=%.2f max=%.2f\n",
                //    dx_min.freq, dp->freq + ((float) dp->offset / 1000.0), dp->idx, cur_len, min, max);
        
                int dx_filter = 0, fn_flags = 0;
                if (conn->dx_filter_ident || conn->dx_filter_notes) {
                    dx_filter = 1;
                    fn_flags = conn->dx_filter_case? 0 : FNM_CASEFOLD;
                    //cprintf(conn, "DX_MKR FILTERING on <%s> <%s> case=%d wild=%d grep=%d\n",
                    //    conn->dx_filter_ident, conn->dx_filter_notes, conn->dx_filter_case, conn->dx_filter_wild, conn->dx_filter_grep);
                    //show_conn("DX FILTER ", conn);
                }
        
                //if (drx->db == DB_EiBi) cprintf(conn, "EiBi BSEARCH len=%d %.2f\n", cur_len, dp->freq);
                int clutter_filtered = 0;
                for (; dp < &cur_list[cur_len] && dp >= cur_list; dp += dir) {
                    u4_t flags = 0;
                    float freq = dp->freq + ((float) dp->offset / 1000.0);		// carrier plus offset
                    //if (drx->db == DB_EiBi) cprintf(conn, "DX_MKR EiBi CONSIDER %.2f\n", freq);

                    if (func == DX_MKRS && freq > max + DX_SEARCH_WINDOW) break;    // get extra one above for label stepping
                
                    if (drx->db == DB_EiBi) {
                        u4_t type_bit = 1 << (((dp->flags & DX_TYPE) - DX_BCAST) >> 4);
                        if ((type_bit & eibi_types_mask) == 0) continue;
                    }

                    // This is tricky.
                    //
                    // It's possible for begin/end times to span midnight (00:00 UTC).
                    // The DOW value is only specified for the pre-midnight part of the span.
                    // So if we're checking during the post-midnight part of the span need to
                    // check against what's now the previous DOW in case the current DOW is
                    // not part of the schedule.
                    //
                    // Example: The schedule is Fri 23:00 - 02:00. When the current time
                    // is >= 00:00 today_Mo_0 will now be Sat. So need to check instead against
                    // yesterday_Mo_0 which is now set to Fri (remember that today_Mo_0 and
                    // yesterday_Mo_0 always follow the current TOD when this code runs).
                    
                    u2_t time_begin = dp->time_begin, time_end = dp->time_end;
                    if (dp->time_begin == 0 && dp->time_end == 0) {
                        cprintf(conn, "$DX_MKRS why is b=e=0? idx=%d f=%.2f\n", dp->idx, dp->freq);
                        dp->time_end = 2400;
                    }
                    bool rev = (time_begin > time_end);

                    u2_t dow = (dp->flags & DX_DOW) >> DX_DOW_SFT;
                    if (dow == 0) {
                        cprintf(conn, "$DX_MKRS why is dow=0? idx=%d f=%.2f\n", dp->idx, dp->freq);
                        dp->flags |= DX_DOW;
                    }

                    u4_t dow_mask = dp->flags & DX_DOW;
                    if (dow_mask == 0) dow_mask = DX_DOW;   // zero same as all dow specified (saves space in dx.jaon)
                    u4_t today_bit =  DX_MON >> today_Mo_0;
                    u4_t yesterday_bit = DX_MON >> yesterday_Mo_0;
                    bool today_ok = ((dow_mask & today_bit) != 0);
                    bool yesterday_ok = ((dow_mask & yesterday_bit) != 0);
                    bool dow_ok;
                    
                    if (!rev) {
                        // begin/end times are within current day
                        dow_ok = today_ok;
                    } else {
                        // begin/end times span today and tomorrow
                        // this means when we get to tomorrow we need to check for yesterday enabled in dow mask
                        dow_ok = (hr_min >= time_begin)? today_ok : yesterday_ok;
                    }
                    
                    dx_print_dow_time(!dow_ok || rev, "%s today_Mo_0=%d|%d today_ok=%d|%d b=%04x|%04x m=%04x fl=%04x rev=%d %04d|%04d|%04d %s %.2f %s\n",
                        !dow_ok? "!DOW" : "REV",
                        today_Mo_0, yesterday_Mo_0, today_ok, yesterday_ok, today_bit, yesterday_bit,
                        dow_mask, dp->flags, rev, time_begin, hr_min, time_end,
                        filter_tod? "FILTERED-OUT" : "FILTER-OFF", freq, dp->ident_s);

                    if (!dow_ok) {
                        if (filter_tod)
                            continue;
                        else
                            flags = DX_FILTERED;
                    }
                    
                    bool within = (hr_min >= time_begin && hr_min < time_end);
                    bool within_rev = (hr_min >= time_end && hr_min < time_begin);
                    if ((!rev && within) || (rev && !within_rev)) {
                        // TOD okay
                    } else {
                        dx_print_dow_time(true, "!TOD %04d|%04d|%04d fl=04%x %s %.2f %s\n", time_begin, hr_min, time_end, dp->flags,
                            filter_tod? "FILTERED-OUT" : "FILTER-OFF", freq, dp->ident_s);
                        if (filter_tod)
                            continue;
                        else
                            flags = DX_FILTERED;
                    }
            
                    if (dx_filter) {
                        if (conn->dx_filter_grep) {
                            if (conn->dx_has_preg_ident) {
                                if (regexec(&conn->dx_preg_ident, dp->ident_s, 0, NULL, 0) == REG_NOMATCH) continue;
                            }
                            if (conn->dx_has_preg_notes) {
                                if (regexec(&conn->dx_preg_notes, dp->notes_s, 0, NULL, 0) == REG_NOMATCH) continue;
                            }
                            //cprintf(conn, "DX_MKR FILTER MATCHED-grep %s<%s> %s<%s>\n",
                            //    conn->dx_has_preg_ident? "*":"", dp->ident_s, conn->dx_has_preg_notes? "*":"", dp->notes_s);
                        } else
                        if (conn->dx_filter_wild) {
                            if (fnmatch(conn->dx_filter_ident, dp->ident_s, fn_flags) != 0) continue;
                            if (conn->dx_filter_notes && conn->dx_filter_notes[0] != '\0' &&
                                fnmatch(conn->dx_filter_notes, dp->notes_s, fn_flags) != 0) continue;
                            //cprintf(conn, "DX_MKR FILTER MATCHED-wild <%s> <%s>\n", dp->ident_s, dp->notes_s);
                        } else {
                            if (conn->dx_filter_case) {
                                if (strstr(dp->ident_s, conn->dx_filter_ident) == NULL) continue;
                                if (conn->dx_filter_notes && strstr(dp->notes_s, conn->dx_filter_notes) == NULL) continue;
                            } else {
                                if (strcasestr(dp->ident_s, conn->dx_filter_ident) == NULL) continue;
                                if (conn->dx_filter_notes && strcasestr(dp->notes_s, conn->dx_filter_notes) == NULL) continue;
                            }
                            //cprintf(conn, "DX_MKR FILTER MATCHED-no-grep <%s> <%s>\n", dp->ident_s, dp->notes_s);
                        }
                    }
            
                    // reduce dx label clutter
                    if (func == DX_MKRS && anti_clutter && zoom <= DX_SPACING_ZOOM_THRESHOLD) {
                        int x = ((dp->freq - min) / bw) * width;
                        int diff = x - dx_lastx;
                        //cprintf(conn, "DX_MKR spacing %d %d %d %s\n", dx_lastx, x, diff, dp->ident_s);
                        if (!first && diff < DX_SPACING_THRESHOLD_PX) {
                            clutter_filtered++;
                            continue;
                        }
                        dx_lastx = x;
                        first = false;
                    }
            
                    // NB: ident, notes and params are already stored URL encoded
                    if (func == DX_MKRS || dp->freq != min) {
                        //#define DEBUG_DX_MKRS
                        #ifdef DEBUG_DX_MKRS
                            int fail = 0;
                            char *c;
                            c = dp->country;
                            if (kiwi_strnlen(c, 3) > 3) {
                                lprintf("country %02x %02x %02x %02x\n", c[0], c[1], c[2], c[3]);
                                c[3] = '\0';
                                fail = 1;
                            }
                            c = dp->lang;
                            if (kiwi_strnlen(c, 3) > 3) {
                                lprintf("lang %02x %02x %02x %02x\n", c[0], c[1], c[2], c[3]);
                                c[3] = '\0';
                                fail = 1;
                            }
                            c = dp->target;
                            if (kiwi_strnlen(c, 3) > 3) {
                                lprintf("target %02x %02x %02x %02x\n", c[0], c[1], c[2], c[3]);
                                c[3] = '\0';
                                fail = 1;
                            }
                            c = (char *) dp->ident;
                            if (kiwi_strnlen(c, 255) > 255) {
                                c[255] = '\0';
                                lprintf("ident <%s>\n", c);
                                fail = 1;
                            }
                            c = dp->notes;
                            if (kiwi_strnlen(c, 255) > 255) {
                                c[255] = '\0';
                                lprintf("notes <%s>\n", c);
                                fail = 1;
                            }
                            c = (char *) dp->params;
                            if (kiwi_strnlen(c, 255) > 255) {
                                c[255] = '\0';
                                lprintf("params <%s>\n", c);
                                fail = 1;
                            }
                        
                            if (fail) {
                                lprintf("$$$$$$$$ DEBUG_DX_MKRS: ERROR freq=%.3f db=%s\n", freq, (drx->db == DB_EiBi)? "EiBi" : "stored");
                            } else
                        #endif
                    
                        {
                            sb = kstr_asprintf(sb, ",{\"g\":%d,\"f\":%.3f,\"lo\":%d,\"hi\":%d,\"o\":%d,\"fl\":%d,"
                                "\"b\":%d,\"e\":%d,\"c\":\"%s\",\"l\":\"%s\",\"t\":\"%s\","
                                "\"i\":\"%s\"%s%s%s%s%s%s}",
                                dp->idx, freq, dp->low_cut, dp->high_cut, dp->offset, dp->flags | flags,
                                time_begin, time_end, dp->country, dp->lang, dp->target,
                                dp->ident,
                                dp->notes? ",\"n\":\"":"", dp->notes? dp->notes:"", dp->notes? "\"":"",
                                dp->params? ",\"p\":\"":"", dp->params? dp->params:"", dp->params? "\"":"");
                            //if (drx->db == DB_EiBi) cprintf(conn, "DX_MKR EiBi %d: %.2f(%d) %s\n", send, freq, dp->idx, dp->ident_s);
                            dx_print_mkrs(drx->db == DB_STORED, "DX_MKR #%d %.2f flags=%04x o=%d b=%d e=%d \"%s\"\n", dp->idx, freq, dp->flags, dp->offset,
                                time_begin, time_end, kiwi_str_decode_static((char *) dp->ident));
                            send++;
                        }
                    }
            
                    // return the very first we hit that passed the filtering criteria above
                    if (func == DX_STEP && send) break;
                }
            }
        
            sb = kstr_cat(sb, "]");
            send_msg(conn, false, "MSG mkr=%s", kstr_sp(sb));
            //cprintf(conn, "DX_MKR send=%d anti_clutter=%d clutter_filtered=%d\n", send, anti_clutter, clutter_filtered);
            if (drx->db == DB_EiBi) {
                //cprintf(conn, "DX_MKR EiBi send=%d\n", send);
                //real_printf("%s\n", kstr_sp(sb));
            }
            kstr_free(sb);

            if (db_changed)
                send_msg(conn, false, "MSG request_dx_update");     // get client to request updated dx list

            return true;
        }
	    break;
	
    case CMD_GET_DX:
        if (strcmp(cmd, "SET GET_DX_SIZE") == 0) {
            // send just the database entry count, as entries will be incrementally read
            send_msg(conn, false, "MSG dx_size=%d", dx.stored_len);
            return true;
        }

        if (strcmp(cmd, "SET GET_DX_LIST") == 0) {
            // get all active user connections to refresh dx list (and updated cfg)
            // when a non dx list change is made by an admin connection (e.g. dx type menu)
            cfg_cfg.update_seq++;
            dx.update_seq++;
            return true;
        }
	    break;

	// SECURITY: should be okay: checks for conn->auth_admin first
    case CMD_DX_SET:
        if (conn->auth_admin == false) {
            cprintf(conn, "CMD_DX_SET NO ADMIN AUTH %s\n", conn->remote_ip);
            return true;
        }

        if (strcmp(cmd, "SET DX_SET...") == 0) {
            return true;
        }
	    break;
#endif


////////////////////////////////
// status and config
////////////////////////////////

    case CMD_GET_CONFIG:
        if (strcmp(cmd, "SET GET_CONFIG") == 0) {
            asprintf(&sb, "{\"r\":%d,\"g\":%d,\"s\":%d,\"pu\":\"%s\",\"pe\":%d,\"pv\":\"%s\",\"pi\":%d,\"n\":%d,\"m\":\"%s\",\"v1\":%d,\"v2\":%d}",
                rx_chans, GPS_CHANS, net.serno, net.ip_pub, net.port_ext, net.ip_pvt, net.port, net.nm_bits, net.mac, version_maj, version_min);
            send_msg(conn, false, "MSG config_cb=%s", sb);
            kiwi_ifree(sb);
            return true;
        }
	    break;
	
    case CMD_STATS_UPD:
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
                    if (c && c->valid && c->arrived && c->type == STREAM_SOUND && c->ident_user != NULL) {
                        underruns += c->audio_underrun;
                        seq_errors += c->sequence_errors;
                    }
                }
            }
        
            sb = kstr_asprintf(NULL, cpu_stats_buf? "{%s," : "{", kstr_sp(cpu_stats_buf));

            float sum_kbps = audio_kbps[rx_chans] + waterfall_kbps[rx_chans] + http_kbps;
            sb = kstr_asprintf(sb, "\"ac\":%.0f,\"wc\":%.0f,\"fc\":%.0f,\"ah\":%.0f,\"as\":%.0f,\"sr\":%.6f,\"nsr\":%d",
                audio_kbps[ch], waterfall_kbps[ch], waterfall_fps[ch], http_kbps, sum_kbps,
                ext_update_get_sample_rateHz(-1), snd_rate);

            sb = kstr_asprintf(sb, ",\"ga\":%d,\"gt\":%d,\"gg\":%d,\"gf\":%d,\"gc\":%.6f,\"go\":%d",
                gps.acquiring, gps.tracking, gps.good, gps.fixes, adc_clock_system()/1e6, clk.adc_gps_clk_corrections);

    #ifdef USE_SDR
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
            
            sb = kstr_asprintf(sb, ",\"sa\":%d,\"sh\":%d", snr_all, freq_offset? -1 : snr_HF);
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
	    break;

#ifdef USE_SDR

    case CMD_GET_USERS:
	if (strcmp(cmd, "SET GET_USERS") == 0) {
		bool include_ip = (conn->type == STREAM_ADMIN);
		sb = rx_users(include_ip);
		send_msg(conn, false, "MSG user_cb=%s", kstr_sp(sb));
		kstr_free(sb);
		return true;
	}
	    break;
	    
#endif


////////////////////////////////
// UI
////////////////////////////////

    case CMD_IDENT_USER:
        if (kiwi_str_begins_with(cmd, "SET ident_user=")) {
            
            // if it's nothing but spaces the scanf fails and ident_user_m = NULL
            char *ident_user_m = NULL;
            sscanf(cmd, "SET ident_user=%256ms", &ident_user_m);
            bool noname = (ident_user_m == NULL || ident_user_m[0] == '\0');
            bool setUserIP = false;

            if (conn->mc == NULL) return true;	// we've seen this

            // no name sent, no previous name set: use ip as name
            if (noname && !conn->ident_user) setUserIP = true;

            // no name sent, but ip changed: use ip as name
            if (noname && conn->ident_user && strcmp(conn->ident_user, conn->remote_ip)) setUserIP = true;
        
            // else no name sent, but ip didn't change: do nothing
        
            //cprintf(conn, "SET ident_user=<%s> noname=%d setUserIP=%d\n", ident_user_m, noname, setUserIP);
        
            if (setUserIP) {
                kiwi_str_redup(&conn->ident_user, "ident_user", conn->remote_ip);
                conn->isUserIP = TRUE;
                // printf(">>> isUserIP TRUE: %s:%05d setUserIP=%d noname=%d user=%s <%s>\n",
                // 	conn->remote_ip, conn->remote_port, setUserIP, noname, conn->ident_user, cmd);
            }

            // name sent: save new, replace previous (if any)
            if (!noname) {
                kiwi_str_decode_inplace(ident_user_m);
                int printable, UTF;
                char *esc = kiwi_str_escape_HTML(ident_user_m, &printable, &UTF);
                if (esc) {
                    kiwi_ifree(ident_user_m);
                    ident_user_m = esc;
                }
                if (!(printable || UTF)) {
                    clprintf(conn, "API: ident has no printable chars! ");
                    int sl = strlen(ident_user_m);
                    for (i = 0; i < sl; i++) {
                        clprintf(conn, "%02x ", ident_user_m[i]);
                    }
                    clprintf(conn, "(%d) %s\n", sl, conn->remote_ip);
                    kiwi_ifree(ident_user_m);
                    ident_user_m = strdup("(bad identity)");
                }

                kiwi_str_redup(&conn->ident_user, "ident_user", ident_user_m);
                int len = cfg_int("ident_len", NULL, CFG_REQUIRED);
                len = MAX(len, IDENT_LEN_MIN);
                if (strlen(conn->ident_user) > len)
                    conn->ident_user[len] = '\0';
                //printf("ident <%s> len=%d\n", conn->ident_user, len);
                conn->isUserIP = FALSE;
                // printf(">>> isUserIP FALSE: %s:%05d setUserIP=%d noname=%d user=%s <%s>\n",
                // 	conn->remote_ip, conn->remote_port, setUserIP, noname, conn->ident_user, cmd);
            }
        
            kiwi_ifree(ident_user_m);
            conn->ident = true;
            return true;
        }
	    break;

    case CMD_NEED_STATUS:
        n = sscanf(cmd, "SET need_status=%d", &j);
        if (n == 1) {
            if (conn->mc == NULL) return true;	// we've seen this
            char *status = (char*) cfg_string("status_msg", NULL, CFG_REQUIRED);
            send_msg_encoded(conn, "MSG", "status_msg_html", "\f%s", status);
            cfg_string_free(status);
            return true;
        }
	    break;
	
    case CMD_GEO_LOC: {
        char *geo_m = NULL;
        n = sscanf(cmd, "SET geoloc=%127ms", &geo_m);
        //cprintf(conn, "GEOLOC: n=%d cmd=<%s> geo_m=%p <%s>\n", n, cmd, geo_m, geo_m);
        if (n == 1) {
            kiwi_str_decode_inplace(geo_m);
            //cprintf(conn, "GEOLOC: recv geoloc from client: %p <%s>\n", geo_m, geo_m);
            char *esc = kiwi_str_escape_HTML(geo_m);
            if (esc) {
                kiwi_ifree(geo_m);
                geo_m = esc;
            }
            kiwi_ifree(conn->geo);
            conn->geo = geo_m;
            return true;
        }
	    break;
	}

    case CMD_GEO_JSON: {
        char *geojson_m = NULL;
        n = sscanf(cmd, "SET geojson=%256ms", &geojson_m);
        if (n == 1) {
            kiwi_str_decode_inplace(geojson_m);
            //clprintf(conn, "SET geojson<%s>\n", geojson_m);
            kiwi_ifree(geojson_m);
            return true;
        }
	    break;
    }
	
    case CMD_BROWSER: {
        char *browser_m = NULL;
        n = sscanf(cmd, "SET browser=%256ms", &browser_m);
        if (n == 1) {
            kiwi_str_decode_inplace(browser_m);
            //clprintf(conn, "SET browser=<%s>\n", browser_m);
            kiwi_ifree(browser_m);
            return true;
        }
	    break;
    }
	
#ifdef USE_SDR
    // used by signal generator etc.
    case CMD_WF_COMP: {
        int wf_comp;
        n = sscanf(cmd, "SET wf_comp=%d", &wf_comp);
        if (n == 1) {
            c2s_waterfall_compression(conn->rx_channel, wf_comp? true:false);
            //printf("### SET wf_comp=%d\n", wf_comp);
            return true;
        }
	    break;
    }
#endif

    case CMD_INACTIVITY_ACK: {
        if (strcmp(cmd, "SET inactivity_ack") == 0) {
            conn->last_tune_time = timer_sec();
            return true;
        }
	    break;
    }

    case CMD_NOTIFY_MSG: {
        if (strcmp(cmd, "SET notify_msg") == 0) {
            send_msg_encoded(conn, "MSG", "notify_msg", "%s", extint.notify_msg);
            return true;
        }
	    break;
    }


////////////////////////////////
// preferences
////////////////////////////////

    case CMD_PREF_EXPORT: {
        if (kiwi_str_begins_with(cmd, "SET pref_export")) {
            kiwi_ifree(conn->pref_id);
            kiwi_ifree(conn->pref);
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
                    kiwi_ifree(c->pref_id); c->pref_id = NULL;
                    kiwi_ifree(c->pref); c->pref = NULL;
                }
            }
        
            return true;
        }
	    break;
	}
	
    case CMD_PREF_IMPORT: {
        if (kiwi_str_begins_with(cmd, "SET pref_import")) {
            kiwi_ifree(conn->pref_id);
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

            kiwi_ifree(conn->pref_id); conn->pref_id = NULL;
            return true;
        }
	    break;
    }


////////////////////////////////
// kiwiclient
////////////////////////////////

    // DEPRECATED
    // Replaced by "--tlimit-pw" option that specifies an actual time limit exemption password.
    // But silently accept for backward compatibility with old versions of kiwiclient still in use.
    case CMD_OVERRIDE: {
        int inactivity_timeout;
        n = sscanf(cmd, "SET OVERRIDE inactivity_timeout=%d", &inactivity_timeout);
        if (n == 1) {
            return true;
        }
	    break;
    }


////////////////////////////////
// debugging
////////////////////////////////

	// SECURITY: only used during debugging
    case CMD_NOCACHE:
        n = sscanf(cmd, "SET nocache=%d", &i);
        if (n == 1) {
            web_nocache = i? true : false;
            printf("SET nocache=%d\n", web_nocache);
            return true;
        }
	    break;

	// SECURITY: only used during debugging
    case CMD_CTRACE:
        n = sscanf(cmd, "SET ctrace=%d", &i);
        if (n == 1) {
            web_caching_debug = i? true : false;
            printf("SET ctrace=%d\n", web_caching_debug);
            return true;
        }
	    break;

	// SECURITY: only used during debugging
    case CMD_DEBUG_VAL:
        n = sscanf(cmd, "SET dbug_v=%d", &i);
        if (n == 1) {
            debug_v = i;
            //printf("SET dbug_v=%d\n", debug_v);
            return true;
        }
	    break;

	// SECURITY: only used during debugging
    case CMD_DEBUG_MSG: {
        if (kiwi_str_begins_with(cmd, "SET dbug_msg=")) {
            kiwi_str_decode_inplace(cmd);
            clprintf(conn, "### DEBUG MSG: <%s>\n", &cmd[13]);
            return true;
        }
	    break;
    }


////////////////////////////////
// misc
////////////////////////////////

    case CMD_IS_ADMIN:
        if (strcmp(cmd, "SET is_admin") == 0) {
            send_msg(conn, false, "MSG is_admin=%d", conn->auth_admin);
            return true;
        }
	    break;

    case CMD_GET_AUTHKEY:
    if (strcmp(cmd, "SET get_authkey") == 0) {
        if (conn->auth_admin == false) {
            cprintf(conn, "get_authkey NO ADMIN AUTH %s\n", conn->remote_ip);
            return true;
        }

        kiwi_ifree(current_authkey);
        current_authkey = kiwi_authkey();
        send_msg(conn, false, "MSG authkey_cb=%s", current_authkey);
        return true;
    }
	    break;

    case CMD_CLK_ADJ: {
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
	    break;
    }

    case CMD_SERVER_DE_CLIENT:
	    if (kiwi_str_begins_with(cmd, "SERVER DE CLIENT"))
	        return true;
	    break;
	
    case CMD_X_DEBUG:
        if (kiwi_str_begins_with(cmd, "SET x-DEBUG")) {
            cprintf(conn, "x-DEBUG %s \"%s\"\n", conn->remote_ip, &cmd[12]);
            return true;
        }
	    break;

	default:
	    break;
	}
	
	// kiwiclient still uses "SET geo=" which is shorter than the max_hash_len
	// so must be checked manually
    char *geo_m = NULL;
    n = sscanf(cmd, "SET geo=%127ms", &geo_m);
    if (n == 1) {
        kiwi_str_decode_inplace(geo_m);
        cprintf(conn, "recv geoloc from client: %s\n", geo_m);
        char *esc = kiwi_str_escape_HTML(geo_m);
        if (esc) {
            kiwi_ifree(geo_m);
            geo_m = esc;
        }
        kiwi_ifree(conn->geo);
        conn->geo = geo_m;
        return true;
    }

	// we see these sometimes; not part of our protocol
    if (strcmp(cmd, "PING") == 0)
        return true;

	// we see these at the close of a connection sometimes; not part of our protocol
    #define ASCII_ETX 3
	if (strlen(cmd) == 2 && cmd[0] == ASCII_ETX)
	    return true;
	
	// we see these timestamps sometimes; not part of our protocol
	int y, d, h, m, s;
	if (sscanf(cmd, "%d/%d/%d %d:%d:%d", &y, &n, &d, &h, &m, &s) == 6) {
	    conn->spurious_timestamps_recvd++;
	    return true;
	}

    //printf("#### rx_common_cmd(%s): key=%d hash=0x%04x \"%s\"\n",
    //    stream_name, key, rx_common_cmd_hash.cur_hash, cmd);
	return false;       // signal we didn't handle command
}

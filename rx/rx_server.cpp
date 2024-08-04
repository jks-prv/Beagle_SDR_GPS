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

// Copyright (c) 2014-2016 John Seamons, ZL4VO/KF6VO

#include "types.h"
#include "config.h"
#include "options.h"
#include "kiwi.h"
#include "rx.h"
#include "rx_server.h"
#include "rx_util.h"
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
#include "data_pump.h"
#include "mongoose.h"
#include "wspr.h"

#ifdef USE_SDR
 #include "ext_int.h"
#endif

#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <sched.h>
#include <math.h>
#include <signal.h>

//#define CONN_PRINTF
#ifdef CONN_PRINTF
	#define conn_printf(fmt, ...) \
		printf(fmt, ## __VA_ARGS__)
#else
	#define conn_printf(fmt, ...)
#endif

conn_t conns[N_CONNS];
bool force_camp;

rx_chan_t rx_channels[MAX_RX_CHANS];

// NB: must be in conn_t.type order
rx_stream_t rx_streams[] = {
	{ AJAX_VERSION,		"VER" },
	{ STREAM_ADMIN,		"admin",	&c2s_admin,		&c2s_admin_setup,		&c2s_admin_shutdown,	 TASK_MED_PRIORITY },
	{ STREAM_MFG,		"mfg",		&c2s_mfg,		&c2s_mfg_setup,			NULL,                    TASK_MED_PRIORITY },
#ifdef USE_SDR
	{ STREAM_SOUND,		"SND",		&c2s_sound,		&c2s_sound_setup,		&c2s_sound_shutdown,	 SND_PRIORITY },
	{ STREAM_WATERFALL,	"W/F",		&c2s_waterfall,	&c2s_waterfall_setup,	&c2s_waterfall_shutdown, WF_PRIORITY },
	{ STREAM_EXT,		"EXT",		&extint_c2s,	&extint_setup_c2s,		&extint_shutdown_c2s,    TASK_MED_PRIORITY },
	{ STREAM_MONITOR,   "MON",		&c2s_mon,	    &c2s_mon_setup,         NULL,                    TASK_MED_PRIORITY },

	// AJAX requests
	{ AJAX_DISCOVERY,	"DIS" },
	{ AJAX_PHOTO,		"PIX" },
	{ AJAX_DX,		    "DX" },
	{ AJAX_STATUS,		"status" },
	{ AJAX_USERS,		"users" },
	{ AJAX_SNR,         "snr" },
	{ AJAX_ADC,         "adc" },
	{ AJAX_ADC_OV,      "adc_ov" },
	{ AJAX_S_METER,     "s-meter" },
#endif
	{ 0 }
};

static void conn_init(conn_t *c)
{
	memset(c, 0, sizeof(conn_t));
	c->magic = CN_MAGIC;
	c->self_idx = c - conns;
	c->rx_channel = -1;
}

const char *rx_conn_type(conn_t *c)
{
    if (c == NULL) return "nil";
	if (c->type < 0 || c->type >= ARRAY_LEN(rx_streams)) return "(bad type)";
	return (rx_streams[c->type].uri);
}

void rx_enable(int chan, rx_chan_action_e action)
{
    if (chan < 0) return;
	rx_chan_t *rx = &rx_channels[chan];
	
	switch (action) {

	case RX_CHAN_ENABLE: rx->chan_enabled = true; break;
	case RX_CHAN_DISABLE: rx->chan_enabled = false; break;
	case RX_DATA_ENABLE: rx->data_enabled = true; break;
	case RX_CHAN_FREE: memset(rx, 0, sizeof(rx_chan_t)); break;
	default: panic("rx_enable"); break;

	}
	
	#ifdef USE_SDR
	    data_pump_start_stop();
	#endif
}

cfg_t cfg_ipl;

void rx_server_init()
{
	int i, j;
	
	printf("RX N_CONNS %d conns %.3f MB\n", N_CONNS, (float) sizeof(conns)/M);
	conn_t *c = conns;
	for (i=0; i<N_CONNS; i++) {
		conn_init(c);
		ndesc_register(&c->c2s);
		ndesc_register(&c->s2c);
		c++;
	}
	
	debug_init();
	
    //#ifndef DEVSYS
    #if 0
	    sig_arm(SIG_BACKTRACE, debug_exit_backtrace_handler);
    #endif

	update_vars_from_config(true);      // add any missing config vars
	
	// if not overridden in command line, set enable server according to configuration param
	if (!down) {
		bool error;
		bool server_enabled = admcfg_bool("server_enabled", &error, CFG_OPTIONAL);
		if (error || server_enabled == TRUE)
			down = FALSE;
		else
			down = TRUE;
	}

    json_init(&cfg_ipl, (char *) "{}", "cfg_ipl");
    
    ov_mask = 0xfc00;

    #ifdef USE_SDR
        spi_set(CmdSetOVMask, 0, ov_mask);
    #endif
}

void rx_server_remove(conn_t *c)
{
    rx_stream_t *st = &rx_streams[c->type];
    
    // kick corresponding ext if any 
    if (c->type == STREAM_SOUND || c->type == STREAM_WATERFALL) {
        //cprintf(c, "EXT remove from=conn-%02d rx_chan=%d tstamp=%016llx\n", c->self_idx, c->rx_channel, c->tstamp);
        //dump();
	    for (conn_t *conn = conns; conn < &conns[N_CONNS]; conn++) {
	        if (!conn->valid) continue;
	        if (conn->type == STREAM_EXT && conn->rx_channel == c->rx_channel && conn->tstamp == c->tstamp) {
	            //cprintf(c, "ext_kick rx_channel=%d\n", conn->rx_channel);
	            ext_kick(conn->rx_channel);
	        }
	    }
    }
    
    if (st->shutdown) (st->shutdown)((void *) c);
	c->stop_data = TRUE;

	struct mg_connection *mc = c->mc;
    #ifdef CONN_PRINTF
        cprintf(c, "rx_server_remove: c=%p mc=%p mc->connection_param=%p == %s\n",
            c, mc, mc? mc->connection_param : NULL, (mc && c == (conn_t *) mc->connection_param)? "T":"F");
    #endif
	c->mc = NULL;

    if (c->isMaster) {
        if (c->arrived) rx_loguser(c, LOG_LEAVING);
        clprintf(c, "--- connection closed -----------------------------------------------------\n");
    }
	webserver_connection_cleanup(c);
	kiwi_free("ident_user", c->ident_user);
	kiwi_asfree(c->geo);
	kiwi_asfree(c->pref_id);
	kiwi_asfree(c->pref);
	kiwi_asfree(c->browser);
	kiwi_asfree(c->dx_filter_ident);
	kiwi_asfree(c->dx_filter_notes);
    if (c->dx_has_preg_ident) { regfree(&c->dx_preg_ident); c->dx_has_preg_ident = false; }
    if (c->dx_has_preg_notes) { regfree(&c->dx_preg_notes); c->dx_has_preg_notes = false; }
    
    //if (!is_multi_core && c->is_locked) {
    if (c->is_locked) {
        //cprintf(c, "DRM rx_server_remove: global is_locked = 0\n");
        is_locked = 0;
    }
	
	int task = c->task;
	int cn = c->self_idx;
	conn_init(c);
	check_for_update(WAIT_UNTIL_NO_USERS, NULL);
	conn_printf("CONN-%02d rx_server_remove %p %s\n", cn, c, Task_ls(task));
	TaskRemove(task);
}

void rx_stream_tramp(void *param)
{
	conn_t *conn = (conn_t *) param;
	(conn->task_func)(param);
}

// if this connection is new, spawn new receiver channel with sound/waterfall tasks
conn_t *rx_server_websocket(websocket_mode_e mode, struct mg_connection *mc, u4_t ws_flags)
{
	int i;
	conn_t *c;
	rx_stream_t *st;
	bool internal = (mode == WS_INTERNAL_CONN);

    c = (conn_t*) mc->connection_param;

    //#ifdef CONN_PRINTF
    #if 0
        printf("rx_server_websocket(%s): mc: %p %s:%d %s %s\n", ws_mode_s[mode], mc, mc->remote_ip, mc->remote_port, mc->uri, mc->query);
        if (c != NULL)
            printf("rx_server_websocket: (mc=%p == mc->c->mc=%p)? mc->c=%p mc->c->valid=%d mc->c->magic=0x%x CN_MAGIC=0x%x mc->c->rport=%d\n",
                mc, c->mc, c, c->valid, c->magic, CN_MAGIC, c->remote_port);
        else
            printf("rx_server_websocket: c == NULL\n");
    #endif
    
    if (c) {	// existing connection
        
        if (c->magic != CN_MAGIC || !c->valid || mc != c->mc || mc->remote_port != c->remote_port) {
            if (mode != WS_MODE_ALLOC && !internal) return NULL;
        #ifdef CONN_PRINTF
            lprintf("rx_server_websocket(%s): BAD CONN MC PARAM\n", ws_mode_s[mode]);
            lprintf("rx_server_websocket: (mc=%p == mc->c->mc=%p)? mc->c=%p mc->c->valid=%d mc->c->magic=0x%x CN_MAGIC=0x%x mc->c->rport=%d\n",
                mc, c->mc, c, c->valid, c->magic, CN_MAGIC, c->remote_port);
            lprintf("rx_server_websocket: mc: %s:%d %s\n", mc->remote_ip, mc->remote_port, mc->uri);
            dump();
            lprintf("rx_server_websocket: returning NULL\n");
        #endif
            return NULL;
        }
        
        if (mode == WS_MODE_CLOSE) {
            //cprintf(c, "WS_MODE_CLOSE %s KICK KA=%02d/60 KC=%05d\n", rx_conn_type(c), c->keep_alive, c->keepalive_count);
            if (!c->internal_connection) {
                mg_ws_send(mc, "", 0, WEBSOCKET_OP_CLOSE);
            }
            c->mc = NULL;
            c->kick = true;
            return NULL;
        }
    
        return c;	// existing connection is valid
    }
	
	// if we're doing anything other than allocating (e.g. lookup, close) we should have matched above
	if (mode != WS_MODE_ALLOC && !internal)
		return NULL;
	
	// new connection needed
	const char *uri_ts = mc->uri;
	if (uri_ts[0] == '/') uri_ts++;
	conn_printf("#### new connection: %s:%d %s\n", mc->remote_ip, mc->remote_port, uri_ts);
	
	bool isKiwi_UI = false, isNo_WF = false, isWF_conn = false, isWB_conn = false, isWebSocket, isKrec = false;
	u64_t tstamp;
	char *type_m = NULL, *uri_m = NULL;
	int n = 0;


	// The new mongoose API requires something in the URL to distinguish web socket connections from
	// normal HTTP transfers. Since our code has to decide when a connection is a web socket requiring
	// an upgrade call to mg_ws_upgrade(). We prefix web socket URLs with "ws/" to do this.
	//
	// Kiwirecorder continues to work because its URL format is unique (<tstamp>/<uri>) compared to
	// the old Kiwi API (<type>/<tstamp/<uri>) Kiwirecorder always uses a web socket (SND, W/F, EXT)

	if ((n=sscanf(uri_ts, "ws/%8m[^/]/%lld/%256m[^\?]", &type_m, &tstamp, &uri_m)) == 3) {
	    isWebSocket = true;
	} else
	if (sscanf(uri_ts, "%8m[^/]/%lld/%256m[^\?]", &type_m, &tstamp, &uri_m) == 3) {
	    isWebSocket = false;
	} else
    // kiwiclient / kiwirecorder
    if (sscanf(uri_ts, "%lld/%256m[^\?]", &tstamp, &uri_m) == 2) {
        type_m = strdup("krec");
        isWebSocket = true;
        isKrec = true;
    } else {
        printf("bad URI_TS format n=%d <%s>\n", n, uri_ts);
        kiwi_asfree(type_m);
        kiwi_asfree(uri_m);
        return NULL;
    }
    conn_printf("URI_TS <%s> web_socket=%d <%s> %lld <%s>\n", uri_ts, isWebSocket, type_m, tstamp, uri_m);
    
    if (strcmp(type_m, "kiwi") == 0) {
	    isKiwi_UI = internal? false : true;
    } else
    if (strcmp(type_m, "no_wf") == 0) {
	    isKiwi_UI = true;
	    isNo_WF = true;
    } else
    if (strcmp(type_m, "wb") == 0) {
	    isWB_conn = true;
    } else
    if (!isKrec) {
        printf("bad URI_TS type <%s> <%s>\n", type_m, uri_ts);
        kiwi_asfree(type_m);
        kiwi_asfree(uri_m);
        return NULL;
    }
    kiwi_asfree(type_m);
    
    // specifically asked for waterfall-containing channel (e.g. kiwirecorder WF-only mode)
    if (strstr(uri_m, "W/F"))
        isWF_conn = true;
	
    //printf("URL <%s> <%s> <%s>\n", mc->uri, mc->query, uri_m);
	for (i=0; rx_streams[i].uri; i++) {
		st = &rx_streams[i];
		
		if (strcmp(uri_m, st->uri) == 0)
			break;
	}
	
	if (!rx_streams[i].uri) {
		lprintf("**** unknown stream type <%s>\n", uri_m);
        kiwi_asfree(uri_m);
		return NULL;
	}
    kiwi_asfree(uri_m);
    
	// handle case of server initially starting disabled, but then being enabled later by admin
#ifdef USE_SDR
	static bool init_snd_wf;
	if (!init_snd_wf) {
		c2s_sound_init();
		c2s_waterfall_init();
		init_snd_wf = true;
	}
#endif

	// iptables will stop regular connection attempts from a blacklisted ip.
	// But when proxied we need to check the forwarded ip address.
	// Note that this code always sets ip_forwarded[] as a side-effect for later use (the real client ip).
	//
	// check_ip_blacklist() is always called (not just for proxied connections as done previously)
	// since the internal blacklist is now used by the 24hr auto ban mechanism.

	char ip_forwarded[NET_ADDRSTRLEN];
    check_if_forwarded("CONN", mc, ip_forwarded);
	char *ip_unforwarded = ip_remote(mc);
    
    if (check_ip_blacklist(ip_forwarded) || check_ip_blacklist(ip_unforwarded)) return NULL;
    
    if (!kiwi.allow_admin_conns && timer_sec() > 60) {
        kiwi.allow_admin_conns = true;
        lprintf("WARNING: allow_admin_conns still unset > 60 seconds after startup\n");
    }
    
	if (down || update_in_progress || backup_in_progress) {
		conn_printf("down=%d UIP=%d BIP=%d stream=%s\n", down, update_in_progress, backup_in_progress, st->uri);
        conn_printf("URL <%s> <%s> %s\n", mc->uri, mc->query, ip_forwarded);
        bool update_backup = (update_in_progress || backup_in_progress);

        // internal STREAM_SOUND connections don't understand "reason_disabled" API, see below
		if (st->type == STREAM_SOUND && !internal) {
			int type;
			const char *reason_disabled = NULL;

			if (!down && update_in_progress) {
				type = 1;
			} else
			if (!down && backup_in_progress) {
				type = 2;
			} else {
				bool error;
				reason_disabled = cfg_string("reason_disabled", &error, CFG_OPTIONAL);
				if (error) reason_disabled = "";
				type = 0;
			}
			
			char *reason_enc = kiwi_str_encode((char *) reason_disabled);
			conn_printf("send_msg_mc MSG reason=<%s> down=%d\n", reason_disabled, type);
			send_msg_mc(mc, SM_NO_DEBUG, "MSG reason_disabled=%s down=%d", reason_enc, type);
			cfg_string_free(reason_disabled);
			kiwi_ifree(reason_enc, "reason_enc");
            //printf("DOWN %s %s\n", rx_streams[st->type].uri, ip_forwarded);
			return NULL;
		} else
		if (internal && update_backup) {
			return NULL;
		}

		if (st->type != STREAM_ADMIN && !internal) {
            //printf("DOWN %s %s\n", rx_streams[st->type].uri, ip_forwarded);
			return NULL;
		}

		// should only get here for admin connections or internal connections when not update/backup
	}

    bool isRetry = false;
retry:
	conn_printf("CONN LOOKING for free conn for type=%d(%s%s%s%s%s%s%s) ip=%s:%d:%016llx mc=%p\n",
	    st->type, st->uri,
	    isWB_conn? ",WIDEBAND" : "", internal? ",INTERNAL" : "", isKiwi_UI? "" : ",NON-KIWI",
	    (ws_flags & WS_FL_PREEMPT_AUTORUN)? ",PREEMPT" : "", (ws_flags & WS_FL_IS_AUTORUN)? ",AUTORUN" : "",
	    isRetry? ",RETRY" : "",
	    ip_forwarded, mc->remote_port, tstamp, mc);
	bool multiple = false;
	int cn, cnfree;
	conn_t *cfree = NULL, *cother = NULL;
	bool snd_or_wf = (st->type == STREAM_SOUND || st->type == STREAM_WATERFALL);
	bool snd_or_wf_or_ext = (snd_or_wf || st->type == STREAM_EXT);
	int mon_total = 0;
	
	for (c = conns, cn=0; c < &conns[N_CONNS]; c++, cn++) {
		assert(c->magic == CN_MAGIC);

		// cull conns stuck in STOP_DATA state (Novosibirsk problem)
		if (c->valid && c->stop_data && c->mc == NULL) {
			//clprintf(c, "STOP_DATA cull conn-%02d %s rx_chan=%d\n", c->self_idx, rx_conn_type(c), c->rx_channel);
			rx_enable(c->rx_channel, RX_CHAN_FREE);
			rx_server_remove(c);
		}
		
		if (!c->valid) {
			if (!cfree) { cfree = c; cnfree = cn; }     // remember first free conn
			//printf("CONN-%d !VALID\n", cn);
			continue;
		}
		
		conn_printf("search CONN-%02d is %p type=%d(%s) ip=%s:%d:%016llx rx=%d auth=%d other=%s%ld mc=%p\n", cn, c, c->type, rx_conn_type(c),
		    c->remote_ip, c->remote_port, c->tstamp, c->rx_channel, c->auth, c->other? "CONN-":"", c->other? c->other-conns:-1, c->mc);

        // Link streams to each other, e.g. snd <=> wf, snd => ext
        // If using the new tstamp space ignore IP address matching to compensate for VPN services that
        // fragment the source IP into multiple addresses (e.g. Cloudflare WARP).
		if (c->tstamp == tstamp && ((tstamp & NEW_TSTAMP_SPACE) || strcmp(ip_forwarded, c->remote_ip) == 0)) {
			if (snd_or_wf_or_ext && c->type == st->type) {
				conn_printf("CONN-%02d DUPLICATE!\n", cn);
				return NULL;
			}

			if (st->type == STREAM_SOUND && (c->type == STREAM_WATERFALL || c->type == STREAM_MONITOR)) {
				if (!multiple) {
					cother = c;
					multiple = true;
					#ifdef CONN_PRINTF
					    conn_printf("NEW SND, OTHER is %s @ CONN-%02d\n", rx_conn_type(c), cn);
					    //dump();
					#endif
				} else {
					printf("NEW SND, MULTIPLE OTHERS!\n");
					return NULL;
				}
			}

			if (st->type == STREAM_WATERFALL && (c->type == STREAM_SOUND || c->type == STREAM_MONITOR)) {
				if (!multiple) {
					cother = c;
					multiple = true;
					#ifdef CONN_PRINTF
					    conn_printf("NEW WF, OTHER is %s @ CONN-%02d\n", rx_conn_type(c), cn);
					    //dump();
					#endif
				} else {
					printf("NEW WF, MULTIPLE OTHERS!\n");
					return NULL;
				}
			}

			if (st->type == STREAM_EXT && c->type == STREAM_SOUND) {
				if (!multiple) {
					cother = c;
					multiple = true;
					#ifdef CONN_PRINTF
					    conn_printf("NEW EXT, OTHER is %s @ CONN-%02d\n", rx_conn_type(c), cn);
					    //dump();
					#endif
				} else {
					printf("NEW EXT, MULTIPLE OTHERS!\n");
					return NULL;
				}
			}
		}
		
		if (c->type == STREAM_MONITOR) mon_total++;
	}
	
	if (c == &conns[N_CONNS]) {
		if (cfree) {
			c = cfree;
			cn = cnfree;
			assert(cn >= 0);    // keep static analyzer quiet
		} else {
			conn_printf("(too many network connections open for %s)\n", st->uri);
			if (st->type != STREAM_WATERFALL && !internal)
			    send_msg_mc(mc, SM_NO_DEBUG, "MSG too_busy=%d", rx_chans);
			return NULL;
		}
	}

	mc->connection_param = c;
	conn_init(c);
	if (internal) c->internal_connection = true;
	c->type = st->type;
	c->other = cother;

	if (snd_or_wf) {
		int rx_n, heavy;

        #ifdef USE_WB
            if (kiwi.isWB) {
                rx_chan_t *rxc = &rx_channels[RX_CHAN0];
                if (isWB_conn) {
                    printf("isWB isWB_conn rx_channels[RX_CHAN0]=%d|%d|%d\n", rxc->chan_enabled, rxc->data_enabled, rxc->busy);
                    rx_n = rxc->busy? -1 : RX_CHAN0;
                } else {
                    rx_n = -1;
                }
                if (rx_n == -1) {
                    if (!internal) send_msg_mc(mc, SM_NO_DEBUG, "MSG wb_only");
                    mc->connection_param = NULL;
                    conn_init(c);
                    return NULL;
                }
                rxc->busy = 1;
            } else
                // non-wb firmware: fall through to normal rx connect block below
                // which will always fail because rx_n == -1
        #endif
        
        {
            if (!cother) {
                // if autorun on configurations with limited wf chans (e.g. rx8_wf2) never use the wf chans at all
                rx_free_count_e wf_flags = ((ws_flags & WS_FL_IS_AUTORUN) && !(ws_flags & WS_FL_INITIAL))? RX_COUNT_NO_WF_AT_ALL : RX_COUNT_NO_WF_FIRST;
                rx_free_count_e flags = ((isKiwi_UI || isWF_conn) && !isNo_WF)? RX_COUNT_ALL : wf_flags;
                int inuse = rx_chans - rx_chan_free_count(flags, &rx_n, &heavy);
                conn_printf("%s cother=%p isKiwi_UI=%d isWF_conn=%d isNo_WF=%d inuse=%d/%d use_rx=%d heavy=%d locked=%d %s\n",
                    st->uri, cother, isKiwi_UI, isWF_conn, isNo_WF, inuse, rx_chans, rx_n, heavy, is_locked,
                    (flags == RX_COUNT_ALL)? "RX_COUNT_ALL" : ((flags == RX_COUNT_NO_WF_FIRST)? "RX_COUNT_NO_WF_FIRST" : "RX_COUNT_NO_WF_AT_ALL"));
            
                if (is_locked) {
                    if (inuse == 0) {
                        printf("DRM note: locked but no channels in use?\n");
                        is_locked = 0;
                    } else {
                        printf("DRM nreg_chans=%d inuse=%d heavy=%d (is_locked=1)\n", drm_nreg_chans, inuse, heavy);
                        if (inuse > drm_nreg_chans) {
                            printf("DRM (locked for exclusive use %s)\n", st->uri);
                            if (!internal) send_msg_mc(mc, SM_NO_DEBUG, "MSG exclusive_use");
                            mc->connection_param = NULL;
                            conn_init(c);
                            return NULL;
                        }
                    }
                }
    
                if (rx_n == -1 || force_camp) {
                    //cprintf(c, "rx=%d force_camp=%d\n", rx_n, force_camp);
                    if (force_camp) {
                        rx_n = -1;
                        force_camp = false;
                    } else {
                        // Attempt to kick a channel using autorun.
                        // Be careful not to let an autorun process kick another autorun.
                        bool ok_kiwi = (isKiwi_UI && !internal);
                        bool ok_non_kiwi = (!isKiwi_UI && !internal && any_preempt_autorun);    // e.g. kiwirecorder
                        bool ok_internal = (!isKiwi_UI && internal && (ws_flags & WS_FL_PREEMPT_AUTORUN));  // e.g. SNR_meas
                        conn_printf("CONN check preempt kick: any_preempt_autorun=%d isKiwi_UI=%d internal=%d ws_flags=%d | ok_kiwi=%d ok_non_kiwi=%d ok_internal=%d\n",
                            any_preempt_autorun, isKiwi_UI, internal, ws_flags, ok_kiwi, ok_non_kiwi, ok_internal);
                        if (ok_kiwi || ok_non_kiwi || ok_internal) {
                            for (i = 0; i < rx_chans; i++) {
                                int victim;
                                if ((victim = rx_autorun_find_victim()) != -1) {
                                    rx_n = victim;
                                    c = rx_channels[rx_n].conn;
                                    c->preempted = true;
                                    rx_enable(rx_n, RX_CHAN_FREE);
                                    rx_server_remove(c);
                                    isRetry = true;
                                    goto retry;
                                }
                            }
                        }
                    }
                    
                    #ifdef USE_SDR
                        if (isKiwi_UI && (mon_total < monitors_max)) {
                            // turn first connection when no channels (SND or WF) into MONITOR
                            c->type = STREAM_MONITOR;
                            st = &rx_streams[STREAM_MONITOR];
                            snd_or_wf_or_ext = snd_or_wf = false;
                            conn_printf("STREAM_MONITOR 1st OK %s conn-%ld\n", st->uri, c-conns);
                        } else
                    #endif
                    {
                        char *url_redirect = (char *) admcfg_string("url_redirect", NULL, CFG_REQUIRED);
                        if (url_redirect != NULL && *url_redirect != '\0' && !internal) {
                            conn_printf("(too many rx channels open for %s -- redirect to %s)\n", st->uri, url_redirect);
                            send_msg_mc_encoded(mc, "MSG", "redirect", "%s", url_redirect);
                        } else {
                            conn_printf("(too many rx channels open for %s)\n", st->uri);
                            if (!internal) send_msg_mc(mc, SM_NO_DEBUG, "MSG too_busy=%d", rx_chans);
                        }
                        admcfg_string_free(url_redirect);
    
                        mc->connection_param = NULL;
                        conn_init(c);
                        return NULL;
                    }
                } else {
                    if (st->type == STREAM_WATERFALL && rx_n >= wf_chans) {
                
                        // Kiwi UI handles no-WF condition differently -- don't send error
                        if (!isKiwi_UI) {
                            conn_printf("(case 1: too many wf channels open for %s)\n", st->uri);
                            if (!internal) send_msg_mc(mc, SM_NO_DEBUG, "MSG too_busy=%d", rx_chans);
                            mc->connection_param = NULL;
                            conn_init(c);
                            return NULL;
                        }
                    }
                }
                
                if (rx_n != -1) {
                    conn_printf("CONN-%02d no other, new alloc rx%d\n", cn, rx_n);
                    rx_chan_t *rxc;
                    rxc = &rx_channels[rx_n];
                    memset(rxc, 0, sizeof(rx_chan_t));
                    rxc->busy = true;
                }
            } else {
                conn_printf("### %s cother=%p isKiwi_UI=%d isNo_WF=%d isWF_conn=%d\n",
                    st->uri, cother, isKiwi_UI, isNo_WF, isWF_conn);
    
                if (cother->type == STREAM_MONITOR) {   // sink second connection
                    conn_printf("STREAM_MONITOR 2nd SINK %s conn-%ld other conn-%d\n", st->uri, c-conns, cother->self_idx);
                    mc->connection_param = NULL;
                    conn_init(c);
                    return NULL;
                }
    
                if (st->type == STREAM_WATERFALL && cother->rx_channel >= wf_chans) {
    
                    // Kiwi UI handles no-WF condition differently -- don't send error
                    if (!isKiwi_UI) {
                        conn_printf("(case 2: too many wf channels open for %s)\n", st->uri);
                        mc->connection_param = NULL;
                        conn_init(c);
                        return NULL;
                    }
                }
                
                rx_n = -1;
                cother->other = c;
            }
        }
		
		c->rx_channel = cother? cother->rx_channel : rx_n;
		if (st->type == STREAM_SOUND && c->rx_channel != -1) {
		    rx_channels[c->rx_channel].conn = c;
		    c->isMaster = true;
		    if (cother) cother->isMaster = false;
		}
		
		// e.g. for WF-only kiwirecorder connections (won't override above)
		if (st->type == STREAM_WATERFALL && c->rx_channel != -1 && rx_channels[c->rx_channel].conn == NULL) {
		    rx_channels[c->rx_channel].conn = c;
		    c->isMaster = true;
		}

        const char *cp;
        if (mc->query && (cp = strstr(mc->query, "foff=")) != NULL && sscanf(cp, "foff=%lf", &c->foff_in_URL) == 1) {
            if (c->foff_in_URL < 0 || c->foff_in_URL > 100e9) c->foff_in_URL = 0;
            c->foff_set_in_URL = true;
        }
	}
	
	if (st->type == STREAM_EXT) {
	    if (c->other == NULL) {
	        printf("NEW EXT, DID NOT FIND OTHER SND CONN! (NULL) type=%d(%s) ip=%s:%d:%016llx\n",
	            st->type, st->uri, ip_forwarded, mc->remote_port, tstamp);
            dump();
	        return NULL;
	    }
	    if (c->other->rx_channel == -1) {
	        printf("NEW EXT, DID NOT FIND OTHER SND CONN! (rx_channel == -1) type=%d(%s) ip=%s:%d:%016llx\n",
	            st->type, st->uri, ip_forwarded, mc->remote_port, tstamp);
            dump();
	        return NULL;
	    }
	    c->rx_channel = c->other->rx_channel;
	}
  
	c->mc = mc;
    kiwi_strncpy(c->remote_ip, ip_forwarded, NET_ADDRSTRLEN);
	c->remote_port = mc->remote_port;
	bool is_loopback;
	c->isLocal_ip = isLocal_ip(c->remote_ip, &is_loopback);
	if (is_loopback) c->isLocal_ip = true;
	c->tstamp = tstamp;
	ndesc_init(&c->s2c, mc);
	ndesc_init(&c->c2s, mc);
	c->arrival = timer_sec();
	c->isWF_conn = !isNo_WF;
	clock_conn_init(c);
	conn_printf("NEW %s channel RX%d\n", st->uri, c->rx_channel);
	
	if (st->f != NULL) {
		c->task_func = st->f;
    	if (snd_or_wf_or_ext)
    		asprintf(&c->tname, "%s-%d", st->uri, c->rx_channel);
    	else
    		asprintf(&c->tname, "%s[%02d]", st->uri, c->self_idx);
    	u4_t flags = CTF_TNAME_FREE;    // ask TaskRemove to free name so debugging has longer access to it
    	if (c->rx_channel != -1) flags |= CTF_RX_CHANNEL | (c->rx_channel & CTF_CHANNEL);
    	if (isWF_conn) flags |= CTF_STACK_MED;
    	flags |= CTF_SOFT_FAIL;
    	if (ws_flags & WS_FL_NO_LOG) flags |= CTF_NO_LOG;
		int id = CreateTaskSF(rx_stream_tramp, c->tname, c, (st->priority == TASK_MED_PRIORITY)? task_medium_priority : st->priority, flags, 0);
		
		if (id < 0) {
	        conn_printf("CONN-%02d %p NO TASKS AVAILABLE\n", cn, c);
            kiwi_asfree((void *) c->tname);
            mc->connection_param = NULL;
			rx_enable(c->rx_channel, RX_CHAN_FREE);
            conn_init(c);
		    return NULL;
		}
		
	    conn_printf("CONN-%02d %p CreateTask %s\n", cn, c, Task_ls(id));
		c->task = id;
	}
	
	conn_printf("CONN-%02d %p <=== USE THIS ONE\n", cn, c);
	c->valid = true;
	return c;
}

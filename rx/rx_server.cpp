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
#include "data_pump.h"

#if RX_CHANS
 #include "ext_int.h"
#endif

#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <sched.h>
#include <math.h>
#include <signal.h>
#include <fftw3.h>

conn_t conns[N_CONNS];

rx_chan_t rx_channels[RX_CHANS];

// NB: must be in conn_t.type order
stream_t streams[] = {
	{ AJAX_VERSION,		"VER" },
	{ STREAM_ADMIN,		"admin",	&c2s_admin,		&c2s_admin_setup,		&c2s_admin_shutdown,	ADMIN_PRIORITY },
#if RX_CHANS
	{ STREAM_SOUND,		"SND",		&c2s_sound,		&c2s_sound_setup,		NULL,                   SND_PRIORITY },
	{ STREAM_WATERFALL,	"W/F",		&c2s_waterfall,	&c2s_waterfall_setup,	NULL,                   WF_PRIORITY },
	{ STREAM_MFG,		"mfg",		&c2s_mfg,		&c2s_mfg_setup,			NULL,                   ADMIN_PRIORITY },
	{ STREAM_EXT,		"EXT",		&extint_c2s,	&extint_setup_c2s,		NULL,                   EXT_PRIORITY },

	// AJAX requests
	{ AJAX_DISCOVERY,	"DIS" },
	{ AJAX_PHOTO,		"PIX" },
	{ AJAX_STATUS,		"status" },
#endif
	{ 0 }
};

static void conn_init(conn_t *c)
{
	memset(c, 0, sizeof(conn_t));
	c->magic = CN_MAGIC;
	c->self = c;
	c->self_idx = c - conns;
	c->rx_channel = -1;
	c->ext_rx_chan = -1;
}

void rx_enable(int chan, rx_chan_action_e action)
{
	rx_chan_t *rx = &rx_channels[chan];
	
	switch (action) {

	case RX_CHAN_ENABLE: rx->enabled = true; break;
	case RX_CHAN_DISABLE: rx->enabled = false; break;
	case RX_CHAN_FREE: memset(rx, 0, sizeof(rx_chan_t)); break;
	default: panic("rx_enable"); break;

	}
	
	data_pump_start_stop();
}

int rx_chan_free(int *idx)
{
	int i, free_cnt = 0, free_idx = -1;
	rx_chan_t *rx;

	for (i = 0; i < RX_CHANS; i++) {
		rx = &rx_channels[i];
		if (!rx->busy) {
			free_cnt++;
			if (free_idx == -1) free_idx = i;
		}
	}
	
	if (idx != NULL) *idx = free_idx;
	return free_cnt;
}

void dump()
{
	int i;
	
	for (i=0; i < RX_CHANS; i++) {
		rx_chan_t *rx = &rx_channels[i];
		lprintf("RX%d en%d busy%d conn%d-%p\n", i, rx->enabled, rx->busy,
			rx->conn_snd? rx->conn_snd->self_idx : 9999, rx->conn_snd? rx->conn_snd : 0);
	}

	conn_t *cd;
	for (cd = conns, i=0; cd < &conns[N_CONNS]; cd++, i++) {
		if (cd->valid) {
			lprintf("CONN%02d-%p %s%s rx=%d auth/admin=%d/%d KA=%02d/60 KC=%05d mc=%9p magic=0x%x ip=%s:%d other=%s%d %s%s\n",
				i, cd, streams[cd->type].uri, cd->internal_connection? "(INT)":"",
				(cd->type == STREAM_EXT)? cd->ext_rx_chan : cd->rx_channel,
				cd->auth, cd->auth_admin, cd->keep_alive, cd->keepalive_count, cd->mc, cd->magic,
				cd->remote_ip, cd->remote_port, cd->other? "CONN":"", cd->other? cd->other-conns:-1,
				(cd->type == STREAM_EXT)? (cd->ext? cd->ext->name : "?") : "", cd->stop_data? " STOP_DATA":"");
			if (cd->arrived)
				lprintf("       user=<%s> isUserIP=%d geo=<%s>\n", cd->user, cd->isUserIP, cd->geo);
		}
	}
	
	TaskDump(TDUMP_LOG | TDUMP_HIST | PRINTF_LOG);
	lock_dump();
}

static void dump_conn()
{
	int i;
	conn_t *cd;
	for (cd = conns, i=0; cd < &conns[N_CONNS]; cd++, i++) {
		lprintf("dump_conn: CONN-%02d %p valid=%d type=%d [%s] auth=%d KA=%d/60 KC=%d mc=%p rx=%d %s magic=0x%x ip=%s:%d other=%s%d %s\n",
			i, cd, cd->valid, cd->type, streams[cd->type].uri, cd->auth, cd->keep_alive, cd->keepalive_count, cd->mc, cd->rx_channel,
			cd->magic, cd->remote_ip, cd->remote_port, cd->other? "CONN-":"", cd->other? cd->other-conns:0, cd->stop_data? "STOP":"");
	}
	rx_chan_t *rc;
	for (rc = rx_channels, i=0; rc < &rx_channels[RX_CHANS]; rc++, i++) {
		lprintf("dump_conn: RX_CHAN-%d en %d busy %d conn = %s%d %p\n",
			i, rc->enabled, rc->busy, rc->conn_snd? "CONN-":"", rc->conn_snd? rc->conn_snd-conns:0, rc->conn_snd);
	}
}

// invoked by "make reload" command which will send SIGUSR1 to the kiwi server process
static void cfg_reload_handler(int arg)
{
	lprintf("SIGUSR1: reloading configuration, dx list..\n");
	cfg_reload(NOT_CALLED_FROM_MAIN);

	struct sigaction act;
	act.sa_flags = 0;
	sigemptyset(&act.sa_mask);
	act.sa_handler = cfg_reload_handler;
	scall("SIGUSR1", sigaction(SIGUSR1, &act, NULL));
}

// can optionally configure SIGUSR1 to call this debug handler
static void debug_dump_handler(int arg)
{
	lprintf("\n");
	lprintf("SIGUSR1: debugging..\n");
	dump();

	struct sigaction act;
	act.sa_flags = 0;
	sigemptyset(&act.sa_mask);
	act.sa_handler = debug_dump_handler;
	scall("SIGUSR1", sigaction(SIGUSR1, &act, NULL));
}

cfg_t cfg_ipl;

void rx_server_init()
{
	int i, j;
	
	conn_t *c = conns;
	for (i=0; i<N_CONNS; i++) {
		conn_init(c);
		ndesc_register(&c->c2s);
		ndesc_register(&c->s2c);
		c++;
	}
	
	// SIGUSR2 is now used exclusively by TaskCollect()
	struct sigaction act;
	memset(&act, 0, sizeof(act));
	sigemptyset(&act.sa_mask);
	
	#if 1
	act.sa_handler = debug_dump_handler;
	scall("SIGUSR1", sigaction(SIGUSR1, &act, NULL));
	#else
	act.sa_handler = cfg_reload_handler;
	scall("SIGUSR1", sigaction(SIGUSR1, &act, NULL));
	#endif

	update_vars_from_config();      // add any missing config vars
	
	// if not overridden in command line, set enable server according to configuration param
	if (!down) {
		bool error;
		bool server_enabled = admcfg_bool("server_enabled", &error, CFG_OPTIONAL);
		if (error || server_enabled == TRUE)
			down = FALSE;
		else
			down = TRUE;
	}

    json_init(&cfg_ipl, (char *) "{}");
}

void loguser(conn_t *c, logtype_e type)
{
	char *s;
	u4_t now = timer_sec();
	u4_t t = now - c->arrival;
	u4_t sec = t % 60; t /= 60;
	u4_t min = t % 60; t /= 60;
	u4_t hr = t;

	if (type == LOG_ARRIVED) {
		asprintf(&s, "(ARRIVED)");
		c->last_tune_time = now;
	} else
	if (type == LOG_LEAVING) {
		asprintf(&s, "(LEAVING after %d:%02d:%02d)", hr, min, sec);
	} else {
		asprintf(&s, "%d:%02d:%02d%s", hr, min, sec, (type == LOG_UPDATE_NC)? " n/c":"");
	}
	
	if (type == LOG_ARRIVED || type == LOG_LEAVING) {
		int ext_chan = c->rx_channel;
		clprintf(c, "%8.2f kHz %3s z%-2d %s%s\"%s\"%s%s%s%s %s\n", (float) c->freqHz / kHz + freq_offset,
			kiwi_enum2str(c->mode, mode_s, ARRAY_LEN(mode_s)), c->zoom,
			c->ext? c->ext->name : "", c->ext? " ":"",
			c->user, c->isUserIP? "":" ", c->isUserIP? "":c->remote_ip, c->geo? " ":"", c->geo? c->geo:"", s);
	}
	
	// we don't do anything with LOG_UPDATE and LOG_UPDATE_NC at present
	free(s);
}

void rx_server_remove(conn_t *c)
{
    stream_t *st = &streams[c->type];
    if (st->shutdown) (st->shutdown)((void *) c);
    
	c->stop_data = TRUE;
	c->mc = NULL;

	if (c->arrived) loguser(c, LOG_LEAVING);
	webserver_connection_cleanup(c);
	if (c->user) kiwi_free("user", c->user);
	if (c->geo) free(c->geo);
	if (c->pref_id) free(c->pref_id);
	if (c->pref) free(c->pref);
	
	int task = c->task;
	conn_init(c);
	check_for_update(WAIT_UNTIL_NO_USERS, NULL);
	//printf("### rx_server_remove %s\n", Task_ls(task));
	TaskRemove(task);
}

int rx_server_users()
{
	int users=0, any=0;
	
	conn_t *c = conns;
	for (int i=0; i < N_CONNS; i++) {
		//if (c->valid && c->type == STREAM_SOUND && c->arrived) users++;
		if (c->valid && c->type == STREAM_SOUND) users++;
		if (c->valid && (c->type == STREAM_SOUND || c->type == STREAM_WATERFALL)) any = 1;
		c++;
	}
	return (users? users : any);
}

void rx_server_user_kick(int chan)
{
	// kick users off (all or individual channel)
	printf("rx_server_user_kick rx=%d\n", chan);
	conn_t *c = conns;
	for (int i=0; i < N_CONNS; i++, c++) {
		if (!c->valid)
			continue;

		if (c->type == STREAM_SOUND || c->type == STREAM_WATERFALL) {
		    if (chan == -1 || chan == c->rx_channel) {
                c->kick = true;
                if (chan != -1)
                    printf("rx_server_user_kick KICKING rx=%d %s\n", chan, streams[c->type].uri);
            }
		} else
		
		if (c->type == STREAM_EXT) {
		    if (chan == -1 || chan == c->ext_rx_chan) {
                c->kick = true;
                if (chan != -1)
                    printf("rx_server_user_kick KICKING rx=%d EXT %s\n", chan, c->ext->name);
            }
		}
	}
}

void rx_server_send_config(conn_t *conn)
{
	// SECURITY: only send configuration after auth has validated
	assert(conn->auth == true);

	char *json = cfg_get_json(NULL);
	if (json != NULL) {
		send_msg(conn, SM_NO_DEBUG, "MSG version_maj=%d version_min=%d", version_maj, version_min);
		send_msg_encoded(conn, "MSG", "load_cfg", "%s", json);

		// send admin config ONLY if this is an authenticated connection from the admin page
		if (conn->type == STREAM_ADMIN && conn->auth_admin) {
			char *adm_json = admcfg_get_json(NULL);
			if (adm_json != NULL) {
				send_msg_encoded(conn, "MSG", "load_adm", "%s", adm_json);
			}
		}
	}
}

void stream_tramp(void *param)
{
	conn_t *conn = (conn_t *) param;
	(conn->task_func)(param);
}

// if this connection is new, spawn new receiver channel with sound/waterfall tasks
conn_t *rx_server_websocket(websocket_mode_e mode, struct mg_connection *mc)
{
	int i;
	conn_t *c;
	stream_t *st;

    c = (conn_t*) mc->connection_param;
    if (c) {	// existing connection
        
        if (c->magic != CN_MAGIC || !c->valid || mc != c->mc || mc->remote_port != c->remote_port) {
            if (mode != WS_MODE_ALLOC && mode != WS_INTERNAL_CONN) return NULL;
        #if 0
            lprintf("rx_server_websocket(%s): BAD CONN MC PARAM\n", (mode == WS_MODE_LOOKUP)? "lookup" : "alloc");
            lprintf("rx_server_websocket: (mc=%p == mc->c->mc=%p)? mc->c=%p mc->c->valid %d mc->c->magic=0x%x CN_MAGIC=0x%x mc->c->rport=%d\n",
                mc, c->mc, c, c->valid, c->magic, CN_MAGIC, c->remote_port);
            lprintf("rx_server_websocket: mc: %s:%d %s\n", mc->remote_ip, mc->remote_port, mc->uri);
            dump_conn();
            lprintf("rx_server_websocket: returning NULL\n");
        #endif
            return NULL;
        }
        
        if (mode == WS_MODE_CLOSE) {
            //cprintf(c, "WS_MODE_CLOSE %s KA=%02d/60 KC=%05d\n", streams[c->type].uri, c->keep_alive, c->keepalive_count);
            c->mc = NULL;
            c->kick = true;
            return NULL;
        }
    
        return c;	// existing connection is valid
    }
	
	// if we're doing anything other than allocating (e.g. lookup, close) we should have matched above
	if (mode != WS_MODE_ALLOC && mode != WS_INTERNAL_CONN)
		return NULL;
	
	// new connection needed
	const char *uri_ts = mc->uri;
	if (uri_ts[0] == '/') uri_ts++;
	//printf("#### new connection: %s:%d %s\n", mc->remote_ip, mc->remote_port, uri_ts);
	
	u64_t tstamp;
	char *uri_m = NULL;
	if (sscanf(uri_ts, "%lld/%256ms", &tstamp, &uri_m) != 2) {
		printf("bad URI_TS format\n");
		free(uri_m);
		return NULL;
	}
	
	for (i=0; streams[i].uri; i++) {
		st = &streams[i];
		
		if (strcmp(uri_m, st->uri) == 0)
			break;
	}
	
	if (!streams[i].uri) {
		lprintf("**** unknown stream type <%s>\n", uri_m);
        free(uri_m);
		return NULL;
	}
    free(uri_m);

	// handle case of server initially starting disabled, but then being enabled later by admin
#if RX_CHANS
	static bool init_snd_wf;
	if (!init_snd_wf && !down) {
		c2s_sound_init();
		c2s_waterfall_init();
		init_snd_wf = true;
	}
#endif

	if (down || update_in_progress || backup_in_progress) {
		//printf("down=%d UIP=%d stream=%s\n", down, update_in_progress, st->uri);
		if (st->type == STREAM_SOUND) {
			int type;
			const char *reason_disabled = NULL;

			int comp_ctr = 0;
			if (!down && update_in_progress) {
				FILE *fp;
				fp = fopen("/root/" REPO_NAME "/.comp_ctr", "r");
				if (fp != NULL) {
					int n = fscanf(fp, "%d\n", &comp_ctr);
					//printf(".comp_ctr %d\n", comp_ctr);
					fclose(fp);
				}
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
			//printf("send_msg_mc MSG comp_ctr=%d reason=<%s> down=%d\n", comp_ctr, reason_disabled, type);
			send_msg_mc(mc, SM_NO_DEBUG, "MSG comp_ctr=%d reason_disabled=%s down=%d", comp_ctr, reason_enc, type);
			cfg_string_free(reason_disabled);
			free(reason_enc);
			return NULL;
		}

		// always allow admin connections
		if (st->type != STREAM_ADMIN)
			return NULL;
	}
	
	// determine real client ip if proxied
	char remote_ip[NET_ADDRSTRLEN];
    check_if_forwarded("CONN", mc, remote_ip);
    
	//printf("CONN LOOKING for free conn for type=%d(%s) ip=%s:%d mc=%p\n", st->type, st->uri, remote_ip, mc->remote_port, mc);
	bool multiple = false;
	int cn, cnfree;
	conn_t *cfree = NULL, *cother = NULL;
	bool snd_or_wf = (st->type == STREAM_SOUND || st->type == STREAM_WATERFALL);
	
	for (c=conns, cn=0; c<&conns[N_CONNS]; c++, cn++) {
		assert(c->magic == CN_MAGIC);

		// cull conns stuck in STOP_DATA state (Novosibirsk problem)
		if (c->valid && c->stop_data && c->mc == NULL) {
			clprintf(c, "STOP_DATA cull conn-%02d %s rx_chan=%d\n", c->self_idx, streams[c->type].uri, c->rx_channel);
			rx_enable(c->rx_channel, RX_CHAN_FREE);
			rx_server_remove(c);
		}
		
		if (!c->valid) {
			if (!cfree) { cfree = c; cnfree = cn; }
			//printf("CONN-%d !VALID\n", cn);
			continue;
		}
		
		//printf("CONN-%d IS %p type=%d(%s) tstamp=%lld ip=%s:%d rx=%d auth=%d other%s%ld mc=%p\n", cn, c, c->type, streams[c->type].uri, c->tstamp,
		//    c->remote_ip, c->remote_port, c->rx_channel, c->auth, c->other? "=CONN-":"=", c->other? c->other-conns:0, c->mc);
		if (c->tstamp == tstamp && (strcmp(remote_ip, c->remote_ip) == 0)) {
			if (snd_or_wf && c->type == st->type) {
				//printf("CONN-%d DUPLICATE!\n", cn);
				return NULL;
			}
			if (st->type == STREAM_SOUND && c->type == STREAM_WATERFALL) {
				if (!multiple) {
					cother = c;
					multiple = true;
					//printf("CONN-%d OTHER WF @ CONN-%ld\n", cn, c-conns);
				} else {
					printf("CONN-%d MULTIPLE OTHER!\n", cn);
					return NULL;
				}
			}
			if (st->type == STREAM_WATERFALL && c->type == STREAM_SOUND) {
				if (!multiple) {
					cother = c;
					multiple = true;
					//printf("CONN-%d OTHER SND @ CONN-%ld\n", cn, c-conns);
				} else {
					printf("CONN-%d MULTIPLE OTHER!\n", cn);
					return NULL;
				}
			}
		}
	}
	
	if (c == &conns[N_CONNS]) {
		if (cfree) {
			c = cfree;
			cn = cnfree;
		} else {
			//printf("(too many network connections open for %s)\n", st->uri);
			if (st->type != STREAM_WATERFALL) send_msg_mc(mc, SM_NO_DEBUG, "MSG too_busy=%d", RX_CHANS);
			return NULL;
		}
	}

	mc->connection_param = c;
	conn_init(c);
	if (mode == WS_INTERNAL_CONN) c->internal_connection = true;
	c->type = st->type;
	c->other = cother;

	if (snd_or_wf) {
		int rx;
		if (!cother) {
			int rx_free = rx_chan_free(&rx);
			if (rx == -1) {
				//printf("(too many rx channels open for %s)\n", st->uri);
				if (st->type == STREAM_SOUND) send_msg_mc(mc, SM_NO_DEBUG, "MSG too_busy=%d", RX_CHANS);
				mc->connection_param = NULL;
				conn_init(c);
				return NULL;
			}
			//printf("CONN-%d no other, new alloc rx%d\n", cn, rx);
			rx_channels[rx].busy = true;
		} else {
			rx = -1;
			cother->other = c;
		}
		c->rx_channel = cother? cother->rx_channel : rx;
		if (st->type == STREAM_SOUND) rx_channels[c->rx_channel].conn_snd = c;
	}
  
	c->mc = mc;
    kiwi_strncpy(c->remote_ip, remote_ip, NET_ADDRSTRLEN);
	c->remote_port = mc->remote_port;
	c->tstamp = tstamp;
	ndesc_init(&c->s2c, mc);
	ndesc_init(&c->c2s, mc);
	c->ui = find_ui(mc->local_port);
	assert(c->ui);
	c->arrival = timer_sec();
	clock_conn_init(c);
	//printf("NEW channel %d\n", c->rx_channel);
	
	if (st->f != NULL) {
		c->task_func = st->f;
    	if (snd_or_wf)
    		asprintf(&c->tname, "%s-%d", st->uri, c->rx_channel);
    	else
    		asprintf(&c->tname, "%s[%02d]", st->uri, c->self_idx);
    	u4_t flags = CTF_TNAME_FREE;    // ask TaskRemove to free name so debugging has longer access to it
    	if (c->rx_channel != -1) flags |= CTF_RX_CHANNEL | (c->rx_channel & CTF_CHANNEL);
		int id = CreateTaskSF(stream_tramp, c->tname, c, st->priority, flags, 0);
		c->task = id;
	}
	
	//printf("CONN-%d <=== USE THIS ONE\n", cn);
	c->valid = true;
	return c;
}

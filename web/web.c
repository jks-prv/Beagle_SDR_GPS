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

#include <string.h>
#include <time.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>

#include "kiwi.h"
#include "types.h"
#include "config.h"
#include "misc.h"
#include "timer.h"
#include "web.h"
#include "coroutines.h"
#include "mongoose.h"
#include "nbuf.h"
#include "cfg.h"
#include "ext_int.h"

// Copyright (c) 2014-2016 John Seamons, ZL/KF6VO

user_iface_t user_iface[] = {
	KIWI_UI_LIST
	{0}
};

user_iface_t *find_ui(int port)
{
	user_iface_t *ui = user_iface;
	while (ui->port) {
		if (ui->port == port)
			return ui;
		ui++;
	}
	return NULL;
}

void webserver_connection_cleanup(conn_t *c)
{
	nbuf_cleanup(&c->c2s);
	nbuf_cleanup(&c->s2c);
}


// c2s
// client to server
// 1) websocket: SET messages sent from .js via (ws).send(), received via websocket connection threads
//		no response returned (unless s2c send_msg*() done)
// 2) HTTP GET: normal browser file downloads, response returned (e.g. HTML, .css, .js, images)
// 3) HTTP GET: AJAX requests, response returned (e.g. "GET /status")
//		eliminating most of these in favor of websocket messages so connection auth can be performed
// 4) HTTP PUT: e.g. kiwi_ajax_send() upload photo file, response returned

extern const char *edata_embed(const char *, size_t *);
extern const char *edata_always(const char *, size_t *);

static const char* edata(const char *uri, size_t *size, char **free_buf)
{
	const char* data = NULL;
	
#ifdef EDATA_EMBED
	// the normal background daemon loads files from in-memory embedded data for speed
	data = edata_embed(uri, size);
#endif

	// some large, seldom-changed files are always loaded from memory
	if (!data) data = edata_always(uri, size);

#ifdef EDATA_EMBED
	// only root-referenced files are opened from filesystem when embedded
	if (uri[0] != '/')
		return data;
#endif

	// to speed edit-copy-compile-debug development, load the files from the local filesystem
	bool free_uri2 = false;
	char *uri2 = (char *) uri;

#ifdef EDATA_DEVEL
	if (uri[0] != '/') {
		asprintf(&uri2, "web/%s", uri);
		free_uri2 = true;
	}
#endif

	// try as a local file
	if (!data) {
		int fd = open(uri2, O_RDONLY);
		if (fd >= 0) {
			struct stat st;
			fstat(fd, &st);
			*size = st.st_size;
			data = (char *) kiwi_malloc("req-file", *size);
			*free_buf = (char *) data;
			ssize_t rsize = read(fd, (void *) data, *size);
			assert(rsize == *size);
			close(fd);
		}
	}

	if (free_uri2) free(uri2);
	return data;
}

struct iparams_t {
	char *id, *val;
};

#define	N_IPARAMS	64
static iparams_t iparams[N_IPARAMS];
static int n_iparams;

void index_params_cb(cfg_t *cfg, jsmntok_t *jt, int seq, int hit, int lvl, int rem)
{
	char *json = cfg_get_json(NULL);
	if (json == NULL || jt->type != JSMN_STRING)
		return;
	
	check(n_iparams < N_IPARAMS);
	iparams_t *ip = &iparams[n_iparams];
	char *s = &json[jt->start];
	int n = jt->end - jt->start;
	if (JSMN_IS_ID(jt)) {
		ip->id = (char *) malloc(n + SPACE_FOR_NULL);
		mg_url_decode(s, n, (char *) ip->id, n + SPACE_FOR_NULL, 0);
		//printf("index_params_cb: %d %d/%d/%d/%d ID %d <%s>\n", n_iparams, seq, hit, lvl, rem, n, ip->id);
	} else {
		ip->val = (char *) malloc(n + SPACE_FOR_NULL);
		// Leave it encoded in case it's a string. [not done currently because although this fixes
		// substitution in .js files it breaks inline substitution for HTML files]
		// Non-string types shouldn't have any escaped characters.
		//strncpy(ip->val, s, n); ip->val[n] = '\0';
		mg_url_decode(s, n, (char *) ip->val, n + SPACE_FOR_NULL, 0);
		//printf("index_params_cb: %d %d/%d/%d/%d %s: %s\n", n_iparams, seq, hit, lvl, rem, ip->id, ip->val);
		n_iparams++;
	}
}

void reload_index_params()
{
	int i;
	//printf("reload_index_params: free %d\n", n_iparams);
	for (i=0; i < n_iparams; i++) {
		free(iparams[i].id);
		free(iparams[i].val);
	}
	n_iparams = 0;
	//cfg_walk("index_html_params", cfg_print_tok);
	cfg_walk("index_html_params", index_params_cb);
	
	// add the list of extensions
	// FIXME move this outside of the repeated calls to reload_index_params
	iparams_t *ip = &iparams[n_iparams];
	asprintf(&ip->id, "EXT_LIST_JS");
	char *s = extint_list_js();
	asprintf(&ip->val, "%s", s);
	free(s);
	//printf("EXT_LIST_JS: %s", ip->val);
	n_iparams++;
}

// requests created by data coming in to the web server
static int request(struct mg_connection *mc) {
	int i;
	size_t edata_size=0;
	const char *edata_data;
	char *free_buf = NULL;

	if (mc->is_websocket) {
		// This handler is called for each incoming websocket frame, one or more
		// times for connection lifetime.
		char *s = mc->content;
		int sl = mc->content_len;
		//printf("WEBSOCKET: len %d uri <%s>\n", sl, mc->uri);
		if (sl == 0) {
			//printf("----KA %d\n", mc->remote_port);
			return MG_TRUE;	// keepalive?
		}
		
		conn_t *c = rx_server_websocket(mc, WS_MODE_ALLOC);
		if (c == NULL) {
			s[sl]=0;
			//if (!down) lprintf("rx_server_websocket(alloc): msg was %d <%s>\n", sl, s);
			return MG_FALSE;
		}
		if (c->stop_data) return MG_FALSE;
		
		nbuf_allocq(&c->c2s, s, sl);
		
		if (mc->content_len == 4 && !memcmp(mc->content, "exit", 4)) {
			//printf("----EXIT %d\n", mc->remote_port);
			return MG_FALSE;
		}
		
		return MG_TRUE;
	} else {
		if (strcmp(mc->uri, "/") == 0) mc->uri = "index.html"; else
		if (mc->uri[0] == '/') mc->uri++;
		
		char *ouri = (char *) mc->uri;
		char *uri;
		bool free_uri = FALSE, has_prefix = FALSE;
		
		//printf("URL <%s>\n", ouri);
		char *suffix = strrchr(ouri, '.');

		if (suffix && (strcmp(suffix, ".json") == 0 || strcmp(suffix, ".json/") == 0)) {
			lprintf("attempt to fetch config file: %s query=<%s> from %s\n", ouri, mc->query_string, mc->remote_ip);
			return MG_FALSE;
		}
		
		// if uri uses a subdir we know about just use the absolute path
		if (strncmp(ouri, "kiwi/", 5) == 0) {
			uri = ouri;
			has_prefix = TRUE;
		} else
		if (strncmp(ouri, "extensions/", 11) == 0) {
			uri = ouri;
			has_prefix = TRUE;
		} else
		if (strncmp(ouri, "pkgs/", 5) == 0) {
			uri = ouri;
			has_prefix = TRUE;
		} else
		if (strncmp(ouri, "config/", 7) == 0) {
			asprintf(&uri, "%s/%s", DIR_CFG, &ouri[7]);
			free_uri = TRUE;
			has_prefix = TRUE;
		} else
		if (strncmp(ouri, "kiwi.config/", 12) == 0) {
			asprintf(&uri, "%s/%s", DIR_CFG, &ouri[12]);
			free_uri = TRUE;
			has_prefix = TRUE;
		} else {
			// use name of active ui as subdir
			user_iface_t *ui = find_ui(mc->local_port);
			// should never not find match since we only listen to ports in ui table
			assert(ui);
			asprintf(&uri, "%s/%s", ui->name, ouri);
			free_uri = TRUE;
		}
		//printf("---- HTTP: uri %s (%s)\n", ouri, uri);

		// try as file from in-memory embedded data or local filesystem
		edata_data = edata(uri, &edata_size, &free_buf);
		
		// try again with ".html" appended
		if (!edata_data) {
			char *uri2;
			asprintf(&uri2, "%s.html", uri);
			if (free_uri) free(uri);
			uri = uri2;
			free_uri = TRUE;
			edata_data = edata(uri, &edata_size, &free_buf);
		}
		
		// try looking in "kiwi" subdir as a default
		if (!edata_data && !has_prefix) {
			if (free_uri) free(uri);
			asprintf(&uri, "kiwi/%s", ouri);
			free_uri = TRUE;

			// try as file from in-memory embedded data or local filesystem
			edata_data = edata(uri, &edata_size, &free_buf);
			
			// try again with ".html" appended
			if (!edata_data) {
				if (free_uri) free(uri);
				asprintf(&uri, "kiwi/%s.html", ouri);
				free_uri = TRUE;
				edata_data = edata(uri, &edata_size, &free_buf);
			}
		}

		// try as AJAX request
		bool isAJAX = false;
		if (!edata_data) {
			edata_data = rx_server_ajax(mc);	// mc->uri is ouri without ui->name prefix
			if (edata_data) {
				edata_size = strlen(edata_data);
				isAJAX = true;
			}
		}

		// give up
		if (!edata_data) {
			printf("unknown URL: %s (%s) query=<%s> from %s\n", ouri, uri, mc->query_string, mc->remote_ip);
			if (free_uri) free(uri);
			if (free_buf) kiwi_free("req-*", free_buf);
			return MG_FALSE;
		}
		
		// for *.html and *.css process %[substitution]
		// fixme: don't just panic because the config params are bad
		bool free_edata = false;
		suffix = strrchr(uri, '.');
		
		if (!isAJAX && suffix && (strcmp(suffix, ".html") == 0 || strcmp(suffix, ".css") == 0)) {
			int nsize = edata_size;
			char *html_buf = (char *) kiwi_malloc("html_buf", nsize);
			free_edata = true;
			char *cp = (char *) edata_data, *np = html_buf, *pp;
			int cl, sl, pl, nl;

			//printf("%%[ %s %d\n", uri, nsize);

			for (cl=nl=0; cl < edata_size;) {
				if (*cp == '%' && *(cp+1) == '[') {
					cp += 2; cl += 2; pp = cp; pl = 0;
					while (*cp != ']' && cl < edata_size) { cp++; cl++; pl++; }
					cp++; cl++;
					
					for (i=0; i < n_iparams; i++) {
						iparams_t *ip = &iparams[i];
						if (strncmp(pp, ip->id, pl) == 0) {
							sl = strlen(ip->val);

							// expand buffer
							html_buf = (char *) kiwi_realloc("html_buf", html_buf, nsize+sl);
							np = html_buf + nl;		// in case buffer moved
							nsize += sl;
							//printf("%d %%[%s %d <%s>\n", nsize, ip->id, sl, ip->val);
							strcpy(np, ip->val); np += sl; nl += sl;
							break;
						}
					}
					
					if (i == n_iparams) {
						// not found, put back original
						strcpy(np, "%["); np += 2;
						strncpy(np, pp, pl); np += pl;
						*np++ = ']';
						nl += pl + 3;
					}
				} else {
					*np++ = *cp++; nl++; cl++;
				}

				assert(nl <= nsize);
			}
			
			edata_data = html_buf;
			assert((np - html_buf) == nl);
			edata_size = nl;
		}

		mg_send_header(mc, "Content-Type", mg_get_mime_type(isAJAX? mc->uri : uri, "text/plain"));
		
		// needed by auto-discovery port scanner
		mg_send_header(mc, "Access-Control-Allow-Origin", "*");
		
		// add version checking to each .js file served
		if (!isAJAX && suffix && strcmp(suffix, ".js") == 0) {
			char *s;
			asprintf(&s, "kiwi_check_js_version.push({ VERSION_MAJ:%d, VERSION_MIN:%d, file:'%s' });\n\n", VERSION_MAJ, VERSION_MIN, uri);
			mg_send_data(mc, s, strlen(s));
			free(s);
		}

		mg_send_data(mc, edata_data, edata_size);
		
		if (free_edata) kiwi_free("html_buf", (void *) edata_data);
		if (free_uri) free(uri);
		if (free_buf) kiwi_free("req-*", free_buf);
		
		http_bytes += edata_size;
		return MG_TRUE;
	}
}

// events created by data coming in to the web server
static int ev_handler(struct mg_connection *mc, enum mg_event ev) {
  int r;
  
  //printf("ev_handler %d:%d len %d ", mc->local_port, mc->remote_port, (int) mc->content_len);
  if (ev == MG_REQUEST) {
  	//printf("MG_REQUEST: URI:%s query:%s\n", mc->uri, mc->query_string);
    r = request(mc);
    //printf("\n");
    return r;
  } else
  if (ev == MG_CLOSE) {
  	//printf("MG_CLOSE\n");
  	rx_server_websocket(mc, WS_MODE_CLOSE);
  	mc->connection_param = NULL;
    return MG_TRUE;
  } else
  if (ev == MG_AUTH) {
  	//printf("MG_AUTH\n");
    return MG_TRUE;
  } else {
  	//printf("MG_OTHER\n");
    return MG_FALSE;
  }
}

int web_to_app(conn_t *c, nbuf_t **nbp)
{
	nbuf_t *nb;
	
	if (c->stop_data) return 0;
	nb = nbuf_dequeue(&c->c2s);
	if (!nb) {
		*nbp = NULL;
		return 0;
	}
	assert(!nb->done && !nb->expecting_done && nb->buf && nb->len);
	nb->expecting_done = TRUE;
	*nbp = nb;
	
	return nb->len;
}

void web_to_app_done(conn_t *c, nbuf_t *nb)
{
	assert(nb->expecting_done && !nb->done);
	nb->expecting_done = FALSE;
	nb->done = TRUE;
}


// s2c
// server to client
// 1) websocket: {AUD, FFT} data streams received by .js via (ws).onmessage()
// 2) websocket: {MSG, ADM, MFG, EXT, DAT} messages sent by send_msg*(), received via open_websocket() msg_cb/recv_cb routines
// 3) 

void app_to_web(conn_t *c, char *s, int sl)
{
	if (c->stop_data) return;
	nbuf_allocq(&c->s2c, s, sl);
	//NextTask("s2c");
}

// polled send of data to web server
static int iterate_callback(struct mg_connection *mc, enum mg_event ev)
{
	int ret;
	nbuf_t *nb;
	
	if (ev == MG_POLL && mc->is_websocket) {
		conn_t *c = rx_server_websocket(mc, WS_MODE_LOOKUP);
		if (c == NULL)  return MG_FALSE;

		while (TRUE) {
			if (c->stop_data) break;
			nb = nbuf_dequeue(&c->s2c);
			//printf("s2c CHK port %d nb %p\n", mc->remote_port, nb);
			
			if (nb) {
				assert(!nb->done && nb->buf && nb->len);

				//#ifdef SND_SEQ_CHECK
				#if 0
				// check timing of audio output
				snd_pkt_t *out = (snd_pkt_t *) nb->buf;
				if (c->type == STREAM_SOUND && strncmp(out->h.id, "AUD ", 4) == 0) {
					u4_t now = timer_ms();
					if (!c->audio_check) {
						c->audio_epoch = now;
						c->audio_sequence = c->audio_pkts_sent = c->audio_last_time = c->sum2 = 0;
						c->audio_check = true;
					}
					double audio_rate = adc_clock / (RX1_DECIM * RX2_DECIM);
					u4_t expected1 = c->audio_epoch + (u4_t)((1.0/audio_rate * (512*4) * c->audio_pkts_sent)*1000.0);
					s4_t diff1 = (s4_t)(now - expected1);
					u4_t expected2 = (u4_t)((1.0/audio_rate * (512*4))*1000.0);
					s4_t diff2 = c->audio_last_time? (s4_t)((now - c->audio_last_time) - expected2) : 0;
					c->audio_last_time = now;
					#define DIFF1 30
					#define DIFF2 1
					if (diff1 < -DIFF1 || diff1 > DIFF1 || diff2 < -DIFF2 || diff2 > DIFF2) {
						printf("SND%d %4d Q%d d1=%6.3f d2=%6.3f/%6.3f %.6f %f\n",
							c->rx_channel, c->audio_sequence, nbuf_queued(&c->s2c)+1,
							(float)diff1/1e4, (float)diff2/1e4, (float)c->sum2/1e4,
							adc_clock/1e6, audio_rate);
					}
					c->sum2 += diff2;
					if (out->h.seq != c->audio_sequence) {
						printf("SND%d SEQ expecting %d got %d, %s -------------------------\n",
							c->rx_channel, c->audio_sequence, out->h.seq, c->user);
						c->audio_sequence = out->h.seq;
					}
					c->audio_sequence++;
					c->audio_pkts_sent++;
				}
				#endif

				//printf("s2c %d WEBSOCKET: %d %p\n", mc->remote_port, nb->len, nb->buf);
				ret = mg_websocket_write(mc, WS_OPCODE_BINARY, nb->buf, nb->len);
				if (ret<=0) printf("$$$$$$$$ socket write ret %d\n", ret);
				nb->done = TRUE;
			} else {
				break;
			}
		}
	} else {
		if (ev != MG_POLL) printf("$$$$$$$$ s2c %d OTHER: %d len %d\n", mc->remote_port, (int) ev, (int) mc->content_len);
	}
	
	//NextTask("web callback");
	
	return MG_TRUE;
}

void web_server(void *param)
{
	user_iface_t *ui = (user_iface_t *) param;
	struct mg_server *server = ui->server;
	const char *err;
	
	while (1) {
		mg_poll_server(server, 0);		// passing 0 effects a poll
		mg_iterate_over_connections(server, iterate_callback);
		TaskSleep(WEB_SERVER_POLL_US);
        TaskStat(TSTAT_INCR|TSTAT_ZERO, 0, 0, 0);
	}
}

void web_server_init(ws_init_t type)
{
	user_iface_t *ui = user_iface;
	static bool init;
	
	if (!init) {
		nbuf_init();
		init = TRUE;
	}
	
	if (type == WS_INIT_CREATE) {
		// if specified, override the default port number of the first UI
		int port;
		if (alt_port)
			port = alt_port;
		else
			port = admcfg_int("port", NULL, CFG_REQUIRED);
		if (port) {
			lprintf("listening on port %d for \"%s\"\n", port, ui->name);
			ui->port = port;
		} else {
			lprintf("listening on default port %d for \"%s\"\n", ui->port, ui->name);
		}
	} else

	if (type == WS_INIT_START) {
		reload_index_params();
		services_start(SVCS_RESTART_FALSE);
	}

	// create webserver port(s)
	while (ui->port) {
		if (type == WS_INIT_CREATE) {
			ui->server = mg_create_server(NULL, ev_handler);
			char *s_port;
			asprintf(&s_port, "[::]:%d", ui->port);
			if (mg_set_option(ui->server, "listening_port", s_port) != NULL) {
				lprintf("network port %s for \"%s\" in use\n", s_port, ui->name);
				lprintf("app already running in background?\ntry \"make stop\" (or \"m stop\") first\n");
				xit(-1);
			}
			lprintf("webserver for \"%s\" on port %s\n", ui->name, mg_get_option(ui->server, "listening_port"));
			free(s_port);
		} else {	// WS_INIT_START
			CreateTask(web_server, ui, WEBSERVER_PRIORITY);
		}
		ui++;
		if (down) break;
	}
}

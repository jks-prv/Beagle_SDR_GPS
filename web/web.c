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
#include "net.h"
#include "coroutines.h"
#include "mongoose.h"
#include "nbuf.h"
#include "cfg.h"
#include "str.h"
#include "rx.h"
#include "clk.h"
#include "spi.h"

// This file is compiled twice into two different object files:
// Once with EDATA_EMBED defined when installed as the production server in /usr/local/bin
// Once with EDATA_DEVEL defined when compiled into the build directory during development 

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


extern const char *edata_embed(const char *, size_t *);
extern const char *edata_always(const char *, size_t *);
u4_t mtime_obj_keep_edata_always_o;
bool web_caching_debug;

static const char* edata(const char *uri, bool cache_check, size_t *size, u4_t *mtime)
{
	const char* data = NULL;
	bool absPath = (uri[0] == '/');
	const char *type, *subtype, *reason;
	
    type = cache_check? "cache check" : "fetch file";

#ifdef EDATA_EMBED
	// The normal background daemon loads files from in-memory embedded data for speed.
	// In development mode these files are always loaded from the local filesystem.
	data = edata_embed(uri, size);
	if (data) {
		// In production mode the only thing we have is the server binary build time.
		// But this is okay since because that's the origin of the data and the binary is
		// only updated when a software update occurs.
		*mtime = timer_server_build_unix_time();
		if (cache_check) web_printf("----\n");
		subtype = "edata_embed file";
		reason = "using server build";
	}
#endif

	// some large, seldom-changed files are always loaded from memory, even in development mode
	if (!data) {
		data = edata_always(uri, size);
		if (data) {
			if (cache_check) web_printf("----\n");
		    subtype = "edata_always file";
#ifdef EDATA_EMBED
			// In production mode the only thing we have is the server binary build time.
			// But this is okay since because that's the origin of the data and the binary is
			// only updated when a software update occurs.
			*mtime = timer_server_build_unix_time();
			reason = "using server build";
#else
			// In development mode this is better than the constantly-changing server binary
			// (i.e. the obj_keep/edata_always.o file is rarely updated).
			// NB: mtime_obj_keep_edata_always_o is only updated once per server restart.
			*mtime = mtime_obj_keep_edata_always_o;
			reason = "using edata_always.o";
#endif
		}
	}

    if (data)
	    web_printf("EDATA           %s, %s, %s: mtime=%lu/%lx %s\n", type, subtype, reason, *mtime, *mtime, uri);

#ifdef EDATA_EMBED
	// only root-referenced files are opened from filesystem when in embedded (production) mode
	if (!absPath)
		return data;
#endif

	// to speed edit-copy-compile-debug development, load the files from the local filesystem
	bool free_uri2 = false;
	char *uri2 = (char *) uri;

#ifdef EDATA_DEVEL
	if (!absPath) {
		asprintf(&uri2, "web/%s", uri);
		free_uri2 = true;
	}
#endif

	// try as a local file
	// NB: in embedded mode this can be true if loading an extension from an absolute path,
	// so this code is not enclosed in an "#ifdef EDATA_DEVEL".
	if (!data) {

        // Since the file content must be known during the cache_check pass (for "%[" substitution dirty testing)
        // keep the content buffer for the possible fetch_file pass which might happen immediately after.
		static char *last_uri2_read;
		static const char *last_data, *last_free;
		static size_t last_size;
		static u4_t last_mtime;
		struct stat st;
		bool nofile = false;

        // NB: This is tricky. We must handle the freeing of the file buffer ourselves because it
        // cannot be done correctly by the caller. If we're holding the buffer after the cache check
        // for a possible file fetch, but the fetch never happens because the caller determines no
        // caching is allowed, then the caller never has a chance to free the buffer. We must do
        // that on the delayed entry to the next cache check.

		if (cache_check ||
		    (!cache_check && (last_uri2_read == NULL || strlen(uri2) != strlen(last_uri2_read) || strcmp(uri2, last_uri2_read) != 0))) {
            //printf(">CONSIDER cache_check=%d %s\n", cache_check, uri2);
            bool fail = false;
            int fd;
            
            struct stat st;
            if (stat(uri2, &st) < 0) {
                fail = true;
            } else
            if ((st.st_mode & S_IFMT) != S_IFREG) {
                fail = true;
            } else
            if ((fd = open(uri2, O_RDONLY)) < 0) {
                fail = true;
            } else {
                last_size = *size = st.st_size;
                last_mtime = st.st_mtime;

                if (last_free) {
                    //printf(">FREE %p\n", last_free);
                    kiwi_free("edata_file", (void *) last_free);
                }
                
                last_free = last_data = data = (char *) kiwi_malloc("edata_file", *size);
                ssize_t rsize = read(fd, (void *) data, *size);
                close(fd);
                if (rsize != *size) {
                    fail = true;
                } else {
                    //printf(">READ %p %d %s\n", last_data, last_size, uri2);
                    if (last_uri2_read != NULL) free(last_uri2_read);
                    last_uri2_read = strdup(uri2);
                }
            }
            
            if (fail) {
                if (last_uri2_read != NULL) free(last_uri2_read);
                last_uri2_read = NULL;
                nofile = true;
            }
		} else {
		    if (last_data != NULL) {
                //printf(">USE-ONCE %p %d %s\n", last_data, last_size, uri2);
                data = last_data;
                *size = last_size;
    
                // only use once
                if (last_uri2_read != NULL) free(last_uri2_read);
                last_uri2_read = NULL;
                last_data = NULL;
                // but note last_free remains valid until next cache_check
            }
		}

        if (cache_check) {
            web_printf("----\n");
        }
        
        type = cache_check? "cache check" : "fetch file";
		char *suffix = strrchr(uri2, '.');
		bool isJS = (suffix && strcmp(suffix, ".js") == 0);

        if (nofile) {
            *mtime = 0;
            reason = "NO FILE";
        } else

        // because of the version number appending, mtime has to be greater of file and server build time for .js files
        if (isJS && timer_server_build_unix_time() > last_mtime) {
            *mtime = timer_server_build_unix_time();
            reason = "is .js file, using newer server build (due to verno check)";
        } else {
            *mtime = last_mtime;
            if (isJS) {
                reason = "is .js file, using file";
            } else {
                reason = "not .js file, using file";
            }
        }
        web_printf("EDATA           %s, %s: mtime=%lu/%lx %s %s\n", type, reason, *mtime, *mtime, uri, uri2);
	}

	if (free_uri2) free(uri2);
	return data;
}

struct iparams_t {
	char *id, *encoded, *decoded;
};

#define	N_IPARAMS	256
static iparams_t iparams[N_IPARAMS];
static int n_iparams;

void iparams_add(const char *id, char *encoded)
{
    // Save encoded in case it's a string. This is needed for
    // substitution in .js files and decoded is needed for substitution in HTML files.
	iparams_t *ip = &iparams[n_iparams];
	asprintf(&ip->id, "%s", (char *) id);
	asprintf(&ip->encoded, "%s", encoded);
	int n = strlen(encoded);
    ip->decoded = (char *) malloc(n + SPACE_FOR_NULL);
    mg_url_decode(ip->encoded, n, ip->decoded, n + SPACE_FOR_NULL, 0);
    //printf("iparams_add: %d %s <%s>\n", n_iparams, ip->id, ip->decoded);
	n_iparams++;
}

bool index_params_cb(cfg_t *cfg, void *param, jsmntok_t *jt, int seq, int hit, int lvl, int rem, void **rval)
{
	char *json = cfg_get_json(NULL);
	if (json == NULL || rem == 0)
		return false;
	
    static char *id_last;

	check(n_iparams < N_IPARAMS);
	iparams_t *ip = &iparams[n_iparams];
	char *s = &json[jt->start];
	int n = jt->end - jt->start;
	if (JSMN_IS_ID(jt)) {
		id_last = (char *) malloc(n + SPACE_FOR_NULL);
		mg_url_decode(s, n, id_last, n + SPACE_FOR_NULL, 0);
		//printf("index_params_cb: %d %d/%d/%d/%d ID %d <%s>\n", n_iparams, seq, hit, lvl, rem, n, id_last);
	} else {
		char *encoded = (char *) malloc(n + SPACE_FOR_NULL);
		kiwi_strncpy(encoded, s, n + SPACE_FOR_NULL);
		//printf("index_params_cb: %d %d/%d/%d/%d VAL %s: <%s>\n", n_iparams, seq, hit, lvl, rem, id_last, encoded);
		iparams_add(id_last, encoded);
		free(id_last);
		free(encoded);
	}
	
	return false;
}

void reload_index_params()
{
	int i;
	
	// don't free previous on reload because not all were malloc()'d
	// (the memory loss is very small)
	n_iparams = 0;
	//cfg_walk("index_html_params", cfg_print_tok, NULL);
	cfg_walk("index_html_params", index_params_cb, NULL);

	char *cs = (char *) cfg_string("owner_info", NULL, CFG_REQUIRED);
	iparams_add("OWNER_INFO", cs);
	cfg_string_free(cs);

	// add the list of extensions
#if RX_CHANS
	char *s = extint_list_js();
	iparams_add("EXT_LIST_JS", kstr_sp(s));
	kstr_free(s);
#else
	iparams_add("EXT_LIST_JS", (char *) "");
#endif
}


/*
	Architecture of web server:
		c2s = client-to-server
		s2c = server-to-client
	
	NB: The only "push" s2c data is server websocket output (stream data and messages).
	Other s2c data are responses to c2s requests.
	
	Called by Kiwi server code:
		// server polled check of websocket SET messages
		web_to_app()
			return nbuf_dequeue(c2s nbuf)
		
		// server demand push of websocket stream data
		app_to_web(buf)
			buf => nbuf_allocq(s2c)

		// server demand push of websocket message data (no need to use nbufs)
		send_msg*()
			mg_websocket_write()
	
	Called by (or on behalf of) mongoose web server:
		// event requests _from_ web server:
		// (prompted by data coming into web server)
		mg_create_server()
			ev_handler()
				MG_REQUEST:
					request()
						is_websocket:
							copy mc data to nbufs:
								mc data => nbuf_allocq(c2s) [=> web_to_app()]
						file:
							return file/AJAX data to server:
								mg_send_header
								(file data, AJAX) => mg_send_data()
				MG_CLOSE:
					rx_server_websocket(WS_MODE_CLOSE)
				MG_AUTH:
					MG_TRUE
		
		// polled push of data _to_ web server
		TASK:
		web_server()
			mg_poll_server()	// also forces mongoose internal buffering to write to sockets
			mg_iterate_over_connections()
				iterate_callback()
					is_websocket:
						[app_to_web() =>] nbuf_dequeue(s2c) => mg_websocket_write()
					other:
						ERROR
			LOOP
					
*/


// c2s
// client to server
// 1) websocket: {SET, SND, W/F, EXT, ADM, MFG} messages sent from .js via {msg,snd,wf,ext}_send(), received via websocket connection threads
//		no response returned (unless s2c send_msg*() done)
// 2) HTTP GET: normal browser file downloads, response returned (e.g. .html, .css, .js, images files)
// 3) HTTP GET: AJAX requests, response returned (e.g. "GET /status")
//		eliminating most of these in favor of websocket messages so connection auth can be performed
// 4) HTTP PUT: e.g. kiwi_ajax_send() upload photo file, response returned

int web_to_app(conn_t *c, nbuf_t **nbp)
{
	nbuf_t *nb;
	
    *nbp = NULL;
	if (c->stop_data) return 0;
	nb = nbuf_dequeue(&c->c2s);
	if (!nb) return 0;
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
// 1) websocket: {SND, W/F} data streams received by .js via (ws).onmessage()
// 2) websocket: {MSG, ADM, MFG, EXT, DAT} messages sent by send_msg*(), received via open_websocket() msg_cb/recv_cb routines
// 3) 

void app_to_web(conn_t *c, char *s, int sl)
{
	if (c->stop_data) return;
	if (c->internal_connection) {
	    //printf("app_to_webinternal_connection sl=%d\n", sl);
	    return;
	}
	nbuf_allocq(&c->s2c, s, sl);
	//NextTask("s2c");
}


// event requests _from_ web server:
// (prompted by data coming into web server)
//	1) handle incoming websocket data
//	2) HTML GET ordinary requests, including cache info requests
//	3) HTML GET AJAX requests
//	4) HTML PUT requests

bool web_nocache;
//bool web_nocache = true;
static char *web_server_hdr;

static int request(struct mg_connection *mc, enum mg_event ev) {
	int i, n;
	size_t edata_size = 0;
	const char *edata_data;

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
		
		conn_t *c = rx_server_websocket(WS_MODE_ALLOC, mc);
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
	}
    
    char remote_ip[NET_ADDRSTRLEN];
    check_if_forwarded(NULL, mc, remote_ip);
    bool is_sdr_hu = ip_match(remote_ip, &ddns.ips_sdr_hu);
    //printf("is_sdr_hu=%d %s %s\n", is_sdr_hu, remote_ip, mc->uri);
		
	if (ev == MG_CACHE_RESULT) {
		web_printf("MG_CACHE_RESULT %s:%05d%s cached=%s (etag_match=%d || not_mod_since=%d) mtime=%lu/%lx",
			remote_ip, mc->remote_port, is_sdr_hu? "[sdr.hu]":"",
			mc->cache_info.cached? "YES":"NO", mc->cache_info.etag_match, mc->cache_info.not_mod_since,
			mc->cache_info.st.st_mtime, mc->cache_info.st.st_mtime);

		if (!mc->cache_info.if_mod_since) {
			float diff = ((float) time_diff_s(mc->cache_info.st.st_mtime, mc->cache_info.client_mtime)) / 60.0;
			char suffix = 'm';
			if (diff >= 60.0 || diff <= -60.0) {
				diff /= 60.0;
				suffix = 'h';
				if (diff >= 24.0 || diff <= -24.0) {
					diff /= 24.0;
					suffix = 'd';
				}
			}
			web_printf("[%+.1f%c]", diff, suffix);
		}
		
		web_printf(" client=%lu/%lx\n", mc->cache_info.client_mtime, mc->cache_info.client_mtime);
		assert(!is_sdr_hu);
		return MG_TRUE;
	} else {
		char *o_uri = (char *) mc->uri;      // o_uri = original uri
		char *uri;
		bool free_uri = FALSE, has_prefix = FALSE, is_extension = FALSE;
		u4_t mtime = 0;
		
		//printf("URL <%s> <%s>\n", o_uri, mc->query_string);

		if (strcmp(o_uri, "/") == 0) {
			o_uri = (char *) "index.html";
		} else {
		    if (*o_uri == '/') o_uri++;
		}
		
		// SECURITY: prevent escape out of local directory
		mg_remove_double_dots_and_double_slashes((char *) mc->uri);

		char *suffix = strrchr(o_uri, '.');
		
		if (suffix && (strcmp(suffix, ".json") == 0 || strcmp(suffix, ".json/") == 0)) {
			lprintf("attempt to fetch config file: %s query=<%s> from %s\n", o_uri, mc->query_string, ip_remote(mc));
			return MG_FALSE;
		}
		
		// if uri uses a subdir we know about just use the absolute path
		if (strncmp(o_uri, "kiwi/", 5) == 0) {
			uri = o_uri;
			has_prefix = TRUE;
		} else
		if (strncmp(o_uri, "extensions/", 11) == 0) {
			uri = o_uri;
			has_prefix = TRUE;
			is_extension = TRUE;
		} else
		if (strncmp(o_uri, "pkgs/", 5) == 0) {
			uri = o_uri;
			has_prefix = TRUE;
		} else
		if (strncmp(o_uri, "config/", 7) == 0) {
			asprintf(&uri, "%s/%s", DIR_CFG, &o_uri[7]);
			free_uri = TRUE;
			has_prefix = TRUE;
		} else
		if (strncmp(o_uri, "kiwi.config/", 12) == 0) {
			asprintf(&uri, "%s/%s", DIR_CFG, &o_uri[12]);
			free_uri = TRUE;
			has_prefix = TRUE;
		} else {
			// use name of active ui as subdir
			user_iface_t *ui = find_ui(mc->local_port);
			// should never not find match since we only listen to ports in ui table
			assert(ui);
			asprintf(&uri, "%s/%s", ui->name, o_uri);
			free_uri = TRUE;
		}
		//printf("---- HTTP: uri %s (%s)\n", o_uri, uri);

		// try as file from in-memory embedded data or local filesystem
		edata_data = edata(uri, ev == MG_CACHE_INFO, &edata_size, &mtime);
		
		// try again with ".html" appended
		if (!edata_data) {
			char *uri2;
			asprintf(&uri2, "%s.html", uri);
			if (free_uri) free(uri);
			uri = uri2;
			free_uri = TRUE;
			edata_data = edata(uri, ev == MG_CACHE_INFO, &edata_size, &mtime);
		}
		
		// try looking in "kiwi" subdir as a default
		if (!edata_data && !has_prefix) {
			if (free_uri) free(uri);
			asprintf(&uri, "kiwi/%s", o_uri);
			free_uri = TRUE;

			// try as file from in-memory embedded data or local filesystem
			edata_data = edata(uri, ev == MG_CACHE_INFO, &edata_size, &mtime);
			
			// try again with ".html" appended
			if (!edata_data) {
				if (free_uri) free(uri);
				asprintf(&uri, "kiwi/%s.html", o_uri);
				free_uri = TRUE;
				edata_data = edata(uri, ev == MG_CACHE_INFO, &edata_size, &mtime);
			}
		}

		suffix = strrchr(uri, '.');
		if (edata_data && ev != MG_CACHE_INFO && suffix && strcmp(suffix, ".html") == 0 && mc->query_string) {
		    #define NQS 32
            char *r_buf, *qs[NQS+1];
            n = kiwi_split((char *) mc->query_string, &r_buf, "&", qs, NQS);
            for (i=0; i < n; i++) {
		        if (strcmp(qs[i], "nocache") == 0) {
		            web_nocache = true;
		            printf("### nocache\n");
		        } else
		        if (strcmp(qs[i], "ctrace") == 0) {
		            web_caching_debug = true;
		            printf("### ctrace\n");
		        } else {
		            char *su_m = NULL;
                    if (sscanf(qs[i], "su=%256ms", &su_m) == 1) {
                        if (kiwi_sha256_strcmp(su_m, "7cdd62b9f85bb7a8f9d85595c4e488d8090c435cf71f8dd41ff7177ea6735189") == 0) {
                            auth_su = true;     // a little dodgy that we have to use a global -- be sure to reset asap
                            kiwi_strncpy(auth_su_remote_ip, remote_ip, NET_ADDRSTRLEN);
                        }
            
                        // erase cleartext as much as possible
                        bzero(su_m, strlen(su_m));
                        int slen = strlen(mc->query_string);
                        bzero(r_buf, slen);
                        bzero((char *) mc->query_string, slen);
                        free(su_m);
                        break;      // have to stop because query string is now erased
                    }
                    free(su_m);
		        }
            }
            free(r_buf);
		}

		// For extensions, try looking in external extension directory (outside this package).
		// SECURITY: But ONLY for extensions! Don't allow any other root-referenced accesses.
		// "o_uri" has been previously protected against "../" directory escape.
		if (!edata_data && is_extension) {
			if (free_uri) free(uri);
			asprintf(&uri, "/root/%s", o_uri);
			free_uri = TRUE;

			// try as file from in-memory embedded data or local filesystem
			edata_data = edata(uri, ev == MG_CACHE_INFO, &edata_size, &mtime);
			
			// try again with ".html" appended
			if (!edata_data) {
				if (free_uri) free(uri);
				asprintf(&uri, "/root/%s.html", o_uri);
				free_uri = TRUE;
				edata_data = edata(uri, ev == MG_CACHE_INFO, &edata_size, &mtime);
			}
		}

		// try as AJAX request
		char *ajax_data;
		bool isAJAX = false, free_ajax_data = false;
		if (!edata_data) {
		
			// don't try AJAX during the MG_CACHE_INFO pass
			if (ev == MG_CACHE_INFO) {
				if (free_uri) free(uri);
				return MG_FALSE;
			}
			//printf("rx_server_ajax: %s\n", mc->uri);
			ajax_data = rx_server_ajax(mc);     // mc->uri is o_uri without ui->name prefix
			if (ajax_data) {
			    if (strcmp(ajax_data, "NO-REPLY") == 0) {
                    if (free_uri) free(uri);
                    return MG_FALSE;
			    }
			    
			    edata_data = ajax_data;
				edata_size = kstr_len((char *) ajax_data);
				isAJAX = true;
				free_ajax_data = true;
			}
		}

		// give up
		if (!edata_data) {
			//printf("unknown URL: %s (%s) query=<%s> from %s\n", o_uri, uri, mc->query_string, ip_remote(mc));
			if (free_uri) free(uri);
			return MG_FALSE;
		}
		
		// for *.html and *.css process %[substitution]
		// fixme: don't just panic because the config params are bad
		char *html_data;
		bool free_html_data = false;
		suffix = strrchr(uri, '.');
		
		bool dirty = false;		// must never return a 304 if a %[] substitution was made!
		if (!isAJAX && suffix && (strcmp(suffix, ".html") == 0 || strcmp(suffix, ".css") == 0)) {
			int nsize = edata_size;
			html_data = (char *) kiwi_malloc("html_data", nsize);
			free_html_data = true;
			char *cp = (char *) edata_data, *np = html_data, *pp;
			int cl, sl, pl, nl;

			//printf("checking for \"%%[\": %s %d\n", uri, nsize);

			for (cl=nl=0; cl < edata_size;) {
				if (*cp == '%' && *(cp+1) == '[') {
					cp += 2; cl += 2; pp = cp; pl = 0;
					while (*cp != ']' && cl < edata_size) { cp++; cl++; pl++; }
					cp++; cl++;
					
					for (i=0; i < n_iparams; i++) {
						iparams_t *ip = &iparams[i];
						if (strncmp(pp, ip->id, pl) == 0) {
							sl = strlen(ip->decoded);

							// expand buffer
							html_data = (char *) kiwi_realloc("html_data", html_data, nsize+sl);
							np = html_data + nl;		// in case buffer moved
							nsize += sl;
							//printf("%d %%[%s] %d <%s> %s\n", nsize, ip->id, sl, ip->decoded, uri);
							strcpy(np, ip->decoded); np += sl; nl += sl;
							dirty = true;
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
			
			assert((np - html_data) == nl);
			edata_data = html_data;
			edata_size = nl;
		}
		
		// Add version checking to each .js file served.
		// Add to end of file so line numbers printed in javascript errors are not effected.
		char *ver = NULL;
		int ver_size = 0;
		bool isJS = (suffix && strcmp(suffix, ".js") == 0);
		if (!isAJAX && isJS) {
			asprintf(&ver, "kiwi_check_js_version.push({ VERSION_MAJ:%d, VERSION_MIN:%d, file:'%s' });\n", version_maj, version_min, uri);
			ver_size = strlen(ver);
		}

		// Tell web server the file size and modify time so it can make a decision about caching.
		// Modify time _was_ conservative: server start time as .js files have version info appended.
		// Modify time is now:
		//		server running in background (production) mode: build time of server binary.
		//		server running in foreground (development) mode: stat is fetched from filesystems, else build time of server binary.
		
		// NB: Will see cases of etag_match=N but not_mod_since=Y because of %[] substitution.
		// The size in the etag is different due to the substitution, but the underlying file mtime hasn't changed.
		
		// FIXME: Is what we do here re caching really correct? Do we need to be returning "Cache-Control: must-revalidate"?
		
		// FIXME? no caching on mobile devices because of possible problems (e.g. audio start icon broken on iPhone Safari)
		bool mobile_device = false;
        if (!isAJAX) {
            const char *ua = mg_get_header(mc, "User-Agent");
            if (ua != NULL && (strstr(ua, "iPad") != NULL || strstr(ua, "iPhone") != NULL || strstr(ua, "Android") != NULL))
                mobile_device = true;
            //if (mobile_device) real_printf("mobile_device User-Agent: %s | %s\n", ua, mc->uri);
        }

  		mc->cache_info.st.st_size = edata_size + ver_size;
  		if (!isAJAX) assert(mtime != 0);
  		mc->cache_info.st.st_mtime = mtime;

		if (!(isAJAX && ev == MG_CACHE_INFO)) {		// don't print for isAJAX + MG_CACHE_INFO nop case
			web_printf("%-15s %s:%05d%s size=%6d dirty=%d mtime=%lu/%lx %s %s %s%s\n", (ev == MG_CACHE_INFO)? "MG_CACHE_INFO" : "MG_REQUEST",
				remote_ip, mc->remote_port, is_sdr_hu? "[sdr.hu]":"",
				mc->cache_info.st.st_size, dirty, mtime, mtime, isAJAX? mc->uri : uri, mg_get_mime_type(isAJAX? mc->uri : uri, "text/plain"),
				(mc->query_string != NULL)? "qs:" : "", (mc->query_string != NULL)? mc->query_string : "");
		}

		int rtn = MG_TRUE;
		if (ev == MG_CACHE_INFO) {
			if (dirty || isAJAX || is_sdr_hu || web_nocache || mobile_device) {   // FIXME: it's really wrong that nocache is not applied per-connection
			    web_printf("%-15s NO CACHE %s%s\n", "MG_CACHE_INFO",
			        mobile_device? "mobile_device " : (is_sdr_hu? "sdr.hu " : ""), uri);
				rtn = MG_FALSE;		// returning false here will prevent any 304 decision based on the mtime set above
			}
		} else {
		    const char *hdr_type;
		    bool isPNG = (suffix && strcmp(suffix, ".png") == 0);
		
			// NB: prevent AJAX responses from getting cached by not sending standard headers which include etag etc!
			if (isAJAX) {
				//printf("AJAX: %s %s\n", mc->uri, uri);
				mg_send_header(mc, "Content-Type", "text/plain");
				
				// needed by, e.g., auto-discovery port scanner
				// SECURITY FIXME: can we detect a special request header in the pre-flight and return this selectively?
				
				// An <iframe sandbox="allow-same-origin"> is not sufficient for subsequent
				// non-same-origin XHRs because the
				// "Access-Control-Allow-Origin: *" must be specified in the pre-flight.
				mg_send_header(mc, "Access-Control-Allow-Origin", "*");
			    hdr_type = "AJAX";
			} else
			if (web_nocache || is_sdr_hu || (mobile_device && !isPNG)) {    // sdr.hu doesn't like our new caching headers for the avatar
			    mg_send_header(mc, "Content-Type", mg_get_mime_type(uri, "text/plain"));
			    hdr_type = "non-caching";
			} else {
				mg_send_standard_headers(mc, uri, &mc->cache_info.st, "OK", (char *) "", true);
				// cache PNGs to keep GPS az/el img from flashing on periodic re-render with Safari
				mg_send_header(mc, "Cache-Control", isPNG? "max-age=3600" : "max-age=0");
			    hdr_type = "caching";
			}
			web_printf("%-15s %s headers, is_sdr_hu=%d\n", "sending", hdr_type, is_sdr_hu);
			
			//if (is_sdr_hu) mg_send_header(mc, "Content-Length", stprintf("%d", edata_size));
			
			if (!is_sdr_hu)
			    mg_send_header(mc, "Server", web_server_hdr);
			
			mg_send_data(mc, kstr_sp((char *) edata_data), edata_size);

			if (ver != NULL) {
				mg_send_data(mc, ver, ver_size);
			}
		}
		
		if (ver != NULL) free(ver);
		if (free_html_data) kiwi_free("html_data", (void *) html_data);
		if (free_ajax_data) kstr_free((char *) ajax_data);
		if (free_uri) free(uri);
		
		if (ev != MG_CACHE_INFO) http_bytes += edata_size;
		return rtn;
	}
}

// event requests _from_ web server:
// (prompted by data coming into web server)
static int ev_handler(struct mg_connection *mc, enum mg_event ev) {
  
	//printf("ev_handler %d:%d len %d\n", mc->local_port, mc->remote_port, (int) mc->content_len);
	//printf("MG_REQUEST: URI:%s query:%s\n", mc->uri, mc->query_string);
	if (ev == MG_REQUEST || ev == MG_CACHE_INFO || ev == MG_CACHE_RESULT) {
		int r = request(mc, ev);
		return r;
	} else
	if (ev == MG_CLOSE) {
		//printf("MG_CLOSE\n");
		rx_server_websocket(WS_MODE_CLOSE, mc);
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

// polled send of data _to_ web server
static int iterate_callback(struct mg_connection *mc, enum mg_event ev)
{
	int ret;
	nbuf_t *nb;
	
	if (ev == MG_POLL && mc->is_websocket) {
		conn_t *c = rx_server_websocket(WS_MODE_LOOKUP, mc);
		if (c == NULL)  return MG_FALSE;

		while (TRUE) {
			if (c->stop_data) break;
			nb = nbuf_dequeue(&c->s2c);
			//printf("s2c CHK port %d nb %p\n", mc->remote_port, nb);
			
			if (nb) {
				assert(!nb->done && nb->buf && nb->len);

				#ifdef SND_TIMING_CK
				// check timing of audio output (assumes non-IQ mode always selected)
				snd_pkt_real_t *out = (snd_pkt_real_t *) nb->buf;
				if (c->type == STREAM_SOUND && strncmp(out->h.id, "SND", 3) == 0) {
					u4_t now = timer_ms();
					if (!c->audio_check) {
						c->audio_epoch = now;
						c->audio_sequence = c->audio_pkts_sent = c->audio_last_time = c->sum2 = 0;
						c->audio_check = true;
					}
					double audio_rate = ext_update_get_sample_rateHz(c->rx_channel);
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
							adc_clock_system()/1e6, audio_rate);
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
		TaskSleepUsec(WEB_SERVER_POLL_US);
		
		//#define CHECK_ECPU_STACK
		#ifdef CHECK_ECPU_STACK
            static int every_1sec;
            every_1sec += WEB_SERVER_POLL_US;
            if (every_1sec >= 1000000) {
                static SPI_MISO sprp;
                spi_get_noduplex(CmdGetSPRP, &sprp, 4);
                printf("e_cpu: SP=%04x RP=%04x\n", sprp.word[0], sprp.word[1]);
                every_1sec = 0;
            }
		#endif
	}
}

void web_server_init(ws_init_t type)
{
	int i;
	user_iface_t *ui = user_iface;
	static bool init;
	
	if (!init) {
		nbuf_init();

		// add the new "port_ext" config param if needed
		// done here because web_server_init(WS_INIT_CREATE) called earlier than rx_server_init() in main.c
		int port = admcfg_int("port", NULL, CFG_REQUIRED);
		bool error;
		admcfg_int("port_ext", &error, CFG_OPTIONAL);
		if (error) {
			admcfg_set_int("port_ext", port);
			admcfg_save_json(cfg_adm.json);
		}
		
#ifdef EDATA_DEVEL
		struct stat st;
		scall("stat edata_always", stat("./obj_keep/edata_always.o", &st));
		mtime_obj_keep_edata_always_o = st.st_mtime;
#endif

        asprintf(&web_server_hdr, "KiwiSDR_Mongoose/%d.%d", version_maj, version_min);
		init = TRUE;
	}
	
	if (type == WS_INIT_CREATE) {
		// if specified, override the default port number
		if (alt_port) {
			ddns.port = ddns.port_ext = alt_port;
		} else {
			ddns.port = admcfg_int("port", NULL, CFG_REQUIRED);
			ddns.port_ext = admcfg_int("port_ext", NULL, CFG_REQUIRED);
		}
		lprintf("listening on %s port %d/%d for \"%s\"\n", alt_port? "alt":"default",
			ddns.port, ddns.port_ext, ui->name);
		ui->port = ddns.port;
		ui->port_ext = ddns.port_ext;
	} else

	if (type == WS_INIT_START) {
		reload_index_params();
		services_start(SVCS_RESTART_FALSE);
	}

	// create webserver port(s)
	for (i = 0; ui->port; i++) {
	
		if (type == WS_INIT_CREATE) {
			// FIXME: stopgap until admin page supports config of multiple UIs
			if (i != 0) {
				ui->port = ddns.port + i;
				ui->port_ext = ddns.port_ext + i;
			}
			
			ui->server = mg_create_server(NULL, ev_handler);
			//mg_set_option(ui->server, "document_root", "./");		// if serving from file system
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

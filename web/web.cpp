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

// Copyright (c) 2014-2019 John Seamons, ZL/KF6VO

#include "kiwi.h"
#include "types.h"
#include "config.h"
#include "mem.h"
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
#include "ext_int.h"
#include "debug.h"

#include <string.h>
#include <ctype.h>
#include <time.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>

// This file is compiled twice into two different object files:
// Once with EDATA_EMBED defined when installed as the production server in /usr/local/bin
// Once with EDATA_DEVEL defined when compiled into the build directory during development 

#define MTR 0

static int bsearch_edatacomp(const void *key, const void *elem)
{
	embedded_files_t *ef_elem = (embedded_files_t *) elem;
    return strcmp((char *) key, ef_elem->name);
}

static const char *edata_lookup(embedded_files_t files[], const char *name, size_t *size)
{
    embedded_files_t *p;
    
    #define EDATA_BSEARCH
    #ifdef EDATA_BSEARCH
        // works because files[] is presorted by mkdata.pl script
        p = files;
        if (p->count == 0) {    // determine count needed by bsearch()
            for (; p->name != NULL; p++) files[0].count++;
            p = files;
        }
        p = (embedded_files_t *) bsearch(name, p, p->count, sizeof(embedded_files_t), bsearch_edatacomp);
        if (p != NULL) {
            if (size != NULL) *size = p->size;
            return (const char *) p->data;
        }
    #else
        for (p = files; p->name != NULL; p++) {
            if (strcmp(p->name, name) == 0) {
                if (size != NULL) *size = p->size;
                return (const char *) p->data;
            }
        }
    #endif

    return NULL;
}

time_t mtime_obj_keep_edata_always_o;
time_t mtime_obj_keep_edata_always2_o;

int web_caching_debug;

static const char* edata(const char *uri, bool cache_check, size_t *size, time_t *mtime, bool *is_file)
{
	const char* data = NULL;
	bool absPath = (uri[0] == '/');
	const char *type, *subtype, *reason;
	
    type = cache_check? "cache check" : "fetch file";
    *is_file = false;

#ifdef EDATA_EMBED
	// The normal background daemon loads files from in-memory embedded data for speed.
	// In development mode these files are always loaded from the local filesystem.
	data = edata_lookup(edata_embed, uri, size);
	if (data) {
		// In production mode the only thing we have is the server binary build time.
		// But this is okay since because that's the origin of the data and the binary is
		// only updated when a software update occurs.
		*mtime = timer_server_build_unix_time();
		subtype = "edata_embed file";
		reason = "using server build";
	}
#endif

	// some large, seldom-changed files are always loaded from memory, even in development mode
	if (!data) {
		data = edata_lookup(edata_always, uri, size);
		if (data) {
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

	if (!data) {
		data = edata_lookup(edata_always2, uri, size);
		if (data) {
		    subtype = "edata_always2 file";
#ifdef EDATA_EMBED
			// In production mode the only thing we have is the server binary build time.
			// But this is okay since because that's the origin of the data and the binary is
			// only updated when a software update occurs.
			*mtime = timer_server_build_unix_time();
			reason = "using server build";
#else
			// In development mode this is better than the constantly-changing server binary
			// (i.e. the obj_keep/edata_always2.o file is rarely updated).
			// NB: mtime_obj_keep_edata_always2_o is only updated once per server restart.
			*mtime = mtime_obj_keep_edata_always2_o;
			reason = "using edata_always2.o";
#endif
		}
	}

    if (data)
	    web_printf_all("%-16s %s, %s, %s: mtime=[%s] %s\n", "EDATA", type, subtype, reason, var_ctime_static(mtime), uri);

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
        if (MTR) real_printf("-> edata() uri2 ASPR %p %d<%s>\n", uri2, strlen(uri2), uri2);
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
		static time_t last_mtime;
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
            memset(&st, 0, sizeof(st));
            evWS(EC_EVENT, EV_WS, 0, "WEB_SERVER", "stat..");
            int rv = stat(uri2, &st);
            evWS(EC_EVENT, EV_WS, 0, "WEB_SERVER", evprintf("stat size %d", st.st_size));
            if (rv < 0) {
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
                    kiwi_ifree((void *) last_free, "edata_file");
                }
                
                evWS(EC_EVENT, EV_WS, 0, "WEB_SERVER", "malloc..");
                last_free = last_data = data = (char *) kiwi_imalloc("edata_file", *size);
                evWS(EC_EVENT, EV_WS, 0, "WEB_SERVER", "read..");
                ssize_t rsize = read(fd, (void *) data, *size);
                close(fd);
                evWS(EC_EVENT, EV_WS, 0, "WEB_SERVER", evprintf("read %d", rsize));
                if (rsize != *size) {
                    fail = true;
                } else {
                    //printf(">READ %p %d %s\n", last_data, last_size, uri2);
                    if (last_uri2_read != NULL) kiwi_ifree(last_uri2_read);
                    last_uri2_read = strdup(uri2);
                }
            }
            
            if (fail) {
                if (last_uri2_read != NULL) kiwi_ifree(last_uri2_read);
                last_uri2_read = NULL;
                nofile = true;
            }
		} else {
		    if (last_data != NULL) {
                //printf(">USE-ONCE %p %d %s\n", last_data, last_size, uri2);
                data = last_data;
                *size = last_size;
    
                // only use once
                if (last_uri2_read != NULL) kiwi_ifree(last_uri2_read);
                last_uri2_read = NULL;
                last_data = NULL;
                // but note last_free remains valid until next cache_check
            }
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
        web_printf_all("%-16s %s, %s: mtime=[%s] %s %s\n", "EDATA", type, reason, var_ctime_static(mtime), uri, uri2);
	    *is_file = nofile? false:true;
	}

	if (free_uri2) {
        if (MTR) real_printf("-> edata() uri2 FREE %p %d<%s>\n", uri2, strlen(uri2), uri2);
	    kiwi_ifree(uri2);
	}
	return data;
}

static char *cached_o_uri;
static const char *cached_edata_data;
static size_t cached_size;
static time_t cached_mtime;
static bool cached_is_min;
static bool cached_is_gzip;
static bool cached_is_file;

static const char* edata_with_file_ext(char **o_uri, bool free_o_uri, bool *free_uri, const char *prefix_list[],
    bool cache_check, size_t *size, time_t *mtime, bool *is_min, bool *is_gzip, bool *is_file)
{
	const char *edata_data = NULL;
	// the ".html" is here so a URL of "mykiwi:8073/admin" will match admin.html file
	const char *zip_suffix[] = { ".gz", "", ".html.gz", ".html" };
	const char *no_zip_suffix[] = { "", ".html" };
    char *uri, *uri2;
	const char *no_prefix[] = { "", NULL };
	if (prefix_list == NULL) prefix_list = no_prefix;
	
	web_printf_all("%-16s basename: %s prefixes: ", cache_check? "CACHE_CHECK":"REQUEST", *o_uri);
	if (web_caching_debug & WEB_CACHING_DEBUG_ALL) {
        for (char **pre = (char **) prefix_list; *pre != NULL; pre++) {
            web_printf_all("%s ", (**pre == '\0')? "[./]" : *pre);
        }
    }
	
	// a CACHE_CHECK is very likely to be followed by a REQUEST for the same file, so cache that case
	if (cached_edata_data != NULL && strcmp(*o_uri, cached_o_uri) == 0) {
	    web_printf_all(" ### REQUEST_CACHE_HIT ### %p\n", cached_edata_data);
	    *size = cached_size;
	    *mtime = cached_mtime;
	    *is_min = cached_is_min;
	    *is_gzip = cached_is_gzip;
	    *is_file = cached_is_file;
	    *free_uri = FALSE;
	    return cached_edata_data;
	}
	
	web_printf_all("\n");
	*free_uri = FALSE;
	
	char *sp;
	const char **suffix;
	int suffix_len;
	
    sp = strrchr(*o_uri, '.');
    //lprintf("sp=%p %s %s\n", sp, sp, *o_uri);
    // sp == NULL case needed to catch "mykiwi:8073/admin" case mentioned above (e.g. admin.min.html)
	int check_for_minified = (sp == NULL || !strcmp(sp, ".js") || !strcmp(sp, ".css") || !strcmp(sp, ".html"))? 1:0;
	if (sp == NULL) sp = (char *) "";
    //lprintf("check_for_minified=%d sp=%p %s %s\n", check_for_minified, sp, sp, *o_uri);

    bool check_min_first = true;    // when check_min_first == true minimization check will be overridden by check_for_minified == 0
    
#ifdef EDATA_DEVEL
	if (!use_foptim) {
	    // In EDATA_DEVEL mode use non-optimized file versions since re-optimization doesn't normally occur after each file edit.
	    // "-fopt" run arg can be used to force optimized files to be used as with EDATA_EMBED mode.
        suffix = no_zip_suffix;
        suffix_len = ARRAY_LEN(no_zip_suffix);
        check_min_first = false;
        // NB: checks regular file first but then always goes on to check for minimized version
	} else
#endif
    {
        suffix = zip_suffix;
        suffix_len = ARRAY_LEN(zip_suffix);
    }
	
	for (int cm = (check_min_first? check_for_minified:0); cm >= 0 && cm <= 1; cm = cm + (check_min_first? -1:1)) {

        for (char **pre = (char **) prefix_list; *pre != NULL && !edata_data; pre++) {
            for (int i = 0; i < suffix_len && !edata_data; i++) {
                if (cm) {
                    asprintf(&uri2, "%s%.*s.min%s%s", *pre, (int) (strlen(*o_uri) - strlen(sp)), *o_uri, sp, suffix[i]);
                    if (MTR) real_printf("-> edata_with_file_ext() uri2-1 ASPR %p %d<%s>\n", uri2, strlen(uri2), uri2);
                } else {
                    asprintf(&uri2, "%s%s%s", *pre, *o_uri, suffix[i]);
                    if (MTR) real_printf("-> edata_with_file_ext() uri2-2 ASPR %p %d<%s>\n", uri2, strlen(uri2), uri2);
                }
                if (*free_uri) {
                    if (MTR) real_printf("-> edata_with_file_ext() uri FREE %p %d<%s>\n", uri, strlen(uri), uri);
                    kiwi_ifree(uri);
                }
                uri = uri2;
                *free_uri = TRUE;
                edata_data = edata(uri, cache_check, size, mtime, is_file);
                if (edata_data) {
                    *is_min = (cm? true:false);
                    *is_gzip = (kiwi_str_ends_with(uri, ".gz") != NULL);
                }
                web_printf_all("%-16s cm%d %s %p\n", "TRY", cm, uri, edata_data);
                // NB: "&& !edata_data" in both for loops will cause double break if edata_data becomes non-NULL
            }
        }
    }
    
    web_printf_all("%-16s %p %s %s %s%s\n", edata_data? "RTN-FOUND" : "RTN-NIL",
        edata_data, uri, cache_check? "CACHE_CHECK":"REQUEST", *is_min? "MIN ":"", *is_gzip? "GZIP":"");
    if (*is_gzip) uri[strlen(uri) - 3] = '\0';      // remove ".gz" suffix

    if (cache_check) {
        if (MTR && cached_o_uri) real_printf("-> edata_with_file_ext() cached_o_uri FREE %p %d<%s>\n", cached_o_uri, strlen(cached_o_uri), cached_o_uri);
        kiwi_ifree(cached_o_uri);
        cached_o_uri = strdup(*o_uri);
        if (MTR) real_printf("-> edata_with_file_ext() cached_o_uri STRDUP %p %d<%s>\n", cached_o_uri, strlen(cached_o_uri), cached_o_uri);
        cached_edata_data = edata_data;
        cached_size = *size;
        cached_mtime = *mtime;
        cached_is_min = *is_min;
        cached_is_gzip = *is_gzip;
        cached_is_file = *is_file;
    } else {
        cached_edata_data = NULL;
    }

    if (free_o_uri) {
        if (MTR) real_printf("-> edata_with_file_ext() o_uri FREE %p %d<%s>\n", *o_uri, strlen(*o_uri), *o_uri);
        kiwi_ifree(*o_uri);
    }
    *o_uri = uri;   // final uri considered with corresponding *free_uri = TRUE
    return edata_data;
}


typedef struct {
	char *id, *encoded, *decoded;
} iparams_t;

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
    ip->decoded = (char *) kiwi_imalloc("iparams_add", n + SPACE_FOR_NULL);
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
		id_last = (char *) kiwi_imalloc("index_params_cb", n + SPACE_FOR_NULL);
		mg_url_decode(s, n, id_last, n + SPACE_FOR_NULL, 0);
		//printf("index_params_cb: %d %d/%d/%d/%d ID %d <%s>\n", n_iparams, seq, hit, lvl, rem, n, id_last);
	} else {
		char *encoded = (char *) kiwi_imalloc("index_params_cb", n + SPACE_FOR_NULL);
		kiwi_strncpy(encoded, s, n + SPACE_FOR_NULL);
		//printf("index_params_cb: %d %d/%d/%d/%d VAL %s: <%s>\n", n_iparams, seq, hit, lvl, rem, id_last, encoded);
		if (strcmp(id_last, "PAGE_TITLE") == 0 && *encoded == '\0') {
		    iparams_add(id_last, (char *) "KiwiSDR");
		} else {
		    iparams_add(id_last, encoded);
		}
		kiwi_ifree(id_last);
		kiwi_ifree(encoded);
	}
	
	return false;
}

void reload_index_params()
{
	int i;
	char *sb, *sb2;
	
	// don't free previous on reload because not all were kiwi_imalloc()'d
	// (the memory loss is very small)
	n_iparams = 0;
	//cfg_walk("index_html_params", cfg_print_tok, NULL);
	cfg_walk("index_html_params", index_params_cb, NULL);

	sb = (char *) cfg_string("owner_info", NULL, CFG_REQUIRED);
	iparams_add("OWNER_INFO", sb);
	cfg_string_free(sb);
	
	
	// To aid development, only use packages when running in embedded mode (i.e. background mode),
	// or in development mode when "-fopt" argument is used.
	// Otherwise javascript errors would give line numbers in the optimized files which are
	// useless for debugging.
	// Currently this only applies to the kiwisdr.min.{js,css} package.
	int embed = bg? 1 : (use_foptim? 1:0);

	const char *gen_list_css[2][7] = {
	    {
		    "pkgs/font-awesome-4.6.3/css/font-awesome.min.css",
		    "pkgs/text-security/text-security-disc.css",
		    "pkgs/w3.css",
		    "kiwi/w3_ext.css",
		    "openwebrx/openwebrx.css",
		    "kiwi/kiwi.css",
		    NULL
		}, {
		    "kiwisdr.min.css",
		    NULL
		}
	};

	sb = NULL;
	for (i=0; gen_list_css[embed][i] != NULL; i++) {
		sb = kstr_asprintf(sb, "<link rel=\"stylesheet\" type=\"text/css\" href=\"%s\" />\n", gen_list_css[embed][i]);
	}
	iparams_add("GEN_LIST_CSS", kstr_sp(sb));
	kstr_free(sb);
	
	const char *gen_list_js[2][13] = {
	    {
		    "kiwi/kiwi_util.js",
		    "kiwi/kiwi.js",
		    "kiwi/kiwi_ui.js",
		    "kiwi/w3_util.js",
		    "pkgs/w3color.js",
		    "kiwi/monitor.js",
		    "openwebrx.js",
		    "ima_adpcm.js",
		    "audio.js",
		    "pkgs/xdLocalStorage/xd-utils.js",
		    "pkgs/xdLocalStorage/xdLocalStorage.js",
		    "extensions/ext.js",
		    NULL
		}, {
		    "kiwisdr.min.js",
		    NULL
		}
	};

	sb = NULL;
	for (i=0; gen_list_js[embed][i] != NULL; i++) {
		sb = kstr_asprintf(sb, "<script src=\"%s\"></script>\n", gen_list_js[embed][i]);
	}
	iparams_add("GEN_LIST_JS", kstr_sp(sb));
	kstr_free(sb);

	// add the list of extensions (only used by admin.html)
#ifdef USE_SDR
	sb = extint_list_js();
	iparams_add("EXT_LIST_JS", kstr_sp(sb));
	//real_printf("%s\n", kstr_sp(sb));
	kstr_free(sb);
#else
	iparams_add("EXT_LIST_JS", (char *) "");
#endif
}

static char cached_ip[NET_ADDRSTRLEN];
static int cached_served;

void web_has_served(const char *from, char *ip_s, const char *fn_s)
{
	conn_t *c;
	int ch;
    rx_chan_t *rx;
    bool found = false;

    for (rx = rx_channels, ch = 0; rx < &rx_channels[rx_chans]; rx++, ch++) {
        if (!rx->busy) continue;
		c = rx->conn;
		if (c == NULL || !c->valid) continue;
		if (c->isLocal || c->internal_connection || c->ext_api_determined ||
            (c->type != STREAM_SOUND && c->type != STREAM_WATERFALL)) continue;
		if (strcmp(ip_s, c->remote_ip) == 0) {
		    c->served++;
		    //printf("SERVED: %6s RX%d #%d %s %s\n", from, ch, c->served, ip_s, fn_s);
		    found = true;
		}
	}
	
	if (!found) {
	    if (strcmp(cached_ip, ip_s) == 0) {
	        cached_served++;
            //printf("SERVED: %6s LAST cached #%d %s %s\n", from, cached_served, ip_s, fn_s);
	    } else {
	        kiwi_strncpy(cached_ip, ip_s, NET_ADDRSTRLEN);
	        cached_served = 1;
            //printf("SERVED: %6s NEW cached #1 %s %s\n", from, ip_s, fn_s);
        }
    }
}

// Race between when the master web socket is established so conn_t.served can be incremented
// and when web file requests are actually served. So use a cache to hold early-served requests.
int web_served(conn_t *c)
{
    int served;

    if (strcmp(c->remote_ip, cached_ip) == 0) {
        served = c->served + cached_served;
        //printf("SERVED: RESULT served(%d) + cached(%d) = %d %s\n", c->served, cached_served, served, c->remote_ip);
    } else {
        served = c->served;
        //printf("SERVED: RESULT served(%d) + NO_CACHE_MATCH = %d %s (cached=%s)\n", c->served, served, c->remote_ip, cached_ip);
    }
    
    return served;
}

void web_served_clear_cache(conn_t *c)
{
    if (strcmp(c->remote_ip, cached_ip) == 0) {
        //printf("SERVED: RESULT CLEAR_CACHE %s\n", c->remote_ip);
        cached_ip[0] = '\0';
        cached_served = 0;
    }
}


// event requests _from_ web server:
// (prompted by data coming into web server)
//	1) handle incoming websocket data
//	2) HTML GET ordinary requests, including cache info requests
//	3) HTML GET AJAX requests
//	4) HTML PUT requests

bool webserver_caching, web_nocache;

int web_request(struct mg_connection *mc, enum mg_event evt) {
	int i, n;
	size_t edata_size = 0;
	const char *edata_data;
	bool isPort80 = (net.port != 80 && mc->local_port == 80);
	char *ip_unforwarded = ip_remote(mc);

    //if (web_caching_debug == 0) web_caching_debug = bg? 3:1;

    // access to the local HTTP port should only come from local ip addresses
    if (mc->local_port == net.port_http_local && !isLocal_ip(ip_unforwarded)) {
        if (evt == MG_REQUEST) {
            printf("webserver: HTTP but not local ip evt=%d %s:%d <%s>\n", evt, ip_unforwarded, mc->local_port, mc->uri);
            mg_send_status(mc, 403);    // 403 = "forbidden"
            mg_send_data(mc, NULL, 0);
            return MG_TRUE;
        } else
            return MG_FALSE;
    }


    ////////////////////////////////
    // web socket
    ////////////////////////////////

	if (mc->is_websocket) {
		// This handler is called for each incoming websocket frame, one or more
		// times for connection lifetime.
		
		if (isPort80) return MG_FALSE;

        if ((mc->wsbits & 0x0F) == WEBSOCKET_OPCODE_CONNECTION_CLOSE) { // close request from client
            // respond with a close request to the client
            // after this close request, the connection is scheduled to be closed
            mg_websocket_write(mc, WEBSOCKET_OPCODE_CONNECTION_CLOSE, mc->content, mc->content_len);
            return MG_TRUE;
        }

		char *s = mc->content;
		int sl = mc->content_len;
		//printf("WEBSOCKET: len %d uri <%s>\n", sl, mc->uri);
		if (sl == 0) {
			//printf("----KA %d\n", mc->remote_port);
			return MG_TRUE;	// keepalive?
		}
		
		conn_t *c = rx_server_websocket(WS_MODE_ALLOC, mc);
		if (c == NULL) {
			s[sl] = 0;
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
    
    
    ////////////////////////////////
    // not web socket
    ////////////////////////////////

	// iptables will stop regular connection attempts from a blacklisted ip.
	// But when proxied we need to check the forwarded ip address.
	// Note that this code always sets remote_ip[] as a side-effect for later use (the real client ip).
	char ip_forwarded[NET_ADDRSTRLEN];
    check_if_forwarded("WEB", mc, ip_forwarded);
    if (check_ip_blacklist(ip_forwarded) || check_ip_blacklist(ip_unforwarded)) {
		//printf("WEB: IP BLACKLISTED: %s/%s url=<%s> qs=<%s>\n", ip_unforwarded, ip_forwarded, mc->uri, mc->query_string);
        mg_send_status(mc, 403);    // 403 = "forbidden"
        mg_send_data(mc, NULL, 0);
        return MG_TRUE;
    }
    
    //#define WEB_PRINTF_ON_URL
    #ifdef WEB_PRINTF_ON_URL
        web_caching_debug |= (strstr(mc->uri, "extensions") != NULL)? 3:0;
    #endif
		
    //#define WEB_PRINTF_ON_FILE
    #ifdef WEB_PRINTF_ON_FILE
        #if 0
            web_caching_debug |= (
                strcmp(mc->uri, "mfg") == 0 || strcmp(mc->uri, "/mfg") == 0 ||
                strcmp(mc->uri, "admin") == 0 || strcmp(mc->uri, "/admin") == 0)? 3:0;
        #else
            web_caching_debug = (strstr(mc->uri, "DRM.js") != NULL)? 7:0;
        #endif
    #endif
		
	if (evt == MG_CACHE_RESULT) {
	    web_has_served("CACHED", ip_forwarded, mc->uri);
	    if (web_caching_debug == 0) return MG_TRUE;
	    
	    if (mc->cache_info.cached)
            web_printf_cached("%-16s %6s %11s %4s %3s %4s %5s %s\n", "webserver", "-", mc->cache_info.cached? "304-CACHED":"NO_CACHE", "", "", "", "", mc->uri);

		web_printf_all("%-16s %s:%05d %s (etag_match=%c not_mod_since=%c) mtime=[%s]", "MG_CACHE_RESULT",
			ip_forwarded, mc->remote_port,
			mc->cache_info.cached? "### CLIENT_CACHED ###":"NOT_CACHED", mc->cache_info.etag_match? 'T':'F', mc->cache_info.not_mod_since? 'T':'F',
			var_ctime_static(&mc->cache_info.st.st_mtime));

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
			web_printf_all("[%+.1f%c]", diff, suffix);
		}
		
		web_printf_all(" client=[%s]\n", var_ctime_static(&mc->cache_info.client_mtime));
		return MG_TRUE;
	}
	
	
	// evt == MG_CACHE_INFO or MG_REQUEST
	
    char *o_uri = (char *) mc->uri;      // o_uri = original uri
    char *uri;
    bool free_uri = FALSE, has_prefix = FALSE, is_extension = FALSE;
    time_t mtime = 0;

    if (evt == MG_CACHE_INFO) web_printf_all("----\n");
    web_printf_all("%-16s %s %s\n", "URL", o_uri, mc->query_string);
    evWS(EC_EVENT, EV_WS, 0, "WEB_SERVER", evprintf("URL <%s> <%s> %s", o_uri, mc->query_string,
        (evt == MG_CACHE_INFO)? "MG_CACHE_INFO" : "MG_REQUEST"));
    
    // Only ACME requests can be on port 80 (if port 80 not configured as main Kiwi port).
    // Only port 80 can be used for ACME requests.
    bool acme = (strncmp(o_uri, "/.well-known/", 13) == 0);
    if ((isPort80 || acme) && evt == MG_CACHE_INFO) return MG_FALSE;
    if ((isPort80 && !acme) || (acme && !isPort80)) {
        printf("webserver: bad ACME request evt=%d %s:%d <%s>\n", evt, ip_forwarded, mc->local_port, o_uri);
        mg_send_status(mc, 403);    // 403 = "forbidden"
        mg_send_data(mc, NULL, 0);
        return MG_TRUE;
    }

    while (*o_uri == '/') o_uri++;
    if (*o_uri == '\0' || strcmp(o_uri, "index") == 0 || strcmp(o_uri, "index.html") == 0 || strcmp(o_uri, "index.htm") == 0) {
        o_uri = (char *) "index.html";

        // Kiwi URL redirection
        // don't redirect if camp specified in URL
        bool camp = (mc->query_string && (kiwi_str_begins_with((char *) mc->query_string, "camp") || strstr(mc->query_string, "&camp")));
        bool all_chans_full = (rx_count_server_conns(INCLUDE_INTERNAL) == rx_chans);
        if ((!camp && all_chans_full) || down || update_in_progress || backup_in_progress) {
            char *url_redirect = (char *) admcfg_string("url_redirect", NULL, CFG_REQUIRED);
            if (url_redirect != NULL && *url_redirect != '\0') {
            
                // if redirect url ends in numeric port number must add '/' before '?' of query string
                char *sep = (char *) (isdigit(url_redirect[strlen(url_redirect)-1])? "/?" : "?");
                kstr_t *args = mc->query_string? kstr_cat(sep, mc->query_string) : NULL;
                kstr_t *redirect = kstr_cat(url_redirect, args);
                printf("REDIRECT: %s\n", kstr_sp(redirect));
                mg_send_status(mc, 307);
                mg_send_header(mc, "Location", kstr_sp(redirect));
                mg_send_data(mc, NULL, 0);
                evWS(EC_EVENT, EV_WS, 0, "WEB_SERVER", "307 redirect");
                kstr_free(redirect);
                admcfg_string_free(url_redirect);
                return MG_TRUE;
            }
            admcfg_string_free(url_redirect);
        }
    }

    // SECURITY: prevent escape out of local directory
    mg_remove_double_dots_and_double_slashes((char *) mc->uri);

    char *suffix = strrchr(o_uri, '.');
    
    // disallow certain suffixes in *any* directory
    // NB: legitimate json files use .cjson suffix, e.g. web/extensions/ALE_2G/ALE_nets.cjson
    if (suffix) {
        //printf("SUFFIX <%s>\n", suffix);
        int last = strlen(suffix) - 1;
        if (last >= 0 && suffix[last] == '/')
            suffix[last] = '\0';
        if (strcmp(suffix, ".json") == 0 || strcmp(suffix, ".ini") == 0 || strcmp(suffix, ".conf") == 0) {
            if (evt == MG_REQUEST)
                lprintf("webserver: attempt to fetch config file: %s query=<%s> from %s\n", o_uri, mc->query_string, ip_forwarded);
            evWS(EC_EVENT, EV_WS, 0, "WEB_SERVER", "BAD fetch config file");
            return MG_FALSE;
        }
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
    if (strncmp(o_uri, "pkgs_maps/", 10) == 0) {
        uri = o_uri;
        has_prefix = TRUE;
    } else
    if (strncmp(o_uri, "config/", 7) == 0) {
        asprintf(&uri, "%s/%s", DIR_CFG, &o_uri[7]);
        if (MTR) real_printf("-> uri-1 ASPR %p %d<%s>\n", uri, strlen(uri), uri);
        free_uri = TRUE;
        has_prefix = TRUE;
    } else
    if (strncmp(o_uri, "kiwi.config/", 12) == 0) {
        asprintf(&uri, "%s/%s", DIR_CFG, &o_uri[12]);
        if (MTR) real_printf("-> uri-2 ASPR %p %d<%s>\n", uri, strlen(uri), uri);
        free_uri = TRUE;
        has_prefix = TRUE;
    } else
    // handle HTTP-01 authenticator challenges for ACME clients like certbot
    if (strncmp(o_uri, ".well-known/", 12) == 0) {
        asprintf(&uri, "%s/%s", DIR_CFG, o_uri);
        if (MTR) real_printf("-> ACME <%s>\n", uri);
        free_uri = TRUE;
        has_prefix = TRUE;
    } else {
        asprintf(&uri, "openwebrx/%s", o_uri);
        if (MTR) real_printf("-> uri-3 ASPR %p %d<%s>\n", uri, strlen(uri), uri);
        free_uri = TRUE;
    }
    //printf("---- HTTP: uri %s (orig %s)\n", uri, o_uri);
    
    #if 0
    if (cache.valid && evt == MG_REQUEST && strcmp(uri, cache.uri) == 0) {
        edata_data = cache.edata_data;
        edata_size = cache.edata_size;
        if (cache.free_uri) kiwi_ifree(cache.uri);
        cache.valid = FALSE;
    }
    #endif

    // try as file from in-memory embedded data or local filesystem
    bool is_min = false, is_gzip = false, is_file = false;
    edata_data = edata_with_file_ext(&uri, free_uri, &free_uri, NULL, evt == MG_CACHE_INFO, &edata_size, &mtime, &is_min, &is_gzip, &is_file);

    // try looking in "kiwi" subdir as a default if no prefix was used (or only ui subdir as prefix)
    if (!edata_data && !has_prefix) {
        if (free_uri) {
            if (MTR) real_printf("-> uri-!edata_data-&&-!has_prefix FREE %p %d<%s>\n", uri, strlen(uri), uri);
            kiwi_ifree(uri);
        }
        uri = o_uri;
        const char *prefix_kiwi[] = { "kiwi/", "", NULL };
        edata_data = edata_with_file_ext(&uri, FALSE, &free_uri, prefix_kiwi, evt == MG_CACHE_INFO, &edata_size, &mtime, &is_min, &is_gzip, &is_file);
    }

    // Process query string parameters even if file is cached.
    // Query string is only available on index.html connection (uri='/') but qs parameters
    // need to be passed to subsequent web socket connections.
    // How to do this? mc is different for each connection. Can only really be done by
    // js-side passing qs (which it has access to) on a web socket command.
    suffix = strrchr(uri, '.');
    if (edata_data && suffix && strcmp(suffix, ".html") == 0 && mc->query_string) {
        //printf("--> QS uri=%s qs=%s\n", mc->uri, mc->query_string);
        webserver_caching = cfg_bool("webserver_caching", NULL, CFG_REQUIRED);

        int ctrace;
        #define NQS 32
        char *r_buf, *qs[NQS+1];
        n = kiwi_split((char *) mc->query_string, &r_buf, "&", qs, NQS);
        for (i=0; i < n; i++) {
            if (strcmp(qs[i], "nocache") == 0) {
                web_nocache = true;
                printf("WEB nocache\n");
            } else
            if (strcmp(qs[i], "cache") == 0) {
                web_nocache = false;
                printf("WEB cache\n");
            } else
            if (sscanf(qs[i], "ctrace=%d", &ctrace) == 1) {
                web_caching_debug = ctrace;
                printf("WEB ctrace=%d\n", web_caching_debug);
            } else
            
            // needed before connection established, so can't use CMD_OPTIONS mechanism
            // (a slight race condition if there are multiple simultaneous connections)
            if (strcmp(qs[i], "camp") == 0) {
                force_camp = true;
            }
        }
        kiwi_ifree(r_buf);
    }

    // For extensions, try looking in external extension directory (outside this package).
    // SECURITY: But ONLY for extensions! Don't allow any other root-referenced accesses.
    // "o_uri" has been previously protected against "../" directory escape.
    if (!edata_data && is_extension) {
        if (free_uri) {
            if (MTR) real_printf("-> uri-is_extension FREE %p %d<%s>\n", uri, strlen(uri), uri);
            kiwi_ifree(uri);
            uri = NULL;
        }
        uri = o_uri;
        const char *prefix_ext[] = { "/root/", NULL };
        edata_data = edata_with_file_ext(&uri, FALSE, &free_uri, prefix_ext, evt == MG_CACHE_INFO, &edata_size, &mtime, &is_min, &is_gzip, &is_file);
    }

    #if 0
    // if we found the file, and this is the cache info phase, cache the result so a subsequent request lookup is faster
    if (evt == MG_CACHE_INFO && edata_data) {
        kiwi_ifree(cache.o_uri);
        cache.o_uri = strdup(o_uri);
        cache.edata_data = edata_data;
        cache.edata_size = edata_size;
        cache.valid = TRUE;
    }
    #endif

    // try as AJAX request
    char *ajax_data;
    bool isAJAX = false, free_ajax_data = false;
    if (!edata_data) {
    
        // don't try AJAX during the MG_CACHE_INFO pass
        if (evt == MG_CACHE_INFO) {
            if (free_uri) {
                if (MTR) real_printf("-> uri-MG_CACHE_INFO FREE %p %d<%s>\n", uri, strlen(uri), uri);
                kiwi_ifree(uri);
            }
            evWS(EC_EVENT, EV_WS, 0, "WEB_SERVER", "skip AJAX MG_CACHE_INFO");
            return MG_FALSE;
        }
        //printf("rx_server_ajax: %s\n", mc->uri);
        ajax_data = rx_server_ajax(mc, ip_forwarded);       // mc->uri is o_uri without "openwebrx" prefix
        
        // ajax_data can be: NULL, -1 or char *
        if (ajax_data) {
            if (FROM_VOID_PARAM(ajax_data) == -1) {
                if (free_uri) {
                    if (MTR) real_printf("-> uri-ajax FREE %p %d<%s>\n", uri, strlen(uri), uri);
                    kiwi_ifree(uri);
                }
                evWS(EC_EVENT, EV_WS, 0, "WEB_SERVER", "NO-REPLY");
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
        //printf("unknown URL: %s (%s) query=<%s> from %s\n", o_uri, uri, mc->query_string, ip_forwarded);
        web_printf_sent("%-16s %50s %15s %s\n", "webserver:", "NOT FOUND", ip_forwarded, isAJAX? mc->uri : uri);
        if (free_uri) {
            if (MTR) real_printf("-> uri-give-up FREE %p %d<%s>\n", uri, strlen(uri), uri);
            kiwi_ifree(uri);
        }
        evWS(EC_EVENT, EV_WS, 0, "WEB_SERVER", "GIVE UP");
        return MG_FALSE;
    }
    
    // for *.html and *.css process %[substitution]
    char *html_data;
    bool free_html_data = false;
    suffix = strrchr(uri, '.');     // NB: can't use previous suffix -- uri may have changed recently in some cases
    
    bool dirty = false;		// must never return a 304 if a %[] substitution was made!
    if (!isAJAX && suffix && (strcmp(suffix, ".html") == 0 || strcmp(suffix, ".css") == 0)) {
        int nsize = edata_size;
        html_data = (char *) kiwi_imalloc("html_data", nsize);
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
                        html_data = (char *) kiwi_irealloc("html_data", html_data, nsize+sl);
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
        if (MTR) real_printf("-> ver ASPR %p %d<%s>\n", ver, strlen(ver), ver);
        ver_size = strlen(ver);
    }

    // Tell web server the file size and modify time so it can make a decision about caching.
    // Modify time _was_ conservative: server start time as .js files have version info appended.
    // Modify time is now:
    //		server running in background (production) mode: build time of server binary.
    //		server running in foreground (development) mode: stat is fetched from filesystems, else build time of server binary.
    
    // NB: Will see cases of etag_match=N but not_mod_since=Y because of %[] substitution.
    // The size in the etag is different due to the substitution, but the underlying file mtime hasn't changed.
    
    mc->cache_info.st.st_size = edata_size + ver_size;
    if (!isAJAX) assert(mtime != 0);
    mc->cache_info.st.st_mtime = mtime;

    if (!(isAJAX && evt == MG_CACHE_INFO)) {		// don't print for isAJAX + MG_CACHE_INFO nop case
        web_printf_all("%-16s %s:%05d size=%6d dirty=%d mtime=[%s] %s %s %s%s\n", (evt == MG_CACHE_INFO)? "MG_CACHE_INFO" : "MG_REQUEST",
            ip_forwarded, mc->remote_port,
            mc->cache_info.st.st_size, dirty, var_ctime_static(&mtime), isAJAX? mc->uri : uri, mg_get_mime_type(isAJAX? mc->uri : uri, "text/plain"),
            (mc->query_string != NULL)? "qs:" : "", (mc->query_string != NULL)? mc->query_string : "");
    }

    bool isImage = (suffix && (strcmp(suffix, ".png") == 0 || strcmp(suffix, ".jpg") == 0 || strcmp(suffix, ".ico") == 0));
    int rtn = MG_TRUE;
    if (evt == MG_CACHE_INFO) {
        if (dirty || isAJAX || web_nocache || !webserver_caching) {
            //web_printf_all("%-16s NO CACHE %s\n", "MG_CACHE_INFO", isAJAX? mc->uri : uri);
            web_printf_all("%-16s NO CACHE %s%s%s\n", "MG_CACHE_INFO",
                dirty? "dirty-%[] " : "", isAJAX? "AJAX " : "", isAJAX? mc->uri : uri);
            rtn = MG_FALSE;		// returning false here will prevent any 304 decision based on the mtime set above
        }
    } else {
        // evt == MG_REQUEST
        const char *hdr_type;
    
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
        if (web_nocache || !webserver_caching) {
            mg_send_header(mc, "Content-Type", mg_get_mime_type(uri, "text/plain"));
            hdr_type = "NO-CACHE";
        } else {
            mg_send_standard_headers(mc, uri, &mc->cache_info.st, "OK", NULL, NULL);
            // Cache image files for a fixed amount of time to keep, e.g.,
            // GPS az/el img from flashing on periodic re-render with Safari.
            //mg_send_header(mc, "Cache-Control", "max-age=0");
            //mg_send_header(mc, "Cache-Control", isImage? "max-age=31536000":"max-age=0");
            mg_send_header(mc, "Cache-Control", isImage? "max-age=31536000":"max-age=0, must-revalidate");
            hdr_type = isImage? "CACHE-IMAGE" : "CACHE-REVAL";
        }
        
        if (is_gzip) mg_send_header(mc, "Content-Encoding", "gzip");
        
        web_printf_all("%-16s %11s %s%s\n", "sending", hdr_type, is_min? "MIN ":"", is_gzip? "GZIP":"");

        web_printf_sent("%-16s %7d %11s %4s %3s %4s %c%c|%c%c %5d%5s ", "webserver:", edata_size, hdr_type,
            is_file? "FILE":"", is_min? "MIN":"", is_gzip? "GZIP":"",
            mc->cache_info.if_none_match? 'T':'F', mc->cache_info.etag_match? 'T':'F',
            mc->cache_info.if_mod_since? 'T':'F', mc->cache_info.not_mod_since? 'T':'F',
            mc->local_port, mc->is_ssl? "(SSL)" : "");
        if (web_caching_debug & WEB_CACHING_DEBUG_SENT) {
            if (mc->cache_info.if_none_match && !mc->cache_info.etag_match)
                web_printf_sent("%s %s ", mc->cache_info.etag_server, mc->cache_info.etag_client);
            if (mc->cache_info.if_mod_since && !mc->cache_info.not_mod_since) {
	            // two web_printf_all() due to var_ctime_static()
                web_printf_sent("%s ", var_ctime_static(&mc->cache_info.server_mtime));
                web_printf_sent("%s ", var_ctime_static(&mc->cache_info.client_mtime));
            }
        }
        web_printf_sent("%15s %s\n", ip_forwarded, isAJAX? mc->uri : uri);
        if (!isAJAX) web_has_served("SENT", ip_forwarded, mc->uri);
        
        mg_send_header(mc, "Server", web_server_hdr);
        
        evWS(EC_EVENT, EV_WS, 0, "WEB_SERVER", "mg_send_data..");
        mg_send_data(mc, kstr_sp((char *) edata_data), edata_size);
        evWS(EC_EVENT, EV_WS, 0, "WEB_SERVER", "..mg_send_data");

        if (ver != NULL) {
            mg_send_data(mc, ver, ver_size);
        }
    }
    
    if (ver != NULL) {
        if (MTR) real_printf("-> ver FREE %p %d<%s>\n", ver, strlen(ver), ver);
        kiwi_ifree(ver);
    }
    if (free_html_data) kiwi_ifree((void *) html_data, "html_data");
    if (free_ajax_data) kstr_free((char *) ajax_data);
    if (free_uri) {
        if (MTR) real_printf("-> uri-bottom FREE %p %d<%s>\n", uri, strlen(uri), uri);
        kiwi_ifree(uri);
    }
    
    if (evt != MG_CACHE_INFO) http_bytes += edata_size;
    evWS(EC_EVENT, EV_WS, 0, "WEB_SERVER", evprintf("edata_size %d", edata_size));
    return rtn;
}

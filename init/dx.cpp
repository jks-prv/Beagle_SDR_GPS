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

// Copyright (c) 2016 John Seamons, ZL/KF6VO

#include "types.h"
#include "config.h"
#include "kiwi.h"
#include "options.h"
#include "mem.h"
#include "misc.h"
#include "str.h"
#include "timer.h"
#include "web.h"
#include "jsmn.h"
#include "cfg.h"
#include "dx.h"
#include "coroutines.h"

#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <signal.h>

// maintains a dx_t/dxlist_t struct parallel to JSON for fast lookups

//#define TMEAS(x) x
#define TMEAS(x)

#include "EiBi.h"

dxlist_t dx;

// create JSON string from dx_t struct representation
void dx_save_as_json()
{
	int i, n;
	cfg_t *cfg = &cfg_dx;
	dx_t *dxp;
	char *sb;

	TMEAS(u4_t start = timer_ms();)
	TMEAS(printf("DX_UPD dx_save_as_json: START saving as dx.json, %d entries\n", dx.stored_len);)
	
	typedef struct { char *sp; int sl; } dx_a_t;
	dx_a_t *dx_a = (dx_a_t *) kiwi_imalloc("dx_a", dx.stored_len * sizeof(dx_a_t));
	int sb_len = 0;

	for (i=0, dxp = dx.stored_list; i < dx.stored_len; i++, dxp++) {
	    char *ident = kiwi_str_decode_selective_inplace(strdup(dxp->ident), FEWER_ENCODED);
	    char *notes = dxp->notes? kiwi_str_decode_selective_inplace(strdup(dxp->notes), FEWER_ENCODED) : strdup("");
	    char *params = (dxp->params && *dxp->params)? kiwi_str_decode_selective_inplace(strdup(dxp->params), FEWER_ENCODED) : NULL;
	    
	    sb = kstr_asprintf(NULL, "[%.2f, \"%s\", \"%s\", \"%s\"",
	        dxp->freq, modu_s[dxp->flags & DX_MODE], ident, notes);
        free(ident); free(notes);   // not kiwi_ifree() because from strdup()

		u4_t type = dxp->flags & DX_TYPE;
		if (type || dxp->low_cut || dxp->high_cut || dxp->offset || params) {
			const char *delim = ", {";
			const char *type_s = "";
			
			/*
			// deprecated
			if (type == DX_WL) type_s = "WL"; else
			if (type == DX_SB) type_s = "SB"; else
			if (type == DX_DG) type_s = "DG"; else
			if (type == DX_SE) type_s = "SE"; else
			if (type == DX_XX) type_s = "XX"; else
			if (type == DX_MASKED) type_s = "MK";
			
			if (type) {
			    sb = kstr_asprintf(sb, "%s\"%s\":1", delim, type_s);
			    delim = ", ";
			}
			*/
			
			if (type) {
			    sb = kstr_asprintf(sb, "%s\"T%d\":1", delim, DX_STORED_TYPE2IDX(type));
			    delim = ", ";
			}

			if (dxp->low_cut) {
			    sb = kstr_asprintf(sb, "%s\"lo\":%d", delim, dxp->low_cut);
			    delim = ", ";
			}
			
			if (dxp->high_cut) {
			    sb = kstr_asprintf(sb, "%s\"hi\":%d", delim, dxp->high_cut);
			    delim = ", ";
			}
			
			if (dxp->offset) {
			    sb = kstr_asprintf(sb, "%s\"o\":%d", delim, dxp->offset);
			    delim = ", ";
			}
			
			u2_t dow = dxp->flags & DX_DOW;
			if (dow != 0 && dow != DX_DOW) {
			    sb = kstr_asprintf(sb, "%s\"d0\":%d", delim, dow >> DX_DOW_SFT);
			    delim = ", ";
			}
			
            u2_t time_begin = dxp->time_begin, time_end = dxp->time_end;
            if (time_begin == 0 && time_end == 2400) time_end = 0;
			if (time_begin != 0 || time_end != 0) {
			    sb = kstr_asprintf(sb, "%s\"b0\":%d, \"e0\":%d", delim, time_begin, time_end);
			    delim = ", ";
			}
			
			if (params) {
			    sb = kstr_asprintf(sb, "%s\"p\":\"%s\"", delim, params);
			}
			
			sb = kstr_cat(sb, "}");
			free(params);   // not kiwi_ifree() because from strdup()
		}

        sb = kstr_asprintf(sb, "]%s\n", (i != dx.stored_len-1)? ",":"");
        n = kstr_len(sb);
        sb_len += n;
        dx_a[i].sl = n;
        dx_a[i].sp = kstr_free_return_malloced(sb);

		if ((i & 0x1f) == 0) {
		    NextTask("dx_save_as_json");
		}
	}
	
	kiwi_ifree(dx.json);
	TMEAS(u4_t split = timer_ms(); printf("DX_UPD sb_len=%d %.3f sec\n", sb_len, TIME_DIFF_MS(split, start));)
	dx.json = (char *) kiwi_imalloc("dx.json", sb_len + 32);     // 32 is for '{"dx":[...]}' below
	char *cp = dx.json;
	int tsize = 0;
	n = sprintf(cp, "{\"dx\":[\n");
	cp += n;
	tsize += n;

	for (i = 0; i < dx.stored_len; i++) {
	    strcpy(cp, dx_a[i].sp);
	    kiwi_ifree(dx_a[i].sp);
	    n = dx_a[i].sl;
	    cp += n;
	    tsize += n;
		if ((i & 0x1f) == 0) {
		    NextTask("dx_save_as_json");
		}
	}

	kiwi_ifree(dx_a);
	n = sprintf(cp, "]}");      // dxcfg_save_json() adds final \n
    tsize += n;
	TMEAS(printf("DX_UPD tsize=%d\n", tsize);)
    cfg->json = dx.json;
    cfg->json_buf_size = tsize;

	TMEAS(u4_t split2 = timer_ms(); printf("DX_UPD dx_save_as_json: dx struct -> json string %.3f sec\n", TIME_DIFF_MS(split2, split));)
	dxcfg_save_json(cfg->json);
	TMEAS(u4_t now = timer_ms(); printf("DX_UPD dx_save_as_json: DONE %.3f/%.3f sec\n", TIME_DIFF_MS(now, split2), TIME_DIFF_MS(now, start));)
}

static void dx_mode(dx_t *dxp, const char *s)
{
    int i;
    
    for (i = 0; i < N_MODE; i++) {
        if (strcasecmp(s, modu_s[i]) == 0)
            break;
    }
    
    if (i == N_MODE) {
	    lprintf("unknown dx config mode \"%s\", defaulting to AM\n", s);
	    i = 0;
	}
	
	dxp->flags = i;
}

static void dx_flag(dx_t *dxp, const char *flag)
{
    u4_t type;
    
    // legacy compatibility
	if (strcmp(flag, "WL") == 0) dxp->flags |= DX_WL; else
	if (strcmp(flag, "SB") == 0) dxp->flags |= DX_SB; else
	if (strcmp(flag, "DG") == 0) dxp->flags |= DX_DG; else
	if (strcmp(flag, "NoN") == 0) dxp->flags |= DX_SE; else     // deprecated, convert to "SE" on next file write
	if (strcmp(flag, "SE") == 0) dxp->flags |= DX_SE; else
	if (strcmp(flag, "XX") == 0) dxp->flags |= DX_XX; else
	if (strcmp(flag, "PB") == 0) ; else     // deprecated, but here in case any json file still has it
	if (strcmp(flag, "MK") == 0) dxp->flags |= DX_MASKED; else
	
	// new format
	if (sscanf(flag, "T%d", &type) == 1) {
	    dxp->flags |= (type << DX_TYPE_SFT) & DX_TYPE;
	} else
	
	lprintf("%.2f \"%s\": unknown dx flag \"%s\"\n", dxp->freq, dxp->ident, flag);
}

// prepare dx list by conditionally sorting, initializing self indexes and constructing new masked freq list
void dx_prep_list(bool need_sort, dx_t *_dx_list, int _dx_list_len, int _dx_list_len_new)
{
    int i, j;
    dx_t *dxp;
    
	if (need_sort) qsort(_dx_list, _dx_list_len, sizeof(dx_t), qsort_floatcomp);

    // have to sort first before rebuilding masked list in case an entry is being deleted
    dx.masked_len = 0;
    for (i = 0, dxp = _dx_list; i < _dx_list_len_new; i++, dxp++) {
        dxp->idx = i;   // init self index
        if ((dxp->flags & DX_TYPE) == DX_MASKED) dx.masked_len++;
    }
    kiwi_ifree(dx.masked_list); dx.masked_list = NULL;
    if (dx.masked_len > 0) dx.masked_list = (dx_mask_t *) kiwi_imalloc("dx_prep_list", dx.masked_len * sizeof(dx_mask_t));

    for (i = j = 0, dxp = _dx_list; i < _dx_list_len_new; i++, dxp++) {
        if ((dxp->flags & DX_TYPE) == DX_MASKED) {
            dx_mask_t *dmp = &dx.masked_list[j++];
            int mode = dxp->flags & DX_MODE;
            int masked_f = roundf(dxp->freq * kHz);
            int hbw = mode_hbw[mode];
            int offset = mode_offset[mode];
            dmp->masked_lo = masked_f + offset + (dxp->low_cut? dxp->low_cut : -hbw);
            dmp->masked_hi = masked_f + offset + (dxp->high_cut? dxp->high_cut : hbw);
            //printf("masked %.3f %d-%d %s hbw=%d off=%d lc=%d hc=%d\n",
            //    dxp->freq, dmp->masked_lo, dmp->masked_hi, modu_s[mode], hbw, offset, dxp->low_cut, dxp->high_cut);
        }
    }

    // this causes all active user waterfall tasks to reload the dx list when it changes
    // from dx edit panel or admin dx tab
    dx.update_seq++;
}

enum { E_ARRAY = 0, E_FREQ, E_MODE, E_IDENT, E_NOTES, E_OPT_ARRAY, E_OPT_ID } error_e;
	
static const char *error_s[] = {
    "enclosing array missing",
    "expected frequency number (1st parameter)",
    "expected mode string (2nd parameter)",
    "expected ident string (3rd parameter)",
    "expected notes string (4th parameter)",
    "expected options array",
    "expected options key"
};

static void dx_reload_error(cfg_t *cfg, int line, int error, bool have_source)
{
    if (have_source) {
        printf("DX ERROR: line=%d %s\n", line, error_s[error]);
        printf("DX ERROR: %s\n", cfg->json);
    } else {
        printf("DX ERROR: %s\n", error_s[error]);
        panic("dx_reload_error");
    }
}

static jsmntok_t *dx_reload_element(jsmntok_t *jt, jsmntok_t *end_tok, dx_t *dxp, int *error)
{
	const char *s;
    u4_t dow = DX_DOW;
    
    if (!JSMN_IS_ARRAY(jt)) { *error = E_ARRAY; return NULL; }
    jt++;
    
    double f;
    if (dxcfg_float_json(jt, &f) == false) { *error = E_FREQ; return NULL; }
    dxp->freq = f;
    jt++;
    
    const char *mode;
    if (dxcfg_string_json(jt, &mode) == false) { *error = E_MODE; return NULL; }
    dx_mode(dxp, mode);
    dxcfg_string_free(mode);
    jt++;
    
    if (dxcfg_string_json(jt, &s) == false) { *error = E_IDENT; return NULL; }
    kiwi_str_unescape_quotes((char *) s);
    dxp->ident_s = strdup(s);
    dxp->ident = kiwi_str_encode((char *) s);
    dxcfg_string_free(s);
    jt++;
    
    if (dxcfg_string_json(jt, &s) == false) { *error = E_NOTES; return NULL; }
    kiwi_str_unescape_quotes((char *) s);
    dxp->notes_s = strdup(s);
    dxp->notes = kiwi_str_encode((char *) s);
    dxcfg_string_free(s);
    if (*dxp->notes == '\0') {
        dxcfg_string_free(dxp->notes);
        dxp->notes = NULL;
    }
    jt++;
    
    // deprecated
    int timestamp;
    if (dxcfg_int_json(jt, &timestamp)) {
        jt++;
    } else {
        //printf("### DX #%d missing timestamp\n", i);
        //dxp->timestamp = utc_time_since_2018() / 60;
    }
    
    int tag;
    if (dxcfg_int_json(jt, &tag)) {
        jt++;
    } else {
        //printf("### DX #%d missing tag\n", i);
        //dxp->tag = random() % 10000;
    }
    
    //printf("dx.json %d %.2f 0x%x \"%s\" \"%s\"\n", i, dxp->freq, dxp->flags, dxp->ident, dxp->notes);

    if (jt != end_tok && JSMN_IS_OBJECT(jt)) {
        jt++;
        while (jt != end_tok && !JSMN_IS_ARRAY(jt)) {
            if (!JSMN_IS_ID(jt)) { *error = E_OPT_ARRAY; return NULL; }
            const char *id;
            if (dxcfg_string_json(jt, &id) == false) { *error = E_OPT_ID; return NULL; }
            jt++;
            
            int num;
            if (dxcfg_int_json(jt, &num) == true) {
                if (strcmp(id, "lo") == 0) {
                    dxp->low_cut = num;
                } else
                
                if (strcmp(id, "hi") == 0) {
                    dxp->high_cut = num;
                } else
                
                if (strcmp(id, "o") == 0) {
                    dxp->offset = num;
                    //printf("dx.json %d offset %s %d\n", i, id, num);
                } else
                
                if (strcmp(id, "d0") == 0) {
                    dow = (num << DX_DOW_SFT) & DX_DOW;
                    if (dow == 0) dow = DX_DOW;
                } else
                
                if (strcmp(id, "b0") == 0) {
                    dxp->time_begin = num;
                } else
                
                if (strcmp(id, "e0") == 0) {
                    dxp->time_end = num;
                } else {
                    if (num) {
                        dx_flag(dxp, id);
                        //printf("dx.json %d dx_flag %s\n", i, id);
                    }
                }
            } else
            if (dxcfg_string_json(jt, &s) == true) {
                //printf("dx.json %s=<%s>\n", id, s);
                if (strcmp(id, "p") == 0) {
                    kiwi_str_unescape_quotes((char *) s);
                    dxp->params = kiwi_str_encode((char *) s);
                }
                dxcfg_string_free(s);
            }

            dxcfg_string_free(id);
            jt++;
        }
    }
    
    dxp->flags |= dow;
    if (dxp->time_begin == 0 && dxp->time_end == 0) dxp->time_end = 2400;
    
    return jt;
}

static void dx_reload_finalize(dx_t *_dx_list, int _dx_list_len)
{
    int i;
    dx_prep_list(true, _dx_list, _dx_list_len, _dx_list_len);

	// switch to new list
	dx_t *prev_dx_list = dx.stored_list;
	int prev_dx_list_len = dx.stored_len;

	dx.stored_list = _dx_list;
	dx.stored_len = _dx_list_len;
	
	for (i = 0; i < MAX_RX_CHANS; i++) {
        dx_rx_t *drx = &dx.dx_rx[i];
	    drx->db = DB_STORED;
	}

	dx.hidden_used = false;
	dx.json_up_to_date = true;
	
	// release previous
	if (prev_dx_list) {
		dx_t *dxp;
		
		for (i = 0, dxp = prev_dx_list; i < prev_dx_list_len; i++, dxp++) {
			// previous allocators better have used malloc(), strdup() et al for these and not kiwi_malloc()
			kiwi_ifree((void *) dxp->ident_s);
			kiwi_ifree((void *) dxp->ident);
			kiwi_ifree((void *) dxp->notes_s);
			kiwi_ifree((void *) dxp->notes);
			kiwi_ifree((void *) dxp->params);
		}
	}
	
	kiwi_free("dx_list", prev_dx_list);
}

// create and switch to new dx_t struct from JSON token list representation

#ifdef OPTION_DX_INCREMENTAL_PARSE

// caller has not read JSON file nor parsed it (do that here on a per-line basis)
static void _dx_reload_file(cfg_t *cfg)
{
    int i;
    FILE *fp;

    lprintf("reading configuration from file %s\n", cfg->filename);
    scallz("_cfg_load_json fopen", (fp = fopen(cfg->filename, "r")));

    #define LBUF 512
    char lbuf[LBUF], *s;
    fgets(lbuf, LBUF, fp);      // skip first line
    dx.lines = 1;
    cfg->json = lbuf;

	jsmn_parser parser;
	check(cfg->tok_size == 0);
	#define NTOK 32
	cfg->tok_size = NTOK;
    cfg->tokens = (jsmntok_t *) kiwi_malloc("cfg tokens", sizeof(jsmntok_t) * cfg->tok_size);
    
	int size_i = 0;
	dx_t *_dx_list = NULL;

	dx_t *dxp;
	bool done = false;

    do {
        for (i = 0; !done; i++) {
            if (i >= size_i) {
                _dx_list = (dx_t *) kiwi_table_realloc("dx_list", _dx_list, size_i, DX_LIST_ALLOC_CHUNK, sizeof(dx_t));
                size_i = size_i + DX_LIST_ALLOC_CHUNK;
            }

            dxp = &_dx_list[i];
            if ((s = fgets(lbuf, LBUF, fp)) != NULL) {
                dx.lines++;
                jsmn_init(&parser);
                int slen = strlen(s);
                if (slen > 0) {
                    slen--;     // remove \n
                    if (slen > 0) if (s[slen-1] == ',') slen--;
                    s[slen] = '\0';
                }
                cfg->json_buf_size = slen + SPACE_FOR_NULL;
                int rc = jsmn_parse(&parser, s, slen, cfg->tokens, cfg->tok_size, /* yield */ false);
                //printf("DX: #%d rc=%d <%s>\n", i, rc, s);
                if (rc == 0) { i--; continue; }
                if (rc < 0) {
                    if (strcmp(s, "]}") != 0) {          // i.e. "]}" on last line
                        const char *err = "(unknown error)";
                        if (rc == JSMN_ERROR_INVAL) err = "invalid character inside JSON string"; else
                        if (rc == JSMN_ERROR_PART) err = "the string is not a full JSON packet, more bytes expected";
                        lprintf("DX ERROR: line=%d position=%d token=%d %s\n", dx.lines, parser.pos, parser.toknext, err);
                        dx.json_parse_errors++;
                        lprintf("DX ERROR: %s\n", s);
                        lprintf("DX ERROR: %*s JSON error position\n", parser.pos, "^");
                    }

                    i--;    // don't load labels with errors
                    continue;
                } else {
                    cfg->ntok = rc;
                    check(cfg->ntok < NTOK);
                    jsmntok_t *end_tok = &(cfg->tokens[cfg->ntok]);
                    int error;
                    jsmntok_t *jt = dx_reload_element(cfg->tokens, end_tok, dxp, &error);
                    if (jt == NULL) {
                        dx_reload_error(cfg, dx.lines, error, true);
                        dx.dx_format_errors++;
                        i--;    // don't load labels with errors
                    }
                }
            } else {
                i--;    // detection of EOF is another trip through loop
                done = true;
            }
        }
    } while (!done);
    
    // Don't have to do anything to make room for DX_HIDDEN_SLOT because the
    // extra trip through the loop to detect EOF guarantees _dx_list has at least one more slot.
    
    //printf("%d/%d DX entries\n", i, size_i);
    lprintf("DX: %d label entries", i);
    if (dx.json_parse_errors) lprintf(", %d JSON parse error%s", dx.json_parse_errors, (dx.json_parse_errors == 1)? "":"s");
    if (dx.dx_format_errors) lprintf(", %d label format error%s", dx.dx_format_errors, (dx.dx_format_errors == 1)? "":"s");
    lprintf("\n");

    fclose(fp);
    kiwi_free("cfg tokens", cfg->tokens);
    
    int _dx_list_len = i;   // NB: doesn't include DX_HIDDEN_SLOT
    dx.stored_alloc_size = size_i;
	dx_reload_finalize(_dx_list, _dx_list_len);
}

#else

// caller has already read JSON file and parsed it
static void _dx_reload_json(cfg_t *cfg)
{
	jsmntok_t *end_tok = &(cfg->tokens[cfg->ntok]);
	jsmntok_t *jt = dxcfg_lookup_json("dx");
	check(jt != NULL);
	check(JSMN_IS_ARRAY(jt));
	int _dx_list_len = jt->size;
	jt++;
	
	lprintf("DX: %d label entries\n", _dx_list_len);
	
	// NB: kiwi_malloc() zeros mem
	dx_t *_dx_list = (dx_t *) kiwi_malloc("dx_list", (_dx_list_len + DX_HIDDEN_SLOT) * sizeof(dx_t));
	
	dx_t *dxp = _dx_list;
	int i = 0;

	for (; jt != end_tok; dxp++, i++) {
        check(i < _dx_list_len);
        int error;
	    jt = dx_reload_element(jt, end_tok, dxp, &error);
	    if (jt == NULL) {
	        dx_reload_error(cfg, -1, error, false);
	    }
	}
	
	dx_reload_finalize(_dx_list, _dx_list_len);
}
#endif

// reload requested at startup
void dx_reload()
{
	cfg_t *cfg = &cfg_dx;
	
	check(DX_MODE <= N_MODE);

	TMEAS(u4_t start = timer_ms();)
	TMEAS(printf("DX_RELOAD START\n");)
	if (!dxcfg_init())
		return;
	
    TMEAS(u4_t split = timer_ms(); printf("DX_RELOAD json file read and json struct %.3f sec\n", TIME_DIFF_MS(split, start));)
	#ifdef OPTION_DX_INCREMENTAL_PARSE
        _dx_reload_file(cfg);
	#else
        //dxcfg_walk(NULL, cfg_print_tok, NULL);
        _dx_reload_json(cfg);
    #endif
    TMEAS(u4_t now = timer_ms(); printf("DX_RELOAD DONE json struct -> dx struct %.3f/%.3f sec\n", TIME_DIFF_MS(now, split), TIME_DIFF_MS(now, start));)
}

void dx_eibi_init()
{
    int i, n = 0;
    dx_t *dxp;
    //float f = 0;
    
    for (i = 0, dxp = eibi_db; dxp->freq; i++, dxp++) {
        //if (dxp->freq == f) continue;   // temp: only take first per freq
        n++;
        //f = dxp->freq;
        dxp->ident_s = eibi_ident[dxp->ident_idx];
		dxp->ident = kiwi_str_encode((char *) dxp->ident_s);
        //asprintf(&(dxp->notes_s), "%04d-%04d %s", dxp->time_begin, dxp->time_end, eibi_notes[dxp->notes_idx]);
        //dxp->notes_s = (char *) eibi_notes[dxp->notes_idx];
        dxp->notes_s = (char *) "";
		dxp->notes = kiwi_str_encode((char *) dxp->notes_s);
		dxp->params = "";
		dxp->idx = i;
		
		// setup special mode-specific passbands
		int type = dxp->flags & DX_TYPE;
		if (type == DX_ALE)  { dxp->low_cut = 600; dxp->high_cut = 2650; } else
		if (type == DX_HFDL) { dxp->low_cut = 300; dxp->high_cut = 2600; } else
		if (type == DX_FAX)  { dxp->low_cut = 800; dxp->high_cut = 3000; } else
		    ;
    }
    printf("DX_UPD dx_eibi_init: %d/%d EiBi entries\n", n, i);

    dx.eibi_list = eibi_db;
    dx.eibi_len = n;
}

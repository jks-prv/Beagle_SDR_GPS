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

// Copyright (c) 2016 John Seamons, ZL4VO/KF6VO

#include "types.h"
#include "config.h"
#include "kiwi.h"
#include "mode.h"
#include "options.h"
#include "mem.h"
#include "net.h"
#include "misc.h"
#include "str.h"
#include "timer.h"
#include "web.h"
#include "jsmn.h"
#include "cfg.h"
#include "dx.h"
#include "rx.h"
#include "rx_util.h"
#include "coroutines.h"
#include "sha256.h"

#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <signal.h>

// maintains a dx_t/dxlist_t struct parallel to JSON for fast lookups

//#define TMEAS(x) x
#define TMEAS(x)

#define _      0
#define EXT    DX_HAS_EXT

#define BCST   DX_BCAST
#define UTIL   DX_UTIL
#define TIME   DX_TIME
#define ALE    DX_ALE
#undef  HFDL
#define HFDL   DX_HFDL
#define MIL    DX_MILCOM
#define CW_    DX_CW
#define FSK    DX_FSK
#define FAX    DX_FAX
#define AERO   DX_AERO
#define MAR    DX_MARINE
#define SPY    DX_SPY

#define AM     MODE_AM
#define AMN    MODE_AMN
#define USB    MODE_USB
#define LSB    MODE_LSB
#define CW     MODE_CW
#define CWN    MODE_CWN
#define IQ     MODE_IQ

#include "EiBi.h"

#undef _
#undef EXT

#undef BCST
#undef UTIL
#undef TIME
#undef ALE
#undef HFDL
#undef MIL
#undef CW_
#undef FSK
#undef FAX
#undef AERO
#undef MAR
#undef SPY

#undef AM
#undef AMN
#undef USB
#undef LSB
#undef CW
#undef CWN
#undef IQ

dxlist_t dx;

// create JSON string from dx_t struct representation
void dx_save_as_json(dx_db_t *dx_db, bool dx_label_foff_convert)
{
	int i, n;
	cfg_t *cfg = &cfg_dx;
	dx_t *dxp;
	char *sb;

    check (dx_db->db == DB_STORED);
	TMEAS(u4_t start = timer_ms();)
	TMEAS(printf("DX_UPD dx_save_as_json: START saving as dx-json, %d entries\n", dx_db->actual_len);)
	
	if (dx_label_foff_convert && !freq.isOffset) {
	    lprintf("dx_save_as_json: DX_LABEL_FOFF_CONVERT requested, but freq not offset. Ignored!\n");
	    dx_label_foff_convert = false;
	}
	
	typedef struct { char *sp; int sl; } dx_a_t;
	dx_a_t *dx_a = (dx_a_t *) kiwi_imalloc("dx_a", dx_db->actual_len * sizeof(dx_a_t));
	int sb_len = 0;

	for (i=0, dxp = dx_db->list; i < dx_db->actual_len; i++, dxp++) {
	    char *ident = kiwi_str_decode_selective_inplace(strdup(dxp->ident), FEWER_ENCODED);
	    char *notes = dxp->notes? kiwi_str_decode_selective_inplace(strdup(dxp->notes), FEWER_ENCODED) : strdup("");
	    char *params = (dxp->params && *dxp->params != '\0')? kiwi_str_decode_selective_inplace(strdup(dxp->params), FEWER_ENCODED) : NULL;
	    
	    double freq_kHz = dxp->freq;
	    if (dx_label_foff_convert && freq_kHz <= 32000) {
	        freq_kHz += freq.offset_kHz;
	        printf("DX CONVERT: %.2f => %.2f %s\n", dxp->freq, freq_kHz, ident);
	    }
	    int mode_i = DX_DECODE_MODE(dxp->flags);
	    sb = kstr_asprintf(NULL, "[%.2f, \"%s\", \"%s\", \"%s\"", freq_kHz, mode_uc[mode_i], ident, notes);
        free(ident); free(notes);

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
			    sb = kstr_asprintf(sb, "%s\"T%d\":1", delim, DX_STORED_FLAGS_TYPEIDX(type));
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
			
			if (dxp->sig_bw) {
			    sb = kstr_asprintf(sb, "%s\"s\":%d", delim, dxp->sig_bw);
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
			free(params);
		}

        sb = kstr_asprintf(sb, "]%s\n", (i != dx_db->actual_len-1)? ",":"");
        n = kstr_len(sb);
        sb_len += n;
        dx_a[i].sl = n;
        dx_a[i].sp = kstr_free_return_malloced(sb);

		if ((i & 0x1f) == 0) {
		    NextTask("dx_save_as_json");
		}
	}
	
	kiwi_ifree(dx_db->json, "dx json");
	TMEAS(u4_t split = timer_ms(); printf("DX_UPD sb_len=%d %.3f sec\n", sb_len, TIME_DIFF_MS(split, start));)
    int rem = sb_len + 32;      // 32 is for '{"dx":[...]}' below
	dx_db->json = (char *) kiwi_imalloc("dx-json", rem);
	char *cp = dx_db->json;
	int tsize = 0;
	n = kiwi_snprintf_ptr(cp, rem, "{\"dx\":[\n");
	cp += n;
	rem -= n;
	tsize += n;

	for (i = 0; i < dx_db->actual_len; i++) {
	    strcpy(cp, dx_a[i].sp);
	    kiwi_ifree(dx_a[i].sp, "dx_a[]");
	    n = dx_a[i].sl;
	    cp += n;
	    rem -= n;
	    tsize += n;
		if ((i & 0x1f) == 0) {
		    NextTask("dx_save_as_json");
		}
	}

	kiwi_ifree(dx_a, "dx_a");
	n = kiwi_snprintf_ptr(cp, rem, "]}");    // dx_save_json() adds final \n
    tsize += n;
	TMEAS(printf("DX_UPD tsize=%d\n", tsize);)
    cfg->json = dx_db->json;
    cfg->json_buf_size = tsize + SPACE_FOR_NULL;

	TMEAS(u4_t split2 = timer_ms(); printf("DX_UPD dx_save_as_json: dx struct -> json string %.3f sec\n", TIME_DIFF_MS(split2, split));)
	dx_save_json(cfg->json);
	TMEAS(u4_t now = timer_ms(); printf("DX_UPD dx_save_as_json: DONE %.3f/%.3f sec\n", TIME_DIFF_MS(now, split2), TIME_DIFF_MS(now, start));)
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

void update_masked_freqs(dx_t *_dx_list, int _dx_list_len)
{
    int i, j;
    dx_t *dxp;
    //printf("update_masked_freqs\n");
    
    // always use masked freqs set by admin in stored list, even when other lists currently active
    if (_dx_list == NULL) {
        dx_db_t *dx_db = &dx.dx_db[DB_STORED];
        _dx_list = dx_db->list;
        _dx_list_len = dx_db->actual_len;
    }

    dx.masked_len = 0;
    for (i = 0, dxp = _dx_list; i < _dx_list_len; i++, dxp++) {
        if ((dxp->flags & DX_TYPE) == DX_MASKED && rx_freq_inRange(dxp->freq))
            dx.masked_len++;
    }
    kiwi_ifree(dx.masked_list, "dx masked"); dx.masked_list = NULL;
    if (dx.masked_len > 0) dx.masked_list = (dx_mask_t *) kiwi_imalloc("update_masked_freqs", dx.masked_len * sizeof(dx_mask_t));

    for (i = j = 0, dxp = _dx_list; i < _dx_list_len; i++, dxp++) {
        if ((dxp->flags & DX_TYPE) == DX_MASKED && rx_freq_inRange(dxp->freq)) {
            dx_mask_t *dmp = &dx.masked_list[j++];
            int mode_i = DX_DECODE_MODE(dxp->flags);
            int masked_f = roundf((dxp->freq - freq.offset_kHz) * kHz);     // masked freq list always baseband
            int hbw = mode_hbw[mode_i];
            int offset = mode_offset[mode_i];
            dmp->masked_lo = masked_f + offset + (dxp->low_cut? dxp->low_cut : -hbw);
            dmp->masked_hi = masked_f + offset + (dxp->high_cut? dxp->high_cut : hbw);
            //printf("masked %.2f baseband: %d-%d %s hbw=%d off=%d lc=%d hc=%d\n",
            //    dxp->freq, dmp->masked_lo, dmp->masked_hi, mode_uc[mode_i], hbw, offset, dxp->low_cut, dxp->high_cut);
        }
    }
}

// prepare dx list by conditionally sorting, initializing self indexes and constructing new masked freq list
void dx_prep_list(dx_db_t *dx_db, bool need_sort, dx_t *_dx_list, int _dx_list_len_prev, int _dx_list_len_new)
{
    int i;
    dx_t *dxp;
    
    // have to sort first before rebuilding masked list in case an entry is being deleted
	if (dx_db->db == DB_STORED && need_sort)
	    qsort(_dx_list, _dx_list_len_prev, sizeof(dx_t), qsort_doublecomp);

    for (i = 0, dxp = _dx_list; i < _dx_list_len_new; i++, dxp++) {
        dxp->idx = i;   // init self index
    }
    
    if (dx_db->db != DB_STORED) return;
    
    update_freqs();     // on startup dx list read before cfg setup
    update_masked_freqs(_dx_list, _dx_list_len_new);

    // this causes all active user waterfall tasks to reload the dx list when it changes
    // from dx edit panel or admin dx tab
    dx.update_seq++;
}

enum { E_ARRAY = 0, E_FREQ, E_MODE, E_IDENT, E_NOTES, E_OPT_ARRAY, E_OPT_ID, E_SORT, E_UNEXPECTED } error_e;
	
static const char *error_s[] = {
    "enclosing array missing",
    "expected frequency number (1st parameter)",
    "expected mode string (2nd parameter)",
    "expected ident string (3rd parameter)",
    "expected notes string (4th parameter)",
    "expected options array",
    "expected options key",
    "frequency sort error",
    "unexpected error"
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

static jsmntok_t *dx_reload_element(cfg_t *cfg, dx_db_t *dx_db, jsmntok_t *jt, jsmntok_t *end_tok, dx_t *dxp, int *error)
{
	const char *s;
    u4_t dow = DX_DOW;
    *error = E_UNEXPECTED;
    
    if (!JSMN_IS_ARRAY(jt)) { *error = E_ARRAY; return NULL; }
    jt++;
    
    double f;
    if (cfg_float_tok(cfg, jt, &f) == false) { *error = E_FREQ; return NULL; }
    dxp->freq = f;
    jt++;
    
    if (f < dx_db->last_freq) {
        dx_db->last_freq = 0;
        *error = E_SORT;
        return NULL;
    }
    dx_db->last_freq = f;
    
    const char *mode_s;
    if (cfg_string_tok(cfg, jt, &mode_s) == false) { *error = E_MODE; return NULL; }
    int mode_i = rx_mode2enum(mode_s);
    if (mode_i == NOT_FOUND) mode_i = 0;    // default to AM
	dxp->flags = DX_ENCODE_MODE(mode_i);    // dxp->flags has no prior bits to preserve
    cfg_string_tok_free(cfg, mode_s);
    jt++;
    
    if (cfg_string_tok(cfg, jt, &s) == false) { *error = E_IDENT; return NULL; }
    kiwi_str_unescape_quotes((char *) s);
    dxp->ident_s = strdup(s);
    dxp->ident = kiwi_str_encode((char *) s, "dx", USE_MALLOC);
    cfg_string_tok_free(cfg, s);
    jt++;
    
    if (cfg_string_tok(cfg, jt, &s) == false) { *error = E_NOTES; return NULL; }
    kiwi_str_unescape_quotes((char *) s);
    dxp->notes_s = strdup(s);
    if (*s == '\0') {
        dxp->notes = NULL;
    } else {
        dxp->notes = kiwi_str_encode((char *) s, "dx", USE_MALLOC);
    }
    cfg_string_tok_free(cfg, s);
    jt++;
    
    // deprecated
    int timestamp;
    if (cfg_int_tok(cfg, jt, &timestamp)) {
        jt++;
    } else {
        //printf("### DX #%d missing timestamp\n", i);
        //dxp->timestamp = utc_time_since_2018() / 60;
    }
    
    int tag;
    if (cfg_int_tok(cfg, jt, &tag)) {
        jt++;
    } else {
        //printf("### DX #%d missing tag\n", i);
        //dxp->tag = random() % 10000;
    }
    
    //printf("dx-json %d %.2f 0x%x \"%s\" \"%s\"\n", i, dxp->freq, dxp->flags, dxp->ident, dxp->notes);

    int i = 0;
    if (jt != end_tok && JSMN_IS_OBJECT(jt)) {
        jt++;
        while (jt != end_tok && !JSMN_IS_ARRAY(jt)) {
            if (!JSMN_IS_ID(jt)) { *error = E_OPT_ARRAY; return NULL; }
            const char *id;
            if (cfg_string_tok(cfg, jt, &id) == false) { *error = E_OPT_ID; return NULL; }
            jt++;
            
            int num;
            if (cfg_int_tok(cfg, jt, &num) == true) {
                if (strcmp(id, "lo") == 0) {
                    dxp->low_cut = num;
                } else
                
                if (strcmp(id, "hi") == 0) {
                    dxp->high_cut = num;
                } else
                
                if (strcmp(id, "o") == 0) {
                    dxp->offset = num;
                    //printf("dx-json %d offset %s %d\n", i, id, num);
                } else
                
                if (strcmp(id, "s") == 0) {
                    dxp->sig_bw = num;
                    //printf("dx-json %d sig_bw %s %d\n", i, id, num);
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
                        //if (dx_db->db == DB_COMMUNITY) printf("dx-json %d dx_flags: 0x%x %s\n", i, dxp->flags & DX_TYPE, id);
                    }
                }
            } else
            if (cfg_string_tok(cfg, jt, &s) == true) {
                //printf("dx-json %s=<%s>\n", id, s);
                if (strcmp(id, "p") == 0) {
                    kiwi_str_unescape_quotes((char *) s);
                    dxp->params = kiwi_str_encode((char *) s, "dx", USE_MALLOC);
                }
                cfg_string_tok_free(cfg, s);
            }

            cfg_string_tok_free(cfg, id);
            jt++;
            i++;
        }
    }
    
    dxp->flags |= dow;
    if (dxp->time_begin == 0 && dxp->time_end == 0) dxp->time_end = 2400;
    
    return jt;
}

static void dx_reload_finalize(dx_db_t *dx_db, dx_t *_dx_list, int _dx_list_len)
{
    int i;
    
    dx_prep_list(dx_db, true, _dx_list, _dx_list_len, _dx_list_len);

	// switch to new list
	dx_t *prev_dx_list = dx_db->list;
	int prev_dx_list_len = dx_db->actual_len;

	dx_db->list = _dx_list;
	dx_db->actual_len = _dx_list_len;
	
	dx_db->hidden_used = false;
	dx_db->json_up_to_date = true;
	
	// release previous
	if (prev_dx_list) {
        check(dx_db == &dx.dx_db[DB_STORED])
		dx_t *dxp;
		
		for (i = 0, dxp = prev_dx_list; i < prev_dx_list_len; i++, dxp++) {
			// previous allocators better have used malloc(), strdup() et al for these and not kiwi_malloc()
			kiwi_asfree((void *) dxp->ident_s);
			free((void *) dxp->ident);
			kiwi_asfree((void *) dxp->notes_s);
			free((void *) dxp->notes);
			free((void *) dxp->params);
		}
	}
	
	kiwi_free("dx_list", prev_dx_list);
}

// create and switch to new dx_t struct from JSON token list representation

// caller has not read JSON file nor parsed it (do that here on a per-line basis)
static void _dx_reload_file(cfg_t *cfg, int db)
{
    int i;
    FILE *fp;

    lprintf("reading configuration from file %s\n", cfg->filename);
    scallz("_dx_reload_file fopen", (fp = fopen(cfg->filename, "r")));

    #define LBUF 512
    char lbuf[LBUF], *s;
    fgets(lbuf, LBUF, fp);      // skip first line
    
    dx_db_t *dx_db = &dx.dx_db[db];
    dx_db->lines = 1;
    dx_db->json_parse_errors = dx_db->dx_format_errors = 0;
    cfg->json = lbuf;

	jsmn_parser parser;
	check(cfg->tok_size == 0);
	#define NTOK 32
	cfg->tok_size = NTOK;
    cfg->tokens = (jsmntok_t *) kiwi_malloc("dx tokens", sizeof(jsmntok_t) * cfg->tok_size);
    
	int size_i = 0;
	dx_t *_dx_list = NULL;

	dx_t *dxp;
	bool done = false;

    SHA256_CTX ctx;
    sha256_init(&ctx);

    do {
        for (i = 0; !done; i++) {
            if (i >= size_i) {
                _dx_list = (dx_t *) kiwi_table_realloc(cfg->filename, _dx_list, size_i, DX_LIST_ALLOC_CHUNK, sizeof(dx_t));
                size_i = size_i + DX_LIST_ALLOC_CHUNK;
            }

            dxp = &_dx_list[i];
            if ((s = fgets(lbuf, LBUF, fp)) != NULL) {
                dx_db->lines++;
                int slen = strlen(s);
                if (slen > 0) {
                    slen--;     // remove \n
                    if (slen > 0) if (s[slen-1] == ',') slen--;
                    s[slen] = '\0';
                }
                if (slen >= 2 && strncmp(s, "//", 2) == 0) { i--; continue; }   // ignore comments
                cfg->json_buf_size = slen + SPACE_FOR_NULL;
                jsmn_init(&parser);
                int ntok_or_err = jsmn_parse(&parser, s, slen, cfg->tokens, cfg->tok_size, /* yield */ false);
                //printf("DX: #%d ntok_or_err=%d tok_size=%d %d<%s><%s>\n", i, ntok_or_err, cfg->tok_size, slen, s, cfg->json);
                if (ntok_or_err == 0) { i--; continue; }
                if (ntok_or_err < 0) {
                    if (strcmp(s, "]}") != 0) {          // i.e. "]}" on last line
                        const char *err = "(unknown error)";
                        if (ntok_or_err == JSMN_ERROR_INVAL) err = "invalid character inside JSON string"; else
                        if (ntok_or_err == JSMN_ERROR_PART) err = "the string is not a full JSON packet, more bytes expected";
                        lprintf("DX ERROR: line=%d position=%d token=%d %s\n", dx_db->lines, parser.pos, parser.toknext, err);
                        dx_db->json_parse_errors++;
                        lprintf("DX ERROR: %s\n", s);
                        lprintf("DX ERROR: %*s JSON error position\n", parser.pos, "^");
                    }

                    i--;    // don't load labels with errors
                    continue;
                } else {
                    cfg->ntok = ntok_or_err;
                    check(cfg->ntok < NTOK);
                    jsmntok_t *end_tok = &(cfg->tokens[cfg->ntok]);
                    int error;
                    jsmntok_t *jt = dx_reload_element(cfg, dx_db, cfg->tokens, end_tok, dxp, &error);
                    if (jt == NULL) {
                        dx_reload_error(cfg, dx_db->lines, error, true);
                        dx_db->dx_format_errors++;
                        i--;    // don't load labels with errors
                    }
                }
                
                sha256_update(&ctx, (BYTE *) s, slen);
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
    if (dx_db->json_parse_errors) lprintf(", %d JSON parse error%s", dx_db->json_parse_errors, (dx_db->json_parse_errors == 1)? "":"s");
    if (dx_db->dx_format_errors) lprintf(", %d label format error%s", dx_db->dx_format_errors, (dx_db->dx_format_errors == 1)? "":"s");
    lprintf("\n");

    fclose(fp);
    kiwi_free("dx tokens", cfg->tokens); cfg->tokens = NULL;
    
    BYTE hash[SHA256_BLOCK_SIZE];
    sha256_final(&ctx, hash);
    mg_bin2str(dx_db->file_hash, hash, N_DX_FILE_HASH_BYTES);
    dx_db->file_size = (int) kiwi_file_size(cfg->filename);
    lprintf("DX: file = %d,%s,%d\n", i, dx_db->file_hash, dx_db->file_size);

    int _dx_list_len = i;   // NB: doesn't include DX_HIDDEN_SLOT
    dx_db->alloc_size = size_i;
    dx_reload_finalize(dx_db, _dx_list, _dx_list_len);
}

static bool dx_download_file(const char *host, const char *src, const char *dst)
{
    bool restart = false, rm_tmp = true;
    char *cmd, *tmp;
    lprintf("DX: checking %s against %s/%s\n", dst, host, src);
    asprintf(&tmp, "%s.tmp", dst);
    asprintf(&cmd, "wget --timeout=10 --tries=2 --inet4-only -q --no-use-server-timestamps -O %s %s/%s 2>&1", tmp, host, src);
    int status = system(cmd);
    if (status) {
        lprintf("DX: <%s>\n", cmd);
        lprintf("DX: ERROR status=%d\n", status);
    } else
    if (kiwi_file_size(tmp) > 256) {
        char *sum1 = non_blocking_cmd_fmt(NULL, "sum %s", tmp);
        char *sum2 = non_blocking_cmd_fmt(NULL, "sum %s", dst);
        char *sum1_s = kstr_sp(sum1);
        char *sum2_s = kstr_sp(sum2);
        //printf("DX: sum1=<%s> sum2=<%s>\n", sum1_s, sum2_s);
        if (sum1 == NULL || sum2 == NULL || sum1_s == NULL || sum2_s == NULL || strncmp(sum1_s, sum2_s, 11) != 0) {
            //if (sum1_s) lprintf("DX: sum1 <%.*s> %s\n", strlen(sum1_s)-1, sum1_s, tmp);
            //if (sum2_s) lprintf("DX: sum2 <%.*s> %s\n", strlen(sum1_s)-1, sum2_s, dst);
            lprintf("DX: UPDATING %s\n", dst);
            blocking_system("mv %s %s", tmp, dst);
            rm_tmp = false;
            lprintf("DX: new file installed, need RESTART\n");
            restart = true;
        } else {
            lprintf("DX: CURRENT %s\n", dst);
        }
        kstr_free(sum1); kstr_free(sum2);
    }
    if (rm_tmp) {
        blocking_system("rm -f %s", tmp);
    }
    kiwi_asfree(cmd); kiwi_asfree(tmp);
    return restart;
}

void dx_last_community_download(bool capture_time)
{
    if (capture_time) dx.last_community_download = utc_time();
    
    if (admcfg_bool("dx_comm_auto_download", NULL, CFG_OPTIONAL)) {
        snd_send_msg_encoded(SM_RX_CHAN_ALL, false, "MSG", "last_community_download", "Downloads enabled. Last checked: %s",
            var_ctime_static(&dx.last_community_download));
    } else {
        snd_send_msg_encoded(SM_RX_CHAN_ALL, false, "MSG", "last_community_download", "Downloads disabled.");
    }
}

bool dx_community_get(bool download_diff_restart)
{
    bool restart = false;
    char *kiwisdr_com = DNS_lookup_result("dx_community_get", "kiwisdr.com", &net.ips_kiwisdr_com);

    bool download_oneshot = kiwi_file_exists(DX_DOWNLOAD_ONESHOT_FN);
    if (download_oneshot) {
        system("rm -f " DX_DOWNLOAD_ONESHOT_FN);
        printf("DX: dx_community_download ONESHOT\n");
    }

    // attempt to update dx community label database
    if (admcfg_bool("dx_comm_auto_download", NULL, CFG_OPTIONAL) || download_oneshot) {
        restart |= dx_download_file(kiwisdr_com, "dx/dx_community_config.json", DIR_CFG "/dx_community_config.json");
        restart |= dx_download_file(kiwisdr_com, "dx/dx_community.json", DIR_CFG "/dx_community.json");
    }
    dx_last_community_download(true);

    return restart;
}

void dx_label_init()
{
    int i;
    
    for (i = 0; i < DB_N; i++) {
        dx_db_t *dx_db = &dx.dx_db[i];
        dx_db->db = i;
    }
    
	for (i = 0; i <= MAX_RX_CHANS; i++) {       // <= for STREAM_ADMIN use at end
	    dx.rx_db[i] = &dx.dx_db[DB_STORED];
	}

    const char *s;
    if ((s = cfg_array("dx_type", NULL, CFG_OPTIONAL)) != NULL) {
        cfg_array_free(s);
        
        // kiwi.json/dx_type[] exists
        
        if (kiwi_file_exists(DIR_CFG "/dx_config.json")) {
            lprintf("DX config conversion: ERROR dx_config.json exists but also kiwi.json/{dx_type, band_svc, bands}?\n");
            lprintf("DX config conversion: will leave dx_config.json intact, but remove kiwi.json/{dx_type, band_svc, bands}\n");
        } else {
            lprintf("DX config conversion: MOVING kiwi.json/{dx_type, band_svc, bands} => dx_config.json\n");
            system("jq '{dx_type, band_svc, bands}' " DIR_CFG "/kiwi.json >" DIR_CFG "/dx_config.json");
        }
        system("jq 'del(.dx_type) | del(.band_svc) | del(.bands)' " DIR_CFG "/kiwi.json >" DIR_CFG "/kiwi.NEW.json");
        system("mv " DIR_CFG "/kiwi.NEW.json " DIR_CFG "/kiwi.json");
        lprintf("DX config conversion: RESTART Kiwi..\n");
        kiwi_exit(0);
    }

	dxcfg_init();

	TMEAS(u4_t start = timer_ms();)
	TMEAS(printf("DX_RELOAD START\n");)

	if (!dx_init())
		return;

    TMEAS(u4_t split = timer_ms(); printf("DX_RELOAD json file read and json struct %.3f sec\n", TIME_DIFF_MS(split, start));)
        _dx_reload_file(&cfg_dx, DB_STORED);
    TMEAS(u4_t now = timer_ms(); printf("DX_RELOAD DONE json struct -> dx struct %.3f/%.3f sec\n", TIME_DIFF_MS(now, split), TIME_DIFF_MS(now, start));)

	if (!dxcomm_init())
		return;
	
	if (!dxcomm_cfg_init())
		return;
	
    TMEAS(u4_t split = timer_ms(); printf("DX_RELOAD json file read and json struct %.3f sec\n", TIME_DIFF_MS(split, start));)
        _dx_reload_file(&cfg_dxcomm, DB_COMMUNITY);
    TMEAS(u4_t now = timer_ms(); printf("DX_RELOAD DONE json struct -> dx struct %.3f/%.3f sec\n", TIME_DIFF_MS(now, split), TIME_DIFF_MS(now, start));)
}

void dx_eibi_init()
{
    int i, n = 0;
    dx_t *dxp;
    //float f = 0;
    
    // augment EiBi list with additional or decoded info
    for (i = 0, dxp = eibi_db; dxp->freq; i++, dxp++) {
        //if (dxp->freq == f) continue;   // temp: only take first per freq
        n++;
        //f = dxp->freq;
        dxp->ident_s = eibi_ident[dxp->ident_idx];
		dxp->ident = kiwi_str_encode((char *) dxp->ident_s, "dx", USE_MALLOC);
        //asprintf(&(dxp->notes_s), "%04d-%04d %s", dxp->time_begin, dxp->time_end, eibi_notes[dxp->notes_idx]);
        //dxp->notes_s = (char *) eibi_notes[dxp->notes_idx];
        dxp->notes_s = (char *) "";
		dxp->notes = kiwi_str_encode((char *) dxp->notes_s, "dx", USE_MALLOC);
		dxp->params = "";
		dxp->idx = i;
		
		// setup special type-specific passbands
		int type = dxp->flags & DX_TYPE;
		if (type == DX_ALE)  { dxp->low_cut = 600; dxp->high_cut = 2650; } else
		if (type == DX_HFDL) { dxp->low_cut = 300; dxp->high_cut = 2600; } else
		if (type == DX_FAX)  { dxp->low_cut = 800; dxp->high_cut = 3000; } else
		    ;
    }
    printf("DX_UPD dx_eibi_init: %d/%d EiBi entries\n", n, i);

    dx_db_t *dx_db = &dx.dx_db[DB_EiBi];
    dx_db->list = eibi_db;
    dx_db->actual_len = n;
}


// AJAX_DX support

bool _dx_parse_csv_field(int type, char *field, void *val, bool empty_ok, bool *empty)
{
    bool fail = false;
    if (empty != NULL) *empty = false;
    
    if (type == CSV_FLOAT) {
        int sl = kiwi_strnlen(field, 256);
        if (sl == 0) {
            *((float *) val) = 0;               // empty number field becomes zero
            if (empty != NULL) *empty = true;
        } else {
            if (field[0] == '\'') field++;      // allow for leading zero escape e.g. '0123
            char *endp;
            *((float *) val) = strtof(field, &endp);
            if (endp == field) { fail = true; }     // no number was found
        }
    } else

    // don't require that string fields be quoted (except when they contain field delimiter)
    if (type == CSV_STRING || type == CSV_DECODE) {
        char *s = field;
        int sl = kiwi_strnlen(s, 1024);

        if (sl == 0) {
            *((char **) val) = (type == CSV_DECODE)? strdup("\"\"") : (char *) "\"\"";     // empty string field becomes ""
            if (empty != NULL) *empty = true;
            if (!empty_ok) fail = true;
            return fail;
        } else

        // remove the doubled-up double-quotes (if any)
        if (sl > 2) {
            kiwi_str_replace(s, "\"\"", "\"");      // shrinking, so same mem space
            sl = kiwi_strnlen(s, 1024);
        }

        // decode if requested
        if (type == CSV_DECODE) {
            // replace beginning and ending " into something that won't get encoded (restore below)
            bool restore_sf = false, restore_sl = false;
            if (s[0] == '"') { s[0] = 'x'; restore_sf = true; }
            if (s[sl-1] == '"') { s[sl-1] = 'x'; restore_sl = true; }
            s = kiwi_str_decode_selective_inplace(kiwi_str_encode(kiwi_str_decode_inplace(s), "dx csv"), FEWER_ENCODED);
            sl = strlen(s);
            if (restore_sf) s[0] = '"';
            if (restore_sl) s[sl-1] = '"';
        }
        *((char **) val) = s;       // if type == CSV_DECODE caller must kiwi_ifree() val
    } else
        panic("_dx_parse_csv_field");
    
    return fail;
}

void _dx_write_file(void *param)
{
    dx_param_t *dxp = (dx_param_t *) FROM_VOID_PARAM(param);
    int i, n, rc = 0;

    FILE *fp = fopen(DIR_CFG "/upload.dx.json", "w");
    if (fp == NULL) { rc = 3; goto fail; }

    if (dxp->type == TYPE_JSON) {
        n = fwrite(dxp->data, 1, dxp->data_len, fp);
        if (n != dxp->data_len) { rc = 3; goto fail; }
    }

    if (dxp->type == TYPE_CSV) {
        for (i = 0; i < dxp->idx; i++) {
            if (fputs(dxp->s_a[i], fp) < 0) { rc = 3; goto fail; }
        }
    }

fail:
    if (fp != NULL) fclose(fp);
	child_exit(rc);
}


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
#include "mem.h"
#include "misc.h"
#include "str.h"
#include "timer.h"
#include "web.h"
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

#ifdef DEVL_EiBi
    #include "EiBi.h"
#endif

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
	    char *ident = kiwi_str_decode_selective_inplace(strdup(dxp->ident));
	    char *notes = dxp->notes? kiwi_str_decode_selective_inplace(strdup(dxp->notes)) : strdup("");
	    char *params = (dxp->params && *dxp->params)? kiwi_str_decode_selective_inplace(strdup(dxp->params)) : NULL;
	    
	    sb = kstr_asprintf(NULL, "[%.2f, \"%s\", \"%s\", \"%s\", %d, %d",
	        dxp->freq, modu_s[dxp->flags & DX_MODE], ident, notes, dxp->timestamp, dxp->tag);
        free(ident); free(notes);   // not kiwi_ifree() because from strdup()

		u4_t type = dxp->flags & DX_TYPE;
		if (type || dxp->low_cut || dxp->high_cut || dxp->offset || params) {
			const char *delim = ", {";
			const char *type_s = "";
			
			if (type == DX_WL) type_s = "WL"; else
			if (type == DX_SB) type_s = "SB"; else
			if (type == DX_DG) type_s = "DG"; else
			if (type == DX_SE) type_s = "SE"; else
			if (type == DX_XX) type_s = "XX"; else
			if (type == DX_MK) type_s = "MK";
			
			if (type) {
			    sb = kstr_asprintf(sb, "%s\"%s\":1", delim, type_s);
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
	n = sprintf(cp, "]}\n");
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
	if (strcmp(flag, "WL") == 0) dxp->flags |= DX_WL; else
	if (strcmp(flag, "SB") == 0) dxp->flags |= DX_SB; else
	if (strcmp(flag, "DG") == 0) dxp->flags |= DX_DG; else
	if (strcmp(flag, "NoN") == 0) dxp->flags |= DX_SE; else     // deprecated, convert to "SE" on next file write
	if (strcmp(flag, "SE") == 0) dxp->flags |= DX_SE; else
	if (strcmp(flag, "XX") == 0) dxp->flags |= DX_XX; else
	if (strcmp(flag, "PB") == 0) ; else     // deprecated, but here in case any json file still has it
	if (strcmp(flag, "MK") == 0) dxp->flags |= DX_MK; else
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
        if ((dxp->flags & DX_TYPE) == DX_MK) dx.masked_len++;
    }
    kiwi_ifree(dx.masked_list); dx.masked_list = NULL;
    if (dx.masked_len > 0) dx.masked_list = (dx_mask_t *) kiwi_imalloc("dx_prep_list", dx.masked_len * sizeof(dx_mask_t));

    for (i = j = 0, dxp = _dx_list; i < _dx_list_len_new; i++, dxp++) {
        if ((dxp->flags & DX_TYPE) == DX_MK) {
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

    dx.masked_seq++;
}
	
// create and switch to new dx_t struct from JSON token list representation
static void _dx_reload_json(cfg_t *cfg)
{
	const char *s;
	jsmntok_t *end_tok = &(cfg->tokens[cfg->ntok]);
	jsmntok_t *jt = dxcfg_lookup_json("dx");
	check(jt != NULL);
	check(JSMN_IS_ARRAY(jt));
	int _dx_list_len = jt->size;
	jt++;
	
	lprintf("%d dx entries\n", _dx_list_len);
	
	dx_t *_dx_list = (dx_t *) kiwi_malloc("dx_list", (_dx_list_len + DX_HIDDEN_SLOT) * sizeof(dx_t));
	
	dx_t *dxp = _dx_list;
	int i = 0;

	for (; jt != end_tok; dxp++, i++) {
		check(i < _dx_list_len);
		check(JSMN_IS_ARRAY(jt));
		jt++;
		
		double f;
		check(dxcfg_float_json(jt, &f) == true);
		dxp->freq = f;
		jt++;
		
		const char *mode;
		check(dxcfg_string_json(jt, &mode) == true);
		dx_mode(dxp, mode);
		dxcfg_string_free(mode);
		jt++;
		
		check(dxcfg_string_json(jt, &s) == true);
		kiwi_str_unescape_quotes((char *) s);
        dxp->ident_s = strdup(s);
		dxp->ident = kiwi_str_encode((char *) s);
		dxcfg_string_free(s);
		jt++;
		
		check(dxcfg_string_json(jt, &s) == true);
		kiwi_str_unescape_quotes((char *) s);
        dxp->notes_s = strdup(s);
		dxp->notes = kiwi_str_encode((char *) s);
		dxcfg_string_free(s);
		if (*dxp->notes == '\0') {
			dxcfg_string_free(dxp->notes);
			dxp->notes = NULL;
		}
		jt++;
		
		if (dxcfg_int_json(jt, &dxp->timestamp)) {
		    jt++;
		} else {
		    //printf("### DX #%d missing timestamp\n", i);
		    dxp->timestamp = utc_time_since_2018() / 60;
		}
		
		if (dxcfg_int_json(jt, &dxp->tag)) {
		    jt++;
		} else {
		    //printf("### DX #%d missing tag\n", i);
		    dxp->tag = random() % 10000;
		}
		
		//printf("dx.json %d %.2f 0x%x \"%s\" \"%s\"\n", i, dxp->freq, dxp->flags, dxp->ident, dxp->notes);

		if (JSMN_IS_OBJECT(jt)) {
			jt++;
			while (jt != end_tok && !JSMN_IS_ARRAY(jt)) {
				check(JSMN_IS_ID(jt));
				const char *id;
				check(dxcfg_string_json(jt, &id) == true);
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
	}
	
    dx_prep_list(true, _dx_list, _dx_list_len, _dx_list_len);

	// switch to new list
	dx_t *prev_dx_list = dx.stored_list;
	int prev_dx_list_len = dx.stored_len;

	dx.stored_list = _dx_list;
	dx.stored_len = _dx_list_len;
	
	for (i = 0; i < MAX_RX_CHANS; i++) {
        dx_rx_t *drx = &dx.dx_rx[i];
	    drx->db = DB_STORED;
	    drx->cur_list = dx.stored_list;
	    drx->cur_len = dx.stored_len;
	}

	dx.hidden_used = false;
	dx.json_up_to_date = true;
	
	// release previous
	if (prev_dx_list) {
		int i;
		for (i=0, dxp = prev_dx_list; i < prev_dx_list_len; i++, dxp++) {
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

// reload requested, at startup or when file edited by hand
void dx_reload()
{
	cfg_t *cfg = &cfg_dx;
	
	check(DX_MODE <= N_MODE);

	TMEAS(u4_t start = timer_ms();)
	TMEAS(printf("DX_RELOAD START\n");)
	if (!dxcfg_init())
		return;
	
	//dxcfg_walk(NULL, cfg_print_tok, NULL);
	TMEAS(u4_t split = timer_ms(); printf("DX_RELOAD json file read and json struct %.3f sec\n", TIME_DIFF_MS(split, start));)
	_dx_reload_json(cfg);
	TMEAS(u4_t now = timer_ms(); printf("DX_RELOAD DONE json struct -> dx struct %.3f/%.3f sec\n", TIME_DIFF_MS(now, split), TIME_DIFF_MS(now, start));)
}

void dx_eibi_init()
{
#ifdef DEVL_EiBi
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
#endif
}

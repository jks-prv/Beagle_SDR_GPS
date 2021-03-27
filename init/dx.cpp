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

dxlist_t dx;

// create JSON string from dx_t struct representation
void dx_save_as_json()
{
	int i, n;
	cfg_t *cfg = &cfg_dx;
	dx_t *dxp;

	TMEAS(printf("dx_save_as_json: START saving as dx.json, %d entries\n", dx.len);)

	#define DX_JSON_OVERHEAD 128	// gross assumption about size required for everything else
	n = DX_JSON_OVERHEAD;   // room for "{"dx":[]}" etc.
	for (i=0, dxp = dx.list; i < dx.len; i++, dxp++) {
		n += DX_JSON_OVERHEAD;
		n += strlen(dxp->ident);
		if (dxp->notes)
			n += strlen(dxp->notes);
		if (dxp->params)
			n += strlen(dxp->params);
	}

    kiwi_free("dx json buf", cfg->json);
	cfg->json = (char *) kiwi_malloc("dx json buf", n);
	cfg->json_buf_size = n;
	char *cp = cfg->json;
	n = sprintf(cp, "{\"dx\":["); cp += n;

	for (i=0, dxp = dx.list; i < dx.len; i++, dxp++) {
	    assert((cp - cfg->json) < cfg->json_buf_size);
	    
		n = sprintf(cp, "%s[%.2f", i? ",":"", dxp->freq); cp += n;
		n = sprintf(cp, ",\"%s\"", modu_s[dxp->flags & DX_MODE]); cp += n;
		n = sprintf(cp, ",\"%s\",\"%s\"", dxp->ident, dxp->notes? dxp->notes:""); cp += n;
		n = sprintf(cp, ",%d", dxp->timestamp); cp += n;
		n = sprintf(cp, ",%d", dxp->tag); cp += n;

		u4_t type = dxp->flags & DX_TYPE;
		if (type || dxp->low_cut || dxp->high_cut || dxp->offset || (dxp->params && *dxp->params)) {
			const char *delim = ",{";
			const char *type_s = "";
			if (type == DX_WL) type_s = "WL"; else
			if (type == DX_SB) type_s = "SB"; else
			if (type == DX_DG) type_s = "DG"; else
			if (type == DX_SE) type_s = "SE"; else
			if (type == DX_XX) type_s = "XX"; else
			if (type == DX_MK) type_s = "MK";
			if (type) {
			    n = sprintf(cp, "%s\"%s\":1", delim, type_s); cp += n;
			    delim = ",";
			}
			if (dxp->low_cut) {
			    n = sprintf(cp, "%s\"lo\":%d", delim, dxp->low_cut); cp += n;
			    delim = ",";
			}
			if (dxp->high_cut) {
			    n = sprintf(cp, "%s\"hi\":%d", delim, dxp->high_cut); cp += n;
			    delim = ",";
			}
			if (dxp->offset) {
			    n = sprintf(cp, "%s\"o\":%d", delim, dxp->offset); cp += n;
			    delim = ",";
			}
			if (dxp->params && *dxp->params) {
			    n = sprintf(cp, "%s\"p\":\"%s\"", delim, dxp->params); cp += n;
			    //delim = ",";
			}
			*cp++ = '}';
		}
		*cp++ = ']';
		*cp++ = '\n';

		if ((i&31) == 0) NextTask("dx_save_as_json");
	}
	
	n = sprintf(cp, "]}"); cp += n;
    assert((cp - cfg->json) < cfg->json_buf_size);
	TMEAS(printf("dx_save_as_json: dx struct -> json string\n");)
	dxcfg_save_json(cfg->json);
	TMEAS(printf("dx_save_as_json: DONE\n");)
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
        if ((dxp->flags & DX_TYPE) == DX_MK) dx.masked_len++;
    }
    kiwi_ifree(dx.masked_idx); dx.masked_idx = NULL;
    if (dx.masked_len > 0) dx.masked_idx = (int *) kiwi_imalloc("dx_prep_list", dx.masked_len * sizeof(int));
    for (i = j = 0, dxp = _dx_list; i < _dx_list_len_new; i++, dxp++) {
        dxp->idx = i;
        if ((dxp->flags & DX_TYPE) == DX_MK)
            dx.masked_idx[j++] = i;
    }

    for (i = 0; i < dx.masked_len; i++) {
        dxp = &_dx_list[dx.masked_idx[i]];
        int mode = dxp->flags & DX_MODE;
        int masked_f = roundf(dxp->freq * kHz);
        int hbw = mode_hbw[mode];
        int offset = mode_offset[mode];
        dxp->masked_lo = masked_f + offset + (dxp->low_cut? dxp->low_cut : -hbw);
        dxp->masked_hi = masked_f + offset + (dxp->high_cut? dxp->high_cut : hbw);
        //printf("masked %.3f %d-%d %s hbw=%d off=%d lc=%d hc=%d\n",
        //    dxp->freq, dxp->masked_lo, dxp->masked_hi, modu_s[mode], hbw, offset, dxp->low_cut, dxp->high_cut);
    }
    dx.masked_seq++;
}
	
// create and switch to new dx_t struct from JSON token list representation
static void dx_reload_json(cfg_t *cfg)
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
	dx_t *prev_dx_list = dx.list;
	int prev_dx_list_len = dx.len;
	dx.list = _dx_list;
	dx.len = _dx_list_len;
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
	dx_reload_json(cfg);
	TMEAS(u4_t now = timer_ms(); printf("DX_RELOAD DONE json struct -> dx struct %.3f/%.3f sec\n", TIME_DIFF_MS(now, split), TIME_DIFF_MS(now, start));)
}

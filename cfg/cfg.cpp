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
#include "options.h"
#include "mem.h"
#include "misc.h"
#include "timer.h"
#include "web.h"
#include "eeprom.h"
#include "coroutines.h"
#include "jsmn.h"
#include "cfg.h"
#include "utf8.h"

#ifdef USE_SDR
 #include "dx.h"
#endif

#include <sys/types.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>
#include <signal.h>
#include <ctype.h>

#define CFG_PRE         DIR_CFG "/" CFG_PREFIX
#define CFG_FN          CFG_PRE "kiwi.json"
#define ADM_FN          CFG_PRE "admin.json"
#define DX_FN           CFG_PRE "dx.json"
#define DX_CFG_FN       CFG_PRE "dx_config.json"
#define DX_COMM_FN      CFG_PRE "dx_community.json"
#define DX_COMM_CFG_FN  CFG_PRE "dx_community_config.json"

#define SPACE_FOR_CLOSE_BRACE       1
#define SPACE_FOR_POSSIBLE_COMMA    1
#define	SLEN_NOTHING_ELSE           0
#define	SLEN_2QUOTES                2
#define JSON_ID_FIRST_QUOTE         1
#define JSON_ID_QUOTE_COLON         2

#define FL_PANIC    1
static bool _cfg_parse_json(cfg_t *cfg, u4_t flags = 0);

//#define CFG_PRF_SAVE
#ifdef CFG_PRF_SAVE
    #define cfg_printf(fmt, ...) \
        printf(fmt, ## __VA_ARGS__)
#else
    #define cfg_printf(fmt, ...)
#endif

//#define CFG_TEST
#ifdef CFG_TEST

static void cfg_test()
{
	cfg_t cfgx;
	char *buf;
	
	real_printf("\n");
	real_printf("test 4 basic cut/ins cases:\n");
	buf = (char *) "{\"L\": 1, \"foo\": 123, \"R\": 2}";
	json_init_flags(&cfgx, CFG_DEBUG, buf);
	json_set_int(&cfgx, "foo", 9999);
	_cfg_parse_json(&cfgx);

	real_printf("\n");
	buf = (char *) "{\"foo\": 123, \"R\": 2}";
	json_init_flags(&cfgx, CFG_DEBUG, buf);
	json_set_int(&cfgx, "foo", 9999);
	_cfg_parse_json(&cfgx);
	
	real_printf("\n");
	buf = (char *) "{\"foo\": 123}";
	json_init_flags(&cfgx, CFG_DEBUG, buf);
	json_set_int(&cfgx, "foo", 9999);
	_cfg_parse_json(&cfgx);

	real_printf("\n");
	buf = (char *) "{\"L\": 1, \"foo\": 123}";
	json_init_flags(&cfgx, CFG_DEBUG, buf);
	json_set_int(&cfgx, "foo", 9999);
	_cfg_parse_json(&cfgx);

	real_printf("\n");
	real_printf("test 2 new creation cases:\n");
	buf = (char *) "{}";
	json_init_flags(&cfgx, CFG_DEBUG, buf);
	json_set_int(&cfgx, "foo", 9999);
	_cfg_parse_json(&cfgx);

	real_printf("\n");
	buf = (char *) "{\"L\": 1}";
	json_init_flags(&cfgx, CFG_DEBUG, buf);
	json_set_int(&cfgx, "foo", 9999);
	_cfg_parse_json(&cfgx);

	real_printf("\n");
	real_printf("test cut/ins of other types:\n");
	buf = (char *) "{\"L\": 1, \"foo\": 1.234, \"R\": 2}";
	json_init_flags(&cfgx, CFG_DEBUG, buf);
	json_set_float(&cfgx, "foo", 5.678);
	_cfg_parse_json(&cfgx);

	real_printf("\n");
	buf = (char *) "{\"L\": 1, \"foo\": false, \"R\": 2}";
	json_init_flags(&cfgx, CFG_DEBUG, buf);
	json_set_bool(&cfgx, "foo", true);
	_cfg_parse_json(&cfgx);

	real_printf("\n");
	buf = (char *) "{\"L\": 1, \"foo\": \"bar\", \"R\": 2}";
	json_init_flags(&cfgx, CFG_DEBUG, buf);
	json_set_string(&cfgx, "foo", "baz");
	_cfg_parse_json(&cfgx);

	real_printf("\n");
	real_printf("newline cases and space after id:\n");
	buf = (char *) "{\"L\": \"Ld\", \n  \"foo\": \"bar\", \n  \"R\": \"Rd\"}";
	json_init_flags(&cfgx, CFG_DEBUG, buf);
	json_rem_string(&cfgx, "foo");
	_cfg_parse_json(&cfgx);

	real_printf("\n");
	buf = (char *) "{\"foo\": \"bar\",\n  \"R\": \"Rd\"}";
	json_init_flags(&cfgx, CFG_DEBUG, buf);
	json_rem_string(&cfgx, "foo");
	_cfg_parse_json(&cfgx);

	real_printf("\n");
	buf = (char *) "{\"L\": \"Ld\",\n  \"foo\": \"bar\"}";
	json_init_flags(&cfgx, CFG_DEBUG, buf);
	json_rem_string(&cfgx, "foo");
	_cfg_parse_json(&cfgx);

	//_cfg_walk(&cfgx, NULL, cfg_print_tok);
	kiwi_exit(0);
}
#endif

//#define CFG_TEST_YIELD_RACE
#ifdef CFG_TEST_YIELD_RACE

static void _cfgTestTask(void *param) {
    int seq = 0;
    
	while (1) {
	    real_printf(" %d>", seq); fflush(stdout);
	    admcfg_bool("always_acq_gps", NULL, CFG_REQUIRED);
	    real_printf("<%d ", seq); fflush(stdout);
	    seq++;
	    TaskSleepMsec(100);
	}
}
#endif


////////////////////////////////
// init
////////////////////////////////

int serial_number;

// only called once from main()
void cfg_reload()
{
    //printf_highlight(1, "_cfg_");
	cfg_init();
	admcfg_init();

    #ifdef CFG_TEST
        cfg_test();
    #endif
    
    model_e model;
    int serno = eeprom_check(&model);
    if ((serial_number = cfg_int("serial_number", NULL, CFG_OPTIONAL)) > 0) {
        lprintf("serial number override from configuration: %d\n", serial_number);
    } else {
        if ((serial_number = serno) <= 0) {
            lprintf("can't read serial number from EEPROM and no configuration override\n");
            serial_number = 0;
        } else {
            lprintf("serial number from EEPROM: %d\n", serial_number);
        }
    }

    bool err;
    kiwi.model = (model_e) admcfg_int("kiwi_model", &err, CFG_OPTIONAL);
    if (!err) {
        lprintf("Kiwi model override from configuration: KiwiSDR %d\n", kiwi.model);
    } else {
        if (serno <= 0 || model <= 0) {
            lprintf("can't read Kiwi model from EEPROM and no configuration override\n");
            lprintf("assuming model: KiwiSDR 2\n");
            kiwi.model = KiwiSDR_2;
        } else {
            kiwi.model = model;
            lprintf("model: KiwiSDR %d\n", model);
        }
    }

    #ifdef USE_SDR
        dx_label_init();
    #endif

    #ifdef CFG_TEST_YIELD_RACE
        CreateTaskF(_cfgTestTask, 0, GPS_ACQ_PRIORITY, CTF_NO_PRIO_INV);
    #endif
}

cfg_t cfg_cfg, cfg_adm, cfg_dx, cfg_dxcfg, cfg_dxcomm, cfg_dxcomm_cfg;
	
char *_cfg_get_json(cfg_t *cfg, int *size)
{
	if (!cfg->init) return NULL;
	
	if (size) *size = cfg->json_buf_size;
	return cfg->json;
}

char *_cfg_realloc_json(cfg_t *cfg, int new_size, u4_t flags);
static bool _cfg_load_json(cfg_t *cfg);

bool _cfg_init(cfg_t *cfg, int flags, char *buf, const char *id)
{
    bool rv = true;
    bool do_rtn = false;
    
	if (buf != NULL) {
		memset(cfg, 0, sizeof(cfg_t));
        cfg->flags = flags;
		cfg->filename = (char *) id;
		_cfg_realloc_json(cfg, strlen(buf) + SPACE_FOR_NULL, CFG_NONE);
		strcpy(cfg->json, buf);
		if (_cfg_parse_json(cfg) == false) {
			return false;
		}
		cfg->init = true;
		do_rtn = true;
	} else
	
	if (cfg != NULL && buf == NULL && cfg->filename != NULL && (flags & CFG_IS_JSON)) {
        cfg->init = false;
	} else
	
	if (cfg == &cfg_cfg) {
		cfg->filename = CFG_FN;
	} else

	if (cfg == &cfg_adm) {
		cfg->filename = ADM_FN;
	} else

	if (cfg == &cfg_dx) {
		cfg->filename = DX_FN;
		flags |= CFG_NO_PARSE | CFG_INT_BASE10 | CFG_YIELD | CFG_NO_INTEG;
    } else

	if (cfg == &cfg_dxcfg) {
		cfg->filename = DX_CFG_FN;
		flags |= CFG_NO_PARSE | CFG_INT_BASE10;
    } else

	if (cfg == &cfg_dxcomm) {
        cfg->filename = DX_COMM_FN;
		flags |= CFG_NO_PARSE | CFG_INT_BASE10;
    } else

	if (cfg == &cfg_dxcomm_cfg) {
        cfg->filename = DX_COMM_CFG_FN;
		flags |= CFG_NO_PARSE | CFG_INT_BASE10;
	} else {
		panic("cfg_init cfg");
	}
	
    cfg->basename = strrchr(cfg->filename, '/');
    if (!cfg->basename) cfg->basename = cfg->filename; else cfg->basename++;
	
	snprintf(cfg->id_tokens, CFG_ID_N, "tokens:%s", cfg->filename);
	snprintf(cfg->id_json,   CFG_ID_N, "json_buf:%s", cfg->filename);
	if (do_rtn) return true;
	
    bool parse = !(flags & CFG_NO_PARSE);
	if (!cfg->init) {
        cfg->flags = flags;
        
        if (parse) {
            cfg->init_load = true;
            if (_cfg_load_json(cfg) == false) {
                if (cfg->flags & CFG_IS_JSON) {
                    lprintf("JSON parse failed: %s\n", cfg->filename);
                    return false;
                }
                panic("cfg_init json");
            }
        } else
        
        if (flags & CFG_LOAD_ONLY) {
            lprintf("reading configuration from file %s\n", cfg->filename);
            if (_cfg_load_json(cfg) == false) {
                lprintf("configuration file %s doesn't exist\n", cfg->filename);
                rv = false;
            }
        }
        
		cfg->init = true;
	    cfg->init_load = false;
	}
	
    if (parse)
        lprintf("reading configuration from file %s: %d tokens (%s bytes)\n",
            cfg->filename, cfg->ntok, toUnits(cfg->json_buf_size, 0));

	if (cfg == &cfg_cfg) {
		struct stat st;
		const char *adm_fn = ADM_FN;
		
		if (!kiwi_file_exists(adm_fn)) {
			system("echo -n \"{}\" >" ADM_FN);
			admcfg_init();
			lprintf("#### transitioning to admin.json file ####\n");
			cfg_adm_transition();
		}
	}
	
	return rv;
}

void _cfg_release(cfg_t *cfg)
{
	if (!cfg->init || !(cfg->flags & CFG_IS_JSON)) return;
	if (cfg->tokens) {
	    //printf("cfg_release: tokens %p\n", cfg->tokens);
	    kiwi_free(cfg->id_tokens, cfg->tokens);
	}
	cfg->tokens = NULL;
	if (cfg->json) {
	    //printf("cfg_release: json %p\n", cfg->json);
	    kiwi_free(cfg->id_json, cfg->json);
	}
	cfg->json = NULL;
}

bool cfg_gdb_break(bool val)
{
    return val;
}


////////////////////////////////
// lookup
////////////////////////////////

#define LVL1_MATCH  +1
#define LVL2_MATCH  +2

// search for match with specified id, with optional level matching/rejection
static bool _cfg_lookup_json_cb(cfg_t *cfg, void *param1, void *param2, jsmntok_t *jt, int seq, int hit, int lvl, int rem, void **rval)
{
	char *id_match = (char *) FROM_VOID_PARAM(param1);
	int id_match_len = strlen(id_match);
	int lvl_match = (int) FROM_VOID_PARAM(param2);
	
	
	if (!JSMN_IS_ID(jt)) return false;
	//cfg_print_tok(cfg, param1, param2, jt, seq, hit, lvl, rem, rval);
	char *s = &cfg->json[jt->start];
	int n = jt->end - jt->start;
	
	bool rv = true;
	if (lvl_match > 0 && lvl != lvl_match) rv = false;      // lvl_match > 0 means match lvl
	else
	if (lvl_match < 0 && lvl == lvl_match) rv = false;      // lvl_match < 0 means reject lvl
	else
	if (n != id_match_len || strncmp(s, id_match, n) != 0) rv = false;
	if (rv == true && rval) *rval = jt+1;
	//printf("_cfg_lookup_json_cb TEST id=\"%.*s\" want_id=\"%s\" have_id=%s lvl_match=%d lvl=%d\n", n, s, id_match, lvl_match, lvl, rv? "T":"F");
	return rv;
}

jsmntok_t *_cfg_lookup_json(cfg_t *cfg, const char *id, cfg_lookup_e option)
{
	if (!cfg->init) return NULL;
    assert(cfg->flags & CFG_PARSE_VALID);
	
	int i;
	
	jsmntok_t *jt;
	//printf("_cfg_lookup_json: key=\"%s\" %d\n", id, strlen(id));
	char *dot = (char *) strchr(id, '.');
	char *dotdot = dot? (char *) strchr(dot+1, '.') : NULL;

	// handle two levels of id scope, i.e. id1.id2, but ignore more than that
	if (dot && !dotdot && option != CFG_OPT_NO_DOT) {
		char *id1_m = NULL, *id2_m = NULL;
		i = sscanf(id, "%m[^.].%ms", &id1_m, &id2_m);
		//printf("_cfg_lookup_json 2-scope: key=\"%s\" n=%d id1=\"%s\" id2=\"%s\"\n", id, i, id1_m, id2_m);
		if (i != 2) {
            kiwi_asfree(id1_m); kiwi_asfree(id2_m);
		    return NULL;
		}
		if (strchr(id2_m, '.') != NULL) panic("_cfg_lookup_json: more than two levels of scope in id");
		
		// lookup just the id1 of a two-scope id
		if (option == CFG_OPT_ID1) {
		    //jt = _cfg_lookup_id(cfg, id1_m);
            jt = (jsmntok_t *) _cfg_walk(cfg, NULL, _cfg_lookup_json_cb, TO_VOID_PARAM(id1_m), TO_VOID_PARAM(LVL1_MATCH));
            kiwi_asfree(id1_m); kiwi_asfree(id2_m);
		    return jt;
		} else {
            // run callback for all second scope objects of id1
            //printf("_cfg_lookup_json WALK id1=%s id2=%s => ", id1_m, id2_m);
            void *rtn_rval = _cfg_walk(cfg, id1_m, _cfg_lookup_json_cb, TO_VOID_PARAM(id2_m), TO_VOID_PARAM(LVL2_MATCH));
            
            // if id1 exists but id2 is missing then return this fact
            if (rtn_rval == NULL && _cfg_walk(cfg, NULL, _cfg_lookup_json_cb, TO_VOID_PARAM(id1_m), TO_VOID_PARAM(LVL1_MATCH)) != NULL) {
                kiwi_asfree(id1_m); kiwi_asfree(id2_m);
                return CFG_LOOKUP_LVL1;
            }
            
            //printf("_cfg_lookup_json 2-scope: rtn_rval=%p id=%s\n", rtn_rval, id);
            kiwi_asfree(id1_m); kiwi_asfree(id2_m);
            return (jsmntok_t *) rtn_rval;
        }
        assert("not reached");
		
	} else {
		//return _cfg_lookup_id(cfg, id);
        //printf("_cfg_lookup_json ONLY_LVL1 id=%s\n", id);
        void *rtn_rval = _cfg_walk(cfg, NULL, _cfg_lookup_json_cb, TO_VOID_PARAM(id), TO_VOID_PARAM(LVL1_MATCH));
        return (jsmntok_t *) rtn_rval;
	}

	return NULL;
}

bool _cfg_type_json(cfg_t *cfg, jsmntype_t jt_type, jsmntok_t *jt, const char **str)
{
	if (!cfg->init) return false;
	
	assert(jt != NULL);
	char *s = &cfg->json[jt->start];
	if (jt->type == jt_type) {
		int n = jt->end - jt->start;
		*str = (const char *) kiwi_imalloc("_cfg_type_json", n + SPACE_FOR_NULL);
		mg_url_decode((const char *) s, n, (char *) *str, n + SPACE_FOR_NULL, 0);
		return true;
	} else {
		return false;
	}
}

void _cfg_free(cfg_t *cfg, const char *str)
{
	if (!cfg->init) return;
	if (str != NULL) kiwi_ifree((char *) str, "_cfg_free");
}


////////////////////////////////
// edit
////////////////////////////////

// NB: Editing of JSON occurs on the source representation and not the parsed binary.
// And only for primitive, named object elements.
// This is not yet general purpose (i.e. as good as Javascript)

static int _cfg_cut(cfg_t *cfg, jsmntok_t *jt, int skip)
{
	char *s, *e;
	jsmntok_t *key_jt = jt-1;
	assert(JSMN_IS_ID(key_jt));
	int key_size = key_jt->end - key_jt->start;		// remember (end - start) doesn't include first quote
	int val_size = jt->end - jt->start;		// does NOT include the two quotes around the string value

	// L,ccc,R => L,R
	// {ccc,R => {R
	// {ccc} => {}
	// L,ccc} => L}
	if (cfg->flags & CFG_DEBUG) real_printf("PRE-CUT <<%s>>\n", cfg->json);
	s = &cfg->json[key_jt->start - JSON_ID_FIRST_QUOTE - 1];
	while (*s == ' ' || *s == '\t' || *s == '\n')
		s--;
	check(*s == '{' || *s == ',');
	s++;
	e = &cfg->json[key_jt->start + key_size + JSON_ID_QUOTE_COLON];
	while (*e == ' ' || *e == '\t') e++;    // whitespace between ':' and '"'
	e += val_size + skip;
	// "id": "val",
	// s          e     at this point

    while (*e == ',' || *e == ' ' || *e == '\n') e++;

	// e.g. xxx,ccc} -> xxx,} -> xxx}
	if (*(s-1) == ',' && *e == '}')
		s--;

	kiwi_overlap_strcpy(s, e);
	if (cfg->flags & CFG_DEBUG) real_printf("POST-CUT %d <<%s>>\n", s - cfg->json, cfg->json);
	return s - cfg->json;
}

static void _cfg_ins(cfg_t *cfg, int pos, char *val)
{
	int i;
	int slen = strlen(cfg->json);
	int vlen = strlen(val);
	char *r = &cfg->json[pos];
	int rlen = strlen(r) + SPACE_FOR_NULL;
	char *s = &cfg->json[slen];
	char *d = &cfg->json[slen+vlen];
	bool Lcomma = false, Rcomma = false;

	// L,R => L,ccc,R	ccc,
	//   ^r
	// {R => {ccc,R		ccc,
	//  ^r
	// {} => {ccc}		ccc
	//  ^r
	// L} => L,ccc}		,ccc
	//  ^r
	if (cfg->flags & CFG_DEBUG) real_printf("PRE-INS <<%s>>\n", cfg->json);

	if (*r == '}' && *(r-1) == '{') {
		;
	} else
	if (*r == '}' && *(r-1) != '{') {
		r++; d++;
		Lcomma = true;
	} else
	{
		d++;
		Rcomma = true;
	}

	for (i=0; i < rlen; i++) {
		*d-- = *s--;
	}
	if (Rcomma) *d-- = ',';
	strncpy(r, val, vlen);
	if (Lcomma) *(r-1) = ',';

	if (cfg->flags & CFG_DEBUG) real_printf("POST-INS <<%s>>\n", cfg->json);
}


////////////////////////////////
// int
////////////////////////////////

bool _cfg_int_json(cfg_t *cfg, jsmntok_t *jt, int *num)
{
	assert(jt != NULL);
	char *s = &cfg->json[jt->start];
	if (JSMN_IS_PRIMITIVE(jt) && (*s == '-' || isdigit(*s))) {
	    if (cfg->flags & CFG_INT_BASE10)
		    *num = strtol(s, 0, 10);
		else
		    *num = strtol(s, 0, 0);
		return true;
	} else {
		return false;
	}
}

int _cfg_int(cfg_t *cfg, const char *name, bool *error, u4_t flags)
{
	int num = 0;
	bool err = false;

	//jsmntok_t *jt = _cfg_lookup_json(cfg, name, CFG_OPT_NONE);
	jsmntok_t *jt = _cfg_lookup_json(cfg, name, (flags & CFG_NO_DOT)? CFG_OPT_NO_DOT : CFG_OPT_NONE);
	if (!jt || jt == CFG_LOOKUP_LVL1 || _cfg_int_json(cfg, jt, &num) == false) {
		err = true;
	}
	if (error) *error = err;
	if (err) {
		if (!(flags & CFG_REQUIRED)) return 0;
		lprintf("%s: required parameter not found: %s\n", cfg->filename, name);
		panic("cfg_int");
	}
	
	if (flags & CFG_PRINT) lprintf("CFG read %s: %s = %d\n", cfg->filename, name, num);
	return num;
}

int _cfg_set_int(cfg_t *cfg, const char *name, int val, u4_t flags, int pos)
{
	int slen;
	char *s;
	char *id2 = strchr((char *) name, '.') + 1;
	//jsmntok_t *jt = _cfg_lookup_json(cfg, name, CFG_OPT_NONE);
	jsmntok_t *jt = _cfg_lookup_json(cfg, name, (flags & CFG_NO_DOT)? CFG_OPT_NO_DOT : CFG_OPT_NONE);

	if (flags & CFG_REMOVE) {
		if (!jt || jt == CFG_LOOKUP_LVL1) {
			lprintf("%s: cfg_set_int(CFG_REMOVE) a parameter that doesn't exist: %s\n", cfg->filename, name);
			return 0;
		}
		
		s = &cfg->json[jt->start];
		
		if (isdigit(*s) || *s == '-') {
            // ,"id":int or {"id":int
            //       ^start
            assert(JSMN_IS_PRIMITIVE(jt));
		    pos = _cfg_cut(cfg, jt, SLEN_NOTHING_ELSE);
		} else
		if (strncmp(s, "null", 4) == 0) {
            // ,"id":null or {"id":null
            //       ^start
            assert(JSMN_IS_PRIMITIVE(jt));
		    pos = _cfg_cut(cfg, jt, SLEN_NOTHING_ELSE);
        } else {
			lprintf("%s: cfg_set_int(CFG_REMOVE) unexpected value: %s\n", cfg->filename, name);
            panic("cfg");
        }
	} else
	if (flags & (CFG_SET | CFG_CHANGE)) {
		if (!jt || jt == CFG_LOOKUP_LVL1) {
			char *int_sval;
			char *id = (jt == CFG_LOOKUP_LVL1)? id2 : (char *) name;
            asprintf(&int_sval, "\"%s\":%d", id, val);
			slen = strlen(int_sval) + SPACE_FOR_POSSIBLE_COMMA;
			assert(cfg->json_buf_size);
			_cfg_realloc_json(cfg, cfg->json_buf_size + slen + SPACE_FOR_NULL, CFG_COPY);
			
			// if creating (not changing) put at end of JSON object
			// unless level one id was found in which case put as first object element
			if ((flags & CFG_CHANGE) == 0) {
			    if (jt == CFG_LOOKUP_LVL1) {
			        jt = _cfg_lookup_json(cfg, name, CFG_OPT_ID1);
			        assert(jt);
			        pos = jt->start + 1;      // jt->start points to '{' of "id1":{...
			    } else {
				    pos = strlen(cfg->json) - SPACE_FOR_CLOSE_BRACE;
				}
			}
			_cfg_ins(cfg, pos, int_sval);
			
			kiwi_asfree(int_sval);
		} else {
			pos = _cfg_set_int(cfg, name, 0, CFG_REMOVE, 0);
			_cfg_set_int(cfg, name, val, CFG_CHANGE, pos);
		}
	} else {
	    panic("_cfg_set_int");
	}

    assert((cfg->flags & CFG_NO_UPDATE) == 0);
    if (flags & CFG_SAVE) {
        cfg_printf("_cfg_set_int %s=%d SAVE\n", name, val);
        _cfg_save_json(cfg, cfg->json);
    } else
	    _cfg_parse_json(cfg, FL_PANIC);	// must re-parse
	return pos;
}

int _cfg_default_int(cfg_t *cfg, const char *name, int val, bool *error_p)
{
    bool error;
	int existing = _cfg_int(cfg, name, &error, CFG_OPTIONAL);
	if (error) {
		_cfg_set_int(cfg, name, val, CFG_SET, 0);
		existing = val;
		//printf("_cfg_default_int: %s = %d\n", name, val);
	}
	if (error_p) *error_p = *error_p | cfg_gdb_break(error);
	return existing;
}

int _cfg_update_int(cfg_t *cfg, const char *name, int val, bool *changed)
{
    bool modified = false;
    int existing = _cfg_default_int(cfg, name, val, &modified);
    if (existing != val) {
        _cfg_set_int(cfg, name, val, CFG_SET, 0);
        existing = val;
        modified = true;
    }
    if (modified && changed != NULL) *changed = true;
    return existing;
}


////////////////////////////////
// float
////////////////////////////////

bool _cfg_float_json(cfg_t *cfg, jsmntok_t *jt, double *num)
{
	assert(jt != NULL);
	char *s = &cfg->json[jt->start];
	if (JSMN_IS_PRIMITIVE(jt) && (*s == '-' || isdigit(*s) || *s == '.')) {
		*num = strtod(s, NULL);
		return true;
	} else {
		return false;
	}
}

double _cfg_float(cfg_t *cfg, const char *name, bool *error, u4_t flags)
{
	double num = 0;
	bool err = false;

	jsmntok_t *jt = _cfg_lookup_json(cfg, name, CFG_OPT_NONE);
	if (!jt || jt == CFG_LOOKUP_LVL1 || _cfg_float_json(cfg, jt, &num) == false) {
		err = true;
	}
	if (error) *error = err;
	if (err) {
		if (!(flags & CFG_REQUIRED)) return 0;
		lprintf("%s: required parameter not found: %s\n", cfg->filename, name);
		panic("cfg_float");
	}
	
	if (flags & CFG_PRINT) lprintf("CFG read %s: %s = %f\n", cfg->filename, name, num);
	return num;
}

int _cfg_set_float(cfg_t *cfg, const char *name, double val, u4_t flags, int pos)
{
	int slen;
	char *s;
	char *id2 = strchr((char *) name, '.') + 1;
	jsmntok_t *jt = _cfg_lookup_json(cfg, name, CFG_OPT_NONE);

	if (flags & CFG_REMOVE) {
		if (!jt || jt == CFG_LOOKUP_LVL1) {
			lprintf("%s: cfg_set_float(CFG_REMOVE) a parameter that doesn't exist: %s\n", cfg->filename, name);
			return 0;
		}
		
		s = &cfg->json[jt->start];

		if (isdigit(*s) || *s == '-' || *s == '.') {
            // ,"id":float or {"id":float
            //       ^start
            assert(JSMN_IS_PRIMITIVE(jt));
		    pos = _cfg_cut(cfg, jt, SLEN_NOTHING_ELSE);
		} else
		if (strncmp(s, "null", 4) == 0) {
            // ,"id":null or {"id":null
            //       ^start
            assert(JSMN_IS_PRIMITIVE(jt));
		    pos = _cfg_cut(cfg, jt, SLEN_NOTHING_ELSE);
        } else {
			lprintf("%s: cfg_set_float(CFG_REMOVE) unexpected value: %s\n", cfg->filename, name);
            panic("cfg");
        }
	} else
	if (flags & (CFG_SET | CFG_CHANGE)) {
		if (!jt || jt == CFG_LOOKUP_LVL1) {
			char *float_sval;
			char *id = (jt == CFG_LOOKUP_LVL1)? id2 : (char *) name;
			asprintf(&float_sval, "\"%s\":%g", id, val);
			slen = strlen(float_sval) + SPACE_FOR_POSSIBLE_COMMA;
			assert(cfg->json_buf_size);
			_cfg_realloc_json(cfg, cfg->json_buf_size + slen + SPACE_FOR_NULL, CFG_COPY);
			
			// if creating (not changing) put at end of JSON object
			// unless level one id was found in which case put as first object element
			if ((flags & CFG_CHANGE) == 0) {
			    if (jt == CFG_LOOKUP_LVL1) {
			        jt = _cfg_lookup_json(cfg, name, CFG_OPT_ID1);
			        assert(jt);
			        pos = jt->start + 1;      // jt->start points to '{' of "id1":{...
			    } else {
				    pos = strlen(cfg->json) - SPACE_FOR_CLOSE_BRACE;
				}
			}
			_cfg_ins(cfg, pos, float_sval);

			kiwi_asfree(float_sval);
		} else {
			pos = _cfg_set_float(cfg, name, 0, CFG_REMOVE, 0);
			_cfg_set_float(cfg, name, val, CFG_CHANGE, pos);
		}
	} else {
	    panic("_cfg_set_float");
	}
	
    assert((cfg->flags & CFG_NO_UPDATE) == 0);
    if (flags & CFG_SAVE) {
        cfg_printf("_cfg_set_float %s=%f SAVE\n", name, val);
        _cfg_save_json(cfg, cfg->json);
    } else
	    _cfg_parse_json(cfg, FL_PANIC);	// must re-parse
	return pos;
}

double _cfg_default_float(cfg_t *cfg, const char *name, double val, bool *error_p)
{
    bool error;
	double existing = _cfg_float(cfg, name, &error, CFG_OPTIONAL);
	if (error) {
		_cfg_set_float(cfg, name, val, CFG_SET, 0);
		existing = val;
		//printf("_cfg_default_float: %s = %g\n", name, val);
	}
	if (error_p) *error_p = *error_p | cfg_gdb_break(error);
	return existing;
}


////////////////////////////////
// bool
////////////////////////////////

bool _cfg_bool_json(cfg_t *cfg, jsmntok_t *jt, int *num)
{
	assert(jt != NULL);
	char *s = &cfg->json[jt->start];
	if (JSMN_IS_PRIMITIVE(jt) && (*s == 't' || *s == 'f')) {
		*num = (*s == 't')? 1:0;
		return true;
	} else {
		return false;
	}
}

int _cfg_bool(cfg_t *cfg, const char *name, bool *error, u4_t flags)
{
	int num = 0;
	bool err = false;

	jsmntok_t *jt = _cfg_lookup_json(cfg, name, CFG_OPT_NONE);
	if (!jt || jt == CFG_LOOKUP_LVL1 || _cfg_bool_json(cfg, jt, &num) == false) {
		err = true;
	}
	if (error) *error = err;
	if (err) {
		if (!(flags & CFG_REQUIRED)) return NOT_FOUND;
		lprintf("%s: required parameter not found: %s\n", cfg->filename, name);
		panic("cfg_bool");
	}

	num = num? true : false;
	if (flags & CFG_PRINT) lprintf("CFG read %s: %s = %s\n", cfg->filename, name, num? "true":"false");
	return num;
}

int _cfg_set_bool(cfg_t *cfg, const char *name, u4_t val, u4_t flags, int pos)
{
	int slen;
	char *s;
	char *id2 = strchr((char *) name, '.') + 1;
	//jsmntok_t *jt = _cfg_lookup_json(cfg, name, CFG_OPT_NONE);
	jsmntok_t *jt = _cfg_lookup_json(cfg, name, (flags & CFG_NO_DOT)? CFG_OPT_NO_DOT : CFG_OPT_NONE);
	
	if (flags & CFG_REMOVE) {
		if (!jt || jt == CFG_LOOKUP_LVL1) {
			lprintf("%s: cfg_set_bool(CFG_REMOVE) a parameter that doesn't exist: %s\n", cfg->filename, name);
			return 0;
		}

		s = &cfg->json[jt->start];

		if (*s == 't' || *s == 'f') {
            // ,"id":t/f or {"id":t/f
            //       ^start
            assert(JSMN_IS_PRIMITIVE(jt));
		    pos = _cfg_cut(cfg, jt, SLEN_NOTHING_ELSE);
		} else
		if (strncmp(s, "null", 4) == 0) {
            // ,"id":null or {"id":null
            //       ^start
            assert(JSMN_IS_PRIMITIVE(jt));
		    pos = _cfg_cut(cfg, jt, SLEN_NOTHING_ELSE);
        } else {
			lprintf("%s: cfg_set_bool(CFG_REMOVE) unexpected value: %s\n", cfg->filename, name);
            panic("cfg");
        }
	} else
	if (flags & (CFG_SET | CFG_CHANGE)) {
		bool bool_val = val? true : false;
		if (!jt || jt == CFG_LOOKUP_LVL1) {
			char *bool_sval;
			char *id = (jt == CFG_LOOKUP_LVL1)? id2 : (char *) name;
			asprintf(&bool_sval, "\"%s\":%s", id, bool_val? "true" : "false");
			slen = strlen(bool_sval) + SPACE_FOR_POSSIBLE_COMMA;
			assert(cfg->json_buf_size);
			_cfg_realloc_json(cfg, cfg->json_buf_size + slen + SPACE_FOR_NULL, CFG_COPY);
			
			// if creating (not changing) put at end of JSON object
			// unless level one id was found in which case put as first object element
			if ((flags & CFG_CHANGE) == 0) {
			    if (jt == CFG_LOOKUP_LVL1) {
			        jt = _cfg_lookup_json(cfg, name, CFG_OPT_ID1);
			        assert(jt);
			        pos = jt->start + 1;      // jt->start points to '{' of "id1":{...
			    } else {
				    pos = strlen(cfg->json) - SPACE_FOR_CLOSE_BRACE;
				}
			}
			_cfg_ins(cfg, pos, bool_sval);
			
			kiwi_asfree(bool_sval);
		} else {
			pos = _cfg_set_bool(cfg, name, 0, CFG_REMOVE, 0);
			_cfg_set_bool(cfg, name, val, CFG_CHANGE, pos);
		}
	} else {
	    panic("_cfg_set_bool");
	}
	
    assert((cfg->flags & CFG_NO_UPDATE) == 0);
    if (flags & CFG_SAVE) {
        cfg_printf("_cfg_set_bool %s=%d SAVE\n", name, val);
        _cfg_save_json(cfg, cfg->json);
    } else
	    _cfg_parse_json(cfg, FL_PANIC);	// must re-parse
	return pos;
}

bool _cfg_default_bool(cfg_t *cfg, const char *name, u4_t val, bool *error_p)
{
    bool error;
	bool existing = _cfg_bool(cfg, name, &error, CFG_OPTIONAL);
	if (error) {
		_cfg_set_bool(cfg, name, val, CFG_SET, 0);
		existing = val;
		//printf("_cfg_default_bool: %s = %s\n", name, val? "true" : "false");
	}
	if (error_p) *error_p = *error_p | cfg_gdb_break(error);
	return existing;
}


////////////////////////////////
// string
////////////////////////////////

static char *_cfg_string_check_utf8(cfg_t *cfg, const char *name, const char *val, bool *error = NULL)
{
    bool err = false;
    char *uc = strdup(val);
        void *invalid_cp = utf8valid(uc);
        if (cfg == &cfg_cfg && invalid_cp) {
            printf("================================================================================\n");
            lprintf("NOT VALID UTF-8: pos=%d %s=<%s>\n", (char *) invalid_cp - uc, name, val);
            utf8makevalid(uc, '?');
            char *uc2 = kiwi_str_encode(uc);
                kiwi_str_decode_selective_inplace(uc2);
            kiwi_ifree(uc2, "cfg utf8");
            err = true;
        }
    if (error) *error = err;
    return uc;      // caller must kiwi_asfree()
}

const char *_cfg_string(cfg_t *cfg, const char *name, bool *error, u4_t flags)
{
	const char *str = NULL;
	bool err = false;

	jsmntok_t *jt = _cfg_lookup_json(cfg, name, CFG_OPT_NONE);
	if (!jt || jt == CFG_LOOKUP_LVL1 || _cfg_type_json(cfg, JSMN_STRING, jt, &str) == false) {
		err = true;
	}
	if (error) *error = err;
	if (err) {
		if (!(flags & CFG_REQUIRED)) return NULL;
		lprintf("%s: required parameter not found: %s\n", cfg->filename, name);
		panic("cfg_string");
	}

	if (flags & CFG_PRINT) lprintf("CFG read %s: %s = \"%s\"\n", cfg->filename, name, str);
	return str;
}

int _cfg_set_string(cfg_t *cfg, const char *name, const char *val, u4_t flags, int pos)
{
	int slen;
	char *s;
	char *id2 = strchr((char *) name, '.') + 1;
	jsmntok_t *jt = _cfg_lookup_json(cfg, name, CFG_OPT_NONE);
	
	if (flags & CFG_REMOVE) {
		if (!jt || jt == CFG_LOOKUP_LVL1) {
			lprintf("%s: cfg_set_string(CFG_REMOVE) a parameter that doesn't exist: %s\n", cfg->filename, name);
			return 0;
		}
		
		s = &cfg->json[jt->start];

		if (*(s-1) == '\"') {
            // ,"id":"string" or {"id":"string"
            //        ^start (NB: NOT first double quote)
            assert(JSMN_IS_STRING(jt));
		    pos = _cfg_cut(cfg, jt, SLEN_2QUOTES);
		} else
		if (strncmp(s, "null", 4) == 0) {
            // ,"id":null or {"id":null
            //       ^start
            // DANGER: might actually be:
            //      ,"id": null     (note space before "null")
            // This is because we're now pretty-printing JSON output created by javascript.
            // So CANNOT strncmp for ":null" -- must only be for "null" instead.
            assert(JSMN_IS_PRIMITIVE(jt));
		    pos = _cfg_cut(cfg, jt, SLEN_NOTHING_ELSE);
        } else {
			lprintf("%s: cfg_set_string(CFG_REMOVE) unexpected value: %s\n", cfg->filename, name);
            panic("cfg");
        }
	} else
	if (flags & (CFG_SET | CFG_CHANGE)) {
		if (!jt || jt == CFG_LOOKUP_LVL1) {
			if (val == NULL) val = (char *) "null";
			char *str_sval;
			char *id = (jt == CFG_LOOKUP_LVL1)? id2 : (char *) name;
			asprintf(&str_sval, "\"%s\":\"%s\"", id, val);
			slen = strlen(str_sval) + SPACE_FOR_POSSIBLE_COMMA;
			assert(cfg->json_buf_size);
			_cfg_realloc_json(cfg, cfg->json_buf_size + slen + SPACE_FOR_NULL, CFG_COPY);
			
			// if creating (not changing) put at end of JSON object
			// unless level one id was found in which case put as first object element
			if ((flags & CFG_CHANGE) == 0) {
			    if (jt == CFG_LOOKUP_LVL1) {
			        jt = _cfg_lookup_json(cfg, name, CFG_OPT_ID1);
			        assert(jt);
			        pos = jt->start + 1;      // jt->start points to '{' of "id1":{...
			    } else {
				    pos = strlen(cfg->json) - SPACE_FOR_CLOSE_BRACE;
				}
			}
			_cfg_ins(cfg, pos, str_sval);
			
			kiwi_asfree(str_sval);
		} else {
			pos = _cfg_set_string(cfg, name, NULL, CFG_REMOVE, 0);
			_cfg_set_string(cfg, name, val, CFG_CHANGE, pos);
		}
	} else {
	    panic("_cfg_set_string");
	}
	
    assert((cfg->flags & CFG_NO_UPDATE) == 0);
    if (flags & CFG_SAVE) {
        cfg_printf("_cfg_set_string %s=%s SAVE\n", name, val);
        _cfg_save_json(cfg, cfg->json);
    } else {
	    _cfg_parse_json(cfg, FL_PANIC);	// must re-parse
	}
	return pos;
}

void _cfg_default_string(cfg_t *cfg, const char *name, const char *def, bool *error_p)
{
    // v1.463
    // Recover from broken UTF-8 sequences stored in cfg by checking each time
    // even when default is not going to be stored.
    bool error;
	const char *str = _cfg_string(cfg, name, &error, CFG_OPTIONAL);

    char *val = _cfg_string_check_utf8(cfg, name, error? def : str);
    if (error) {
        printf("_cfg_default_string: %s = <%s>\n", name, def);
        _cfg_set_string(cfg, name, def, CFG_SET, 0);
    }
    kiwi_asfree(val);
    _cfg_free(cfg, str);
	if (error_p) *error_p = *error_p | cfg_gdb_break(error);
}


////////////////////////////////
// array
////////////////////////////////

const char *_cfg_array(cfg_t *cfg, const char *name, bool *error, u4_t flags)
{
	const char *array = NULL;
	bool err = false;

	jsmntok_t *jt = _cfg_lookup_json(cfg, name, CFG_OPT_NONE);
	if (!jt || jt == CFG_LOOKUP_LVL1 || _cfg_type_json(cfg, JSMN_ARRAY, jt, &array) == false) {
		err = true;
	}
	if (error) *error = err;
	if (err) {
		if (!(flags & CFG_REQUIRED)) return NULL;
		lprintf("%s: required parameter not found: %s\n", cfg->filename, name);
		panic("cfg_array");
	}

	if (flags & CFG_PRINT) lprintf("CFG read %s: %s = \"%s\"\n", cfg->filename, name, array);
	//if (flags & CFG_PRINT) real_printf("CFG read %s: %s = \"%s\"\n", cfg->filename, name, array);
	return array;
}


////////////////////////////////
// object
////////////////////////////////

const char *_cfg_object(cfg_t *cfg, const char *name, bool *error, u4_t flags)
{
	const char *obj = NULL;
	bool err = false;

	jsmntok_t *jt = _cfg_lookup_json(cfg, name, CFG_OPT_NONE);
	if (!jt || jt == CFG_LOOKUP_LVL1 || _cfg_type_json(cfg, JSMN_OBJECT, jt, &obj) == false) {
		err = true;
	}
	if (error) *error = err;
	if (err) {
		if (!(flags & CFG_REQUIRED)) return NULL;
		lprintf("%s: required parameter not found: %s\n", cfg->filename, name);
		panic("cfg_object");
	}

	if (flags & CFG_PRINT) lprintf("CFG read %s: %s = \"%s\"\n", cfg->filename, name, obj);
	return obj;
}

int _cfg_set_object(cfg_t *cfg, const char *name, const char *val, u4_t flags, int pos)
{
	int slen;
	char *s;
	char *id2 = strchr((char *) name, '.') + 1;
	jsmntok_t *jt = _cfg_lookup_json(cfg, name, CFG_OPT_NONE);
	
	if (flags & CFG_REMOVE) {
		if (!jt || jt == CFG_LOOKUP_LVL1) {
			lprintf("%s: cfg_set_object(CFG_REMOVE) a parameter that doesn't exist: %s\n", cfg->filename, name);
			return 0;
		}
		
		s = &cfg->json[jt->start];

		if (*s == '{') {
            // ,"id":{...} or {"id":{...}
            //       ^start
            assert(JSMN_IS_OBJECT(jt));
		    pos = _cfg_cut(cfg, jt, SLEN_NOTHING_ELSE);
		} else
		if (strncmp(s, "null", 4) == 0) {
            // ,"id":null or {"id":null
            //       ^start
            assert(JSMN_IS_PRIMITIVE(jt));
		    pos = _cfg_cut(cfg, jt, SLEN_NOTHING_ELSE);
        } else {
			lprintf("%s: cfg_set_object(CFG_REMOVE) unexpected value: %s\n", cfg->filename, name);
            panic("cfg");
        }
	} else
	if (flags & (CFG_SET | CFG_CHANGE)) {
		if (!jt || jt == CFG_LOOKUP_LVL1) {
			// create by appending to the end of the JSON string
			if (val == NULL) val = (char *) "null";
			char *obj_sval;
			char *id = (jt == CFG_LOOKUP_LVL1)? id2 : (char *) name;
			asprintf(&obj_sval, "\"%s\":%s", id, val);
			slen = strlen(obj_sval) + SPACE_FOR_POSSIBLE_COMMA;
			assert(cfg->json_buf_size);
			_cfg_realloc_json(cfg, cfg->json_buf_size + slen + SPACE_FOR_NULL, CFG_COPY);
			
			// if creating (not changing) put at end of JSON object
			// unless level one id was found in which case put as first object element
			if ((flags & CFG_CHANGE) == 0) {
			    if (jt == CFG_LOOKUP_LVL1) {
			        jt = _cfg_lookup_json(cfg, name, CFG_OPT_ID1);
			        assert(jt);
			        pos = jt->start + 1;      // jt->start points to '{' of "id1":{...
			    } else {
				    pos = strlen(cfg->json) - SPACE_FOR_CLOSE_BRACE;
				}
			}
			_cfg_ins(cfg, pos, obj_sval);
			
			kiwi_asfree(obj_sval);
		} else {
			pos = _cfg_set_object(cfg, name, NULL, CFG_REMOVE, 0);
			_cfg_set_object(cfg, name, val, CFG_CHANGE, pos);
		}
	} else {
	    panic("_cfg_set_object");
	}
	
    assert((cfg->flags & CFG_NO_UPDATE) == 0);
    if (flags & CFG_SAVE) {
        cfg_printf("_cfg_set_object %s=%s SAVE\n", name, val);
        _cfg_save_json(cfg, cfg->json);
    } else
	    _cfg_parse_json(cfg, FL_PANIC);	// must re-parse
	return pos;
}

void _cfg_default_object(cfg_t *cfg, const char *name, const char *val, bool *error_p)
{
    bool error;
	const char *s = _cfg_object(cfg, name, &error, CFG_OPTIONAL);
	if (error) {
		_cfg_set_object(cfg, name, val, CFG_SET, 0);
		//printf("_cfg_default_object: %s = %s\n", name, val);
	} else {
		_cfg_free(cfg, s);
	}
	if (error_p) *error_p = *error_p | cfg_gdb_break(error);
}


////////////////////////////////
// walk
////////////////////////////////

static const char *jsmntype_s[] = {
	"undef", "obj", "array", "string", "prim"
};

#define TOK_VIRTUAL -1
#define N_INDENT 3

bool cfg_print_tok(cfg_t *cfg, void *param1, void *param2, jsmntok_t *jt, int seq, int hit, int lvl, int rem, void **rval)
{
	int n;
	char *s = &cfg->json[jt->start];
	if (lvl >= 1 && seq == TOK_VIRTUAL) lvl--;
	printf("%4d: %d-%03d%s %4d ", seq, lvl, rem, (hit == lvl)? "*":" ", jt->start);
	int indent_n = lvl? ((lvl-1) * N_INDENT) : 0;
	const char *indent_s = "                                   ";

	switch (jt->type) {
        case JSMN_OBJECT:
        case JSMN_ARRAY:
            if (seq == TOK_VIRTUAL)
                // virtual token
                printf("%6s #%02d %.*s%c\n", jsmntype_s[jt->type], jt->size, indent_n, indent_s, (JSMN_IS_OBJECT(jt))? '}':']');
            else
                printf("%6s #%02d %.*s%c %d-%d\n", jsmntype_s[jt->type], jt->size, indent_n, indent_s, s[0], jt->start, jt->end);
            break;
        case JSMN_STRING:
            n = jt->end - jt->start;
            if (JSMN_IS_ID(jt)) {
                printf("    id #%02d %.*s%.*s:\n", jt->size, indent_n, indent_s, n, s);
            } else {
                printf("%6s #%02d %.*s\"%.*s\"\n", jsmntype_s[jt->type], jt->size, indent_n + N_INDENT, indent_s, n, s);
            }
            break;
        case JSMN_PRIMITIVE:
            n = jt->end - jt->start;
            printf("%6s #%02d %.*s%.*s\n", jsmntype_s[jt->type], jt->size, indent_n + N_INDENT, indent_s, n, s);
            break;
        default:
            break;
	}
	
	return false;
}

// The callback returns true if rval contains the value _cfg_walk() should return.
// lvl_id constrains callbacks to elements of a matching level 1 sub-object.
void *_cfg_walk(cfg_t *cfg, const char *lvl_id, cfg_walk_cb_t cb, void *param1, void *param2)
{
	int i, n, idlen = lvl_id? strlen(lvl_id) : 0;
	jsmntok_t *jt = cfg->tokens;
	int hit = -1;
	int lvl = 0, remstk[32], _lvl = 0, _rem;
	memset(remstk, 0, sizeof(remstk));
	jsmntok_t *remjt[32];
	void *rval, *rtn_rval = NULL;
	bool stop = false;
	
	for (i=0; i < cfg->ntok && !stop; i++) {
		char *s = &cfg->json[jt->start];
		_lvl = lvl; _rem = remstk[lvl];

		if (lvl && (jt->type != JSMN_STRING || JSMN_IS_STRING(jt))) {
			remstk[lvl]--;
		}

		if (JSMN_IS_OBJECT(jt) || JSMN_IS_ARRAY(jt)) {
			lvl++;
			if (!lvl_id || _lvl == hit) {
				if (cb(cfg, param1, param2, jt, i, hit, _lvl, _rem, &rval)) {
				    rtn_rval = rval;
				    stop = true;
				}
			}
			remstk[lvl] = jt->size;
			remjt[lvl] = jt;
		} else {
			if (!lvl_id || _lvl == hit) {
				if (cb(cfg, param1, param2, jt, i, hit, _lvl, _rem, &rval)) {
				    rtn_rval = rval;
				    stop = true;
				}
			}
		}

		// check for optional level 1 id match
		if (hit == -1 && lvl == 1 && JSMN_IS_ID(jt)) {
			n = jt->end - jt->start;
			if (lvl_id && n == idlen && strncmp(s, lvl_id, n) == 0) {
			    //printf("_cfg_walk: hit=%d jt#%d lvl_id=%s\n", lvl, i, lvl_id);
				hit = lvl+1;
			}
		}

		while (lvl && remstk[lvl] == 0) {
			cb(cfg, param1, param2, remjt[lvl], TOK_VIRTUAL, hit, lvl, 0, &rval);	// virtual-tokens to close objects and arrays
			lvl--;
			if (hit != -1 && lvl < hit)
				hit = -1;	// clear id match once level is complete
		}

		jt++;
	}
	
	return rtn_rval;
}


////////////////////////////////
// misc
////////////////////////////////

//#define TMEAS(x) x
#define TMEAS(x)

static bool _cfg_parse_json(cfg_t *cfg, u4_t flags)
{
    // the dx list can be huge, so yield during the time-consuming parsing process
    bool yield = ((cfg->flags & CFG_YIELD) && !cfg->init_load);
    if (!cfg->basename) cfg->basename = cfg->filename;
    TMEAS(printf("cfg_parse_json: START %s yield=%d\n", cfg->basename, yield);)
    
	if (cfg->tok_size == 0)
		cfg->tok_size = 64;
	
	// remove any cjson comment lines
	char *cp = cfg->json, *cp2;
	while ((cp = strstr(cfg->json, "\n//")) != NULL) {
	    cp2 = strstr(cp+1, "\n");
	    if (cp2) {
	        kiwi_overlap_strcpy(cp+1, cp2+1);
	    }
	}
	
	int slen = strlen(cfg->json);
	assert(cfg->json_buf_size >= slen + SPACE_FOR_NULL);
	jsmn_parser parser;
	
	int rc;
	do {
		if (cfg->tokens)
            kiwi_free(cfg->id_tokens, cfg->tokens);
		
		TMEAS(printf("cfg_parse_json: TOKENS %s tok_size=%d tok_mem=%d\n", cfg->basename, cfg->tok_size, sizeof(jsmntok_t) * cfg->tok_size);)
		cfg->tokens = (jsmntok_t *) kiwi_malloc(cfg->id_tokens, sizeof(jsmntok_t) * cfg->tok_size);

		jsmn_init(&parser);
		if (cfg == &cfg_cfg) {
		    cfg_printf("_cfg_parse_json cfg_cfg\n");
		}
		if ((rc = jsmn_parse(&parser, cfg->json, slen, cfg->tokens, cfg->tok_size, yield)) >= 0)
			break;
		
		if (rc == JSMN_ERROR_NOMEM) {
			//printf("not enough tokens (%d) were provided\n", cfg->tok_size);
			cfg->tok_size *= 4;		// keep going until we hit safety limit in kiwi_malloc()
		} else {
		    const char *err = "(unknown error)";
		    if (rc == JSMN_ERROR_INVAL) err = "invalid character inside JSON string"; else
		    if (rc == JSMN_ERROR_PART) err = "the string is not a full JSON packet, more bytes expected";
			lprintf("cfg_parse_json: file %s line=%d position=%d token=%d %s\n",
				cfg->filename, parser.line, parser.pos, parser.toknext, err);

			// show INDENT chars before error, but handle case where there aren't that many available
			#define INDENT 4
			int pos = parser.pos;
			pos = MAX(0, pos -INDENT);
			int cnt = parser.pos - pos;
			char *cp = &cfg->json[pos];
			for (int i=0; i < 64 && *cp != '\0'; i++) {
				if (*cp == '\n')
				    *cp = '\0';
				else
				    cp++;
			}
			lprintf("%.64s\n", &cfg->json[pos]);
			lprintf("%s^ JSON error position\n", cnt? &"    "[INDENT-cnt] : "");
			if (flags & FL_PANIC) { panic("jsmn_parse"); } else return false;
		}
	} while (rc == JSMN_ERROR_NOMEM);

	//printf("using %d of %d tokens\n", rc, cfg->tok_size);
	cfg->ntok = rc;
    TMEAS(printf("cfg_parse_json: DONE %s json string -> json struct\n", cfg->basename);)
    cfg->flags |= CFG_PARSE_VALID;
	return true;
}

char *_cfg_realloc_json(cfg_t *cfg, int new_size, u4_t flags)
{
	assert(new_size > 0);
	if (cfg->json_buf_size >= new_size) {
		assert(cfg->json);
	} else {
		char *prev = cfg->json;
		int prev_size = cfg->json_buf_size;
		cfg->json_buf_size = new_size;
		cfg->json = (char *) kiwi_malloc(cfg->id_json, cfg->json_buf_size);
		if (flags & CFG_COPY) {
			if (prev != NULL && prev_size != 0) {
				strcpy(cfg->json, prev);
			}
		}
		if (prev)
			kiwi_free(cfg->id_json, prev);
	}
	return cfg->json;
}

static bool _cfg_load_json(cfg_t *cfg)
{
	int i;
	size_t n;
	
    TMEAS(printf("cfg_load_json: START %s\n", cfg->basename);)

	off_t fsize = kiwi_file_size(cfg->filename);
	if (fsize == -1) return false;
	_cfg_realloc_json(cfg, fsize + SPACE_FOR_NULL, CFG_NONE);
	
	if (fsize > 128*K)
	    lprintf("CAUTION: large configuration file, will take time to parse: %s ...\n", cfg->filename);
	
    FILE *fp;
    scallz("_cfg_load_json fopen", (fp = fopen(cfg->filename, "r")));
    n = fread(cfg->json, 1, cfg->json_buf_size, fp);
    assert(n >= 0 && n < cfg->json_buf_size);   // n == 0 if e.g. a curl/wget fails and file is zero length
    fclose(fp);

	// turn into a string
	cfg->json[n] = '\0';
	if (n > 0 && cfg->json[n-1] == '\n')
		cfg->json[n-1] = '\0';

	// hack to add passband configuration (too difficult to do with cfg.h interface)
	if (cfg == &cfg_cfg && strstr(cfg->json, "\"passbands\":") == NULL) {
	    _cfg_realloc_json(cfg, n + 1024, CFG_COPY);
	    strcpy(&(cfg->json[n-2]),
	        ",\"passbands\":{"
	            "\"am\":  {\"lo\":-4900, \"hi\":4900},"
	            "\"amn\": {\"lo\":-2500, \"hi\":2500},"
	            "\"amw\": {\"lo\":-6000, \"hi\":6000},"
                "\"sam\": {\"lo\":-4900, \"hi\":4900},"
                "\"sal\": {\"lo\":-4900, \"hi\":   0},"
                "\"sau\": {\"lo\":    0, \"hi\":4900},"
                "\"sas\": {\"lo\":-4900, \"hi\":4900},"
                "\"qam\": {\"lo\":-4900, \"hi\":4900},"
                "\"drm\": {\"lo\":-5000, \"hi\":5000},"
                "\"lsb\": {\"lo\":-2700, \"hi\":-300},"
                "\"lsn\": {\"lo\":-2400, \"hi\":-300},"
                "\"usb\": {\"lo\":  300, \"hi\":2700},"
                "\"usn\": {\"lo\":  300, \"hi\":2400},"
                "\"cw\":  {\"lo\":  300, \"hi\": 700},"
                "\"cwn\": {\"lo\":  470, \"hi\": 530},"
                "\"nbfm\":{\"lo\":-6000, \"hi\":6000},"
                "\"nnfm\":{\"lo\":-3000, \"hi\":3000},"
                "\"iq\":  {\"lo\":-5000, \"hi\":5000}"
	        "}}");
        _cfg_save_json(cfg, cfg->json);
        return true;
	}
	
	if (!(cfg->flags & CFG_LOAD_ONLY)) {
        TMEAS(printf("cfg_load_json: PARSE %s\n", cfg->basename);)
        if (_cfg_parse_json(cfg) == false)
            return false;

        #if 0
            if (cfg != &cfg_dx) {
                printf("walking %s config list after load (%d tokens)...\n", cfg->filename, cfg->ntok);
                _cfg_walk(cfg, NULL, cfg_print_tok);
            }
        #endif
    }

    TMEAS(printf("cfg_load_json: DONE %s\n", cfg->basename);)
	return true;
}

// *** CAUTION: Only use real_printf() here.
// Regular printf()s shouldn't be used from a child process.
static void _cfg_write_file(void *param)
{
    cfg_t *cfg = (cfg_t *) FROM_VOID_PARAM(param);
	FILE *fp;

    //real_printf("_cfg_write_file %s %d|%d\n", cfg->filename, strlen(cfg->json_write), cfg->json_buf_size);
	scallz("_cfg_write_file fopen", (fp = fopen(cfg->filename, "w")));
	fprintf(fp, "%s\n", cfg->json_write);
	fclose(fp);
    //real_printf("_cfg_write_file DONE\n");
}

void _cfg_save_json(cfg_t *cfg, char *json)
{
    int jsl = strlen(json);
    if (cfg == &cfg_cfg) {
        cfg_printf("_cfg_save_json CFG size %d\n", jsl);
        if (jsl < 32)
            panic("_cfg_save_json too small?");
    }
    
    TMEAS(static int g_seq; int seq = g_seq; g_seq++;)
	TMEAS(u4_t start = timer_ms(); printf("cfg_save_json START #%d %s json_len=%d\n", seq, cfg->basename, strlen(cfg->json));)

	// if new buffer is different update our copy
	if (!cfg->json || (cfg->json && cfg->json != json)) {
	    //printf("_cfg_save_json CUR:json=%d CUR:json_buf_size=%d NEW:json=%d new_size=%d\n", strlen(cfg->json), cfg->json_buf_size, jsl, jsl + SPACE_FOR_NULL);
		_cfg_realloc_json(cfg, jsl + SPACE_FOR_NULL, CFG_NONE);
		//printf("_cfg_save_json NEW:json_buf_size=%d\n", cfg->json_buf_size);
		strcpy(cfg->json, json);
	}

	cfg->json_write = strdup(cfg->json);

    #define CHECK_JSON_INTEGRITY_BEFORE_SAVE
    #ifdef CHECK_JSON_INTEGRITY_BEFORE_SAVE
        if (!(cfg->flags & CFG_NO_INTEG)) {
            cfg_t tcfg;
            memset(&tcfg, 0, sizeof(tcfg));
            tcfg.flags = CFG_NONE;
            asprintf((char **) &tcfg.filename, "tcfg:%s", cfg->basename);
            tcfg.json = cfg->json_write;
            tcfg.json_buf_size = cfg->json_buf_size;
            //printf("cfg_save_json START %s %d|%d\n", tcfg.basename, strlen(tcfg.json), tcfg.json_buf_size);
    
            //#define TEST_JSON_INTEGRITY_CHECK
            #ifdef TEST_JSON_INTEGRITY_CHECK
                static int pass;
                if ((pass % 5) == 4) {
                    tcfg.json[0] = '!';     // test that re-parse failure works
                }
                pass++;
            #endif
        
            bool parsed_ok = _cfg_parse_json(&tcfg);
            //printf("cfg_save_json END\n");
            kiwi_free(tcfg.filename, tcfg.tokens);
            free((char *) tcfg.filename);
    
            if (!parsed_ok) {
                lprintf("cfg_save_json: %s JSON PARSE ERROR -- FILE SAVE ABORTED!\n", cfg->filename);
                #define PANIC_ON_INTEGRITY_FAIL
                #ifdef PANIC_ON_INTEGRITY_FAIL
                    real_printf("%s\n", tcfg.json);
                    panic("json integrity fail");
                #else
                    free(cfg->json_write);
                    cfg->json_write = NULL;
                    return;
                #endif
            }
        }
    #endif

    // This takes forever for a large file.
    // But we fixed the realtime impact by putting a NextTask() in jsmn_parse()
    // (conditional on cfg->flags & CFG_YIELD)
    if ((cfg->flags & CFG_NO_UPDATE) == 0) {
        _cfg_parse_json(cfg, FL_PANIC);
    } else {
        cfg->flags &= ~CFG_PARSE_VALID;
    }
    TMEAS(u4_t split = timer_ms(); printf("cfg_save_json SPLIT #%d %s reparse %.3f msec\n", seq, cfg->basename, TIME_DIFF_MS(split, start));)

    // File writes can sometimes take a long time -- use a child task and wait via NextTask()
    // CAUTION: Must not do any task yielding until re-parse above (if any) is finished
    // so other tasks don't see an empty cfg. So delay write of file until here.
    int status = child_task("kiwi.cfg", _cfg_write_file, POLL_MSEC(100), TO_VOID_PARAM(cfg));
    int exit_status;
    if (WIFEXITED(status) && (exit_status = WEXITSTATUS(status))) {
        printf("cfg_write_file exit_status=0x%x\n", exit_status);
    }

    free(cfg->json_write);
    cfg->json_write = NULL;
    //printf("cfg_save_json DONE\n");

    TMEAS(u4_t now = timer_ms(); printf("cfg_save_json DONE #%d %s file save %.3f/%.3f msec\n", seq, cfg->basename, TIME_DIFF_MS(now, split), TIME_DIFF_MS(now, start));)
}

void _cfg_update_json(cfg_t *cfg)
{
    if (cfg == NULL) return;

    if ((cfg->flags & CFG_NO_UPDATE) && (cfg->flags & CFG_PARSE_VALID) == 0) {
        printf("_cfg_update_json: cfg <%s> OUT-OF-DATE re-parsing\n", cfg->basename);
        _cfg_parse_json(cfg, FL_PANIC);
    }
}

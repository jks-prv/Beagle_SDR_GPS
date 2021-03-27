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
#include "timer.h"
#include "web.h"
#include "eeprom.h"
#include "coroutines.h"
#include "jsmn.h"
#include "cfg.h"

#ifndef CFG_GPS_ONLY
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

#define CFG_FN	DIR_CFG "/" CFG_PREFIX "kiwi.json";
#define ADM_FN	DIR_CFG "/" CFG_PREFIX "admin.json"
#define DX_FN	DIR_CFG "/" CFG_PREFIX "dx.json";

#define SPACE_FOR_CLOSE_BRACE				1
#define SPACE_FOR_POSSIBLE_COMMA			1
#define	SLEN_QUOTE_COLON					2
#define	SLEN_3QUOTES_COLON					4
#define JSON_FIRST_QUOTE					1

static void cfg_test()
{
	cfg_t cfgx;
	char *buf;

	printf("\n");
	printf("test 4 basic cut/ins cases:\n");
	buf = (char *) "{\"L\":1,\"foo\":123,\"R\":2}";
	json_init(&cfgx, buf);
	json_set_int(&cfgx, "foo", 9999);

	printf("\n");
	buf = (char *) "{\"foo\":123,\"R\":2}";
	json_init(&cfgx, buf);
	json_set_int(&cfgx, "foo", 9999);
	
	printf("\n");
	buf = (char *) "{\"foo\":123}";
	json_init(&cfgx, buf);
	json_set_int(&cfgx, "foo", 9999);

	printf("\n");
	buf = (char *) "{\"L\":1,\"foo\":123}";
	json_init(&cfgx, buf);
	json_set_int(&cfgx, "foo", 9999);

	printf("\n");
	printf("test 2 new creation cases:\n");
	buf = (char *) "{}";
	json_init(&cfgx, buf);
	json_set_int(&cfgx, "foo", 9999);

	printf("\n");
	buf = (char *) "{\"L\":1}";
	json_init(&cfgx, buf);
	json_set_int(&cfgx, "foo", 9999);

	printf("\n");
	printf("test cut/ins of other types:\n");
	buf = (char *) "{\"L\":1,\"foo\":1.234,\"R\":2}";
	json_init(&cfgx, buf);
	json_set_float(&cfgx, "foo", 5.678);

	printf("\n");
	buf = (char *) "{\"L\":1,\"foo\":false,\"R\":2}";
	json_init(&cfgx, buf);
	json_set_bool(&cfgx, "foo", true);

	printf("\n");
	buf = (char *) "{\"L\":1,\"foo\":\"bar\",\"R\":2}";
	json_init(&cfgx, buf);
	json_set_string(&cfgx, "foo", "baz");

	//_cfg_walk(&cfgx, NULL, cfg_print_tok, NULL);
	kiwi_exit(0);
}

int serial_number;

void cfg_reload()
{
	cfg_init();
	admcfg_init();

    //cfg_test();
    
    if ((serial_number = cfg_int("serial_number", NULL, CFG_OPTIONAL)) > 0) {
        lprintf("serial number override from configuration: %d\n", serial_number);
    } else {
        if ((serial_number = eeprom_check()) <= 0) {
            lprintf("can't read serial number from EEPROM and no configuration override\n");
            serial_number = 0;
        } else {
            lprintf("serial number from EEPROM: %d\n", serial_number);
        }
    }

#ifndef CFG_GPS_ONLY
	dx_reload();
#endif
}

cfg_t cfg_cfg, cfg_adm, cfg_dx;
	
char *_cfg_get_json(cfg_t *cfg, int *size)
{
	if (!cfg->init) return NULL;
	
	if (size) *size = cfg->json_buf_size;
	return cfg->json;
}

char *_cfg_realloc_json(cfg_t *cfg, int new_size, u4_t flags);
static bool _cfg_load_json(cfg_t *cfg);
static bool _cfg_parse_json(cfg_t *cfg, bool doPanic);

bool _cfg_init(cfg_t *cfg, int flags, char *buf)
{
	if (buf != NULL) {
		memset(cfg, 0, sizeof(cfg_t));
		cfg->filename = (char *) "(buf)";
		_cfg_realloc_json(cfg, strlen(buf) + SPACE_FOR_NULL, CFG_NONE);
		strcpy(cfg->json, buf);
		if (_cfg_parse_json(cfg, false) == false)
			return false;
		cfg->init = true;
		cfg->isJSON = true;
		return true;
	} else
	
	if (cfg == &cfg_cfg) {
		cfg->filename = CFG_FN;
	} else
	if (cfg == &cfg_adm) {
		cfg->filename = ADM_FN;
	} else
	if (cfg == &cfg_dx) {
		cfg->filename = DX_FN;
	} else {
		panic("cfg_init cfg");
	}
	
	if (!cfg->init) {
        cfg->flags = flags;
	    cfg->init_load = true;
		if (_cfg_load_json(cfg) == false) {
			if (cfg == &cfg_dx) {
				lprintf("DX configuration file %s: JSON parse failed\n", cfg->filename);
				return false;
			}
			panic("cfg_init json");
		}
		cfg->init = true;
	    cfg->init_load = false;
	}
	
	lprintf("reading configuration from file %s: %d tokens\n", cfg->filename, cfg->ntok);

	if (cfg == &cfg_cfg) {
		struct stat st;
		const char *adm_fn = ADM_FN;
		
		if (stat(adm_fn, &st) != 0) {
			system("echo -n \"{}\" >" ADM_FN);
			admcfg_init();
			lprintf("#### transitioning to admin.json file ####\n");
			cfg_adm_transition();
		}
	}
	
	return true;
}

void _cfg_release(cfg_t *cfg)
{
	if (!cfg->init || !cfg->isJSON) return;
	if (cfg->tokens) {
	    //printf("cfg_release: tokens %p\n", cfg->tokens);
	    kiwi_free("cfg tokens", cfg->tokens);
	}
	cfg->tokens = NULL;
	if (cfg->json) {
	    //printf("cfg_release: json %p\n", cfg->json);
	    kiwi_free("json buf", cfg->json);
	}
	cfg->json = NULL;
}

static jsmntok_t *_cfg_lookup_id(cfg_t *cfg, jsmntok_t *jt_start, const char *id)
{
	if (!cfg->init) return NULL;
	
	int i, idlen = strlen(id);
	jsmntok_t *jt;
	
	for (jt = jt_start; jt != &cfg->tokens[cfg->ntok]; jt++) {
		int n = jt->end - jt->start;
		char *s = &cfg->json[jt->start];
		if (JSMN_IS_ID(jt)) {
			//printf("key %d: %d <%.*s> cmp %d\n", i, n, n, s, strncmp(id, s, n));
			if (n == idlen && strncmp(id, s, n) == 0) {
				return jt+1;
			}
		}
	}
	
	return NULL;
}

// should get called for every second-level object of id1
// search for match with id2
static bool _cfg_lookup_json_cb(cfg_t *cfg, void *param, jsmntok_t *jt, int seq, int hit, int lvl, int rem, void **rval)
{
	char *id2 = (char *) param;
	int id2_len = strlen(id2);
	
	//cfg_print_tok(cfg, param, jt, seq, hit, lvl, rem, rval);
	if (!JSMN_IS_ID(jt)) return false;
	char *s = &cfg->json[jt->start];
	int n = jt->end - jt->start;
	//printf("_cfg_lookup_json_cb 2-scope: TEST id=\"%.*s\" WANT id2=\"%s\"\n", n, s, id2);
	if (n != id2_len || strncmp(s, id2, n) != 0) return false;
	if (rval) *rval = jt+1;
	return true;
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

	// handle two levels of id scope, i.e. id1.id2, but ignore more like ip addresses with three dots
	if (dot && !dotdot && option != CFG_OPT_NO_DOT) {
		char *id1_m = NULL, *id2_m = NULL;
		i = sscanf(id, "%m[^.].%ms", &id1_m, &id2_m);
		//printf("_cfg_lookup_json 2-scope: key=\"%s\" n=%d id1=\"%s\" id2=\"%s\"\n", id, i, id1_m, id2_m);
		if (i != 2) {
            kiwi_ifree(id1_m); kiwi_ifree(id2_m);
		    return NULL;
		}
		if (strchr(id2_m, '.') != NULL) panic("_cfg_lookup_json: more than two levels of scope in id");
		
		// lookup just the id1 of a two-scope id
		if (option == CFG_OPT_ID1) {
		    jt = _cfg_lookup_id(cfg, cfg->tokens, id1_m);
            kiwi_ifree(id1_m); kiwi_ifree(id2_m);
		    return jt;
		} else {
            // run callback for all second scope objects of id1
            void *rtn_rval = _cfg_walk(cfg, id1_m, _cfg_lookup_json_cb, (void *) id2_m);
            
            if (rtn_rval == NULL && _cfg_lookup_id(cfg, cfg->tokens, id1_m) != NULL) {
                // if id1 exists but id2 is missing then return this fact
                kiwi_ifree(id1_m); kiwi_ifree(id2_m);
                return CFG_LOOKUP_LVL1;
            }
            
            //printf("_cfg_lookup_json 2-scope: rtn_rval=%p id=%s\n", rtn_rval, id);
            kiwi_ifree(id1_m); kiwi_ifree(id2_m);
            return (jsmntok_t *) rtn_rval;
        }
        assert("not reached");
		
	} else {
		return _cfg_lookup_id(cfg, cfg->tokens, id);
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
	if (str != NULL) kiwi_ifree((char *) str);
}

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
	//real_printf("PRE-CUT <<%s>>\n", cfg->json);
	s = &cfg->json[key_jt->start - JSON_FIRST_QUOTE - 1];
	while (*s == ' ' || *s == '\t')
		s--;
	assert(*s == '{' || *s == ',' || *s == '\n');
	s++;
	e = &cfg->json[key_jt->start + key_size + val_size + skip];

	// ddd\nxxx or ddd,xxx -> xxx
	if (*e == '\n' || (*e == ',' && *(e+1) != '\n')) e++;
	else
	// ddd,\nxxx -> xxx
	if (*e == ',' && *(e+1) == '\n') e += 2;

	// e.g. xxx,ddd} -> xxx,} -> xxx}
	if (*(s-1) == ',' && *e == '}')
		s--;

	kiwi_overlap_strcpy(s, e);
	//real_printf("POST-CUT %d <<%s>>\n", s - cfg->json, cfg->json);
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
	//real_printf("PRE-INS <<%s>>\n", cfg->json);

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

	//real_printf("POST-INS <<%s>>\n", cfg->json);
}

bool _cfg_int_json(cfg_t *cfg, jsmntok_t *jt, int *num)
{
	assert(jt != NULL);
	char *s = &cfg->json[jt->start];
	if (JSMN_IS_PRIMITIVE(jt) && (*s == '-' || isdigit(*s))) {
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
		    pos = _cfg_cut(cfg, jt, SLEN_QUOTE_COLON);
		} else
		if (strncmp(s, "null", 4) == 0) {
            // ,"id":null or {"id":null
            //       ^start
            assert(JSMN_IS_PRIMITIVE(jt));
		    pos = _cfg_cut(cfg, jt, SLEN_QUOTE_COLON);
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
			_cfg_realloc_json(cfg, cfg->json_buf_size + slen, CFG_COPY);
			
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
			
			kiwi_ifree(int_sval);
		} else {
			pos = _cfg_set_int(cfg, name, 0, CFG_REMOVE, 0);
			_cfg_set_int(cfg, name, val, CFG_CHANGE, pos);
		}
	} else {
	    panic("_cfg_set_int");
	}

    assert((cfg->flags & CFG_NO_UPDATE) == 0);
    if (flags & CFG_SAVE)
        _cfg_save_json(cfg, cfg->json);
    else
	    _cfg_parse_json(cfg, true);	// must re-parse
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
	if (error_p) *error_p = *error_p | error;
	return existing;
}

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
		    pos = _cfg_cut(cfg, jt, SLEN_QUOTE_COLON);
		} else
		if (strncmp(s, "null", 4) == 0) {
            // ,"id":null or {"id":null
            //       ^start
            assert(JSMN_IS_PRIMITIVE(jt));
		    pos = _cfg_cut(cfg, jt, SLEN_QUOTE_COLON);
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
			_cfg_realloc_json(cfg, cfg->json_buf_size + slen, CFG_COPY);
			
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

			kiwi_ifree(float_sval);
		} else {
			pos = _cfg_set_float(cfg, name, 0, CFG_REMOVE, 0);
			_cfg_set_float(cfg, name, val, CFG_CHANGE, pos);
		}
	} else {
	    panic("_cfg_set_float");
	}
	
    assert((cfg->flags & CFG_NO_UPDATE) == 0);
    if (flags & CFG_SAVE)
        _cfg_save_json(cfg, cfg->json);
    else
	    _cfg_parse_json(cfg, true);	// must re-parse
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
	if (error_p) *error_p = *error_p | error;
	return existing;
}

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
		    pos = _cfg_cut(cfg, jt, SLEN_QUOTE_COLON);
		} else
		if (strncmp(s, "null", 4) == 0) {
            // ,"id":null or {"id":null
            //       ^start
            assert(JSMN_IS_PRIMITIVE(jt));
		    pos = _cfg_cut(cfg, jt, SLEN_QUOTE_COLON);
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
			_cfg_realloc_json(cfg, cfg->json_buf_size + slen, CFG_COPY);
			
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
			
			kiwi_ifree(bool_sval);
		} else {
			pos = _cfg_set_bool(cfg, name, 0, CFG_REMOVE, 0);
			_cfg_set_bool(cfg, name, val, CFG_CHANGE, pos);
		}
	} else {
	    panic("_cfg_set_bool");
	}
	
    assert((cfg->flags & CFG_NO_UPDATE) == 0);
    if (flags & CFG_SAVE)
        _cfg_save_json(cfg, cfg->json);
    else
	    _cfg_parse_json(cfg, true);	// must re-parse
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
	if (error_p) *error_p = *error_p | error;
	return existing;
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
		
		s = &cfg->json[jt->start] - 1;

		if (*s == '\"') {
            // ,"id":"string" or {"id":"string"
            //        ^start (NB: NOT first double quote)
            assert(JSMN_IS_STRING(jt));
		    pos = _cfg_cut(cfg, jt, SLEN_3QUOTES_COLON);
		} else
		if (strncmp(s, ":null", 5) == 0) {
            // ,"id":null or {"id":null
            //       ^start
            assert(JSMN_IS_PRIMITIVE(jt));
		    pos = _cfg_cut(cfg, jt, SLEN_QUOTE_COLON);
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
			_cfg_realloc_json(cfg, cfg->json_buf_size + slen, CFG_COPY);
			
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
			
			kiwi_ifree(str_sval);
		} else {
			pos = _cfg_set_string(cfg, name, NULL, CFG_REMOVE, 0);
			_cfg_set_string(cfg, name, val, CFG_CHANGE, pos);
		}
	} else {
	    panic("_cfg_set_string");
	}
	
    assert((cfg->flags & CFG_NO_UPDATE) == 0);
    if (flags & CFG_SAVE)
        _cfg_save_json(cfg, cfg->json);
    else
	    _cfg_parse_json(cfg, true);	// must re-parse
	return pos;
}

void _cfg_default_string(cfg_t *cfg, const char *name, const char *val, bool *error_p)
{
    bool error;
	const char *s = _cfg_string(cfg, name, &error, CFG_OPTIONAL);
	if (error) {
		_cfg_set_string(cfg, name, val, CFG_SET, 0);
		//printf("_cfg_default_string: %s = %s\n", name, val);
	} else {
		_cfg_free(cfg, s);
	}
	if (error_p) *error_p = *error_p | error;
}


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
		    pos = _cfg_cut(cfg, jt, SLEN_QUOTE_COLON);
		} else
		if (strncmp(s, "null", 4) == 0) {
            // ,"id":null or {"id":null
            //       ^start
            assert(JSMN_IS_PRIMITIVE(jt));
		    pos = _cfg_cut(cfg, jt, SLEN_QUOTE_COLON);
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
			_cfg_realloc_json(cfg, cfg->json_buf_size + slen, CFG_COPY);
			
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
			
			kiwi_ifree(obj_sval);
		} else {
			pos = _cfg_set_object(cfg, name, NULL, CFG_REMOVE, 0);
			_cfg_set_object(cfg, name, val, CFG_CHANGE, pos);
		}
	} else {
	    panic("_cfg_set_object");
	}
	
    assert((cfg->flags & CFG_NO_UPDATE) == 0);
    if (flags & CFG_SAVE)
        _cfg_save_json(cfg, cfg->json);
    else
	    _cfg_parse_json(cfg, true);	// must re-parse
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
	if (error_p) *error_p = *error_p | error;
}


static const char *jsmntype_s[] = {
	"undef", "obj", "array", "string", "prim"
};

bool cfg_print_tok(cfg_t *cfg, void *param, jsmntok_t *jt, int seq, int hit, int lvl, int rem, void **rval)
{
	int n;
	char *s = &cfg->json[jt->start];
	printf("%4d: %d-%02d%s %4d ", seq, lvl, rem, (hit == lvl)? "*":" ", jt->start);

	switch (jt->type) {
	case JSMN_OBJECT:
	case JSMN_ARRAY:
		if (seq == -1)
			// virtual token
			printf("%6s #%02d '%c'\n", jsmntype_s[jt->type], jt->size, (JSMN_IS_OBJECT(jt))? '}':']');
		else
			printf("%6s #%02d '%c' %d-%d\n", jsmntype_s[jt->type], jt->size, s[0], jt->start, jt->end);
		break;
	case JSMN_STRING:
		n = jt->end - jt->start;
		if (JSMN_IS_ID(jt)) {
			printf("    id #%02d %.*s:\n", jt->size, n, s);
		} else {
			printf("%6s #%02d \"%.*s\"\n", jsmntype_s[jt->type], jt->size, n, s);
		}
		break;
	case JSMN_PRIMITIVE:
		n = jt->end - jt->start;
		printf("%6s #%02d %.*s\n", jsmntype_s[jt->type], jt->size, n, s);
		break;
	default:
		break;
	}
	
	return false;
}

// the callback returns true if rval contains the value _cfg_walk() should return
void *_cfg_walk(cfg_t *cfg, const char *id, cfg_walk_cb_t cb, void *param)
{
	int i, n, idlen = id? strlen(id):0;
	jsmntok_t *jt = cfg->tokens;
	int hit = -1;
	int lvl = 0, remstk[32], _lvl = 0, _rem;
	memset(remstk, 0, sizeof(remstk));
	jsmntok_t *remjt[32];
	void *rval, *rtn_rval = NULL;
	
	for (i=0; i < cfg->ntok; i++) {
		char *s = &cfg->json[jt->start];
		_lvl = lvl; _rem = remstk[lvl];

		if (lvl && (jt->type != JSMN_STRING || JSMN_IS_STRING(jt))) {
			remstk[lvl]--;
		}

		if (JSMN_IS_OBJECT(jt) || JSMN_IS_ARRAY(jt)) {
			lvl++;
			if (!id || _lvl == hit) {
				if (cb(cfg, param, jt, i, hit, _lvl, _rem, &rval))
				    rtn_rval = rval;
			}
			remstk[lvl] = jt->size;
			remjt[lvl] = jt;
		} else {
			if (!id || _lvl == hit) {
				if (cb(cfg, param, jt, i, hit, _lvl, _rem, &rval))
				    rtn_rval = rval;
			}
		}

		// check for optional id match
		if (hit == -1 && JSMN_IS_ID(jt)) {
			n = jt->end - jt->start;
			if (id && n == idlen && strncmp(s, id, n) == 0)
				hit = lvl+1;
		}

		while (lvl && remstk[lvl] == 0) {
			cb(cfg, param, remjt[lvl], -1, hit, lvl, 0, &rval);	// virtual-tokens to close objects and arrays
			lvl--;
			if (hit != -1 && lvl < hit)
				hit = -1;	// clear id match once level is complete
		}

		jt++;
	}
	
	return rtn_rval;
}

//#define TMEAS(x) x
#define TMEAS(x)

static bool _cfg_parse_json(cfg_t *cfg, bool doPanic)
{
    // the dx list can be huge, so yield during the time-consuming parsing process
    bool yield = (cfg == &cfg_dx && !cfg->init_load);
    TMEAS(printf("cfg_parse_json: START %s yield=%d\n", cfg->filename, yield);)
    
	if (cfg->tok_size == 0)
		cfg->tok_size = 64;
	
	int slen = strlen(cfg->json);
	assert(cfg->json_buf_size >= slen + SPACE_FOR_NULL);
	jsmn_parser parser;
	
	int rc;
	do {
		if (cfg->tokens)
			kiwi_free("cfg tokens", cfg->tokens);
		
		TMEAS(printf("cfg_parse_json: file=%s tok_size=%d\n", cfg->filename, cfg->tok_size);)
		cfg->tokens = (jsmntok_t *) kiwi_malloc("cfg tokens", sizeof(jsmntok_t) * cfg->tok_size);

		jsmn_init(&parser);
		if ((rc = jsmn_parse(&parser, cfg->json, slen, cfg->tokens, cfg->tok_size, yield)) >= 0)
			break;
		
		if (rc == JSMN_ERROR_NOMEM) {
			//printf("not enough tokens (%d) were provided\n", cfg->tok_size);
			cfg->tok_size *= 4;		// keep going until we hit safety limit in kiwi_malloc()
		} else {
			lprintf("cfg_parse_json: file %s line=%d pos=%d tok=%d\n",
				cfg->filename, parser.line, parser.pos, parser.toknext);
			if (rc == JSMN_ERROR_INVAL)
				lprintf("invalid character inside JSON string\n");
			else
			if (rc == JSMN_ERROR_PART)
				lprintf("the string is not a full JSON packet, more bytes expected\n");

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
			if (doPanic) { panic("jsmn_parse"); } else return false;
		}
	} while (rc == JSMN_ERROR_NOMEM);

	//printf("using %d of %d tokens\n", rc, cfg->tok_size);
	cfg->ntok = rc;
    TMEAS(printf("cfg_parse_json: DONE json string -> json struct\n");)
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
		cfg->json = (char *) kiwi_malloc("json buf", cfg->json_buf_size);
		if (flags & CFG_COPY) {
			if (prev != NULL && prev_size != 0) {
				strcpy(cfg->json, prev);
			}
		}
		if (prev)
			kiwi_free("json buf", prev);
	}
	return cfg->json;
}

static bool _cfg_load_json(cfg_t *cfg)
{
	int i;
	size_t n;
	
    TMEAS(printf("cfg_load_json: START file=%s\n", cfg->filename);)

	struct stat st;
	scall("stat", stat(cfg->filename, &st));
	_cfg_realloc_json(cfg, st.st_size + SPACE_FOR_NULL, CFG_NONE);
	
    FILE *fp;
    scallz("_cfg_load_json fopen", (fp = fopen(cfg->filename, "r")));
    n = fread(cfg->json, 1, cfg->json_buf_size, fp);
    assert(n > 0 && n < cfg->json_buf_size);
    fclose(fp);

	// turn into a string
	cfg->json[n] = '\0';
	if (cfg->json[n-1] == '\n')
		cfg->json[n-1] = '\0';
	
    TMEAS(printf("cfg_load_json: parse\n");)
	if (_cfg_parse_json(cfg, false) == false)
		return false;

	if (0 && cfg != &cfg_dx) {
	//if (1) {
		printf("walking %s config list after load (%d tokens)...\n", cfg->filename, cfg->ntok);
		_cfg_walk(cfg, NULL, cfg_print_tok, NULL);
	}

    TMEAS(printf("cfg_load_json: DONE\n");)
	return true;
}

static void _cfg_write_file(void *param)
{
    cfg_t *cfg = (cfg_t *) FROM_VOID_PARAM(param);
	FILE *fp;

	scallz("_cfg_write_file fopen", (fp = fopen(cfg->filename, "w")));
	fprintf(fp, "%s\n", cfg->json_write);
	fclose(fp);
}

// FIXME guard better against file getting trashed
void _cfg_save_json(cfg_t *cfg, char *json)
{
	TMEAS(u4_t start = timer_ms(); printf("cfg_save_json START fn=%s json_len=%d\n", cfg->filename, strlen(cfg->json));)

    // file writes can sometimes take a long time -- use a child task and wait via NextTask()
	cfg->json_write = json;
    int status = child_task("kiwi.cfg", _cfg_write_file, POLL_MSEC(100), TO_VOID_PARAM(cfg));
    int exit_status;
    if (WIFEXITED(status) && (exit_status = WEXITSTATUS(status))) {
        printf("cfg_write_file exit_status=0x%x\n", exit_status);
    }

	// if new buffer is different update our copy
	if (!cfg->json || (cfg->json && cfg->json != json)) {
		_cfg_realloc_json(cfg, strlen(json) + SPACE_FOR_NULL, CFG_NONE);
		strcpy(cfg->json, json);
	}

    // This takes forever for a large file. But we fixed it by putting a NextTask() in jsmn_parse().
    TMEAS(u4_t split = timer_ms(); printf("cfg_save_json json string -> file save %.3f msec\n", TIME_DIFF_MS(split, start));)
    #if 1
        if ((cfg->flags & CFG_NO_UPDATE) == 0) {
            _cfg_parse_json(cfg, true);
        } else {
            cfg->flags &= ~CFG_PARSE_VALID;
        }
    #else
        _cfg_parse_json(cfg, true);
    #endif
    TMEAS(u4_t now = timer_ms(); printf("cfg_save_json DONE reparse %.3f/%.3f msec\n", TIME_DIFF_MS(now, split), TIME_DIFF_MS(now, start));)
}

void _cfg_update_json(cfg_t *cfg)
{
    if (cfg == NULL) return;

    if ((cfg->flags & CFG_NO_UPDATE) && (cfg->flags & CFG_PARSE_VALID) == 0) {
        printf("_cfg_update_json: cfg <%s> OUT-OF-DATE re-parsing\n", cfg->filename);
        _cfg_parse_json(cfg, true);
    }
}

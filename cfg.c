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
#include "misc.h"
#include "timer.h"
#include "web.h"
#include "peri.h"
#include "spi.h"
#include "coroutines.h"
#include "jsmn.h"

#define CFG_DOT_C
#include "cfg.h"
#include "dx.h"

#include <sys/types.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>
#include <signal.h>

#define CFG_FN	DIR_CFG "/" CFG_PREFIX "kiwi.json";
#define ADM_FN	DIR_CFG "/" CFG_PREFIX "admin.json"
#define DX_FN	DIR_CFG "/" CFG_PREFIX "dx.json";

#define SPACE_FOR_CLOSE_BRACE		1
#define SLEN_COMMA_2QUOTES_COLON	4
#define SLEN_COMMA_4QUOTES_COLON	6
#define JSON_FIRST_QUOTE			1
#define JSON_SEPARATING_COMMA		1

int serial_number;

void cfg_reload(bool called_from_main)
{
	cfg_init();
	admcfg_init();

	if (called_from_main) {
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
	}
	
	dx_reload();
	
	if (!called_from_main) {
		services_start(SVCS_RESTART_TRUE);
	}
}

cfg_t cfg_cfg, cfg_adm, cfg_dx;
	
char *_cfg_get_json(cfg_t *cfg, int *size)
{
	if (size) *size = cfg->json_buf_size;
	return cfg->json;
}

static int _cfg_load_json(cfg_t *cfg);
static void cfg_parse_json(cfg_t *cfg);

void _cfg_init(cfg_t *cfg)
{
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
		if (_cfg_load_json(cfg) == 0)
			panic("cfg_init json");
		cfg->init = true;
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
}

jsmntok_t *_cfg_lookup_json(cfg_t *cfg, const char *id)
{
	int i, idlen = strlen(id);
	
	jsmntok_t *jt = cfg->tokens;
	//printf("_cfg_lookup_json: key=\"%s\" %d\n", id, idlen);
	for (i=0; i < cfg->ntok; i++) {
		int n = jt->end - jt->start;
		char *s = &cfg->json[jt->start];
		if (JSMN_IS_ID(jt)) {
			//printf("key %d: %d <%.*s> cmp %d\n", i, n, n, s, strncmp(id, s, n));
			if (n == idlen && strncmp(id, s, n) == 0) {
				return jt+1;
			}
		}
		jt++;
	}
	return NULL;
}

bool _cfg_int_json(cfg_t *cfg, jsmntok_t *jt, int *num)
{
	assert(jt != NULL);
	char *s = &cfg->json[jt->start];
	if (jt->type == JSMN_PRIMITIVE && (*s == '-' || isdigit(*s))) {
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

	jsmntok_t *jt = _cfg_lookup_json(cfg, name);
	if (!jt || _cfg_int_json(cfg, jt, &num) == false) {
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

void _cfg_set_int(cfg_t *cfg, const char *name, int val, u4_t flags)
{
	int slen;
	char *s;
	jsmntok_t *jt = _cfg_lookup_json(cfg, name);

	if (flags == CFG_REMOVE) {
		if (!jt) {
			lprintf("%s: cfg_set_int(CFG_REMOVE) a parameter that doesn't exist: %s\n", cfg->filename, name);
			panic("cfg_set_int");
		}
		
		jsmntok_t *key_jt = jt-1;
		assert(JSMN_IS_ID(key_jt));
		int key_size = key_jt->end - key_jt->start;
		
		s = &cfg->json[jt->start];
		assert(jt->type == JSMN_PRIMITIVE && (isdigit(*s) || *s == '-'));
		int val_size = jt->end - jt->start;
		
		// ,"id":int or {"id":int
		//   ^start
		s = &cfg->json[key_jt->start - JSON_FIRST_QUOTE - JSON_SEPARATING_COMMA];
		slen = key_size + val_size + SLEN_COMMA_2QUOTES_COLON;	// remember (jt->end - jt->start) doesn't include first quote
		if (*s == '{') s++;		// at the beginning, slen stays the same because need to remove trailing comma
		//printf("cfg_set_int(CFG_REMOVE): %s %d %d %d <%.*s>\n", name, slen, key_size, val_size, slen, s);
		strcpy(s, s + slen);
	} else {
		if (!jt) {
			// create by appending to the end of the JSON string
			char *int_sval;
			asprintf(&int_sval, "%d", val);
			slen = strlen(name) + strlen(int_sval) + SLEN_COMMA_2QUOTES_COLON;
			assert(cfg->json_buf_size);
			_cfg_realloc_json(cfg, cfg->json_buf_size + slen, CFG_COPY);
			s = cfg->json + strlen(cfg->json) - SPACE_FOR_CLOSE_BRACE;
			bool head = (s[-1] == '{');
			sprintf(s, "%s\"%s\":%s}", head? "":",", name, int_sval);
			free(int_sval);
		} else {
			lprintf("%s: don't support cfg_set_int() update yet: %s\n", cfg->filename, name);
			panic("cfg_set_int");
		}
	}
	
	cfg_parse_json(cfg);	// must re-parse
}

bool _cfg_float_json(cfg_t *cfg, jsmntok_t *jt, double *num)
{
	assert(jt != NULL);
	char *s = &cfg->json[jt->start];
	if (jt->type == JSMN_PRIMITIVE && (*s == '-' || isdigit(*s))) {
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

	jsmntok_t *jt = _cfg_lookup_json(cfg, name);
	if (!jt || _cfg_float_json(cfg, jt, &num) == false) {
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

void _cfg_set_float(cfg_t *cfg, const char *name, double val, u4_t flags)
{
	int slen;
	char *s;
	jsmntok_t *jt = _cfg_lookup_json(cfg, name);

	if (flags == CFG_REMOVE) {
		if (!jt) {
			lprintf("%s: cfg_set_float(CFG_REMOVE) a parameter that doesn't exist: %s\n", cfg->filename, name);
			panic("cfg_set_float");
		}
		
		jsmntok_t *key_jt = jt-1;
		assert(JSMN_IS_ID(key_jt));
		int key_size = key_jt->end - key_jt->start;
		
		s = &cfg->json[jt->start];
		assert(jt->type == JSMN_PRIMITIVE && (isdigit(*s) || *s == '-' || *s == '.'));
		int val_size = jt->end - jt->start;
		
		// ,"id":float or {"id":float
		//   ^start
		s = &cfg->json[key_jt->start - JSON_FIRST_QUOTE - JSON_SEPARATING_COMMA];
		slen = key_size + val_size + SLEN_COMMA_2QUOTES_COLON;	// remember (jt->end - jt->start) doesn't include first quote
		if (*s == '{') s++;		// at the beginning, slen stays the same because need to remove trailing comma
		//printf("cfg_set_float(CFG_REMOVE): %s %d %d %d <%.*s>\n", name, slen, key_size, val_size, slen, s);
		strcpy(s, s + slen);
	} else {
		if (!jt) {
			// create by appending to the end of the JSON string
			char *float_sval;
			asprintf(&float_sval, "%g", val);
			slen = strlen(name) + strlen(float_sval) + SLEN_COMMA_2QUOTES_COLON;
			assert(cfg->json_buf_size);
			_cfg_realloc_json(cfg, cfg->json_buf_size + slen, CFG_COPY);
			s = cfg->json + strlen(cfg->json) - SPACE_FOR_CLOSE_BRACE;
			bool head = (s[-1] == '{');
			sprintf(s, "%s\"%s\":%s}", head? "":",", name, float_sval);
			free(float_sval);
		} else {
			lprintf("%s: don't support cfg_set_float() update yet: %s\n", cfg->filename, name);
			panic("cfg_set_float");
		}
	}
	
	cfg_parse_json(cfg);	// must re-parse
}

int _cfg_bool(cfg_t *cfg, const char *name, bool *error, u4_t flags)
{
	int num = 0;
	bool err = false;

	jsmntok_t *jt = _cfg_lookup_json(cfg, name);
	char *s = jt? &cfg->json[jt->start] : NULL;
	if (!(jt && jt->type == JSMN_PRIMITIVE && (*s == 't' || *s == 'f'))) {
		err = true;
	} else {
		num = (*s == 't')? 1:0;
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

void _cfg_set_bool(cfg_t *cfg, const char *name, u4_t val)
{
	int slen;
	char *s;
	jsmntok_t *jt = _cfg_lookup_json(cfg, name);
	
	if (val == CFG_REMOVE) {
		if (!jt) {
			lprintf("%s: cfg_set_bool(CFG_REMOVE) a parameter that doesn't exist: %s\n", cfg->filename, name);
			panic("cfg_set_bool");
		}
		
		jsmntok_t *key_jt = jt-1;
		assert(JSMN_IS_ID(key_jt));
		int key_size = key_jt->end - key_jt->start;
		
		s = &cfg->json[jt->start];
		assert(jt->type == JSMN_PRIMITIVE && (*s == 't' || *s == 'f'));
		int val_size = jt->end - jt->start;
		
		// ,"id":t/f or {"id":t/f
		//   ^start
		s = &cfg->json[key_jt->start - JSON_FIRST_QUOTE - JSON_SEPARATING_COMMA];
		slen = key_size + val_size + SLEN_COMMA_2QUOTES_COLON;	// remember (jt->end - jt->start) doesn't include first quote
		if (*s == '{') s++;		// at the beginning, slen stays the same because need to remove trailing comma
		//printf("cfg_set_bool(CFG_REMOVE): %s %d %d %d <%.*s>\n", name, slen, key_size, val_size, slen, s);
		strcpy(s, s + slen);
	} else {
		bool bool_val = val? true : false;
		if (!jt) {
			// create by appending to the end of the JSON string
			const char *bool_sval = bool_val? "true" : "false";
			slen = strlen(name) + strlen(bool_sval) + SLEN_COMMA_2QUOTES_COLON;
			assert(cfg->json_buf_size);
			_cfg_realloc_json(cfg, cfg->json_buf_size + slen, CFG_COPY);
			s = cfg->json + strlen(cfg->json) - SPACE_FOR_CLOSE_BRACE;
			bool head = (s[-1] == '{');
			sprintf(s, "%s\"%s\":%s}", head? "":",", name, bool_sval);
		} else {
			lprintf("%s: don't support cfg_set_bool() update yet: %s\n", cfg->filename, name);
			panic("cfg_set_bool");
		}
	}
	
	cfg_parse_json(cfg);	// must re-parse
}

bool _cfg_type_json(cfg_t *cfg, jsmntype_t jt_type, jsmntok_t *jt, const char **str)
{
	assert(jt != NULL);
	char *s = &cfg->json[jt->start];
	if (jt->type == jt_type) {
		int n = jt->end - jt->start;
		*str = (const char *) malloc(n + SPACE_FOR_NULL);
		mg_url_decode((const char *) s, n, (char *) *str, n + SPACE_FOR_NULL, 0);
		return true;
	} else {
		return false;
	}
}

void _cfg_free(cfg_t *cfg, const char *str)
{
	if (str != NULL) free((char *) str);
}

const char *_cfg_string(cfg_t *cfg, const char *name, bool *error, u4_t flags)
{
	const char *str = NULL;
	bool err = false;

	jsmntok_t *jt = _cfg_lookup_json(cfg, name);
	if (!jt || _cfg_type_json(cfg, JSMN_STRING, jt, &str) == false) {
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

void _cfg_set_string(cfg_t *cfg, const char *name, const char *val)
{
	int slen;
	char *s;
	jsmntok_t *jt = _cfg_lookup_json(cfg, name);
	
	if (val == NULL) {
		if (!jt) {
			lprintf("%s: cfg_set_string(CFG_REMOVE) a parameter that doesn't exist: %s\n", cfg->filename, name);
			panic("cfg_set_string");
		}
		
		jsmntok_t *key_jt = jt-1;
		assert(JSMN_IS_ID(key_jt));
		int key_size = key_jt->end - key_jt->start;
		
		s = &cfg->json[jt->start];
		assert(jt->type == JSMN_STRING && s[-1] == '\"');
		int val_size = jt->end - jt->start;
		
		// ,"id":"string" or {"id":"string"
		//   ^start
		s = &cfg->json[key_jt->start - JSON_FIRST_QUOTE - JSON_SEPARATING_COMMA];
		slen = key_size + val_size + SLEN_COMMA_4QUOTES_COLON;	// remember (jt->end - jt->start) doesn't include first quote
		if (*s == '{') s++;		// at the beginning, slen stays the same because need to remove trailing comma
		//printf("cfg_set_string(CFG_REMOVE): %s %d %d %d <%.*s>\n", name, slen, key_size, val_size, slen, s);
		strcpy(s, s + slen);
	} else {
		if (!jt) {
			// create by appending to the end of the JSON string
			slen = strlen(name) + strlen(val) + SLEN_COMMA_4QUOTES_COLON;
			assert(cfg->json_buf_size);
			_cfg_realloc_json(cfg, cfg->json_buf_size + slen, CFG_COPY);
			s = cfg->json + strlen(cfg->json) - SPACE_FOR_CLOSE_BRACE;
			bool head = (s[-1] == '{');
			sprintf(s, "%s\"%s\":\"%s\"}", head? "":",", name, val);
		} else {
			lprintf("%s: don't support cfg_set_string() update yet: %s\n", cfg->filename, name);
			panic("cfg_set_string");
		}
	}
	
	cfg_parse_json(cfg);	// must re-parse
}


const char *_cfg_object(cfg_t *cfg, const char *name, bool *error, u4_t flags)
{
	const char *obj = NULL;
	bool err = false;

	jsmntok_t *jt = _cfg_lookup_json(cfg, name);
	if (!jt || _cfg_type_json(cfg, JSMN_OBJECT, jt, &obj) == false) {
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

void _cfg_set_object(cfg_t *cfg, const char *name, const char *val)
{
	int slen;
	char *s;
	jsmntok_t *jt = _cfg_lookup_json(cfg, name);
	
	if (val == NULL) {
		if (!jt) {
			lprintf("%s: cfg_set_object(CFG_REMOVE) a parameter that doesn't exist: %s\n", cfg->filename, name);
			panic("cfg_set_object");
		}
		
		jsmntok_t *key_jt = jt-1;
		assert(JSMN_IS_ID(key_jt));
		int key_size = key_jt->end - key_jt->start;
		
		s = &cfg->json[jt->start];
		assert(jt->type == JSMN_OBJECT && *s == '{');
		int val_size = jt->end - jt->start;
		
		// ,"id":{...} or {"id":{...}
		//   ^start
		s = &cfg->json[key_jt->start - JSON_FIRST_QUOTE - JSON_SEPARATING_COMMA];
		slen = key_size + val_size + SLEN_COMMA_2QUOTES_COLON;	// remember (jt->end - jt->start) doesn't include first quote
		if (*s == '{') s++;		// at the beginning, slen stays the same because need to remove trailing comma
		//printf("cfg_set_object(CFG_REMOVE): %s %d %d %d <%.*s>\n", name, slen, key_size, val_size, slen, s);
		strcpy(s, s + slen);
	} else {
		if (!jt) {
			// create by appending to the end of the JSON string
			slen = strlen(name) + strlen(val) + SLEN_COMMA_2QUOTES_COLON;
			assert(cfg->json_buf_size);
			_cfg_realloc_json(cfg, cfg->json_buf_size + slen, CFG_COPY);
			s = cfg->json + strlen(cfg->json) - SPACE_FOR_CLOSE_BRACE;
			bool head = (s[-1] == '{');
			sprintf(s, "%s\"%s\":%s}", head? "":",", name, val);
		} else {
			lprintf("%s: don't support cfg_set_object() update yet: %s\n", cfg->filename, name);
			panic("cfg_set_object");
		}
	}
	
	cfg_parse_json(cfg);	// must re-parse
}


static const char *jsmntype_s[] = {
	"undef", "obj", "array", "string", "prim"
};

void cfg_print_tok(cfg_t *cfg, jsmntok_t *jt, int seq, int hit, int lvl, int rem)
{
	int n;
	char *s = &cfg->json[jt->start];
	printf("%4d: %d-%02d%s %4d ", seq, lvl, rem, (hit == lvl)? "*":" ", jt->start);

	switch (jt->type) {
	case JSMN_OBJECT:
	case JSMN_ARRAY:
		if (seq == -1)
			// virtual token
			printf("%6s #%02d '%c'\n", jsmntype_s[jt->type], jt->size, (jt->type == JSMN_OBJECT)? '}':']');
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
}

void _cfg_walk(cfg_t *cfg, const char *id, cfg_walk_cb_t cb)
{
	int i, n, idlen = id? strlen(id):0;
	jsmntok_t *jt = cfg->tokens;
	int hit = -1;
	int lvl = 0, remstk[32], _lvl = 0, _rem;
	memset(remstk, 0, sizeof(remstk));
	jsmntok_t *remjt[32];
	
	for (i=0; i < cfg->ntok; i++) {
		char *s = &cfg->json[jt->start];
		_lvl = lvl; _rem = remstk[lvl];

		if (lvl && (jt->type != JSMN_STRING || JSMN_IS_STRING(jt))) {
			remstk[lvl]--;
		}

		if (jt->type == JSMN_OBJECT || jt->type == JSMN_ARRAY) {
			lvl++;
			if (!id || _lvl == hit)
				cb(cfg, jt, i, hit, _lvl, _rem);
			remstk[lvl] = jt->size;
			remjt[lvl] = jt;
		} else {
			if (!id || _lvl == hit)
				cb(cfg, jt, i, hit, _lvl, _rem);
		}

		// check for optional id match
		if (hit == -1 && JSMN_IS_ID(jt)) {
			n = jt->end - jt->start;
			if (id && n == idlen && strncmp(s, id, n) == 0)
				hit = lvl+1;
		}

		while (lvl && remstk[lvl] == 0) {
			cb(cfg, remjt[lvl], -1, hit, lvl, 0);	// virtual-tokens to close objects and arrays
			lvl--;
			hit = -1;	// clear id match once level is complete
		}

		jt++;
	}
}

static void cfg_parse_json(cfg_t *cfg)
{
	if (cfg->tok_size == 0)
		cfg->tok_size = 64;
	
	int slen = strlen(cfg->json);
	assert(cfg->json_buf_size >= slen + SPACE_FOR_NULL);
	jsmn_parser parser;
	
	int rc;
	do {
		if (cfg->tokens)
			kiwi_free("cfg tokens", cfg->tokens);
		
		cfg->tokens = (jsmntok_t *) kiwi_malloc("cfg tokens", sizeof(jsmntok_t) * cfg->tok_size);

		jsmn_init(&parser);
		if ((rc = jsmn_parse(&parser, cfg->json, slen, cfg->tokens, cfg->tok_size)) >= 0)
			break;
		
		if (rc == JSMN_ERROR_NOMEM) {
			//printf("not enough tokens (%d) were provided\n", cfg->tok_size);
			cfg->tok_size *= 2;		// keep going until we hit safety limit in kiwi_malloc()
		} else {
			lprintf("cfg_parse_json: file %s line=%d pos=%d tok=%d\n",
				cfg->filename, parser.line, parser.pos, parser.toknext);
			if (rc == JSMN_ERROR_INVAL)
				lprintf("invalid character inside JSON string\n");
			else
			if (rc == JSMN_ERROR_PART)
				lprintf("the string is not a full JSON packet, more bytes expected\n");
			printf("%s\n", cfg->json);
			panic("jsmn_parse");
		}
	} while (rc == JSMN_ERROR_NOMEM);

	//printf("using %d of %d tokens\n", rc, cfg->tok_size);
	cfg->ntok = rc;
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

static int _cfg_load_json(cfg_t *cfg)
{
	int i;
	FILE *fp;
	size_t n;
	
	if ((fp = fopen(cfg->filename, "r")) == NULL)
		return 0;
	
	struct stat st;
	scall("stat", stat(cfg->filename, &st));
	_cfg_realloc_json(cfg, st.st_size + SPACE_FOR_NULL, CFG_NONE);
	
	n = fread(cfg->json, 1, cfg->json_buf_size, fp);
	assert(n > 0 && n < cfg->json_buf_size);
	fclose(fp);

	// turn into a string
	cfg->json[n] = '\0';
	if (cfg->json[n-1] == '\n')
		cfg->json[n-1] = '\0';
	
	cfg_parse_json(cfg);

	if (0 && cfg != &cfg_dx) {
	//if (1) {
		printf("walking %s config list after load (%d tokens)...\n", cfg->filename, cfg->ntok);
		_cfg_walk(cfg, NULL, cfg_print_tok);
	}

	return 1;
}

// FIXME guard better against file getting trashed
void _cfg_save_json(cfg_t *cfg, char *json)
{
	FILE *fp;

	//printf("_cfg_save_json fn=%s json=%s\n", cfg->filename, json);
	scallz("_cfg_save_json fopen", (fp = fopen(cfg->filename, "w")));
	fprintf(fp, "%s\n", json);
	fclose(fp);
	
	// if new buffer is different update our copy
	if (!cfg->json || (cfg->json && cfg->json != json)) {
		_cfg_realloc_json(cfg, strlen(json) + SPACE_FOR_NULL, CFG_NONE);
		strcpy(cfg->json, json);
	}

	cfg_parse_json(cfg);
}

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

int serial_number;
bool reload_kiwi_cfg;

void cfg_reload(bool called_from_main)
{
	cfg_init();

	// yes, race with reload_kiwi_cfg since cfg_reload() is async, but low probability
	reload_kiwi_cfg = true;
	
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

cfg_t cfg_cfg, cfg_dx;
	
static void cfg_error(config_t *config, const char *msg)
{
	lprintf("%s:%d - %s\n", config_error_file(config), config_error_line(config), config_error_text(config));
	config_destroy(config);
	panic(msg);
}

char *_cfg_get_json(cfg_t *cfg, int *size)
{
	if (!cfg->use_json)
		return NULL;
	if (size) *size = cfg->json_size;
	return cfg->json;
}

static int _cfg_load_json(cfg_t *cfg);

void _cfg_init(cfg_t *cfg)
{
	// special transition to JSON files
	if (cfg == &cfg_cfg) {
		cfg->filename_cfg = DIR_CFG "/" CFG_PREFIX "kiwi.cfg";
		cfg->filename_json = DIR_CFG "/" CFG_PREFIX "kiwi.json";
	} else {
		cfg->filename_cfg = DIR_CFG "/" CFG_PREFIX "dx.cfg";
		cfg->filename_json = DIR_CFG "/" CFG_PREFIX "dx.json";
	}
	
	if (!cfg->init) {
		struct stat st;

		if (cfg == &cfg_cfg) {
			// cfg: .cfg before .json
			if (stat(cfg->filename_cfg, &st) == 0) {
				cfg->use_json = false;
				cfg->filename = cfg->filename_cfg;
			} else {
				cfg->use_json = true;
				cfg->filename = cfg->filename_json;
				if (_cfg_load_json(cfg) == 0)
					panic("cfg_init json");
			}
		} else {
			// dxcfg: .json before .cfg
			if (stat(cfg->filename_json, &st) == 0) {
				cfg->use_json = true;
				cfg->filename = cfg->filename_json;
				if (_cfg_load_json(cfg) == 0)
					panic("cfg_init json");
			} else {
				cfg->use_json = false;
				cfg->filename = cfg->filename_cfg;
			}
		}
	
		cfg->init = true;
	}
	
	lprintf("reading configuration from file %s\n", cfg->filename);

	if (cfg->use_json)
		return;


	// libconfig of .cfg files
	
	if (cfg->config_init) {
		config_destroy(&cfg->config);
	}
	
	config_init(&cfg->config);

	if (!config_read_file(&cfg->config, cfg->filename)) {
		lprintf("check that config file is installed in %s\n", cfg->filename);
		cfg_error(&cfg->config, cfg->filename);
	}
	
	cfg->root = config_root_setting(&cfg->config);
	cfg->node = cfg->root;
	cfg->node_name = "root";
	cfg->config_init = true;
}

// change current node, absolute or relative path name
int _cfg_node(cfg_t *cfg, const char *path, u4_t flags)
{
	config_setting_t *node;
	
	node = (flags & CFG_ABS)? config_lookup(&cfg->config, path) : config_lookup_from(cfg->node, path);
	if (!node) return false;
	cfg->node = node;
	return true;
}

static config_setting_t *_cfg_member(cfg_t *cfg, const char *name,  int type)
{
	config_setting_t *node = cfg->node, *set;
	if (!(set = config_setting_get_member(node, name))) {
		if (!(set = config_setting_add(node, name, type))) {
			lprintf("could not add member to %s: %s\n", cfg->node_name, name);
			panic("_cfg_member");
		}
	}
	return set;
}

jsmntok_t *_cfg_lookup_json(cfg_t *cfg, const char *id)
{
	int i, idlen = strlen(id);
	
	jsmntok_t *jt = cfg->tokens;
	//printf("_cfg_lookup_json: key=\"%s\" %d\n", id, idlen);
	for (i=0; i < cfg->ntok; i++) {
		int n = jt->end - jt->start;
		char *s = &cfg->json[jt->start];
		if (jt->type == JSMN_STRING && jt->size == 1) {
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

int _cfg_int(cfg_t *cfg, const char *name, int *val, u4_t flags)
{
	int num;
	bool err = false;

	if (cfg->use_json) {
		jsmntok_t *jt = _cfg_lookup_json(cfg, name);
		if (!jt || _cfg_int_json(cfg, jt, &num) == false) {
			err = true;
		}
	} else {
		if (!config_lookup_int(&cfg->config, name, &num)) {
			if (config_error_line(&cfg->config)) cfg_error(&cfg->config, "cfg_int");
			err = true;
		}
	}
	if (err) {
		if (!(flags & CFG_REQUIRED)) return 0;
		lprintf("%s: required parameter not found: %s\n", cfg->filename, name);
		panic("cfg_int");
	}
	
	if (flags & CFG_PRINT) lprintf("CFG read %s: %s = %d\n", cfg->filename, name, num);
	if (val) *val = num;
	return num;
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

double _cfg_float(cfg_t *cfg, const char *name, double *val, u4_t flags)
{
	double num;
	bool err = false;

	if (cfg->use_json) {
		jsmntok_t *jt = _cfg_lookup_json(cfg, name);
		if (!jt || _cfg_float_json(cfg, jt, &num) == false) {
			err = true;
		}
	} else {
		if (!config_lookup_float(&cfg->config, name, &num)) {
			if (config_error_line(&cfg->config)) cfg_error(&cfg->config, "cfg_float");
			err = true;
		}
	}
	if (err) {
		if (!(flags & CFG_REQUIRED)) return 0;
		lprintf("%s: required parameter not found: %s\n", cfg->filename, name);
		panic("cfg_float");
	}
	
	if (flags & CFG_PRINT) lprintf("CFG read %s: %s = %f\n", cfg->filename, name, num);
	if (val) *val = num;
	return num;
}

int _cfg_bool(cfg_t *cfg, const char *name, int *val, u4_t flags)
{
	int num;
	bool err = false;

	if (cfg->use_json) {
		jsmntok_t *jt = _cfg_lookup_json(cfg, name);
		char *s = jt? &cfg->json[jt->start] : NULL;
		if (!(jt && jt->type == JSMN_PRIMITIVE && (*s == 't' || *s == 'f'))) {
			err = true;
		} else {
			num = (*s == 't')? 1:0;
		}
	} else {
		if (!config_lookup_bool(&cfg->config, name, &num)) {
			if (config_error_line(&cfg->config)) cfg_error(&cfg->config, "cfg_bool");
			err = true;
		}
	}
	if (err) {
		if (!(flags & CFG_REQUIRED)) return NOT_FOUND;
		
		// transitional hack -- these are missing in old kiwi.cfg, assume they are true
		if (cfg == &cfg_cfg && !cfg->use_json && (
			strcmp(name, "update_check") == 0 ||
			strcmp(name, "update_install") == 0 ||
			strcmp(name, "user_auto_login") == 0 ||
			strcmp(name, "admin_auto_login") == 0
		)) {
			num = 1;
		} else {
			lprintf("%s: required parameter not found: %s\n", cfg->filename, name);
			panic("cfg_bool");
		}
	}

	num = num? true : false;
	if (flags & CFG_PRINT) lprintf("CFG read %s: %s = %s\n", cfg->filename, name, num? "true":"false");
	if (val) *val = num;
	return num;
}

bool _cfg_string_json(cfg_t *cfg, jsmntok_t *jt, const char **str)
{
	assert(jt != NULL);
	char *s = &cfg->json[jt->start];
	if (jt->type == JSMN_STRING) {
		int n = jt->end - jt->start;
		*str = (const char *) malloc(n+1);
		mg_url_decode((const char *) s, n, (char *) *str, n+1, 0);
		return true;
	} else {
		return false;
	}
}

void _cfg_string_free(cfg_t *cfg, const char *str)
{
	if (cfg->use_json) {
		free((char *) str);
	}
}

const char *_cfg_string(cfg_t *cfg, const char *name, const char **val, u4_t flags)
{
	const char *str;
	bool err = false;

	if (cfg->use_json) {
		jsmntok_t *jt = _cfg_lookup_json(cfg, name);
		if (!jt || _cfg_string_json(cfg, jt, &str) == false) {
			err = true;
		}
	} else {
		if (!config_lookup_string(&cfg->config, name, &str)) {
			if (config_error_line(&cfg->config)) cfg_error(&cfg->config, "cfg_string");
			err = true;
		}
	}
	if (err) {
		if (!(flags & CFG_REQUIRED)) return NULL;
		lprintf("%s: required parameter not found: %s\n", cfg->filename, name);
		panic("cfg_string");
	}

	if (flags & CFG_PRINT) lprintf("CFG read %s: %s = \"%s\"\n", cfg->filename, name, str);
	if (val) *val = str;
	return str;
}


config_setting_t *_cfg_lookup(cfg_t *cfg, const char *path, u4_t flags)
{
	config_setting_t *setting;
	if ((setting = config_lookup(&cfg->config, path)) == NULL) {
		if (config_error_line(&cfg->config)) cfg_error(&cfg->config, "cfg_lookup");
		if (!(flags & CFG_REQUIRED)) return NULL;
		lprintf("%s: lookup parameter not found: %s\n", cfg->filename, path);
		panic("cfg_lookup");
	}
	return setting;
}

static const char *jsmntype_s[] = {
	"undef", "obj", "array", "string", "prim"
};

void cfg_print_tok(cfg_t *cfg, jsmntok_t *jt, int seq, int hit, int lvl, int rem)
{
	int n;
	char *s = &cfg->json[jt->start];
	printf("%4d: %d-%02d%s ", seq, lvl, rem, (hit == lvl)? "*":" ");

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
		if (jt->size == 1) {
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
	
	if (!cfg->use_json)
		return;
	
	for (i=0; i < cfg->ntok; i++) {
		char *s = &cfg->json[jt->start];
		_lvl = lvl; _rem = remstk[lvl];

		if (lvl && (jt->type != JSMN_STRING || (jt->type == JSMN_STRING && jt->size == 0))) {
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
		if (hit == -1 && jt->type == JSMN_STRING && jt->size == 1) {
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
			panic("jsmn_parse");
		}
	} while(rc == JSMN_ERROR_NOMEM);

	//printf("using %d of %d tokens\n", rc, cfg->tok_size);
	cfg->ntok = rc;
}

char *_cfg_realloc_json(cfg_t *cfg, int size)
{
	assert(size > 0);
	if (cfg->json_size >= size) {
		assert(cfg->json);
	} else {
		if (cfg->json)
			kiwi_free("json buf", cfg->json);
		cfg->json_size = size;
		cfg->json = (char *) kiwi_malloc("json buf", cfg->json_size);
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
	_cfg_realloc_json(cfg, st.st_size+1);
	
	n = fread(cfg->json, 1, cfg->json_size, fp);
	assert(n > 0 && n < cfg->json_size);
	fclose(fp);

	// turn into a string
	cfg->json[n] = '\0';
	if (cfg->json[n-1] == '\n')
		cfg->json[n-1] = '\0';
	
	cfg_parse_json(cfg);
	
	if (0 && cfg == &cfg_cfg) {
		printf("walking config list...\n");
		cfg_walk(NULL, cfg_print_tok);
	}

	return 1;
}

// FIXME guard better against file getting trashed
void _cfg_save_json(cfg_t *cfg, char *json)
{
	FILE *fp;
	
	// dx conversion: first time saving as json switch to using json thereafter
	if (cfg == &cfg_dx && !cfg->use_json) {
		cfg->filename = cfg->filename_json;
		cfg->use_json = true;
	}
	
	assert(cfg->use_json);
	assert(cfg->filename == cfg->filename_json);
	
	scallz("_cfg_save_json fopen", (fp = fopen(cfg->filename, "w")));
	fprintf(fp, "%s\n", json);
	fclose(fp);
	
	// if new buffer is different update our copy
	if (!cfg->json || (cfg->json && cfg->json != json)) {
		_cfg_realloc_json(cfg, strlen(json)+1);
		strcpy(cfg->json, json);
	}

	cfg_parse_json(cfg);

	if (0 && cfg == &cfg_dx) {
		printf("walking DX list...\n");
		dxcfg_walk(NULL, cfg_print_tok);
	}
}

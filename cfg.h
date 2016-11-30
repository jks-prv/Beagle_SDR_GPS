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

#pragma once

#include "types.h"
#include "kiwi.h"
#include "jsmn.h"

// configuration

struct cfg_t {
	bool init;
	const char *filename;

	char *json;
	int json_size;		// includes terminating null

	int tok_size, ntok;
	jsmntok_t *tokens;
};

extern cfg_t cfg_cfg, cfg_pwd, cfg_dx;

#define CFG_NONE		0x00
#define CFG_OPTIONAL	0x00
#define CFG_PRINT		0x01
#define CFG_REQUIRED	0x02
#define CFG_ABS			0x04
#define CFG_REL			0x08
#define CFG_REMOVE		0x10
#define CFG_COPY		0x20

#define cfg_init()						_cfg_init(&cfg_cfg)
#define	cfg_get_json(size)				_cfg_get_json(&cfg_cfg, size)
#define	cfg_realloc_json(size, flags)	_cfg_realloc_json(&cfg_cfg, size, flags)
#define cfg_save_json(json)				_cfg_save_json(&cfg_cfg, json)
#define cfg_walk(id, cb)				_cfg_walk(&cfg_cfg, id, cb)

#define cfg_int(name, val, flags)		_cfg_int(&cfg_cfg, name, val, flags)
#define cfg_float(name, val, flags)		_cfg_float(&cfg_cfg, name, val, flags)
#define cfg_bool(name, val, flags)		_cfg_bool(&cfg_cfg, name, val, flags)
#define cfg_set_bool(name, val)			_cfg_set_bool(&cfg_cfg, name, (u4_t) val)
#define cfg_string(name, val, flags)	_cfg_string(&cfg_cfg, name, val, flags)
#define cfg_string_free(val)			_cfg_string_free(&cfg_cfg, val)
#define cfg_set_string(name, val)		_cfg_set_string(&cfg_cfg, name, val)

#define cfg_int_json(jt, val)			_cfg_int_json(&cfg_cfg, jt, val)
#define cfg_float_json(jt, val)			_cfg_float_json(&cfg_cfg, jt, val)
#define cfg_bool_json(jt, val)			_cfg_bool_json(&cfg_cfg, jt, val)
#define cfg_string_json(jt, val)		_cfg_string_json(&cfg_cfg, jt, val)
#define cfg_lookup_json(id)				_cfg_lookup_json(&cfg_cfg, id)

#define pwdcfg_init()					_cfg_init(&cfg_pwd)
#define	pwdcfg_get_json(size)			_cfg_get_json(&cfg_pwd, size)
#define	pwdcfg_realloc_json(size, flags) _cfg_realloc_json(&cfg_pwd, size, flags)
#define pwdcfg_save_json(json)			_cfg_save_json(&cfg_pwd, json)

#define pwdcfg_bool(name, val, flags)	_cfg_bool(&cfg_pwd, name, val, flags)
#define pwdcfg_set_bool(name, val)		_cfg_set_bool(&cfg_pwd, name, (u4_t) val)
#define pwdcfg_string(name, val, flags)	_cfg_string(&cfg_pwd, name, val, flags)
#define pwdcfg_string_free(val)			_cfg_string_free(&cfg_pwd, val)
#define pwdcfg_set_string(name, val)	_cfg_set_string(&cfg_pwd, name, val)

#define dxcfg_init()					_cfg_init(&cfg_dx)
#define	dxcfg_get_json(size)			_cfg_get_json(&cfg_dx, size)
#define	dxcfg_realloc_json(size, flags)	_cfg_realloc_json(&cfg_dx, size, flags)
#define dxcfg_save_json(json)			_cfg_save_json(&cfg_dx, json)
#define dxcfg_walk(id, cb)				_cfg_walk(&cfg_dx, id, cb)

#define dxcfg_int(name, val, flags)		_cfg_int(&cfg_dx, name, val, flags)
#define dxcfg_float(name, val, flags)	_cfg_float(&cfg_dx, name, val, flags)
#define dxcfg_bool(name, val, flags)	_cfg_bool(&cfg_dx, name, val, flags)
#define dxcfg_string(name, val, flags)	_cfg_string(&cfg_dx, name, val, flags)
#define dxcfg_string_free(val)			_cfg_string_free(&cfg_dx, val)

#define dxcfg_int_json(jt, val)			_cfg_int_json(&cfg_dx, jt, val)
#define dxcfg_float_json(jt, val)		_cfg_float_json(&cfg_dx, jt, val)
#define dxcfg_bool_json(jt, val)		_cfg_bool_json(&cfg_dx, jt, val)
#define dxcfg_string_json(jt, val)		_cfg_string_json(&cfg_dx, jt, val)
#define dxcfg_lookup_json(id)			_cfg_lookup_json(&cfg_dx, id)

#define	CALLED_FROM_MAIN		true
#define	NOT_CALLED_FROM_MAIN	false
void cfg_reload(bool called_from_main);

void _cfg_init(cfg_t *cfg);
void _cfg_save_json(cfg_t *cfg, char *json);

int _cfg_int(cfg_t *cfg, const char *name, int *val, u4_t flags);
double _cfg_float(cfg_t *cfg, const char *name, double *val, u4_t flags);
int _cfg_bool(cfg_t *cfg, const char *name, int *val, u4_t flags);
void _cfg_set_bool(cfg_t *cfg, const char *name, u4_t val);
const char *_cfg_string(cfg_t *cfg, const char *name, const char **val, u4_t flags);
void _cfg_string_free(cfg_t *cfg, const char *str);
void _cfg_set_string(cfg_t *cfg, const char *name, const char *val);

char *_cfg_get_json(cfg_t *cfg, int *size);
char *_cfg_realloc_json(cfg_t *cfg, int size, u4_t flags);

void cfg_print_tok(cfg_t *cfg, jsmntok_t *jt, int seq, int hit, int lvl, int rem);
typedef void (*cfg_walk_cb_t)(cfg_t *cfg, jsmntok_t *, int, int, int, int);
void _cfg_walk(cfg_t *cfg, const char *id, cfg_walk_cb_t cb);

bool _cfg_int_json(cfg_t *cfg, jsmntok_t *jt, int *num);
bool _cfg_float_json(cfg_t *cfg, jsmntok_t *jt, double *num);
bool _cfg_string_json(cfg_t *cfg, jsmntok_t *jt, const char **str);
jsmntok_t *_cfg_lookup_json(cfg_t *cfg, const char *id);

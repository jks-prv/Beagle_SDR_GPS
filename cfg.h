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
	int json_buf_size;		// includes terminating null

	int tok_size, ntok;
	jsmntok_t *tokens;
};

extern cfg_t cfg_cfg, cfg_adm, cfg_dx;

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

#define cfg_int(name, err, flags)		_cfg_int(&cfg_cfg, name, err, flags)
#define cfg_set_int(name, val)			_cfg_set_int(&cfg_cfg, name, val, CFG_NONE)
#define cfg_rem_int(name)				_cfg_set_int(&cfg_cfg, name, 0, CFG_REMOVE)
#define cfg_float(name, err, flags)		_cfg_float(&cfg_cfg, name, err, flags)
#define cfg_set_float(name, val)		_cfg_set_float(&cfg_cfg, name, val, CFG_NONE)
#define cfg_rem_float(name)				_cfg_set_float(&cfg_cfg, name, 0, CFG_REMOVE)
#define cfg_bool(name, err, flags)		_cfg_bool(&cfg_cfg, name, err, flags)
#define cfg_set_bool(name, val)			_cfg_set_bool(&cfg_cfg, name, (u4_t) val)
#define cfg_rem_bool(name)				_cfg_set_bool(&cfg_cfg, name, CFG_REMOVE)
#define cfg_string(name, err, flags)	_cfg_string(&cfg_cfg, name, err, flags)
#define cfg_string_free(val)			_cfg_free(&cfg_cfg, val)
#define cfg_set_string(name, val)		_cfg_set_string(&cfg_cfg, name, val)
#define cfg_rem_string(name)			_cfg_set_string(&cfg_cfg, name, NULL)
#define cfg_object(name, err, flags)	_cfg_object(&cfg_cfg, name, err, flags)
#define cfg_object_free(val)			_cfg_free(&cfg_cfg, val)
#define cfg_set_object(name, val)		_cfg_set_object(&cfg_cfg, name, val)
#define cfg_rem_object(name)			_cfg_set_object(&cfg_cfg, name, NULL)

#define cfg_int_json(jt, val)			_cfg_int_json(&cfg_cfg, jt, val)
#define cfg_float_json(jt, val)			_cfg_float_json(&cfg_cfg, jt, val)
#define cfg_bool_json(jt, val)			_cfg_bool_json(&cfg_cfg, jt, val)
#define cfg_string_json(jt, val)		_cfg_type_json(&cfg_cfg, JSMN_STRING, jt, val)
#define cfg_lookup_json(id)				_cfg_lookup_json(&cfg_cfg, id)

#define admcfg_init()					_cfg_init(&cfg_adm)
#define	admcfg_get_json(size)			_cfg_get_json(&cfg_adm, size)
#define	admcfg_realloc_json(size, flags) _cfg_realloc_json(&cfg_adm, size, flags)
#define admcfg_save_json(json)			_cfg_save_json(&cfg_adm, json)
#define admcfg_walk(id, cb)				_cfg_walk(&cfg_adm, id, cb)

#define admcfg_int(name, err, flags)	_cfg_int(&cfg_adm, name, err, flags)
#define admcfg_set_int(name, val)		_cfg_set_int(&cfg_adm, name, val, CFG_NONE)
#define admcfg_rem_int(name)			_cfg_set_int(&cfg_adm, name, 0, CFG_REMOVE)
#define admcfg_float(name, err, flags)	_cfg_float(&cfg_adm, name, err, flags)
#define admcfg_bool(name, err, flags)	_cfg_bool(&cfg_adm, name, err, flags)
#define admcfg_set_bool(name, val)		_cfg_set_bool(&cfg_adm, name, (u4_t) val)
#define admcfg_rem_bool(name)			_cfg_set_bool(&cfg_adm, name, CFG_REMOVE)
#define admcfg_string(name, err, flags)	_cfg_string(&cfg_adm, name, err, flags)
#define admcfg_string_free(val)			_cfg_free(&cfg_adm, val)
#define admcfg_set_string(name, val)	_cfg_set_string(&cfg_adm, name, val)
#define admcfg_rem_string(name)			_cfg_set_string(&cfg_adm, name, NULL)
#define admcfg_object(name, err, flags)	_cfg_object(&cfg_adm, name, err, flags)
#define admcfg_object_free(val)			_cfg_free(&cfg_adm, val)
#define admcfg_set_object(name, val)	_cfg_set_object(&cfg_adm, name, val)
#define admcfg_rem_object(name)			_cfg_set_object(&cfg_adm, name, NULL)

#define dxcfg_init()					_cfg_init(&cfg_dx)
#define	dxcfg_get_json(size)			_cfg_get_json(&cfg_dx, size)
#define	dxcfg_realloc_json(size, flags)	_cfg_realloc_json(&cfg_dx, size, flags)
#define dxcfg_save_json(json)			_cfg_save_json(&cfg_dx, json)
#define dxcfg_walk(id, cb)				_cfg_walk(&cfg_dx, id, cb)

#define dxcfg_int(name, err, flags)		_cfg_int(&cfg_dx, name, err, flags)
#define dxcfg_float(name, err, flags)	_cfg_float(&cfg_dx, name, err, flags)
#define dxcfg_bool(name, err, flags)	_cfg_bool(&cfg_dx, name, err, flags)
#define dxcfg_string(name, err, flags)	_cfg_string(&cfg_dx, name, err, flags)
#define dxcfg_string_free(val)			_cfg_free(&cfg_dx, val)

#define dxcfg_int_json(jt, val)			_cfg_int_json(&cfg_dx, jt, val)
#define dxcfg_float_json(jt, val)		_cfg_float_json(&cfg_dx, jt, val)
#define dxcfg_bool_json(jt, val)		_cfg_bool_json(&cfg_dx, jt, val)
#define dxcfg_string_json(jt, val)		_cfg_type_json(&cfg_dx, JSMN_STRING, jt, val)
#define dxcfg_lookup_json(id)			_cfg_lookup_json(&cfg_dx, id)

#define	CALLED_FROM_MAIN		true
#define	NOT_CALLED_FROM_MAIN	false
void cfg_reload(bool called_from_main);

void _cfg_init(cfg_t *cfg);
void _cfg_save_json(cfg_t *cfg, char *json);

int _cfg_int(cfg_t *cfg, const char *name, bool *error, u4_t flags);
void _cfg_set_int(cfg_t *cfg, const char *name, int val, u4_t flags);
double _cfg_float(cfg_t *cfg, const char *name, bool *error, u4_t flags);
void _cfg_set_float(cfg_t *cfg, const char *name, double val, u4_t flags);
int _cfg_bool(cfg_t *cfg, const char *name, bool *error, u4_t flags);
void _cfg_set_bool(cfg_t *cfg, const char *name, u4_t val);
const char *_cfg_string(cfg_t *cfg, const char *name, bool *error, u4_t flags);
void _cfg_set_string(cfg_t *cfg, const char *name, const char *val);
const char *_cfg_object(cfg_t *cfg, const char *name, bool *error, u4_t flags);
void _cfg_set_object(cfg_t *cfg, const char *name, const char *val);

char *_cfg_get_json(cfg_t *cfg, int *size);
char *_cfg_realloc_json(cfg_t *cfg, int size, u4_t flags);

void cfg_print_tok(cfg_t *cfg, jsmntok_t *jt, int seq, int hit, int lvl, int rem);
typedef void (*cfg_walk_cb_t)(cfg_t *cfg, jsmntok_t *, int, int, int, int);
void _cfg_walk(cfg_t *cfg, const char *id, cfg_walk_cb_t cb);

bool _cfg_int_json(cfg_t *cfg, jsmntok_t *jt, int *num);
bool _cfg_float_json(cfg_t *cfg, jsmntok_t *jt, double *num);
bool _cfg_type_json(cfg_t *cfg, jsmntype_t jt_type, jsmntok_t *jt, const char **str);
jsmntok_t *_cfg_lookup_json(cfg_t *cfg, const char *id);
void _cfg_free(cfg_t *cfg, const char *str);

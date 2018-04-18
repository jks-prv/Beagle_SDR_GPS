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
#include "coroutines.h"

// configuration

struct cfg_t {
	bool init, init_load;
	lock_t lock;    // FIXME: now that parsing the dx list is yielding probably need to lock
	const char *filename;

	char *json, *json_write;
	int json_buf_size;		// includes terminating null

	int tok_size, ntok;
	jsmntok_t *tokens;
};

extern cfg_t cfg_cfg, cfg_adm, cfg_dx;

#define CFG_NONE		0x0000
#define CFG_OPTIONAL	0x0000
#define CFG_PRINT		0x0001
#define CFG_REQUIRED	0x0002
#define CFG_SET			0x0010
#define CFG_REMOVE		0x0020
#define CFG_COPY		0x0040
#define CFG_CHANGE		0x0080
#define CFG_NO_DOT		0x0100

#define CFG_LOOKUP_LVL1 ((jsmntok_t *) -1)

#define cfg_init()							_cfg_init(&cfg_cfg, NULL)
#define	cfg_get_json(size)					_cfg_get_json(&cfg_cfg, size)
#define	cfg_realloc_json(size, flags)		_cfg_realloc_json(&cfg_cfg, size, flags)
#define cfg_save_json(json)					_cfg_save_json(&cfg_cfg, json)
#define cfg_walk(id, cb, param)				_cfg_walk(&cfg_cfg, id, cb, param)

#define cfg_int(name, err, flags)			_cfg_int(&cfg_cfg, name, err, flags)
#define cfg_set_int(name, val)				_cfg_set_int(&cfg_cfg, name, val, CFG_SET, 0)
#define cfg_rem_int(name)					_cfg_set_int(&cfg_cfg, name, 0, CFG_REMOVE, 0)
#define cfg_default_int(name, val, err)	    _cfg_default_int(&cfg_cfg, name, val, err)

#define cfg_float(name, err, flags)			_cfg_float(&cfg_cfg, name, err, flags)
#define cfg_set_float(name, val)			_cfg_set_float(&cfg_cfg, name, val, CFG_SET, 0)
#define cfg_rem_float(name)					_cfg_set_float(&cfg_cfg, name, 0, CFG_REMOVE, 0)
#define cfg_default_float(name, val, err)	_cfg_default_float(&cfg_cfg, name, val, err)

#define cfg_bool(name, err, flags)			_cfg_bool(&cfg_cfg, name, err, flags)
#define cfg_set_bool(name, val)				_cfg_set_bool(&cfg_cfg, name, (u4_t) val, CFG_SET, 0)
#define cfg_rem_bool(name)					_cfg_set_bool(&cfg_cfg, name, 0, CFG_REMOVE, 0)
#define cfg_default_bool(name, val, err)	_cfg_default_bool(&cfg_cfg, name, val, err)

#define cfg_string(name, err, flags)		_cfg_string(&cfg_cfg, name, err, flags)
#define cfg_string_free(val)				_cfg_free(&cfg_cfg, val)
#define cfg_set_string(name, val)			_cfg_set_string(&cfg_cfg, name, val, CFG_SET, 0)
#define cfg_rem_string(name)				_cfg_set_string(&cfg_cfg, name, NULL, CFG_REMOVE, 0)
#define cfg_default_string(name, val, err)	_cfg_default_string(&cfg_cfg, name, val, err)

#define cfg_object(name, err, flags)		_cfg_object(&cfg_cfg, name, err, flags)
#define cfg_object_free(val)				_cfg_free(&cfg_cfg, val)
#define cfg_set_object(name, val)			_cfg_set_object(&cfg_cfg, name, val, CFG_SET, 0)
#define cfg_rem_object(name)				_cfg_set_object(&cfg_cfg, name, NULL, CFG_REMOVE, 0)

#define admcfg_init()						_cfg_init(&cfg_adm, NULL)
#define	admcfg_get_json(size)				_cfg_get_json(&cfg_adm, size)
#define	admcfg_realloc_json(size, flags) 	_cfg_realloc_json(&cfg_adm, size, flags)
#define admcfg_save_json(json)				_cfg_save_json(&cfg_adm, json)
#define admcfg_walk(id, cb, param)			_cfg_walk(&cfg_adm, id, cb, param)

#define admcfg_int(name, err, flags)		_cfg_int(&cfg_adm, name, err, flags)
#define admcfg_set_int(name, val)			_cfg_set_int(&cfg_adm, name, val, CFG_SET, 0)
#define admcfg_rem_int(name)				_cfg_set_int(&cfg_adm, name, 0, CFG_REMOVE, 0)
#define admcfg_default_int(name, val, err)	_cfg_default_int(&cfg_adm, name, val, err)

#define admcfg_float(name, err, flags)		_cfg_float(&cfg_adm, name, err, flags)
#define admcfg_set_float(name, val)			_cfg_set_float(&cfg_adm, name, val, CFG_SET, 0)
#define admcfg_rem_float(name)				_cfg_set_float(&cfg_adm, name, 0, CFG_REMOVE, 0)
#define admcfg_default_float(name, val, err) _cfg_default_float(&cfg_adm, name, val, err)

#define admcfg_bool(name, err, flags)		_cfg_bool(&cfg_adm, name, err, flags)
#define admcfg_set_bool(name, val)			_cfg_set_bool(&cfg_adm, name, (u4_t) val, CFG_SET, 0)
#define admcfg_rem_bool(name)				_cfg_set_bool(&cfg_adm, name, 0, CFG_REMOVE, 0)
#define admcfg_default_bool(name, val, err)	_cfg_default_bool(&cfg_adm, name, val, err)

#define admcfg_string(name, err, flags)		_cfg_string(&cfg_adm, name, err, flags)
#define admcfg_string_free(val)				_cfg_free(&cfg_adm, val)
#define admcfg_set_string(name, val)		_cfg_set_string(&cfg_adm, name, val, CFG_SET, 0)
#define admcfg_rem_string(name)				_cfg_set_string(&cfg_adm, name, NULL, CFG_REMOVE, 0)
#define admcfg_default_string(name, val, err) _cfg_default_string(&cfg_adm, name, val, err)

#define admcfg_object(name, err, flags)		_cfg_object(&cfg_adm, name, err, flags)
#define admcfg_object_free(val)				_cfg_free(&cfg_adm, val)
#define admcfg_set_object(name, val)		_cfg_set_object(&cfg_adm, name, val, CFG_SET, 0)
#define admcfg_rem_object(name)				_cfg_set_object(&cfg_adm, name, NULL, CFG_REMOVE, 0)

#define dxcfg_init()						_cfg_init(&cfg_dx, NULL)
#define	dxcfg_get_json(size)				_cfg_get_json(&cfg_dx, size)
#define	dxcfg_realloc_json(size, flags)		_cfg_realloc_json(&cfg_dx, size, flags)
#define dxcfg_save_json(json)				_cfg_save_json(&cfg_dx, json)
#define dxcfg_walk(id, cb, param)			_cfg_walk(&cfg_dx, id, cb, param)

#define dxcfg_int(name, err, flags)			_cfg_int(&cfg_dx, name, err, flags)
#define dxcfg_float(name, err, flags)		_cfg_float(&cfg_dx, name, err, flags)
#define dxcfg_bool(name, err, flags)		_cfg_bool(&cfg_dx, name, err, flags)
#define dxcfg_string(name, err, flags)		_cfg_string(&cfg_dx, name, err, flags)
#define dxcfg_string_free(val)				_cfg_free(&cfg_dx, val)

// process from a particular token, usually from an array
#define dxcfg_int_json(jt, val)				_cfg_int_json(&cfg_dx, jt, val)
#define dxcfg_float_json(jt, val)			_cfg_float_json(&cfg_dx, jt, val)
#define dxcfg_bool_json(jt, val)			_cfg_bool_json(&cfg_dx, jt, val)
#define dxcfg_string_json(jt, val)			_cfg_type_json(&cfg_dx, JSMN_STRING, jt, val)
#define dxcfg_lookup_json(id)				_cfg_lookup_json(&cfg_dx, id, CFG_OPT_NONE)

// process JSON from a buffer
#define json_init(cfg, json)				_cfg_init(cfg, json)

#define json_int(cfg, name, err, flags)		_cfg_int(cfg, name, err, flags)
#define json_set_int(cfg, name, val)		_cfg_set_int(cfg, name, val, CFG_SET, 0)
#define json_rem_int(cfg, name)				_cfg_set_int(cfg, name, 0, CFG_REMOVE, 0)
#define json_default_int(cfg, name, val, err) _cfg_default_int(cfg, name, val, err)

#define json_float(cfg, name, err, flags)	_cfg_float(cfg, name, err, flags)
#define json_set_float(cfg, name, val)		_cfg_set_float(cfg, name, val, CFG_SET, 0)
#define json_rem_float(cfg, name)			_cfg_set_float(cfg, name, 0, CFG_REMOVE, 0)

#define json_bool(cfg, name, err, flags)	_cfg_bool(cfg, name, err, flags)
#define json_set_bool(cfg, name, val)		_cfg_set_bool(cfg, name, (u4_t) val, CFG_SET, 0)
#define json_rem_bool(cfg, name)			_cfg_set_bool(cfg, name, 0, CFG_REMOVE, 0)

#define json_string(cfg, name, err, flags)	_cfg_string(cfg, name, err, flags)
#define json_string_free(cfg, val)			_cfg_free(cfg, val)
#define json_set_string(cfg, name, val)		_cfg_set_string(cfg, name, val, CFG_SET, 0)
#define json_rem_string(cfg, name)			_cfg_set_string(cfg, name, NULL, CFG_REMOVE, 0)

#define	CALLED_FROM_MAIN		true
#define	NOT_CALLED_FROM_MAIN	false
void cfg_reload(bool called_from_main);

bool _cfg_init(cfg_t *cfg, char *buf);
void _cfg_save_json(cfg_t *cfg, char *json);

int _cfg_int(cfg_t *cfg, const char *name, bool *error, u4_t flags);
int _cfg_set_int(cfg_t *cfg, const char *name, int val, u4_t flags, int pos);
int _cfg_default_int(cfg_t *cfg, const char *name, int val, bool *error);

double _cfg_float(cfg_t *cfg, const char *name, bool *error, u4_t flags);
int _cfg_set_float(cfg_t *cfg, const char *name, double val, u4_t flags, int pos);
double _cfg_default_float(cfg_t *cfg, const char *name, double val, bool *error);

int _cfg_bool(cfg_t *cfg, const char *name, bool *error, u4_t flags);
int _cfg_set_bool(cfg_t *cfg, const char *name, u4_t val, u4_t flags, int pos);
bool _cfg_default_bool(cfg_t *cfg, const char *name, u4_t val, bool *error);

const char *_cfg_string(cfg_t *cfg, const char *name, bool *error, u4_t flags);
int _cfg_set_string(cfg_t *cfg, const char *name, const char *val, u4_t flags, int pos);
void _cfg_default_string(cfg_t *cfg, const char *name, const char *val, bool *error);

const char *_cfg_object(cfg_t *cfg, const char *name, bool *error, u4_t flags);
int _cfg_set_object(cfg_t *cfg, const char *name, const char *val, u4_t flags, int pos);

char *_cfg_get_json(cfg_t *cfg, int *size);
char *_cfg_realloc_json(cfg_t *cfg, int size, u4_t flags);

void cfg_print_tok(cfg_t *cfg, void *param, jsmntok_t *jt, int seq, int hit, int lvl, int rem);
typedef bool (*cfg_walk_cb_t)(cfg_t *cfg, void *param, jsmntok_t *jt, int seq, int hit, int lvl, int rem, void **rval);
void *_cfg_walk(cfg_t *cfg, const char *id, cfg_walk_cb_t cb, void *param);

bool _cfg_int_json(cfg_t *cfg, jsmntok_t *jt, int *num);
bool _cfg_float_json(cfg_t *cfg, jsmntok_t *jt, double *num);
bool _cfg_type_json(cfg_t *cfg, jsmntype_t jt_type, jsmntok_t *jt, const char **str);
void _cfg_free(cfg_t *cfg, const char *str);

enum cfg_lookup_e { CFG_OPT_NONE, CFG_OPT_ID1, CFG_OPT_ID2, CFG_OPT_NO_DOT };
jsmntok_t *_cfg_lookup_json(cfg_t *cfg, const char *id, cfg_lookup_e option);

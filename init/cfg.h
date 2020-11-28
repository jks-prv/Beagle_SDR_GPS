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

typedef struct {
	bool init, init_load, isJSON;
	lock_t lock;    // FIXME: now that parsing the dx list is yielding probably need to lock
	int flags;
	const char *filename;

	char *json, *json_write;
	int json_buf_size;		// includes terminating null

	int tok_size, ntok;
	jsmntok_t *tokens;
} cfg_t;

extern cfg_t cfg_cfg, cfg_adm, cfg_dx;

#define CFG_NONE		0x0000
#define CFG_OPTIONAL	0x0000
#define CFG_PRINT		0x0001
#define CFG_REQUIRED	0x0002
#define CFG_SAVE        0x0004
#define CFG_SET			0x0010
#define CFG_REMOVE		0x0020
#define CFG_COPY		0x0040
#define CFG_CHANGE		0x0080
#define CFG_NO_DOT		0x0100
#define CFG_NO_UPDATE   0x0200
#define CFG_PARSE_VALID 0x0400

#define CFG_LOOKUP_LVL1 ((jsmntok_t *) -1)

#define cfg_init()							_cfg_init(&cfg_cfg, CFG_NONE, NULL)
#define	cfg_get_json(size)					_cfg_get_json(&cfg_cfg, size)
#define cfg_save_json(json)					_cfg_save_json(&cfg_cfg, json)
#define cfg_walk(id, cb, param)				_cfg_walk(&cfg_cfg, id, cb, param)

#define cfg_int(name, err, flags)			_cfg_int(&cfg_cfg, name, err, flags)
#define cfg_set_int(name, val)				_cfg_set_int(&cfg_cfg, name, val, CFG_SET, 0)
#define cfg_set_int_save(name, val)			_cfg_set_int(&cfg_cfg, name, val, CFG_SET | CFG_SAVE, 0)
#define cfg_rem_int(name)					_cfg_set_int(&cfg_cfg, name, 0, CFG_REMOVE, 0)
#define cfg_default_int(name, val, err)	    _cfg_default_int(&cfg_cfg, name, val, err)

#define cfg_float(name, err, flags)			_cfg_float(&cfg_cfg, name, err, flags)
#define cfg_set_float(name, val)			_cfg_set_float(&cfg_cfg, name, val, CFG_SET, 0)
#define cfg_set_float_save(name, val)       _cfg_set_float(&cfg_cfg, name, val, CFG_SET | CFG_SAVE, 0)
#define cfg_rem_float(name)					_cfg_set_float(&cfg_cfg, name, 0, CFG_REMOVE, 0)
#define cfg_default_float(name, val, err)	_cfg_default_float(&cfg_cfg, name, val, err)

#define cfg_bool(name, err, flags)			_cfg_bool(&cfg_cfg, name, err, flags)
#define cfg_set_bool(name, val)				_cfg_set_bool(&cfg_cfg, name, (u4_t) val, CFG_SET, 0)
#define cfg_set_bool_save(name, val)		_cfg_set_bool(&cfg_cfg, name, (u4_t) val, CFG_SET | CFG_SAVE, 0)
#define cfg_rem_bool(name)					_cfg_set_bool(&cfg_cfg, name, 0, CFG_REMOVE, 0)
#define cfg_default_bool(name, val, err)	_cfg_default_bool(&cfg_cfg, name, val, err)

#define cfg_string(name, err, flags)		_cfg_string(&cfg_cfg, name, err, flags)
#define cfg_string_free(val)				_cfg_free(&cfg_cfg, val)
#define cfg_set_string(name, val)			_cfg_set_string(&cfg_cfg, name, val, CFG_SET, 0)
#define cfg_set_string_save(name, val)		_cfg_set_string(&cfg_cfg, name, val, CFG_SET | CFG_SAVE, 0)
#define cfg_rem_string(name)				_cfg_set_string(&cfg_cfg, name, NULL, CFG_REMOVE, 0)
#define cfg_default_string(name, val, err)	_cfg_default_string(&cfg_cfg, name, val, err)

#define cfg_object(name, err, flags)		_cfg_object(&cfg_cfg, name, err, flags)
#define cfg_object_free(val)				_cfg_free(&cfg_cfg, val)
#define cfg_set_object(name, val)			_cfg_set_object(&cfg_cfg, name, val, CFG_SET, 0)
#define cfg_set_object_save(name, val)		_cfg_set_object(&cfg_cfg, name, val, CFG_SET | CFG_SAVE, 0)
#define cfg_rem_object(name)				_cfg_set_object(&cfg_cfg, name, NULL, CFG_REMOVE, 0)
#define cfg_default_object(name, val, err) 	_cfg_default_object(&cfg_cfg, name, val, err)

#define admcfg_init()						_cfg_init(&cfg_adm, CFG_NONE, NULL)
#define	admcfg_get_json(size)				_cfg_get_json(&cfg_adm, size)
#define admcfg_save_json(json)				_cfg_save_json(&cfg_adm, json)
#define admcfg_walk(id, cb, param)			_cfg_walk(&cfg_adm, id, cb, param)

#define admcfg_int(name, err, flags)		_cfg_int(&cfg_adm, name, err, flags)
#define admcfg_set_int(name, val)			_cfg_set_int(&cfg_adm, name, val, CFG_SET, 0)
#define admcfg_set_int_save(name, val)		_cfg_set_int(&cfg_adm, name, val, CFG_SET | CFG_SAVE, 0)
#define admcfg_rem_int(name)				_cfg_set_int(&cfg_adm, name, 0, CFG_REMOVE, 0)
#define admcfg_default_int(name, val, err)	_cfg_default_int(&cfg_adm, name, val, err)

#define admcfg_float(name, err, flags)		_cfg_float(&cfg_adm, name, err, flags)
#define admcfg_set_float(name, val)			_cfg_set_float(&cfg_adm, name, val, CFG_SET, 0)
#define admcfg_set_float_save(name, val)	_cfg_set_float(&cfg_adm, name, val, CFG_SET | CFG_SAVE, 0)
#define admcfg_rem_float(name)				_cfg_set_float(&cfg_adm, name, 0, CFG_REMOVE, 0)
#define admcfg_default_float(name, val, err) _cfg_default_float(&cfg_adm, name, val, err)

#define admcfg_bool(name, err, flags)		_cfg_bool(&cfg_adm, name, err, flags)
#define admcfg_set_bool(name, val)			_cfg_set_bool(&cfg_adm, name, (u4_t) val, CFG_SET, 0)
#define admcfg_set_bool_save(name, val)		_cfg_set_bool(&cfg_adm, name, (u4_t) val, CFG_SET | CFG_SAVE, 0)
#define admcfg_rem_bool(name)				_cfg_set_bool(&cfg_adm, name, 0, CFG_REMOVE, 0)
#define admcfg_default_bool(name, val, err)	_cfg_default_bool(&cfg_adm, name, val, err)

#define admcfg_string(name, err, flags)		_cfg_string(&cfg_adm, name, err, flags)
#define admcfg_string_free(val)				_cfg_free(&cfg_adm, val)
#define admcfg_set_string(name, val)		_cfg_set_string(&cfg_adm, name, val, CFG_SET, 0)
#define admcfg_set_string_save(name, val)	_cfg_set_string(&cfg_adm, name, val, CFG_SET | CFG_SAVE, 0)
#define admcfg_rem_string(name)				_cfg_set_string(&cfg_adm, name, NULL, CFG_REMOVE, 0)
#define admcfg_default_string(name, val, err) _cfg_default_string(&cfg_adm, name, val, err)

#define admcfg_object(name, err, flags)		_cfg_object(&cfg_adm, name, err, flags)
#define admcfg_object_free(val)				_cfg_free(&cfg_adm, val)
#define admcfg_set_object(name, val)		_cfg_set_object(&cfg_adm, name, val, CFG_SET, 0)
#define admcfg_set_object_save(name, val)	_cfg_set_object(&cfg_adm, name, val, CFG_SET | CFG_SAVE, 0)
#define admcfg_rem_object(name)				_cfg_set_object(&cfg_adm, name, NULL, CFG_REMOVE, 0)
#define admcfg_default_object(name, val, err) _cfg_default_object(&cfg_adm, name, val, err)

#define dxcfg_init()						_cfg_init(&cfg_dx, CFG_NO_UPDATE, NULL)
#define	dxcfg_get_json(size)				_cfg_get_json(&cfg_dx, size)
#define dxcfg_save_json(json)				_cfg_save_json(&cfg_dx, json)
#define dxcfg_update_json()                 _cfg_update_json(&cfg_dx)
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
#define json_init(cfg, json)				_cfg_init(cfg, CFG_NONE, json)
#define json_release(cfg)                   _cfg_release(cfg)
#define json_walk(cfg, id, cb, param)       _cfg_walk(cfg, id, cb, param)

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

void cfg_reload();

bool _cfg_init(cfg_t *cfg, int flags, char *buf);
void _cfg_release(cfg_t *cfg);
void _cfg_save_json(cfg_t *cfg, char *json);
void _cfg_update_json(cfg_t *cfg);

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
void _cfg_default_object(cfg_t *cfg, const char *name, const char *val, bool *error);

char *_cfg_get_json(cfg_t *cfg, int *size);

bool cfg_print_tok(cfg_t *cfg, void *param, jsmntok_t *jt, int seq, int hit, int lvl, int rem, void **rval);
typedef bool (*cfg_walk_cb_t)(cfg_t *cfg, void *param, jsmntok_t *jt, int seq, int hit, int lvl, int rem, void **rval);
void *_cfg_walk(cfg_t *cfg, const char *id, cfg_walk_cb_t cb, void *param);

bool _cfg_int_json(cfg_t *cfg, jsmntok_t *jt, int *num);
bool _cfg_float_json(cfg_t *cfg, jsmntok_t *jt, double *num);
bool _cfg_type_json(cfg_t *cfg, jsmntype_t jt_type, jsmntok_t *jt, const char **str);
void _cfg_free(cfg_t *cfg, const char *str);

typedef enum { CFG_OPT_NONE, CFG_OPT_ID1, CFG_OPT_ID2, CFG_OPT_NO_DOT } cfg_lookup_e;
jsmntok_t *_cfg_lookup_json(cfg_t *cfg, const char *id, cfg_lookup_e option);

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

#pragma once

#include "types.h"
#include "kiwi.h"
#include "jsmn.h"
#include "coroutines.h"

// configuration

typedef struct {
	bool init, init_load;
	lock_t lock;    // FIXME: now that parsing the dx list is yielding probably need to lock
	u4_t flags;
	const char *filename, *basename;
	#define CFG_ID_N 64
	char id_tokens[CFG_ID_N], id_json[CFG_ID_N];
	u4_t update_seq;

	char *json, *json_write;
	int json_buf_size;		// includes terminating null

	int tok_size, ntok;
	jsmntok_t *tokens;
} cfg_t;

extern cfg_t cfg_cfg, cfg_adm, cfg_dx, cfg_dxcfg, cfg_dxcomm, cfg_dxcomm_cfg;

#define CFG_NONE		0x00000
#define CFG_OPTIONAL	0x00000
#define CFG_PRINT		0x00001
#define CFG_REQUIRED	0x00002
#define CFG_SAVE        0x00004
#define CFG_YIELD       0x00008
#define CFG_SET			0x00010
#define CFG_REMOVE		0x00020
#define CFG_COPY		0x00040
#define CFG_CHANGE		0x00080
#define CFG_NO_DOT		0x00100
#define CFG_NO_UPDATE   0x00200
#define CFG_PARSE_VALID 0x00400
#define CFG_DEBUG       0x00800
#define CFG_NO_PARSE    0x01000
#define CFG_INT_BASE10  0x02000
#define CFG_LOAD_ONLY   0x04000
#define CFG_IS_JSON     0x08000
#define CFG_NO_INTEG    0x10000
#define CFG_GDB_BREAK   0x20000

#define CFG_LOOKUP_LVL1 ((jsmntok_t *) -1)

#define cfg_init()							_cfg_init(&cfg_cfg, CFG_NONE, NULL)
#define	cfg_get_json(size)					_cfg_get_json(&cfg_cfg, size)
#define cfg_save_json(json)					_cfg_save_json(&cfg_cfg, json)
#define cfg_walk(id, cb, p1, p2)            _cfg_walk(&cfg_cfg, id, cb, p1, p2)

#define cfg_int(name, err, flags)			_cfg_int(&cfg_cfg, name, err, flags)
#define cfg_set_int(name, val)				_cfg_set_int(&cfg_cfg, name, val, CFG_SET, 0)
#define cfg_set_int_save(name, val)			_cfg_set_int(&cfg_cfg, name, val, CFG_SET | CFG_SAVE, 0)
#define cfg_rem_int(name)					_cfg_set_int(&cfg_cfg, name, 0, CFG_REMOVE, 0)
#define cfg_default_int(name, val, err)	    _cfg_default_int(&cfg_cfg, name, val, err)
#define cfg_update_int(name, val, changed)  _cfg_update_int(&cfg_cfg, name, val, changed)

#define cfg_float(name, err, flags)			_cfg_float(&cfg_cfg, name, err, flags)
#define cfg_set_float(name, val)			_cfg_set_float(&cfg_cfg, name, val, CFG_SET, 0)
#define cfg_set_float_save(name, val)       _cfg_set_float(&cfg_cfg, name, val, CFG_SET | CFG_SAVE, 0)
#define cfg_rem_float(name)					_cfg_set_float(&cfg_cfg, name, 0, CFG_REMOVE, 0)
#define cfg_default_float(name, val, err)	_cfg_default_float(&cfg_cfg, name, val, err)

#define cfg_bool(name, err, flags)			_cfg_bool(&cfg_cfg, name, err, flags)
#define cfg_true(name)			            (_cfg_bool(&cfg_cfg, name, NULL, CFG_OPTIONAL) == true)
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

#define cfg_array(name, err, flags)		    _cfg_array(&cfg_cfg, name, err, flags)
#define cfg_array_free(val)				    _cfg_free(&cfg_cfg, val)

#define cfg_object(name, err, flags)		_cfg_object(&cfg_cfg, name, err, flags)
#define cfg_object_free(val)				_cfg_free(&cfg_cfg, val)
#define cfg_set_object(name, val)			_cfg_set_object(&cfg_cfg, name, val, CFG_SET, 0)
#define cfg_set_object_save(name, val)		_cfg_set_object(&cfg_cfg, name, val, CFG_SET | CFG_SAVE, 0)
#define cfg_rem_object(name)				_cfg_set_object(&cfg_cfg, name, NULL, CFG_REMOVE, 0)
#define cfg_default_object(name, val, err) 	_cfg_default_object(&cfg_cfg, name, val, err)

#define admcfg_init()						_cfg_init(&cfg_adm, CFG_NONE, NULL)
#define	admcfg_get_json(size)				_cfg_get_json(&cfg_adm, size)
#define admcfg_save_json(json)				_cfg_save_json(&cfg_adm, json)
#define admcfg_walk(id, cb, p1, p2)         _cfg_walk(&cfg_adm, id, cb, p1, p2)

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
#define admcfg_true(name)                   (_cfg_bool(&cfg_adm, name, NULL, CFG_OPTIONAL) == true)
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

// DX databases
#define dx_init()						    _cfg_init(&cfg_dx, CFG_NO_UPDATE, NULL)
#define dx_save_json(json)				    _cfg_save_json(&cfg_dx, json)
#define dxcfg_init()						_cfg_init(&cfg_dxcfg, CFG_NO_UPDATE | CFG_LOAD_ONLY, NULL)
#define	dxcfg_get_json(size)				_cfg_get_json(&cfg_dxcfg, size)
#define dxcfg_save_json(json)			    _cfg_save_json(&cfg_dxcfg, json)

#define dxcomm_init()						_cfg_init(&cfg_dxcomm, CFG_NO_UPDATE, NULL)
#define dxcomm_cfg_init()					_cfg_init(&cfg_dxcomm_cfg, CFG_NO_UPDATE | CFG_LOAD_ONLY, NULL)
#define	dxcomm_cfg_get_json(size)			_cfg_get_json(&cfg_dxcomm_cfg, size)

// process from a particular token, usually from an array
#define cfg_int_tok(cfg, jt, val)           _cfg_int_json(cfg, jt, val)
#define cfg_float_tok(cfg, jt, val)         _cfg_float_json(cfg, jt, val)
#define cfg_bool_tok(cfg, jt, val)          _cfg_bool_json(cfg, jt, val)
#define cfg_string_tok(cfg, jt, val)        _cfg_type_json(cfg, JSMN_STRING, jt, val)
#define cfg_string_tok_free(cfg, val)       _cfg_free(cfg, val)
#define cfg_lookup_tok(cfg, id)             _cfg_lookup_json(cfg, id, CFG_OPT_NONE)

// process JSON from a buffer
#define json_init(cfg, json, id)            _cfg_init(cfg, CFG_IS_JSON, json, id)
#define json_init_flags(cfg, f, json, id)   _cfg_init(cfg, (f) | CFG_IS_JSON, json, id)
#define json_init_file(cfg)				    _cfg_init(cfg, CFG_IS_JSON, NULL)
#define json_save(cfg, json)                _cfg_save_json(cfg, json)
#define json_release(cfg)                   _cfg_release(cfg)
#define json_walk(cfg, id, cb, p1, p2)      _cfg_walk(cfg, id, cb, p1, p2)

#define json_int(cfg, name, err, flags)		_cfg_int(cfg, name, err, flags)
#define json_set_int(cfg, name, val)		_cfg_set_int(cfg, name, val, CFG_SET, 0)
#define json_rem_int(cfg, name)				_cfg_set_int(cfg, name, 0, CFG_REMOVE, 0)
#define json_default_int(cfg, name, val, e) _cfg_default_int(cfg, name, val, e)

#define json_float(cfg, name, err, flags)	_cfg_float(cfg, name, err, flags)
#define json_set_float(cfg, name, val)		_cfg_set_float(cfg, name, val, CFG_SET, 0)
#define json_rem_float(cfg, name)			_cfg_set_float(cfg, name, 0, CFG_REMOVE, 0)

#define json_bool(cfg, name, err, flags)	_cfg_bool(cfg, name, err, flags)
#define json_true(name)			            (_cfg_bool(cfg, name, NULL, CFG_OPTIONAL) == true)
#define json_set_bool(cfg, name, val)		_cfg_set_bool(cfg, name, (u4_t) val, CFG_SET, 0)
#define json_rem_bool(cfg, name)			_cfg_set_bool(cfg, name, 0, CFG_REMOVE, 0)

#define json_string(cfg, name, err, flags)	_cfg_string(cfg, name, err, flags)
#define json_string_free(cfg, val)			_cfg_free(cfg, val)
#define json_set_string(cfg, name, val)		_cfg_set_string(cfg, name, val, CFG_SET, 0)
#define json_rem_string(cfg, name)			_cfg_set_string(cfg, name, NULL, CFG_REMOVE, 0)

void cfg_reload();
bool cfg_gdb_break(bool val);

bool _cfg_init(cfg_t *cfg, int flags, char *buf, const char *id = NULL);
void _cfg_release(cfg_t *cfg);
void _cfg_save_json(cfg_t *cfg, char *json);
void _cfg_update_json(cfg_t *cfg);

int _cfg_int(cfg_t *cfg, const char *name, bool *error, u4_t flags);
int _cfg_set_int(cfg_t *cfg, const char *name, int val, u4_t flags, int pos);
int _cfg_default_int(cfg_t *cfg, const char *name, int val, bool *error);
int _cfg_update_int(cfg_t *cfg, const char *name, int val, bool *changed);

double _cfg_float(cfg_t *cfg, const char *name, bool *error, u4_t flags);
int _cfg_set_float(cfg_t *cfg, const char *name, double val, u4_t flags, int pos);
double _cfg_default_float(cfg_t *cfg, const char *name, double val, bool *error);

int _cfg_bool(cfg_t *cfg, const char *name, bool *error, u4_t flags);
int _cfg_set_bool(cfg_t *cfg, const char *name, u4_t val, u4_t flags, int pos);
bool _cfg_default_bool(cfg_t *cfg, const char *name, u4_t val, bool *error);

const char *_cfg_string(cfg_t *cfg, const char *name, bool *error, u4_t flags);
int _cfg_set_string(cfg_t *cfg, const char *name, const char *val, u4_t flags, int pos);
void _cfg_default_string(cfg_t *cfg, const char *name, const char *val, bool *error);

const char *_cfg_array(cfg_t *cfg, const char *name, bool *error, u4_t flags);

const char *_cfg_object(cfg_t *cfg, const char *name, bool *error, u4_t flags);
int _cfg_set_object(cfg_t *cfg, const char *name, const char *val, u4_t flags, int pos);
void _cfg_default_object(cfg_t *cfg, const char *name, const char *val, bool *error);

char *_cfg_get_json(cfg_t *cfg, int *size);

bool cfg_print_tok(cfg_t *cfg, void *param, jsmntok_t *jt, int seq, int hit, int lvl, int rem, void **rval);
typedef bool (*cfg_walk_cb_t)(cfg_t *cfg, void *param1, void *param2, jsmntok_t *jt, int seq, int hit, int lvl, int rem, void **rval);
void *_cfg_walk(cfg_t *cfg, const char *id, cfg_walk_cb_t cb, void *param1 = NULL, void *param2 = NULL);

bool _cfg_int_json(cfg_t *cfg, jsmntok_t *jt, int *num);
bool _cfg_float_json(cfg_t *cfg, jsmntok_t *jt, double *num);
bool _cfg_type_json(cfg_t *cfg, jsmntype_t jt_type, jsmntok_t *jt, const char **str);
void _cfg_free(cfg_t *cfg, const char *str);

typedef enum { CFG_OPT_NONE, CFG_OPT_ID1, CFG_OPT_ID2, CFG_OPT_NO_DOT } cfg_lookup_e;
jsmntok_t *_cfg_lookup_json(cfg_t *cfg, const char *id, cfg_lookup_e option);

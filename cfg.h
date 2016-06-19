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

#ifdef DEVSYS
	#ifdef CFG_DOT_C
		#define CFG_VOID { }
		#define CFG_ZERO { return 0; }
		#define CFG_NULL { return NULL; }
	#else
		#define CFG_VOID ;
		#define CFG_ZERO ;
		#define CFG_NULL ;
	#endif
	
	struct config_t { };
	struct config_setting_t { };
	
	void config_init(config_t *config) CFG_VOID
	void config_destroy(config_t *config) CFG_VOID
	int config_read_file(config_t *config, const char *file) CFG_ZERO
	int config_write_file(config_t *config, const char *file) CFG_ZERO
	char *config_error_file(config_t *config) CFG_NULL
	int config_error_line(config_t *config) CFG_ZERO
	char *config_error_text(config_t *config) CFG_NULL

	// get from abs path
	int config_lookup_int(config_t *config, const char *path, int *val) CFG_ZERO
	int config_lookup_float(config_t *config, const char *path, double *val) CFG_ZERO
	int config_lookup_bool(config_t *config, const char *path, int *val) CFG_ZERO
	int config_lookup_string(config_t *config, const char *path, const char **val) CFG_ZERO

	// node from root and abs/rel paths
	config_setting_t *config_root_setting(const config_t *config) CFG_NULL
	config_setting_t *config_lookup(const config_t *config, const char *abs_path) CFG_NULL
	config_setting_t *config_lookup_from(const config_setting_t *setting, const char *rel_path) CFG_NULL

	// get/set from node
	int config_setting_get_int(const config_setting_t *setting) CFG_ZERO
	double config_setting_get_float(const config_setting_t *setting) CFG_ZERO
	int config_setting_get_bool(const config_setting_t *setting) CFG_ZERO
	const char *config_setting_get_string(const config_setting_t *setting) CFG_NULL

	int config_setting_set_int(config_setting_t *setting, int value) CFG_ZERO
	int config_setting_set_float(config_setting_t *setting, double value) CFG_ZERO
	int config_setting_set_bool(config_setting_t *setting, int value) CFG_ZERO
	int config_setting_set_string(config_setting_t *setting, const char *value) CFG_ZERO

	// get from node child
	int config_setting_lookup_int(const config_setting_t *setting, const char *name, int *value) CFG_ZERO
	int config_setting_lookup_float(const config_setting_t *setting, const char *name, double *value) CFG_ZERO
	int config_setting_lookup_bool(const config_setting_t *setting, const char *name, int *value) CFG_ZERO
	int config_setting_lookup_string(const config_setting_t *setting, const char *name, const char **value) CFG_ZERO
	
	// type from node
	#define CONFIG_TYPE_INT	0
	#define CONFIG_TYPE_INT64	1
	#define CONFIG_TYPE_FLOAT	2
	#define CONFIG_TYPE_STRING	3
	#define CONFIG_TYPE_BOOL	4
	#define CONFIG_TYPE_ARRAY	5
	#define CONFIG_TYPE_LIST	6
	#define CONFIG_TYPE_GROUP	7
	int config_setting_type(const config_setting_t *setting) CFG_ZERO

	// node properties
	int config_setting_length(const config_setting_t *setting) CFG_ZERO
	int config_setting_index(const config_setting_t *setting) CFG_ZERO
	const char *config_setting_name(const config_setting_t *setting) CFG_NULL
	config_setting_t *config_setting_parent(const config_setting_t *setting) CFG_NULL
	
	// node from child of CONFIG_TYPE_GROUP
	config_setting_t *config_setting_get_member(const config_setting_t *setting, const char *name) CFG_NULL

	// node from elem of CONFIG_TYPE_ARRAY, CONFIG_TYPE_LIST, CONFIG_TYPE_GROUP
	config_setting_t *config_setting_get_elem(const config_setting_t *setting, u4_t index) CFG_NULL

	// get/set from elem of CONFIG_TYPE_ARRAY, CONFIG_TYPE_LIST
	int config_setting_get_int_elem(const config_setting_t *setting, int index) CFG_ZERO
	double config_setting_get_float_elem(const config_setting_t *setting, int index) CFG_ZERO
	int config_setting_get_bool_elem(const config_setting_t *setting, int index) CFG_ZERO
	const char *config_setting_get_string_elem(const config_setting_t *setting, int index) CFG_NULL

	// neg index means append
	config_setting_t * config_setting_set_int_elem(config_setting_t *setting, int index, int value) CFG_NULL
	config_setting_t * config_setting_set_float_elem(config_setting_t *setting, int index, double value) CFG_NULL
	config_setting_t * config_setting_set_bool_elem(config_setting_t *setting, int index, int value) CFG_NULL
	config_setting_t * config_setting_set_string_elem(config_setting_t *setting, int index, const char *value) CFG_NULL

	config_setting_t *config_setting_add(config_setting_t *parent, const char *name, int type) CFG_NULL
#else
	#include <libconfig.h>
#endif

struct cfg_t {
	bool init, dirty, use_json;
	const char *filename;

	// libconfig
	const char *filename_cfg;
	bool config_init;
	config_t config;
	config_setting_t *root, *node;
	const char *node_name;
	
	// json
	const char *filename_json;
	char *json;
	int json_size;
	int tok_size, ntok;
	jsmntok_t *tokens;
};

extern cfg_t cfg_cfg, cfg_dx;

#define CFG_NONE		0x00
#define CFG_OPTIONAL	0x00
#define CFG_PRINT		0x01
#define CFG_REQUIRED	0x02
#define CFG_ABS			0x04
#define CFG_REL			0x08

#define cfg_init()						_cfg_init(&cfg_cfg)
#define cfg_update()					_cfg_update(&cfg_cfg)
#define	cfg_get_json(size)				_cfg_get_json(&cfg_cfg, size)
#define cfg_save_json(json)				_cfg_save_json(&cfg_cfg, json)
#define cfg_walk(id, cb)				_cfg_walk(&cfg_cfg, id, cb)
#define cfg_node_abs(path)				_cfg_node(&cfg_cfg, path, CFG_ABS)
#define cfg_node_rel(path)				_cfg_node(&cfg_cfg, path, CFG_REL)

#define cfg_int(name, val, flags)		_cfg_int(&cfg_cfg, name, val, flags)
#define cfg_float(name, val, flags)		_cfg_float(&cfg_cfg, name, val, flags)
#define cfg_bool(name, val, flags)		_cfg_bool(&cfg_cfg, name, val, flags)
#define cfg_string(name, val, flags)	_cfg_string(&cfg_cfg, name, val, flags)
#define cfg_lookup(name, flags)			_cfg_lookup(&cfg_cfg, name, flags)


#define dxcfg_init()					_cfg_init(&cfg_dx)
#define dxcfg_update()					_cfg_update(&cfg_dx)
#define	dxcfg_get_json(size)			_cfg_get_json(&cfg_dx, size)
#define dxcfg_save_json(json)			_cfg_save_json(&cfg_dx, json)
#define dxcfg_walk(id, cb)				_cfg_walk(&cfg_dx, id, cb)

#define dxcfg_int(name, val, flags)		_cfg_int(&cfg_dx, name, val, flags)
#define dxcfg_float(name, val, flags)	_cfg_float(&cfg_dx, name, val, flags)
#define dxcfg_bool(name, val, flags)	_cfg_bool(&cfg_dx, name, val, flags)
#define dxcfg_string(name, val, flags)	_cfg_string(&cfg_dx, name, val, flags)
#define dxcfg_lookup(name, flags)		_cfg_lookup(&cfg_dx, name, flags)

#define	CALLED_FROM_MAIN		true
#define	NOT_CALLED_FROM_MAIN	false
void cfg_reload(bool called_from_main);

void _cfg_init(cfg_t *cfg);
void _cfg_update(cfg_t *cfg);
void _cfg_save_json(cfg_t *cfg, char *json);
int _cfg_node(cfg_t *cfg, const char *path, u4_t flags);

int _cfg_int(cfg_t *cfg, const char *name, int *val, u4_t flags);
int _cfg_set_int(cfg_t *cfg, const char *name, int val, u4_t flags);
double _cfg_float(cfg_t *cfg, const char *name, double *val, u4_t flags);
int _cfg_set_float(cfg_t *cfg, const char *name, double val, u4_t flags);
int _cfg_bool(cfg_t *cfg, const char *name, int *val, u4_t flags);
int _cfg_set_bool(cfg_t *cfg, const char *name, int val, u4_t flags);
const char *_cfg_string(cfg_t *cfg, const char *name, const char **val, u4_t flags);
int _cfg_set_string(cfg_t *cfg, const char *name, const char *val, u4_t flags);

config_setting_t *_cfg_lookup(cfg_t *cfg, const char *path, u4_t flags);

char *_cfg_get_json(cfg_t *cfg, int *size);

typedef void (*cfg_walk_cb_t)(cfg_t *cfg, jsmntok_t *, int, int, int, int);
void _cfg_walk(cfg_t *cfg, const char *id, cfg_walk_cb_t cb);

extern bool reload_kiwi_cfg;
extern int inactivity_timeout_mins;

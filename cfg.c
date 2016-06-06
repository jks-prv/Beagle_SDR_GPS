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
#include "cfg.h"
#include "coroutines.h"

#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>

// fixme: read from file, database etc.
// fixme: display depending on rx time-of-day

dx_t *dx_list;
static int dx_list_len;

static void dxcfg_flag(dx_t *dxp, const char *flag)
{
	if (strcmp(flag, "WL") == 0) dxp->flags |= WL; else
	if (strcmp(flag, "SB") == 0) dxp->flags |= SB; else
	if (strcmp(flag, "DG") == 0) dxp->flags |= DG; else
	if (strcmp(flag, "NoN") == 0) dxp->flags |= NoN; else
	if (strcmp(flag, "XX") == 0) dxp->flags |= XX; else
	if (strcmp(flag, "PB") == 0) dxp->flags |= PB; else
	lprintf("%.2f \"%s\": unknown dx flag \"%s\"\n", dxp->freq, dxp->ident, flag);
}

int serial_number;
int inactivity_timeout_mins;
bool reload_kiwi_cfg;

void cfg_reload(bool called_from_main)
{
	int i, j;
	const char *s;
	
	cfg_init(DIR_CFG "/kiwi.cfg");

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
	
	inactivity_timeout_mins = cfg_int("inactivity_timeout_mins", NULL, CFG_REQUIRED);
	
	
	dxcfg_init(DIR_CFG "/dx.cfg");
	config_setting_t *dx = dxcfg_lookup("dx", CFG_REQUIRED);
	assert(config_setting_type(dx) == CONFIG_TYPE_LIST);
	
	const config_setting_t *dxe;
	for (i=0; (dxe = config_setting_get_elem(dx, i)) != NULL; i++) {
		assert(config_setting_type(dxe) == CONFIG_TYPE_GROUP);
	}
	int _dx_list_len = i-1;
	lprintf("%d dx entries\n", _dx_list_len);
	
	dx_t *_dx_list = (dx_t *) kiwi_malloc("dx_list", _dx_list_len * sizeof(dx_t));
	
	float f = 0;
	
	dx_t *dxp;
	for (i=0, dxp = _dx_list; i < _dx_list_len; i++, dxp++) {
		dxe = config_setting_get_elem(dx, i);
		
		config_setting_t *e;
		assert((e = config_setting_get_member(dxe, "e")) != NULL);
		
		assert((dxp->freq = (float) config_setting_get_float_elem(e, 0)) != 0);
		if (dxp->freq < f)
			lprintf(">>>> DX: entry with freq %.2f < current freq %.2f\n", dxp->freq, f);
		else
			f = dxp->freq;

		assert((s = config_setting_get_string_elem(e, 1)) != NULL);
		if (strcmp(s, "AM") == 0) dxp->flags = AM; else
		if (strcmp(s, "AMN") == 0) dxp->flags = AMN; else
		if (strcmp(s, "LSB") == 0) dxp->flags = LSB; else
		if (strcmp(s, "USB") == 0) dxp->flags = USB; else
		if (strcmp(s, "CW") == 0) dxp->flags = CW; else
		if (strcmp(s, "CWN") == 0) dxp->flags = CWN; else panic("dx config mode");
		
		assert((s = config_setting_get_string_elem(e, 2)) != NULL);
		dxp->ident = strdup(s);
		kiwi_chrrep((char *) dxp->ident, '\'', '"');		// SECURITY: prevent Ajax reply escape
		
		if ((s = config_setting_get_string_elem(e, 3)) == NULL) {
			dxp->notes = NULL;
		} else {
			dxp->notes = strdup(s);
			kiwi_chrrep((char *) dxp->notes, '\'', '"');		// SECURITY: prevent Ajax reply escape
		}

		config_setting_t *flags;
		const char *flag;
		if ((flags = config_setting_get_member(dxe, "f")) != NULL) {
			if (config_setting_type(flags) == CONFIG_TYPE_ARRAY) {
				for (j=0; j < config_setting_length(flags); j++) {
					assert((flag = config_setting_get_string_elem(flags, j)) != NULL);
					dxcfg_flag(dxp, flag);
				}
			} else {
				assert((flag = config_setting_get_string(flags)) != NULL);
				dxcfg_flag(dxp, flag);
			}
		}

		config_setting_t *offset;
		if ((offset = config_setting_get_member(dxe, "o")) != NULL) {
			if (config_setting_type(offset) == CONFIG_TYPE_ARRAY) {
				assert((dxp->low_cut = (float) config_setting_get_int_elem(offset, 0)) != 0);
				assert((dxp->high_cut = (float) config_setting_get_int_elem(offset, 1)) != 0);
			} else {
				assert((dxp->offset = (float) config_setting_get_int(offset)) != 0);
			}
		}

//printf("dxe %d f %.2f notes-%c off %.0f,%.0f\n", i, dxp->freq, dxp->notes? 'Y':'N', dxp->offset, dxp->high_cut);
		dxp->next = dxp+1;
	}
	(dxp-1)->next = NULL;
	
	// switch to new list
	dx_t *prev_dx_list = dx_list;
	int prev_dx_list_len = dx_list_len;
	dx_list = _dx_list;
	dx_list_len = _dx_list_len;
	
	// release previous
	if (prev_dx_list) {
		for (i=0, dxp = prev_dx_list; i < prev_dx_list_len; i++, dxp++) {
			if (dxp->ident) free((void *) dxp->ident);
			if (dxp->notes) free((void *) dxp->notes);
		}
	}
	kiwi_free("dx_list", prev_dx_list);
	
	if (!called_from_main) {
		services_start(SVCS_RESTART_TRUE);
	}
}

#ifdef DEVSYS
	void config_init(config_t *config) {}
	void config_destroy(config_t *config) {}
	int config_read_file(config_t *config, const char *file) { return 0; }
	char *config_error_file(config_t *config) { return NULL; }
	int config_error_line(config_t *config) {return 0; }
	char *config_error_text(config_t *config) { return NULL; }

	int config_lookup_int(config_t *config, const char *name, int *val) { return 0; }
	int config_lookup_float(config_t *config, const char *name, double *val) { return 0; }
	int config_lookup_bool(config_t *config, const char *name, int *val) { return 0; }
	int config_lookup_string(config_t *config, const char *name, const char **val) { return 0; }

	config_setting_t *config_lookup(const config_t *config, const char *path) { return NULL; }
	config_setting_t *config_setting_get_elem(const config_setting_t *setting, u4_t index) { return NULL; }

	int config_setting_lookup_int(const config_setting_t *setting, const char *path, int *value) { return 0; }
	int config_setting_lookup_float(const config_setting_t *setting, const char *path, double *value) { return 0; }
	int config_setting_lookup_string(const config_setting_t *setting, const char *path, const char **value) { return 0; }

	int config_setting_type(const config_setting_t *setting) { return 0; }
	int config_setting_length(const config_setting_t *setting) { return 0; }
	config_setting_t *config_setting_get_member(const config_setting_t *setting, const char *path) { return NULL; }
	const char *config_setting_name(const config_setting_t *setting) { return NULL; }

	int config_setting_get_int(const config_setting_t *setting) { return 0; }
	int config_setting_get_int_elem(const config_setting_t *setting, int index) { return 0; }

	double config_setting_get_float(const config_setting_t *setting) { return 0; }
	double config_setting_get_float_elem(const config_setting_t *setting, int index) { return 0; }

	const char *config_setting_get_string(const config_setting_t *setting) { return NULL; }
	const char *config_setting_get_string_elem(const config_setting_t *setting, int index) { return NULL; }
#endif

cfg_t cfg_cfg, cfg_dx;
	
static void cfg_error(config_t *config, const char *msg)
{
	lprintf("%s:%d - %s\n", config_error_file(config), config_error_line(config), config_error_text(config));
	config_destroy(config);
	panic(msg);
}

void _cfg_init(cfg_t *cfg, const char *filename)
{
	if (cfg->init) {
		config_destroy(&cfg->config);
	}
	
	lprintf("reading configuration from file %s\n", filename);
	cfg->filename = strdup(filename);
	config_init(&cfg->config);
	if (!config_read_file(&cfg->config, filename)) {
		lprintf("check that config file is installed in %s\n", filename);
		cfg_error(&cfg->config, filename);
	}

	cfg->init = true;
}

int _cfg_int(cfg_t *cfg, const char *name, int *val, u4_t flags)
{
	int num;
	if (!config_lookup_int(&cfg->config, name, &num)) {
		if (config_error_line(&cfg->config)) cfg_error(&cfg->config, "cfg_int");
		if (!(flags & CFG_REQUIRED)) return 0;
		lprintf("%s: required parameter not found: %s\n", cfg->filename, name);
		panic("cfg_int");
	}
	if (flags & CFG_PRINT) lprintf("%s: %s = %d\n", cfg->filename, name, num);
	if (val) *val = num;
	return num;
}

double _cfg_float(cfg_t *cfg, const char *name, double *val, u4_t flags)
{
	double num;
	if (!config_lookup_float(&cfg->config, name, &num)) {
		if (config_error_line(&cfg->config)) cfg_error(&cfg->config, "cfg_float");
		if (!(flags & CFG_REQUIRED)) return 0;
		lprintf("%s: required parameter not found: %s\n", cfg->filename, name);
		panic("cfg_float");
	}
	if (flags & CFG_PRINT) lprintf("%s: %s = %f\n", cfg->filename, name, num);
	if (val) *val = num;
	return num;
}

int _cfg_bool(cfg_t *cfg, const char *name, int *val, u4_t flags)
{
	int num;
	if (!config_lookup_bool(&cfg->config, name, &num)) {
		if (config_error_line(&cfg->config)) cfg_error(&cfg->config, "cfg_bool");
		if (!(flags & CFG_REQUIRED)) return NOT_FOUND;
		lprintf("%s: required parameter not found: %s\n", cfg->filename, name);
		panic("cfg_bool");
	}
	num = num? true : false;
	if (flags & CFG_PRINT) lprintf("%s: %s = %s\n", cfg->filename, name, num? "true":"false");
	if (val) *val = num;
	return num;
}

const char *_cfg_string(cfg_t *cfg, const char *name, const char **val, u4_t flags)
{
	const char *str;
	if (!config_lookup_string(&cfg->config, name, &str)) {
		if (config_error_line(&cfg->config)) cfg_error(&cfg->config, "cfg_string");
		if (!(flags & CFG_REQUIRED)) return NULL;
		lprintf("%s: required parameter not found: %s\n", cfg->filename, name);
		panic("cfg_string");
	}
	if (flags & CFG_PRINT) lprintf("%s: %s = %s\n", cfg->filename, name, str);
	if (val) *val = str;
	return str;
}

config_setting_t *_cfg_lookup(cfg_t *cfg, const char *path, u4_t flags)
{
	config_setting_t *setting;
	if ((setting = config_lookup(&cfg->config, path)) == NULL) {
		if (config_error_line(&cfg->config)) cfg_error(&cfg->config, "cfg_string");
		if (!(flags & CFG_REQUIRED)) return NULL;
		lprintf("%s: lookup parameter not found: %s\n", cfg->filename, path);
		panic("cfg_string");
	}
	return setting;
}

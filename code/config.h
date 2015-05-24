#ifndef _CONFIG_H_
#define _CONFIG_H_

// these defines are set by the makefile:
// DEBUG
// FIRMWARE_VER_MAJ, FIRMWARE_VER_MIN
// ARCH_*, PLATFORM_*
// LOGGING_HOST, WRX_UI_LIST
// {EDATA_DEVEL, EDATA_EMBED}

#define SERIAL_NUMBER	1000			// fixme: read from pcb eeprom instead

typedef struct {
	const char *param, *value;
} index_html_params_t;

#include "mongoose.h"

typedef struct {
	 int port;
	 const char *name;
	 int color_map;
	 double ui_srate;
	 struct mg_server *server;
} user_iface_t;

#ifdef _WRX_CONFIG_

index_html_params_t index_html_params[] = {
	{ "PAGE_TITLE",			"WRX prototype receiver" },
	{ "CLIENT_ID",			"jks" },
	{ "RX_PHOTO_HEIGHT",	"350+67" },
	{ "RX_PHOTO_TITLE",		"WRX prototype receiver" },
	{ "RX_PHOTO_DESC",		"Hand-wired prototype and first PCB layout" },
	{ "RX_TITLE",			"WRX prototype receiver at ZL/KF6VO" },
	{ "RX_LOC",				"Tauranga, New Zealand" },
	{ "RX_QRA",				"RF82" },
	{ "RX_ASL",				"30 m" },
	{ "RX_GPS",				"Tauranga/@-37.7039674,176.1586309,12z" },
};

user_iface_t user_iface[] = {
	WRX_UI_LIST
	{0}
};

#endif

#endif

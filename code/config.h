#ifndef _CONFIG_H_
#define _CONFIG_H_

// these defines are set by the makefile:
// DEBUG
// FIRMWARE_VER_MAJ, FIRMWARE_VER_MIN
// ARCH_*, PLATFORM_*
// LOGGING_HOST, KIWI_UI_LIST
// {EDATA_DEVEL, EDATA_EMBED}

#define SERIAL_NUMBER	1000	// fixme: read from pcb eeprom instead

#define	DYN_DNS_SERVER	"www.jks.com"

// applications
//#define	APP_WSPR

//#define USE_SPIDEV				// use SPI device driver instead of manipulating SPI hardware directly

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

#ifdef _KIWI_CONFIG_

index_html_params_t index_html_params[] = {
	{ "PAGE_TITLE",			"KiwiSDR" },
	{ "CLIENT_ID",			"jks" },
	{ "RX_PHOTO_HEIGHT",	"350+67" },
	{ "RX_PHOTO_TITLE",		"KiwiSDR: software-defined receiver" },
	{ "RX_PHOTO_DESC",		"First production PCB" },
	{ "RX_TITLE",			"KiwiSDR: Software-defined receiver at ZL/KF6VO" },
	{ "RX_LOC",				"Tauranga, New Zealand"},
	{ "RX_QRA",				"RF82ci" },
	{ "RX_ASL",				"30 m" },
	{ "RX_GMAP",			"Tauranga/@-37.7039674,176.1586309,12z" },
};

user_iface_t user_iface[] = {
	KIWI_UI_LIST
	{0}
};

#else
extern user_iface_t user_iface[];
#endif

#endif

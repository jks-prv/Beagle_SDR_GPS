#ifndef _CONFIG_H_
#define _CONFIG_H_

// these defines are set by the makefile:
// DEBUG
// FIRMWARE_VER_MAJ, FIRMWARE_VER_MIN
// ARCH_*, PLATFORM_*
// LOGGING_HOST, KIWI_UI_LIST
// {EDATA_DEVEL, EDATA_EMBED}

#define SERIAL_NUMBER	1000	// fixme: read from pcb eeprom instead

#define INSTALL_DIR		"/root/kiwi/"
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

extern user_iface_t user_iface[];

#endif

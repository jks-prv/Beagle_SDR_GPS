#ifndef _CONFIG_H_
#define _CONFIG_H_

// these defines are set by the makefile:
// DEBUG
// VERSION_MAJ, VERSION_MIN
// ARCH_*, PLATFORM_*
// LOGGING_HOST, KIWI_UI_LIST, REPO
// {EDATA_DEVEL, EDATA_EMBED}

// unfortunately, valgrind doesn't work on Debian 8 with our private thread scheme
//#define USE_VALGRIND

#define	DYN_DNS_SERVER	"www.kiwisdr.com"
#define	UPDATE_HOST		"www.kiwisdr.com"

#define	STATS_INTERVAL_SECS		10

// applications
//#define	APP_WSPR

// Debian 8 finally does DMA for transfers > 160 bytes
// When making a permanent change here, set the corresponding value in
// ./k and ./unix_env/kiwid
#define USE_SPIDEV		// use SPI device driver instead of manipulating SPI hardware directly

typedef struct {
	const char *param, *value;
} index_html_params_t;

#include "mongoose.h"

enum compression_e { COMPRESSION_NONE, COMPRESSION_ADPCM };

typedef struct {
	 int port;
	 const char *name;
	 int color_map;
	 double ui_srate;
	 struct mg_server *server;
} user_iface_t;

extern user_iface_t user_iface[];

#endif

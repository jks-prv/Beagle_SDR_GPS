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

#define	STATS_INTERVAL_SECS			10
#define	INACTIVITY_WARNING_SECS		10

typedef struct {
	const char *param, *value;
} index_html_params_t;

#include "mongoose.h"

typedef struct {
	 int port;
	 const char *name;
	 int color_map;
	 struct mg_server *server;
} user_iface_t;

extern user_iface_t user_iface[];

#endif

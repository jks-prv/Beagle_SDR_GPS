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

#define KIWISDR_COM_PUBLIC_IP   "103.26.16.225"

#define	STATS_INTERVAL_SECS			10
#define	INACTIVITY_WARNING_SECS		10

#define PHOTO_UPLOAD_MAX_SIZE (2*M)

typedef struct {
	const char *param, *value;
} index_html_params_t;

#include "mongoose.h"

typedef struct {
	 const char *name;
	 int color_map;
	 int port, port_ext;
	 struct mg_server *server;
} user_iface_t;

extern user_iface_t user_iface[];

#endif

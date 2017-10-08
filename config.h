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

// backup values only if dig lookup fails
#define KIWISDR_COM_PUBLIC_IP   "50.116.2.70"
#define SDR_HU_PUBLIC_IP        "174.138.38.40"

// INET6_ADDRSTRLEN (46) plus some extra in case ipv6 scope/zone is an issue
// can't be in net.h due to #include recursion problems
#define NET_ADDRSTRLEN      64
#define NET_ADDRSTRLEN_S    "64"

#define	STATS_INTERVAL_SECS			10

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

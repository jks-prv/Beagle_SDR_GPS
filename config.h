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

// these defines are set by the makefile:
// DEBUG
// VERSION_MAJ, VERSION_MIN
// ARCH_*, PLATFORM_*
// LOGGING_HOST, KIWI_UI_LIST, REPO
// {EDATA_DEVEL, EDATA_EMBED}

// backup values only if dig lookup fails
#define KIWISDR_COM_PUBLIC_IP   "50.116.2.70"
#define SDR_HU_PUBLIC_IP        "167.99.214.222"

// INET6_ADDRSTRLEN (46) plus some extra in case ipv6 scope/zone is an issue
// can't be in net.h due to #include recursion problems
#define NET_ADDRSTRLEN      64
#define NET_ADDRSTRLEN_S    "64"

#define	N_EXT	16		// max # of different extensions

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

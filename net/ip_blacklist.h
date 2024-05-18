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

// Copyright (c) 2022-2023 John Seamons, ZL4VO/KF6VO

#pragma once

#define USE_IPSET

//#define IP_BL_DBG
#ifdef IP_BL_DBG
	#define ipbl_prf(fmt, ...) \
	    printf(fmt,  ## __VA_ARGS__);
#else
	#define ipbl_prf(fmt, ...)
#endif

//#define IP_BL_DBG2
#ifdef IP_BL_DBG2
	#define ipbl_prf2(fmt, ...) \
	    printf(fmt,  ## __VA_ARGS__);
	#define ipbl_dbg(x) x
#else
	#define ipbl_prf2(fmt, ...)
	#define ipbl_dbg(x)
#endif

#define N_IP_BLACKLIST 512
#define N_IP_BLACKLIST_HASH_BYTES 4     // 4 bytes = 8 chars

typedef struct {
    u4_t dropped;
    u4_t ip;        // ipv4
    u1_t a,b,c,d;   // ipv4
    u4_t nm;        // ipv4
    u1_t cidr;
    bool whitelist;
    u4_t last_dropped;
} ip_blacklist_t;

void ip_blacklist_system(const char *cmd);
void ip_blacklist_disable();
void ip_blacklist_enable();
#define USE_IPTABLES true
int ip_blacklist_add_iptables(char *ip_s, bool use_iptables = false);
void ip_blacklist_init();
bool check_ip_blacklist(char *remote_ip, bool log = false);
void ip_blacklist_dump(bool show_all);

#define BL_DOWNLOAD_RELOAD          0
#define BL_DOWNLOAD_DIFF_RESTART    1
bool ip_blacklist_get(bool download_diff_restart);

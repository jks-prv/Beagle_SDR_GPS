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

// Copyright (c) 2014-2016 John Seamons, ZL/KF6VO

#pragma once

#include "types.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>

// dot to host (little-endian) conversion
#define INET4_DTOH(a, b, c, d) \
	(((a)&0xff)<<24) | (((b)&0xff)<<16) | (((c)&0xff)<<8) | ((d)&0xff)

// dot to network (big endian) conversion
#define INET4_DTON(a, b, c, d) \
	(((d)&0xff)<<24) | (((c)&0xff)<<16) | (((b)&0xff)<<8) | ((a)&0xff)

// network (big endian) to host (little endian) conversion
#define INET4_NTOH(u32) \
	FLIP32(u32)

struct ddns_t {
	bool valid, pub_valid;
	u4_t serno;
	char ip_pub[NI_MAXHOST], *ip_pvt;
	int port;
	int nm_bits;
	char mac[64];
	
	// IPv4
	bool ip4_valid;
	char ip4_pvt_s[NI_MAXHOST];
	u4_t ip4_pvt;
	u4_t netmask4;

	// IPv4-mapped IPv6
	bool ip4_6_valid;
	char ip4_6_pvt_s[NI_MAXHOST];
	u4_t ip4_6_pvt;
	u4_t netmask4_6;

	// IPv6
	bool ip6_valid;
	char ip6_pvt_s[NI_MAXHOST];
	u1_t ip6_pvt[16];
	u1_t netmask6[16];

	// IPv6 link-local
	bool ip6LL_valid;
	char ip6LL_pvt_s[NI_MAXHOST];
	u1_t ip6LL_pvt[16];
	u1_t netmask6LL[16];
};

extern ddns_t ddns;

bool find_local_IPs();
bool isLocal_IP(char *remote_ip_s, bool print);
u4_t inet4_d2h(char *inet4_str);
bool is_inet4_map_6(u1_t *a);
int inet_nm_bits(int family, void *netmask);

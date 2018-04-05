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
#include "mongoose.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>

// dot to host (little-endian) conversion
#define INET4_DTOH(a, b, c, d) \
	( (((a)&0xff)<<24) | (((b)&0xff)<<16) | (((c)&0xff)<<8) | ((d)&0xff) )

// dot to network (big endian) conversion
#define INET4_DTON(a, b, c, d) \
	( (((d)&0xff)<<24) | (((c)&0xff)<<16) | (((b)&0xff)<<8) | ((a)&0xff) )

// network (big endian) to host (little endian) conversion
#define INET4_NTOH(u32) \
	FLIP32(u32)

struct ip_lookup_t {
    bool valid, backup;
    int n_ips;
	#define N_IPS 16
	char *ip_list[N_IPS];
};

struct ddns_t {
	bool valid, pub_valid;
	int auto_nat;
	u4_t serno;
    u64_t dna;
	char ip_pub[NET_ADDRSTRLEN];
	int port, port_ext;
	char mac[64];
	
    ip_lookup_t ips_kiwisdr_com, ips_sdr_hu;

	ip_lookup_t pub_ips;
	bool pub_server;	// this kiwi is one of the public.kiwisdr.com servers
	
	bool lat_lon_valid;
	double lat, lon;

	// set by find_local_IPs()
	char *ip_pvt;
	int nm_bits;
	
	// IPv4
	bool ip4_valid;
	char ip4_pvt_s[NET_ADDRSTRLEN];
	u4_t ip4_pvt;
	u4_t netmask4;
	int nm_bits4;

	// IPv4LL
	bool ip4LL;

	// IPv4-mapped IPv6
	bool ip4_6_valid;
	char ip4_6_pvt_s[NET_ADDRSTRLEN];
	u4_t ip4_6_pvt;
	u4_t netmask4_6;
	int nm_bits4_6;

	// IPv6
	bool ip6_valid;
	char ip6_pvt_s[NET_ADDRSTRLEN];
	u1_t ip6_pvt[16];
	u1_t netmask6[16];
	int nm_bits6;

	// IPv6 link-local
	bool ip6LL_valid;
	char ip6LL_pvt_s[NET_ADDRSTRLEN];
	u1_t ip6LL_pvt[16];
	u1_t netmask6LL[16];
	int nm_bits6LL;
};

extern ddns_t ddns;

enum isLocal_t { IS_NOT_LOCAL, IS_LOCAL, NO_LOCAL_IF };
isLocal_t isLocal_if_ip(conn_t *conn, char *ip_addr, const char *log_prefix);

bool find_local_IPs();
u4_t inet4_d2h(char *inet4_str, bool *error, u4_t *ap, u4_t *bp, u4_t *cp, u4_t *dp);
bool is_inet4_map_6(u1_t *a);
int inet_nm_bits(int family, void *netmask);
bool isLocal_ip(char *ip);

int DNS_lookup(const char *domain_name, ip_lookup_t *r_ips, int n_ips, const char *ip_backup);
bool ip_match(const char *ip, ip_lookup_t *ips);

char *ip_remote(struct mg_connection *mc);
void check_if_forwarded(const char *id, struct mg_connection *mc, char *remote_ip);


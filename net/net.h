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

// Copyright (c) 2014-2016 John Seamons, ZL4VO/KF6VO

#pragma once

#include "types.h"
#include "ip_blacklist.h"

#include <sys/types.h>
#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>

// backup values only if dig lookup fails
#define KIWISDR_COM_PUBLIC_IP   "50.116.2.70"
#define GITHUB_COM_PUBLIC_IP    "52.64.108.95"      // was "192.30.253.112"

// range of port base: + 0 .. MAX_RX_CHANS(instance) * 3(SND/WF/EXT)
#define PORT_BASE_INTERNAL_WSPR     1138
#define PORT_BASE_INTERNAL_FT8      1238
#define PORT_BASE_INTERNAL_SNR      1338
#define PORT_BASE_INTERNAL_S_METER  1438

#define NET_DEBUG
#ifdef NET_DEBUG
	#define net_printf(fmt, ...) \
	    if (debug_printfs) lprintf("NET(%d) DEBUG " fmt, retry, ## __VA_ARGS__)
	#define net_printf2(fmt, ...) \
	    if (debug_printfs) lprintf(fmt, ## __VA_ARGS__)
#else
	#define net_printf(fmt, ...)
	#define net_printf2(fmt, ...)
#endif

#define NET_WAIT_COND(caller, from, cond) \
    if (!(cond)) { \
        net_printf2("NET_WAIT_COND %s %s(%s): waiting...\n", #cond, caller, from); \
        while (!(cond)) \
            TaskSleepSec(5); \
        net_printf2("NET_WAIT_COND %s %s(%s): ...wakeup\n", #cond, caller, from); \
    }

typedef enum { IPV_NONE = 0, IPV4 = 4, IPV6 = 6 } ipv46_e;

// dot to host (little-endian) conversion
#define INET4_DTOH(a, b, c, d) \
	( (((a)&0xff)<<24) | (((b)&0xff)<<16) | (((c)&0xff)<<8) | ((d)&0xff) )

// dot to network (big endian) conversion
#define INET4_DTON(a, b, c, d) \
	( (((d)&0xff)<<24) | (((c)&0xff)<<16) | (((b)&0xff)<<8) | ((a)&0xff) )

// network (big endian) to host (little endian) conversion
#define INET4_NTOH(u32) \
	FLIP32(u32)

typedef struct {
    bool valid, backup;
    int n_ips;
	#define N_IPS 16
	char *ip_list[N_IPS];
	u4_t ip[N_IPS];
} ip_lookup_t;

enum { BL_PORT_DEFAULT = 0, BL_PORT_YES = 1, BL_PORT_NO = 2 };

typedef struct {
    // set by init_NET()
	ipv46_e pvt_valid;

	// set by find_local_IPs()
	char *ip_pvt;
	int nm_bits;
	
	// set by ipinfo_json()
	bool pub_valid;
	char ip_pub[NET_ADDRSTRLEN];
	int port, port_ext;
	int port_http_local;
	bool use_ssl;

    bool mac_valid;
	char mac[32], mac_no_delim[16];
	
	bool auto_nat_valid;
	int auto_nat;
	
	u4_t serno;
    u64_t dna;
    
    int proxy_status, DUC_status;
	
	// IPv4
	bool ip4_valid;
	char *ip4_if_name;
	char ip4_pvt_s[NET_ADDRSTRLEN];
	u4_t ip4_pvt;
	u4_t netmask4;
	int nm_bits4;

	// IPv4LL
	bool ip4LL;

	// IPv4-mapped IPv6
	bool ip4_6_valid;
	char *ip4_6_if_name;
	char ip4_6_pvt_s[NET_ADDRSTRLEN];
	u4_t ip4_6_pvt;
	u4_t netmask4_6;
	int nm_bits4_6;

	// IPv6
	#define N_IPV6 8
	int ip6_valid;
	char *ip6_if_name;
	char ip6_pvt_s[N_IPV6][NET_ADDRSTRLEN];
	u1_t ip6_pvt[N_IPV6][16];
	u1_t netmask6[N_IPV6][16];
	int nm_bits6[N_IPV6];

	// IPv6 link-local
	bool ip6LL_valid;
	char *ip6LL_if_name;
	char ip6LL_pvt_s[NET_ADDRSTRLEN];
	u1_t ip6LL_pvt[16];
	u1_t netmask6LL[16];
	int nm_bits6LL;

    ip_lookup_t ips_kiwisdr_com;
    
    bool ip_blacklist_inuse, ip_blacklist_update_busy;
    int ip_blacklist_port_only;
    int ip_blacklist_len;
    ip_blacklist_t ip_blacklist[N_IP_BLACKLIST];
    char ip_blacklist_hash[N_IP_BLACKLIST_HASH_BYTES*2 + SPACE_FOR_NULL];
    FILE *isf;
} net_t;

// (net_t) net located in shmem for benefit of e.g. led task
// #include needs to be below definition of net_t
#include "shmem.h"      // shmem->net_shmem
#define net shmem->net_shmem

typedef enum { IS_NOT_LOCAL, IS_LOCAL, NO_LOCAL_IF } isLocal_t;
// "struct conn_st" because of forward reference from inclusion by conn.h
struct conn_st;
isLocal_t isLocal_if_ip(struct conn_st *conn, char *ip_addr, const char *log_prefix);

bool find_local_IPs(int retry);
u4_t inet4_d2h(char *inet4_str, bool *error = NULL, u1_t *ap = NULL, u1_t *bp = NULL, u1_t *cp = NULL, u1_t *dp = NULL);
void inet4_h2d(u4_t inet4, u1_t *ap, u1_t *bp, u1_t *cp, u1_t *dp);
char *inet4_h2s(u4_t inet4, int which = 0);
bool is_inet4_map_6(u1_t *a);
int inet_nm_bits(int family, void *netmask);
bool isLocal_ip(char *ip, bool *is_loopback = NULL, u4_t *ipv4 = NULL);

int DNS_lookup(const char *domain_name, ip_lookup_t *r_ips, int n_ips, const char *ip_backup = NULL);
char *DNS_lookup_result(const char *caller, const char *host, ip_lookup_t *ips);
bool ip_match(const char *ip, ip_lookup_t *ips);

char *ip_remote(struct mg_connection *mc);
bool check_if_forwarded(const char *id, struct mg_connection *mc, char *remote_ip);


typedef struct {
    struct mg_connection snd_mc, wf_mc, ext_mc;
    conn_t *csnd, *cwf, *cext;
    void *param;
} internal_conn_t;

const u4_t ICONN_WS_SND = 1, ICONN_WS_WF = 2, ICONN_WS_EXT = 4;

bool internal_conn_setup(u4_t ws, internal_conn_t *iconn, int instance, int port_base, u4_t ws_flags,
    const char *mode, int locut, int hicut, float freq_kHz,
    const char *ident_user, const char *geoloc, const char *client = NULL,
    int zoom = 0, float cf_kHz = 0, int min_dB = 0, int max_dB = 0, int wf_speed = 0, int wf_comp = 0);

void internal_conn_shutdown(internal_conn_t *iconn);

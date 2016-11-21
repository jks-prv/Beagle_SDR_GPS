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

#include "kiwi.h"
#include "types.h"
#include "config.h"
#include "misc.h"
#include "timer.h"
#include "web.h"
#include "coroutines.h"
#include "mongoose.h"
#include "nbuf.h"
#include "cfg.h"
#include "net.h"

#include <string.h>
#include <time.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <ifaddrs.h>

//#define IPV6_TEST

ddns_t ddns;

// determine all possible IPv4, IPv4-mapped IPv6 and IPv6 addresses on eth0 interface
bool find_local_IPs()
{
	int i;
	struct ifaddrs *ifaddr, *ifa;
	scall("getifaddrs", getifaddrs(&ifaddr));
	
	for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
		if (ifa->ifa_addr == NULL)
			continue;
		
		int family = ifa->ifa_addr->sa_family;
		if (strcmp(ifa->ifa_name, "eth0") != 0 || (family != AF_INET && family != AF_INET6)) {
			//printf("getifaddrs: %s fam=%d\n", ifa->ifa_name, family);
			continue;
		}

		char *ip_pvt;
		socklen_t salen;

		if (family == AF_INET) {
			struct sockaddr_in *sin = (struct sockaddr_in *) (ifa->ifa_addr);
			ddns.ip4_pvt = INET4_NTOH(* (u4_t *) &sin->sin_addr);
			ip_pvt = ddns.ip4_pvt_s;
			salen = sizeof(struct sockaddr_in);
			sin = (struct sockaddr_in *) (ifa->ifa_netmask);
			ddns.netmask4 = INET4_NTOH(* (u4_t *) &sin->sin_addr);

			#ifndef IPV6_TEST
				ddns.ip4_valid = true;
			#endif
		} else {
			struct sockaddr_in6 *sin6;
			sin6 = (struct sockaddr_in6 *) (ifa->ifa_addr);
			u1_t *a = (u1_t *) &(sin6->sin6_addr);
			sin6 = (struct sockaddr_in6 *) (ifa->ifa_netmask);
			u1_t *m = (u1_t *) &(sin6->sin6_addr);

			if (is_inet4_map_6(a)) {
				ddns.ip4_6_pvt = INET4_NTOH(* (u4_t *) &a[12]);
				ip_pvt = ddns.ip4_6_pvt_s;
				salen = sizeof(struct sockaddr_in);
				ddns.netmask4_6 = INET4_NTOH(* (u4_t *) &m[12]);
				ddns.ip4_6_valid = true;
			} else {
				if (a[0] == 0xfe && a[1] == 0x80) {
					memcpy(ddns.ip6LL_pvt, a, sizeof(ddns.ip6LL_pvt));
					ip_pvt = ddns.ip6LL_pvt_s;
					salen = sizeof(struct sockaddr_in6);
					memcpy(ddns.netmask6LL, m, sizeof(ddns.netmask6LL));
					ddns.ip6LL_valid = true;
				} else {
					memcpy(ddns.ip6_pvt, a, sizeof(ddns.ip6_pvt));
					ip_pvt = ddns.ip6_pvt_s;
					salen = sizeof(struct sockaddr_in6);
					memcpy(ddns.netmask6, m, sizeof(ddns.netmask6));
	
					#ifdef IPV6_TEST
						ddns.netmask6[8] = 0xff;
					#endif
					
					ddns.ip6_valid = true;
				}
			}
		}
		
		int rc = getnameinfo(ifa->ifa_addr, salen, ip_pvt, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
		if (rc != 0) {
			lprintf("getnameinfo() failed: %s, fam=%d %s\n", ifa->ifa_name, family, gai_strerror(rc));
			continue;
		}
	}
	
	if (ddns.ip4_valid) {
		ddns.nm_bits4 = inet_nm_bits(AF_INET, &ddns.netmask4);
		lprintf("DDNS: private IPv4 <%s> 0x%08x /%d 0x%08x\n", ddns.ip4_pvt_s, ddns.ip4_pvt,
			ddns.nm_bits4, ddns.netmask4);
	}

	if (ddns.ip4_6_valid) {
		ddns.nm_bits4_6 = inet_nm_bits(AF_INET, &ddns.netmask4_6);
		lprintf("DDNS: private IPv4_6 <%s> 0x%08x /%d 0x%08x\n", ddns.ip4_6_pvt_s, ddns.ip4_6_pvt,
			ddns.nm_bits4_6, ddns.netmask4_6);
	}

	if (ddns.ip6_valid) {
		ddns.nm_bits6 = inet_nm_bits(AF_INET6, &ddns.netmask6);
		lprintf("DDNS: private IPv6 <%s> /%d ", ddns.ip6_pvt_s, ddns.nm_bits6);
		for (i=0; i < 16; i++) lprintf("%02x:", ddns.netmask6[i]);
		lprintf("\n");
	}

	if (ddns.ip6LL_valid) {
		ddns.nm_bits6LL = inet_nm_bits(AF_INET6, &ddns.netmask6LL);
		lprintf("DDNS: private IPv6 LINK-LOCAL <%s> /%d ", ddns.ip6LL_pvt_s, ddns.nm_bits6LL);
		for (i=0; i < 16; i++) lprintf("%02x:", ddns.netmask6LL[i]);
		lprintf("\n");
	}

	// assign ddns.ip_pvt & ddns.nm_bits in priority order, IPV4 first
	if (ddns.ip4_valid) {
		ddns.ip_pvt = ddns.ip4_pvt_s;
		ddns.nm_bits = ddns.nm_bits4;
	} else
	if (ddns.ip4_6_valid) {
		ddns.ip_pvt = ddns.ip4_6_pvt_s;
		ddns.nm_bits = ddns.nm_bits4_6;
	} else
	if (ddns.ip6_valid) {
		ddns.ip_pvt = ddns.ip6_pvt_s;
		ddns.nm_bits = ddns.nm_bits6;
	} else
	if (ddns.ip6LL_valid) {
		ddns.ip_pvt = ddns.ip6LL_pvt_s;
		ddns.nm_bits = ddns.nm_bits6LL;
	}
	
	freeifaddrs(ifaddr);
	
	return (ddns.ip4_valid || ddns.ip4_6_valid || ddns.ip6_valid || ddns.ip6LL_valid);
}


// Find all possible client IP addresses, IPv4 or IPv6, and compare against all our
// server IPv4 or IPv6 addresses on the eth0 interface looking for a local network match.

bool isLocal_IP(char *remote_ip_s, bool print)
{
	int i, rc;
	
	struct addrinfo *res, *rp, hint;

	// So it seems getaddrinfo() doesn't work as we expect for IPv4 addresses.
	// If a remote (client) host has both IPv4 and IPv6 addresses then you might expect
	// getaddrinfo() to return an ai_family == AF_INET in the results list.
	//
	// But it doesn't IF the library of Internet address handling routines OR the network stack has
	// decided we are using IPv6 addresses locally on the server! This happens because we now always use
	// an IPv6 address when we specify the server listening port, i.e. [::]:8073 in web.c
	// No matter what combination of "hints" flags we give the IPv4 entries always come
	// back as "IPv4-mapped IPv6", i.e. ai_family == AF_INET6. So we must parse this case to decode
	// the IPv4-mapped address.

	#ifdef IPV6_TEST
		remote_ip_s = (char *) "fe80:0000:0000:0000:beef:feed:cafe:babe";
	#endif

	/*
	if (print) printf("isLocal_IP: remote_ip_s=%s AF_INET=%d AF_INET6=%d IPPROTO_TCP=%d IPPROTO_UDP=%d\n",
		remote_ip_s, AF_INET, AF_INET6, IPPROTO_TCP, IPPROTO_UDP);
	if (print) printf("isLocal_IP: AI_V4MAPPED=0x%02x AI_ALL=0x%02x AI_ADDRCONFIG=0x%02x AI_NUMERICHOST=0x%02x AI_PASSIVE=0x%02x\n",
		AI_V4MAPPED, AI_ALL, AI_ADDRCONFIG, AI_NUMERICHOST, AI_PASSIVE);
	*/
	
	memset(&hint, 0, sizeof(hint));
	hint.ai_flags = AI_V4MAPPED | AI_ALL;
	hint.ai_family = AF_UNSPEC;
	
	rc = getaddrinfo(remote_ip_s, NULL, &hint, &res);
	if (rc != 0) {
		if (print) lprintf("isLocal_IP getaddrinfo: %s FAILED %s\n", remote_ip_s, gai_strerror(rc));
		return false;
	}
	
	bool is_local = false;
	int check = 0, n;
	for (rp = res, n=0; rp != NULL; rp = rp->ai_next, n++) {

		union socket_address {
			struct sockaddr sa;
			struct sockaddr_in sin;
			struct sockaddr_in6 sin6;
		} sa, *s;
		
		s = &sa;
		struct sockaddr *sa_p = &s->sa;
		socklen_t salen;
		int family = rp->ai_family;
		sa_p->sa_family = family;
		int proto = rp->ai_protocol;

		//if (print) printf("isLocal_IP: AI%d flg=0x%x fam=%d socktype=%d proto=%d addrlen=%d\n",
		//	n, rp->ai_flags, family, rp->ai_socktype, rp->ai_protocol, rp->ai_addrlen);

		if (proto != IPPROTO_TCP)
			continue;

		struct sockaddr_in *sin;
		struct sockaddr_in6 *sin6;
		
		if (family == AF_INET) {
			sin = (struct sockaddr_in *) (rp->ai_addr);
			s->sin = *sin;
			salen = sizeof(struct sockaddr_in);
			check++;
		} else
		if (family == AF_INET6) {
			sin6 = (struct sockaddr_in6 *) (rp->ai_addr);
			s->sin6 = *sin6;
			salen = sizeof(struct sockaddr_in6);
			check++;
		} else
			continue;
		
		// get numeric name string for this address
		char ip_client_s[NI_MAXHOST];
		ip_client_s[0] = 0;
		int rc = getnameinfo(sa_p, salen, ip_client_s, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
		if (rc != 0) {
			lprintf("getnameinfo() failed: %s\n", gai_strerror(rc));
			continue;
		}
		
		if (print) printf("isLocal_IP: flg=0x%x fam=%d socktype=%d proto=%d addrlen=%d %s\n",
			rp->ai_flags, family, rp->ai_socktype, rp->ai_protocol, rp->ai_addrlen, ip_client_s);

		// do local network check depending on type of client address: IPv4, IPv4-mapped IPv6, IPv6 or IPv6LL
		u4_t ip_server, ip_client;
		u1_t *ip_server6, *ip_client6;
		const char *ips_type;
		char *ip_server_s;
		u4_t nm_bits, netmask;
		u1_t *netmask6;

		bool ipv4_test = false;
		if (family == AF_INET) {
			ip_client = INET4_NTOH(* (u4_t *) &sin->sin_addr);

			if (ddns.ip4_valid) {
				ips_type = "IPv4";
				ip_server = ddns.ip4_pvt;
				ip_server_s = ddns.ip4_pvt_s;
				netmask = ddns.netmask4;
			} else
			if (ddns.ip4_6_valid) {
				ips_type = "IPv4_6";
				ip_server = ddns.ip4_6_pvt;
				ip_server_s = ddns.ip4_6_pvt_s;
				netmask = ddns.netmask4_6;
			} else {
				if (print) lprintf("isLocal_IP: IPv4 client, but no server IPv4/IPv4_6\n");
				continue;
			}

			nm_bits = inet_nm_bits(AF_INET, &netmask);
			ipv4_test = true;
		} else

		// detect IPv4-mapped IPv6 (::ffff/96 i.e. 0:0:0:0:0:ffff:a.b.c.d/96)
		if (family == AF_INET6) {
			u1_t *a = (u1_t *) &sin6->sin6_addr;

			if (is_inet4_map_6(a)) {
				ip_client = INET4_NTOH(* (u4_t *) &a[12]);
				
				if (ddns.ip4_6_valid) {
					ips_type = "IPv4_6";
					ip_server = ddns.ip4_6_pvt;
					ip_server_s = ddns.ip4_6_pvt_s;
					netmask = ddns.netmask4_6;
				} else
				if (ddns.ip4_valid) {
					ips_type = "IPv4";
					ip_server = ddns.ip4_pvt;
					ip_server_s = ddns.ip4_pvt_s;
					netmask = ddns.netmask4;
				} else {
					if (print) lprintf("isLocal_IP: IPv4_6 client, but no server IPv4 or IPv4_6\n");
					continue;
				}
					
				nm_bits = inet_nm_bits(AF_INET, &netmask);
				ipv4_test = true;
			} else {
				if (ddns.ip6_valid) {
					ip_client6 = a;
					ips_type = "IPv6";
					ip_server6 = ddns.ip6_pvt;
					ip_server_s = ddns.ip6_pvt_s;
					netmask6 = ddns.netmask6;
					nm_bits = inet_nm_bits(AF_INET6, netmask6);
				} else
				if (ddns.ip6LL_valid) {
					ip_client6 = a;
					ips_type = "IPv6LL";
					ip_server6 = ddns.ip6LL_pvt;
					ip_server_s = ddns.ip6LL_pvt_s;
					netmask6 = ddns.netmask6LL;
					nm_bits = inet_nm_bits(AF_INET6, netmask6);
				} else {
					if (print) lprintf("isLocal_IP: IPv6 client, but no server IPv6\n");
					continue;
				}
			}
		}
		
		// This makes the critical assumption that an ip_client coming from "outside" the
		// local subnet could never match with a local subnet address.
		// i.e. that local address space is truly un-routable across the internet or local subnet.
		
		bool local;
		if (ipv4_test) {
			local = ((ip_client & netmask) == (ip_server & netmask));
			if (print) lprintf("isLocal_IP %s IPv4/4_6 remote_ip %s ip_client %s/0x%08x ip_server[%s] %s/0x%08x nm /%d 0x%08x\n",
				local? "TRUE":"FALSE", remote_ip_s, ip_client_s, ip_client, ips_type, ip_server_s, ip_server, nm_bits, netmask);
		} else {
			for (i=0; i < 16; i++) {
				local = ((ip_client6[i] & netmask6[i]) == (ip_server6[i] & netmask6[i]));
				if (local == false)
					break;
			}
			if (print) lprintf("isLocal_IP %s IPv6 remote_ip %s ip_client %s ip_server[%s] %s nm /%d\n",
				local? "TRUE":"FALSE", remote_ip_s, ip_client_s, ips_type, ip_server_s, nm_bits);
		}
		if (local) is_local = true;
	}
	
	freeaddrinfo(res);

	if (res == NULL || check == 0 ) {
		if (print) lprintf("isLocal_IP getaddrinfo: %s NO CLIENT RESULTS?\n", remote_ip_s);
		is_local = false;
	}
	
	return is_local;
}

u4_t inet4_d2h(char *inet4_str)
{
	int n;
	u4_t a, b, c, d;
	n = sscanf(inet4_str, "%d.%d.%d.%d", &a, &b, &c, &d);
	if (n != 4) {
		n = sscanf(inet4_str, "::ffff:%d.%d.%d.%d", &a, &b, &c, &d); //IPv4-mapped address
		if (n != 4)
			return 0xffffffff; //IPv6
	}
	return INET4_DTOH(a, b, c, d);
}

bool is_inet4_map_6(u1_t *a)
{
	int i;
	for (i = 0; i < 10; i++) {
		if (a[i] != 0)
			break;
	}
	if (i == 10 && a[10] == 0xff && a[11] == 0xff)
		return true;

	return false;
}

int inet_nm_bits(int family, void *netmask)
{
	int i, b, nm_bits;

	if (family == AF_INET) {
		u4_t nm = * (u4_t *) netmask;
		for (i=0; i < 32; i++) {
			if (nm & (1 << i))
				break;
		}
		nm_bits = 32 - i;
	} else

	if (family == AF_INET6) {
		for (b = 15; b >= 0; b--) {
			u1_t nm = ((u1_t *) netmask)[b];
			if (nm) {
				for (i=0; i < 8; i++) {
					if (nm & (1 << i))
						break;
				}
				if (i < 8)
					break;
			} else
				continue;
		}

		if (b > 0)
			nm_bits = 128 - ((15-b)*8 + i);
		else
			nm_bits = 0;
	} else
		panic("inet_nm_bits");

	return nm_bits;
}

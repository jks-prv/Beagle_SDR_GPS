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

#include "kiwi.h"
#include "types.h"
#include "config.h"
#include "test.h"
#include "mem.h"
#include "misc.h"
#include "timer.h"
#include "web.h"
#include "coroutines.h"
#include "mongoose.h"
#include "nbuf.h"
#include "cfg.h"
#include "net.h"
#include "rx_util.h"

#include <string.h>
#include <time.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <ifaddrs.h>

//#define TEST_DELAYED_IPV4
static int test_delayed_ipv4 = 4;

//#define IPV6_TEST

// determine all possible IPv4, IPv4-mapped IPv6 and IPv6 addresses on local network interfaces
bool find_local_IPs(int retry)
{
	int i, j;
	struct ifaddrs *ifaddr, *ifa;
	scall("getifaddrs", getifaddrs(&ifaddr));
	
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL)
            continue;
        
        int family = ifa->ifa_addr->sa_family;
        int flags = ifa->ifa_flags;
        net_printf("getifaddrs: IF %s fam=%d(%s) flags=0x%x%s%s | ", ifa->ifa_name,
            family, (family == AF_INET)? "ipv4" : ((family == AF_INET6)? "ipv6" : "other"),
            flags, (flags & IFF_UP)? " UP":"", (flags & IFF_RUNNING)? " RUNNING":"");

        if (family != AF_INET && family != AF_INET6) {
            net_printf2("BAD: family\n");
            continue;
        }
        
        //bool is_eth0 = kiwi_str_begins_with(ifa->ifa_name, "eth0");
        // require IFF_RUNNING for eth0 interface because of problems we've seen where
        // eth0 is marked IFF_UP without the cable connected!
        // But, decided don't want to do this because of possible side-effects.
        // Instead, add an option to admin security tab that disables local network check.
        //if ((is_eth0 && (!(flags & IFF_UP) || !(flags & IFF_RUNNING))) ||
        //    (!is_eth0 && !(flags & IFF_UP))) {
        //    net_printf2("BAD: not UP (eth0: and RUNNING)\n");
        
        if (!(flags & IFF_UP)) {
            net_printf2("BAD: not UP\n");
            continue;
        }

        if (strcmp(ifa->ifa_name, "lo") == 0 || kiwi_str_begins_with(ifa->ifa_name, "usb") || kiwi_str_begins_with(ifa->ifa_name, "SoftAp")) {
            net_printf2("BAD: IF\n");
            continue;
        }

        bool is_ipv4LL = (kiwi_str_ends_with(ifa->ifa_name, ":avahi") != NULL);
        
        net_printf2("OK\n");

        char *ip_pvt;
        socklen_t salen;
        bool multiple = false;

        if (family == AF_INET) {
            #ifdef TEST_DELAYED_IPV4
                if (test_delayed_ipv4-- != 0) {
                    net_printf("TEST_DELAYED_IPV4\n");
                    goto rtn;
                }
            #endif
            
            if (!net.ip4_valid) {
                net.ip4_if_name = strdup(ifa->ifa_name);
                struct sockaddr_in *sin = (struct sockaddr_in *) (ifa->ifa_addr);
                net.ip4_pvt = INET4_NTOH(* (u4_t *) &sin->sin_addr);
                ip_pvt = net.ip4_pvt_s;
                salen = sizeof(struct sockaddr_in);
                sin = (struct sockaddr_in *) (ifa->ifa_netmask);
                net.netmask4 = INET4_NTOH(* (u4_t *) &sin->sin_addr);
                net_printf("IF IPv4 0x%08x /%d 0x%08x %s\n", net.ip4_pvt,
                    inet_nm_bits(AF_INET, &net.netmask4), net.netmask4, net.ip4_if_name);
            
                // FIXME: if ip4LL, because a DHCP server wasn't responding, need to periodically reprobe
                // for when it comes back online
                if (is_ipv4LL) net.ip4LL = true;

                #ifdef IPV6_TEST
                #else
                    net.ip4_valid = true;
                #endif
            } else {
                multiple = true;
                net_printf("BAD: MULTIPLE ipv4 ADDRESS\n");
            }
        } else {
            struct sockaddr_in6 *sin6;
            sin6 = (struct sockaddr_in6 *) (ifa->ifa_addr);
            u1_t *a = (u1_t *) &(sin6->sin6_addr);
            sin6 = (struct sockaddr_in6 *) (ifa->ifa_netmask);
            u1_t *m = (u1_t *) &(sin6->sin6_addr);

            if (is_inet4_map_6(a)) {
                if (!net.ip4_6_valid) {
                    net.ip4_6_if_name = strdup(ifa->ifa_name);
                    net.ip4_6_pvt = INET4_NTOH(* (u4_t *) &a[12]);
                    ip_pvt = net.ip4_6_pvt_s;
                    salen = sizeof(struct sockaddr_in);
                    net.netmask4_6 = INET4_NTOH(* (u4_t *) &m[12]);
                    net_printf("IF IPv4_6 0x%08x /%d 0x%08x %s\n", net.ip4_6_pvt,
                        inet_nm_bits(AF_INET, &net.netmask4_6), net.netmask4_6, net.ip4_6_if_name);
                    net.ip4_6_valid = true;
                } else {
                    multiple = true;
                    net_printf("BAD: MULTIPLE ipv4_6 ADDRESS\n");
                }
            } else {
                if (a[0] == 0xfe && a[1] == 0x80) {
                    if (!net.ip6LL_valid) {
                        net.ip6LL_if_name = strdup(ifa->ifa_name);
                        memcpy(net.ip6LL_pvt, a, sizeof(net.ip6LL_pvt));
                        ip_pvt = net.ip6LL_pvt_s;
                        salen = sizeof(struct sockaddr_in6);
                        memcpy(net.netmask6LL, m, sizeof(net.netmask6LL));
                        net_printf("IF IPv6 LINK-LOCAL /%d %s\n", inet_nm_bits(AF_INET6, &net.netmask6LL), net.ip6LL_if_name);
                        net.ip6LL_valid = true;
                    } else {
                        multiple = true;
                        net_printf("BAD: MULTIPLE ip6LL ADDRESS\n");
                    }
                } else
                if (net.ip6_valid < N_IPV6) {
                    i = net.ip6_valid;
                    if (i == 0) net.ip6_if_name = strdup(ifa->ifa_name);
                    memcpy(net.ip6_pvt[i], a, sizeof(net.ip6_pvt[0]));
                    ip_pvt = net.ip6_pvt_s[i];
                    salen = sizeof(struct sockaddr_in6);
                    memcpy(net.netmask6[i], m, sizeof(net.netmask6[0]));
                    net_printf("IF IPv6 #%d /%d %s ", i, inet_nm_bits(AF_INET6, net.netmask6[i]), net.ip6_if_name);
		            for (j=0; j < 16; j++) net_printf2("%02x:", net.ip6_pvt[i][j]);
		            net_printf2("\n");
    
                    #ifdef IPV6_TEST
                        net.netmask6[i][8] = 0xff;
                    #endif
                    
                    net.ip6_valid++;
                } else {
                    multiple = true;
                    net_printf("BAD: TOO MANY ipv6 ADDRESSES\n");
                }
            }
        }
        
        if (multiple) continue;
        
        int rc = getnameinfo(ifa->ifa_addr, salen, ip_pvt, NET_ADDRSTRLEN, NULL, 0, NI_NUMERICHOST);
        if (rc != 0) {
            lprintf("NET getnameinfo() failed: %s, fam=%d %s\n", ifa->ifa_name, family, gai_strerror(rc));
            continue;
        }
    }
	
	if (net.ip4_valid) {
		net.nm_bits4 = inet_nm_bits(AF_INET, &net.netmask4);
		lprintf("NET(%d): private IPv4 <%s> 0x%08x /%d 0x%08x %s\n", retry, net.ip4_pvt_s, net.ip4_pvt,
			net.nm_bits4, net.netmask4, net.ip4_if_name);
	}

	if (net.ip4_6_valid) {
		net.nm_bits4_6 = inet_nm_bits(AF_INET, &net.netmask4_6);
		lprintf("NET(%d): private IPv4_6 <%s> 0x%08x /%d 0x%08x %s\n", retry, net.ip4_6_pvt_s, net.ip4_6_pvt,
			net.nm_bits4_6, net.netmask4_6, net.ip4_6_if_name);
	}

	for (i = 0; i < net.ip6_valid; i++) {
		net.nm_bits6[i] = inet_nm_bits(AF_INET6, net.netmask6[i]);
		lprintf("NET(%d): private IPv6 #%d <%s> ", retry, i, net.ip6_pvt_s[i]);
		for (j=0; j < 16; j++) lprintf("%02x:", net.ip6_pvt[i][j]);
		lprintf(" /%d ", net.nm_bits6[i]);
		for (j=0; j < 16; j++) lprintf("%02x:", net.netmask6[i][j]);
		lprintf(" %s\n", net.ip6_if_name);
	}

	if (net.ip6LL_valid) {
		net.nm_bits6LL = inet_nm_bits(AF_INET6, &net.netmask6LL);
		lprintf("NET(%d): private IPv6 LINK-LOCAL <%s> /%d ", retry, net.ip6LL_pvt_s, net.nm_bits6LL);
		for (i=0; i < 16; i++) lprintf("%02x:", net.netmask6LL[i]);
		lprintf(" %s\n", net.ip6LL_if_name);
	}

	// assign net.ip_pvt & net.nm_bits in priority order, IPV4 first
	if (net.ip4_valid) {
		net.ip_pvt = net.ip4_pvt_s;
		net.nm_bits = net.nm_bits4;
	} else
	if (net.ip4_6_valid) {
		net.ip_pvt = net.ip4_6_pvt_s;
		net.nm_bits = net.nm_bits4_6;
	} else
	if (net.ip6_valid) {
		net.ip_pvt = net.ip6_pvt_s[0];
		net.nm_bits = net.nm_bits6[0];
	} else
	if (net.ip6LL_valid) {
		net.ip_pvt = net.ip6LL_pvt_s;
		net.nm_bits = net.nm_bits6LL;
	}

rtn:	
	freeifaddrs(ifaddr);
    net_printf("ip4_valid=%d ip4_6_valid=%d ip6_valid=%d ip6LL_valid=%d\n",
        net.ip4_valid, net.ip4_6_valid, net.ip6_valid, net.ip6LL_valid);
	return (net.ip4_valid || net.ip4_6_valid || net.ip6_valid || net.ip6LL_valid);
}


// Find all possible client IP addresses, IPv4 or IPv6, and compare against all our
// server IPv4 or IPv6 addresses on the local network interfaces looking for a local network match.

isLocal_t isLocal_if_ip(conn_t *conn, char *remote_ip_s, const char *log_prefix)
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
	if (log_prefix) printf("%s isLocal_if_ip: remote_ip_s=%s AF_INET=%d AF_INET6=%d IPPROTO_TCP=%d IPPROTO_UDP=%d\n",
		log_prefix, remote_ip_s, AF_INET, AF_INET6, IPPROTO_TCP, IPPROTO_UDP);
	if (log_prefix) printf("%s isLocal_if_ip: AI_V4MAPPED=0x%02x AI_ALL=0x%02x AI_ADDRCONFIG=0x%02x AI_NUMERICHOST=0x%02x AI_PASSIVE=0x%02x\n",
		log_prefix, AI_V4MAPPED, AI_ALL, AI_ADDRCONFIG, AI_NUMERICHOST, AI_PASSIVE);
	*/
	
	memset(&hint, 0, sizeof(hint));
	hint.ai_flags = AI_V4MAPPED | AI_ALL;
	hint.ai_family = AF_UNSPEC;
	
	rc = getaddrinfo(remote_ip_s, NULL, &hint, &res);
	if (rc != 0) {
		if (log_prefix) clprintf(conn, "%s isLocal_if_ip: getaddrinfo %s FAILED %s\n", log_prefix, remote_ip_s, gai_strerror(rc));
		return IS_NOT_LOCAL;
	}
	
	bool is_local = false;
	isLocal_t isLocal = IS_NOT_LOCAL;
	bool have_server_local = false;
	int check = 0, n;

	for (rp = res, n=0; rp != NULL && isLocal != IS_LOCAL; rp = rp->ai_next, n++) {

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

		//if (log_prefix) printf("%s isLocal_if_ip: AI%d flg=0x%x fam=%d socktype=%d proto=%d addrlen=%d\n",
		//	log_prefix, n, rp->ai_flags, family, rp->ai_socktype, rp->ai_protocol, rp->ai_addrlen);

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
		char ip_client_s[NET_ADDRSTRLEN];
		ip_client_s[0] = 0;
		int rc = getnameinfo(sa_p, salen, ip_client_s, NET_ADDRSTRLEN, NULL, 0, NI_NUMERICHOST);
		if (rc != 0) {
			if (log_prefix) clprintf(conn, "%s isLocal_if_ip: getnameinfo() failed: %s\n", log_prefix, gai_strerror(rc));
			continue;
		}
		
		if (log_prefix) cprintf(conn, "%s isLocal_if_ip: flg=0x%x fam=%d socktype=%d proto=%d addrlen=%d %s/%s\n",
			log_prefix, rp->ai_flags, family, rp->ai_socktype, rp->ai_protocol, rp->ai_addrlen, ip_client_s, conn->remote_ip);

		// do local network check depending on type of client address: IPv4, IPv4-mapped IPv6, IPv6 or IPv6LL
		u4_t ip_server, ip_client;
		u1_t *ip_server6, *ip_client6;
		const char *ips_type;
		char *ip_server_s;
		u4_t nm_bits, netmask;
		u1_t *netmask6;
        bool local = false;
		int ip6_n = 0;

        do {
        
            bool ipv4_test = false;
            if (family == AF_INET) {
                ip_client = INET4_NTOH(* (u4_t *) &sin->sin_addr);

                if (net.ip4_valid) {
                    ips_type = "IPv4";
                    ip_server = net.ip4_pvt;
                    ip_server_s = net.ip4_pvt_s;
                    netmask = net.netmask4;
                    have_server_local = true;
                } else
                if (net.ip4_6_valid) {
                    ips_type = "IPv4_6";
                    ip_server = net.ip4_6_pvt;
                    ip_server_s = net.ip4_6_pvt_s;
                    netmask = net.netmask4_6;
                    have_server_local = true;
                } else {
                    if (log_prefix) clprintf(conn, "%s isLocal_if_ip: IPv4 client, but no server IPv4/IPv4_6\n", log_prefix);
                    continue;
                }

                nm_bits = inet_nm_bits(AF_INET, &netmask);
                ipv4_test = true;
            } else

            if (family == AF_INET6) {
                u1_t *a = (u1_t *) &sin6->sin6_addr;

                // detect IPv4-mapped IPv6 (::ffff:a.b.c.d/96)
                if (is_inet4_map_6(a)) {
                    ip_client = INET4_NTOH(* (u4_t *) &a[12]);
                
                    if (net.ip4_6_valid) {
                        ips_type = "IPv4_6";
                        ip_server = net.ip4_6_pvt;
                        ip_server_s = net.ip4_6_pvt_s;
                        netmask = net.netmask4_6;
                        have_server_local = true;
                    } else
                    if (net.ip4_valid) {
                        ips_type = "IPv4";
                        ip_server = net.ip4_pvt;
                        ip_server_s = net.ip4_pvt_s;
                        netmask = net.netmask4;
                        have_server_local = true;
                    } else {
                        if (log_prefix) clprintf(conn, "%s isLocal_if_ip: IPv4_6 client, but no server IPv4 or IPv4_6\n", log_prefix);
                        continue;
                    }
                    
                    nm_bits = inet_nm_bits(AF_INET, &netmask);
                    ipv4_test = true;
                } else {
                    if (net.ip6_valid) {
                        ip_client6 = a;
                        ips_type = "IPv6";
                        ip_server6 = net.ip6_pvt[ip6_n];
                        ip_server_s = net.ip6_pvt_s[ip6_n];
                        netmask6 = net.netmask6[ip6_n];
                        nm_bits = inet_nm_bits(AF_INET6, netmask6);
                        have_server_local = true;
                        ip6_n++;
                        if (ip6_n >= net.ip6_valid) ip6_n = 0;
                    } else
                    if (net.ip6LL_valid) {
                        ip_client6 = a;
                        ips_type = "IPv6LL";
                        ip_server6 = net.ip6LL_pvt;
                        ip_server_s = net.ip6LL_pvt_s;
                        netmask6 = net.netmask6LL;
                        nm_bits = inet_nm_bits(AF_INET6, netmask6);
                        have_server_local = true;
                    } else {
                        if (log_prefix) clprintf(conn, "%s isLocal_if_ip: IPv6 client, but no server IPv6\n", log_prefix);
                        continue;
                    }
                }
            }
        
            // This makes the critical assumption that an ip_client coming from "outside" the
            // local subnet could never match with a local subnet address.
            // i.e. that local address space is truly un-routable across the internet or local subnet.
        
            #define IPV4_LOOPBACK 0x7f000001
            u1_t ipv6_loopback[16] = { 0,0,0,0,0,0,0,1 };
        
            if (ipv4_test) {
                local = (
                    ((ip_client & netmask) == (ip_server & netmask))
                    /* || (ip_client == IPV4_LOOPBACK) */
                );
                
                // NB: we print conn->remote_ip because remote_ip_s will be 127.0.0.1 for proxied Kiwis.
                // But we NEVER test local network matching against it since it can be spoofed (a proxied Kiwi can never be local anyway).
                if (log_prefix) clprintf(conn, "%s isLocal_if_ip: %s IPv4/4_6 remote_ip %s/%s ip_client %s/0x%08x ip_server[%s] %s/0x%08x nm /%d 0x%08x\n",
                    log_prefix, local? "TRUE":"FALSE", remote_ip_s, conn->remote_ip, ip_client_s, ip_client, ips_type, ip_server_s, ip_server, nm_bits, netmask);
            } else {
                local = true;
                for (i=0; i < 16; i++) {
                    bool match = ((ip_client6[i] & netmask6[i]) == (ip_server6[i] & netmask6[i]));
                    if (!match)
                        local = false;
                    if (ip6_n) {
                        net_printf2("NET DEBUG %s %d %c c=%02x s=%02x nm=%02x\n", ips_type, i, match? 'T':'F', ip_client6[i], ip_server6[i], netmask6[i]);
                    } else {
                        net_printf2("NET DEBUG %s %d %c c=%02x s=%02x nm=%02x\n", ips_type, i, match? 'T':'F', ip_client6[i], ip_server6[i], netmask6[i]);
                    }
                }
                /*
                if (!local) for (i=0; i < 16; i++) {
                    local = (ip_client6[i] == ipv6_loopback[i]);
                    if (local == false)
                        break;
                }
                */
                if (log_prefix) clprintf(conn, "%s isLocal_if_ip: %s IPv6 remote_ip %s/%s ip_client %s ip_server[%s] %s nm /%d\n",
                    log_prefix, local? "TRUE":"FALSE", remote_ip_s, conn->remote_ip, ip_client_s, ips_type, ip_server_s, nm_bits);
            }
		
		} while (ip6_n && local == false);      // more ipv6 addresses to check
		
		if (local) isLocal = IS_LOCAL;
	}
	
	if (res != NULL) freeaddrinfo(res);

	if (res == NULL || check == 0) {
		if (log_prefix) clprintf(conn, "%s isLocal_if_ip: getaddrinfo %s NO CLIENT RESULTS?\n", log_prefix, remote_ip_s);
		return IS_NOT_LOCAL;
	}
	
    if (!have_server_local) isLocal = NO_LOCAL_IF;
	return isLocal;
}

u4_t inet4_d2h(char *inet4_str, bool *error, u1_t *ap, u1_t *bp, u1_t *cp, u1_t *dp)
{
    if (error != NULL) *error = false;
	int n;
	u4_t a, b, c, d;

	if (inet4_str == NULL)
	    goto err;
	
	n = sscanf(inet4_str, "%d.%d.%d.%d", &a, &b, &c, &d);
	if (n != 4) {
		n = sscanf(inet4_str, "::ffff:%d.%d.%d.%d", &a, &b, &c, &d); //IPv4-mapped address
		if (n != 4)
			goto err;  // IPv6 or invalid
	}
	if (a > 255 || b > 255 || c > 255 || d > 255)
	    goto err;

    if (ap != NULL) *ap = a & 255;
    if (bp != NULL) *bp = b & 255;
    if (cp != NULL) *cp = c & 255;
    if (dp != NULL) *dp = d & 255;
	return INET4_DTOH(a, b, c, d);

err:
    if (error != NULL) *error = true;
    return 0;
}

void inet4_h2d(u4_t inet4, u1_t *ap, u1_t *bp, u1_t *cp, u1_t *dp)
{
    if (ap != NULL) *ap = bf(inet4, 31, 24);
    if (bp != NULL) *bp = bf(inet4, 23, 16);
    if (cp != NULL) *cp = bf(inet4, 15,  8);
    if (dp != NULL) *dp = bf(inet4,  7,  0);
}

char *inet4_h2s(u4_t inet4, int which)
{
    u1_t a,b,c,d;
    inet4_h2d(inet4, &a,&b,&c,&d);
    return stnprintf(which, "%d.%d.%d.%d", a,b,c,d);
}

// ::ffff:a.b.c.d/96
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

bool isLocal_ip(char *ip_str, bool *is_loopback, u4_t *ipv4)
{
    bool error;
    u4_t ip = inet4_d2h(ip_str, &error);
    if (ipv4) *ipv4 = 0;
    
    if (!error) {
        // ipv4
        if (is_loopback)
            *is_loopback = (ip == INET4_DTOH(127,0,0,1));
        if (
            (ip >= INET4_DTOH(10,0,0,0) && ip <= INET4_DTOH(10,255,255,255)) ||
            (ip >= INET4_DTOH(172,16,0,0) && ip <= INET4_DTOH(172,31,255,255)) ||
            (ip >= INET4_DTOH(192,168,0,0) && ip <= INET4_DTOH(192,168,255,255)) ) {
            if (ipv4) *ipv4 = ip;
            return true;
        }
    } else {
        // ipv6
        if (is_loopback)
            *is_loopback = (strcmp(ip_str, "::1") == 0 || strcmp(ip_str, ":0:0:0:0:0:0:0:1") == 0);
        if (strncasecmp(ip_str, "fd", 2) == 0)
            return true;
    }
    
    return false;
}

bool ip_match(const char *ip, ip_lookup_t *ips)
{
    char *needle_ip;

    for (int i = 0; (needle_ip = ips->ip_list[i]) != NULL; i++) {
        bool match = (*needle_ip != '\0' && strstr(ip, needle_ip) != NULL);
        //printf("ipmatch: %s %d=\"%s\" %s\n", ip, i, needle_ip, match? "T":"F");
        if (match) {
            //printf("ipmatch: TRUE\n");
            return true;
        }
    }

    //printf("ipmatch: FALSE\n");
    return false;
}

int DNS_lookup(const char *domain_name, ip_lookup_t *r_ips, int n_ips, const char *ip_backup)
{
    int i, n, status;
    char *cmd_p;
    char **ip_list = r_ips->ip_list;

    assert(n_ips <= N_IPS);
    asprintf(&cmd_p, "dig +short +noedns +time=3 +tries=3 %s A %s AAAA", domain_name, domain_name);
    //printf("LOOKUP: \"%s\" <%s>\n", domain_name, cmd_p);
	kstr_t *reply = non_blocking_cmd(cmd_p, &status);
	
	if (reply != NULL && status >= 0 && WEXITSTATUS(status) == 0) {
		char *r_buf;
		str_split_t ips[N_IPS];
		n = kiwi_split(kstr_sp(reply), &r_buf, "\n", ips, n_ips-1);

        for (i = 0; i < n; i++) {
            ip_list[i] = strndup(ips[i].str, NET_ADDRSTRLEN);
            int slen = strlen(ip_list[i]);
            if (ip_list[i][slen-1] == '\n') ip_list[i][slen-1] = '\0';    // remove trailing \n
	        printf("LOOKUP: \"%s\" %s\n", domain_name, ip_list[i]);
	        r_ips->ip[i] = inet4_d2h(ip_list[i], NULL);
        }
        
        kiwi_ifree(r_buf, "DNS_lookup");
        r_ips->valid = true;
	} else {
	    if (ip_backup != NULL) {
            ip_list[0] = (char *) ip_backup;
	        r_ips->ip[0] = inet4_d2h(ip_list[0], NULL);
            n = 1;
            r_ips->valid = r_ips->backup = true;
            lprintf("WARNING: lookup for \"%s\" failed, using backup IPv4 address %s\n", domain_name, ip_backup);
        } else {
	        printf("LOOKUP: \"%s\" NOT FOUND\n", domain_name);
            n = 0;
        }
	}
	kiwi_asfree(cmd_p);
	kstr_free(reply);
    ip_list[n] = NULL;
    r_ips->n_ips = n;
	return n;
}

char *DNS_lookup_result(const char *caller, const char *host, ip_lookup_t *ips)
{
    NET_WAIT_COND(caller, "DNS_lookup_result", ips->valid);
    return ips->backup? ips->ip_list[0] : (char *) host;
}

char *ip_remote(struct mg_connection *mc)
{
    return kiwi_skip_over(mc->remote_ip, "::ffff:");
}

// CAUTION: returned remote_ip could be fake if X-Real-IP or X-Forwarded-For are spoofed
bool check_if_forwarded(const char *id, struct mg_connection *mc, char *remote_ip)
{
    kiwi_strncpy(remote_ip, ip_remote(mc), NET_ADDRSTRLEN);     // unforwarded
    
    const char *x_real_ip = mg_get_header(mc, "X-Real-IP");
    const char *x_forwarded_for = mg_get_header(mc, "X-Forwarded-For");
    //printf("check_if_forwarded: x_real_ip=<%s> x_forwarded_for=<%s>\n", x_real_ip, x_forwarded_for);
    if (x_real_ip == NULL && x_forwarded_for == NULL) return false;

    int n = 0;
    char *ip_r = NULL;
    
    if (x_real_ip != NULL) {
        //if (id != NULL) printf("%s: %s X-Real-IP %s\n", id, remote_ip, x_real_ip);
        n = sscanf(x_real_ip, "%" NET_ADDRSTRLEN_S "ms", &ip_r);
    }
    
    if (x_forwarded_for != NULL) {
        //if (id != NULL) printf("%s: %s X-Forwarded-For %s\n", id, remote_ip, x_forwarded_for);
        if (x_real_ip == NULL || n != 1) {
            // take only client ip in case "X-Forwarded-For: client, proxy1, proxy2 ..."
            n = sscanf(x_forwarded_for, "%" NET_ADDRSTRLEN_S "m[^, ]", &ip_r);
        }
    }

    bool forwarded = false;
    if (n == 1) {
        if (strcmp(ip_r, "127.0.0.1") != 0) {
            kiwi_strncpy(remote_ip, ip_r, NET_ADDRSTRLEN);
            forwarded = true;
        } else {
            //printf("check_if_forwarded: BOTH loopback? x_real_ip=<%s> x_forwarded_for=<%s>\n", x_real_ip, x_forwarded_for);
        }
    }
    
    mg_free_header(x_real_ip);
    mg_free_header(x_forwarded_for);
    kiwi_asfree(ip_r);
    return forwarded;
}


// Emulates the client-side (js) of a Kiwi sound/waterfall/extension channel API connection.
// For use by server-side internal connections, e.g. WSPR autorun, FT8 autorun, SNR measurement.

bool internal_conn_setup(u4_t ws, internal_conn_t *iconn, int instance, int port_base, u4_t ws_flags,
    const char *mode, int locut, int hicut, float freq_kHz,
    const char *ident_user, const char *geoloc, const char *client,
    int zoom, float cf_kHz, int min_dB, int max_dB, int wf_speed, int wf_comp)
{
    struct mg_connection *mc_fail, *mcs = NULL, *mcw = NULL, *mce = NULL;
    conn_t *csnd = NULL, *cwf, *cext;
    int local_port = port_base + instance * 3;
    u64_t tstamp = rx_conn_tstamp();
    bool ident_geo_sent = false;
    memset(iconn, 0, sizeof(internal_conn_t));
    
    if (ws & ICONN_WS_SND) {
        mcs = &iconn->snd_mc;
        mc_fail = mcs;
        mcs->connection_param = NULL;
        asprintf((char **) &mcs->uri, "ws/kiwi/%lld/SND", tstamp);
        kiwi_strncpy(mcs->remote_ip, "127.0.0.1", NET_ADDRSTRLEN);
        mcs->remote_port = local_port;
        mcs->local_port = net.port;
        csnd = rx_server_websocket(WS_INTERNAL_CONN, mcs, ws_flags);
        if (csnd == NULL) goto error;
        iconn->csnd = csnd;
        csnd->internal_want_snd = true;
        input_msg_internal(csnd, (char *) "SET auth t=kiwi p=");
        input_msg_internal(csnd, (char *) "SET AR OK in=12000 out=44100");
        input_msg_internal(csnd, (char *) "SET agc=1 hang=0 thresh=-100 slope=6 decay=1000 manGain=50");
        input_msg_internal(csnd, (char *) "SET mod=%s low_cut=%d high_cut=%d freq=%.3f",
            mode, locut, hicut, freq_kHz);
        input_msg_internal(csnd, (char *) "SET ident_user=%s", ident_user);
        if (geoloc) input_msg_internal(csnd, (char *) "SET geoloc=%s", geoloc);
        ident_geo_sent = true;
    }

    if (ws & ICONN_WS_WF) {
        mcw = &iconn->wf_mc;
        mc_fail = mcw;
        mcw->connection_param = NULL;
        asprintf((char **) &mcw->uri, "ws/kiwi/%lld/W/F", tstamp);
        kiwi_strncpy(mcw->remote_ip, "127.0.0.1", NET_ADDRSTRLEN);
        mcw->remote_port = local_port + 1;
        mcw->local_port = net.port;
        cwf = rx_server_websocket(WS_INTERNAL_CONN, mcw, ws_flags);
        if (cwf == NULL) goto error;
        iconn->cwf = cwf;
        cwf->internal_want_wf = true;
        input_msg_internal(cwf, (char *) "SET auth t=kiwi p=");
        input_msg_internal(cwf, (char *) "SET zoom=%d cf=%.3f", zoom, cf_kHz);
        input_msg_internal(cwf, (char *) "SET maxdb=%d mindb=%d", max_dB, min_dB);
        input_msg_internal(cwf, (char *) "SET wf_speed=%d", wf_speed);
        input_msg_internal(cwf, (char *) "SET wf_comp=%d", wf_comp);
        if (!ident_geo_sent) {
            input_msg_internal(cwf, (char *) "SET ident_user=%s", ident_user);
            if (geoloc) input_msg_internal(cwf, (char *) "SET geoloc=%s", geoloc);
            ident_geo_sent = true;
        }
    }

    if (ws & ICONN_WS_EXT) {
        mce = &iconn->ext_mc;
        mc_fail = mce;
        if (csnd == NULL) goto error2;  // i.e. ICONN_WS_SND must be used together with ICONN_WS_EXT
        mce->connection_param = NULL;
        asprintf((char **) &mce->uri, "ws/kiwi/%lld/EXT", tstamp);
        kiwi_strncpy(mce->remote_ip, "127.0.0.1", NET_ADDRSTRLEN);
        mce->remote_port = local_port + 2;
        mce->local_port = net.port;
        cext = rx_server_websocket(WS_INTERNAL_CONN, mce, ws_flags);
        if (cext == NULL) goto error;
        iconn->cext = cext;
        input_msg_internal(cext, (char *) "SET auth t=kiwi p=");
        input_msg_internal(cext, (char *) "SET ext_switch_to_client=%s first_time=1 rx_chan=%d",
            client, csnd->rx_channel);
    }

    return true;

error:
    //printf("internal_conn_setup: %s couldn't get websocket instance=%d uri=%s port=%d\n",
    //    ident_user, instance, mc_fail->uri, mc_fail->remote_port);
    internal_conn_shutdown(iconn);
    return false;

error2:
    printf("internal_conn_setup: %s need (ICONN_WS_EXT | ICONN_WS_SND) instance=%d uri=%s port=%d\n",
        ident_user, instance, mc_fail->uri, mc_fail->remote_port);
    internal_conn_shutdown(iconn);
    return false;
}

void internal_conn_shutdown(internal_conn_t *iconn)
{
    if (iconn->csnd) rx_server_websocket(WS_MODE_CLOSE, &iconn->snd_mc);
    if (iconn->cwf)  rx_server_websocket(WS_MODE_CLOSE, &iconn->wf_mc);
    if (iconn->cext) rx_server_websocket(WS_MODE_CLOSE, &iconn->ext_mc);
}

#include <string.h>
#include <time.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>

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

// Copyright (c) 2014-2016 John Seamons, ZL/KF6VO

ddns_t ddns;

// we've seen the ident.me site respond very slowly at times, so do this in a separate task
void dyn_DNS(void *param)
{
	int i, n, status;
	bool noEthernet = false, noInternet = false;

	if (!do_dyn_dns)
		return;
		
	ddns.serno = serial_number;
	ddns.port = user_iface[0].port;
	
	char buf[256];
	
	for (i=0; i<1; i++) {	// hack so we can use 'break' statements below

		// send private/public ip addrs to registry
		//n = non_blocking_cmd("hostname --all-ip-addresses", buf, sizeof(buf), NULL);
		n = non_blocking_cmd("ip addr show dev eth0 | grep 'inet ' | cut -d ' ' -f 6", buf, sizeof(buf), &status);
		noEthernet = (status < 0 || WEXITSTATUS(status) != 0);
		if (!noEthernet && n > 0) {
			n = sscanf(buf, "%[^/]/%d", ddns.ip_pvt, &ddns.netmask);
			assert (n == 2);
			ddns.pvt_valid = true;
		} else
			break;
		
		//n = non_blocking_cmd("ifconfig eth0", buf, sizeof(buf), NULL);
		n = non_blocking_cmd("cat /sys/class/net/eth0/address", buf, sizeof(buf), &status);
		noEthernet = (status < 0 || WEXITSTATUS(status) != 0);
		if (!noEthernet && n > 0) {
			//n = sscanf(buf, "eth0 Link encap:Ethernet HWaddr %17s", ddns.mac);
			n = sscanf(buf, "%17s", ddns.mac);
			assert (n == 1);
		} else
			break;
		
		if (no_net) {
			noInternet = true;
			break;
		}
		
		n = non_blocking_cmd("curl -s ident.me", buf, sizeof(buf), &status);
		noInternet = (status < 0 || WEXITSTATUS(status) != 0);
		if (!noInternet && n > 0) {
			i = sscanf(buf, "%16s", ddns.ip_pub);
			assert (i == 1);
			ddns.pub_valid = true;
		} else
			break;
	}
	
	if (ddns.serno == 0) lprintf("DDNS: no serial number?\n");
	if (noEthernet) lprintf("DDNS: no Ethernet interface active?\n");
	if (noInternet) lprintf("DDNS: no Internet access?\n");

	if (ddns.pvt_valid)
		lprintf("DDNS: private ip %s/%d\n", ddns.ip_pvt, ddns.netmask);

	if (ddns.pub_valid)
		lprintf("DDNS: public ip %s\n", ddns.ip_pub);

	// no Internet access or no serial number available, so no point in registering
	if (noEthernet || noInternet || ddns.serno == 0)
		return;
	
	ddns.valid = true;

	// FIXME might as well remove from code when we get to v1.0
	if (register_on_kiwisdr_dot_com) {
		char *bp;
		asprintf(&bp, "curl -s -o /dev/null http://%s/php/register.php?reg=%d.%s.%d.%s.%d.%s",
			DYN_DNS_SERVER, ddns.serno, ddns.ip_pub, ddns.port, ddns.ip_pvt, ddns.netmask, ddns.mac/*, VERSION_MAJ, VERSION_MIN*/);
		n = system(bp);
		lprintf("registering: <%s> returned %d\n", bp, n);
		free(bp);
	}
}

bool isLocal_IP(struct mg_connection *mc, char *ip_server_s, u4_t netmask, bool print)
{
	int rc;
	bool is_local;
	u4_t ip_client, ip_server = kiwi_inet4_d2h(ip_server_s);
	u4_t nm = ~((1 << (32 - netmask)) - 1);
	
	if (ip_server == 0xffffffff) {		// server has an IPv6 which we can never match
		if (print) lprintf("isLocal_IP: SERVER HAS ONLY IPv6 ADDRESS %s\n", ip_server_s);
		return false;
	}
	
	// if mc->remote_ip is IPv6 see if there is an IPv4 overlay address
	struct addrinfo hints, *res, *rp;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;		// IPv4 only
	rc = getaddrinfo(mc->remote_ip, NULL, &hints, &res);
	if (rc != 0) {
		if (print) lprintf("isLocal_IP getaddrinfo: %s IPv4 FAILED %s\n", mc->remote_ip, gai_strerror(rc));
		return false;
	}
	
	for (rp = res; rp != NULL; rp = rp->ai_next) {
		if (rp->ai_family != AF_INET)
			continue;

		// This makes the critical assumption that an ip_client coming from "outside" the
		// local subnet could never match with a local subnet address.
		// i.e. that local address space is truly un-routable across the internet or local subnet.
		ip_client = INET4_NTOH(* (u4_t *) &((struct sockaddr_in *) (rp->ai_addr))->sin_addr);
		is_local = ((ip_client & nm) == (ip_server & nm));
		if (print) lprintf("isLocal_IP: ip_client %s/0x%08x ip_server %s/0x%08x nm /%d 0x%08x is_local %s\n",
			mc->remote_ip, ip_client, ip_server_s, ip_server, netmask, nm, is_local? "TRUE":"FALSE");
		if (is_local)
			break;
	}
	
	freeaddrinfo(res);

	if (rp == NULL) {
		if (print) lprintf("isLocal_IP getaddrinfo: %s NO MATCHING IPv4 IN RESULT\n", mc->remote_ip);
		return false;
	}
	
	return is_local;
}


static void reg_SDR_hu(void *param)
{
	int n, retrytime_mins=0;
	char *cmd_p;
	
	// reply is a bunch of HTML, buffer has to be big enough not to miss/split status
	#define NBUF 32768
	char *reply = (char *) kiwi_malloc("register_SDR_hu", NBUF);
	
	const char *server_url = cfg_string("server_url", NULL, CFG_OPTIONAL);
	const char *api_key = cfg_string("api_key", NULL, CFG_OPTIONAL);
	asprintf(&cmd_p, "wget --timeout=3 -qO- http://sdr.hu/update --post-data \"url=http://%s:%d&apikey=%s\" 2>&1",
		server_url, user_iface[0].port, api_key);
	if (server_url) cfg_string_free(server_url);
	if (api_key) cfg_string_free(api_key);

	while (1) {
		n = non_blocking_cmd(cmd_p, reply, NBUF/2, NULL);
		//printf("sdr.hu: REPLY <%s>\n", reply);
		char *sp, *sp2;
		if (n > 0 && (sp = strstr(reply, "UPDATE:")) != 0) {
			sp += 7;
			if (strncmp(sp, "SUCCESS", 7) == 0) {
				if (retrytime_mins != 20) lprintf("sdr.hu registration: WORKED\n");
				retrytime_mins = 20;
			} else {
				if ((sp2 = strchr(sp, '\n')) != NULL)
					*sp2 = '\0';
				lprintf("sdr.hu registration: \"%s\"\n", sp);
				retrytime_mins = 2;
			}
		} else {
			lprintf("sdr.hu registration: FAILED n=%d sp=%p\n", n, sp);
			retrytime_mins = 2;
		}
		TaskSleep(SEC_TO_USEC(MINUTES_TO_SEC(retrytime_mins)));
	}
	kiwi_free("register_SDR_hu", reply);
	free(cmd_p);
	#undef NBUF
}

void services_start(bool restart)
{
	CreateTask(dyn_DNS, 0, WEBSERVER_PRIORITY);

	if (!no_net && !restart && !down && !alt_port && cfg_bool("sdr_hu_register", NULL, CFG_PRINT) == true) {
		CreateTask(reg_SDR_hu, 0, WEBSERVER_PRIORITY);
	}
}

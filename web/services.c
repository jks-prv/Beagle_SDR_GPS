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
#include <netdb.h>
#include <arpa/inet.h>
#include <ifaddrs.h>

// we've seen the ident.me site respond very slowly at times, so do this in a separate task
// FIXME: this doesn't work if someone is using WiFi or USB networking

static void dyn_DNS(void *param)
{
	int i, n, status;
	bool noEthernet = false, noInternet = false;

	if (!do_dyn_dns)
		return;
		
	ddns.serno = serial_number;
	ddns.port = user_iface[0].port;
	
	char buf[256];
	
	for (i=0; i<1; i++) {	// hack so we can use 'break' statements below

		// get Ethernet interface MAC address
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
		
		// get our public IP with the assistance of ident.me
		// FIXME: should try other sites if ident.me is down or goes away
		n = non_blocking_cmd("curl -s ident.me", buf, sizeof(buf), &status);
		noInternet = (status < 0 || WEXITSTATUS(status) != 0);
		if (!noInternet && n > 0) {
			// FIXME: start using returned routine allocated buffers instead of fixed passed buffers
			//char *p;
			//i = sscanf(buf, "%ms", &p);
			i = sscanf(buf, "%s", ddns.ip_pub);
			check(i == 1);
			//kiwi_copy_terminate_free(p, ddns.ip_pub, sizeof(ddns.ip_pub));
			ddns.pub_valid = true;
		} else
			break;
	}
	
	if (ddns.serno == 0) lprintf("DDNS: no serial number?\n");
	if (noEthernet) lprintf("DDNS: no Ethernet interface active?\n");
	if (noInternet) lprintf("DDNS: no Internet access?\n");

	if (!find_local_IPs()) {
		lprintf("DDNS: no Ethernet interface IP addresses?\n");
		noEthernet = true;
	}

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
			DYN_DNS_SERVER, ddns.serno, ddns.ip_pub, ddns.port, ddns.ip_pvt, ddns.nm_bits, ddns.mac/*, VERSION_MAJ, VERSION_MIN*/);
		n = system(bp);
		lprintf("registering: <%s> returned %d\n", bp, n);
		free(bp);
	}
}

// routine that processes the output of the registration wget command
static int _reg_SDR_hu(void *param)
{
	nbcmd_args_t *args = (nbcmd_args_t *) param;
	int n = args->bc;
	char *sp, *sp2;
	int retrytime_mins = args->func_param;

	if (n > 0 && (sp = strstr(args->bp, "UPDATE:")) != 0) {
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
	
	return retrytime_mins;
}

static void reg_SDR_hu(void *param)
{
	int n;
	char *cmd_p;
	int retrytime_mins = 1;
	
	// reply is a bunch of HTML, buffer has to be big enough not to miss/split status
	#define NBUF 16384
	
	const char *server_url = cfg_string("server_url", NULL, CFG_OPTIONAL);
	const char *api_key = cfg_string("api_key", NULL, CFG_OPTIONAL);
	asprintf(&cmd_p, "wget --timeout=15 -qO- http://sdr.hu/update --post-data \"url=http://%s:%d&apikey=%s\" 2>&1",
		server_url, user_iface[0].port, api_key);
	if (server_url) cfg_string_free(server_url);
	if (api_key) cfg_string_free(api_key);

	while (1) {
		retrytime_mins = non_blocking_cmd_child(cmd_p, _reg_SDR_hu, retrytime_mins, NBUF);
		TaskSleepUsec(SEC_TO_USEC(MINUTES_TO_SEC(retrytime_mins)));
	}
	
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

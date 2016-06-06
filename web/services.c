#include <string.h>
#include <time.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>

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
void dynamic_DNS(void *param)
{
	int i, n, status;

	if (!do_dyn_dns)
		return;
		
	ddns.serno = serial_number;
	ddns.port = user_iface[0].port;
	
	char buf[256];
	
	// send private/public ip addrs to registry
	//n = non_blocking_cmd("hostname --all-ip-addresses", buf, sizeof(buf), NULL);
	n = non_blocking_cmd("ip addr show dev eth0 | grep 'inet ' | cut -d ' ' -f 6", buf, sizeof(buf), NULL);
	assert (n > 0);
	n = sscanf(buf, "%[^/]/%d", ddns.ip_pvt, &ddns.netmask);
	assert (n == 2);
	
	//n = non_blocking_cmd("ifconfig eth0", buf, sizeof(buf), NULL);
	n = non_blocking_cmd("cat /sys/class/net/eth0/address", buf, sizeof(buf), NULL);
	assert (n > 0);
	//n = sscanf(buf, "eth0 Link encap:Ethernet HWaddr %17s", ddns.mac);
	n = sscanf(buf, "%17s", ddns.mac);
	assert (n == 1);
	
	n = non_blocking_cmd("curl -s ident.me", buf, sizeof(buf), &status);
	if (n > 0) {
		i = sscanf(buf, "%16s", ddns.ip_pub);
		assert (i == 1);
	}
	
	ddns.valid = true;

	// no Internet access or no serial number available, so no point in registering
	bool noInternet = (status < 0 || WEXITSTATUS(status) != 0);
	if (ddns.serno == 0) lprintf("DDNS: no serial number?\n");
	if (noInternet) lprintf("DDNS: no Internet access?\n");
	if (noInternet || ddns.serno == 0)
		return;
	
	lprintf("DDNS: private ip %s/%d, public ip %s\n", ddns.ip_pvt, ddns.netmask, ddns.ip_pub);

	if (register_on_kiwisdr_dot_com) {
		char *bp;
		asprintf(&bp, "curl -s -o /dev/null http://%s/php/register.php?reg=%d.%s.%d.%s.%d.%s",
			DYN_DNS_SERVER, ddns.serno, ddns.ip_pub, ddns.port, ddns.ip_pvt, ddns.netmask, ddns.mac/*, VERSION_MAJ, VERSION_MIN*/);
		n = system(bp);
		lprintf("registering: <%s> returned %d\n", bp, n);
		free(bp);
	}
}


static void register_SDR_hu(void *param)
{
	int n, retrytime_mins=0;
	char *cmd_p;
	
	// reply is a bunch of HTML, buffer has to be big enough not to miss/split status
	#define NBUF 32768
	char *reply = (char *) kiwi_malloc("register_SDR_hu", NBUF);
	
	asprintf(&cmd_p, "wget --timeout=15 -qO- http://sdr.hu/update --post-data \"url=http://%s:%d&apikey=%s\" 2>&1",
		cfg_string("server_url", NULL, CFG_OPTIONAL), user_iface[0].port, cfg_string("api_key", NULL, CFG_OPTIONAL));

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
		TaskSleep(MINUTES_TO_SECS(retrytime_mins) * 1000000);
	}
	kiwi_free("register_SDR_hu", reply);
	free(cmd_p);
	#undef NBUF
}

void services_start(bool restart)
{
	CreateTask(dynamic_DNS, 0, WEBSERVER_PRIORITY);

	if (!restart && !down && !alt_port && cfg_bool("sdr_hu_register", NULL, CFG_PRINT) == true)
		CreateTask(register_SDR_hu, 0, WEBSERVER_PRIORITY);
}

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
#include "str.h"
#include "jsmn.h"
#include "gps.h"

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

int utc_offset = -1, dst_offset = -1;
char *tzone_id = (char *) "null", *tzone_name = (char *) "null";

static void get_TZ(void *param)
{
	int n, stat;
	char buf[1024], *cmd_p, *lat_lon;
	cfg_t cfg_tz;
	
	TaskSleepUsec(SEC_TO_USEC(10));		// long enough for ddns.lat_lon_valid to be set

	int report = 3;
	while (1) {
		double lat, lon;
		char *s;
		bool err, haveLatLon = false;
	
		lat_lon = (char *) cfg_string("rx_gps", NULL, CFG_OPTIONAL);
		if (lat_lon != NULL) {
			n = sscanf(lat_lon, "%*[^0-9+-]%lf%*[^0-9+-]%lf)", &lat, &lon);
			// consider default lat/lon to be the same as unset
			if (n == 2 && strcmp(lat_lon, "(-37.631120, 176.172210)") != 0) {
				lprintf("TIMEZONE: lat/lon from sdr.hu config: (%lf, %lf)\n", lat, lon);
				haveLatLon = true;
			}
			cfg_string_free(lat_lon);
		}
	
		if (!haveLatLon && gps.StatLat) {
			lat = gps.sgnLat; lon = gps.sgnLon;
			lprintf("TIMEZONE: lat/lon from GPS: (%lf, %lf)\n", lat, lon);
			haveLatLon = true;
		}
		
		if (!haveLatLon && ddns.lat_lon_valid) {
			lat = ddns.lat; lon = ddns.lon;
			lprintf("TIMEZONE: lat/lon from DDNS: (%lf, %lf)\n", lat, lon);
			haveLatLon = true;
		}
		
		if (!haveLatLon) {
			if (report) lprintf("TIMEZONE: no lat/lon available from sdr.hu config, DDNS or GPS\n");
			goto retry;
		}
	
		time_t utc_sec; time(&utc_sec);
		asprintf(&cmd_p, "curl -s \"https://maps.googleapis.com/maps/api/timezone/json?location=%f,%f&timestamp=%lu&sensor=false\" 2>&1",
			lat, lon, utc_sec);
		n = non_blocking_cmd(cmd_p, buf, sizeof(buf), &stat);
		free(cmd_p);
		if (stat < 0 || WEXITSTATUS(stat) != 0 || n <= 0) {
			lprintf("TIMEZONE: googleapis.com curl error\n");
			goto retry;
		}
	
		json_init(&cfg_tz, buf);
		err = false;
		s = (char *) json_string(&cfg_tz, "status", &err, CFG_OPTIONAL);
		if (err) goto retry;
		if (strcmp(s, "OK") != 0) {
			lprintf("TIMEZONE: googleapis.com returned status \"%s\"\n", s);
			err = true;
		}
		cfg_string_free(s);
		if (err) goto retry;
		
		utc_offset = json_int(&cfg_tz, "rawOffset", &err, CFG_OPTIONAL);
		if (err) goto retry;
		dst_offset = json_int(&cfg_tz, "dstOffset", &err, CFG_OPTIONAL);
		if (err) goto retry;
		tzone_id = (char *) json_string(&cfg_tz, "timeZoneId", NULL, CFG_OPTIONAL);
		tzone_name = (char *) json_string(&cfg_tz, "timeZoneName", NULL, CFG_OPTIONAL);
		
		lprintf("TIMEZONE: for (%f, %f): utc_offset=%d/%.1f dst_offset=%d/%.1f\n",
			lat, lon, utc_offset, (float) utc_offset / 3600, dst_offset, (float) dst_offset / 3600);
		lprintf("TIMEZONE: \"%s\", \"%s\"\n", tzone_id, tzone_name);
		s = tzone_id; tzone_id = str_encode(s); cfg_string_free(s);
		s = tzone_name; tzone_name = str_encode(s); cfg_string_free(s);
		
		return;
retry:
		if (report) lprintf("TIMEZONE: will retry..\n");
		if (report) report--;
		TaskSleepUsec(SEC_TO_USEC(MINUTES_TO_SEC(1)));
	}
}

// we've seen the ident.me site respond very slowly at times, so do this in a separate task
// FIXME: this doesn't work if someone is using WiFi or USB networking because only "eth0" is checked

static bool ipinfo_json(char *buf)
{
	int n;
	char *s;
	cfg_t cfgx;
	json_init(&cfgx, buf);
	//_cfg_walk(&cfgx, NULL, cfg_print_tok, NULL);
	
	s = (char *) json_string(&cfgx, "ip", NULL, CFG_OPTIONAL);
	if (s == NULL) return false;
	strcpy(ddns.ip_pub, s);
	iparams_add("IP_PUB", s);
	ddns.pub_valid = true;
	
	s = (char *) json_string(&cfgx, "loc", NULL, CFG_OPTIONAL);
	if (s != NULL) {
		n = sscanf(s, "%lf,%lf)", &ddns.lat, &ddns.lon);
		if (n == 2) ddns.lat_lon_valid = true;
	}
	
	bool err;
	double lat = json_float(&cfgx, "latitude", &err, CFG_OPTIONAL);
	if (!err) {
		double lon = json_float(&cfgx, "longitude", &err, CFG_OPTIONAL);
		if (!err) {
			ddns.lat = lat; ddns.lon = lon; ddns.lat_lon_valid = true;
		}
	}
	
	if (ddns.lat_lon_valid)
		lprintf("DDNS: lat/lon = (%lf, %lf)\n", ddns.lat, ddns.lon);
	return true;
}

static void dyn_DNS(void *param)
{
	int i, n, status;
	bool noEthernet = false, noInternet = false;

	if (!do_dyn_dns)
		return;
		
	ddns.serno = serial_number;
	
	char buf[2048];
	
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
		
		// get our public IP and possibly lat/lon
		//n = non_blocking_cmd("curl -s ident.me", buf, sizeof(buf), &status);
		//n = non_blocking_cmd("curl -s icanhazip.com", buf, sizeof(buf), &status);
		n = non_blocking_cmd("curl -s --connect-timeout 10 ipinfo.io/json/", buf, sizeof(buf), &status);
		if (status < 0 || WEXITSTATUS(status) != 0 || !ipinfo_json(buf)) {
			n = non_blocking_cmd("curl -s --connect-timeout 10 freegeoip.net/json/", buf, sizeof(buf), &status);
			if (status < 0 || WEXITSTATUS(status) != 0 || !ipinfo_json(buf))
				break;
		}
	}
	
	if (ddns.serno == 0) lprintf("DDNS: no serial number?\n");
	if (noEthernet) lprintf("DDNS: no Ethernet interface active?\n");
	if (noInternet) lprintf("DDNS: no Internet access?\n");

	if (!find_local_IPs()) {
		lprintf("DDNS: no Ethernet interface IP addresses?\n");
		noEthernet = true;
	}

	n = non_blocking_cmd("dig +short public.kiwisdr.com", buf, sizeof(buf), &status);
	if (n > 0 && status >= 0 && WEXITSTATUS(status) == 0) {
		char *ips[NPUB_IPS+1];
		n = kiwi_split(buf, "\n", ips, NPUB_IPS);
		lprintf("SERVER-POOL: %d ip addresses for public.kiwisdr.com\n", n);
		for (i=0; i < n; i++) {
			lprintf("SERVER-POOL: #%d %s\n", i, ips[i]);
			strncpy(ddns.pub_ips[i], ips[i], 31);
			ddns.pub_ips[i][31] = '\0';
			
			if (ddns.pub_valid && strcmp(ddns.ip_pub, ddns.pub_ips[i]) == 0 && ddns.port_ext == 8073 &&
					admcfg_bool("sdr_hu_register", NULL, CFG_REQUIRED) == true)
				ddns.pub_server = true;
		}
		ddns.npub_ips = i;
		if (ddns.pub_server)
			lprintf("SERVER-POOL: ==> we are a server for public.kiwisdr.com\n");
	}

	if (ddns.pub_valid)
		lprintf("DDNS: public ip %s\n", ddns.ip_pub);

	// no Internet access or no serial number available, so no point in registering
	if (noEthernet || noInternet || ddns.serno == 0)
		return;
	
	// Attempt to open NAT port in local network router using UPnP (if router supports IGD).
	// Saves Kiwi owner the hassle of figuring out how to do this manually on their router.
	if (admcfg_bool("auto_add_nat", NULL, CFG_REQUIRED) == true) {
		char *cmd_p;
		asprintf(&cmd_p, "upnpc %s -a %s %d %d TCP 2>&1", (debian_ver != 7)? "-e KiwiSDR" : "",
			ddns.ip_pvt, ddns.port, ddns.port_ext);
		n = non_blocking_cmd(cmd_p, buf, sizeof(buf), &status);
		printf("%s\n", buf);

		if (status >= 0 && n > 0) {
			if (strstr(buf, "code 718")) {
				lprintf("### %s: NAT port mapping in local network firewall/router already exists\n", cmd_p);
				ddns.auto_nat = 3;
			} else
			if (strstr(buf, "is redirected to")) {
				lprintf("### %s: NAT port mapping in local network firewall/router created\n", cmd_p);
				ddns.auto_nat = 1;
			} else {
				lprintf("### %s: No IGD UPnP local network firewall/router found\n", cmd_p);
				lprintf("### %s: See kiwisdr.com for help manually adding a NAT rule on your firewall/router\n", cmd_p);
				ddns.auto_nat = 2;
			}
		} else {
			lprintf("### %s: command failed?\n", cmd_p);
			ddns.auto_nat = 4;
		}
		
		free(cmd_p);
	} else {
		lprintf("### auto NAT is set false\n");
		ddns.auto_nat = 0;
	}
	
	ddns.valid = true;

	system("killall -q noip2");
	if (admcfg_bool("duc_enable", NULL, CFG_REQUIRED) == true) {
		lprintf("starting noip.com DUC\n");
		DUC_enable_start = true;
    	if (background_mode)
			system("sleep 1; /usr/local/bin/noip2 -c " DIR_CFG "/noip2.conf");
		else
			system("sleep 1; ./pkgs/noip2/noip2 -c " DIR_CFG "/noip2.conf");
	}
}


// routine that processes the output of the registration wget command

#define RETRYTIME_WORKED	20
#define RETRYTIME_FAIL		2

static int _reg_SDR_hu(void *param)
{
	nbcmd_args_t *args = (nbcmd_args_t *) param;
	int n = args->bc;
	char *sp = args->bp, *sp2;
	int retrytime_mins = args->func_param;

	if (n > 0 && (sp = strstr(args->bp, "UPDATE:")) != 0) {
		sp += 7;
		if (strncmp(sp, "SUCCESS", 7) == 0) {
			if (retrytime_mins != RETRYTIME_WORKED) lprintf("sdr.hu registration: WORKED\n");
			retrytime_mins = RETRYTIME_WORKED;
		} else {
			if ((sp2 = strchr(sp, '\n')) != NULL)
				*sp2 = '\0';
			lprintf("sdr.hu registration: \"%s\"\n", sp);
			retrytime_mins = RETRYTIME_FAIL;
		}
	} else {
		lprintf("sdr.hu registration: FAILED n=%d sp=%p <%.32s>\n", n, sp, sp);
		retrytime_mins = RETRYTIME_FAIL;
	}
	
	return retrytime_mins;
}

static void reg_SDR_hu(void *param)
{
	int n;
	char *cmd_p;
	int retrytime_mins = RETRYTIME_FAIL;
	
	// reply is a bunch of HTML, buffer has to be big enough not to miss/split status
	#define NBUF 16384
	
	const char *server_url = cfg_string("server_url", NULL, CFG_OPTIONAL);
	const char *api_key = admcfg_string("api_key", NULL, CFG_OPTIONAL);
	asprintf(&cmd_p, "wget --timeout=15 -qO- http://sdr.hu/update --post-data \"url=http://%s:%d&apikey=%s\" 2>&1",
		server_url, ddns.port_ext, api_key);
	cfg_string_free(server_url);
	admcfg_string_free(api_key);

	while (1) {
        //printf("%s\n", cmd_p);
		retrytime_mins = non_blocking_cmd_child(cmd_p, _reg_SDR_hu, retrytime_mins, NBUF);
		TaskSleepUsec(SEC_TO_USEC(MINUTES_TO_SEC(retrytime_mins)));
	}
	
	free(cmd_p);
	#undef NBUF
}

static int _reg_kiwisdr_com(void *param)
{
	nbcmd_args_t *args = (nbcmd_args_t *) param;
	int n = args->bc;
	char *sp = args->bp;
	sp[n] = '\0';
    //printf("_reg_kiwisdr_com <%s>\n", sp);

	return 0;
}

static void reg_kiwisdr_com(void *param)
{
	int n;
	char *cmd_p;
	int retrytime_mins = 30;
	
	// reply is a bunch of HTML, buffer has to be big enough not to miss/split status
	#define NBUF 256
	
	const char *server_url = cfg_string("server_url", NULL, CFG_OPTIONAL);
	const char *api_key = admcfg_string("api_key", NULL, CFG_OPTIONAL);
	const char *admin_email = cfg_string("admin_email", NULL, CFG_OPTIONAL);
	char *email = str_encode((char *) admin_email);
	cfg_string_free(admin_email);
	int add_nat = (admcfg_bool("auto_add_nat", NULL, CFG_OPTIONAL) == true)? 1:0;

	TaskSleepUsec(SEC_TO_USEC(10));		// long enough for ddns.mac to become valid

	while (1) {
		asprintf(&cmd_p, "wget --timeout=15 -qO- \"http://kiwisdr.com/php/update.php?url=http://%s:%d&apikey=%s&mac=%s&email=%s&add_nat=%d&ver=%d.%d&up=%d\" 2>&1",
			server_url, ddns.port_ext, api_key, ddns.mac,
			email, add_nat, version_maj, version_min, timer_sec());
        //printf("%s\n", cmd_p);
		non_blocking_cmd_child(cmd_p, _reg_kiwisdr_com, retrytime_mins, NBUF);
		free(cmd_p);
		TaskSleepUsec(SEC_TO_USEC(MINUTES_TO_SEC(retrytime_mins)));
	}
	
	cfg_string_free(server_url);
	admcfg_string_free(api_key);
	free(email);
	#undef NBUF
}

void services_start(bool restart)
{
	CreateTask(dyn_DNS, 0, WEBSERVER_PRIORITY);
	CreateTask(get_TZ, 0, WEBSERVER_PRIORITY);

	if (!no_net && !restart && !down && !alt_port && admcfg_bool("sdr_hu_register", NULL, CFG_PRINT) == true) {
		CreateTask(reg_SDR_hu, 0, WEBSERVER_PRIORITY);
		CreateTask(reg_kiwisdr_com, 0, WEBSERVER_PRIORITY);
	}
}

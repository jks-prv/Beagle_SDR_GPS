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
#include "leds.h"

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
	char *cmd_p, *reply, *lat_lon;
	cfg_t cfg_tz;
	
	TaskSleepSec(10);		// long enough for ddns.lat_lon_valid to be set

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
		asprintf(&cmd_p, "curl -s --ipv4 \"https://maps.googleapis.com/maps/api/timezone/json?location=%f,%f&timestamp=%lu&sensor=false\" 2>&1",
			lat, lon, utc_sec);
		reply = non_blocking_cmd(cmd_p, &stat);
		free(cmd_p);
		if (reply == NULL || stat < 0 || WEXITSTATUS(stat) != 0) {
			lprintf("TIMEZONE: googleapis.com curl error\n");
		    kstr_free(reply);
			goto retry;
		}
	
		json_init(&cfg_tz, kstr_sp(reply));
		kstr_free(reply);
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
		s = tzone_id; tzone_id = kiwi_str_encode(s); cfg_string_free(s);
		s = tzone_name; tzone_name = kiwi_str_encode(s); cfg_string_free(s);
		
		#define KIWI_SURVEY
		#ifdef KIWI_SURVEY
            
            #define SURVEY_LAST 179
            if (admcfg_int("survey", NULL, CFG_REQUIRED) != SURVEY_LAST) {
                admcfg_set_int("survey", SURVEY_LAST);
	            admcfg_save_json(cfg_adm.json);
            
                bool sdr_hu_reg;
                sdr_hu_reg = (admcfg_bool("sdr_hu_register", NULL, CFG_OPTIONAL) == 1)? 1:0;
                char *cmd_p;

                if (sdr_hu_reg) {
                    const char *server_url;
                    server_url = cfg_string("server_url", NULL, CFG_OPTIONAL);
                    // proxy always uses port 8073
                    int sdr_hu_dom_sel;
                    sdr_hu_dom_sel = cfg_int("sdr_hu_dom_sel", NULL, CFG_REQUIRED);
                    int server_port;
                    server_port = (sdr_hu_dom_sel == DOM_SEL_REV)? 8073 : ddns.port_ext;
                    
                    #if 0
                    asprintf(&cmd_p, "curl --silent --show-error --ipv4 --connect-timeout 15 "
                        "\"http://%s/php/survey.php?last=%d&serno=%d&dna=%08x%08x&mac=%s&sdr_hu=1&url=http://%s:%d&tz_id=%s&tz_n=%s\"",
                        ddns.ips_kiwisdr_com.backup? ddns.ips_kiwisdr_com.ip_list[0] : "kiwisdr.com",
                        SURVEY_LAST, ddns.serno, PRINTF_U64_ARG(ddns.dna), ddns.mac,
                        server_url, server_port, tzone_id, tzone_name);
                    #else
                    asprintf(&cmd_p, "curl --silent --show-error --ipv4 --connect-timeout 15 "
                        "\"http://%s/php/survey.php?last=%d&serno=%d&dna=%08x%08x&mac=%s&sdr_hu=1&url=http://%s:%d\"",
                        ddns.ips_kiwisdr_com.backup? ddns.ips_kiwisdr_com.ip_list[0] : "kiwisdr.com",
                        SURVEY_LAST, ddns.serno, PRINTF_U64_ARG(ddns.dna), ddns.mac, server_url, server_port);
                    #endif
                    cfg_string_free(server_url);
                } else {
                    asprintf(&cmd_p, "curl --silent --show-error --ipv4 --connect-timeout 15 "
                        "\"http://%s/php/survey.php?last=%d&serno=%d&dna=%08x%08x&mac=%s&sdr_hu=0\"",
                        ddns.ips_kiwisdr_com.backup? ddns.ips_kiwisdr_com.ip_list[0] : "kiwisdr.com",
                        SURVEY_LAST, ddns.serno, PRINTF_U64_ARG(ddns.dna), ddns.mac);
                }

                non_blocking_cmd(cmd_p, &stat);
                free(cmd_p);
            }
        #endif

		return;
retry:
		if (report) lprintf("TIMEZONE: will retry..\n");
		if (report) report--;
		TaskSleepSec(MINUTES_TO_SEC(1));
	}
}

static bool ipinfo_json(char *buf)
{
	int n;
	char *s;
	cfg_t cfgx;
	
	if (buf == NULL) return false;
	json_init(&cfgx, buf);
	//_cfg_walk(&cfgx, NULL, cfg_print_tok, NULL);
	
	s = (char *) json_string(&cfgx, "ip", NULL, CFG_OPTIONAL);
	if (s == NULL) return false;
	kiwi_strncpy(ddns.ip_pub, s, NET_ADDRSTRLEN);
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

static int UPnP_port_open(const char *host, int port_int, int port_ext)
{
    int status, rtn = 0;
	char *reply;
    char *cmd_p;
    asprintf(&cmd_p, "upnpc %s -a %s %d %d TCP 2>&1", (debian_ver != 7)? "-e KiwiSDR" : "", host, port_int, port_ext);
    reply = non_blocking_cmd(cmd_p, &status);
    char *rp = kstr_sp(reply);

    if (status >= 0 && reply != NULL) {
        printf("%s\n", rp);
        if (strstr(rp, "code 718")) {
            lprintf("### %s: NAT port mapping in local network firewall/router already exists\n", cmd_p);
            rtn = 3;
        } else
        if (strstr(rp, "is redirected to")) {
            lprintf("%s: NAT port mapping in local network firewall/router created\n", cmd_p);
            rtn = 1;
        } else {
            lprintf("### %s: No IGD UPnP local network firewall/router found\n", cmd_p);
            lprintf("### %s: See kiwisdr.com for help manually adding a NAT rule on your firewall/router\n", cmd_p);
            rtn = 2;
        }
    } else {
        lprintf("### %s: command failed?\n", cmd_p);
        rtn = 4;
    }
    
    free(cmd_p);
    kstr_free(reply);
    return rtn;
}

// we've seen the ident.me site respond very slowly at times, so do this in a separate task
// FIXME: this doesn't work if someone is using WiFi or USB networking because only "eth0" is checked

static void dyn_DNS(void *param)
{
	int i, n, status;
	char *reply;
	bool noEthernet = false, noInternet = false;

	if (!do_dyn_dns)
		return;

	ddns.serno = serial_number;
	
	for (i=0; i<1; i++) {	// hack so we can use 'break' statements below

		// get Ethernet interface MAC address
		reply = read_file_string_reply("/sys/class/net/eth0/address");
		if (reply != NULL) {
			n = sscanf(kstr_sp(reply), "%17s", ddns.mac);
			assert (n == 1);
			kstr_free(reply);
		} else {
			noInternet = true;
			break;
		}
		
		if (no_net) {
			noInternet = true;
			break;
		}
		
		// get our public IP and possibly lat/lon
		//reply = non_blocking_cmd("curl -s --ipv4 ident.me", &status);
		//reply = non_blocking_cmd("curl -s --ipv4 icanhazip.com", &status);
		reply = non_blocking_cmd("curl -s --ipv4 --connect-timeout 15 ipinfo.io/json/", &status);
		if (status < 0 || WEXITSTATUS(status) != 0 || reply == NULL || !ipinfo_json(kstr_sp(reply))) {
			reply = non_blocking_cmd("curl -s --ipv4 --connect-timeout 15 freegeoip.net/json/", &status);
			if (status < 0 || WEXITSTATUS(status) != 0 || reply == NULL || !ipinfo_json(kstr_sp(reply)))
				break;
		}
		kstr_free(reply);
	}
	
	if (ddns.serno == 0) lprintf("DDNS: no serial number?\n");
	if (noEthernet) lprintf("DDNS: no Ethernet interface active?\n");
	if (noInternet) lprintf("DDNS: no Internet access?\n");

	if (!find_local_IPs()) {
		lprintf("DDNS: no Ethernet interface IP addresses?\n");
		noEthernet = true;
	}

    DNS_lookup("sdr.hu", &ddns.ips_sdr_hu, N_IPS, SDR_HU_PUBLIC_IP);
    DNS_lookup("kiwisdr.com", &ddns.ips_kiwisdr_com, N_IPS, KIWISDR_COM_PUBLIC_IP);

    bool reg_sdr_hu = (admcfg_bool("sdr_hu_register", NULL, CFG_REQUIRED) == true);
    n = DNS_lookup("public.kiwisdr.com", &ddns.pub_ips, N_IPS, KIWISDR_COM_PUBLIC_IP);
    lprintf("SERVER-POOL: %d ip addresses for public.kiwisdr.com\n", n);
    for (i = 0; i < n; i++) {
        lprintf("SERVER-POOL: #%d %s\n", i+1, ddns.pub_ips.ip_list[i]);
        if (ddns.pub_valid && strcmp(ddns.ip_pub, ddns.pub_ips.ip_list[i]) == 0 && ddns.port_ext == 8073 && reg_sdr_hu)
            ddns.pub_server = true;
    }
    if (ddns.pub_server)
        lprintf("SERVER-POOL: ==> we are a server for public.kiwisdr.com\n");
    
	if (ddns.pub_valid)
		lprintf("DDNS: public ip %s\n", ddns.ip_pub);

    if (!disable_led_task)
	    CreateTask(led_task, NULL, ADMIN_PRIORITY);

	// no Internet access or no serial number available, so no point in registering
	if (noEthernet || noInternet || ddns.serno == 0)
		return;
	
	// Attempt to open NAT port in local network router using UPnP (if router supports IGD).
	// Saves Kiwi owner the hassle of figuring out how to do this manually on their router.
	if (admcfg_bool("auto_add_nat", NULL, CFG_REQUIRED) == true) {
	    ddns.auto_nat = UPnP_port_open(ddns.ip_pvt, ddns.port, ddns.port_ext);
	} else {
		lprintf("auto NAT is set false\n");
		ddns.auto_nat = 0;
	}
	
	// proxy testing
	#ifdef PROXY_TEST
        if (test_flag) {
        
            //#define NGINX
            #ifdef NGINX
                UPnP_port_open("192.168.1.100", 6001, 6001);
            #endif
            
            #define FRP
            #ifdef FRP
                UPnP_port_open("192.168.1.100", 7500, 7500);
                UPnP_port_open("192.168.1.100", 6001, 6001);
            #endif
        }
    #endif

	ddns.valid = true;

    // DUC
	system("killall -q noip2");
	if (admcfg_bool("duc_enable", NULL, CFG_REQUIRED) == true) {
		lprintf("starting noip.com DUC\n");
		DUC_enable_start = true;
    	if (background_mode)
			system("sleep 1; /usr/local/bin/noip2 -c " DIR_CFG "/noip2.conf");
		else
			system("sleep 1; ./pkgs/noip2/noip2 -c " DIR_CFG "/noip2.conf");
	}

    // reverse proxy
	system("killall -q frpc");
	int sdr_hu_dom_sel = cfg_int("sdr_hu_dom_sel", NULL, CFG_REQUIRED);
	printf("PROXY: sdr_hu_dom_sel=%d\n", sdr_hu_dom_sel);
	
	if (sdr_hu_dom_sel == DOM_SEL_REV) {
		lprintf("PROXY: starting frpc\n");
		rev_enable_start = true;
    	if (background_mode)
			system("sleep 1; /usr/local/bin/frpc -c " DIR_CFG "/frpc.ini &");
		else
			system("sleep 1; ./pkgs/frp/frpc -c " DIR_CFG "/frpc.ini &");
	}
}

static void git_commits(void *param)
{
	int i, n, status;
	char *reply;

    reply = non_blocking_cmd("git log --format='format:%h %ci %s' --grep='^v[1-9]' --grep='^release v[1-9]' | head", &status);
    char *rp = kstr_sp(reply);

    if (status >= 0 && reply != NULL) {
        //TaskSleepSec(15);
        while (*rp != '\0') {
            char *rpe = strchr(rp, '\n');
            if (rpe == NULL)
                break;
            int slen = rpe - rp;

            char sha[16], date[16], time[16], tz[16], msg[256];
            int vmaj, vmin;
            n = -1;
            n = sscanf(rp, "%15s %15s %15s %15s v%d.%d: %255[^\n]", sha, date, time, tz, &vmaj, &vmin, msg);
            if (n != 7)
                n = sscanf(rp, "%15s %15s %15s %15s release v%d.%d: %255[^\n]", sha, date, time, tz, &vmaj, &vmin, msg);
            if (n != 7) {
                printf("GIT ERROR <%.*s>\n", slen, rp);
            } else {
                //printf("<%.*s>\n", slen, rp);
                printf("%s v%d.%d \"%s\"\n", sha, vmaj, vmin, msg);
            }
            rp = rpe + 1;
        }
    }

    kstr_free(reply);
}


/*
    // task
    reg_SDR_hu()
        status = non_blocking_cmd_func_child(cmd, _reg_SDR_hu)
		    if (WIFEXITED(status) && (exit_status = WEXITSTATUS(status)))
		        retrytime_mins = exit_status;

    non_blocking_cmd_func_child(cmd, func)
        return status = child_task(_non_blocking_cmd_forall, cmd, func)
    
    child_task(func)
        if (fork())
            // child
            func() -> _non_blocking_cmd_forall(cmd, func)
                result = popen(cmd)
                rv = func(result) -> _reg_SDR_hu(result)
                                        if (result) ...
                exit(rv)
    
        // parent
        while
            waitpid(&status)
        return status
*/

// routine that processes the output of the registration wget command

#define RETRYTIME_WORKED	20
#define RETRYTIME_FAIL		2

static int _reg_SDR_hu(void *param)
{
	nbcmd_args_t *args = (nbcmd_args_t *) param;
	char *sp = kstr_sp(args->kstr), *sp2, *sp3;
	int retrytime_mins = args->func_param;

	if (sp == NULL) {
		lprintf("sdr.hu registration: DOWN\n");
        retrytime_mins = RETRYTIME_FAIL;
	} else {
        if ((sp2 = strstr(sp, "UPDATE:")) != 0) {
            sp2 += 7;
            
            if ((sp3 = strchr(sp2, '\n')) != NULL)
                *sp3 = '\0';
            else
            if ((sp3 = strchr(sp2, '<')) != NULL)
                *sp3 = '\0';
            
            if (strncmp(sp2, "SUCCESS", 7) == 0) {
                if (retrytime_mins != RETRYTIME_WORKED || sdr_hu_debug)
                    lprintf("sdr.hu registration: WORKED\n");
                retrytime_mins = RETRYTIME_WORKED;
            } else {
                lprintf("sdr.hu registration: \"%s\"\n", sp2);
                retrytime_mins = RETRYTIME_FAIL;
            }
        } else {
            lprintf("sdr.hu registration: FAILED <%.64s>\n", sp);
            retrytime_mins = RETRYTIME_FAIL;
        }
        
        // pass sdr.hu reply message back to parent task
        //printf("SET sdr_hu_status %d [%s]\n", strlen(sp2), sp2);
        kiwi_strncpy(log_save_p->sdr_hu_status, sp2, N_LOG_MSG_LEN);
    }
	
	return retrytime_mins;
}

static void reg_SDR_hu(void *param)
{
	char *cmd_p;
	int retrytime_mins = RETRYTIME_FAIL;
	
	while (!ddns.ips_sdr_hu.valid)
        TaskSleepSec(5);		// wait for ddns.ips_sdr_hu to become valid (needed in processing of /status reply)

	while (1) {
        const char *server_url = cfg_string("server_url", NULL, CFG_OPTIONAL);
        const char *api_key = admcfg_string("api_key", NULL, CFG_OPTIONAL);

        if (server_url == NULL || api_key == NULL) return;
        //char *server_enc = kiwi_str_encode((char *) server_url);
        
        // proxy always uses port 8073
	    int sdr_hu_dom_sel = cfg_int("sdr_hu_dom_sel", NULL, CFG_REQUIRED);
        int server_port = (sdr_hu_dom_sel == DOM_SEL_REV)? 8073 : ddns.port_ext;
        
        // registration must be sent from proxy server if proxying so sdr.hu ip-address/url-address check will work
        
        // use "--inet4-only" because if sdr.hu receives an ipv6 registration packet it doesn't match
        // against a possible ipv6 domain record ("AAAA") if it exists.
        
        if (sdr_hu_dom_sel == DOM_SEL_REV) {
            asprintf(&cmd_p, "wget --timeout=15 --tries=3 --inet4-only -qO- \"http://proxy.kiwisdr.com?url=http://%s:%d&apikey=%s\" 2>&1",
			    server_url, server_port, api_key);
		} else {
            asprintf(&cmd_p, "wget --timeout=15 --tries=3 --inet4-only -qO- http://%s/update --post-data \"url=http://%s:%d&apikey=%s\" 2>&1",
			    ddns.ips_sdr_hu.backup? ddns.ips_sdr_hu.ip_list[0] : "sdr.hu", server_url, server_port, api_key);
		}
        //free(server_enc);
        cfg_string_free(server_url);
        admcfg_string_free(api_key);

		bool server_enabled = (!down && admcfg_bool("server_enabled", NULL, CFG_REQUIRED) == true);
        bool sdr_hu_register = (admcfg_bool("sdr_hu_register", NULL, CFG_REQUIRED) == true);

        if (server_enabled && sdr_hu_register) {
            if (sdr_hu_debug)
                printf("%s\n", cmd_p);

		    int status = non_blocking_cmd_func_child("kiwi.reg", cmd_p, _reg_SDR_hu, retrytime_mins, POLL_MSEC(1000));
		    int exit_status;
		    if (WIFEXITED(status) && (exit_status = WEXITSTATUS(status))) {
		        retrytime_mins = exit_status;
                if (sdr_hu_debug)
		            printf("reg_SDR_hu retrytime_mins=%d\n", retrytime_mins);
		    }
		} else {
		    retrytime_mins = RETRYTIME_FAIL;    // check frequently for registration to be re-enabled
		}
		
	    free(cmd_p);

		TaskSleepSec(MINUTES_TO_SEC(retrytime_mins));
	}
}

#define RETRYTIME_KIWISDR_COM		15

static int _reg_kiwisdr_com(void *param)
{
	nbcmd_args_t *args = (nbcmd_args_t *) param;
	char *sp = kstr_sp(args->kstr);
	if (sp == NULL) {
	    printf("_reg_kiwisdr_com: sp == NULL?\n");
	    return 0;   // we've seen this happen
	}
    //printf("_reg_kiwisdr_com <%s>\n", sp);

    int status = 0;
    sscanf(sp, "status %d", &status);
    //printf("_reg_kiwisdr_com status=%d\n", status);

	return status;
}

int reg_kiwisdr_com_status;

static void reg_kiwisdr_com(void *param)
{
	char *cmd_p;
	int retrytime_mins;
	
	while (ddns.mac[0] == '\0')
        TaskSleepSec(5);		// wait for ddns.mac to become valid (used below)

	while (!ddns.ips_kiwisdr_com.valid)
        TaskSleepSec(5);		// wait for ddns.ips_kiwisdr_com to become valid (not really necessary?)

	while (1) {
        const char *server_url = cfg_string("server_url", NULL, CFG_OPTIONAL);
        const char *api_key = admcfg_string("api_key", NULL, CFG_OPTIONAL);

        const char *admin_email = cfg_string("admin_email", NULL, CFG_OPTIONAL);
        char *email = kiwi_str_encode((char *) admin_email);
        cfg_string_free(admin_email);

        int add_nat = (admcfg_bool("auto_add_nat", NULL, CFG_OPTIONAL) == true)? 1:0;
        //char *server_enc = kiwi_str_encode((char *) server_url);

        // proxy always uses port 8073
	    int sdr_hu_dom_sel = cfg_int("sdr_hu_dom_sel", NULL, CFG_REQUIRED);
        int server_port = (sdr_hu_dom_sel == DOM_SEL_REV)? 8073 : ddns.port_ext;

	    // done here because updating timer_sec() is sent
        asprintf(&cmd_p, "wget --timeout=15 --tries=3 --inet4-only -qO- "
            "\"http://%s/php/update.php?url=http://%s:%d&apikey=%s&mac=%s&email=%s&add_nat=%d&ver=%d.%d&deb=%d.%d&up=%d\" 2>&1",
            ddns.ips_kiwisdr_com.backup? ddns.ips_kiwisdr_com.ip_list[0] : "kiwisdr.com", server_url, server_port, api_key, ddns.mac,
            email, add_nat, version_maj, version_min, debian_maj, debian_min, timer_sec());
    
		bool server_enabled = (!down && admcfg_bool("server_enabled", NULL, CFG_REQUIRED) == true);
        bool sdr_hu_register = (admcfg_bool("sdr_hu_register", NULL, CFG_REQUIRED) == true);

        if (server_enabled && sdr_hu_register) {
            if (sdr_hu_debug)
                printf("%s\n", cmd_p);

            retrytime_mins = RETRYTIME_KIWISDR_COM;
		    int status = non_blocking_cmd_func_child("kiwi.reg", cmd_p, _reg_kiwisdr_com, retrytime_mins, POLL_MSEC(1000));
		    int exit_status;
		    if (WIFEXITED(status) && (exit_status = WEXITSTATUS(status))) {
		        reg_kiwisdr_com_status = exit_status;
                if (sdr_hu_debug)
		            printf("reg_SDR_hu reg_kiwisdr_com_status=0x%x\n", reg_kiwisdr_com_status);
		    }
		} else {
		    retrytime_mins = RETRYTIME_FAIL;    // check frequently for registration to be re-enabled
		}

		free(cmd_p);
		//free(server_enc);
        cfg_string_free(server_url);
        admcfg_string_free(api_key);
        free(email);
        
		TaskSleepSec(MINUTES_TO_SEC(retrytime_mins));
	}
}

void services_start(bool restart)
{
	CreateTask(dyn_DNS, 0, WEBSERVER_PRIORITY);
	CreateTask(get_TZ, 0, WEBSERVER_PRIORITY);
	//CreateTask(git_commits, 0, WEBSERVER_PRIORITY);

	if (!no_net && !restart && !alt_port) {
		CreateTask(reg_SDR_hu, 0, WEBSERVER_PRIORITY);
		CreateTask(reg_kiwisdr_com, 0, WEBSERVER_PRIORITY);
	}
}

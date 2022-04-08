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
#include "services.h"
#include "mem.h"
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
#include "non_block.h"
#include "eeprom.h"
#include "rx_waterfall.h"
#include "security.h"

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
#include <errno.h>

int utc_offset = -1, dst_offset = -1;
char *tzone_id = (char *) "null", *tzone_name = (char *) "null";

static void get_TZ(void *param)
{
	int n, status;
	char *cmd_p, *reply, *lat_lon;
	cfg_t cfg_tz;
	
    TaskSleepSec(5);    // under normal conditions ipinfo takes a few seconds to complete

	int report = 3;
	while (1) {
		float lat, lon;
		char *s;
		bool err, haveLatLon = false;
	
		lat_lon = (char *) cfg_string("rx_gps", NULL, CFG_OPTIONAL);
		if (lat_lon != NULL) {
			n = sscanf(lat_lon, "%*[^0-9+-]%f%*[^0-9+-]%f)", &lat, &lon);
			// consider default lat/lon to be the same as unset
			if (n == 2 && strcmp(lat_lon, "(-37.631120, 176.172210)") != 0 && strcmp(lat_lon, "(0.000000, 0.000000)") != 0) {
				lprintf("TIMEZONE: lat/lon from admin public config: (%f, %f)\n", lat, lon);
				haveLatLon = true;
			}
			cfg_string_free(lat_lon);
		}
	
		if (!haveLatLon && gps.StatLat) {
			lat = gps.sgnLat; lon = gps.sgnLon;
			lprintf("TIMEZONE: lat/lon from GPS: (%f, %f)\n", lat, lon);
			haveLatLon = true;
		}
		
		// lowest priority since it will be least accurate
		if (!haveLatLon && gps.ipinfo_ll_valid) {
			lat = gps.ipinfo_lat; lon = gps.ipinfo_lon;
			lprintf("TIMEZONE: lat/lon from ipinfo: (%f, %f)\n", lat, lon);
			haveLatLon = true;
		}
		
		if (!haveLatLon) {
			if (report) lprintf("TIMEZONE: no lat/lon available from admin public config, ipinfo or GPS\n");
			goto retry;
		}
		
		gps.lowres_lat = ((int) roundf(lat)) & ~1;
		gps.lowres_lon = ((int) roundf(lon)) & ~1;
	
		#define TIMEZONE_DB_COM
		#ifdef TIMEZONE_DB_COM
            #define TZ_SERVER "timezonedb.com"
            asprintf(&cmd_p, "curl -s --ipv4 \"https://api.timezonedb.com/v2.1/get-time-zone?key=HIHUSGTXYI55&format=json&by=position&lat=%f&lng=%f\" 2>&1",
                lat, lon);
        #else
            #define TZ_SERVER "googleapis.com"
            time_t utc_sec = utc_time();
            asprintf(&cmd_p, "curl -s --ipv4 \"https://maps.googleapis.com/maps/api/timezone/json?key=&location=%f,%f&timestamp=%lu&sensor=false\" 2>&1",
                lat, lon, utc_sec);
        #endif

        //printf("TIMEZONE: using %s\n", TZ_SERVER);
		reply = non_blocking_cmd(cmd_p, &status);
		kiwi_ifree(cmd_p);
		if (reply == NULL || status < 0 || WEXITSTATUS(status) != 0) {
			lprintf("TIMEZONE: %s curl error\n", TZ_SERVER);
		    kstr_free(reply);
			goto retry;
		}
	
		json_init(&cfg_tz, kstr_sp(reply));
		kstr_free(reply);
		err = false;
		s = (char *) json_string(&cfg_tz, "status", &err, CFG_OPTIONAL);
		if (err) goto retry;
		if (strcmp(s, "OK") != 0) {
			lprintf("TIMEZONE: %s returned status \"%s\"\n", TZ_SERVER, s);
			err = true;
		}
	    json_string_free(&cfg_tz, s);
		if (err) goto retry;
		
		#ifdef TIMEZONE_DB_COM
            utc_offset = json_int(&cfg_tz, "gmtOffset", &err, CFG_OPTIONAL);
            if (err) goto retry;
            dst_offset = 0;     // gmtOffset includes dst offset
            tzone_id = (char *) json_string(&cfg_tz, "abbreviation", NULL, CFG_OPTIONAL);
            tzone_name = (char *) json_string(&cfg_tz, "zoneName", NULL, CFG_OPTIONAL);
        #else
            utc_offset = json_int(&cfg_tz, "rawOffset", &err, CFG_OPTIONAL);
            if (err) goto retry;
            dst_offset = json_int(&cfg_tz, "dstOffset", &err, CFG_OPTIONAL);
            if (err) goto retry;
            tzone_id = (char *) json_string(&cfg_tz, "timeZoneId", NULL, CFG_OPTIONAL);
            tzone_name = (char *) json_string(&cfg_tz, "timeZoneName", NULL, CFG_OPTIONAL);
        #endif
		
		lprintf("TIMEZONE: from %s for (%f, %f): utc_offset=%d/%.1f dst_offset=%d/%.1f\n",
			TZ_SERVER, lat, lon, utc_offset, (float) utc_offset / 3600, dst_offset, (float) dst_offset / 3600);
		lprintf("TIMEZONE: \"%s\", \"%s\"\n", tzone_id, tzone_name);
		s = tzone_id; tzone_id = kiwi_str_encode(s); json_string_free(&cfg_tz, s);
		s = tzone_name; tzone_name = kiwi_str_encode(s); json_string_free(&cfg_tz, s);
		
	    json_release(&cfg_tz);
		return;
retry:
		if (report) lprintf("TIMEZONE: will retry..\n");
		if (report) report--;
		TaskSleepSec(MINUTES_TO_SEC(1));
	}
}

// done this way because passwords containing single and double quotes are impossible
// to get to chpasswd via a shell invocation
static void set_pwd_task(void *param)
{
    char *cmd_p = (char *) param;
    
    #define PIPE_R 0
    #define PIPE_W 1
    static int si[2];
    scall("P pipe", pipe(si));


	pid_t child_pid;
	scall("fork", (child_pid = fork()));
	
	if (child_pid == 0) {
        scall("C dup PIPE_R", dup2(si[PIPE_R], STDIN_FILENO)); scall("C close PIPE_W", close(si[PIPE_W]));
        scall("C execl", execl("/usr/sbin/chpasswd", "/usr/sbin/chpasswd", (char *) NULL));
        child_exit(EXIT_FAILURE);
    }
    
    scall("P close PIPE_R", close(si[PIPE_R]));
    scall("P write PIPE_W", write(si[PIPE_W], cmd_p, strlen(cmd_p)));
    scall("P close PIPE_W", close(si[PIPE_W]));
}

static void misc_NET(void *param)
{
    char *cmd_p, *cmd_p2;
    int status;
    int err;
    
    // DUC
	system("killall -q noip2");
	if (admcfg_bool("duc_enable", NULL, CFG_REQUIRED) == true) {
		lprintf("starting noip.com DUC\n");
		DUC_enable_start = true;
		
    	if (background_mode)
			system("sleep 1; /usr/local/bin/noip2 -k -c " DIR_CFG "/noip2.conf");
		else
			system("sleep 1; " BUILD_DIR "/gen/noip2 -k -c " DIR_CFG "/noip2.conf");
	}

    // reverse proxy
	system("killall -q frpc");
	int dom_sel = cfg_int("sdr_hu_dom_sel", NULL, CFG_REQUIRED);
	bool proxy = (dom_sel == DOM_SEL_REV);
	lprintf("PROXY: %s dom_sel_menu=%d\n", proxy? "YES":"NO", dom_sel);
	
	if (proxy) {
		lprintf("PROXY: starting frpc\n");
		rev_enable_start = true;
    	if (background_mode)
			system("sleep 1; /usr/local/bin/frpc -c " DIR_CFG "/frpc.ini &");
		else
			system("sleep 1; ./pkgs/frp/frpc -c " DIR_CFG "/frpc.ini &");
	}

    // find and remove known viruses, mostly as a result of Debian root/debian accounts
    // without passwords on networks with ssh open to the Internet
    u4_t vr = 0, vc = 0;
    struct stat st;

	#define VR_DOT_KOWORKER 1
	#define VR_DOT_CRON 2
	#define VR_CRONTAB_ROOT 4
	#define VR_DOT_PROFILES 8

    #define CK(f, r, ...) \
        err = stat(f, &st); \
        if (err == 0) { \
            vr |= r; \
            if (strlen(STRINGIFY(__VA_ARGS__)) == 0) { \
                /*printf("CK vr|=%d unlink: %s\n", r, f);*/ \
                scalle(f, unlink(f)); \
            } else { \
                /*printf("CK vr|=%d cmd: \"%s\"\n", r, STRINGIFY(__VA_ARGS__));*/ \
                __VA_ARGS__ ; \
            } \
            if (r != VR_CRONTAB_ROOT) vc = st.st_ctime; \
        } else { \
            if (errno != ENOENT) perror(f); \
        }
    
    CK("/usr/bin/.koworker", VR_DOT_KOWORKER);
    CK("/usr/bin/.cron", VR_DOT_CRON);

    // NB: dir ".profiles/" not file ".profile"
    #define F_PR "/root/.profiles/"
    CK(F_PR, VR_DOT_PROFILES, (system("rm -rf " F_PR)));
    
    #define F_CT "/var/spool/cron/crontabs/root"
    CK(F_CT, VR_CRONTAB_ROOT, (system("sed -i -f " DIR_CFG "/v.sed " F_CT)));
    
    printf("vr=0x%x vc=0x%x\n", vr, vc);
    
    // apply passwords to password-less root/debian accounts
    int root_pwd_unset=0, debian_pwd_default=0;
    bool error;
    bool passwords_checked = admcfg_bool("passwords_checked", &error, CFG_OPTIONAL);
    if (error) passwords_checked = false;
    bool onetime_password_check = admcfg_bool("onetime_password_check", NULL, CFG_REQUIRED);

    if (onetime_password_check == false) {
        if (passwords_checked) admcfg_rem_bool("passwords_checked");    // for Kiwis that prematurely updated to v1.353
        admcfg_set_bool_save("onetime_password_check", true);
        lprintf("SECURITY: One-time check of Linux passwords..\n");

        status = non_blocking_cmd_system_child("kiwi.chk_pwd", "grep -q '^root::' /etc/shadow", POLL_MSEC(250));
        root_pwd_unset = (WEXITSTATUS(status) == 0)? 1:0;
        
        const char *what = "set to the default";
        status = non_blocking_cmd_system_child("kiwi.chk_pwd", "grep -q '^debian:rcdjoac1gVi9g:' /etc/shadow", POLL_MSEC(250));
        debian_pwd_default = (WEXITSTATUS(status) == 0)? 1:0;
        if (debian_pwd_default == 0) {
            status = non_blocking_cmd_system_child("kiwi.chk_pwd", "grep -q '^debian::' /etc/shadow", POLL_MSEC(250));
            debian_pwd_default = (WEXITSTATUS(status) == 0)? 1:0;
            what = "unset";
        }

        const char *which;

        #ifdef CRYPT_PW
            if (net.serno != 0) {
                asprintf(&cmd_p2, "%d", net.serno);
                which = "Kiwi serial number";
            } else {
                asprintf(&cmd_p2, "kiwizero");
                which = "\"kiwizero\" (because Kiwi serial number is zero!)";
            }
        #else
            const char *admin_pwd = admcfg_string("admin_password", &error, CFG_OPTIONAL);
            if (!error && admin_pwd != NULL && *admin_pwd != '\0') {
                cmd_p2 = strdup(admin_pwd);
                which = "Kiwi admin password";
                admcfg_string_free(admin_pwd);
            } else
            if (net.serno != 0) {
                asprintf(&cmd_p2, "%d", net.serno);
                which = "Kiwi serial number (because Kiwi admin password unset)";
            } else {
                asprintf(&cmd_p2, "kiwizero");
                which = "\"kiwizero\" (because Kiwi admin password unset AND Kiwi serial number is zero!)";
            }
        #endif

        if (root_pwd_unset) {
            lprintf("SECURITY: WARNING Linux \"root\" password is unset!\n");
            lprintf("SECURITY: Setting it to %s\n", which);
            asprintf(&cmd_p, "root:%s", cmd_p2);
            status = child_task("kiwi.set_pwd", set_pwd_task, POLL_MSEC(250), cmd_p);
            status = WEXITSTATUS(status);
            lprintf("SECURITY: \"root\" password set returned status=%d (%s)\n", status, status? "FAIL":"OK");
            kiwi_ifree(cmd_p);
        }

        if (debian_pwd_default) {
            lprintf("SECURITY: WARNING Linux \"debian\" account password is %s!\n", what);
            lprintf("SECURITY: Setting it to %s\n", which);
            asprintf(&cmd_p, "debian:%s", cmd_p2);
            status = child_task("kiwi.set_pwd", set_pwd_task, POLL_MSEC(250), cmd_p);
            status = WEXITSTATUS(status);
            lprintf("SECURITY: \"debian\" password set returned status=%d (%s)\n", status, status? "FAIL":"OK");
            kiwi_ifree(cmd_p);
        }

        kiwi_ifree(cmd_p2);
    }

    // register for my.kiwisdr.com
    // this must be at the end of the routine since it waits an arbitrary amount of time
    
    NET_WAIT_COND("my_kiwi", "misc_NET", net.pvt_valid && net.pub_valid);
    
    bool my_kiwi = admcfg_bool("my_kiwi", NULL, CFG_REQUIRED);
    if (my_kiwi) {
        if (root_pwd_unset || debian_pwd_default)
            asprintf(&cmd_p2, "&r=%d&d=%d", root_pwd_unset, debian_pwd_default);
        else
            cmd_p2 = NULL;

        char *kiwisdr_com = DNS_lookup_result("my_kiwi", "kiwisdr.com", &net.ips_kiwisdr_com);
        asprintf(&cmd_p, "curl --silent --show-error --ipv4 --connect-timeout 5 "
            "\"%s/php/my_kiwi.php?auth=308bb2580afb041e0514cd0d4f21919c&pub=%s&pvt=%s&port=%d&serno=%d%s\"",
            kiwisdr_com, net.ip_pub, net.ip_pvt, net.use_ssl? net.port_http_local : net.port,
            net.serno, cmd_p2? cmd_p2:"");

        kstr_free(non_blocking_cmd(cmd_p, &status));
        kiwi_ifree(cmd_p); kiwi_ifree(cmd_p2);
        lprintf("MY_KIWI: registered\n");
    }
}

#if 0
static void bl_GET(void *param)
{
	int status;
	char *cmd_p, *reply, *dl_sp, st_sp;
	jsmntok_t *jt, *end_tok;
	const char *bl_s;
	int slen, dlen, diff;
	cfg_t cfg = {0};
	kstr_t *sb = NULL;
	
    char *kiwisdr_com = DNS_lookup_result("bl_GET", "kiwisdr.com", &net.ips_kiwisdr_com);
    #define BLACKLIST_FILE "ip_blacklist/ip_blacklist3.cjson"

    asprintf(&cmd_p, "curl -s -f --ipv4 \"%s/%s\" 2>&1", kiwisdr_com, BLACKLIST_FILE);
    //printf("bl_GET: <%s>\n" cmd_p);
    
    reply = non_blocking_cmd(cmd_p, &status);
    kiwi_ifree(cmd_p);

    int exit_status = WEXITSTATUS(status);
    if (WIFEXITED(status) && exit_status != 0) {
        lprintf("bl_GET: failed for %s exit_status=%d\n", BLACKLIST_FILE, exit_status);
        goto fail;
    }

    dl_sp = kstr_sp(reply);
    //real_printf("bl_GET: returned <%s>\n", dl_sp);
    if (json_init(&cfg, dl_sp) == false) {
        lprintf("bl_GET: JSON parse failed for %s\n", BLACKLIST_FILE);
        goto fail;
    }

	end_tok = &(cfg.tokens[cfg.ntok]);
	jt = cfg.tokens;
	if (jt == NULL || !JSMN_IS_ARRAY(jt)) {
        lprintf("bl_GET: JSON token error for %s\n", BLACKLIST_FILE);
        goto fail;
	}

    bl_s = admcfg_string("ip_blacklist", NULL, CFG_REQUIRED);
    slen = strlen(bl_s);
    
    
	for (jt = cfg.tokens + 1; jt != end_tok; jt++) {
		const char *ip_s;
		if (_cfg_type_json(&cfg, JSMN_STRING, jt, &ip_s)) {
		    sb = kstr_cat(sb, (char *) ip_s);
            json_string_free(&cfg, ip_s);
        }
	}
	
    dlen = strlen(kstr_sp(sb));

    diff = strcmp(bl_s, kstr_sp(sb));
    printf("bl_GET: stored=%d downloaded=%d diff=%d\n", slen, dlen, diff);
    admcfg_string_free(bl_s);

    net.ip_blacklist_len = 0;
    system("iptables -D INPUT -j KIWI; iptables -N KIWI; iptables -F KIWI");

	for (jt = cfg.tokens + 1; jt != end_tok; jt++) {
		const char *ip_s;
		if (_cfg_type_json(&cfg, JSMN_STRING, jt, &ip_s)) {
            ip_blacklist_add_iptables((char *) ip_s);
            //real_printf("IP: %s\n", ip_s);
            json_string_free(&cfg, ip_s);
        }
	}

    system("iptables -A KIWI -j RETURN; iptables -A INPUT -j KIWI");
    kstr_free(reply);
    lprintf("bl_GET: successfully downloaded blacklist from %s\n", BLACKLIST_FILE);
#warning update stored bl
    return;

    // if downloading fails just use previously stored blacklist
fail:
    json_release(&cfg);
    kstr_free(reply);
    lprintf("bl_GET: FAILED to download blacklist from %s -- using previously stored blacklist\n", BLACKLIST_FILE);
    ip_blacklist_init();
}
#endif

static bool ipinfo_json(int https, const char *url, const char *path, const char *ip_s, const char *lat_s = NULL, const char *lon_s = NULL)
{
	int n;
	char *s;
	
	int stat;
	char *cmd_p, *reply;
	
	//#define TEST_NO_IPINFO_SERVERS
	#ifdef TEST_NO_IPINFO_SERVERS
	    if (https) {
            TaskSleepSec(5);
	        return false;
	    }
	#endif
	
    asprintf(&cmd_p, "curl -s --ipv4 --connect-timeout 5 \"http%s://%s/%s\" 2>&1", https? "s":"", url, path);
    //printf("IPINFO: <%s>\n", cmd_p);
    
    reply = non_blocking_cmd(cmd_p, &stat);
    kiwi_ifree(cmd_p);

    int estat = WEXITSTATUS(stat);
    if (stat < 0 || estat != 0) {
        lprintf("IPINFO: failed for %s stat=%d %s\n", url, estat, (estat == 28)? "TIMEOUT":"");
        kstr_free(reply);
        return false;
    }
    char *rp = kstr_sp(reply);
    //printf("IPINFO: returned <%s>\n", rp);

	cfg_t cfg_ip;
    //rp[0]=':';    // inject parse error for testing
	bool ret = json_init(&cfg_ip, rp);
	if (ret == false) {
        lprintf("IPINFO: JSON parse failed for %s\n", url);
        kstr_free(reply);
	    return false;
	}
	//json_walk(&cfg_ip, NULL, cfg_print_tok, NULL);
    kstr_free(reply);
	
	ret = false;
	s = (char *) json_string(&cfg_ip, ip_s, NULL, CFG_OPTIONAL);
	if (s != NULL) {
        kiwi_strncpy(net.ip_pub, s, NET_ADDRSTRLEN);
        iparams_add("IP_PUB", s);
        json_string_free(&cfg_ip, s);
        net.pub_valid = true;
		lprintf("IPINFO: public ip %s from %s\n", net.ip_pub, url);
        ret = true;
    }
	
	if (lat_s && lon_s) {
        bool err;
        double lat, lon;
        
        // try as numbers
        lat = json_float(&cfg_ip, lat_s, &err, CFG_OPTIONAL);
        if (!err) {
            lon = json_float(&cfg_ip, lon_s, &err, CFG_OPTIONAL);
            if (!err) {
                gps.ipinfo_lat = lat; gps.ipinfo_lon = lon; gps.ipinfo_ll_valid = true;
            }
        }
        
        // try as strings
        if (!gps.ipinfo_ll_valid) {
            s = (char *) json_string(&cfg_ip, lat_s, NULL, CFG_OPTIONAL);
            if (s != NULL) {
                n = sscanf(s, "%f", &gps.ipinfo_lat);
                json_string_free(&cfg_ip, s);
                if (n == 1) {
                    s = (char *) json_string(&cfg_ip, lon_s, NULL, CFG_OPTIONAL);
                    if (s != NULL) {
                        n = sscanf(s, "%f", &gps.ipinfo_lon);
                        json_string_free(&cfg_ip, s);
                        if (n == 1) gps.ipinfo_ll_valid = true;
                    }
                }
            }
        }
        
        if (gps.ipinfo_ll_valid) {
            lprintf("IPINFO: lat/lon = (%f, %f) from %s\n", gps.ipinfo_lat, gps.ipinfo_lon, url);
        }
    }

	json_release(&cfg_ip);
	return ret;
}

static int _UPnP_port_open(void *param)
{
	nbcmd_args_t *args = (nbcmd_args_t *) param;
	char *rp = kstr_sp(args->kstr);
	int rtn = 0;
	
    if (args->kstr != NULL) {
        printf("UPnP: %s\n", rp);
        if (strstr(rp, "code 718")) {
            lprintf("UPnP: NAT port mapping in local network firewall/router already exists\n");
            rtn = 3;
        } else
        if (strstr(rp, "is redirected to")) {
            lprintf("UPnP: NAT port mapping in local network firewall/router created\n");
            rtn = 1;
        } else
        if (strstr(rp, "UPNP_DeletePortMapping() returned :")) {
            lprintf("UPnP: NAT port mapping in local network firewall/router deleted\n");
            rtn = 7;
        } else {
            lprintf("UPnP: No IGD UPnP local network firewall/router found\n");
            lprintf("UPnP: See kiwisdr.com for help manually adding a NAT rule on your firewall/router\n");
            rtn = 2;
        }
    } else {
        lprintf("UPnP: command failed?\n");
        rtn = 4;
    }

    return rtn;
}

static void UPnP_port_open_task(void *param)
{
    char *cmd_p;
    int status, exit_status;
    int delete_nat = (int) FROM_VOID_PARAM(param);
    //printf("UPnP_port_open_task: delete_nat=%d\n", delete_nat);

    if (delete_nat) {
        asprintf(&cmd_p, "upnpc %s-d %d TCP 2>&1",
            (net.pvt_valid == IPV6)? "-6 " : "",
            net.port_ext);
    } else {
        asprintf(&cmd_p, "upnpc %s%s-a %s %d %d TCP 2>&1",
            (debian_ver != 7)? "-e KiwiSDR " : "",
            (net.pvt_valid == IPV6)? "-6 " : "",
            net.ip_pvt, net.port, net.port_ext);
    }
    net_printf2("UPnP: %s\n", cmd_p);

    status = non_blocking_cmd_func_forall("kiwi.UPnP", cmd_p, _UPnP_port_open, 0, POLL_MSEC(1000));
    if (WIFEXITED(status) && (exit_status = WEXITSTATUS(status))) {
        net.auto_nat = exit_status;
        net_printf2("UPnP_port_open_task: net.auto_nat=%d\n", net.auto_nat);
    } else {
        net.auto_nat = 4;      // command failed
    }
    kiwi_ifree(cmd_p);
    
    #ifdef USE_SSL
        if (delete_nat) return;

        if (net.use_ssl && net.port_ext != 80) {
            asprintf(&cmd_p, "upnpc %s%s-a %s 80 80 TCP 2>&1",
                (debian_ver != 7)? "-e KiwiSDR " : "",
                (net.pvt_valid == IPV6)? "-6 " : "",
                net.ip_pvt);
            printf("UPnP: %s\n", cmd_p);
            status = non_blocking_cmd_func_forall("kiwi.UPnP", cmd_p, _UPnP_port_open, 0, POLL_MSEC(1000));
            if (WIFEXITED(status) && (exit_status = WEXITSTATUS(status))) {
                printf("UPnP_port_open_task: ACME HTTP-01 port exit_status=%d\n", exit_status);
            } else {
                printf("UPnP_port_open_task: ACME HTTP-01 port status=%d\n", status);
            }
            kiwi_ifree(cmd_p);
        }

        //#define TEST_HTTP_LOCAL
        #ifdef TEST_HTTP_LOCAL
            if (net.use_ssl) {
                asprintf(&cmd_p, "upnpc %s%s-a %s %d %d TCP 2>&1",
                    (debian_ver != 7)? "-e KiwiSDR " : "",
                    (net.pvt_valid == IPV6)? "-6 " : "",
                    net.ip_pvt, net.port_http_local, net.port_http_local);
                printf("UPnP: %s\n", cmd_p);
                status = non_blocking_cmd_func_forall("kiwi.UPnP", cmd_p, _UPnP_port_open, 0, POLL_MSEC(1000));
                if (WIFEXITED(status) && (exit_status = WEXITSTATUS(status))) {
                    printf("UPnP_port_open_task: local HTTP port %d exit_status=%d\n", exit_status);
                } else {
                    printf("UPnP_port_open_task: local HTTP port %d status=%d\n", status);
                }
                kiwi_ifree(cmd_p);
            }
        #endif
    #endif
}

void UPnP_port(nat_delete_e nat_delete)
{
    // Requires net.ip_pvt
    // Attempt to open NAT port in local network router using UPnP (if router supports IGD).
    // Saves Kiwi admin the hassle of figuring out how to do this manually on their router.
    net.auto_nat = 0;
    bool add_nat = admcfg_bool("auto_add_nat", NULL, CFG_REQUIRED);
    if (debian_ver == 7 && net.pvt_valid == IPV6) {
        lprintf("auto NAT: not with Debian 7 and IPV6\n");
    } else {
        if (!add_nat && nat_delete == NAT_NO_DELETE) {
            lprintf("auto NAT: is set false\n");
        } else {
            net.auto_nat = add_nat? 5:6;    // mark pending
            int delete_nat = add_nat? 0:1;
            CreateTask(UPnP_port_open_task, TO_VOID_PARAM(delete_nat), SERVICES_PRIORITY);
        }
    }
}

static void pvt_NET(void *param)
{
	int i, n, retry;
	char *reply;

    // make sure /etc/resolv.conf exists
    off_t fsize = kiwi_file_size("/etc/resolv.conf");
    if (fsize <= 0) {
        lprintf("### /etc/resolv.conf missing or zero length, setting to default nameserver 1.1.1.1\n");
        system("echo nameserver 1.1.1.1 >/etc/resolv.conf");
    } else
    
    // make sure well-known DNS server 1.1.1.1 is available as backup
    if (system("grep '1.1.1.1' /etc/resolv.conf >/dev/null 2>&1")) {
        system("echo nameserver 1.1.1.1 >>/etc/resolv.conf");
    }

    DNS_lookup("kiwisdr.com", &net.ips_kiwisdr_com, N_IPS, KIWISDR_COM_PUBLIC_IP);

	for (retry = 0; true; retry++) {

		// get Ethernet interface MAC address
		if (!net.mac_valid) {
            reply = read_file_string_reply("/sys/class/net/eth0/address");
            if (reply != NULL) {
                n = sscanf(kstr_sp(reply), "%17s", net.mac);
                assert (n == 1);
                kstr_free(reply);
                sprintf(net.mac_no_delim, "%.2s%.2s%.2s%.2s%.2s%.2s",
                    &net.mac[0], &net.mac[3], &net.mac[6], &net.mac[9], &net.mac[12], &net.mac[15]);
                printf("NET: eth0 MAC %s (%s)\n", net.mac, net.mac_no_delim);
                net.mac_valid = true;
            }
        }
		
		// get our private IP for use with e.g. admin local network bypass
        find_local_IPs(retry);

        if (net.ip4_valid || net.ip4_6_valid) {
            if (retry) lprintf("NET: private ipv4 address after %d retries\n", retry);
            net.pvt_valid = IPV4;
        }

        #define LOCAL_IP_RETRY 3
        if (retry >= LOCAL_IP_RETRY && net.ip6_valid) {     // NB: not ip6LL_valid
            lprintf("NET: no private ipv4 address after %d retries, but ipv6 address found\n", retry);
            net.pvt_valid = IPV6;
	    }
	    
	    // FIXME: is this strategy with ipv6 desirable when ipv4 is long delayed?
	    if (!net.auto_nat_valid && net.pvt_valid != IPV_NONE) {
	        UPnP_port(NAT_NO_DELETE);
            net.auto_nat_valid = true;
	    }

        if (net.pvt_valid == IPV4)
            break;
        // if ipv6 only continue to search for an ipv4 address
        TaskSleepSec(10);
    }	
}

static void pub_NET(void *param)
{
    char *kiwisdr_com = DNS_lookup_result("pub_NET", "kiwisdr.com", &net.ips_kiwisdr_com);
    
    // get our public IP and possibly lat/lon
    #define N_IPINFO_SERVERS 4
    u4_t i = timer_us();   // mix it up a bit
    #define N_IPINFO_RETRY 6
    int retry = 0;
    bool okay = false;
    do {
        // don't use kiwisdr.com during first half of retries since it doesn't provide lat/lon
        i = (i+1) % ((retry < N_IPINFO_RETRY/2)? (N_IPINFO_SERVERS-1) : N_IPINFO_SERVERS);

        if (i == 0) okay = ipinfo_json(1, "ipapi.co", "json", "ip", "latitude", "longitude");
        else
        if (i == 1) okay = ipinfo_json(1, "get.geojs.io", "v1/ip/geo.json", "ip", "latitude", "longitude");
        else
        if (i == 2) okay = ipinfo_json(0, "ip-api.com", "json", "query", "lat", "lon");
        else
        // must be last
        if (i == 3) okay = ipinfo_json(0, kiwisdr_com, "php/update.php/?pubip=94e2473e8df4e92a0c31944ec62b2a067c26b8d0", "ip");

        retry++;
    } while (!okay && retry <= N_IPINFO_RETRY);   // make multiple attempts
    if (!okay) lprintf("IPINFO: ### FAILED for all ipinfo servers ###\n");
}

/*
static void git_commits(void *param)
{
	int i, n, status;
	char *reply;

    reply = non_blocking_cmd("git log --format='format:%h %ad %s' --grep='^v[1-9]' --grep='^release v[1-9]' | head", &status);
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
*/


/*
    // task
    reg_kiwi()
        status = non_blocking_cmd_func_forall(cmd, _reg_kiwi)
		    if (WIFEXITED(status) && (exit_status = WEXITSTATUS(status)))
		        retrytime_mins = exit_status;

    non_blocking_cmd_func_forall(cmd, func)
        return status = child_task(_non_blocking_cmd_forall, cmd, func)
    
    child_task(func)
        if (fork())
            // child
            func() -> _non_blocking_cmd_forall(cmd, func)
                result = popen(cmd)
                rv = func(result) -> _reg_kiwi(result)
                                        if (result) ...
                child_exit(rv)
    
        // parent
        while
            waitpid(&status)
        return status
*/

// routine that processes the output of the registration wget command

#define RETRYTIME_KIWISDR_COM		30
//#define RETRYTIME_KIWISDR_COM		1
#define RETRYTIME_KIWISDR_COM_FAIL  2

static int _reg_public(void *param)
{
	nbcmd_args_t *args = (nbcmd_args_t *) param;
	char *sp = kstr_sp(args->kstr);
	if (sp == NULL) {
	    printf("_reg_public: sp == NULL?\n");
	    return 0;   // we've seen this happen
	}
    //printf("_reg_public <%s>\n", sp);

    int n, status = 0, kod = 0, serno = 0;
    n = sscanf(sp, "status %d %d %d", &status, &kod, &serno);
    if (n == 3 && status == 22 && serno == net.serno) {
        //printf("_reg_public status=%d kod=%d serno=%d/%d\n", status, kod, serno, net.serno);
        status = kod;
    } else
    if (status >= 42 && status < 50) {
        status = 0;
    }
    //printf("_reg_public status=%d\n", status);

	return status;
}

int reg_kiwisdr_com_status;

static void reg_public(void *param)
{
	char *cmd_p;
	int retrytime_mins;
	
    NET_WAIT_COND("mac", "reg_kiwisdr_com", net.mac_valid);
    char *kiwisdr_com = DNS_lookup_result("reg_kiwisdr_com", "kiwisdr.com", &net.ips_kiwisdr_com);

	while (1) {
        const char *server_url = cfg_string("server_url", NULL, CFG_OPTIONAL);

        const char *admin_email = cfg_string("admin_email", NULL, CFG_OPTIONAL);
        char *email = kiwi_str_encode((char *) admin_email);
        cfg_string_free(admin_email);

        int add_nat = (admcfg_bool("auto_add_nat", NULL, CFG_OPTIONAL) == true)? 1:0;
        //char *server_enc = kiwi_str_encode((char *) server_url);

        bool kiwisdr_com_reg = (admcfg_bool("kiwisdr_com_register", NULL, CFG_OPTIONAL) == true);

        // proxy always uses port 8073
	    int dom_sel = cfg_int("sdr_hu_dom_sel", NULL, CFG_REQUIRED);
        int server_port = (dom_sel == DOM_SEL_REV)? 8073 : net.port_ext;
        int dom_stat = (dom_sel == DOM_SEL_REV)? net.proxy_status : (DUC_enable_start? net.DUC_status : -1);

	    // done here because updating timer_sec() is sent
        asprintf(&cmd_p, "wget --timeout=30 --tries=2 --inet4-only -qO- "
            "\"%s/php/update.php?url=http://%s:%d&mac=%s&email=%s&add_nat=%d&ver=%d.%d&deb=%d.%d"
            "&dom=%d&dom_stat=%d&serno=%d&dna=%08x%08x&reg=%d&pvt=%s&pub=%s&up=%d\" 2>&1",
            kiwisdr_com, server_url, server_port, net.mac,
            email, add_nat, version_maj, version_min, debian_maj, debian_min,
            dom_sel, dom_stat, net.serno, PRINTF_U64_ARG(net.dna), kiwisdr_com_reg? 1:0,
            net.pvt_valid? net.ip_pvt : "not_valid", net.pub_valid? net.ip_pub : "not_valid", timer_sec());
    
		bool server_enabled = (!down && admcfg_bool("server_enabled", NULL, CFG_REQUIRED) == true);
        bool send_deregister = false;
        static bool last_reg;
        if (last_reg && !kiwisdr_com_reg) {     // reg=1 => reg=0 transition
            printf("REG deregister\n");
            send_deregister = true;
        }
        last_reg = kiwisdr_com_reg;

        if (send_deregister || (server_enabled && kiwisdr_com_reg)) {
            if (kiwi_reg_debug)
                printf("%s\n", cmd_p);

            retrytime_mins = RETRYTIME_KIWISDR_COM;
		    int status = non_blocking_cmd_func_forall("kiwi.register", cmd_p, _reg_public, retrytime_mins, POLL_MSEC(1000));
		    if (WIFEXITED(status)) {
		        int exit_status = WEXITSTATUS(status);
                reg_kiwisdr_com_status = exit_status? exit_status : 1;      // for now just indicate that it completed
                if (kiwi_reg_debug) {
                    printf("reg_kiwisdr_com reg_kiwisdr_com_status=0x%x\n", reg_kiwisdr_com_status);
                }
                if (exit_status == 42) {
                    system("touch " DIR_CFG "/opt.debug");
                    
                    #ifdef USE_ASAN
                        // leak detector needs exit while running on main() stack
                        kiwi_restart = true;
                        TaskWakeupF(TID_MAIN, TWF_CANCEL_DEADLINE);
                    #else
                        kiwi_exit(0);
                    #endif
                }
                if (exit_status == 43) {
				    system("reboot");
                    while (true)
                        kiwi_usleep(100000);
                }
		    }
		} else {
		    reg_kiwisdr_com_status = 0;
		    retrytime_mins = RETRYTIME_KIWISDR_COM_FAIL;    // check frequently for registration to be re-enabled
		}

		kiwi_ifree(cmd_p);
		//kiwi_ifree(server_enc);
        cfg_string_free(server_url);
        kiwi_ifree(email);
        
        if (kiwi_reg_debug) printf("reg_kiwisdr_com TaskSleepSec(min=%d)\n", retrytime_mins);
		TaskSleepSec(MINUTES_TO_SEC(retrytime_mins));
	}
}

int reg_kiwisdr_com_tid;

void services_start()
{
	net.serno = serial_number;
	
    // Because these run early on child_task() doesn't have to be used to avoid excessive task pauses.
    // This is good, because otherwise shared memory would have to be used to communicate with
    // the child tasks as it does with led_task.
	CreateTask(pvt_NET, 0, SERVICES_PRIORITY);
	CreateTask(pub_NET, 0, SERVICES_PRIORITY);
	CreateTask(get_TZ, 0, SERVICES_PRIORITY);
	CreateTask(misc_NET, 0, SERVICES_PRIORITY);
    CreateTask(SNR_meas, 0, SERVICES_PRIORITY);
	//CreateTask(git_commits, 0, SERVICES_PRIORITY);

    if (!disable_led_task)
        CreateTask(led_task, NULL, ADMIN_PRIORITY);

    reg_kiwisdr_com_tid = CreateTask(reg_public, 0, SERVICES_PRIORITY);

    #if 0
        if (admcfg_bool("ip_blacklist_auto_download", NULL, CFG_REQUIRED) == true) {
            CreateTask(bl_GET, 0, SERVICES_PRIORITY);
        } else {
            ip_blacklist_init();
        }
    #else
        ip_blacklist_init();
    #endif
}

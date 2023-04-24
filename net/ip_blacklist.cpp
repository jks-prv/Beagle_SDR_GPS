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

// Copyright (c) 2022-2023 John Seamons, ZL/KF6VO

#include "kiwi.h"
#include "types.h"
#include "config.h"
#include "mem.h"
#include "sha256.h"
#include "net.h"
#include "ip_blacklist.h"

// updates net.ip_blacklist
static int ip_blacklist_add(char *ips, bool *whitelist)
{
    char ip_str[NET_ADDRSTRLEN];
    u4_t cidr;
    *whitelist = false;
    
    if (ips[0] == '+') {
        ips++;
        *whitelist = true;
    }
    int n = sscanf(ips, "%[^/]/%d", ip_str, &cidr);
    if (n == 0 || n > 2) return -1;
    if (n == 1) cidr = 32;
    bool error;
    u1_t a,b,c,d;
    u4_t ip = inet4_d2h(ip_str, &error, &a, &b, &c, &d);
    if (error || cidr < 1 || cidr > 32) return -1;
    u4_t nm = ~( (1 << (32-cidr)) -1 );
    ip &= nm;       // make consistent with netmask
    
    if (ip == (net.ips_kiwisdr_com.ip[0] & nm)) {
        lprintf("DANGER: blacklist entry %s would contain kiwisdr.com ip of %s, IGNORED!!!\n", ips, net.ips_kiwisdr_com.ip_list[0]);
        return -1;
    }
    
    int i = net.ip_blacklist_len;
    if (i >= N_IP_BLACKLIST) {
        lprintf("ip_blacklist_add: >= N_IP_BLACKLIST(%d)\n", N_IP_BLACKLIST);
        return -1;
    }
    
    // always add to beginning of list to match iptables insert behavior
    if (net.ip_blacklist_len != 0)
        memmove(&net.ip_blacklist[1], &net.ip_blacklist[0], net.ip_blacklist_len * sizeof(ip_blacklist_t));
    ip_blacklist_t *bl = &net.ip_blacklist[0];
    bl->ip = ip;
    bl->a = a; bl->b = b; bl->c = c; bl->d = d;
    bl->cidr = cidr;
    bl->nm = nm;
    bl->whitelist = *whitelist;
    bl->dropped = bl->last_dropped = 0;
    //printf("ip_blacklist_add[%d] %s %d.%d.%d.%d 0x%08x\n", net.ip_blacklist_len, ips, bl->a, bl->b, bl->c, bl->d, bl->nm);
    net.ip_blacklist_len++;
    
    return 0;
}

// updates net.ip_blacklist (proxied Kiwis) and Linux iptables (non-proxied Kiwis)
// called here and from admin "SET network_ip_blacklist="
int ip_blacklist_add_iptables(char *ip_s)
{
    int rv;
    //real_printf("    \"%s\",\n", ip_s);

    #ifdef TEST_IP_BLACKLIST_USING_LOCAL_IPs
    #else
        bool is_loopback;
        if (isLocal_ip(ip_s, &is_loopback) || is_loopback) {
            lprintf("ip_blacklist_add_iptables: DANGER! local ip ignored: %s\n", ip_s);
            return -1;
        }
    #endif
    
    bool whitelist;
    if ((rv = ip_blacklist_add(ip_s, &whitelist)) != 0) return rv;

    char *cmd_p;
    // NB: insert (-I) NOT append (-A) because we may be incrementally adding after RETURN rule (last) exists
    asprintf(&cmd_p, "iptables -I KIWI -s %s -j %s", ip_s, whitelist? "RETURN" : "DROP");
    rv = non_blocking_cmd_system_child("kiwi.iptables", cmd_p, POLL_MSEC(200));
    rv = WEXITSTATUS(rv);
    lprintf("ip_blacklist_add_iptables: \"%s\" rv=%d\n", cmd_p, rv);
    kiwi_ifree(cmd_p);
    return rv;
}

static void ip_blacklist_init_list(const char *list)
{
    const char *bl_s = admcfg_string(list, NULL, CFG_REQUIRED);
    if (bl_s == NULL) return;

    char *r_buf, *ips[N_IP_BLACKLIST+1];
    int n = kiwi_split((char *) bl_s, &r_buf, " ", ips, N_IP_BLACKLIST);
    //printf("ip_blacklist_init n=%d bl_s=\"%s\"\n", n, bl_s);
    lprintf("ip_blacklist_init_list: %d entries: %s\n", n, list);
    
    for (int i=0; i < n; i++) {
        ip_blacklist_add_iptables(ips[i]);
    }

    kiwi_ifree(r_buf);
    admcfg_string_free(bl_s);
}

void ip_blacklist_init()
{
    net.ip_blacklist_len = 0;
    // clean out old KIWI table first (if any)
    system("iptables -D INPUT -j KIWI; iptables -F KIWI; iptables -X KIWI; iptables -N KIWI");
    ip_blacklist_init_list("ip_blacklist");
    ip_blacklist_init_list("ip_blacklist_local");
    system("iptables -A KIWI -j RETURN; iptables -A INPUT -j KIWI");
}

// check internal blacklist for proxied Kiwis (iptables can't be used)
bool check_ip_blacklist(char *remote_ip, bool log)
{
    bool error;
    u4_t ip = inet4_d2h(remote_ip, &error);
    if (error) return false;
    for (int i=0; i < net.ip_blacklist_len; i++) {
        ip_blacklist_t *bl = &net.ip_blacklist[i];
        u4_t nm = bl->nm;
        if ((ip & nm) == bl->ip) {      // netmask previously applied to bl->ip
            net.ip_blacklist_inuse = true;
            bl->dropped++;
            if (bl->whitelist) return false;
            bl->last_dropped = ip;
            if (log) lprintf("IP BLACKLISTED: %s\n", remote_ip);
            return true;
        }
    }
    return false;
}

void ip_blacklist_dump()
{
    //if (!net.ip_blacklist_inuse) return;
	lprintf("\n");
	lprintf("PROXY IP BLACKLIST:\n");
	lprintf("  dropped  ip\n");
	
	for (int i = 0; i < net.ip_blacklist_len; i++) {
	    ip_blacklist_t *bl = &net.ip_blacklist[i];
	    //if (bl->dropped == 0) continue;
	    u1_t a, b, c, d;
	    inet4_h2d(bl->last_dropped, &a, &b, &c, &d);
	    lprintf("%9d  %18s %08x|%08x last=%d.%d.%d.%d|%08x %s\n",
	        bl->dropped, stprintf("%d.%d.%d.%d/%d", bl->a, bl->b, bl->c, bl->d, bl->cidr),
	        bl->ip, bl->nm, a, b, c, d, bl->last_dropped,
	        bl->whitelist? " WHITELIST" : "");
	}
	lprintf("\n");
}

void bl_GET(void *param)
{
	int status;
	char *cmd_p, *reply, *dl_sp, *bl_sp;
	jsmntok_t *jt, *end_tok;
	const char *bl_old, *bl_new;
	int slen, dlen, diff;
	cfg_t bl_json = {0};
	kstr_t *sb;
	bool failed = true, first, ip_err;
    u4_t mtime;
	
	bool check_only = ((int) FROM_VOID_PARAM(param) == BL_CHECK_ONLY);
	lprintf("bl_GET: running..%s\n", check_only? " (check only)" : "");
	
    char *kiwisdr_com = DNS_lookup_result("bl_GET", "kiwisdr.com", &net.ips_kiwisdr_com);
    #define BLACKLIST_FILE "ip_blacklist/ip_blacklist3.cjson"

    asprintf(&cmd_p, "curl -s -f --ipv4 --connect-timeout 5 \"%s/%s\" 2>&1", kiwisdr_com, BLACKLIST_FILE);
    //printf("bl_GET: <%s>\n", cmd_p);
    
    reply = non_blocking_cmd(cmd_p, &status);
    kiwi_ifree(cmd_p);

    int exit_status = WEXITSTATUS(status);
    if (WIFEXITED(status) && exit_status != 0) {
        lprintf("bl_GET: failed for %s/%s exit_status=%d\n", kiwisdr_com, BLACKLIST_FILE, exit_status);
        goto fail;
    }

    dl_sp = kstr_sp(reply);
    //real_printf("bl_GET: returned <%s>\n", dl_sp);

    SHA256_CTX ctx;
    sha256_init(&ctx);
    sha256_update(&ctx, (BYTE *) dl_sp, strlen(dl_sp));
    BYTE hash[SHA256_BLOCK_SIZE];
    sha256_final(&ctx, hash);
    mg_bin2str(net.ip_blacklist_hash, hash, N_IP_BLACKLIST_HASH_BYTES);
    lprintf("bl_GET: ip_blacklist_hash = %s\n", net.ip_blacklist_hash);

    if (json_init(&bl_json, dl_sp) == false) {
        lprintf("bl_GET: JSON parse failed for %s/%s\n", kiwisdr_com, BLACKLIST_FILE);
        goto fail;
    }

	end_tok = &(bl_json.tokens[bl_json.ntok]);
	jt = bl_json.tokens;
	if (jt == NULL || !JSMN_IS_ARRAY(jt)) {
        lprintf("bl_GET: JSON token error for %s/%s\n", kiwisdr_com, BLACKLIST_FILE);
        goto fail;
	}

    // existing stored blacklist string
    bl_old = admcfg_string("ip_blacklist", NULL, CFG_REQUIRED);
    slen = strlen(bl_old);
    
    // new blacklist string extracted from file json
    bl_sp = NULL;
    ip_err = false;
    first = true;
	for (jt = bl_json.tokens + 1; jt != end_tok && !ip_err; jt++) {
		const char *ip_s;
		if (_cfg_type_json(&bl_json, JSMN_STRING, jt, &ip_s)) {
		    if (!first)
		        bl_sp = kstr_cat(bl_sp, (char *) " ");
		    first = false;
		    bl_sp = kstr_cat(bl_sp, (char *) ip_s);
            inet4_d2h((char *) ip_s, &ip_err);
            json_string_free(&bl_json, ip_s);
        }
	}
	bl_new = kstr_sp(bl_sp);
    dlen = strlen(bl_new);
    
    if (ip_err) {
        lprintf("bl_GET: ip address parse fail for %s/%s\n", kiwisdr_com, BLACKLIST_FILE);
        admcfg_string_free(bl_old);
        kstr_free(bl_sp);
        goto fail;
    }
    
    diff = strcmp(bl_old, bl_new);
    lprintf("bl_GET: stored=%d downloaded=%d diff=%s\n", slen, dlen, diff? "YES" : "NO");
    admcfg_string_free(bl_old);

    if (check_only) {
        //printf("bl_GET: check only\n");
        if (diff) {
            lprintf("bl_GET: new blacklist available, restarting..\n");
            kiwi_exit(0);
        }
    } else {
        if (diff) {
            // update stored blacklist
            mtime = utc_time();
            printf("bl_GET: mtime=%d\n", mtime);
            admcfg_set_int("ip_blacklist_mtime", mtime);
            admcfg_set_string_save("ip_blacklist", bl_new);
            printf("bl_GET: CFG SAVE DONE\n");

            // reload iptables
            lprintf("bl_GET: using DOWNLOADED blacklist from %s/%s\n", kiwisdr_com, BLACKLIST_FILE);
            net.ip_blacklist_len = 0;
            system("iptables -D INPUT -j KIWI; iptables -N KIWI; iptables -F KIWI");
            for (jt = bl_json.tokens + 1; jt != end_tok; jt++) {
                const char *ip_s;
                if (_cfg_type_json(&bl_json, JSMN_STRING, jt, &ip_s)) {
                    ip_blacklist_add_iptables((char *) ip_s);
                    json_string_free(&bl_json, ip_s);
                }
            }

            ip_blacklist_init_list("ip_blacklist_local");
            system("iptables -A KIWI -j RETURN; iptables -A INPUT -j KIWI");
        } else {
            lprintf("bl_GET: using STORED blacklist\n");
            failed = false;
            goto use_stored;
        }
    }

	kiwi.allow_admin_conns = true;
    kstr_free(reply);
    kstr_free(bl_sp);
    json_release(&bl_json);
    return;

    // use previously stored blacklist
use_stored:
fail:
    kstr_free(reply);
    json_release(&bl_json);

    if (failed) 
        lprintf("bl_GET: FAILED to download blacklist from %s/%s\n", kiwisdr_com, BLACKLIST_FILE);
    if (!check_only) {
        lprintf("bl_GET: using previously stored blacklist\n");
        ip_blacklist_init();
    }
	kiwi.allow_admin_conns = true;
}


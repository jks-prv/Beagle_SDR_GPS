/*
    The MIT License (MIT)

    Copyright (c) 2016-2024 Kari Karvonen

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
    SOFTWARE.
*/

#include "ext.h"
#include "kiwi.h"
#include "cfg.h"
#include "str.h"
#include "peri.h"
#include "misc.h"
#include "mem.h"
#include "rx_util.h"
#include "shmem.h"
#include "ant_switch.h"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <math.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/wait.h>

//#define ANTSW_PRF
#ifdef ANTSW_PRF
    #define antsw_printf(fmt, ...) printf(fmt, ## __VA_ARGS__)
    #define antsw_rcprintf(rx_chan, fmt, ...) rcprintf(rx_chan, fmt, ## __VA_ARGS__)
#else
    #define antsw_printf(fmt, ...)
    #define antsw_rcprintf(rx_chan, fmt, ...)
#endif

//#define ANT_SWITCH_DEBUG_MSG	true
#define ANT_SWITCH_DEBUG_MSG	false

antsw_t antsw;
static bool using_default;
static const int poll_msec = 100;

#define ANTSW_SHMEM_STATUS shmem->status_u4[N_SHMEM_STATUS_ANT_SW][0]
#define ANTSW_SHMEM_ANTS   &shmem->status_str_small[0]

void ant_switch_task_start(const char *cmd)
{
    if (ANTSW_SHMEM_STATUS == SHMEM_STATUS_IDLE) {
        kiwi_strncpy(ANTSW_SHMEM_ANTS, cmd, N_ANT);
        ANTSW_SHMEM_STATUS = SHMEM_STATUS_START;
        TaskWakeupF(antsw.task_tid, TWF_CANCEL_DEADLINE);
    } else {
        antsw_printf("ant_switch_task_start EXPECTED-IDLE: ANTSW_SHMEM_STATUS=%s cmd=<%s>\n",
            shmem_status_s[ANTSW_SHMEM_STATUS], cmd);
    }
}

int ant_switch_setantenna(char *antenna) {      // "1" .. "10", "g"
    antsw_printf("ant_switch_setantenna: START %s\n", antenna);
    //non_blocking_cmd_system_child("ant_switch", antenna);
    ant_switch_task_start(antenna);
    antsw_printf("ant_switch_setantenna DONE: %s\n", antenna);
	return 0;
}

int ant_switch_toggleantenna(char *antenna) {   // "t1" .. "t10", "tg"
    char *cmd;
    asprintf(&cmd, "t%s", antenna);
    antsw_printf("ant_switch_toggleantenna: %s\n", cmd);
    //non_blocking_cmd_system_child("ant_switch", cmd);
    ant_switch_task_start(cmd);
    antsw_printf("ant_switch_toggleantenna DONE: %s\n", cmd);
    kiwi_asfree(cmd);
	return 0;
}

int ant_switch_validate_cmd(char *cmd) {
    char c = cmd[0];
    return ((c >= '1' && c <= '9') || c == 'g');
}

bool ant_switch_read_denyswitching(int rx_chan) {
    bool deny = false;
    bool error;
    int allow = cfg_int("ant_switch.denyswitching", &error, CFG_OPTIONAL);
    
    // values used by admin menu
    #define ALLOW_EVERYONE 0
    #define ALLOW_LOCAL_ONLY 1
    #define ALLOW_LOCAL_OR_PASSWORD_ONLY 2
    
    if (error) allow = ALLOW_EVERYONE;
    ext_auth_e auth = ext_auth(rx_chan);    // AUTH_USER, AUTH_LOCAL, AUTH_PASSWORD
    if (allow == ALLOW_LOCAL_ONLY && auth != AUTH_LOCAL) deny = true;
    if (allow == ALLOW_LOCAL_OR_PASSWORD_ONLY && auth == AUTH_USER) deny = true;
    //antsw_rcprintf(rx_chan, "ant_switch: allow=%d auth=%d => deny=%d\n", allow, auth, deny);
    
    return (deny)? true : false;
}

bool ant_switch_read_denymixing() {
    return cfg_true("ant_switch.denymixing");
}

bool ant_switch_read_denymultiuser(int rx_chan) {
    bool deny_multi = cfg_true("ant_switch.denymultiuser");
    if (ext_auth(rx_chan) == AUTH_LOCAL) deny_multi = false;    // don't apply to local connections
    bool result = (deny_multi && kiwi.current_nusers > 1);
    //antsw_rcprintf(rx_chan, "ant_switch_read_denymultiuser deny_multi=%d current_nusers=%d result=%d\n",
    //    deny_multi, kiwi.current_nusers, result);
    return result? true : false;
}

void ant_switch_check_isConfigured()
{
	int i;
	
	if (!cfg_true("ant_switch.enable")) return;
	if (antsw.backend_s && antsw.backend_s[0] != '\0' && strcmp(antsw.backend_s, "No") != 0) {
        for (i = 1; i <= antsw.n_ch && !antsw.isConfigured; i++) {
            char *ant;
            asprintf(&ant, "%d", i);
            const char *desc = cfg_string(stprintf("ant_switch.ant%sdesc", ant), NULL, CFG_OPTIONAL);
            if (desc != NULL && *desc != '\0') {
                antsw_printf("ant_switch is configured\n");
                antsw.isConfigured = true;
            }
            cfg_string_free(desc);
            kiwi_asfree(ant);
        }
    }
}

bool ant_switch_check_deny(int rx_chan)
{
    #define DENY_NONE 0
    #define DENY_SWITCHING 1
    #define DENY_MULTIUSER 2
    
    int deny_reason = DENY_NONE;
    if (ant_switch_read_denyswitching(rx_chan) == true) deny_reason = DENY_SWITCHING;
    else
    if (ant_switch_read_denymultiuser(rx_chan) == true) deny_reason = DENY_MULTIUSER;
    snd_send_msg(rx_chan, ANT_SWITCH_DEBUG_MSG, "MSG antsw_AntennaDenySwitching=%d", deny_reason);
    return (deny_reason != DENY_NONE);
}

void ant_switch_select_default_antenna()
{
	int i;

	if (!cfg_true("ant_switch.enable")) return;
	bool thunderstorm = cfg_true("ant_switch.thunderstorm");
	
	for (i = 1; i <= antsw.n_ch && !thunderstorm; i++) {
	    char *ant;
	    asprintf(&ant, "%d", i);
	    bool isDefault = cfg_true(stprintf("ant_switch.ant%sdefault", ant));
	    if (isDefault) {
	        printf("ant_switch select_default_antenna <%s>\n", ant);
	        ant_switch_setantenna(ant);
	        kiwi_asfree(ant);
            antsw.thunderstorm_mode = false;
            using_default = true;
	        break;
	    }
	    kiwi_asfree(ant);
	}

    // if no antennas marked as default then tell switch to ground all (if switch supports)
	if (!using_default || (thunderstorm && !antsw.thunderstorm_mode)) {
        printf("ant_switch select_default_antenna <g> Tcfg=%d\n", thunderstorm);
        ant_switch_setantenna((char *) "g");
        if (thunderstorm) {
            antsw.thunderstorm_mode = true;
            using_default = false;
        } else {
            antsw.thunderstorm_mode = false;
            using_default = true;
        }
	}
}

void ant_switch_ReportAntenna(conn_t *conn)
{
    int rx_chan = conn? conn->rx_channel : -1;
    char *selected_antennas = ANTSW_SHMEM_ANTS;

    if (cfg_true("ant_switch.thunderstorm")) {
        // admin has switched on thunderstorm mode
        if (conn)
            send_msg(conn, ANT_SWITCH_DEBUG_MSG, "MSG antsw_Thunderstorm=1");

        // also ground antenna if not grounded (do only once per transition)
        if (!antsw.thunderstorm_mode) {
            antsw.thunderstorm_mode = true;
            antsw_printf("ant_switch rx%d PRE TSTORM-ON prev_selected_antennas=%s\n", rx_chan, selected_antennas);
            kiwi_strncpy(antsw.last_ant, selected_antennas, N_ANT);
            ant_switch_setantenna((char *) "g");
            antsw_printf("ant_switch rx%d POST TSTORM-ON cur_selected_antennas=%s\n", rx_chan, selected_antennas);
            NextTask("ant_switch_ReportAntenna tstorm");
        }
    } else {
        if (antsw.thunderstorm_mode) {
            antsw.thunderstorm_mode = false;
            ant_switch_setantenna(antsw.last_ant);
        }
        if (conn) {
            send_msg(conn, ANT_SWITCH_DEBUG_MSG, "MSG antsw_Thunderstorm=0");
            antsw_rcprintf(rx_chan, "ant_switch rx%d antsw_AntennasAre=%s\n", rx_chan, selected_antennas);
            send_msg(conn, ANT_SWITCH_DEBUG_MSG, "MSG antsw_AntennasAre=%s", selected_antennas);
        }
    }

    // setup user notification of antenna change
    static char last_selected_antennas[N_ANT];
    if (strcmp(selected_antennas, last_selected_antennas) != 0) {
        char *s;
        if (strcmp(selected_antennas, "g") == 0)
            s = (char *) "All antennas now grounded.";
        else
            s = stprintf("Selected antennas are now: %s", selected_antennas);
        static u4_t seq;
        antsw_printf("ant_switch ext_notify_connected notify_rx_chan=%d seq=%d %s\n", antsw.notify_rx_chan, seq, s);
        ext_notify_connected(antsw.notify_rx_chan, seq++, s);
        kiwi_strncpy(last_selected_antennas, selected_antennas, N_ANT);
        NextTask("ant_switch_ReportAntenna ant chg");
    }

    if (conn) {
        ant_switch_check_deny(rx_chan);
        int deny_mixing = ant_switch_read_denymixing()? 1:0;
        send_msg(conn, ANT_SWITCH_DEBUG_MSG, "MSG antsw_AntennaDenyMixing=%d", deny_mixing);
    }
}

// called every 10 second
void ant_switch_poll()
{
    ant_switch_check_isConfigured();
    
    bool no_users = (kiwi.current_nusers == 0);
    bool enable = cfg_true("ant_switch.enable");
    bool thunderstorm = cfg_true("ant_switch.thunderstorm");

    antsw_printf("ant_switch_poll using_default=%d enable=%d Tcfg=%d Tmode=%d current_nusers=%d %s\n",
        using_default, enable, thunderstorm, antsw.thunderstorm_mode, kiwi.current_nusers, no_users? "NO USERS" : "");
    if (using_default || (thunderstorm && antsw.thunderstorm_mode) || !enable) return;

    if (no_users && cfg_true("ant_switch.default_when_no_users")) {
        antsw_printf("ant_switch_poll SET DEFAULT ANTENNA\n");
        ant_switch_select_default_antenna();
    }
}

static int _GetAntenna_shmem_func(void *param)
{
	nbcmd_args_t *args = (nbcmd_args_t *) param;
	//args->func_param;
	char *reply = args->kstr;
    if (reply == NULL) return 0;
	char *sp = kstr_sp_less_trailing_nl(reply);
    char *s = NULL;
    antsw_printf("ant_switch _GetAntenna_shmem_func sp=<%s>\n", sp);
    int n = sscanf(sp, "Selected antennas: %15ms", &s);
    if (n == 1) {
        kiwi_strncpy(ANTSW_SHMEM_ANTS, s, N_ANT);
        ANTSW_SHMEM_STATUS = SHMEM_STATUS_DONE;
    } else {
        ANTSW_SHMEM_STATUS = SHMEM_STATUS_ERROR;
    }
    kiwi_asfree(s);
    antsw_printf("ant_switch _GetAntenna_shmem_func status=%s reply=<%s>\n",
        shmem_status_s[ANTSW_SHMEM_STATUS], sp);
    // kstr_free(args->kstr) done by caller after return
    return 0;
}

void ant_switch_notify_users()
{
    for (int rx_chan = 0; rx_chan < rx_chans; rx_chan++) {
        NextTask("antsw_notify_users");
        conn_t *c = rx_channels[rx_chan].conn;
        if (!c || !c->valid || (c->type != STREAM_SOUND && c->type != STREAM_WATERFALL) || c->internal_connection)
            continue;
        ant_switch_ReportAntenna(c);
    }
    
    // Admin selected thunderstorm mode, but no users connected,
    // so ant_switch_ReportAntenna() above not called to handle it.
    if (cfg_true("ant_switch.thunderstorm") && !antsw.thunderstorm_mode) {
        ant_switch_ReportAntenna(NULL);
        using_default = false;
    }
}

void ant_sw(void *param)    // task
{
    while (1) {
        bool regular_wakeup = (bool) FROM_VOID_PARAM(TaskSleepSec(1));

        u4_t status = ANTSW_SHMEM_STATUS;
        //antsw_printf("ant_switch ant_sw TASK WAKEUP regular_wakeup=%d status=%s\n", regular_wakeup, shmem_status_s[status]);

        if (status == SHMEM_STATUS_START) {
            ANTSW_SHMEM_STATUS = SHMEM_STATUS_BUSY;
            char *cmd;
            asprintf(&cmd, "%s %s", FRONTEND, ANTSW_SHMEM_ANTS);
            antsw_printf("ant_switch TASK BUSY cmd=<%s>\n", cmd);
            non_blocking_cmd_func_forall("ant_switch", cmd, _GetAntenna_shmem_func, NO_WAIT);
            // doesn't block, but _GetAntenna_shmem_func() called when all cmd output available
            kiwi_asfree(cmd);
        } else
        if (status == SHMEM_STATUS_DONE || status == SHMEM_STATUS_ERROR) {
            ant_switch_notify_users();
            antsw_printf("ant_switch TASK DONE status=%s result=<%s>\n",
                shmem_status_s[status], ANTSW_SHMEM_ANTS);
            ANTSW_SHMEM_STATUS = SHMEM_STATUS_IDLE;
        }
    }
}

// called from rx_common_cmd():CMD_ANT_SWITCH
bool ant_switch_msgs(char *msg, int rx_chan)
{
	int n = 0;

	antsw_rcprintf(rx_chan, "ant_switch_msgs rx=%d <%s>\n", rx_chan, msg);
	
    if (strcmp(msg, "SET antsw_GetAntenna") == 0) {
        ant_switch_task_start("s");
        return true;
    }

	char *antenna;
    n = sscanf(msg, "SET antsw_SetAntenna=%15ms", &antenna);
    if (n == 1) {
        //antsw_rcprintf(rx_chan, "ant_switch: %s\n", msg);
        antsw.notify_rx_chan = rx_chan;     // notifier is current rx_chan
        if (!ant_switch_check_deny(rx_chan)) {      // prevent circumvention from client side
            if (ant_switch_validate_cmd(antenna)) {
                if (ant_switch_read_denymixing()) {
                    ant_switch_setantenna(antenna);
                } else {
                    ant_switch_toggleantenna(antenna);
                }
                using_default = false;
            } else {
                antsw_rcprintf(rx_chan, "ant_switch: Command not valid SET Antenna=%s\n", antenna);   
            }
        }
        kiwi_asfree(antenna);
        return true;
    }

    int freq_offset_ant;
    n = sscanf(msg, "SET antsw_freq_offset=%d", &freq_offset_ant);
    if (n == 1) {
        //antsw_rcprintf(rx_chan, "ant_switch: freq_offset %d\n", freq_offset_ant);
        if (!ant_switch_check_deny(rx_chan)) {      // prevent circumvention from client side
            cfg_set_float_save("freq_offset", (double) freq_offset_ant);
            freq_offset = freq_offset_ant;
        }
        return true;
    }
    
    int high_side_ant;
    n = sscanf(msg, "SET antsw_high_side=%d", &high_side_ant);
    if (n == 1) {
        //antsw_rcprintf(rx_chan, "ant_switch: high_side %d\n", high_side_ant);
        if (!ant_switch_check_deny(rx_chan)) {      // prevent circumvention from client side
            // if antenna switch extension is active override current inversion setting
            // and lockout the admin config page setting until a restart
            kiwi.spectral_inversion_lockout = true;
            kiwi.spectral_inversion = high_side_ant? true:false;
        }
        return true;
    }

    if (strcmp(msg, "SET antsw_init") == 0) {
        snd_send_msg(rx_chan, ANT_SWITCH_DEBUG_MSG, "MSG antsw_backend_ver=%d.%d",
            antsw.ver_maj, antsw.ver_min);                 
        snd_send_msg(rx_chan, ANT_SWITCH_DEBUG_MSG, "MSG antsw_channels=%d", antsw.n_ch);                 
        return true;
    }
        
    return false;
}

void ant_switch_get_backend_info()
{
	char *reply = non_blocking_cmd(FRONTEND " bi", NULL, poll_msec);
	if (reply) {
        kiwi_asfree_set_null(antsw.backend_s);
        kiwi_asfree_set_null(antsw.mix);
        kiwi_asfree_set_null(antsw.ip_or_url);
        char *sp = kstr_sp(reply);
        int n = sscanf(sp, "%63ms v%d.%d %dch %7ms %63ms",
            &antsw.backend_s, &antsw.ver_maj, &antsw.ver_min, &antsw.n_ch, &antsw.mix, &antsw.ip_or_url);
        if (n != 6) {
            snd_send_msg(SM_ADMIN_ALL, ANT_SWITCH_DEBUG_MSG, "ADM antsw_backend_err");                 
            antsw.backend_ok = false;
        } else {
            printf("ant_switch GET backend info: n=%d %s version=%d.%d channels=%d mix=%s ip_url=%s\n",
                n, antsw.backend_s, antsw.ver_maj, antsw.ver_min, antsw.n_ch, antsw.mix, antsw.ip_or_url);
            antsw.backend_ok = true;
        }
    }
	kstr_free(reply);
}

void ant_switch_init()
{
	// for benefit of Beagle GPIO backend
	GPIO_OUTPUT(P811); GPIO_WRITE_BIT(P811, 0);
	GPIO_OUTPUT(P812); GPIO_WRITE_BIT(P812, 0);
	GPIO_OUTPUT(P813); GPIO_WRITE_BIT(P813, 0);
	GPIO_OUTPUT(P814); GPIO_WRITE_BIT(P814, 0);
	GPIO_OUTPUT(P815); GPIO_WRITE_BIT(P815, 0);
	GPIO_OUTPUT(P816); GPIO_WRITE_BIT(P816, 0);
	GPIO_OUTPUT(P817); GPIO_WRITE_BIT(P817, 0);
	GPIO_OUTPUT(P818); GPIO_WRITE_BIT(P818, 0);
	GPIO_OUTPUT(P819); GPIO_WRITE_BIT(P819, 0);
	GPIO_OUTPUT(P826); GPIO_WRITE_BIT(P826, 0);
	
	#ifdef ANTSW_PRF
        printf_highlight(0, "ant_switch");
    #endif

	// migrate from prior ant switch extension backend selection
	char path[256];
	int n = readlink(OLD_BACKEND, path, sizeof(path));
	if (n > 0) {
	    path[n] = '\0';
	    char *s = strstr(path, "ant-switch-backend-");
	    if (s) {
	        s += strlen("ant-switch-backend-");
	        cfg_set_string_save("ant_switch.backend", s);
	        system("rm -f " BACKEND_FILE);
	        symlink(stprintf(BACKEND_PREFIX "%s", s), BACKEND_FILE);
	        unlink(OLD_BACKEND);
	        printf("ant_switch: MIGRATE backend %s\n", s);
	    }
	}

    ant_switch_get_backend_info();
    ant_switch_check_isConfigured();
	ant_switch_select_default_antenna();
	antsw.task_tid = CreateTask(ant_sw, 0, SERVICES_PRIORITY);
}


////////////////////////////////
// admin interface
////////////////////////////////

// called from c2s_admin()
bool ant_switch_admin_msgs(conn_t *conn, char *cmd)
{
    int n;
    antsw_printf("ant_switch_admin_msgs: <%s>\n", cmd);

    if (strcmp(cmd, "ADM antsw_GetBackends") == 0) {
        char *reply = non_blocking_cmd(FRONTEND " be", NULL, poll_msec);
        char *sp = kstr_sp(reply);
        char *nl = strchr(sp, '\n');
        if (nl) *nl = '\0';
        send_msg_encoded(conn, "ADM", "antsw_backends", "%s", sp);
        kstr_free(reply);
        return true;
    }

    if (strcmp(cmd, "ADM antsw_notify_users") == 0) {
        antsw_printf("ant_switch ADM antsw_notify_users\n");
        cfg_cfg.update_seq++;   // cause cfg to be reloaded by all active user connections
        ant_switch_notify_users();
        return true;
    }
    
    if (strcmp(cmd, "ADM antsw_GetInfo") == 0) {
        printf("ant_switch ADM antsw_GetInfo antsw.backend_s=%s antsw.backend_ok=%d\n", antsw.backend_s, antsw.backend_ok);
        if (antsw.backend_ok) {
            send_msg(conn, SM_NO_DEBUG, "ADM antsw_backend=%s antsw_ver=%d.%d antsw_nch=%d antsw_mix=%s antsw_ip_or_url=%s",
                antsw.backend_s, antsw.ver_maj, antsw.ver_min, antsw.n_ch, antsw.mix, antsw.ip_or_url);
        }
        return true;
    }

	char *antenna;
    n = sscanf(cmd, "ADM antsw_SetBackend=%63ms", &antenna);
    if (n == 1) {
        char *cmd, *reply;
        asprintf(&cmd, FRONTEND " bs %s", antenna);
        kiwi_asfree(antenna);
        reply = non_blocking_cmd(cmd, NULL, poll_msec);
        printf("ant_switch ADM antsw_SetBackend: <%s>\n", kstr_sp(reply));
        kstr_free(reply);
        kiwi_asfree(cmd);
        ant_switch_get_backend_info();
        return true;
    }
    
	char *ip_or_url;
    n = sscanf(cmd, "ADM antsw_SetIP_or_URL=%63ms", &ip_or_url);
    if (n == 1) {
        char *cmd, *reply;
        asprintf(&cmd, FRONTEND " sa %s", ip_or_url);
        kiwi_asfree(ip_or_url);
        reply = non_blocking_cmd(cmd, NULL, poll_msec);
        printf("ant_switch ADM antsw_SetIP_or_URL: <%s>\n", kstr_sp(reply));
        kstr_free(reply);
        kiwi_asfree(cmd);
        ant_switch_get_backend_info();
        return true;
    }
    
    return false;
}

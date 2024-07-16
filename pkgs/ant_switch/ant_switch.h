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

#pragma once

#include "shmem.h"

/*
    Interfacing since extension mechanism is not used:
    
    user:
    
        ext.cpp: extint_setup() => ant_switch_init()
    
        rx/rx_cmd.{h,cpp}: case CMD_ANT_SWITCH => ant_switch_msgs()
    
        kiwi.js:
            [ant_switch.cpp:snd_send_msg(rx_chan, "MSG antsw_*")] => 
                kiwi_msg(): default: param[0].startsWith('antsw_') => ant_switch_msg()
        
            cfg_save_json() path == 'cfg.ant_switch.denyswitching' => ant_sw_first_set_ignored
    
        ext.js:
            extint_select() => ant_switch_view()
            extint_list_json() => extint.ext_names.push('ant_switch')
    
        openwebrx.js: panels_setup() => ant_switch_user_init()
    
    admin:
    
        admin.js:admin_recv() startsWith('antsw_') => ant_switch_admin_msg()
    
        web.cpp: gen_list_js[] = "kiwi/ant_switch.js"
    
        ui/admin.cpp:c2s_admin()
            "ADM antsw_GetInfo" => send_msg("ADM antsw_ver=%d.%d antsw_nch=%d")
            "ADM antsw_GetBackends" => send_msg("ADM antsw_backends=%s")
    
        rx/rx_server_ajax.cpp:AJAX_STATUS => ant_switch_configured()
*/

#define FRONTEND "/root/Beagle_SDR_GPS/pkgs/ant_switch/ant-switch-frontend"
#define BACKEND_FILE "/root/kiwi.config/ant-switch-backend"
#define OLD_BACKEND "/root/extensions/ant_switch/backends/ant-switch-backend"
#define BACKEND_PREFIX "/root/Beagle_SDR_GPS/pkgs/ant_switch/backends/ant-switch-backend-"

typedef struct {
    char *backend_s;
    bool backend_ok;
    int ver_maj, ver_min;
	int n_ch;
	char *mix;
	char *ip_or_url;
	bool isConfigured;
	tid_t task_tid;
	int notify_rx_chan;

	bool thunderstorm_mode;
	#define N_ANT N_SHMEM_STATUS_STR_SMALL
	char last_ant[N_ANT];
} antsw_t;

extern antsw_t antsw;

void ant_switch_init();
bool ant_switch_msgs(char *msg, int rx_chan);
bool ant_switch_admin_msgs(conn_t *conn, char *cmd);
void ant_switch_poll();

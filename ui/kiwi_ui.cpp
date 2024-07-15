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

// Copyright (c) 2023 John Seamons, ZL4VO/KF6VO

#include "types.h"
#include "config.h"
#include "kiwi.h"
#include "rx.h"
#include "rx_util.h"
#include "str.h"
#include "mem.h"
#include "misc.h"
#include "coroutines.h"
#include "debug.h"
#include "printf.h"

#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <sched.h>
#include <stdlib.h>

bool backup_in_progress;

#define SD_CMD_DIR "cd /root/" REPO_NAME "/tools; "
#define SD_CMD_OLD SD_CMD_DIR "./kiwiSDR-make-microSD-flasher-from-eMMC.sh"
#define SD_CMD_NEW SD_CMD_DIR "cp /etc/beagle-flasher/%s-emmc-to-microsd /etc/default/beagle-flasher; ./%s-flasher.sh"

void sd_backup(conn_t *conn, bool from_admin)
{
	char *sb, *sb2;
	char *cmd_p, *buf_m;
	
	#if 0
        if (!kiwi.dbgUs && (debian_maj == 11 || (debian_maj == 12 && debian_min < 4))) {
            send_msg(conn, SM_NO_DEBUG, "%s microSD_done=87", from_admin? "ADM":"MFG");
            return;
        }
    #endif

    // On BBAI-64, backup script only supports dual-partition setups (i.e. /boot/firmware on p1)
    if (kiwi.platform == PLATFORM_BB_AI64 && (
        !kiwi_file_exists("/boot/firmware") ||
        !kiwi_file_exists("/boot/firmware/extlinux") ||
        !kiwi_file_exists("/boot/firmware/extlinux/extlinux.conf")
        )) {
        send_msg(conn, SM_NO_DEBUG, "%s microSD_done=88", from_admin? "ADM":"MFG");
        return;
    }

    backup_in_progress = true;  // NB: must be before rx_server_kick() to prevent new connections
    rx_server_kick(KICK_ALL);      // kick everything (including autorun) off to speed up copy
    // if this delay isn't here the subsequent non_blocking_cmd_popen() hangs for
    // MINUTES, if there is a user connection open, for reasons we do not understand
    TaskSleepReasonSec("kick delay", 5);
    
    // clear user list on status tab
    sb = rx_users(IS_ADMIN);
    send_msg(conn, false, "MSG user_cb=%s", kstr_sp(sb));
    kstr_free(sb);
    
    #define NBUF 256
    char *buf = (char *) kiwi_malloc("sd_backup", NBUF);
    int i, n, err;
    
    sd_copy_in_progress = true;
    non_blocking_cmd_t p;
    const char *platform = platform_s[kiwi.platform];
    asprintf((char **) &p.cmd, (debian_ver >= 11)? SD_CMD_NEW : SD_CMD_OLD, platform, platform);
    //real_printf("microSD_write: kiwi.platform=%d <%s>\n", kiwi.platform, p.cmd);
    //real_printf("microSD_write: non_blocking_cmd_popen..\n");
    non_blocking_cmd_popen(&p);
    //real_printf("microSD_write: ..non_blocking_cmd_popen\n");
    for (i = n = 0; n >= 0; i++) {
        n = non_blocking_cmd_read(&p, buf, NBUF);
        //real_printf("microSD_write: n=%d\n", n);
        if (n > 0) {
            //real_printf("microSD_write: mprintf %d %d <%s>\n", n, strlen(buf), buf);
            mprintf("%s", buf);
        }
        TaskSleepMsec(250);
        u4_t now = timer_sec();
        if ((now - conn->keepalive_time) > 5) {
            send_msg(conn, false, "MSG keepalive");
            conn->keepalive_time = now;
        }
        if (conn->kick) {
            system("pkill rsync");
            if (debian_ver >= 11) {
                system("pkill beagle-flasher");
            } else {
                system("ps laxww|grep kiwiSDR-make-microSD-flasher-from-eMMC|awk '{print $3}'|xargs -n 1 kill");
            }
            //real_printf("microSD_write: KICKED\n");
            break;
        }
    }
    err = non_blocking_cmd_pclose(&p);
    free((char *) p.cmd);
    //real_printf("microSD_write: err=%d\n", err);
    sd_copy_in_progress = false;
    
    err = (err < 0)? err : WEXITSTATUS(err);
    mprintf("sd_backup: system returned %d\n", err);
    kiwi_free("sd_backup", buf);
    #undef NBUF
    //real_printf("microSD_write: microSD_done=%d\n", err);
    send_msg(conn, SM_NO_DEBUG, "%s microSD_done=%d", from_admin? "ADM":"MFG", err);
    backup_in_progress = false;
    if (from_admin) rx_autorun_restart_victims(true);
}

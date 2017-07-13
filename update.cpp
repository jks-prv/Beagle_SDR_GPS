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

// Copyright (c) 2016 John Seamons, ZL/KF6VO

#include "types.h"
#include "config.h"
#include "kiwi.h"
#include "misc.h"
#include "str.h"
#include "timer.h"
#include "web.h"
#include "cfg.h"
#include "coroutines.h"

#include <types.h>
#include <unistd.h>

bool update_pending = false, update_task_running = false, update_in_progress = false;
int pending_maj = -1, pending_min = -1;

static void update_build_ctask(void *param)
{
	bool build_normal = true;
	
    //#define BUILD_SHORT_MF
    //#define BUILD_SHORT
    #if defined(BUILD_SHORT_MF) || defined(BUILD_SHORT)
        bool force_build = (bool) FROM_VOID_PARAM(param);
        if (force_build) {
            #if defined(BUILD_SHORT_MF)
                system("cd /root/" REPO_NAME "; mv Makefile.1 Makefile; rm -f obj/p*.o obj/r*.o obj/f*.o; make");
                build_normal = false;
            #elif defined(BUILD_SHORT)
                system("cd /root/" REPO_NAME "; rm -f obj_O3/p*.o obj_O3/r*.o obj_O3/f*.o; make");
                build_normal = false;
            #endif
        }
    #endif

	if (build_normal) {
		int status = system("cd /root/" REPO_NAME "; make git");
		if (status < 0 || WEXITSTATUS(status) != 0) {
			exit(-1);
		}
		status = system("cd /root/" REPO_NAME "; make clean_dist; make; make install");
	}
	
	exit(0);
}

static void wget_makefile_ctask(void *param)
{
	int status = system("cd /root/" REPO_NAME "; wget --timeout=3 --tries=3 --no-check-certificate https://raw.githubusercontent.com/jks-prv/Beagle_SDR_GPS/master/Makefile -O Makefile.1");

	if (status < 0 || WEXITSTATUS(status) != 0) {
		exit(-1);
	}

	exit(0);
}

static void report_result(conn_t *conn)
{
	// let admin interface know result
	char *date_m = kiwi_str_encode((char *) __DATE__);
	char *time_m = kiwi_str_encode((char *) __TIME__);
	char *sb;
	asprintf(&sb, "{\"p\":%d,\"i\":%d,\"r\":%d,\"g\":%d,\"v1\":%d,\"v2\":%d,\"p1\":%d,\"p2\":%d,\"d\":\"%s\",\"t\":\"%s\"}",
		update_pending, update_in_progress, RX_CHANS, GPS_CHANS, version_maj, version_min, pending_maj, pending_min, date_m, time_m);
	send_msg(conn, false, "MSG update_cb=%s", sb);
	//printf("UPDATE: %s\n", sb);
	free(date_m);
	free(time_m);
	free(sb);
}

static bool daily_restart = false;

static void update_task(void *param)
{
	conn_t *conn = (conn_t *) FROM_VOID_PARAM(param);
	
	lprintf("UPDATE: checking for updates\n");

	// Run wget in a Linux child process otherwise this thread will block and cause trouble
	// if the check is invoked from the admin page while there are active user connections.
	int status = child_task(SEC_TO_MSEC(1), wget_makefile_ctask, NULL);
	int exited = WIFEXITED(status);
	int exit_status = WEXITSTATUS(status);

	if (status < 0 || exit_status != 0) {
		lprintf("UPDATE: wget Makefile, no Internet access?\n");
		update_pending = update_task_running = update_in_progress = false;
		return;
	}
	
	FILE *fp;
	scallz("fopen Makefile.1", (fp = fopen("/root/" REPO_NAME "/Makefile.1", "r")));
		int n1, n2;
		n1 = fscanf(fp, "VERSION_MAJ = %d\n", &pending_maj);
		n2 = fscanf(fp, "VERSION_MIN = %d\n", &pending_min);
	fclose(fp);
	
	bool ver_changed = (n1 == 1 && n2 == 1 && (pending_maj > version_maj  || (pending_maj == version_maj && pending_min > version_min)));
	bool update_install = (admcfg_bool("update_install", NULL, CFG_REQUIRED) == true);
	bool force_check = (conn && conn->update_check == FORCE_CHECK);
	bool force_build = (conn && conn->update_check == FORCE_BUILD);
	
	if (force_check) {
		if (ver_changed)
			lprintf("UPDATE: version changed (current %d.%d, new %d.%d), but check only\n",
				version_maj, version_min, pending_maj, pending_min);
		else
			lprintf("UPDATE: running most current version\n");
		
		report_result(conn);
		conn->update_check = WAIT_UNTIL_NO_USERS;
		update_pending = update_task_running = update_in_progress = false;
		return;
	} else

	if (ver_changed && !update_install && !force_build) {
		lprintf("UPDATE: version changed (current %d.%d, new %d.%d), but update install not enabled\n",
			version_maj, version_min, pending_maj, pending_min);
	} else
	
	if (ver_changed || force_build) {
		lprintf("UPDATE: version changed%s, current %d.%d, new %d.%d\n",
			force_build? " (forced)":"",
			version_maj, version_min, pending_maj, pending_min);
		lprintf("UPDATE: building new version..\n");
		update_in_progress = true;
        rx_server_user_kick(-1);        // kick everyone off to speed up build

		// Run build in a Linux child process so the server can continue to respond to connection requests
		// and display a "software update in progress" message.
		// This is because the calls to system() in update_build_ctask() block for the duration of the build.
		status = child_task(SEC_TO_MSEC(1), update_build_ctask, TO_VOID_PARAM(force_build));
		int exited = WIFEXITED(status);
		int exit_status = WEXITSTATUS(status);
		
		if (! (exited && exit_status == 0)) {
			if (exited) {
				lprintf("UPDATE: git pull, no Internet access?\n");
				//lprintf("UPDATE: error in build, exit status %d, aborting\n", exit_status);
			} else {
				lprintf("UPDATE: error in build, non-normal exit, aborting\n");
			}
			update_pending = update_in_progress = false;
			return;
		}
		
		lprintf("UPDATE: switching to new version %d.%d\n", pending_maj, pending_min);
		xit(0);
	} else {
		lprintf("UPDATE: version %d.%d is current\n", version_maj, version_min);
	}
	
	if (daily_restart) {
	    lprintf("UPDATE: daily restart..\n");
	    xit(0);
	}
	
	if (conn) conn->update_check = WAIT_UNTIL_NO_USERS;
	update_pending = update_task_running = update_in_progress = false;
}

// called at update check TOD, on each user logout in case update is pending or on demand by admin UI
void check_for_update(update_check_e type, conn_t *conn)
{
	bool force = (type != WAIT_UNTIL_NO_USERS);
	
	if (no_net) {
		lprintf("UPDATE: not checked because no-network-mode set\n");
		return;
	}

	if (!force && admcfg_bool("update_check", NULL, CFG_REQUIRED) == false)
		return;
	
	if (force) {
		lprintf("UPDATE: force %s by admin\n", (type == FORCE_CHECK)? "update check" : "build");
		assert(conn != NULL);
		if (update_task_running) {
			lprintf("UPDATE: update task already running\n");
			report_result(conn);
			return;
		} else {
			conn->update_check = type;
		}
	}

	if ((force || (update_pending && rx_server_users() == 0)) && !update_task_running) {
		update_task_running = true;
		CreateTask(update_task, TO_VOID_PARAM(conn), ADMIN_PRIORITY);
	}
}

static bool update_on_startup = true;

// called at the top of each minute
void schedule_update(int hour, int min)
{
	#define UPDATE_SPREAD_HOURS	4	// # hours to spread updates over
	#define UPDATE_SPREAD_MIN	(UPDATE_SPREAD_HOURS * 60)

	#define UPDATE_START_HOUR	2	// 2 AM UTC, 2(3) PM NZT(NZDT)
	#define UPDATE_END_HOUR		(UPDATE_START_HOUR + UPDATE_SPREAD_HOURS)

	bool update = (hour >= UPDATE_START_HOUR && hour < UPDATE_END_HOUR);
	
	// don't let Kiwis hit github.com all at once!
	int mins;
	if (update) {
		mins = min + (hour - UPDATE_START_HOUR) * 60;
		//printf("UPDATE: %02d:%02d waiting for %d min = %d min(sn=%d)\n", hour, min,
		//	mins, serial_number % UPDATE_SPREAD_MIN, serial_number);
		update = update && (mins == (serial_number % UPDATE_SPREAD_MIN));

		daily_restart = (admcfg_bool("daily_restart", NULL, CFG_REQUIRED) == true);
	}
	
	if (update || update_on_startup) {
		lprintf("UPDATE: check scheduled %s\n", update_on_startup? "(startup)":"");
		update_on_startup = false;
		update_pending = true;
		check_for_update(WAIT_UNTIL_NO_USERS, NULL);
	}
}

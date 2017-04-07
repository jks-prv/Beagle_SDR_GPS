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
int force_build = 0;

static void update_build(void *param)
{
	int force_build = (int) (long) param;
	
	if (force_build == 2) {
		system("cd /root/" REPO_NAME "; mv Makefile.1 Makefile; rm -f obj/p*.o obj/r*.o obj/f*.o; make");
	} else
	if (force_build == 3) {
		system("cd /root/" REPO_NAME "; rm -f obj_O3/p*.o obj_O3/r*.o obj_O3/f*.o; make");
	} else {
		int status = system("cd /root/" REPO_NAME "; make git");
		if (status < 0 || WEXITSTATUS(status) != 0) {
			exit(-1);
		}
		status = system("cd /root/" REPO_NAME "; make clean_dist; make; make install");
	}
	exit(0);
}

static void wget_makefile(void *param)
{
	int status = system("cd /root/" REPO_NAME "; wget --no-check-certificate https://raw.githubusercontent.com/jks-prv/Beagle_SDR_GPS/master/Makefile -O Makefile.1");

	if (status < 0 || WEXITSTATUS(status) != 0) {
		exit(-1);
	}
	exit(0);
}

static void report_result(conn_t *conn)
{
	// let admin interface know result
	char *date_m = str_encode((char *) __DATE__);
	char *time_m = str_encode((char *) __TIME__);
	char *sb;
	asprintf(&sb, "{\"p\":%d,\"i\":%d,\"r\":%d,\"g\":%d,\"v1\":%d,\"v2\":%d,\"p1\":%d,\"p2\":%d,\"d\":\"%s\",\"t\":\"%s\"}",
		update_pending, update_in_progress, RX_CHANS, GPS_CHANS, VERSION_MAJ, VERSION_MIN, pending_maj, pending_min, date_m, time_m);
	send_msg(conn, false, "MSG update_cb=%s", sb);
	//printf("UPDATE: %s\n", sb);
	free(date_m);
	free(time_m);
	free(sb);
}

static void update_task(void *param)
{
	conn_t *conn = (conn_t *) param;
	
	lprintf("UPDATE: checking for updates\n");

	// Run wget in a Linux child process otherwise this thread will block and cause trouble
	// if the check is invoked from the admin page while there are active user connections.
	int status = child_task(SEC_TO_MSEC(1), wget_makefile, NULL);
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
	
	bool ver_changed = (n1 == 1 && n2 == 1 && (pending_maj > VERSION_MAJ  || (pending_maj == VERSION_MAJ && pending_min > VERSION_MIN)));
	bool update_install = (admcfg_bool("update_install", NULL, CFG_REQUIRED) == true);
	
	if (conn && conn->update_check_only) {
		if (ver_changed)
			lprintf("UPDATE: version changed (current %d.%d, new %d.%d), but check only\n",
				VERSION_MAJ, VERSION_MIN, pending_maj, pending_min);
		else
			lprintf("UPDATE: running most current version\n");
		
		report_result(conn);
		conn->update_check_only = false;
		update_pending = update_task_running = update_in_progress = false;
		return;
	} else

	if (ver_changed && !update_install && !force_build) {
		lprintf("UPDATE: version changed (current %d.%d, new %d.%d), but update install not enabled\n",
			VERSION_MAJ, VERSION_MIN, pending_maj, pending_min);
	} else
	
	if (ver_changed || force_build) {
		lprintf("UPDATE: version changed%s, current %d.%d, new %d.%d\n",
			force_build? " (forced)":"",
			VERSION_MAJ, VERSION_MIN, pending_maj, pending_min);
		lprintf("UPDATE: building new version..\n");
		update_in_progress = true;

		// Run build in a Linux child process so the server can continue to respond to connection requests
		// and display a "software update in progress" message.
		// This is because the calls to system() in update_build() block for the duration of the build.
		status = child_task(SEC_TO_MSEC(1), update_build, (void *) (long) force_build);
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
		lprintf("UPDATE: version %d.%d is current\n", VERSION_MAJ, VERSION_MIN);
	}
	
	update_pending = update_task_running = update_in_progress = false;
}

// called at update check TOD, on each user logout incase update is pending or on demand by admin UI
void check_for_update(update_check_e type, conn_t *conn)
{
	bool force_check = (type == FORCE_CHECK);
	
	if (no_net) {
		lprintf("UPDATE: not checked because no-network-mode set\n");
		return;
	}

	if (!force_check && admcfg_bool("update_check", NULL, CFG_REQUIRED) == false)
		return;
	
	if (force_check) {
		lprintf("UPDATE: force update check by admin\n");
		assert(conn != NULL);
		if (update_task_running) {
			report_result(conn);
			return;
		} else {
			conn->update_check_only = true;
		}
	}

	if ((force_check || (update_pending && rx_server_users() == 0)) && !update_task_running) {
		update_task_running = true;
		CreateTask(update_task, (void *) conn, ADMIN_PRIORITY);
	}
}

static bool update_on_startup = true;

// called at the top of each minute
void schedule_update(int hour, int min)
{
	bool update = (hour == 2);	// 2 AM UTC, 2(3) PM NZT(NZDT)
	
	// don't all hit github.com at once!
	//if (update) printf("UPDATE: %02d:%02d waiting for %02d min\n", hour, min, (serial_number % 60));
	update = update && (min == (serial_number % 60));
	
	if (update || update_on_startup) {
		lprintf("UPDATE: check scheduled\n");
		update_on_startup = false;
		update_pending = true;
		check_for_update(WAIT_UNTIL_NO_USERS, NULL);
	}
}

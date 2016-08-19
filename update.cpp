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
#include "timer.h"
#include "web.h"
#include "cfg.h"
#include "coroutines.h"

#include <types.h>
#include <unistd.h>

#ifdef HOST
	#include <wait.h>
#endif

bool update_pending = false, update_in_progress = false;
int pending_maj = -1, pending_min = -1;
int force_build = 0;

static void update_task(void *param)
{
	bool check_only = (bool) param;
	
	lprintf("UPDATE: checking for updates\n");
	int status = system("cd /root/" REPO_NAME "; wget --no-check-certificate https://raw.githubusercontent.com/TomGaussiran/Beagle_SDR_GPS/master/Makefile -O Makefile.1");

	if (status < 0 || WEXITSTATUS(status) != 0) {
		lprintf("UPDATE: no Internet access?\n");
		update_pending = update_in_progress = false;
		return;
	}
	
	FILE *fp;
	scallz("fopen Makefile.1", (fp = fopen("/root/" REPO_NAME "/Makefile.1", "r")));
		int n1, n2;
		n1 = fscanf(fp, "VERSION_MAJ = %d\n", &pending_maj);
		n2 = fscanf(fp, "VERSION_MIN = %d\n", &pending_min);
	fclose(fp);
	
	bool ver_changed = (n1 == 1 && n2 == 1 && (pending_maj > VERSION_MAJ  || (pending_maj == VERSION_MAJ && pending_min > VERSION_MIN)));
	bool update_install = (cfg_bool("update_install", NULL, CFG_REQUIRED) == true);
	
	if (check_only && !force_build) {
		if (ver_changed)
			lprintf("UPDATE: version changed (current %d.%d, new %d.%d), but check only\n",
				VERSION_MAJ, VERSION_MIN, pending_maj, pending_min);
		update_pending = update_in_progress = false;
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
		
		pid_t child;
		scall("fork", (child = fork()));
		if (child == 0) {
			if (force_build == 2) {
				system("cd /root/" REPO_NAME "; mv Makefile.1 Makefile; rm -f obj/p*.o obj/r*.o obj/f*.o; make");
			} else
			if (force_build == 3) {
				system("cd /root/" REPO_NAME "; rm -f obj_O3/p*.o obj_O3/r*.o obj_O3/f*.o; make");
			} else {
				system("cd /root/" REPO_NAME "; make git; make clean_dist; make; make install");
			}
			exit(0);
		}
		
		// Run build in a Linux child process so we can respond to connection attempts
		// and display a "software update in progress" message.
		int status;
		do {
			TaskSleep(5000000);
			scall("wait", waitpid(child, &status, WNOHANG));
		} while (!WIFEXITED(status));
		
		lprintf("UPDATE: switching to new version %d.%d\n", pending_maj, pending_min);
		xit(0);
	} else {
		lprintf("UPDATE: version %d.%d is current\n", VERSION_MAJ, VERSION_MIN);
	}
	
	update_pending = update_in_progress = false;
}

// called on each user logout or on demand by UI
void check_for_update(int force_check)
{
	if (no_net) {
		lprintf("UPDATE: not checked because no-network-mode set\n");
		return;
	}

	if (!force_check && cfg_bool("update_check", NULL, CFG_REQUIRED) == false)
		return;
	
	if (force_check)
		lprintf("UPDATE: force update check by admin\n");

	if ((force_check || (update_pending && rx_server_users() == 0)) && !update_in_progress) {
		update_in_progress = true;
		CreateTask(update_task, (void *) (long) force_check, ADMIN_PRIORITY);
	}
}

static bool update_on_startup = true;

// called at the top of each minute
void schedule_update(int hour, int min)
{
	bool update;
	
	if (VERSION_MAJ < 1) {
		update = (hour == 0 || hour == 4);	// more frequently during beta development: noon, 4 PM NZT
	} else {
		update = (hour == 2);	// 2 AM UTC / 2 PM NZT
	}
	
	// don't all hit github at once!
	//if (update) printf("UPDATE: %02d:%02d waiting for %02d min\n", hour, min, (serial_number % 60));
	update = update && (min == (serial_number % 60));
	
	if (update || update_on_startup) {
		lprintf("UPDATE: scheduled\n");
		update_on_startup = false;
		update_pending = true;
		check_for_update(WAIT_UNTIL_NO_USERS);
	}
}

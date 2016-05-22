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
int force_build = 0;

static void update_task()
{
	lprintf("UPDATE: updating sources\n");
	system("cd /root/" REPO_NAME "; wget --no-check-certificate https://raw.githubusercontent.com/jks-prv/Beagle_SDR_GPS/master/Makefile -O Makefile");
	
	FILE *fp;
	scallz("fopen Makefile", (fp = fopen("/root/" REPO_NAME "/Makefile", "r")));
	int n, maj, min;
	n = fscanf(fp, "VERSION_MAJ = %d\n", &maj);
	check(n == 1);
	n = fscanf(fp, "VERSION_MIN = %d\n", &min);
	check(n == 1);
	fclose(fp);
	
	if (VERSION_MAJ != maj || VERSION_MIN != min || force_build) {
		lprintf("UPDATE: version changed%s, current %d.%d, new %d.%d\n",
			force_build? " (forced)":"",
			VERSION_MAJ, VERSION_MIN, maj, min);
		lprintf("UPDATE: building new version..\n");
		
		pid_t child;
		scall("fork", (child = fork()));
		if (child == 0) {
			system("cd /root/" REPO_NAME "; make git");
			system("cd /root/" REPO_NAME "; make; make install");
			exit(0);
		}
		
		// Run build in a Linux child process so we can respond to connection attempts
		// and display a "software update in progress" message.
		int status;
		do {
			TaskSleep(5000000);
			scall("wait", waitpid(child, &status, WNOHANG));
		} while (!WIFEXITED(status));
		
		lprintf("UPDATE: switching to new version %d.%d\n", maj, min);
		xit(0);
	} else {
		lprintf("UPDATE: version %d.%d is current\n", VERSION_MAJ, VERSION_MIN);
	}
	
	update_pending = update_in_progress = false;
}

void check_for_update()
{
	if (update_pending && !update_in_progress && rx_server_users() == 0) {
		update_in_progress = true;
		CreateTask(update_task, ADMIN_PRIORITY);
	}
}

static bool update_on_startup = true;

// called at the top of each hour
void schedule_update(int hour)
{
	bool update;
	if (VERSION_MAJ < 1) {
		update = (hour == 0 || hour == 4);	// more frequently during beta development: noon, 4 PM NZT
	} else {
		update = (hour == 2);	// 2 AM UTC
	}
	
	if (update || update_on_startup) {	// 2 AM UTC
		update_on_startup = false;
		lprintf("UPDATE: scheduled\n");
		update_pending = true;
		check_for_update();
	}
}

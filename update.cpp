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

bool update_pending = false, update_in_progress = false;
int force_build = 0;

static void update_task()
{
	//TaskSleep(5000000);
	lprintf("UPDATE: updating sources\n");
	//system("rsync -av --delete kiwi_rsync@" UPDATE_HOST ":~/kiwi ../");
	//system("rsync -av --delete kiwi_rsync@" "192.168.1.100" ":~/kiwi ../");
	system("cd /root/" REPO_NAME "; make git");
	
	FILE *fp;
	scallz("fopen Makefile", (fp = fopen("/root/" REPO_NAME "/Makefile", "r")));
	int n, maj, min;
	n = fscanf(fp, "VERSION_MAJ = %d\n", &maj);
	check(n == 1);
	n = fscanf(fp, "VERSION_MIN = %d\n", &min);
	check(n == 1);
	fclose(fp);
	
	if (VERSION_MAJ != maj || VERSION_MIN != min || force_build) {
		lprintf("UPDATE: version changed, current %d.%d, new %d.%d\n",
			VERSION_MAJ, VERSION_MIN, maj, min);
		lprintf("UPDATE: building new version..\n");
		//system("cd /root/" REPO_NAME "; make OPT=O0");
		system("cd /root/" REPO_NAME "; make; make install");
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

void schedule_update(int hour)
{
	static bool update_on_startup;
	
	if (hour == 2 || !update_on_startup) {	// 2 AM UTC
		update_on_startup = true;
		lprintf("UPDATE: scheduled\n");
		update_pending = true;
		check_for_update();
	}
}

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
#include <string>

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
   using std::string;

   int status;
   bool check_only = (bool) param;
	
   lprintf("UPDATE: checking for updates from " REPO "\n");
   using namespace std;
   string repo(REPO);
   string repo_name(REPO_NAME);
        
   if (repo.find("https://github.com/") != 0) {
      lprintf("UPDATE: Upstream (%s) must be at github to continue with update\n", repo.substr(0,19).c_str());
      update_pending = update_in_progress = false;
      return;
   }
      // Remove any trailing ".git"
   size_t p = repo_name.rfind(".git");
   if (p != string::npos)
      repo_name.erase(p);
   p = repo.rfind(".git");
   if (p != string::npos)
      repo.erase(p);

   string repo_dir = "/root/" + repo_name;
   string mfn = repo_dir + "/Makefile.1";
        
   string cmd = "wget -q --no-check-certificate https://raw.githubusercontent.com/" +
      repo.substr(19) + "/master/Makefile -O " + mfn;
   lprintf("UPDATE: %s\n", cmd.c_str());
   status = system(cmd.c_str());
  
   if (status < 0 || WEXITSTATUS(status) != 0) {
      lprintf("UPDATE: cmd failed\n");
      update_pending = update_in_progress = false;
      return;
   }
	
   FILE *fp;
   scallz("fopen Makefile.1", (fp = fopen(mfn.c_str(), "r")));
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
      } else if (ver_changed || force_build) {
         lprintf("UPDATE: version changed%s, current %d.%d, new %d.%d\n",
                 force_build? " (forced)":"",
                 VERSION_MAJ, VERSION_MIN, pending_maj, pending_min);
         lprintf("UPDATE: building new version..\n");
         
         pid_t child;
         scall("fork", (child = fork()));
         if (child == 0) {
            if (force_build == 2) {
               cmd = "cd " + repo_dir + "; mv Makefile1 Makefile; rm -f obj/p*.o obj/r*.o obj/f*.o; make";
               lprintf("UPDATE: %s\n", cmd.c_str());
               system(cmd.c_str());
            } else
               if (force_build == 3) {
                  cmd = "cd " + repo_dir + "; rm -f obj_O3/p*.o obj_O3/r*.o obj_O3/f*.o; make";
                  lprintf("UPDATE: %s\n", cmd.c_str());
                  system(cmd.c_str());
               } else {
                  cmd = "cd " + repo_dir + "; make git";
                  lprintf("UPDATE: %s\n", cmd.c_str());
                  int status2 = system(cmd.c_str());
                  if (status2 < 0 || WEXITSTATUS(status2) != 0) {
                     lprintf("UPDATE: git pull, no Internet access?\n");
                     exit(-1);
                  }
                  cmd = "cd " + repo_dir + "; make clean_dist; make; make install";
                  lprintf("UPDATE: %s\n", cmd.c_str());
                  status2 = system(cmd.c_str());
                  lprintf("UPDATE: build status %d\n", status2);
               }
            exit(0);
         }
            
            // Run build in a Linux child process so we can respond to connection attempts
            // and display a "software update in progress" message.
         int pid;
         do {
            TaskSleep(5000000);
            pid = waitpid(child, &status, WNOHANG);
            if (pid < 0) sys_panic("update waitpid");
         } while (pid == 0);
            
         int exit_status = WEXITSTATUS(status);
         if (! (WIFEXITED(status) && exit_status == 0)) {
            if (WIFEXITED(status))
               lprintf("UPDATE: error in build, exit status %d, aborting\n", exit_status);
            else
               lprintf("UPDATE: error in build, non-normal exit, aborting\n");
            update_pending = update_in_progress = false;
            return;
         }
		
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

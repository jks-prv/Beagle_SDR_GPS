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

// Copyright (c) 2014-2017 John Seamons, ZL4VO/KF6VO

#include "types.h"
#include "config.h"
#include "kiwi.h"
#include "mem.h"
#include "misc.h"
#include "str.h"
#include "web.h"
#include "spi.h"
#include "cfg.h"
#include "coroutines.h"
#include "net.h"
#include "debug.h"
#include "non_block.h"

#include <sys/file.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <setjmp.h>
#include <ctype.h>
#include <syslog.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>
#include <sys/mman.h>

#ifdef HOST
	#include <wait.h>
    #include <sys/prctl.h>
#endif

typedef struct {
    #define ZEXP 4      // >= 2
    int size;
    pid_t *list;
} zombies_t;

zombies_t zombies;

void register_zombie(pid_t child_pid)
{
    int i;
    
    for (i=0; i < zombies.size; i++) {
        if (zombies.list[i] == 0) {
            zombies.list[i] = child_pid;
            //lprintf("==== add ZOMBIE @%d pid=%d\n", i, child_pid);
            break;
        }
    }
    
    if (i == zombies.size) {
        zombies.list = (pid_t *) kiwi_irealloc("register_zombie", zombies.list, sizeof(pid_t)*(zombies.size + ZEXP));
        zombies.list[zombies.size] = child_pid;
        //lprintf("==== add exp ZOMBIE @%d pid=%d\n", zombies.size, child_pid);
        memset(&zombies.list[zombies.size+1], 0, sizeof(pid_t)*(ZEXP-1));
        zombies.size += ZEXP;
        //printf("### zombies.size %d\n", zombies.size);
    }
}

void cull_zombies()
{
    for (int i=0; i < zombies.size; i++) {
        if (zombies.list[i] == 0) continue;
        int status;
		pid_t pid = waitpid(zombies.list[i], &status, WNOHANG);
		if (pid > 0) {
		    zombies.list[i] = 0;
		    //lprintf("==== cull ZOMBIE @%d pid=%d status=%d\n", i, pid, status);
		}
    }
}

void child_exit(int rv)
{
    _exit(rv);
}

int child_status_exit(int status, bool error_exit)
{
    if (status < 0 || !WIFEXITED(status))
        child_exit(EXIT_FAILURE);

    int exit_status = WEXITSTATUS(status);
    if (error_exit && exit_status != 0)
        child_exit(exit_status);

    return exit_status;
}

// returns (poll_msec == NO_WAIT)? child_pid : child_status
int child_task(const char *pname, funcP_t func, int poll_msec, void *param)
{
    int i;
    
    // If the child is not going to be waited for because poll_msec == NO_WAIT then it will end up as
    // a zombie process unless we eventually wait for it.
    // We accomplish this by waiting for all child processes in the waitpid() below and detect the zombies.

    //lprintf("CHILD_TASK ENTER %s poll_msec=%d\n", pname, poll_msec);
	pid_t child_pid;
	scall("fork", (child_pid = fork()));
	
	// CAUTION: Only use real_printf() in the child
	if (child_pid == 0) {   // child
		TaskForkChild();

        #ifdef HOST
            // terminate all children when parent exits
            scall("PR_SET_PDEATHSIG", prctl(PR_SET_PDEATHSIG, SIGTERM));

            // rename process as seen by top command
            prctl(PR_SET_NAME, (unsigned long) pname, 0, 0, 0);
        #endif
        // rename process as seen by ps command
        int sl = strlen(main_argv[0]);
        kiwi_snprintf_ptr(main_argv[0], sl, "%-*.*s", sl-1, sl-1, pname);    // have to blank fill, and not overrun, old argv[0]
        
		func(param);	// this function should child_exit() with some other value if it wants
        //real_printf("child_task: child_exit\n");
		child_exit(EXIT_SUCCESS);
	}
	
	// parent
	
    //lprintf("==== child_task: child_pid=%d %s pname=%s\n", child_pid, (poll_msec == 0)? "NO_WAIT":"WAIT", pname);
	if (poll_msec == NO_WAIT) {
	    register_zombie(child_pid);
        //lprintf("CHILD_TASK EXIT %s child_pid=%d\n", pname, child_pid);
	    return child_pid;   // don't wait
	}
	
	int pid = 0, status, polls = 0;
	do {
	    if (pid == 0) {
            TaskSleepMsec(poll_msec);
            polls += poll_msec;
        }
		status = 0;
		pid = waitpid(child_pid, &status, WNOHANG);
        //lprintf("==== child_task: waitpid child_pid=%d pid=%d pname=%s status=%d poll=%d polls=%d\n", child_pid, pid, pname, status, poll_msec, polls);
		//if (pid == 0) lprintf("==== child_task: child_pid=%d NOT YET\n", child_pid);
		//if (pid == child_pid) lprintf("==== child_task: child_pid=%d DONE\n", child_pid);
		//if (pid < 0) lprintf("==== child_task: child_pid=%d ERROR\n", child_pid);
		//if (pid < 0) perror("child_task: waitpid");
	} while (pid != child_pid && pid != -1);

    //lprintf("==== child_task: child_pid=%d status=0x%08x WIFEXITED=%d WEXITSTATUS=%d\n", child_pid, status, WIFEXITED(status), WEXITSTATUS(status));
    if (!WIFEXITED(status))
        printf("child_task WARNING: child returned without WIFEXITED status=0x%08x WIFEXITED=%d WEXITSTATUS=%d\n",
            status, WIFEXITED(status), WEXITSTATUS(status));

    //lprintf("CHILD_TASK EXIT %s status=%d\n", pname, status);
    return status;
}

#define NON_BLOCKING_POLL_MSEC 50

// child task that calls a function for the entire command input read
static void _non_blocking_cmd_forall(void *param)
{
	int n, func_rv = 0;
	nbcmd_args_t *args = (nbcmd_args_t *) param;

	#define NCHUNK 256
	char chunk[NCHUNK + SPACE_FOR_NULL];
	args->kstr = NULL;

    //printf("_non_blocking_cmd_forall: %s\n", args->cmd);
	FILE *pf = popen(args->cmd, "r");
	if (pf == NULL) child_exit(EXIT_FAILURE);
	int pfd = fileno(pf);
	if (pfd <= 0) child_exit(EXIT_FAILURE);
	fcntl(pfd, F_SETFL, O_NONBLOCK);

	do {
		TaskSleepMsec(NON_BLOCKING_POLL_MSEC);
		n = read(pfd, chunk, NCHUNK);
		if (n > 0) {
		    chunk[n] = '\0';
			args->kstr = kstr_cat(args->kstr, chunk);
		}
	} while (n > 0 || (n == -1 && errno == EAGAIN));
	// end-of-input when n == 0 or error

    //printf("_non_blocking_cmd_forall: call func %p\n", args->kstr);
    func_rv = args->func((void *) args);

	kstr_free(args->kstr);
	pclose(pf);

    //printf("_non_blocking_cmd_forall: EXIT func_rv %d\n", func_rv);
	child_exit(func_rv);
	#undef NCHUNK
}

// Like non_blocking_cmd() below, but run in a child process because pclose() can block
// for long periods of time under certain conditions.
// Calls func when _all_ output from cmd is ready to be processed.
//
// CAUTION: func is called in context of child process. So is subject to copy-on-write unless
// shared memory is used to communicated with main Kiwi process.
int non_blocking_cmd_func_forall(const char *pname, const char *cmd, funcPR_t func, int param, int poll_msec)
{
	nbcmd_args_t *args = (nbcmd_args_t *) kiwi_imalloc("non_blocking_cmd_func_forall", sizeof(nbcmd_args_t));
	args->cmd = cmd;
	args->func = func;
	args->func_param = param;
	int status = child_task(pname, _non_blocking_cmd_forall, poll_msec, (void *) args);
	if (poll_msec == NO_WAIT) status = 0;
	kiwi_ifree(args, "non_blocking_cmd_func_forall");
    //printf("non_blocking_cmd_child %d\n", status);
	return status;
}

// child task that calls a function for every chunk of non-blocking command input read
static void _non_blocking_cmd_foreach(void *param)
{
	int i, n, func_rv = 0;
	nbcmd_args_t *args = (nbcmd_args_t *) param;

	#define NCHUNK (1024 + 1)    // NB: must be odd
	char chunk[NCHUNK + SPACE_FOR_NULL];
	args->kstr = NULL;
	bool call = false;
	int offset = 0;

	FILE *pf = popen(args->cmd, "r");
	if (pf == NULL) child_exit(EXIT_FAILURE);
	int pfd = fileno(pf);
	if (pfd <= 0) child_exit(EXIT_FAILURE);
	fcntl(pfd, F_SETFL, O_NONBLOCK);

    //printf("_non_blocking_cmd_foreach START\n");
	do {
		n = read(pfd, chunk, NCHUNK);
		if (n > 0) {
		    chunk[n] = '\0';
		    //printf("_non_blocking_cmd_foreach n=%d\n", n);
			args->kstr = kstr_cat(args->kstr, chunk);
            //printf("_non_blocking_cmd_foreach READ n=%d\n", n);
			call = true;
		} else {
		    if (call) {
		        char *t_kstr = args->kstr;
		        char *sp = kstr_sp(args->kstr);
		        
		        // remove possible padding spaces sent to flush stream
		        // e.g. from our common.php::realtime_msg() used by tdoa.php on server
		        char *ep = sp + strlen(sp) - 1;
		        while (*ep == ' ' && ep != sp) ep--;
		        *(ep+1) = '\0';
		        
		        // remove extra byte that seems to be added to each buffer sent
		        // when padding mechanism is being used
		        for (i = 0; i < offset; i++) {
		            char c = sp[i];
		            if (c != ' ')
		                printf("_non_blocking_cmd_foreach WARN @%d %x<%c>\n", i, c, c);
		        }
		        args->kstr = kstr_cat(sp + offset, NULL);
		        kstr_free(t_kstr);
		        offset++;
		        sp = kstr_sp(args->kstr);
		        int sl = strlen(sp);
		        //printf("_non_blocking_cmd_foreach FUNC sl=%d <%s>\n", sl, sp);
		        if (sl) func_rv = args->func((void *) args);
	            kstr_free(args->kstr);
	            args->kstr = NULL;
		        call = false;
		    }
		    TaskSleepMsec(NON_BLOCKING_POLL_MSEC);
		}
	} while (n > 0 || (n == -1 && errno == EAGAIN));
	// end-of-input when n == 0 or error
    //printf("_non_blocking_cmd_foreach END\n");

	pclose(pf);

	child_exit(func_rv);
	#undef NCHUNK
}

// Like non_blocking_cmd() below, but run in a child process because pclose() can block
// for long periods of time under certain conditions.
// Calls func as _any_ output from cmd is ready to be processed.
//
// CAUTION: func is called in context of child process. So is subject to copy-on-write unless
// shared memory is used to communicated with main Kiwi process.
int non_blocking_cmd_func_foreach(const char *pname, const char *cmd, funcPR_t func, int param, int poll_msec)
{
	nbcmd_args_t *args = (nbcmd_args_t *) kiwi_imalloc("non_blocking_cmd_func_foreach", sizeof(nbcmd_args_t));
	args->cmd = cmd;
	args->func = func;
	args->func_param = param;
	int status = child_task(pname, _non_blocking_cmd_foreach, poll_msec, (void *) args);
	if (poll_msec == NO_WAIT) status = 0;
	kiwi_ifree(args, "non_blocking_cmd_func_foreach");
    //printf("non_blocking_cmd_child %d\n", status);
	return status;
}

static void _non_blocking_cmd_system(void *param)
{
	char *cmd = (char *) param;

    //printf("_non_blocking_cmd_system: %s\n", cmd);
    int rv = system(cmd);
    //printf("_non_blocking_cmd_system: rv=%d %d %d \n", rv, WIFEXITED(rv), WEXITSTATUS(rv));
	child_exit(WEXITSTATUS(rv));
}

// Like non_blocking_cmd() below, but run in a child process because pclose() can block
// for long periods of time under certain conditions.
// Use this instead of non_blocking_cmd() when access to cmd output is not needed.
// You can get the command return status if poll_msec is not NO_WAIT. But then your task
// execution will not progress (even though the task will not block the system).
// Otherwise use non_blocking_cmd_func_forall() or non_blocking_cmd_func_foreach() above.

int non_blocking_cmd_system_child(const char *pname, const char *cmd, int poll_msec)
{
	int status = child_task(pname, _non_blocking_cmd_system, poll_msec, (void *) cmd);
	if (poll_msec == NO_WAIT) status = 0;
    //printf("non_blocking_cmd_child %d\n", status);
	return status;
}

int blocking_system(const char *fmt, ...)
{
	char *sb = NULL;

	va_list ap;
	va_start(ap, fmt);
	vasprintf(&sb, fmt, ap);
	va_end(ap);

	int rv = system(sb);
	kiwi_asfree(sb);
	return rv;
}

// Deprecated for use during normal running when realtime requirements apply
// because pclose() can block for an unpredictable length of time. Use one of the routines above.
// But still useful during init because e.g. non_blocking_cmd() can return an arbitrarily large buffer
// from reading a file as opposed to the above routines which can't due to various limitations.
//
// The popen read can block, so do non-blocking i/o with an interspersed TaskSleep()
// NB: assumes the reply is a string (kstr_t *), not binary with embedded NULLs
kstr_t *non_blocking_cmd(const char *cmd, int *status, int poll_msec)
{
	int n, stat;
	if (poll_msec == 0) poll_msec = NON_BLOCKING_POLL_MSEC;
	#define NBUF 256
	char buf[NBUF + SPACE_FOR_NULL];
	
	NextTask("non_blocking_cmd");
    evNT(EC_EVENT, EV_NEXTTASK, -1, "non_blocking_cmd", evprintf("popen %s...", cmd));
	FILE *pf = popen(cmd, "r");
	if (pf == NULL) return 0;
	int pfd = fileno(pf);
	if (pfd <= 0) return 0;
	fcntl(pfd, F_SETFL, O_NONBLOCK);
    evNT(EC_EVENT, EV_NEXTTASK, -1, "non_blocking_cmd", evprintf("...popen"));
	char *reply = NULL;

	do {
		TaskSleepMsec(poll_msec);
		n = read(pfd, buf, NBUF);
        evNT(EC_EVENT, EV_NEXTTASK, -1, "non_blocking_cmd", evprintf("after read()"));
		if (n > 0) {
		    buf[n] = '\0';
		    reply = kstr_cat(reply, buf);
		}
	} while (n > 0 || (n == -1 && errno == EAGAIN));
	// end-of-input when n == 0 or error

	// assuming we're always expecting a string
    evNT(EC_EVENT, EV_NEXTTASK, -1, "non_blocking_cmd", evprintf("pclose..."));
	stat = pclose(pf);      // yes, pclose returns exit status of cmd
    evNT(EC_EVENT, EV_NEXTTASK, -1, "non_blocking_cmd", evprintf("...pclose"));
	if (status != NULL)
		*status = stat;
	return reply;
	#undef NBUF
}

kstr_t *non_blocking_cmd_fmt(int *status, const char *fmt, ...)
{
	char *cmd = NULL;

	va_list ap;
	va_start(ap, fmt);
	vasprintf(&cmd, fmt, ap);
	va_end(ap);

    kstr_t *rv = non_blocking_cmd(cmd, status);
	kiwi_asfree(cmd);
    return rv;
}

// non_blocking_cmd() broken down into separately callable routines in case you have to
// do something with each chunk of data read
int non_blocking_cmd_popen(non_blocking_cmd_t *p)
{
	NextTask("non_blocking_cmd_popen");
	p->pf = popen(p->cmd, "r");
	if (p->pf == NULL) return 0;
	p->pfd = fileno(p->pf);
	if (p->pfd <= 0) return 0;
	fcntl(p->pfd, F_SETFL, O_NONBLOCK);
	p->open = true;

	return 1;
}

int non_blocking_cmd_read(non_blocking_cmd_t *p, char *reply, int reply_size)
{
	int n;
	assert(p->open);

    TaskSleepMsec(NON_BLOCKING_POLL_MSEC);
    n = read(p->pfd, reply, reply_size - SPACE_FOR_NULL);
    if (n > 0) reply[n] = 0;	// assuming we're always expecting a string

    if (n == 0) n = -1;     // EOF
    else
	if (n == -1 && errno == EAGAIN) n = 0;      // non-block

	return n;
}

int non_blocking_cmd_write(non_blocking_cmd_t *p, char *sbuf)
{
	int n;
	int slen = strlen(sbuf);
	assert(p->open);

	do {
		TaskSleepMsec(NON_BLOCKING_POLL_MSEC);
		n = write(p->pfd, sbuf, slen);
		if (n > 0) {
		    sbuf += n;
		    slen -= n;
		}
	} while (slen > 0 || (n == -1 && errno == EAGAIN));

	return n;
}

int non_blocking_cmd_pclose(non_blocking_cmd_t *p)
{
	assert(p->open);
	return pclose(p->pf);
}

kstr_t *read_file_string_reply(const char *filename)
{
    int n, fd;
	scall("read_file open", (fd = open(filename, O_RDONLY)));
	char *reply = NULL;
	#define NBUF 256
	char buf[NBUF + SPACE_FOR_NULL];
	do {
	    n = read(fd, buf, NBUF);
		if (n > 0) {
		    buf[n] = '\0';
		    reply = kstr_cat(reply, buf);
		}
	} while (n > 0);
	close(fd);
	return reply;
	#undef NBUF
}

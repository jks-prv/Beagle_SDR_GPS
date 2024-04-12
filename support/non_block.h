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

#pragma once

#include "types.h"
#include "kiwi.h"
#include "str.h"
#include "printf.h"


typedef struct {
    bool open;
	const char *cmd;
	FILE *pf;
	int pfd;
} non_blocking_cmd_t;

typedef struct {
	const char *cmd;
	funcPR_t func;
	int func_param, func_rval;
	int poll_msec;
	char *kstr;
} nbcmd_args_t;

#define NO_WAIT         0
#define POLL_MSEC(n)    (n)

int child_task(const char *pname, funcP_t func, int poll_msec = 0, void *param = NULL);
void child_exit(int rv);
#define NO_ERROR_EXIT false
int child_status_exit(int status, bool error_exit = true);
void register_zombie(pid_t child_pid);
void cull_zombies();

int non_blocking_cmd_func_forall(const char *pname, const char *cmd, funcPR_t func, int param, int poll_msec = NO_WAIT);
int non_blocking_cmd_func_foreach(const char *pname, const char *cmd, funcPR_t func, int param, int poll_msec = NO_WAIT);
int non_blocking_cmd_system_child(const char *pname, const char *cmd, int poll_msec = NO_WAIT);

// Deprecated for use during normal running when realtime requirements apply
// because pclose() can block for an unpredictable length of time. Use one of the routines above.
// But still useful during init because e.g. non_blocking_cmd() can return an arbitrarily large buffer
// from reading a file as opposed to the above routines which can't due to various limitations.
kstr_t *non_blocking_cmd(const char *cmd, int *status = NULL, int poll_msec = NO_WAIT);
kstr_t *non_blocking_cmd_fmt(int *status, const char *fmt, ...);
int blocking_system(const char *fmt, ...);

int non_blocking_cmd_popen(non_blocking_cmd_t *p);
int non_blocking_cmd_read(non_blocking_cmd_t *p, char *reply, int reply_size);
int non_blocking_cmd_write(non_blocking_cmd_t *p, char *sbuf);
int non_blocking_cmd_pclose(non_blocking_cmd_t *p);

kstr_t *read_file_string_reply(const char *filename);

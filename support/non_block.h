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

// Copyright (c) 2014-2017 John Seamons, ZL/KF6VO

#pragma once

#include "types.h"
#include "kiwi.h"
#include "str.h"
#include "printf.h"
#include "kiwi.gen.h"

#define N_LOG_MSG_LEN   256
#define N_LOG_SAVE      256

typedef struct {
	int idx, not_shown;
	char *arr[N_LOG_SAVE];
	char mem[1];	// mem allocated starting here; must be last in struct
} log_save_t;

extern log_save_t *log_save_p;

#define N_SHMEM_STATUS 4
#define N_SHMEM_STATUS_STR  256

typedef struct {
    bool kiwi_exit;
    u4_t rv_u4_t[MAX_RX_CHANS];
    u4_t status_u4[N_SHMEM_STATUS][MAX_RX_CHANS];
    double status_f[N_SHMEM_STATUS][MAX_RX_CHANS];
	char status_str[N_SHMEM_STATUS_STR];
    log_save_t log_save;    // must be last because var length
} non_blocking_shmem_t;

extern non_blocking_shmem_t *shmem;

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

int child_task(const char *pname, int poll_msec, funcP_t func, void *param);
void cull_zombies();
int non_blocking_cmd_func_forall(const char *pname, const char *cmd, funcPR_t func, int param, int poll_msec);
int non_blocking_cmd_func_foreach(const char *pname, const char *cmd, funcPR_t func, int param, int poll_msec);
int non_blocking_cmd_system_child(const char *pname, const char *cmd, int poll_msec);
kstr_t *non_blocking_cmd(const char *cmd, int *status);
int non_blocking_cmd_popen(non_blocking_cmd_t *p);
int non_blocking_cmd_read(non_blocking_cmd_t *p, char *reply, int reply_size);
int non_blocking_cmd_write(non_blocking_cmd_t *p, char *sbuf);
int non_blocking_cmd_pclose(non_blocking_cmd_t *p);
kstr_t *read_file_string_reply(const char *filename);

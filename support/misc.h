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

// Copyright (c) 2014-2016 John Seamons, ZL/KF6VO

#pragma once

#include "types.h"
#include "kiwi.h"
#include "misc.h"
#include "printf.h"

#include <sys/file.h>
#include <stdarg.h>

#define MALLOC_DEBUG
#ifdef MALLOC_DEBUG
	void *kiwi_malloc(const char *from, size_t size);
	void *kiwi_realloc(const char *from, void *ptr, size_t size);
	void kiwi_free(const char *from, void *ptr);
	char *kiwi_strdup(const char *from, const char *s);
	void kiwi_str_redup(char **ptr, const char *from, const char *s);
	int kiwi_malloc_stat();
#else
	#define kiwi_malloc(from, size) malloc(size)
	#define kiwi_realloc(from, ptr, size) realloc(ptr, size)
	#define kiwi_free(from, ptr) free(ptr)
	#define kiwi_strdup(from, s) strdup(s)
	void kiwi_str_redup(char **ptr, const char *from, const char *s);
	#define kiwi_malloc_stat() 0
#endif

struct non_blocking_cmd_t {
	const char *cmd;
	FILE *pf;
	int pfd;
};

struct nbcmd_args_t {
	const char *cmd;
	funcPR_t func;
	int func_param, func_rval;
	char *bp;
	int bsize, bc;
};

int child_task(int poll_msec, funcP_t func, void *param);
int non_blocking_cmd_child(const char *cmd, funcPR_t func, int param, int bsize);
int non_blocking_cmd(const char *cmd, char *reply, int reply_size, int *status);
int non_blocking_cmd_popen(non_blocking_cmd_t *p);
int non_blocking_cmd_read(non_blocking_cmd_t *p, char *reply, int reply_size);
int non_blocking_cmd_pclose(non_blocking_cmd_t *p);

u2_t ctrl_get();
void ctrl_clr_set(u2_t clr, u2_t set);
u2_t getmem(u2_t addr);
void printmem(const char *str, u2_t addr);
float ecpu_use();
int qsort_floatcomp(const void* elem1, const void* elem2);

extern char *current_authkey;
char *kiwi_authkey();

#define SM_DEBUG	true
#define SM_NO_DEBUG	false
void send_mc(conn_t *c, char *s, int slen);
void send_msg(conn_t *c, bool debug, const char *msg, ...);
void send_msg_mc(struct mg_connection *mc, bool debug, const char *msg, ...);
void send_msg_data(conn_t *c, bool debug, u1_t dst, u1_t *bytes, int nbytes);
void send_msg_encoded_mc(struct mg_connection *mc, const char *dst, const char *cmd, const char *fmt, ...);

void print_max_min_stream_i(void **state, const char *name, int index, int nargs, ...);
void print_max_min_stream_f(void **state, const char *name, int index, int nargs, ...);
void print_max_min_i(const char *name, int *data, int len);
void print_max_min_f(const char *name, float *data, int len);
void print_max_min_c(const char *name, TYPECPX* data, int len);

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

u2_t ctrl_get();
void ctrl_clr_set(u2_t clr, u2_t set);
void ctrl_positive_pulse(u2_t bits);

union stat_reg_t {
    u2_t word;
    struct {
        u2_t fpga_id:4, stat_user:4, fpga_ver:4, fw_id:3, ovfl:1;
    };
};
stat_reg_t stat_get();

u2_t getmem(u2_t addr);
void printmem(const char *str, u2_t addr);
float ecpu_use();
int qsort_floatcomp(const void* elem1, const void* elem2);

extern char *current_authkey;
char *kiwi_authkey();

#define SM_DEBUG	true
#define SM_NO_DEBUG	false
void send_msg_buf(conn_t *c, char *s, int slen);
void send_msg(conn_t *c, bool debug, const char *msg, ...);
void send_msg_data(conn_t *c, bool debug, u1_t dst, u1_t *bytes, int nbytes);
void send_msg_mc(struct mg_connection *mc, bool debug, const char *msg, ...);
void send_msg_encoded(conn_t *conn, const char *dst, const char *cmd, const char *fmt, ...);
void send_msg_mc_encoded(struct mg_connection *mc, const char *dst, const char *cmd, const char *fmt, ...);
void input_msg_internal(conn_t *conn, const char *fmt, ...);

void print_max_min_stream_i(void **state, const char *name, int index, int nargs, ...);
void print_max_min_stream_f(void **state, const char *name, int index, int nargs, ...);
void print_max_min_u1(const char *name, u1_t *data, int len);
void print_max_min_i(const char *name, int *data, int len);
void print_max_min_f(const char *name, float *data, int len);
void print_max_min_c(const char *name, TYPECPX* data, int len);

int bits_required(u4_t v);

int snd_file_open(const char *fn, int nchans, double srate);

FILE *pgm_file_open(const char *fn, int *offset, int width, int height, int depth);
void pgm_file_height(FILE *fp, int offset, int height);

typedef struct {
	double lat, lon;
} latLon_t;

#define latLon_deg_to_rad(loc) \
	loc.lat = DEG_2_RAD(loc.lat); \
	loc.lon = DEG_2_RAD(loc.lon);

// field square subsquare (extended square)
//   A-R    0-9       a-x              0-9
//   #18    #10       #24              #10

#define SQ_LON_DEG		2.0
#define SQ_LAT_DEG		1.0
#define SUBSQ_PER_SQ	24.0
#define SUBSQ_LON_DEG	(SQ_LON_DEG / SUBSQ_PER_SQ)
#define SUBSQ_LAT_DEG	(SQ_LAT_DEG / SUBSQ_PER_SQ)

#define SQ_PER_FLD		10.0
#define	FLD_DEG_LON		(SQ_PER_FLD * SQ_LON_DEG)
#define	FLD_DEG_LAT		(SQ_PER_FLD * SQ_LAT_DEG)

void grid_to_latLon(char *grid, latLon_t *loc);
int latLon_to_grid6(latLon_t *loc, char *grid);

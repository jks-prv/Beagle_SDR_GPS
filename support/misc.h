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

// Copyright (c) 2014-2016 John Seamons, ZL4VO/KF6VO

#pragma once

#include "types.h"
#include "kiwi.h"
#include "printf.h"
#include "spi.h"

#include <sys/file.h>
#include <stdarg.h>
#include <stdlib.h>

void misc_init();

SPI_MISO *get_misc_miso(int which = 0);
void release_misc_miso(int which = 0);

SPI_MOSI *get_misc_mosi();
void release_misc_mosi();

float ecpu_use();

int qsort_floatcomp(const void *elem1, const void *elem2);
int qsort_doublecomp(const void *elem1, const void *elem2);
int qsort_intcomp(const void *elem1, const void *elem2);
float median_f(float *f, int len, float *pct_1 = NULL, float *pct_2 = NULL);
int median_i(int *i, int len, int *pct_1 = NULL, int *pct_2 = NULL);

#define SM_DEBUG	    true
#define SM_NO_DEBUG	    false
#define SM_RX_CHAN_ALL  -1
#define SM_SND_ADM_ALL  -2
#define SM_ADMIN_ALL    -3

void send_msg_buf(conn_t *c, char *s, int slen);
void send_msg(conn_t *c, bool debug, const char *msg, ...);
int snd_send_msg(int rx_chan, bool debug, const char *msg, ...);
void send_msg_data(conn_t *c, bool debug, u1_t dst, u1_t *bytes, int nbytes);
void send_msg_data2(conn_t *c, bool debug, u1_t dst, u1_t data2, u1_t *bytes, int nbytes);
void send_msg_mc(struct mg_connection *mc, bool debug, const char *msg, ...);
void send_msg_encoded(conn_t *conn, const char *dst, const char *cmd, const char *fmt, ...);
void send_msg_mc_encoded(struct mg_connection *mc, const char *dst, const char *cmd, const char *fmt, ...);
int snd_send_msg_encoded(int rx_chan, bool debug, const char *dst, const char *cmd, const char *msg, ...);
void snd_send_msg_data(int rc_chan, bool debug, u1_t cmd, u1_t *bytes, int nbytes);
void input_msg_internal(conn_t *conn, const char *fmt, ...);

void cmd_debug_print(conn_t *c, char *s, int slen, bool tx);

#define P_MAX_MIN_DEMAND    0x00
#define P_MAX_MIN_RANGE     0x01
#define P_MAX_MIN_DUMP      0x02
#define P_MAX_MIN_RESET     0x04

void print_max_min_stream_i(void **state, int flags, const char *name, int index=0, int nargs=0, ...);
void print_max_min_stream_f(void **state, int flags, const char *name, int index=0, int nargs=0, ...);
void print_max_min_u1(const char *name, u1_t *data, int len);
void print_max_min_u2(const char *name, u2_t *data, int len);
void print_max_min_s2(const char *name, s2_t *data, int len);
void print_max_min_i(const char *name, int *data, int len);
void print_max_min_f(const char *name, float *data, int len);
void print_max_min_c(const char *name, TYPECPX* data, int len);

int bits_required(u4_t v);

int snd_file_open(const char *fn, int nchans, double srate);

FILE *pgm_file_open(const char *fn, int *offset, int width, int height, int depth);
void pgm_file_height(FILE *fp, int offset, int height);

off_t kiwi_file_size(const char *fn);
bool kiwi_file_exists(const char *fn);
int kiwi_file_read(const char *id, const char *fn, char *s, int len, bool rem_nl = false);
int kiwi_file_write(const char *id, const char *fn, char *s, int len, bool add_nl = false);

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

bool grid_to_latLon(const char *grid, latLon_t *loc);
int latLon_to_grid6(latLon_t *loc, char *grid);
int grid_to_distance_km(latLon_t *r_loc, char *grid);

void set_cpu_affinity(int cpu);

u4_t pos_wrap_diff(u4_t next, u4_t prev, u4_t size);

char *toUnits(int num, int instance = 0);

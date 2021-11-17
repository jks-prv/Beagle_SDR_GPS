/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <complex.h>
#include "hfdl_config.h"

typedef struct dumpfile_rf32 *dumpfile_rf32;
typedef struct dumpfile_cf32 *dumpfile_cf32;

#ifdef DATADUMPS

dumpfile_rf32 do_dumpfile_rf32_open(char const *name, float fillval);
void do_dumpfile_rf32_write_value(dumpfile_rf32 f, uint64_t time, float val);
void do_dumpfile_rf32_destroy(dumpfile_rf32 f);

dumpfile_cf32 do_dumpfile_cf32_open(char const *name, float complex fillval);
void do_dumpfile_cf32_write_value(dumpfile_cf32 f, uint64_t time,
		float complex val);
void do_dumpfile_cf32_write_block(dumpfile_cf32 f, uint64_t time,
		float complex *buf, size_t len);
void do_dumpfile_cf32_destroy(dumpfile_cf32 f);

#define dumpfile_rf32_open(name, fillval) do_dumpfile_rf32_open(name, fillval)
#define dumpfile_rf32_write_value(f, time, val) \
	do_dumpfile_rf32_write_value(f, time, val)
#define dumpfile_rf32_destroy(f) do_dumpfile_rf32_destroy(f)

#define dumpfile_cf32_open(name, fillval) do_dumpfile_cf32_open(name, fillval)
#define dumpfile_cf32_write_value(f, time, val) \
	do_dumpfile_cf32_write_value(f, time, val)
#define dumpfile_cf32_write_block(f, time, buf, len) \
	do_dumpfile_cf32_write_block(f, time, buf, len)
#define dumpfile_cf32_destroy(f) do_dumpfile_cf32_destroy(f)

#else

#define dumpfile_rf32_open(name, fillval) NULL
#define dumpfile_rf32_write_value(f, time, val) nop()
#define dumpfile_rf32_destroy(f) nop()

#define dumpfile_cf32_open(name, fillval) NULL
#define dumpfile_cf32_write_value(f, time, val) nop()
#define dumpfile_cf32_write_block(f, time, buf, len) nop()
#define dumpfile_cf32_destroy(f) nop()

#endif

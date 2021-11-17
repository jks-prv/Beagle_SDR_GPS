/* SPDX-License-Identifier: GPL-3.0-or-later */

#ifdef DATADUMPS

#include <stdint.h>
#include <stdio.h>      // fopen, fwrite, fclose
#include <complex.h>    // float complex
#include <math.h>       // NAN
#include <string.h>     // strerror
#include <errno.h>      // errno
#include <unistd.h>     // _exit
#include "hfdl_config.h" // Config
#include "dumpfile.h"   // typedefs
#include "util.h"       // NEW, XFREE, ASSERT

struct dumpfile {
	FILE *f;
	uint64_t time;
};

struct dumpfile_rf32 {
	struct dumpfile *df;
	float fillval;
};

struct dumpfile_cf32 {
	struct dumpfile *df;
	float complex fillval;
};

static struct dumpfile *dumpfile_open(char const *name);
static void dumpfile_write_block(struct dumpfile *df, uint64_t time,
		void *buf, size_t nmemb, size_t len, void *fillval);
static void dumpfile_destroy(struct dumpfile *df);

/**********************************
 * Public routines
 **********************************/

dumpfile_rf32 do_dumpfile_rf32_open(char const *name, float fillval) {
	if(hfdl->Config.datadumps == false) {
		return NULL;
	}
	struct dumpfile *df = dumpfile_open(name);
	NEW(struct dumpfile_rf32, df_r32);
	df_r32->df = df;
	df_r32->fillval = fillval;
	return df_r32;
}

void do_dumpfile_rf32_write_value(dumpfile_rf32 f, uint64_t time, float val) {
	if(hfdl->Config.datadumps == false) {
		return;
	}
	ASSERT(f);
	dumpfile_write_block(f->df, time, &val, 1, sizeof(float), &f->fillval);
}

void do_dumpfile_rf32_destroy(dumpfile_rf32 f) {
	if(hfdl->Config.datadumps == false) {
		return;
	}
	if(f != NULL) {
		dumpfile_destroy(f->df);
		XFREE(f);
	}
}

dumpfile_cf32 do_dumpfile_cf32_open(char const *name, float complex fillval) {
	if(hfdl->Config.datadumps == false) {
		return NULL;
	}
	struct dumpfile *df = dumpfile_open(name);
	NEW(struct dumpfile_cf32, df_c32);
	df_c32->df = df;
	df_c32->fillval = fillval;
	return df_c32;
}

void do_dumpfile_cf32_write_value(dumpfile_cf32 f, uint64_t time,
		float complex val) {
	if(hfdl->Config.datadumps == false) {
		return;
	}
	ASSERT(f);
	dumpfile_write_block(f->df, time, &val, 1, sizeof(float complex), &f->fillval);
}

void do_dumpfile_cf32_write_block(dumpfile_cf32 f, uint64_t time,
		float complex *buf, size_t len) {
	if(hfdl->Config.datadumps == false) {
		return;
	}
	ASSERT(f);
	dumpfile_write_block(f->df, time, buf, len, sizeof(float complex), &f->fillval);
}

void do_dumpfile_cf32_destroy(dumpfile_cf32 f) {
	if(hfdl->Config.datadumps == false) {
		return;
	}
	if(f != NULL) {
		dumpfile_destroy(f->df);
		XFREE(f);
	}
}

/**********************************
 * Private routines
 **********************************/

static struct dumpfile *dumpfile_open(char const *name) {
	NEW(struct dumpfile, df);
	df->f = fopen(name, "w");
	if(df->f == NULL) {
		fprintf(stderr, "Could not open file %s for writing: %s\n",
				name, strerror(errno));
		_exit(1);
	}
	return df;
}

static void dumpfile_write_block(struct dumpfile *df, uint64_t time,
		void *buf, size_t nmemb, size_t len, void *fillval) {
	ASSERT(df);
	ASSERT(buf);
	if(df->time != 0 && df->time < time) {
		// Clock discontinuity means there is a hole in the data.
		// Fill it with the given fill value
		size_t fill_len = time - df->time;
		for(size_t i = 0; i < fill_len; i++) {
			fwrite(fillval, len, 1, df->f);
		}
		df->time += fill_len;
	}
	fwrite(buf, len, nmemb, df->f);
	df->time += nmemb;
}

static void dumpfile_destroy(struct dumpfile *df) {
	if(df == NULL) {
		return;
	}
	if(df->f) {
		fclose(df->f);
	}
	XFREE(df);
}

#endif

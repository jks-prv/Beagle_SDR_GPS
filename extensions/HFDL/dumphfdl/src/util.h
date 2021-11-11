/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

#include <stddef.h>                 // offsetof
#include <stdio.h>                  // fprintf, stderr
#include "pthr.h"                   // pthr_t
#include <stdlib.h>                 // calloc, realloc
#include <libacars/libacars.h>      // la_proto_node, la_type_descriptor
#include <libacars/vstring.h>       // la_vstring
#include "globals.h"                // Config
#include "hfdl_config.h"
#ifndef HAVE_PTHREAD_BARRIERS
#include "pthread_barrier.h"
#endif

// debug message classes
#define D_ALL                       (~0)
#define D_NONE                      (0)
#define D_SDR                       (1 <<  0)
#define D_DSP                       (1 <<  1)
#define D_DSP_DETAIL                (1 <<  2)
#define D_FRAME                     (1 <<  3)
#define D_FRAME_DETAIL              (1 <<  4)
#define D_PROTO                     (1 <<  5)
#define D_PROTO_DETAIL              (1 <<  6)
#define D_STATS                     (1 <<  7)
#define D_CACHE                     (1 <<  8)
#define D_OUTPUT                    (1 <<  9)
#define D_MISC                      (1 << 31)

#define nop() do {} while (0)

#ifdef __GNUC__
#define LIKELY(x)   (__builtin_expect(!!(x),1))
#define UNLIKELY(x) (__builtin_expect(!!(x),0))
#else
#define LIKELY(x) (x)
#define UNLIKELY(x) (x)
#endif

#ifdef __GNUC__
#define PRETTY_FUNCTION __PRETTY_FUNCTION__
#else
#define PRETTY_FUNCTION ""
#endif

#define ASSERT_se(expr) \
	do { \
		if (UNLIKELY(!(expr))) { \
			fprintf(stderr, "Assertion '%s' failed at %s:%u, function %s(). Aborting.\n", #expr, \
					__FILE__, __LINE__, PRETTY_FUNCTION); \
				abort(); \
		} \
	} while (0)

#ifdef NDEBUG
#define ASSERT(expr) nop()
#else
#define ASSERT(expr) ASSERT_se(expr)
#endif

#ifdef DEBUG
#define debug_print(debug_class, fmt, ...) \
	do { \
		if(hfdl->Config.debug_filter & debug_class) { \
			fprintf(stderr, "%s(): " fmt, __func__, ##__VA_ARGS__); \
		} \
	} while (0)

#define debug_print_buf_hex(debug_class, buf, len, fmt, ...) \
	do { \
		if(hfdl->Config.debug_filter & debug_class) { \
			fprintf(stderr, "%s(): " fmt, __func__, ##__VA_ARGS__); \
			fprintf(stderr, "%s(): ", __func__); \
			for(size_t zz = 0; zz < (len); zz++) { \
				fprintf(stderr, "%02x ", buf[zz]); \
				if(zz && (zz+1) % 32 == 0) fprintf(stderr, "\n%s(): ", __func__); \
			} \
			fprintf(stderr, "\n"); \
		} \
	} while(0)
#else
#define debug_print(debug_class, fmt, ...) nop()
#define debug_print_buf_hex(debug_class, buf, len, fmt, ...) nop()
#endif

#define XCALLOC(nmemb, size) xcalloc((nmemb), (size), __FILE__, __LINE__, __func__)
#define XREALLOC(ptr, size) xrealloc((ptr), (size), __FILE__, __LINE__, __func__)
#define XFREE(ptr) do { free(ptr); ptr = NULL; } while(0)
#define NEW(type, x) type *(x) = XCALLOC(1, sizeof(type))
#define UNUSED(x) (void)(x)
#define container_of(ptr, type, member) ((type *)((char *)(ptr) - offsetof(type, member)))
#define max(a, b) ((a) > (b) ? (a) : (b))
#define EOL(x) la_vstring_append_sprintf((x), "%s", "\n")
#define HZ_TO_KHZ(f) ((f) / 1000.0)

// Reverse bit order in a byte (http://graphics.stanford.edu/~seander/bithacks.html#ReverseByteWith64Bits)
#define REVERSE_BYTE(x) (uint8_t)((((x) * 0x80200802ULL) & 0x0884422110ULL) * 0x0101010101ULL >> 32)

void *xcalloc(size_t nmemb, size_t size, char const *file, int32_t line, char const *func);
void *xrealloc(void *ptr, size_t size, char const *file, int32_t line, char const *func);
int32_t start_thread(const char *id, pthr_t *pth, void *(*start_routine)(void *), void *thread_ctx);
void stop_thread(pthr_t pth);
int32_t pthr_barrier_create(const char *id, pthr_barrier_t *barrier, unsigned count);
int32_t pthr_cond_initialize(const char *id, pthr_cond_t *cond, pthr_mutex_t *mutex);
int32_t pthr_mutex_initialize(const char *id, pthr_mutex_t *mutex);

struct octet_string {
	uint8_t *buf;
	size_t len;
};
struct octet_string *octet_string_new(void *buf, size_t len);
struct octet_string *octet_string_copy(struct octet_string const *ostring);
void octet_string_destroy(struct octet_string *ostring);

void append_hexdump_with_indent(la_vstring *vstr, uint8_t *data, size_t len, int32_t indent);

la_proto_node *unknown_proto_pdu_new(void *buf, size_t len);

uint32_t parse_icao_hex(uint8_t const buf[3]);

#define GS_MAX_FREQ_CNT 20       // Max number of frequencies assigned to a ground station
void freq_list_format_text(la_vstring *vstr, int32_t indent, char const *label, uint8_t gs_id, uint32_t freqs);
void gs_id_format_text(la_vstring *vstr, int32_t indent, char const *label, uint8_t gs_id);
void ac_id_format_text(la_vstring *vstr, int32_t indent, char const *label, int32_t freq, uint8_t ac_id);
void ac_data_format_text(la_vstring *vstr, int32_t indent, uint32_t addr);

struct location {
	double lat, lon;
};
double parse_coordinate(uint32_t c);

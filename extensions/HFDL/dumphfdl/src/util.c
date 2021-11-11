/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdio.h>                  // perror
#include <stdlib.h>                 // calloc
#include "pthr.h"                   // pthr_*
#include <errno.h>                  // errno
#include <string.h>                 // strerror
#include <unistd.h>                 // _exit
#include <libacars/libacars.h>      // la_proto_node, la_type_descriptor
#include <libacars/vstring.h>       // la_vstring
#include "hfdl_config.h"
#ifndef HAVE_PTHREAD_BARRIERS
#include "pthread_barrier.h"
#endif
#include "util.h"                   // struct octet_string, struct location
#include "globals.h"                // Systable, Systable_lock, Systable_unlock,
                                    // AC_cache, AC_cache_lock, AC_cache_unlock
#include "systable.h"               // systable_get_station_name
#include "ac_cache.h"               // ac_cache_entry_lookup
#include "ac_data.h"                // ac_data_entry_lookup
#include "kiwi-hfdl.h"

// Forward declarations
char *hexdump(uint8_t *data, size_t len);

void *xcalloc(size_t nmemb, size_t size, char const *file, int32_t line, char const *func) {
	void *ptr = calloc(nmemb, size);
	if(ptr == NULL) {
		fprintf(stderr, "%s:%d: %s(): calloc(%zu, %zu) failed: %s\n",
				file, line, func, nmemb, size, strerror(errno));
		_exit(1);
	}
	return ptr;
}

void *xrealloc(void *ptr, size_t size, char const *file, int32_t line, char const *func) {
	ptr = realloc(ptr, size);
	if(ptr == NULL) {
		fprintf(stderr, "%s:%d: %s(): realloc(%zu) failed: %s\n",
				file, line, func, size, strerror(errno));
		_exit(1);
	}
	return ptr;
}

static int32_t detach_thread(pthr_t *pth) {
	ASSERT(pth);
	int32_t ret = 0;
	if((ret = pthr_detach(*pth) != 0)) {
		errno = ret;
		perror("pthr_detach() failed");
	}
	return ret;
}

int32_t start_thread(const char *id, pthr_t *pth, void *(*start_routine)(void *), void *thread_ctx) {
	int32_t ret = 0;
	if((ret = pthr_create(id, pth, NULL, start_routine, thread_ctx) != 0)) {
		errno = ret;
		perror("pthr_create() failed");
		return ret;
	}
	return detach_thread(pth);
}

void stop_thread(pthr_t pth) {
	int32_t ret = 0;
	if((pthr_join(pth, NULL)) != 0) {
		errno = ret;
		perror("pthr_join failed");
	}
}

int32_t pthr_barrier_create(const char *id, pthr_barrier_t *barrier, uint32_t count) {
	int32_t ret;
	(void) id;
	if((ret = pthr_barrier_init(barrier, NULL, count)) != 0) {
		errno = ret;
		perror("pthr_barrier_init() failed");
	}
	return ret;
}

int32_t pthr_cond_initialize(const char *id, pthr_cond_t *cond, pthr_mutex_t *mutex) {
	int32_t ret;
	if((ret = pthr_cond_init(id, cond, mutex, NULL)) != 0) {
		errno = ret;
		perror("pthr_cond_init() failed");
	}
	return ret;
}

int32_t pthr_mutex_initialize(const char *id, pthr_mutex_t *mutex) {
	int32_t ret;
	if((ret = pthr_mutex_init(id, mutex, NULL)) != 0) {
		errno = ret;
		perror("pthr_mutex_init() failed");
	}
	return ret;
}

struct octet_string *octet_string_new(void *buf, size_t len) {
	NEW(struct octet_string, ostring);
	ostring->buf = buf;
	ostring->len = len;
	return ostring;
}

struct octet_string *octet_string_copy(struct octet_string const *ostring) {
	ASSERT(ostring != NULL);
	NEW(struct octet_string, copy);
	copy->len = ostring->len;
	if(ostring->buf != NULL && ostring->len > 0) {
		copy->buf = XCALLOC(copy->len, sizeof(uint8_t));
		memcpy(copy->buf, ostring->buf, ostring->len);
	}
	return copy;
}

void octet_string_destroy(struct octet_string *ostring) {
	if(ostring == NULL) {
		return;
	}
	XFREE(ostring->buf);
	XFREE(ostring);
}

char *hexdump(uint8_t *data, size_t len) {
	static const char hex[] = "0123456789abcdef";
	if(data == NULL) return strdup("<undef>");
	if(len == 0) return strdup("<none>");

	size_t rows = len / 16;
	if((len & 0xf) != 0) {
		rows++;
	}
	size_t rowlen = 16 * 2 + 16;            // 32 hex digits + 16 spaces per row
	rowlen += 16;                           // ASCII characters per row
	rowlen += 10;                           // extra space for separators
	size_t alloc_size = rows * rowlen + 1;  // terminating NULL
	char *buf = XCALLOC(alloc_size, sizeof(char));
	char *ptr = buf;
	size_t i = 0, j = 0;
	while(i < len) {
		for(j = i; j < i + 16; j++) {
			if(j < len) {
				*ptr++ = hex[((data[j] >> 4) & 0xf)];
				*ptr++ = hex[data[j] & 0xf];
			} else {
				*ptr++ = ' ';
				*ptr++ = ' ';
			}
			*ptr++ = ' ';
			if(j == i + 7) {
				*ptr++ = ' ';
			}
		}
		*ptr++ = ' ';
		*ptr++ = '|';
		for(j = i; j < i + 16; j++) {
			if(j < len) {
				if(data[j] < 32 || data[j] > 126) {
					*ptr++ = '.';
				} else {
					*ptr++ = data[j];
				}
			} else {
				*ptr++ = ' ';
			}
			if(j == i + 7) {
				*ptr++ = ' ';
			}
		}
		*ptr++ = '|';
		*ptr++ = '\n';
		i += 16;
	}
	return buf;
}


void append_hexdump_with_indent(la_vstring *vstr, uint8_t *data, size_t len, int32_t indent) {
	ASSERT(vstr != NULL);
	ASSERT(indent >= 0);
	char *h = hexdump(data, len);
	la_isprintf_multiline_text(vstr, indent, h);
	XFREE(h);
}

// la_proto_node routines for unknown protocols
// which are to be serialized as octet string (hex dump or hex string)

static void unknown_proto_format_text(la_vstring *vstr, void const *data, int32_t indent) {
	ASSERT(vstr != NULL);
	ASSERT(data != NULL);
	ASSERT(indent >= 0);

	struct octet_string const *ostring = data;
	// fmt_hexstring also checks this conditon, but when it hits, it prints "empty" or "none",
	// which we want to avoid here
	if(ostring->buf == NULL || ostring->len == 0) {
		return;
	}
	LA_ISPRINTF(vstr, indent, "Data (%zu bytes):\n", ostring->len);
	append_hexdump_with_indent(vstr, ostring->buf, ostring->len, indent+1);
}

static la_type_descriptor const proto_DEF_unknown = {
	.format_text = unknown_proto_format_text,
	.destroy = NULL
};

la_proto_node *unknown_proto_pdu_new(void *buf, size_t len) {
	struct octet_string *ostring = octet_string_new(buf, len);
	la_proto_node *node = la_proto_node_new();
	node->td = &proto_DEF_unknown;
	node->data = ostring;
	node->next = NULL;
	return node;
}

// Misc utility functions

// Parses ICAO hex address from byte buffer to uint32_t
uint32_t parse_icao_hex(uint8_t const buf[3]) {
	uint32_t result = 0u;
	for(int32_t i = 0; i < 3; i++) {
		result |= REVERSE_BYTE(buf[i]) << (8 * (2 - i));
	}
	return result;
}

void freq_list_format_text(la_vstring *vstr, int32_t indent, char const *label, uint8_t gs_id, uint32_t freqs) {
	ASSERT(vstr);
	ASSERT(indent >= 0);
	ASSERT(label);

	LA_ISPRINTF(vstr, indent, "%s: ", label);
	bool first = true;
	Systable_lock();
	for(int32_t i = 0; i < GS_MAX_FREQ_CNT; i++) {
		if((freqs >> i) & 1) {
			double f = systable_get_station_frequency(hfdl->Systable, gs_id, i);
			if(f > 0.0) {
				la_vstring_append_sprintf(vstr, "%s%.1f", first ? "" : ", ", f);
			} else {
				la_vstring_append_sprintf(vstr, "%s%d", first ? "" : ", ", i);
			}
			first = false;
		}
	}
	Systable_unlock();
	EOL(vstr);
}

void gs_id_format_text(la_vstring *vstr, int32_t indent, char const *label, uint8_t gs_id) {
	ASSERT(vstr);
	ASSERT(indent >= 0);
	ASSERT(label);

	char const *gs_name = NULL;
	Systable_lock();
	gs_name = systable_get_station_name(hfdl->Systable, gs_id);
	Systable_unlock();
	LA_ISPRINTF(vstr, indent, "%s: ", label);
	if(gs_name != NULL) {
		la_vstring_append_sprintf(vstr, "%s\n", gs_name);
	} else {
		la_vstring_append_sprintf(vstr, "%hhu\n", gs_id);
	}
}

void ac_id_format_text(la_vstring *vstr, int32_t indent, char const *label, int32_t freq, uint8_t ac_id) {
	ASSERT(vstr);
	ASSERT(indent >= 0);
	ASSERT(label);

	struct ac_cache_entry *entry = NULL;
	AC_cache_lock();
	entry = ac_cache_entry_lookup(hfdl->AC_cache, freq, ac_id);
	AC_cache_unlock();
	LA_ISPRINTF(vstr, indent, "%s: ", label);
	if(entry != NULL) {
		la_vstring_append_sprintf(vstr, "%hhu (%06X)\n", ac_id, entry->icao_address);
		ac_data_format_text(vstr, indent + 1, entry->icao_address);
	} else {
		la_vstring_append_sprintf(vstr, "%hhu\n", ac_id);
	}
}

void ac_data_format_text(la_vstring *vstr, int32_t indent, uint32_t addr) {
	if(hfdl->Config.ac_data_available == true) {
		struct ac_data_entry *ac = ac_data_entry_lookup(hfdl->AC_data, addr);
		if(hfdl->Config.ac_data_details == AC_DETAILS_NORMAL) {
			LA_ISPRINTF(vstr, indent, "AC info: %s, %s, %s\n",
					ac && ac->registration ? ac->registration : "-",
					ac && ac->icaotypecode ? ac->icaotypecode : "-",
					ac && ac->operatorflagcode ? ac->operatorflagcode : "-"
					);
		} else if(hfdl->Config.ac_data_details == AC_DETAILS_VERBOSE) {
			LA_ISPRINTF(vstr, indent, "AC info: %s, %s, %s, %s\n",
					ac && ac->registration ? ac->registration : "-",
					ac && ac->manufacturer ? ac->manufacturer : "-",
					ac && ac->type ? ac->type : "-",
					ac && ac->registeredowners ? ac->registeredowners : "-"
					);
		}
	}
}

double parse_coordinate(uint32_t c) {
	struct { int32_t coord:20; } s;
	int32_t r = s.coord = (int32_t)c;
	double result = r * 180.0 / (double)0x7ffff;
	debug_print(D_PROTO, "r=%d (%06X)\n", r, r);
	return result;
}

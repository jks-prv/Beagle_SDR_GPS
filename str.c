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

#include "types.h"
#include "config.h"
#include "kiwi.h"
#include "str.h"

#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdarg.h>

void get_chars(char *field, char *value, size_t size)
{
	memcpy(value, field, size);
	value[size] = '\0';
}

void set_chars(char *field, const char *value, const char fill, size_t size)
{
	int slen = strlen(value);
	assert(slen <= size);
	memset(field, (int) fill, size);
	memcpy(field, value, strlen(value));
}

// makes a copy of ocp since delimiters are turned into NULLs
// caller must free *mbuf
int kiwi_split(char *ocp, char **mbuf, const char *delims, char *argv[], int nargs)
{
	int n=0;
	char **ap, *tp;
	*mbuf = (char *) malloc(strlen(ocp)+1);
	strcpy(*mbuf, ocp);
	tp = *mbuf;
	
	for (ap = argv; (*ap = strsep(&tp, delims)) != NULL;) {
		if (**ap != '\0') {
			n++;
			if (++ap >= &argv[nargs])
				break;
		}
	}
	
	return n;
}

void str_unescape_quotes(char *str)
{
	char *s, *o;
	
	for (s = o = str; *s != '\0';) {
		if (*s == '\\' && (*(s+1) == '"' || *(s+1) == '\'')) {
			*o++ = *(s+1); s += 2;
		} else {
			*o++ = *s++;
		}
	}
	
	*o = '\0';
}

char *str_encode(char *src)
{
	if (src == NULL) src = (char *) "null";		// JSON compatibility
	
	#define ENCODE_EXPANSION_FACTOR 3		// c -> %xx
	size_t slen = (strlen(src) * ENCODE_EXPANSION_FACTOR) + SPACE_FOR_NULL;

	// don't use kiwi_malloc() due to large number of these simultaneously active from dx list
	// and also because dx list has to use free() due to related allocations via strdup()
	assert(slen);
	char *dst = (char *) malloc(slen);
	mg_url_encode(src, dst, slen);
	return dst;		// NB: caller must free dst
}

char *str_decode_inplace(char *src)
{
	if (src == NULL) return NULL;
	int slen = strlen(src);
	char *dst = src;
	// dst = src is okay because length dst always <= src
	mg_url_decode(src, slen, dst, slen + SPACE_FOR_NULL, 0);
	return dst;
}

static char dst_static[256];

// for use with e.g. an immediate printf argument
char *str_decode_static(char *src)
{
	if (src == NULL) return NULL;
	mg_url_decode(src, strlen(src), dst_static, sizeof(dst_static), 0);
	return dst_static;
}

int str2enum(const char *s, const char *strs[], int len)
{
	int i;
	for (i=0; i<len; i++) {
		if (strcasecmp(s, strs[i]) == 0) return i;
	}
	return NOT_FOUND;
}

const char *enum2str(int e, const char *strs[], int len)
{
	if (e < 0 || e >= len) return NULL;
	assert(strs[e] != NULL);
	return (strs[e]);
}

void kiwi_chrrep(char *str, const char from, const char to)
{
	char *cp;
	while ((cp = strchr(str, from)) != NULL) {
		*cp = to;
	}
}


// C-string: char array or string constant

struct kstr_t {
	//struct kstr_t *next;
	char *sp;
	int size;
	bool valid, externally_malloced;
};

#define KSTRINGS	1024
kstr_t kstrings[KSTRINGS];

static kstr_t *kstr_is(char *s_kstr_cstr)
{
    // implicit: if (s_kstr_cstr == NULL) return NULL
	kstr_t *ks = (kstr_t *) s_kstr_cstr;
	if (ks >= kstrings && ks < &kstrings[KSTRINGS]) {
	    assert(ks->valid);
		return ks;
	} else {
		return NULL;
	}
}

#define KSTR_EXTERNALLY_MALLOCED 0

// size == 0: cstr points to externally malloc()'d string
// size != 0: size is length (including SPACE_FOR_NULL) of string to malloc()
static char *kstr_alloc(char *cstr, int size)
{
	kstr_t *ks;
	
	for (ks = kstrings; ks < &kstrings[KSTRINGS]; ks++) {
		if (!ks->valid) {
			bool externally_malloced = false;
			if (size == KSTR_EXTERNALLY_MALLOCED) {
				assert(cstr != NULL);
				size = strlen(cstr) + SPACE_FOR_NULL;
				//printf("%3d ALLOC %4d %p {%p} EXT <%s>\n", ks-kstrings, size, ks, cstr, cstr);
				externally_malloced = true;
			} else {
				assert(cstr == NULL);
				cstr = (char *) malloc(size);
				cstr[0] = '\0';
				//printf("%3d ALLOC %4d %p {%p}\n", ks-kstrings, size, ks, cstr);
			}
			ks->sp = cstr;
			ks->size = size;
			ks->externally_malloced = externally_malloced;
	        ks->valid = true;
			return (char *) ks;
		}
	}
	panic("kstr_alloc");
	return NULL;
}

static char *kstr_what(char *s_kstr_cstr)
{
	char *p;
	
	if (s_kstr_cstr == NULL) return (char *) "NULL";
	kstr_t *ks = kstr_is(s_kstr_cstr);
	if (ks) {
		asprintf(&p, "#%ld:%d/%lu|%p|{%p}%s",
			ks-kstrings, ks->size, strlen(ks->sp), ks, ks->sp, ks->externally_malloced? "-EXT":"");
	} else {
		asprintf(&p, "%p", s_kstr_cstr);
	}
	return p;
}

// s: kstr|C-string
char *kstr_sp(char *s_kstr_cstr)
{
	kstr_t *ks = kstr_is(s_kstr_cstr);
	
	if (ks) {
		assert(ks->valid);
		return ks->sp;
	} else {
		return s_kstr_cstr;
	}
}

char *kstr_wrap(char *s_malloc)
{
	if (s_malloc == NULL) return NULL;
	assert (!kstr_is(s_malloc));
	return kstr_alloc(s_malloc, KSTR_EXTERNALLY_MALLOCED);
}

// s: kstr|C-string
void kstr_free(char *s_kstr_cstr)
{
	if (s_kstr_cstr == NULL) return;
	
	kstr_t *ks = kstr_is(s_kstr_cstr);
	
	if (ks) {
		assert(ks->valid);
		//printf("%3d  FREE %4d %p {%p} %s\n", ks-kstrings, ks->size, ks, ks->sp, ks->externally_malloced? "EXT":"");
		free((char *) ks->sp);
		ks->sp = NULL;
		ks->size = 0;
		ks->externally_malloced = false;
		ks->valid = false;
	}
}

int kstr_len(char *s_kstr_cstr)
{
	return (s_kstr_cstr != NULL)? ( strlen(kstr_sp(s_kstr_cstr)) ) : 0;
}

char *kstr_cpy(char *s1, const char *cs2)
{
	char *s2 = (char *) cs2;
	//if (!s1 || !s2) return NULL;
	assert(s1 && s2);
	
	char *sp1 = kstr_sp(s1);
	char *sp2 = kstr_sp(s2);
	//printf("kstr_cpy s1=%s {%p} s2=%s {%p}\n", kstr_what(s1), sp1, kstr_what(s2), sp2);
	strcpy(sp1, sp2);
	return sp1;
	
}

char *kstr_cat(char *s1, const char *cs2)
{
	char *s2 = (char *) cs2;
	int slen = kstr_len(s1) + kstr_len(s2);
	//printf("kstr_cat s1=%s s2=%s\n", kstr_what(s1), kstr_what(s2));
	char *sr = kstr_alloc(NULL, slen + SPACE_FOR_NULL);
	//printf("kstr_cat sr1=%s\n", kstr_what(sr));
	char *srp = kstr_sp(sr);
	
	if (s1) {
		strcpy(srp, kstr_sp(s1));
		kstr_free(s1);
	} else {
		srp[0] = '\0';
	}
	
	if (s2) {
		strcat(srp, kstr_sp(s2));
		kstr_free(s2);
	}
	
	//printf("kstr_cat sr2=%s\n", kstr_what(sr));
	return sr;
}

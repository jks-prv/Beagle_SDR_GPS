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

int kiwi_split(char *cp, const char *delims, char *argv[], int nargs)
{
	int n=0;
	char **ap;
	
	for (ap = argv; (*ap = strsep(&cp, delims)) != NULL;) {
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
	int slen = strlen(src);
	char *dst = src;
	// dst = src is okay because length dst always <= src
	mg_url_decode(src, slen, dst, slen + SPACE_FOR_NULL, 0);
	return dst;
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

void kiwi_copy_terminate_free(char *src, char *dst, int size)
{
	strncpy(dst, src, size);
	dst[size-1] = '\0';
	free(src);
}

// FIXME: change to use a magic header with 2x auto-expanding size
char *kiwi_str(char *s)
{
	int slen = s? strlen(s) : 0;
	char *sr = (char *) malloc(slen + SPACE_FOR_NULL);
	
	if (s) { strcpy(sr, s); } else sr[0] = '\0';
	
	return sr;
}

char *kiwi_strcat(char *s1, char *s2)
{
	int s1len = s1? strlen(s1) : 0;
	int s2len = s2? strlen(s2) : 0;
	char *sr = (char *) malloc(s1len + s2len + SPACE_FOR_NULL);
	
	if (s1) { strcpy(sr, s1); free(s1); } else sr[0] = '\0';
	if (s2) { strcat(sr, s2); free(s2); }
	
	return sr;
}

char *kiwi_strcat_const(char *s1, const char *s2)
{
	int s1len = s1? strlen(s1) : 0;
	int s2len = s2? strlen(s2) : 0;
	char *sr = (char *) malloc(s1len + s2len + SPACE_FOR_NULL);
	
	if (s1) { strcpy(sr, s1); free(s1); } else sr[0] = '\0';
	if (s2) { strcat(sr, s2); }
	
	return sr;
}

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

#define GET_CHARS(field, value) get_chars(field, value, sizeof(field));
void get_chars(char *field, char *value, size_t size);
#define SET_CHARS(field, value, fill) set_chars(field, value, fill, sizeof(field));
void set_chars(char *field, const char *value, const char fill, size_t size);
int kiwi_split(char *cp, const char *delims, char *argv[], int nargs);
void str_unescape_quotes(char *str);
char *str_encode(char *s);
char *str_decode_inplace(char *src);
char *str_decode_static(char *src);
int str2enum(const char *s, const char *strs[], int len);
const char *enum2str(int e, const char *strs[], int len);
void kiwi_chrrep(char *str, const char from, const char to);

// cstr = C-string|NULL
char *kstr_sp(char *s_kstr_cstr);   // return string pointer from kstr object
char *kstr_wrap(char *s_malloc);    // wrap a malloc()'d string in a kstr object
void kstr_free(char *s_kstr_cstr);
int kstr_len(char *s_kstr_cstr);
char *kstr_cpy(char *s1_kstr_cstr, const char *s2_kstr_cstr);   // s1 = kstr|C-string|NULL, s2 = kstr|C-string|NULL
char *kstr_cat(char *s1_kstr_cstr, const char *s2_kstr_cstr);   // s1 = kstr|C-string|NULL, s2 = kstr|C-string|NULL

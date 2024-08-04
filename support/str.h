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

// Copyright (c) 2014-2017 John Seamons, ZL4VO/KF6VO

#pragma once

#include "types.h"


// kstr: Kiwi C-string package
//
// any s_kstr_cstr argument = kstr_t|C-string|NULL
// C-string: char array or string constant or NULL

typedef char kstr_t;

void kstr_init();
C_LINKAGE(char *kstr_sp(kstr_t *s_kstr_cstr));  // return C-string pointer from kstr object
C_LINKAGE(void kstr_free(kstr_t *s_kstr_cstr)); // only frees a kstr object, will not free a malloc()'d c-string unless kstr_wrap()'d
C_LINKAGE(char *kstr_free_return_malloced(kstr_t *s_kstr_cstr));  // frees kstr object, but returns underlying memory malloced for string
C_LINKAGE(int kstr_len(kstr_t *s_kstr_cstr));  // return C-string length from kstr object
C_LINKAGE(kstr_t *kstr_wrap(char *s_malloc));  // wrap a malloc()'d C-string in a kstr object so it is auto-freed later on
C_LINKAGE(kstr_t *kstr_cat(kstr_t *s1_kstr_cstr, const kstr_t *s2_kstr_cstr, int *slen DEF_NULL));  // will kstr_free() s2_kstr_cstr argument
C_LINKAGE(kstr_t *kstr_asprintf(kstr_t *ks, const char *fmt, ...));  // essentially a "kstr_cat(ks, kstr_wrap(asprintf(fmt, ...)))"
C_LINKAGE(kstr_t *kstr_list_int(const char *head, const char *fmt, const char *tail, int *list, int nlist, int *qual DEF_NULL, int bias DEF_0));


#define GET_CHARS(field, value) kiwi_get_chars(field, value, sizeof(field));
void kiwi_get_chars(char *field, char *value, size_t size);
#define SET_CHARS(field, value, fill) kiwi_set_chars(field, value, fill, sizeof(field));
void kiwi_set_chars(char *field, const char *value, const char fill, size_t size);
bool kiwi_nonEmptyStr(const char *s);
char *kiwi_str_replace(char *s, const char *from, const char *to, bool *caller_must_free DEF_NULL);
void kiwi_str_unescape_quotes(char *str);
char *kiwi_json_to_html(char *str, bool doBR DEF_TRUE);
char *kiwi_json_to_string(char *str);
void kiwi_remove_unprintable_chars_inplace(char *str, int *printable DEF_NULL, int *UTF DEF_NULL);
char *kiwi_str_escape_HTML(char *str, int *printable DEF_NULL, int *UTF DEF_NULL);
char *kiwi_str_decode_inplace(char *src);
char *kiwi_str_decode_static(char *src, int which DEF_0);
char *kiwi_str_ASCII_static(char *src, int which DEF_0);
char *kiwi_URL_enc_to_C_hex_esc_enc(char *src);
void kiwi_chrrep(char *str, const char from, const char to);
bool kiwi_str_begins_with(char *s, const char *cs);
char *kiwi_str_ends_with(char *s, const char *cs);
char *kiwi_skip_over(char *s, const char *skip);
char *kstr_sp_less_trailing_nl(char *s_kstr_cstr);
char *kiwi_overlap_strcpy(char *dst, const char *src);
u1_t *kiwi_overlap_memcpy(u1_t *dst, const u1_t *src, int n);
int kiwi_strnlen(const char *s, int limit);
char *kiwi_strncpy(char *dst, const char *src, size_t n);
char *kiwi_strncat(char *dst, const char *src, size_t n);

int _kiwi_snprintf_int(const char *buf, size_t buflen, const char *fmt, ...);
// NB: check that caller buf is const (i.e. not a pointer) so sizeof(buf) is valid
#define kiwi_snprintf_buf(buf, fmt, ...) _kiwi_snprintf_int(buf, sizeof(buf), fmt, ## __VA_ARGS__)
#define kiwi_snprintf_buf_plus_space_for_null(buf, fmt, ...) _kiwi_snprintf_int(buf, sizeof(buf) + SPACE_FOR_NULL, fmt, ## __VA_ARGS__)
#define kiwi_snprintf_ptr(ptr, buflen, fmt, ...) _kiwi_snprintf_int(ptr, buflen, fmt, ## __VA_ARGS__)

bool kiwi_sha256_strcmp(char *str, const char *key);

enum { FEWER_ENCODED = 1, USE_MALLOC = 2 };
char *kiwi_str_decode_selective_inplace(char *src, int flags DEF_0);
char *kiwi_str_encode(char *s, const char *from DEF_NULL, int flags DEF_0);
char *kiwi_str_encode_static(char *src, int flags DEF_0);

typedef struct {
    char *str;
    char delim;
} str_split_t;
enum { KSPLIT_NO_SKIP_EMPTY_FIELDS = 0x1, KSPLIT_HANDLE_EMBEDDED_DELIMITERS = 0x2 };
int kiwi_split(char *ocp, char **mbuf, const char *delims, str_split_t argv[], int nargs, int flags DEF_0);

enum { KCLEAN_DELETE = 1, KCLEAN_REPL_SPACE = 2 };
char *kiwi_str_clean(char *s, int type);

extern char ASCII[256][8];

#define STR_HASH_MISS 0

typedef struct {
    const char *name;
    u2_t key, hash;
} str_hashes_t;

typedef struct {
    bool init;
    const char *id;
    str_hashes_t *hashes;
    int hash_len, max_hash_len;
    int lookup_table_size;
    u2_t cur_hash, *keys;
} str_hash_t;

void str_hash_init(const char *id, str_hash_t *hashp, str_hashes_t *hashes, bool debug DEF_FALSE);
u2_t str_hash_lookup(str_hash_t *hashp, char *str, bool debug DEF_FALSE);

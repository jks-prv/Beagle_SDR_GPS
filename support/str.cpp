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

// Copyright (c) 2014-2017 John Seamons, ZL/KF6VO

#include "types.h"
#include "config.h"
#include "kiwi.h"
#include "misc.h"
#include "str.h"
#include "sha256.h"

#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>


// kstr: Kiwi C-string package
//
// any kstr_cstr argument = kstr_t|C-string|NULL
// C-string: char array or string constant or NULL

typedef struct kstring_st {
	struct kstring_st *next_free;
	char *sp;
	int size;
	bool valid, externally_malloced;
} kstring_t;

#define KSTRINGS	1024
static kstring_t kstrings[KSTRINGS], *kstr_next_free;
int kstr_nused;

void kstr_init()
{
    kstring_t *ks;
    
	for (ks = kstrings; ks < &kstrings[KSTRINGS-1]; ks++) {
	    ks->next_free = ks+1;
	}
	
	ks->next_free = NULL;
	kstr_next_free = kstrings;
}

static kstring_t *kstr_is(char *s_kstr_cstr)
{
    // implicit: if (s_kstr_cstr == NULL) return NULL
	kstring_t *ks = (kstring_t *) s_kstr_cstr;
	if (ks >= kstrings && ks < &kstrings[KSTRINGS]) {
	    assert(ks->valid);
		return ks;
	} else {
		return NULL;
	}
}

typedef enum { KSTR_ALLOC, KSTR_REALLOC, KSTR_EXT_MALLOC } kstr_malloc_e;

static char *kstr_malloc(kstr_malloc_e type, char *s_kstr_cstr, int size)
{
    kstring_t *ks;
    
	if (type == KSTR_REALLOC) {
	    ks = kstr_is(s_kstr_cstr);
        assert(ks != NULL);
        assert(ks->sp != NULL);
        assert(size >= 0);
        ks->sp = (char *) realloc(ks->sp, size);
        ks->size = size;
        return (char *) ks;
	}
	
	ks = kstr_next_free;
	assert(ks != NULL);
	kstr_next_free = ks->next_free;
	kstr_nused++;
	
    bool externally_malloced = false;

    if (type == KSTR_EXT_MALLOC) {
        assert(s_kstr_cstr != NULL);
        assert(!kstr_is(s_kstr_cstr));
        assert(size == 0);
        size = strlen(s_kstr_cstr) + SPACE_FOR_NULL;
        //printf("%3d ALLOC %4d %p {%p} EXT <%s>\n", ks-kstrings, size, ks, s_kstr_cstr, s_kstr_cstr);
        externally_malloced = true;
    } else {    // type == KSTR_ALLOC
        assert(s_kstr_cstr == NULL);
        assert(size >= 0);
        s_kstr_cstr = (char *) malloc(size);
        s_kstr_cstr[0] = '\0';
        //printf("%3d ALLOC %4d %p {%p}\n", ks-kstrings, size, ks, s_kstr_cstr);
    }

    ks->sp = s_kstr_cstr;
    ks->size = size;
    ks->externally_malloced = externally_malloced;
    ks->valid = true;
    return (char *) ks;
}

// debugging only -- caller should really free
static char *kstr_what(char *s_kstr_cstr)
{
	char *p;
	
	if (s_kstr_cstr == NULL) return (char *) "NULL";
	kstring_t *ks = kstr_is(s_kstr_cstr);
	if (ks) {
		asprintf(&p, "#%d:%d/%d|%p|{%p}%s",
			(int) (ks-kstrings), ks->size, (int) strlen(ks->sp), ks, ks->sp, ks->externally_malloced? "-EXT":"");
	} else {
		asprintf(&p, "%p", s_kstr_cstr);
	}
	return p;
}

// return C-string pointer from kstr object
// kstr_cstr: kstr|C-string|NULL
char *kstr_sp(char *s_kstr_cstr)
{
	kstring_t *ks = kstr_is(s_kstr_cstr);
	
	if (ks) {
		assert(ks->valid);
		return ks->sp;
	} else {
		return s_kstr_cstr;
	}
}

// wrap a malloc()'d C-string in a kstr object so it is auto-freed later on
char *kstr_wrap(char *s_malloced)
{
	if (s_malloced == NULL) return NULL;
	assert (!kstr_is(s_malloced));
	return kstr_malloc(KSTR_EXT_MALLOC, s_malloced, 0);
}

// Only frees a kstr object, will not free a malloc()'d C-string unless kstr_wrap()'d.
// It is normal that this routine might be called with a C-string in code that is freeing
// a mix of kstr and C-strings (wrapped or not).
// kstr_cstr: kstr|C-string|NULL
void kstr_free(char *s_kstr_cstr)
{
	if (s_kstr_cstr == NULL) return;
	
	kstring_t *ks = kstr_is(s_kstr_cstr);
	
	if (ks) {
		assert(ks->valid);
		//printf("%3d  FREE %4d %p {%p} %s\n", ks-kstrings, ks->size, ks, ks->sp, ks->externally_malloced? "EXT":"");
		free((char *) ks->sp);
		ks->sp = NULL;
		ks->size = 0;
		ks->externally_malloced = false;
		ks->valid = false;
		ks->next_free = kstr_next_free;
		kstr_next_free = ks;
		kstr_nused--;
	}
}

// return C-string length from kstr object
// kstr_cstr: kstr|C-string|NULL
int kstr_len(char *s_kstr_cstr)
{
	return (s_kstr_cstr != NULL)? ( strlen(kstr_sp(s_kstr_cstr)) ) : 0;
}

// will kstr_free() cs2 argument
char *kstr_cat(char *s1, const char *cs2)
{
	char *s2 = (char *) cs2;
    kstring_t *s1k = kstr_is(s1);
    //kstring_t *s2k = kstr_is(s2);
	char *s1p, *s1c;
	int slen = kstr_len(s1) + kstr_len(s2) + SPACE_FOR_NULL;
	//printf("kstr_cat s1=%s s2=%s\n", kstr_what(s1), kstr_what(s2));
	
	if (s1k != NULL) {
	    // s1 is a kstr
	    s1 = kstr_malloc(KSTR_REALLOC, s1, slen);
	    s1p = kstr_sp(s1);
	} else
	if (s1 != NULL) {
	    // s1 is a C-string
	    s1c = s1;
	    s1 = kstr_malloc(KSTR_ALLOC, NULL, slen);
	    s1p = kstr_sp(s1);
	    strcpy(s1p, s1c);       // safe since lengths already checked and space allocated
	} else {
	    // s1 is NULL
	    s1 = kstr_malloc(KSTR_ALLOC, NULL, slen);
	    s1p = kstr_sp(s1);
		s1p[0] = '\0';
	}
	
	if (s2) {
		strcat(s1p, kstr_sp(s2));       // safe since lengths already checked and space allocated
		kstr_free(s2);
	}
	
	return s1;
}


////////////////////////////////
// misc string functions
////////////////////////////////

void kiwi_get_chars(char *field, char *value, size_t size)
{
	memcpy(value, field, size);
	value[size] = '\0';
}

void kiwi_set_chars(char *field, const char *value, const char fill, size_t size)
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

void kiwi_str_unescape_quotes(char *str)
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

typedef struct {
    const char c;
    const char *rep;
} esc_HTML_t;

static esc_HTML_t esc_HTML[] = {
    '<', "&lt;",
    '>', "&gt;",
    '&', "&amp;",
    '"', "&quot;",
    '\'', "&apos;"
};

// slow, but doesn't matter given who the current users are
char *kiwi_str_escape_HTML(char *str)
{
    int i, n;
	char *s, *o, *sn;
	esc_HTML_t *esc;
	
	n = 0;
	for (s = str; *s != '\0'; s++) {
	    for (esc = esc_HTML; esc < ARRAY_END(esc_HTML); esc++) {
            if (*s == esc->c) {
                n += strlen(esc->rep) - 1;
                break;
            }
        }
    }
    
    if (n == 0) return NULL;
	sn = (char *) malloc(strlen(str) + n + SPACE_FOR_NULL);
	o = sn;

	for (s = str; *s != '\0'; s++) {
	    for (esc = esc_HTML; esc < ARRAY_END(esc_HTML); esc++) {
            if (*s == esc->c) {
                o = stpcpy(o, esc->rep);
                break;
            }
        }
        if (esc == ARRAY_END(esc_HTML)) {
            if (isprint(*s))
                *o++ = *s;
        }
    }
	
	*o = '\0';
	return sn;
}

char *kiwi_str_encode(char *src)
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

#define N_DST_STATIC (1023 + SPACE_FOR_NULL)
static char dst_static[N_DST_STATIC];

// for use with e.g. an immediate printf argument
char *kiwi_str_encode_static(char *src)
{
	if (src == NULL) src = (char *) "null";		// JSON compatibility
	mg_url_encode(src, dst_static, N_DST_STATIC);
	return dst_static;
}

char *kiwi_str_decode_inplace(char *src)
{
	if (src == NULL) return NULL;
	int slen = strlen(src);
	char *dst = src;
	// dst = src is okay because length dst always <= src since we are decoding
	// yes, mg_url_decode() dst length includes SPACE_FOR_NULL
	mg_url_decode(src, slen, dst, slen + SPACE_FOR_NULL, 0);
	return dst;
}

// for use with e.g. an immediate printf argument
char *kiwi_str_decode_static(char *src)
{
	if (src == NULL) return NULL;
	// yes, mg_url_decode() dst length includes SPACE_FOR_NULL
	mg_url_decode(src, strlen(src), dst_static, N_DST_STATIC, 0);
	return dst_static;
}

// FIXME: do something better
char *kiwi_str_clean(char *str)
{
    char *s = str;

    for (; *s != '\0'; s++) {
        if (*s == '\'') *s = ' '; else
        if (*s == '"') *s = ' '; else
        if (*s == '\\') *s = ' '; else
        if (*s == '<') *s = ' '; else
        if (*s == '>') *s = ' '; else
        if (*s == '&') *s = ' ';

        if (isprint(*s)) continue;
        *s = ' ';
    }

    return str;
}

int kiwi_str2enum(const char *s, const char *strs[], int len)
{
	int i;
	for (i=0; i<len; i++) {
		if (strcasecmp(s, strs[i]) == 0) return i;
	}
	return NOT_FOUND;
}

const char *kiwi_enum2str(int e, const char *strs[], int len)
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

bool kiwi_str_begins_with(char *s, const char *cs)
{
    int slen = strlen(cs);
    return (strncmp(s, cs, slen) == 0);
}

char *kiwi_str_ends_with(char *s, const char *cs)
{
    int slen = strlen(cs);
    char *sp = s + strlen(s) - slen;
    return (strncmp(sp, cs, slen) == 0)? sp : NULL;
}

char *kiwi_skip_over(char *s, const char *skip)
{
    int slen = strlen(skip);
    bool match = (strncmp(s, skip, slen) == 0);
    return match? (s + slen) : s;
}

// library strcpy() with overlapping args will trigger clang asan
char *kiwi_overlap_strcpy(char *dst, const char *src)
{
    char *d = dst, c;
    do {
        c = *src++;
        *d++ = c;
    } while (c != '\0');
    return dst;
}


// versions of strncpy/strncat that guarantee string terminated when max size reached
// assumes n has included SPACE_FOR_NULL
// assume that truncated s2 does no harm (e.g. is not interpreted as an unintended cmd or something)

char *kiwi_strncpy(char *dst, const char *src, size_t n)
{
    char *rv = strncpy(dst, src, n);
    rv[n-1] = '\0';     // truncate src if necessary
    return rv;
}

char *kiwi_strncat(char *dst, const char *src, size_t n)
{
    n -= strlen(dst) - SPACE_FOR_NULL;
    char *rv = strncat(dst, src, n);
    // remember that strncat() "adds not more than n chars, then a terminating \0"
    return rv;
}

// SECURITY: zeros stack vars
bool kiwi_sha256_strcmp(char *str, const char *key)
{
    SHA256_CTX ctx;
    sha256_init(&ctx);

    int str_len = strlen(str);
    sha256_update(&ctx, (BYTE *) str, str_len);
    BYTE str_bin[SHA256_BLOCK_SIZE];
    sha256_final(&ctx, str_bin);
    bzero(&ctx, sizeof(ctx));

    char str_s[SHA256_BLOCK_SIZE*2 + SPACE_FOR_NULL];
    mg_bin2str(str_s, str_bin, SHA256_BLOCK_SIZE);
    bzero(str_bin, sizeof(str_bin));
    
    int r = strcmp(str_s, key);
    //printf("kiwi_sha256_strcmp: %s %s %s r=%d\n", str, str_s, key, r);
    bzero(str_s, sizeof(str_s));
    return r;
}

kstr_t *kstr_asprintf(kstr_t *ks, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	char *sb;
	vasprintf(&sb, fmt, ap);
	ks = kstr_cat(ks, kstr_wrap(sb));
    return ks;
}

kstr_t *kstr_list_int(const char *head, const char *fmt, const char *tail, int *list, int nlist, int *qual, int bias)
{
    kstr_t *ks = NULL;
    bool first = true;
    
    if (head) ks = (kstr_t *) head;
    
    for (int i = 0; i < nlist; i++) {
        if (qual != NULL && !qual[i]) continue;
        ks = kstr_asprintf(ks, stprintf("%s%s", first? "":",", fmt), list[i] + bias);
        first = false;
    }

    if (tail) ks = kstr_cat(ks, tail);
    return ks;
}


// string hashing used for command processing

// hash functions: seems like nothing beats simple character summation

#define STR_HASH_FUNC(c,i)  hash += c;
//str_hash_init(snd): entries=20 hash_len=4 maxval=0x01c0 bits_required=9 mask=0x01ff lookup_table_size=512
//str_hash_init(wf): entries=7 hash_len=2 maxval=0x00e3 bits_required=8 mask=0x00ff lookup_table_size=256
//str_hash_init(rx_common_cmd): entries=30 hash_len=3 maxval=0x014f bits_required=9 mask=0x01ff lookup_table_size=512

//#define STR_HASH_FUNC(c,i)  hash += (i&1)? (~c & 0x7f) : c;
//str_hash_init(snd): entries=20 hash_len=4 maxval=0x0144 bits_required=9 mask=0x01ff lookup_table_size=512
//str_hash_init(wf): entries=7 hash_len=2 maxval=0x0082 bits_required=8 mask=0x00ff lookup_table_size=256
//str_hash_init(rx_common_cmd): no unique hashes within max_hash_len=10 limit, collisions=7

//#define STR_HASH_FUNC(c,i)  hash = (hash << 1) ^ c;
//str_hash_init(snd): no unique hashes within max_hash_len=8 limit, collisions=2

//#define STR_HASH_FUNC(c,i)  hash += (i&1)? ((c >> 4) | ((c & 0xf) << 4)) : c;
//str_hash_init(snd): entries=20 hash_len=3 maxval=0x022d bits_required=10 mask=0x03ff lookup_table_size=1024
//str_hash_init(wf): entries=7 hash_len=2 maxval=0x0157 bits_required=9 mask=0x01ff lookup_table_size=512
//str_hash_init(rx_common_cmd): entries=30 hash_len=6 maxval=0x0363 bits_required=10 mask=0x03ff lookup_table_size=1024

//#define STR_HASH_FUNC(c,i)  hash += (i&1)? (c << 1) : c;
//str_hash_init(snd): entries=20 hash_len=3 maxval=0x022e bits_required=10 mask=0x03ff lookup_table_size=1024
//str_hash_init(wf): entries=7 hash_len=2 maxval=0x0156 bits_required=9 mask=0x01ff lookup_table_size=512
//str_hash_init(rx_common_cmd): no unique hashes within max_hash_len=10 limit, collisions=7

void str_hash_init(const char *id, str_hash_t *hashp, str_hashes_t *hashes, bool debug)
{
    int i, hash_len, cidx;
    u4_t maxval;
    bool okay;
    
    if (hashp->init) return;
    hashp->init = true;
    hashp->id = id;
    hashp->max_hash_len = strlen(hashes[1].name);
    
    // skip hashes[0] entry because that contains key = HASH_MISS = 0
    
    // increase hash length (#chars summed from end of strings) until hashes become unique
    for (hash_len = 1; hash_len <= hashp->max_hash_len; hash_len++) {
        int hash_start = hashp->max_hash_len-hash_len;
        for (str_hashes_t *h1 = &hashes[1]; h1->name; h1++) {
            u4_t hash = 0;
            for (cidx = hashp->max_hash_len-1; cidx >= hash_start; cidx--) {
                u1_t c = h1->name[cidx];
                STR_HASH_FUNC(c, cidx);
            }
            h1->hash = hash;
        }
    
        okay = true;
        maxval = 0;
        for (str_hashes_t *h1 = &hashes[1]; h1->name && okay; h1++) {
            if (h1->hash > maxval) maxval = h1->hash;
            for (str_hashes_t *h2 = h1+1; h2->name && okay; h2++) {
                if (h1 != h2 && h1->hash == h2->hash) {
                    okay = false;
                }
            }
        }
        if (okay) break;
    }
    
    if (!okay) {
        printf("str_hash_init(%s): hash failure information follows\n", id);
        
        for (hash_len = 1; hash_len <= hashp->max_hash_len; hash_len++) {
            int hash_start = hashp->max_hash_len-hash_len;
            printf("str_hash_init(%s): hash_len=%d --------------------------------\n", id, hash_len);
            for (str_hashes_t *h1 = &hashes[1]; h1->name; h1++) {
                u4_t hash = 0;
                for (cidx = hashp->max_hash_len-1; cidx >= hash_start; cidx--) {
                    u1_t c = h1->name[cidx];
                    STR_HASH_FUNC(c, cidx);
                }
                h1->hash = hash;
            }

            int collisions = maxval = 0;
            for (str_hashes_t *h1 = &hashes[1]; h1->name; h1++) {
                if (h1->hash > maxval) maxval = h1->hash;
                for (str_hashes_t *h2 = h1+1; h2->name; h2++) {
                    if (h1 != h2 && h1->hash == h2->hash) {
                        collisions++;
                        printf("str_hash_init(%s): HASH COLLISION \"%s\"(0x%04x) == \"%s\"(0x%04x) [\"%s\", \"%s\"]\n",
                            id, h1->name + hash_start, h1->hash, h2->name + hash_start, h2->hash, h1->name, h2->name);
                    }
                }
            }

            for (str_hashes_t *h = &hashes[1]; h->name; h++) {
                printf("str_hash_init(%s): key=%d hash=0x%04x \"%s\"\n", id, h->key, h->hash, h->name);
            }

            printf("str_hash_init(%s): for hash_len=%d collisions=%d maxval=%d bits_required=%d\n",
                id, hash_len, collisions, maxval, bits_required(maxval));
        }

        printf("str_hash_init(%s): no unique hashes within max_hash_len=%d limit\n",
            id, hashp->max_hash_len);
        panic("str_hash_init");
    }
    
    // the maximum hash value determines how large the lookup table must be
    int bits = bits_required(maxval);
    hashp->hashes = hashes;
    hashp->hash_len = hash_len;
    hashp->lookup_table_size = 1 << bits;
    int bsize = hashp->lookup_table_size * sizeof(u2_t);
    hashp->keys = (u2_t *) malloc(bsize);
    memset(hashp->keys, 0, bsize);
    int entries = 0;
    for (str_hashes_t *h = &hashes[1]; h->name; h++, entries++) {
        hashp->keys[h->hash] = h->key;
    }

    printf("str_hash_init(%s): entries=%d hash_len=%d maxval=0x%04x bits_required=%d mask=0x%04x lookup_table_size=%d\n",
        id, entries, hash_len, maxval, bits, ((1 << bits) - 1), hashp->lookup_table_size);
    if (debug) {
        for (str_hashes_t *h = &hashes[1]; h->name; h++) {
            printf("str_hash_init(%s): key=%d hash=0x%04x \"%s\"\n", id, h->key, h->hash, h->name);
        }
    }
}

u2_t str_hash_lookup(str_hash_t *hashp, char *str, bool debug)
{
    u2_t hash = 0;
    for (int i = 0; i < hashp->hash_len; i++) {
        int cidx = hashp->max_hash_len-1-i;
        u1_t c = str[cidx];
        STR_HASH_FUNC(c, cidx);
    }
    hashp->cur_hash = hash;
    u2_t key = hashp->keys[hash];

    if (debug) {
        if (key == 0) {
                printf("#### str_hash_lookup(%s): hash_len=%d hash=0x%04x key=0 \"%s\"\n",
                    hashp->id, hashp->hash_len, hash, str);
        } else {
            int n = strncmp(hashp->hashes[key].name, str, hashp->max_hash_len);
            if (n)
                printf("#### str_hash_lookup(%s): hash_len=%d hash=0x%04x key=%d strncmp=%d \"%s\" \"%s\"\n",
                    hashp->id, hashp->hash_len, hash, key, n, hashp->hashes[key].name, str);
        }
    }
    
    return key;
}


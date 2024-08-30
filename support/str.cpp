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

#include "types.h"
#include "config.h"
#include "kiwi.h"
#include "mem.h"
#include "misc.h"
#include "web.h"
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

#define kdebug(x) check(x)
//#define kdebug(x) assert(x)

#if defined(HOST)
    #define KSTRINGS	1024
#else
    #define KSTRINGS	0xfffe
#endif

typedef struct kstring_st {
	#define KS_LAST (KSTRINGS + 1)
    #if KS_LAST > 0xffff
        #error KS_LAST does not fit in u2_t!
    #endif
	u2_t next_free;
	#define KS_VALID 1
	#define KS_EXT_MALLOCED 2
	u2_t flags;
	char *sp;
	int size;
} kstring_t;

static kstring_t kstrings[KSTRINGS];
u2_t kstr_next_free;
int kstr_nused, kstr_hiwat;

char ASCII[256][8];

void kstr_init()
{
    int i;
    kstring_t *ks;
    
	for (i = 0, ks = kstrings; ks < &kstrings[KSTRINGS-1]; i++, ks++) {
	    ks->next_free = i+1;
	}
	
	ks->next_free = KS_LAST;
	kstr_next_free = 0;
	
	// init ASCII[]
	for (i = 0; i < 32; i++) kiwi_snprintf_buf(ASCII[i], "^%c", '@'+i);
	kiwi_snprintf_buf(ASCII[ 8], "\\b");
	kiwi_snprintf_buf(ASCII[ 9], "\\t");
	kiwi_snprintf_buf(ASCII[10], "\\n");
	kiwi_snprintf_buf(ASCII[13], "\\r");
	kiwi_snprintf_buf(ASCII[27], "\\e");
	for (i = 32; i < 0x7f; i++) kiwi_snprintf_buf(ASCII[i], "%c", i);
	for (i = 0x7f; i <= 0xff; i++) kiwi_snprintf_buf(ASCII[i], "\\x%2X", i);
	
	#if 0
        for (i = 0; i < 256; i++) {
            real_printf("%s ", ASCII[i]);
            if ((i & 7) == 7) real_printf("\n");
        }
        real_printf("\n");

        // shows that C \xhh escapes work for constructing UTF-8 chars
        u1_t x[8]; x[0] = 'A'; x[1] = 0xC3; x[2] = 0x83; x[3] = 0xC2; x[4] = 0xA4; x[5] = 'B'; x[6] = 0;
        real_printf("UTF-8 demo:\nVÃ¤sterÃ¥s #1\n%s\n%s\n%s\n%s\n\n", "VÃ¤sterÃ¥s #2",
            "V\xC3\x83\xC2\xA4ster\xC3\x83\xC2\xA5s #3",
            kiwi_str_ASCII_static((char *) "V\xC3\x83\xC2\xA4ster\xC3\x83\xC2\xA5s #4"),
            (char *) x
            );
        
        kiwi_exit(0);
    #endif
}

static kstring_t *kstr_obj(char *s_kstr_cstr)
{
    // implicit: if (s_kstr_cstr == NULL) return NULL
	kstring_t *ks = (kstring_t *) s_kstr_cstr;
	if (ks >= kstrings && ks < &kstrings[KSTRINGS]) {
	    kdebug(ks->flags & KS_VALID);
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
	    ks = kstr_obj(s_kstr_cstr);
        kdebug(ks != NULL);
        kdebug(ks->sp != NULL);
        kdebug(size >= 0);
        ks->sp = (char *) kiwi_irealloc("kstr_malloc", ks->sp, size);
        ks->size = size;
        return (char *) ks;
	}
	
	ks = &kstrings[kstr_next_free];
	kstr_next_free = ks->next_free;
	if (kstr_next_free == KS_LAST) {
	    printf("out of kstrs: KSTRINGS=%d(0x%x) increase?\n", KSTRINGS, KSTRINGS);
	    panic("kstr_malloc");
	}
	kstr_nused++;
	if (kstr_nused > kstr_hiwat) {
	    kstr_hiwat = kstr_nused;
	    //real_printf("kstr: hiwat=%d\n", kstr_hiwat);
	}
    ks->flags = KS_VALID;
	
    if (type == KSTR_EXT_MALLOC) {
        kdebug(s_kstr_cstr != NULL);
        kdebug(!kstr_obj(s_kstr_cstr));
        kdebug(size == 0);
        size = strlen(s_kstr_cstr) + SPACE_FOR_NULL;
        //printf("%3d ALLOC %4d %p {%p} EXT <%s>\n", ks-kstrings, size, ks, s_kstr_cstr, s_kstr_cstr);
        ks->flags |= KS_EXT_MALLOCED;
    } else {    // type == KSTR_ALLOC
        kdebug(s_kstr_cstr == NULL);
        kdebug(size >= 0);
        s_kstr_cstr = (char *) kiwi_imalloc("kstr_malloc", size);
        s_kstr_cstr[0] = '\0';
        //printf("%3d ALLOC %4d %p {%p}\n", ks-kstrings, size, ks, s_kstr_cstr);
    }

    ks->sp = s_kstr_cstr;
    ks->size = size;
    return (char *) ks;
}

// debugging only -- caller should really free
static char *kstr_what(char *s_kstr_cstr)
{
	char *p;
	
	if (s_kstr_cstr == NULL) return (char *) "NULL";
	kstring_t *ks = kstr_obj(s_kstr_cstr);
	if (ks) {
		asprintf(&p, "#%d:%d/%d|%p|{%p}%s",
			(int) (ks-kstrings), ks->size, (int) strlen(ks->sp), ks, ks->sp, (ks->flags & KS_EXT_MALLOCED)? "-EXT":"");
	} else {
		asprintf(&p, "%p", s_kstr_cstr);
	}
	return p;
}

// return C-string pointer from kstr object
// s_kstr_cstr: kstr|C-string|NULL
char *kstr_sp(char *s_kstr_cstr)
{
	kstring_t *ks = kstr_obj(s_kstr_cstr);
	
	if (ks) {
	    kdebug(ks->flags & KS_VALID);
		return ks->sp;
	} else {
		return s_kstr_cstr;
	}
}

// return C-string pointer from kstr object less trailing newline
// s_kstr_cstr: kstr|C-string|NULL
char *kstr_sp_less_trailing_nl(char *s_kstr_cstr)
{
	char *sp = kstr_sp(s_kstr_cstr);
	if (sp == NULL) return NULL;
	int sl = strlen(sp);
	if (sl >= 1 && (sp[sl-1] == '\n' || sp[sl-1] == '\r')) sp[sl-1] = '\0';
	if (sl >= 2 && (sp[sl-2] == '\n' || sp[sl-2] == '\r')) sp[sl-2] = '\0';
	return sp;
}

// wrap a malloc()'d C-string in a kstr object so it is auto-freed later on
char *kstr_wrap(char *s_malloced)
{
	if (s_malloced == NULL) return NULL;
	kdebug(!kstr_obj(s_malloced));
	return kstr_malloc(KSTR_EXT_MALLOC, s_malloced, 0);
}

// Only frees a kstr object, will not free a malloc()'d C-string unless kstr_wrap()'d.
// It is normal that this routine might be called with a C-string in code that is freeing
// a mix of kstr and C-strings (wrapped or not).
// s_kstr_cstr: kstr|C-string|NULL
typedef enum { KSTR_FREE_NORM, KSTR_FREE_RTN_MALLOC } kstr_free_e;

static char *_kstr_free(char *s_kstr_cstr, kstr_free_e mode)
{
	if (s_kstr_cstr == NULL) return NULL;
	char *rv = NULL;
	
	kstring_t *ks = kstr_obj(s_kstr_cstr);
	
	if (ks) {
	    kdebug(ks->flags & KS_VALID);
		//printf("%3d  FREE %4d %p {%p} %s\n", ks-kstrings, ks->size, ks, ks->sp, (ks->flags & KS_EXT_MALLOCED)? "EXT":"");
		if (mode == KSTR_FREE_RTN_MALLOC) {
		    rv = ks->sp;
		} else
		if (ks->flags & KS_EXT_MALLOCED) {
		    free((char *) ks->sp);      // wasn't allocated with kiwi_imalloc()
		} else {
		    kiwi_ifree((char *) ks->sp, "kstr_free");
		}
		ks->sp = NULL;
		ks->size = 0;
		ks->flags = 0;
		ks->next_free = kstr_next_free;
		kstr_next_free = ks - kstrings;
		kstr_nused--;
	}
	
	return rv;
}

void  kstr_free(char *s_kstr_cstr) { _kstr_free(s_kstr_cstr, KSTR_FREE_NORM); }
char *kstr_free_return_malloced(char *s_kstr_cstr) { return _kstr_free(s_kstr_cstr, KSTR_FREE_RTN_MALLOC); }

// return C-string length from kstr object
// s_kstr_cstr: kstr|C-string|NULL
int kstr_len(char *s_kstr_cstr)
{
    if (s_kstr_cstr == NULL) return 0;
    
	kstring_t *ks = kstr_obj(s_kstr_cstr);
    if (ks) {
        int size = ks->size - SPACE_FOR_NULL;
        //#define KSTR_LEN_CHK_SLEN
        #ifdef KSTR_LEN_CHK_SLEN
            int check_size = strlen(ks->sp);
            if (size != check_size) {
                printf("#### DANGER kstr_len: %p size(%d) != check_size(%d)\n", s_kstr_cstr, size, check_size);
                return check_size;
            }
        #endif
        return size;
    } else {
	    return strlen(s_kstr_cstr);
	}
}

// will kstr_free() cs2 argument
char *kstr_cat(char *s1, const char *cs2, int *_slen)
{
	char *s2 = (char *) cs2;
	char *s1k, *s1p, *s1c;
	int slen = kstr_len(s1) + kstr_len(s2) + SPACE_FOR_NULL;
	//printf("kstr_cat s1=%s s2=%s\n", kstr_what(s1), kstr_what(s2));
	
	if (kstr_obj(s1) != NULL) {
	    // s1 is a kstr
	    check(s1 != s2);    // better not be same two kstrs
	    s1k = kstr_malloc(KSTR_REALLOC, s1, slen);
	    s1p = kstr_sp(s1k);
	} else
	if (s1 != NULL) {
	    // s1 is a C-string
	    s1c = s1;
	    s1k = kstr_malloc(KSTR_ALLOC, NULL, slen);
	    s1p = kstr_sp(s1k);
	    strcpy(s1p, s1c);       // safe since lengths already checked and space allocated
	} else {
	    // s1 is NULL
	    s1k = kstr_malloc(KSTR_ALLOC, NULL, slen);
	    s1p = kstr_sp(s1k);
		s1p[0] = '\0';
	}
	
	if (s2) {
		strcat(s1p, kstr_sp(s2));       // safe since lengths already checked and space allocated
		kstr_free(s2);
	}
	
	if (_slen != NULL) *_slen = kstr_len(s1k);
	return s1k;
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
	kdebug(slen <= size);
	memset(field, (int) fill, size);
	memcpy(field, value, strlen(value));
}

bool kiwi_nonEmptyStr(const char *s)
{
    return (s != NULL && s[0] != '\0');
}

// Version of strsep() that handles delimiters embedded inside double-quotes.
// Also recognizes the spreadsheet standard of escaping quoted double-quotes by doubling them up.
static char *ed_strsep(char **sp, const char *delim)
{
    char *osp;
    if (sp == NULL || *sp == NULL || delim == NULL) return NULL;
    osp = *sp;
    
    if (strlen(delim) == 1) {
        char *cp = osp, c;
        int quoted = 0;
        do {
            c = *cp;
            if (c == '"') {
                if (*(cp+1) == '"') {
                    cp++;           // skip over doubled double-quote escapes
                } else {
                    quoted ^= 1;
                    //printf("ed_strsep FLIP quoted=%d\n", quoted);
                }
            } else

            if (c == *delim) {
                //printf("ed_strsep DELIM quoted=%d\n", quoted);
                if (!quoted) {      // only perform the delimiter action if not quoted
                    *cp = '\0';
                    *sp = cp+1;
                    c = '\0';       // terminate loop
                }
            } else

            if (c == '\0') {
                *sp = NULL;
            }
            cp++;
        } while (c != '\0');
    } else {
        osp = strsep(sp, delim);
    }
    
    return osp;
}

// Makes a copy of ocp since delimiters are turned into NULLs.
// If ocp begins (or ends) with delimiter(s) null entries are _not_ made in argv
// (and reflected in returned count), *unless* the KSPLIT_NO_SKIP_EMPTY_FIELDS flag is given.
// Unlike strsep() delimiter hit is returned in str_split_t
// Caller must free *mbuf
int kiwi_split(char *ocp, char **mbuf, const char *delims, str_split_t argv[], int nargs, int flags)
{
	int n = 0;
	//bool dbg = (strcmp(delims, "\r\n") == 0);
	bool ed = ((flags & KSPLIT_HANDLE_EMBEDDED_DELIMITERS) != 0);
	str_split_t *ap;
	char *tp;
	*mbuf = (char *) kiwi_imalloc("kiwi_split", strlen(ocp) + SPACE_FOR_NULL);
	strcpy(*mbuf, ocp);
	tp = *mbuf;
	//if (dbg) real_printf("kiwi_split <%s> <%s>\n", delims, ocp);
	
	for (ap = argv; (ap->str = (ed? ed_strsep(&tp, delims) : strsep(&tp, delims))) != NULL;) {
	    bool not_empty = (ap->str[0] != '\0');
		if ((flags & KSPLIT_NO_SKIP_EMPTY_FIELDS) || not_empty) {
            ap->delim = (not_empty && tp)? ocp[(tp - *mbuf) - 1] : '\0';
            //if (dbg) real_printf("kiwi_split %d <%s> <%s>\n", n, ap->str, ASCII[ap->delim]);
			n++;
			if (++ap >= &argv[nargs])
				break;
		}
	}
	
	return n;
}

char *kiwi_str_replace(char *s, const char *from, const char *to, bool *caller_must_free)
{
    char *fp;
    if ((fp = strstr(s, from)) == NULL) return NULL;
    
    int flen = strlen(from);
    int tlen = strlen(to);
    
    if (tlen <= flen) {
        do {
            strncpy((char *) fp, to, tlen);
            int rlen = strlen(fp+flen) + 1;
            memmove((char *) fp+tlen, fp+flen, rlen);
        } while ((fp = strstr(s, from)) != NULL);
        if (caller_must_free) *caller_must_free = false;
    } else {
        bool first = true;
        do {
            char *ns;
            asprintf(&ns, "%.*s%s%s", (int) (fp-s), s, to, fp+flen);
            if (!first) kiwi_asfree(s, "kiwi_str_replace");
            first = false;
            s = ns;
        } while ((fp = strstr(s, from)) != NULL);
        if (caller_must_free) *caller_must_free = true;
    }
    
    return (char *) s;
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

char *kiwi_json_to_html(char *str, bool doBR)
{
	char *ostr, *s, *o;
	int sl = strlen(str);
	int n = sl;
	bool doFree, mustCopy = false;
	
	for (s = str; *s != '\0';) {
		if (*s == '\\' && *(s+1) == '"') {
			n--;        // \" => "
			s++;
		} else
		if (*s == '\\' && *(s+1) == 'n') {
		    if (doBR) {
                n += 2;     // \\n => <br>
                s++;
                // because larger output would overwrite input (they're same buffer otherwise)
                mustCopy = true;
            } else {
                #if 0
                    n -= 2;     // \\n => (removed)
                #else
                    n--;        // \\n (2-char) => \n (1-char)
                #endif
                s++;
            }
		} else
		if (*s == '\\' && *(s+1) == 't') {
		    n += 2;     // \\t => "    " (4-chars)
		}
		s++;
	}
	
	if (n > sl || mustCopy) {
        o = ostr = (char *) kiwi_imalloc("kiwi_json_to_html", n + SPACE_FOR_NULL);
        doFree = true;
	} else {
	    o = ostr = str;
	    doFree = false;
	}

	for (s = str; *s != '\0';) {
		if (*s == '\\' && *(s+1) == '"') {
			*o++ = '"';     // \\" => "
			s += 2;
		} else
		if (*s == '\\' && *(s+1) == 'n') {
		    if (doBR) {
                strncpy(o, "<br>", 4);
                o += 4;     // \\n => <br>
                s += 2;
            } else {
                #if 0
                    s += 2;     // leave \\n intact
                #else
                    *o++ = '\n';    // \\n (2-char) => \n (1-char)
                    s += 2;
                #endif
            }
        } else
		if (*s == '\\' && *(s+1) == 't') {
                strncpy(o, "    ", 4);
                o += 4;     // \\t => "    " (4-chars)
                s += 2;
		} else {
			*o++ = *s++;
		}
	}

	*o = '\0';
	//printf("kiwi_json_to_html1 %p %d<%s>\n", str, sl, str);
	if (doFree) {
	    //printf("kiwi_json_to_html doFree\n");
	    kiwi_ifree(str, "kiwi_json_to_html");
	}
	//printf("kiwi_json_to_html2 %p %d<%s>\n", ostr, n, ostr);
	return ostr;    // caller must kiwi_ifree()
}

char *kiwi_json_to_string(char *str)
{
	char *s, *o = str;

	for (s = str; *s != '\0';) {
		if (*s == '\\' && *(s+1) == '"') {
			*o++ = '"';     // \" => "
			s += 2;
		} else {
			*o++ = *s++;
		}
	}

	*o = '\0';
	return str;
}

// inplace is okay because we are only ever shortening the string
void kiwi_remove_unprintable_chars_inplace(char *str, int *printable, int *UTF)
{
	if (printable) *printable = 0;
	if (UTF) *UTF = 0;

    char *o = str;
	for (char *s = str; *s != '\0'; s++) {
	    int c = *((u1_t *) s);
	    bool ok = ((isprint(c) && (!isspace(c) || c == ' ')) || c >= 0x80);
        //printf("<%c>%02x ok=%d isspace=%d\n", c, c, ok, isspace(*s));
	    if (ok) *o++ = c;
	    
        if (printable && ok) *printable = *printable + 1;
        if (UTF && c >= 0x80) *UTF = *UTF + 1;
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
// NB: _caller_ must free str if returned value is non-NULL
char *kiwi_str_escape_HTML(char *str, int *printable, int *UTF)
{
    int i, n;
	char *s;
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
    
    if (n == 0) {
        kiwi_remove_unprintable_chars_inplace(str, printable, UTF);
        return NULL;
    }
    
	char *sn = (char *) kiwi_imalloc("kiwi_str_escape_HTML", strlen(str) + n + SPACE_FOR_NULL);
	char *o = sn;

	for (s = str; *s != '\0'; s++) {
	    for (esc = esc_HTML; esc < ARRAY_END(esc_HTML); esc++) {
            if (*s == esc->c) {
                o = stpcpy(o, esc->rep);
                break;
            }
        }
        if (esc == ARRAY_END(esc_HTML))
            *o++ = *s;
    }
	
	*o = '\0';
    kiwi_remove_unprintable_chars_inplace(sn, printable, UTF);
	return sn;
}

// unlike mg_url_encode, never encodes any chars between [' ', '~']
static void kiwi_fewer_encode(const char *src, char *dst, size_t dst_len) {
  static const char *hex = "0123456789abcdef";
  const char *end = dst + dst_len - 1;

  for (; *src != '\0' && dst < end; src++, dst++) {
    if (*src >= ' ' && *src <= '~') {
      *dst = *src;
    } else if (dst + 3 < end) {
      dst[0] = '<';
      dst[1] = hex[(* (const unsigned char *) src) >> 4];
      dst[2] = hex[(* (const unsigned char *) src) & 0xf];
      dst[3] = '>';
      dst += 3;
    } else break;	// KiwiSDR: for valgrind, don't leave uninitialized bytes in string
  }

  *dst = '\0';
}

char *kiwi_str_encode(char *src, const char *from, int flags)
{
	if (src == NULL) src = (char *) "null";		// JSON compatibility
	size_t slen = strlen(src);
	
	#define ENCODE_EXPANSION_FACTOR 3		// c -> %xx
	size_t dlen = (slen * ENCODE_EXPANSION_FACTOR) + SPACE_FOR_NULL;
	dlen++;     // required by mongoose 7 mg_url_encode()
	//bool trace = (slen == 1 && src[0] == ' ');

	// don't use kiwi_malloc() due to large number of these simultaneously active from dx list
	// and also because dx list has to use kiwi_asfree() due to related allocations via strdup()
	check(dlen != 0);
	char *dst = (char *) ((flags & USE_MALLOC)? malloc(dlen) : kiwi_imalloc(from? from : "kiwi_str_encode", dlen));
	if (flags & FEWER_ENCODED)
	    kiwi_fewer_encode(src, dst, dlen);
	else
	    mg_url_encode(src, slen, dst, dlen);
	//if (trace) printf("kiwi_str_encode flags=0x%x slen=%d <%s> dlen=%d\n", flags, slen, src, dlen);
	//if (trace) printf("kiwi_str_encode dlen=%d <%s>\n", dlen, dst);
	return dst;		// NB: caller must kiwi_ifree(dst)
}

#define N_DST_STATIC 4
#define N_DST_STATIC_BUF (511 + SPACE_FOR_NULL)
static char dst_static[N_DST_STATIC][N_DST_STATIC_BUF];

// for use with e.g. an immediate printf argument
char *kiwi_str_ASCII_static(char *src, int which)
{
	if (src == NULL) return NULL;
	check(which < N_DST_STATIC);
	int i, sl = strlen(src), dl = 0;
	char *dp = dst_static[which];
	*dp = '\0';
	for (char *sp = src; *sp; sp++) {
        kiwi_strncat(dp, ASCII[*sp], N_DST_STATIC_BUF);
        dp += strlen(ASCII[*sp]);
	}
	
	return dst_static[which];
}

// for use with e.g. an immediate printf argument
char *kiwi_str_encode_static(char *src, int flags)
{
	if (src == NULL) src = (char *) "null";		// JSON compatibility
	size_t slen = strlen(src);
	check((slen * ENCODE_EXPANSION_FACTOR) < N_DST_STATIC_BUF);

	if (flags & FEWER_ENCODED)
	    kiwi_fewer_encode(src, dst_static[0], N_DST_STATIC_BUF);
	else
	    mg_url_encode(src, slen, dst_static[0], N_DST_STATIC_BUF);
	return dst_static[0];
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
char *kiwi_str_decode_static(char *src, int which)
{
	if (src == NULL) return NULL;
	check(which < N_DST_STATIC);
	// yes, mg_url_decode() dst length includes SPACE_FOR_NULL
	mg_url_decode(src, strlen(src), dst_static[which], N_DST_STATIC_BUF, 0);
	return dst_static[which];
}

// = 1 encode only if fewer_encoded == false
// = 2 always encode
static u1_t decode_table[128] = {
//  (ctrl)                              always % encoded: 00 - 1f
    2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,

//  (ctrl)
    2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,

//    ! " # $ % & ' ( ) * + , - . /     still % encoded: "22 %25 &26 '27 +2b
    0,0,2,0,0,2,1,1,0,0,0,1,0,0,0,0,

//  0 1 2 3 4 5 6 7 8 9 : ; < = > ?     still % encoded: ;3b <3c >3e
    0,0,0,0,0,0,0,0,0,0,0,1,1,0,1,0,

//  @ (alpha)
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,

//  (alpha)               [ \ ] ^ _     still % encoded: \5c
    0,0,0,0,0,0,0,0,0,0,0,0,2,0,0,0,

//  ` (alpha)                           still % encoded: `60
    1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,

//  (alpha)               { | } ~ del   still % encoded: del 7f
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2,
};

// Like mg_url_decode(), but only decodes chars not likely to cause trouble.
// Used to enhance readability of the kiwi.config/{admin,kiwi,dx}.json files.
//
// This is typically called on a fully-encoded string (via encodeURIComponent() or mg_url_encode())
// before it is saved in one of the .json files.
//
// Although having the flag FEWER_ENCODED in the context of a _decode_ function doesn't seem to
// make sense here's what it means (FEWER_ENCODED is of course used in e.g. kiwi_str_encode())
// When FEWER_ENCODED is set, and a %hh sequence is encountered, the %hh is normally decoded into
// a single char only if not UTF-8 and not any of the decode_table entries.
// Specifying FEWER_ENCODED _also_ decodes chars with a decode_table entry =1  &'+;<>`
// but leaves the =2 and UTF-8 entries %hh encoded.

static void kiwi_url_decode_selective(const char *src, int src_len, char *dst,
    int dst_len, int flags = 0)
{
    int i, j, a, b;
    u1_t c;
    bool fewer_encoded = flags & FEWER_ENCODED;

    //if (fewer_encoded) printf("%s\n", src);
    for (i = j = 0; i < src_len && j < dst_len - 1; i++, j++) {
        //if (fewer_encoded)
            //printf("i%d j%d sl%d dl%d '%c'0x%02x\n", i, j, src_len, dst_len, src[i], src[i]);
        if (src[i] == '%' && i + 2 < src_len &&
            isxdigit(* (const unsigned char *) (src + i + 1)) &&
            isxdigit(* (const unsigned char *) (src + i + 2))) {
            
            a = tolower(* (const unsigned char *) (src + i + 1));
            b = tolower(* (const unsigned char *) (src + i + 2));
            
             // preserve UTF-8 encoding values >= 0x80!
            #define HEXTOI(x) (isdigit(x) ? x - '0' : x - 'W')
            c = (u1_t) (HEXTOI(a) << 4) | HEXTOI(b);
            if (c < 0x80 && (decode_table[c] == 0 || (fewer_encoded && decode_table[c] == 1))) {
                dst[j] = c;
                i += 2;
                //if (fewer_encoded) printf("replace %%xx with: '%c'0x%02x\n", c, c);
            } else {
                dst[j] = '%';
                //if (fewer_encoded && c < 0x80 && decode_table[c] == 2)
                    //printf("=> keeping %% encoding for '%c'0x%02x\n", c, c);
            }
        } else {
            dst[j] = src[i];
        }
    }

    dst[j] = '\0'; // Null-terminate the destination
}

char *kiwi_str_decode_selective_inplace(char *src, int flags)
{
	if (src == NULL) return NULL;
	int slen = strlen(src);
	char *dst = src;
    // dst = src is okay because length dst always <= src since we are decoding
    // yes, kiwi_url_decode_selective() dst length includes SPACE_FOR_NULL
    kiwi_url_decode_selective(src, slen, dst, slen + SPACE_FOR_NULL, flags);
	return dst;
}

// not currently used by anyone
char *kiwi_URL_enc_to_C_hex_esc_enc(char *src)
{
    int i, j, n, a, b;
    int slen = strlen(src);
    char *dst;

	n = 0;
    for (i = 0; i < slen; i++) {
        if (src[i] == '%' && i + 2 < slen &&
            isxdigit(* (const unsigned char *) (src + i + 1)) &&
            isxdigit(* (const unsigned char *) (src + i + 2))) {
            n++;
        }
    }
    
    if (n) {
        dst = (char *) kiwi_imalloc("kiwi_str_escape_HTML", slen + n + SPACE_FOR_NULL);
        char *o = dst;
    
        for (i = 0; i < slen; i++) {
            if (src[i] == '%' && i + 2 < slen &&
                isxdigit(* (const unsigned char *) (src + i + 1)) &&
                isxdigit(* (const unsigned char *) (src + i + 2))) {
    
                a = tolower(* (const unsigned char *) (src + i + 1));
                b = tolower(* (const unsigned char *) (src + i + 2));
                
                #if 0
                    // output \xab
                    *o++ = '\\'; *o++ = 'x';
                    *o++ = (char) c;
                #else
                    // output ab byte value
                    #define HEXTOI(x) (isdigit(x) ? x - '0' : x - 'W')
                    u1_t c = (u1_t) (HEXTOI(a) << 4) | HEXTOI(b);
                    i += 2;
                #endif
            } else {
                *o++ = src[i];
            }
        }
        *o = '\0';
    } else {
        dst = src;
    }
    
    if (n) kiwi_asfree(src);
    
    return dst;
}

static u1_t clean_table[128] = {
//  (ctrl)
    3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,

//  (ctrl)
    3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,

//    ! " # $ % & ' ( ) * + , - . /     not !"#$%'()*
    0,1,1,1,1,1,0,1,1,1,1,0,0,0,0,0,

//  0 1 2 3 4 5 6 7 8 9 : ; < = > ?     not ;<>
    0,0,0,0,0,0,0,0,0,0,0,1,1,0,1,0,

//  @ (alpha)                           not @
    1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,

//  (alpha)               [ \ ] ^ _     not [\]^_
    0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,

//  ` (alpha)                           not `
    1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,

//  (alpha)               { | } ~ del   not {|}~ del
    0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,
};

char *kiwi_str_clean(char *str, int type)
{
    u1_t *s = (u1_t *) str;

    for (; *s != '\0'; s++) {
        if (*s >= 0x80 || (type == KCLEAN_DELETE && (clean_table[*s] & KCLEAN_DELETE))) {
            kiwi_overlap_memcpy(s, s+1, strlen((char *) s+1) + SPACE_FOR_NULL);
            continue;
        }
        if (type == KCLEAN_REPL_SPACE && clean_table[*s] & KCLEAN_REPL_SPACE) *s = ' ';
    }

    return str;
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
    if (s == NULL) return false;
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

// library memcpy() with overlapping args will trigger clang asan
u1_t *kiwi_overlap_memcpy(u1_t *dst, const u1_t *src, int n)
{
    u1_t *d = dst, c;
    for (int i = 0; i < n; i++) {
        c = *src++;
        *d++ = c;
    }
    return dst;
}


// Version of strlen() with limit to handle a corrupt string without proper null-termination.
// Also, unlike strlen(), returns 0 for NULL string pointer.
int kiwi_strnlen(const char *s, int limit)
{
    int i;
    for (i = 0; s && *s && i <= limit; i++)
        s++;
    return i;
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

// From Mongoose mg_vsnprintf()
// Like snprintf(), but never returns negative value, or a value
// that is larger than the supplied buffer.
//
// If setting gdb breakpoints in here must use "noinline" attribute to prevent false breakpoints:
//__attribute__((noinline))
static int kiwi_vsnprintf_int(char *buf, size_t buflen, const char *fmt, va_list ap) {
    int n;
    if (buflen < 1) {
        printf("WARNING kiwi_vsnprintf_int: buflen=%d which is < 1\n", buflen);
        return 0;
    }
    n = vsnprintf(buf, buflen, fmt, ap);
    if (n < 0) {
        n = 0;
    } else
    if (n >= (int) buflen) {
        printf("WARNING kiwi_vsnprintf_int: n=%d buflen=%d RESULT TRUNCATED\n", n, buflen);
        n = (int) buflen - 1;
    }
    buf[n] = '\0';      // ensure string is always terminated
    return n;
}

int _kiwi_snprintf_int(const char *buf, size_t buflen, const char *fmt, ...) {
    int n;

    va_list ap;
    va_start(ap, fmt);
    n = kiwi_vsnprintf_int((char *) buf, buflen, fmt, ap);
    va_end(ap);

    return n;
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
	char *sb = NULL;

	va_list ap;
	va_start(ap, fmt);
	int n = vasprintf(&sb, fmt, ap);
    va_end(ap);

    if (n == -1)
        printf("kstr_asprintf: out of memory?\n");
    else
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
    
    // increase hash length (#chars hashed from end of strings) until hashes become unique
    for (hash_len = 1; hash_len <= hashp->max_hash_len; hash_len++) {
        int hash_start = hashp->max_hash_len - hash_len;
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
    
    #define SHOW_HASH_INFO 0
    if (!okay || SHOW_HASH_INFO) {
        printf("str_hash_init(%s): hash %sinformation follows\n", id, !okay? "failure " : "");
        
        for (hash_len = 1; hash_len <= hashp->max_hash_len; hash_len++) {
            int hash_start = hashp->max_hash_len - hash_len;
            printf("str_hash_init(%s): --- hash_len=%d/%d --------------------------------\n",
                id, hash_len, hashp->max_hash_len);
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
                printf("str_hash_init(%s): key=%02d hash \"%s\"(0x%04x) [\"%s\"]\n",
                    id, h->key, h->name + hash_start, h->hash, h->name);
            }

            printf("str_hash_init(%s): for hash_len=%d collisions=%d maxval=%d bits_required=%d\n",
                id, hash_len, collisions, maxval, bits_required(maxval));
            
            if (!collisions)
                break;
        }

        if (!okay) {
            printf("str_hash_init(%s): no unique hashes within max_hash_len=%d limit\n",
                id, hashp->max_hash_len);
            panic("str_hash_init");
        }
    }
    
    // the maximum hash value determines how large the lookup table must be
    int bits = bits_required(maxval);
    hashp->hashes = hashes;
    hashp->hash_len = hash_len;
    hashp->lookup_table_size = 1 << bits;
    int bsize = hashp->lookup_table_size * sizeof(u2_t);
    hashp->keys = (u2_t *) kiwi_imalloc("str_hash_init", bsize);
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
    
    // The max_hash_len from rx_common_cmd() can be larger than commands from routines
    // that call it. So have to check for strlen(str) >= hashp->max_hash_len here to avoid
    // buffer read overruns which will be picked up by ASAN.
    if (strlen(str) >= hashp->max_hash_len) {
        for (int i = 0; i < hashp->hash_len; i++) {
            int cidx = hashp->max_hash_len-1-i;
            u1_t c = str[cidx];
            STR_HASH_FUNC(c, cidx);
        }
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


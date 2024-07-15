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

// Copyright (c) 2021 John Seamons, ZL4VO/KF6VO

#include "types.h"
#include "config.h"
#include "kiwi.h"
#include "valgrind.h"
#include "mem.h"
#include "misc.h"
#include "str.h"
#include "web.h"
#include "spi.h"
#include "cfg.h"
#include "coroutines.h"
#include "net.h"
#include "debug.h"
#include "security.h"

#include <sys/file.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <setjmp.h>
#include <ctype.h>
#include <syslog.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>
#include <sched.h>

char *current_authkey;

char *kiwi_authkey()
{
	u4_t u[8];
	int n = kiwi_file_read("kiwi_authkey", "/dev/urandom", (char *) u, 32);
	check(n == 32);
	char *s;
	asprintf(&s, "%08x%08x%08x%08x%08x%08x%08x%08x", u[0], u[1], u[2], u[3], u[4], u[5], u[6], u[7]);
	return s;
}

bool admin_pwd_unsafe()
{
    bool unsafe = false;
    if (cfg_int("sdr_hu_dom_sel", NULL, CFG_REQUIRED) != DOM_SEL_REV) return false;

    char *serno_s;
    asprintf(&serno_s, "%d", net.serno);
    const char *apw = admcfg_string("admin_password", NULL, CFG_OPTIONAL);
    bool rev_auto = admcfg_true("rev_auto");
    const char *auto_host = admcfg_string("rev_auto_host", NULL, CFG_OPTIONAL);
    const char *norm_host = admcfg_string("rev_host", NULL, CFG_OPTIONAL);
    const char *host = rev_auto? auto_host : norm_host;

    if (kiwi_nonEmptyStr(host) && strcmp(host, serno_s) == 0 && strcmp(apw, serno_s) == 0) {
        unsafe = true;
    }

    //printf("admin_pwd_unsafe serno=%d apw=<%s> rev_auto=%d auto_host=<%s> norm_host=<%s> unsafe=%d\n",
    //    net.serno, apw, rev_auto, auto_host, norm_host, unsafe);
    admcfg_string_free(serno_s); admcfg_string_free(apw); admcfg_string_free(auto_host); admcfg_string_free(norm_host);
    return unsafe;
}


// see: man 3 crypt

#define HASH_FUNC_SHA_512   "6"
#define N_SALT              16
#define N_HASH              86
#define N_ENCRYPTED         106

bool kiwi_crypt_file_read(const char *fn, int *seq, char **salt, char **hash)
{
    #define N_SEQ 16
    #define N_FILE (N_ENCRYPTED + N_SEQ + SPACE_FOR_NULL)
    char encrypted_s[N_FILE];
    
    int n = kiwi_file_read("kiwi_crypt_file_read", fn, encrypted_s, N_FILE, /* rem_nl */ true);
    if (n < N_ENCRYPTED) {
        printf("kiwi_crypt_file_read SHORT n=%d/%d\n", n, N_ENCRYPTED);
        return false;
    }
    encrypted_s[n] = '\0';
    printf("### kiwi_crypt_file_read \"%s\" n=%d <%s>\n", fn, n, encrypted_s);

    #define N_FILE_FIELDS 4
    char *r_buf;
    str_split_t cf[N_FILE_FIELDS + 1];
    n = kiwi_split(encrypted_s, &r_buf, "$", cf, N_FILE_FIELDS);
    if (n != N_FILE_FIELDS) {
        printf("kiwi_crypt_file_read FORMAT ERROR n=%d/%d\n", n, N_FILE_FIELDS);
        return false;
    }

    n = strlen(cf[1].str);
    if (n != N_SALT) {
        printf("kiwi_crypt_file_read SALT LEN n=%d\n", n);
        return false;
    }
    if (salt) *salt = strdup(cf[1].str);

    n = strlen(cf[2].str);
    if (n != N_HASH) {
        printf("kiwi_crypt_file_read HASH LEN n=%d\n", n);
        return false;
    }
    if (hash) *hash = strdup(cf[2].str);

    if (seq) *seq = strtol(cf[3].str, NULL, 0);

    kiwi_ifree(r_buf, "kiwi_crypt_file_read");
    return true;
}

char *kiwi_crypt_generate(const char *key, int seq)
{
    if (key == NULL) key = "";
    
    u1_t u[N_SALT];
	int n = kiwi_file_read("kiwi_crypt_generate", "/dev/urandom", (char *) u, N_SALT);
	check(n == N_SALT);
    
    // map bottom 6-bits of each random byte into salt dictionary space: [a-zA-Z0-9./]
    char salt[N_SALT + SPACE_FOR_NULL];
    for (int i=0; i < N_SALT; i++) {
        u1_t c = u[i] & 0x3f;
        c += '0';
        if (c > '9') c += 0x07;     // i.e. '9'+1(0x3a) => 'A'(0x41)
        if (c > 'Z') c += 0x06;     // i.e. 'Z'+1(0x5b) => 'a'(0x61)
        if (c > 'z') c -= 0x4d;     // i.e. 'z'+1(0x7b) => '.'(0x2e) and '/'(0x2f)
        salt[i] = c;
    }
    salt[N_SALT] = '\0';
    
    printf("kiwi_crypt_generate: salt=<%s>\n", salt);
    char *salt_s, *encr;
    asprintf(&salt_s, "$%s$%s$", HASH_FUNC_SHA_512, salt);
    #ifdef HOST
        #ifdef USE_CRYPT
            encr = crypt(key, salt_s);
        #else
            panic("compiled without -lcrypt");
        #endif
    #endif
    free(salt_s);
    
    if (encr == NULL || strlen(encr) != N_ENCRYPTED) {
        printf("kiwi_crypt_generate ERROR: encr=%p sl=%d\n", encr, encr? strlen(encr) : 0);
        return NULL;
    }

    char *encr_s;
    asprintf(&encr_s, "%s$%d", encr, seq);

    return encr_s;
}

bool kiwi_crypt_validate(const char *key, char *salt, char *hash_o)
{
    int n;
    if (key == NULL) key = "";

    char *salt_s, *encrypted;
    asprintf(&salt_s, "$%s$%s$", HASH_FUNC_SHA_512, salt);
    #ifdef HOST
        #ifdef USE_CRYPT
            encrypted = crypt(key, salt_s);
        #else
            panic("compiled without -lcrypt");
        #endif
    #endif
    free(salt_s);

    if (encrypted == NULL || strlen(encrypted) != N_ENCRYPTED) {
        printf("kiwi_crypt_validate ERROR: encrypted=%p strlen=%d\n", encrypted, encrypted? strlen(encrypted) : 0);
        return false;
    }
    
    #define N_CRYPT_FIELDS 3
    char *r_buf;
    str_split_t cf[N_CRYPT_FIELDS + 1];
    n = kiwi_split(encrypted, &r_buf, "$", cf, N_CRYPT_FIELDS);
    check(n == N_CRYPT_FIELDS);
    char *hash_n = cf[2].str;

    bool match = (strcmp(hash_o, hash_n) == 0 && strlen(hash_o) == strlen(hash_n))? true:false;
    printf("kiwi_crypt_validate: key=\"%s\" salt=%s match=%s\n", key, salt, match? "OK":"FAIL");
    printf("kiwi_crypt_validate: hash_o=%s\n", hash_o);
    printf("kiwi_crypt_validate: hash_n=%s\n", hash_n);
    kiwi_ifree(r_buf, "kiwi_crypt_validate");

    return match;
}

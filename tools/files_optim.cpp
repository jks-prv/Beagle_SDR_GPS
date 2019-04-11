/*

*/

// Copyright (c) 2018 John Seamons, ZL/KF6VO

#include "types.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

void panic(const char *msg)
{
    printf("panic: %s\n", msg);
    exit(1);
}

#define scall(x, y) if ((y) < 0) panic(x);

bool str_ends_with(char *s, const char *cs)
{
    int slen = strlen(cs);
    return (strncmp(s + strlen(s) - slen, cs, slen) == 0);
}

char *new_ext(char *fn, const char *ext1, const char *ext2)
{
    char *fn2 = strrchr(fn, '.');
    int slen = fn2 - fn;
    int elen1 = strlen(ext1);
    int elen2 = strlen(ext2);
    char *bf = (char *) malloc(slen + elen1 + elen2 + SPACE_FOR_NULL);
    strncpy(bf, fn, slen);
    if (elen1) strncpy(bf + slen, ext1, elen1);
    if (elen2) strncpy(bf + slen + elen1, ext2, elen2);
    bf[slen + elen1 + elen2] = '\0';
    return bf;
}

typedef enum { EMBED, EXT, ALWAYS } proc_e;
const char *proc_s[] = { "EMBED", "EXT", "ALWAYS" };

#define NFILES 128
typedef enum { F_HTML, F_CSS, F_JS, F_JPG, F_PNG, F_MISC, NTYPES } file_e;
const char *ext_s[] = { ".html", ".css", ".js", ".jpg", ".png", "(misc)" };
int fidx[NTYPES];
static char *files[NFILES][NTYPES];

#define MF_NO_MERGE     0x01
#define MF_JS           0x02
#define MF_LIST         0x04

void minify(const char *msg, u4_t mflags, const char *svc, const char *ext, char *fn)
{
    char *cmd;
    
    char *fn_min = new_ext(fn, ".min", ext);
    
    if (mflags & MF_LIST) {
        printf("%s ", fn_min);
        free(fn_min);
        char *fn_zip = new_ext(fn_min, ext, ".gz");
        printf("%s ", fn_zip);
        free(fn_zip);
        return;
    }
    
    struct stat sb_fn, sb_min;
    scall("sb_fn", stat(fn, &sb_fn));
    int err = stat(fn_min, &sb_min);
    
    const char *status_min;
    bool okay = false;
    if (err != 0) {
        status_min = "M-NEW";
    } else
    if (sb_fn.st_mtime > sb_min.st_mtime) {
        status_min = "M-OLD";
    } else {
        status_min = "M-OK ";
        okay = true;
    }

    if (!okay) {
        asprintf(&cmd, "curl -X POST -s --data-urlencode \'input@%s\' https://%sminifier.com/raw >%s",
            fn, svc, fn_min);
        printf("%s\n", cmd);
        system(cmd);
        free(cmd);

        if ((mflags & MF_NO_MERGE) && (mflags & MF_JS)) {
            asprintf(&cmd, "curl -X POST -s --data-urlencode \'input@%s\' https://%sminifier.com/raw >%s",
                fn, svc, fn_min);
            asprintf(&cmd, "echo \'\nkiwi_check_js_version.push({ VERSION_MAJ:%d, VERSION_MIN:%d, file:\"%s\" });\' >>%s",
                VERSION_MAJ, VERSION_MIN, fn, fn_min);
            printf("%s\n", cmd);
            system(cmd);
            free(cmd);
        }

        stat(fn_min, &sb_min);
    }
    
    char *fn_zip = new_ext(fn_min, ext, ".gz");
    struct stat sb_zip;
    err = stat(fn_zip, &sb_zip);

    const char *status_zip;
    okay = false;
    if (err != 0) {
        status_zip = "Z-NEW";
    } else
    if (sb_min.st_mtime > sb_zip.st_mtime) {
        status_zip = "Z-OLD";
    } else {
        status_zip = "Z-OK ";
        okay = true;
    }

    if (!okay) {
        #define MTU 1500
        if (sb_min.st_size > MTU) {
            asprintf(&cmd, "gzip --best --keep %s", fn_min);
            printf("%s\n", cmd);
            system(cmd);
            free(cmd);
            stat(fn_zip, &sb_zip);
        } else {
            sb_zip.st_size = 0;
            status_zip = "Z-SML";
        }
    }
    
    printf("%s %6lld %s %6lld %s %6lld %s\n", msg, sb_fn.st_size, status_min, sb_min.st_size, status_zip, sb_zip.st_size, fn);
    free(fn_min);
    free(fn_zip);
}

int main(int argc, char *argv[])
{
    int i, j;
    char *cmd;
    
    argc--;
    int ai = 1;
    if (argc < 2) panic("argc");

    int flags = 0;
    bool list = false;
    if (strcmp(argv[ai], "-l") == 0) {
        flags |= MF_LIST;
        list = true;
        ai++; argc--;
    }

    proc_e proc;
    if (argv[ai][1] == 'e')
        proc = EMBED;
    else
    if (argv[ai][1] == 'x')
        proc = EXT;
    else
    if (argv[ai][1] == 'a')
        proc = ALWAYS;
    else
        panic("arg");
    ai++; argc--;
    
    int nfiles = argc;
    if (!list) printf("files_optim: %s nfiles=%d\n", proc_s[proc], nfiles);
    
    for (i=0; i < nfiles; i++) {
        char *fn = argv[ai+i];
        for (j=0; j < NTYPES; j++) {
            if (j == NTYPES-1 || str_ends_with(fn, ext_s[j])) {
                files[fidx[j]][j] = fn;
                fidx[j]++;
                break;
            }
        }
    }
    
    if (!list) for (i=0; i < NTYPES; i++) printf("%2d %s\n", fidx[i], ext_s[i]);
    
    switch (proc) {
    
    // Extension .js/.css files are dynamically loaded when extension is first run.
    // So minify and (potentially) gzip them, but don't merge them.
    case EXT:
    #if 1
        //for (i=0; i < fidx[F_JS]; i++) {
        for (i=0; i < 1; i++) {
            minify("EXT js ", MF_NO_MERGE | MF_JS | flags, "javascript-", ".js", files[i][F_JS]);
        }
        //for (i=0; i < fidx[F_CSS]; i++) {
        for (i=0; i < 1; i++) {
            minify("EXT css", MF_NO_MERGE | flags, "css", ".css", files[i][F_CSS]);
        }
    #endif
        break;
    
    default:
        //panic("proc");
        break;
    
    }
    
    if (list) printf("\n");
    return 0;
}












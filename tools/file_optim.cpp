/*

*/

// Copyright (c) 2019 John Seamons, ZL4VO/KF6VO

#include "types.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include <string.h>
#include <fcntl.h>
#include <time.h>
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

// construct new filename with optional extensions: basename.ext => basename{.ext1}{.ext2}
// i.e. .ext is replaced
char *new_ext(char *fn, const char *ext1, const char *ext2)
{
    char *fn2 = strrchr(fn, '.');
    int basename_sl = fn2 - fn;
    int ext1_sl = strlen(ext1);
    int ext2_sl = strlen(ext2);
    char *bf = (char *) malloc(basename_sl + ext1_sl + ext2_sl + SPACE_FOR_NULL);
    strncpy(bf, fn, basename_sl);
    if (ext1_sl) strncpy(bf + basename_sl, ext1, ext1_sl);
    if (ext2_sl) strncpy(bf + basename_sl + ext1_sl, ext2, ext2_sl);
    bf[basename_sl + ext1_sl + ext2_sl] = '\0';
    return bf;
}

#define NFILES 256
typedef enum { F_JS, F_CSS, F_HTML, F_JPG, F_PNG, F_MISC, NTYPES } file_e;
const char *ext_s[NTYPES] = { ".js", ".css", ".html", ".jpg", ".png", "(misc)" };
bool minimize[NTYPES] = { 1, 1, 0, 0, 0, 0 };
int fidx[NTYPES];
static char *files[NFILES][NTYPES];

#define MF_TYPE     0x00ff
#define MF_JS       0x0001
#define MF_CSS      0x0002
#define MF_PNG      0x0004
#define MF_JPG      0x0008
#define MF_HTML     0x0010

#define MF_JS_VER   0x0100
#define MF_LIST     0x0200
#define MF_USE      0x0400
#define MF_ZIP      0x0800
#define MF_DRY_RUN  0x1000

#define MTU 1500

void minify(const char *ext_s, u4_t mflags, const char *svc, const char *ext, char *fn)
{
    char *cmd;
    int err;
    bool not_dry_run = !(mflags & MF_DRY_RUN);
    
    char *fn_min = new_ext(fn, ".min", ext);
    
    if (mflags & MF_LIST) {
        char *fn_zip = new_ext(fn_min, ext, ".gz");
        struct stat sb_zip;
        err = stat(fn_zip, &sb_zip);
        
        // ignore leftover zips that haven't been removed yet due to below MTU
        bool small = (err == 0 && sb_zip.st_size <= MTU);
        if (!(mflags & MF_USE) || err || small) printf("%s ", fn_min);
        free(fn_min);
        if (err == 0 && !small) printf("%s ", fn_zip);
        free(fn_zip);
        return;
    }
    
    struct stat sb_fn, sb_min;
    //scall("sb_fn", stat(fn, &sb_fn));
    err = stat(fn, &sb_fn);
    if (err < 0) {
        printf("%s\n", fn);
        perror("sb_fn");
        panic("sb_fn");
    }
    err = stat(fn_min, &sb_min);
    
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
        //#define MINIFY_WEBSITE_DOWN
        #ifdef MINIFY_WEBSITE_DOWN
            if (mflags & MF_HTML) {
                asprintf(&cmd, "cat %s >%s",
                    fn, fn_min);
            } else
            if (mflags & (MF_JS|MF_CSS)) {
                asprintf(&cmd, "echo '\n\n/* MINIFY_WEBSITE_DOWN %s */' >%s; cat %s >>%s; echo '\n\n' >>%s",
                    fn_min, fn_min, fn, fn_min, fn_min);
            } else {
                asprintf(&cmd, "cat %s >%s",
                    fn, fn_min);
            }
        #else
            if (mflags & MF_HTML) {
                // our HTML minifier just doesn't work -- too many problems
                //asprintf(&cmd, "curl -Ls -X POST --http1.0 --data \'type=html\' --data \'fn=%s\' --data-urlencode \'input@%s\' %s >%s",
                //    fn, fn, svc, fn_min);
                asprintf(&cmd, "cat %s >%s",
                    fn, fn_min);
            } else
            if (mflags & (MF_JS|MF_CSS)) {
                asprintf(&cmd, "echo '\n\n/* %s (%s = %.24s) */' >%s; "
                    "curl -Ls -X POST --http1.0 --data \'type=%s\' --data \'fn=%s\' --data-urlencode \'input@%s\' %s >>%s; "
                    "echo '\n\n' >>%s",
                    fn_min, fn, ctime(&sb_fn.st_mtime), fn_min,
                    (mflags & MF_JS)? "js" : "css", fn, fn, svc, fn_min,
                    fn_min);
            } else {
                //asprintf(&cmd, "curl -Ls -X POST --form \'input=@%s;type=image/%s\' %s >%s",
                //    fn, &ext[1], svc, fn_min);
                asprintf(&cmd, "cat %s >%s",
                    fn, fn_min);
            }
        #endif
        printf("%s\n", cmd);
        if (not_dry_run) system(cmd);
        free(cmd);

        /*  can't do here -- must continue to be done in webserver on demand to catch changing version numbers
        if ((mflags & MF_JS) && (mflags & MF_JS_VER)) {
            asprintf(&cmd, "echo \'\nkiwi_check_js_version.push({ VERSION_MAJ:%d, VERSION_MIN:%d, file:\"%s\" });\' >>%s",
                VERSION_MAJ, VERSION_MIN, fn, fn_min);
            //printf("%s\n", cmd);
            if (not_dry_run) system(cmd);
            free(cmd);
        }
        */

        stat(fn_min, &sb_min);
        
        // browsers don't like zero length .css files etc.
        if (sb_min.st_size == 0) {
            asprintf(&cmd, "echo >%s", fn_min);     // make file single newline
            if (not_dry_run) system(cmd);
            free(cmd);
            stat(fn_min, &sb_min);
        }
    }
    
    struct stat sb_zip = {0};
    const char *status_zip = "Z-NO ";
    bool remove_old_zip = false;
    char *fn_zip = new_ext(fn_min, ext, ".gz");
    err = stat(fn_zip, &sb_zip);

    if (mflags & MF_ZIP) {
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
            if (sb_min.st_size > MTU) {
                //asprintf(&cmd, "gzip --best --keep --force %s", fn_min);
                asprintf(&cmd, "gzip --fast --keep --force %s", fn_min);
                printf("%s\n", cmd);
                if (not_dry_run) system(cmd);
                free(cmd);
                stat(fn_zip, &sb_zip);
            } else {
                remove_old_zip = true;      // prevent old zip leftover if file shrinking
                sb_zip.st_size = 0;
                status_zip = "Z-SML";
            }
        }
    } else {
        remove_old_zip = true;
    }
    
    if (remove_old_zip && err == 0) {
        // ensure any old/unwanted .gz is removed
        asprintf(&cmd, "rm -f %s", fn_zip);
        if (not_dry_run) system(cmd);
        free(cmd);
        status_zip = "Z-RM ";
    }
    
    printf("%-3s %6llu %s %6llu %s %6llu %s\n",
        ext_s, (u64_t) sb_fn.st_size, status_min, (u64_t) sb_min.st_size, status_zip, (u64_t) sb_zip.st_size, fn);
    free(fn_min);
    free(fn_zip);
}


// usage:
// file_optim [-l|u] -e|-x|-a files ...
// -l           optionally produce list of all generated files (e.g. for use with ls and rm)
// -u           optionally produce list of all file variants that should be used
// -n           dry run, don't actually do anything
// -e|-x|-a     file type, embed|extension|always

#define ARG(a,f) if (strcmp(argv[ai], a) == 0) { flags |= (f); ai++; argc--; }

int main(int argc, char *argv[])
{
    int i, j;
    char *cmd;
    u4_t flags = 0;
    int trace;
    
    argc--;
    int ai = 1;
    if (argc < 2) panic("argc");

    while (argc && argv[ai][0] == '-') {
        ARG("-l", MF_LIST) else
        ARG("-u", MF_USE|MF_LIST) else
        ARG("-n", MF_DRY_RUN) else
        ARG("-ver", MF_JS_VER) else
        ARG("-zip", MF_ZIP) else
        ARG("-js", MF_JS) else
        ARG("-css", MF_CSS) else
        ARG("-html", MF_HTML) else
        if (sscanf(argv[ai], "-t%d", &trace) == 1) { ai++; argc--; } else
        {
            printf("file_optim: arg \"%s\"\n", argv[ai]);
            panic("unknown arg");
        }
    }
    
    bool list = (flags & MF_LIST);
    int nfiles = argc;
    if (nfiles == 0) return 0;
    
    // if not limited then select all
    if (!(flags & (MF_JS|MF_CSS|MF_HTML|MF_PNG|MF_JPG)))
        flags |= MF_JS|MF_CSS|MF_HTML|MF_PNG|MF_JPG;
    u4_t mflags = flags & ~MF_TYPE;
    
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
    
    if (!list) {
        printf("file_optim: nfiles=%d\n", nfiles);
        for (i=0; i < NTYPES; i++) printf("%2d %s\n", fidx[i], ext_s[i]);
    } else
        dprintf(STDERR_FILENO, "file_optim: nfiles=%d trace=%d\n", nfiles, trace);
    
    // minify and (potentially) gzip them, but don't merge them.
    if (flags & MF_JS) for (i=0; i < fidx[F_JS]; i++) {
        //minify("js ", MF_JS|mflags, "www.toptal.com/developers/javascript-minifier/api/raw", ".js", files[i][F_JS]);
        minify("js ", MF_JS|mflags, "kiwisdr.com/php/update.php", ".js", files[i][F_JS]);
    }
    if (flags & MF_CSS) for (i=0; i < fidx[F_CSS]; i++) {
        //minify("css", MF_CSS|mflags, "www.toptal.com/developers/cssminifier/raw", ".css", files[i][F_CSS]);
        minify("css", MF_CSS|mflags, "kiwisdr.com/php/update.php", ".css", files[i][F_CSS]);
    }
    if (flags & MF_HTML) for (i=0; i < fidx[F_HTML]; i++) {
        //minify("html", MF_HTML|mflags, "www.toptal.com/developers/html-minifier/raw", ".html", files[i][F_HTML]);
        minify("html", MF_HTML|mflags, "kiwisdr.com/php/update.php", ".html", files[i][F_HTML]);
    }
    if (flags & MF_PNG) for (i=0; i < fidx[F_PNG]; i++) {
        minify("png", MF_PNG|mflags, "www.toptal.com/developers/pngcrush/crush", ".png", files[i][F_PNG]);
    }
    if (flags & MF_JPG) for (i=0; i < fidx[F_JPG]; i++) {
        minify("jpg", MF_JPG|mflags, "www.toptal.com/developers/jpgoptimiser/optimise", ".jpg", files[i][F_JPG]);
    }
    
    return 0;
}

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
#include "mem.h"
#include "printf.h"

#include <stdlib.h>


#ifdef MALLOC_INTERCEPT
    
void *kiwi_imalloc(const char *from, size_t size)
{
    void *ptr = malloc(size);
    //           kiwi_irealloc ______ _
    real_printf("kiwi_imalloc  %6d %04x %s\n",
        size, FROM_VOID_PARAM(ptr) & 0xffff, from);
    return ptr;
}

void *kiwi_icalloc(const char *from, size_t nel, size_t size)
{
    void *ptr = calloc(nel, size);
    //           kiwi_irealloc ______ _
    real_printf("kiwi_icalloc  %6d %04x %s\n",
        nel * size, FROM_VOID_PARAM(ptr) & 0xffff, from);
    return ptr;
}

void *kiwi_irealloc(const char *from, void *ptr, size_t size)
{
    void *ptr2 = realloc(ptr, size);
    //           kiwi_irealloc ______ _
    real_printf("kiwi_irealloc %6d %04x => %04x %s %d\n",
        size, FROM_VOID_PARAM(ptr) & 0xffff, FROM_VOID_PARAM(ptr2), from);
    return ptr2;
}

void kiwi_ifree(const char *from, void *ptr)
{
    //           kiwi_irealloc ______ _
    real_printf("kiwi_ifree           %04x %s\n",
        FROM_VOID_PARAM(ptr) & 0xffff, from);
    free(ptr);
}

#endif


#ifdef MALLOC_DEBUG

// bypass if using valgrind so there are no lingering references from here that defeat leak detection

//#define MALLOC_DEBUG_PRF
#if defined(MALLOC_DEBUG_PRF) && !defined(USE_VALGRIND)
 #define kmprintf(x) printf x;
 //#define kmprintf(x) if (strstr(from, ".json")) printf x;
#else
 #define kmprintf(x)
#endif

#define NMT 1024
struct mtrace_t {
	int i;
	const char *from;
	char *ptr;
	int size;
} mtrace[NMT];

static int nmt;

// call from gdb: "p mt_dump()"
void mt_dump()
{
    int i, j;
	mtrace_t *mt;

    printf("nmt=%d\n", nmt);
	for (i=0; i<NMT; i++) {
		mt = &mtrace[i];
		if (mt->ptr == NULL) continue;
		bool dup = false;
		for (j = i-1; j >= 0; j--) if (mtrace[j].ptr == mt->ptr) dup = true;
		printf("#%d|%d size=%d %p%s \"%s\"\n",
		    i, mt->i, mt->size, mt->ptr, dup? "(DUP)":"", mt->from);
	}
}

static int mt_enter(const char *from, void *ptr, int size)
{
	int i;
	mtrace_t *mt;
	
	#ifdef USE_VALGRIND
	    return 0;
	#endif
	
	for (i=0; i<NMT; i++) {
		mt = &mtrace[i];
		if (mt->ptr == ptr) {
			kmprintf(("  mt_enter \"%s\" #%d (\"%s\") size=%d %p\n", from, i, mt->from, size, ptr));
			panic("mt_enter dup");
		}
		if (mt->ptr == NULL) {
			mt->i = i;
			mt->from = from;
			mt->ptr = (char*) ptr;
			mt->size = size;
			break;
		}
	}
	
	if (i == NMT) panic("mt_enter overflow");
	nmt++;
	return i+1;
}

static void mt_remove(const char *from, void *ptr)
{
	int i;
	mtrace_t *mt;
	
	#ifdef USE_VALGRIND
	    return;
	#endif
	
	for (i=0; i<NMT; i++) {
		mt = &mtrace[i];
		if (mt->ptr == (char*) ptr) {
			mt->ptr = NULL;
			break;
		}
	}
	
	kmprintf(("  mt_remove \"%s\" #%d %p\n", from, i+1, ptr));
	if (i == NMT) {
		printf("mt_remove \"%s\"\n", from);
		panic("mt_remove not found");
	}
	nmt--;
}

#define	MALLOC_MAX	PHOTO_UPLOAD_MAX_SIZE

void *kiwi_malloc(const char *from, size_t size)
{
	//if (size > MALLOC_MAX) panic("malloc > MALLOC_MAX");
	kmprintf(("kiwi_malloc-1 \"%s\" size=%d\n", from, size));
	void *ptr = malloc(size);
	memset(ptr, 0, size);
    #ifdef MALLOC_DEBUG_PRF
        int i = mt_enter(from, ptr, size);
    #else
        mt_enter(from, ptr, size);
    #endif
	kmprintf(("kiwi_malloc-2 \"%s\" #%d size=%d %p\n", from, i, size, ptr));
	return ptr;
}

void *kiwi_realloc(const char *from, void *ptr, size_t size)
{
	//if (size > MALLOC_MAX) panic("malloc > MALLOC_MAX");
	kmprintf(("kiwi_realloc-1 \"%s\" size=%d %p\n", from, size, ptr));
	mt_remove(from, ptr);
	ptr = realloc(ptr, size);
    #ifdef MALLOC_DEBUG_PRF
        int i = mt_enter(from, ptr, size);
    #else
        mt_enter(from, ptr, size);
    #endif
	kmprintf(("kiwi_realloc-2 \"%s\" #%d size=%d %p\n", from, i, size, ptr));
	return ptr;
}

char *kiwi_strdup(const char *from, const char *s)
{
	int sl = strlen(s)+1;
	if (sl == 0 || sl > 1024) panic("strdup size");
	char *ptr = strdup(s);
    #ifdef MALLOC_DEBUG_PRF
        int i = mt_enter(from, (void*) ptr, sl);
    #else
        mt_enter(from, (void*) ptr, sl);
    #endif
	kmprintf(("kiwi_strdup \"%s\" #%d %d %p %p\n", from, i, sl, s, ptr));
	return ptr;
}

void kiwi_free(const char *from, void *ptr)
{
	kmprintf(("kiwi_free \"%s\" %p\n", from, ptr));
	if (ptr == NULL) return;
	mt_remove(from, ptr);
	free(ptr);
}

int kiwi_malloc_stat()
{
	return nmt;
}

#endif  // MALLOC_DEBUG

void kiwi_str_redup(char **ptr, const char *from, const char *s)
{
	int sl = strlen(s)+1;
	if (sl == 0 || sl > 1024) panic("strdup size");
	if (*ptr) kiwi_free(from, (void*) *ptr);
	*ptr = strdup(s);
#ifdef MALLOC_DEBUG
    #ifdef MALLOC_DEBUG_PRF
        int i = mt_enter(from, (void*) *ptr, sl);
    #else
        mt_enter(from, (void*) *ptr, sl);
    #endif
	kmprintf(("kiwi_str_redup \"%s\" #%d %d %p %p\n", from, i, sl, s, *ptr));
#endif
}

void *kiwi_table_realloc(const char *id, void *cur_p, int cur_size, int inc_size, int el_size)
{
    void *new_p;
    char *clr_p = NULL;
    int new_size = cur_size + inc_size;
    
    if ((cur_size == 0 && cur_p != NULL) || (cur_size != 0 && cur_p == NULL)) {
        printf("INVALID COMBO: cur_size=%d cur_p=%p\n", cur_size, cur_p);
        panic("kiwi_table_realloc");
    }
    
    if (cur_size == 0) {
        new_p = kiwi_malloc(id, new_size * el_size);    // kiwi_malloc() zeros mem
    } else {
        new_p = kiwi_realloc(id, cur_p, new_size * el_size);
        clr_p = ((char *) new_p) + (cur_size * el_size);
        memset(clr_p, 0, inc_size * el_size);   // zero new part since kiwi_realloc() doesn't
    }

    //printf("kiwi_table_realloc %s: cur_p=%p cur_size=%d inc_size=%d el_size=%d | new_p=%p clr_p=%p new_size=%d\n",
    //    id, cur_p, cur_size, inc_size, el_size, new_p, clr_p, new_size);
    return new_p;
}

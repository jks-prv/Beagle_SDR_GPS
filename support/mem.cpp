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
#include "misc.h"
#include "printf.h"

#include <stdlib.h>

mem_t mem;

static void mt_gdb(const char *s)
{
    real_panic(s);
}

#ifdef MALLOC_INTERCEPT_PRINTF

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
 #define kmprintf(x) real_printf x;
 //#define kmprintf(x) if (strstr(from, ".json")) real_printf x;
#else
 #define kmprintf(x)
#endif

#ifdef MALLOC_INTERCEPT_DEBUG
    #define NMT 8192
#else
    #define NMT 1024
#endif

struct mtrace_t {
	int i;
	const char *from;
	char *ptr;
	int size;
} mtrace[NMT];

// call from gdb: "p mt_dump()"
void mt_dump()
{
    int i, j;
	mtrace_t *mt;

    real_printf("mt_dump nmt=%d hiwat=%d size=%s(%d)\n",
        mem.nmt, mem.hiwat, toUnits(mem.size), mem.size);
	for (i=0; i < NMT; i++) {
		mt = &mtrace[i];
		if (mt->ptr == NULL) continue;
		bool dup = false;
		for (j = i-1; j >= 0; j--) if (mtrace[j].ptr == mt->ptr) dup = true;
		real_printf("#%d|%d size=%s(%d) %p%s \"%s\"\n",
		    i, mt->i, toUnits(mt->size), mt->size, mt->ptr, dup? "(DUP)":"", mt->from);
	}
}

static int mt_enter(const char *from, void *ptr, int size)
{
	int i;
	mtrace_t *mt;
	
	#ifdef USE_VALGRIND
	    return 0;
	#endif
	
	for (i=0; i < NMT; i++) {
		mt = &mtrace[i];
		if (mt->ptr == ptr) {
			kmprintf(("  mt_enter \"%s\" #%d (\"%s\") size=%d %p\n", from, i, mt->from, size, ptr));
			mt_gdb("mt_enter dup");
		}
		if (mt->ptr == NULL) {
			mt->i = i;
			mt->from = from;
			mt->ptr = (char*) ptr;
			mt->size = size;
			if (i >= mem.hiwat) mem.hiwat = i+1;
			if (mem.hiwat > NMT) mem.hiwat = NMT;
			break;
		}
	}
	
	if (i >= NMT) {
	    real_printf("\n\nmt_enter overflow:");
	    mt_dump();
	    mt_gdb("mt_enter overflow");
	}
	mem.nmt++;
    //real_printf("E n=%02d i=%02d h=%02d m=%02d %p\n", mem.nmt, i, mem.hiwat, mt->i, mt->ptr);
	mem.size += size;
	return i+1;
}

static void mt_remove(const char *from, void *ptr)
{
	int i, j;
	mtrace_t *mt, *mt2;
	size_t size = 0;
	
	#ifdef USE_VALGRIND
	    return;
	#endif
	
	for (i=0; i < mem.hiwat; i++) {
		mt = &mtrace[i];
		if (mt->ptr == (char*) ptr) {
		    size = mt->size;
			mt->ptr = NULL;
			break;
		}
	}
	
	kmprintf(("  mt_remove \"%s\" #%d size=%d %p\n", from, i+1, size, ptr));
	if (i == mem.hiwat || i == NMT) {
		real_printf("mt_remove \"%s\"\n", from);
		mt_gdb("mt_remove not found");
	}
	mem.nmt--;
	
	// Tricky: If removing last entry then hiwat needs to be reduced.
	// But there could be multiple empty entries below it, so search for first occupied.
	if (i+1 == mem.hiwat) {
        for (j = i; j >= 0 ; j--) {
            mt2 = &mtrace[j];
            if (mt2->ptr != NULL)
                break;
            mem.hiwat = j;
        }
    }
    //real_printf("R n=%02d i=%02d h=%02d m=%02d %p\n", mem.nmt, i, mem.hiwat, mt->i, ptr);
	if (mem.hiwat < 0) mem.hiwat = 0;
	mem.size -= size;
}

#endif  // MALLOC_DEBUG

#define	MALLOC_MAX	PHOTO_UPLOAD_MAX_SIZE

void *kiwi_malloc(const char *from, size_t size)
{
	//if (size > MALLOC_MAX) mt_gdb("malloc > MALLOC_MAX");
    #ifdef MALLOC_DEBUG
        kmprintf(("kiwi_malloc-1 \"%s\" size=%d\n", from, size));
    #endif
	void *ptr = malloc(size);
	memset(ptr, 0, size);
    #ifdef MALLOC_DEBUG
        int i = mt_enter(from, ptr, size);
        kmprintf(("kiwi_malloc-2 \"%s\" #%d size=%d %p (total #%d|hi:%d|%s)\n",
            from, i, size, ptr, mem.nmt, mem.hiwat, toUnits(mem.size)));
    #endif
	return ptr;
}

#ifdef MALLOC_DEBUG

void *kiwi_calloc(const char *from, size_t nel, size_t size)
{
    if (nel == 0 || size == 0) return NULL;
    size_t Tsize = nel * size;
	//if (Tsize > MALLOC_MAX) mt_gdb("malloc > MALLOC_MAX");
	kmprintf(("kiwi_calloc-1 \"%s\" nel=%d size=%d Tsize=%d\n", from, nel, size, Tsize));
	void *ptr = malloc(Tsize);
	memset(ptr, 0, Tsize);
    int i = mt_enter(from, ptr, Tsize);
	kmprintf(("kiwi_calloc-2 \"%s\" #%d Tsize=%d %p\n", from, i, Tsize, ptr));
	return ptr;
}

void *kiwi_realloc(const char *from, void *ptr, size_t size)
{
	//if (size > MALLOC_MAX) mt_gdb("malloc > MALLOC_MAX");
	kmprintf(("kiwi_realloc-1 \"%s\" size=%d %p\n", from, size, ptr));
	if (ptr == NULL) {
	    kmprintf(("kiwi_realloc-1.1 \"%s\" PTR NULL -- WILL malloc(%d)\n", from, size));
	    ptr = malloc(size);
	} else
	if (size == 0) {
	    kmprintf(("kiwi_realloc-1.1 \"%s\" SIZE ZERO -- WILL free(%p)\n", from, ptr));
        mt_remove(from, ptr);
	    free(ptr);
	    return NULL;
	} else {
        mt_remove(from, ptr);
        ptr = realloc(ptr, size);
    }

    int i = mt_enter(from, ptr, size);
	kmprintf(("kiwi_realloc-2 \"%s\" #%d size=%d %p\n", from, i, size, ptr));
	return ptr;
}

char *kiwi_strdup(const char *from, const char *s)
{
	int sl = strlen(s)+1;
	if (sl == 0 || sl > 1024) mt_gdb("strdup size");
	char *ptr = strdup(s);
    int i = mt_enter(from, (void *) ptr, sl);
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

#endif  // MALLOC_DEBUG

void kiwi_str_redup(char **ptr, const char *from, const char *s)
{
	int sl = strlen(s)+1;
	if (sl == 0 || sl > 1024) mt_gdb("strdup size");
	if (*ptr) kiwi_free(from, (void *) *ptr);
	*ptr = strdup(s);
    #ifdef MALLOC_DEBUG
        int i = mt_enter(from, (void *) *ptr, sl);
        kmprintf(("kiwi_str_redup \"%s\" #%d %d %p %p\n", from, i, sl, s, *ptr));
    #endif
}

void *kiwi_table_realloc(const char *id, void *cur_p, int cur_nel, int inc_nel, int el_bytes)
{
    void *new_p;
    char *clr_p = NULL;
    int new_nel = cur_nel + inc_nel, clr_bytes = 0;
    
    if ((cur_nel == 0 && cur_p != NULL) || (cur_nel != 0 && cur_p == NULL)) {
        real_printf("INVALID COMBO: cur_nel=%d cur_p=%p\n", cur_nel, cur_p);
        mt_gdb("kiwi_table_realloc");
    }
    
    if (cur_nel == 0) {
        new_p = kiwi_malloc(id, new_nel * el_bytes);    // kiwi_malloc() zeros mem
    } else {
        new_p = kiwi_realloc(id, cur_p, new_nel * el_bytes);
        clr_p = ((char *) new_p) + (cur_nel * el_bytes);
        clr_bytes = inc_nel * el_bytes;
        memset(clr_p, 0, clr_bytes);   // zero new part since kiwi_realloc() doesn't
    }

    //real_printf("kiwi_table_realloc %s: cur_p=%p cur_nel=%d inc_nel=%d el_bytes=%d | new_p=%p new_nel=%d clr_p=%p clr_bytes=%d\n",
    //    id, cur_p, cur_nel, inc_nel, el_bytes, new_p, new_nel, clr_p, clr_bytes);
    return new_p;
}


////////////////////////////////
// list/item
////////////////////////////////

list_t *list_init(const char *id, u4_t item_bytes, u4_t inc_nel)
{
    list_t *list = (list_t *) kiwi_malloc("list_t", sizeof(list_t));   // NB: kiwi_malloc() zeros
    list->id = id;
    list->item_bytes = item_bytes;
    list->inc_nel = list->cur_nel = inc_nel;
    list->items = kiwi_malloc(id, inc_nel * item_bytes);
    return list;
}

void list_grow(list_t *list, u4_t idx)
{
    while (idx >= list->cur_nel) {
        // NB: kiwi_table_realloc() zeros newly allocated area
        list->items = kiwi_table_realloc("list_grow",
            list->items, list->cur_nel, list->inc_nel, list->item_bytes);
        list->cur_nel += list->inc_nel;
        //real_printf("list_grow REALLOC n=%d cur_nel=%d\n", list->n_items, list->cur_nel);
    }
        
}

void list_free(list_t *list)
{
    kiwi_free(list->id, list->items);
    kiwi_free("list_free", list);
}

void *item_ptr(list_t *list, u4_t idx)
{
    if (idx >= list->n_items) return NULL;
    return TO_VOID_PARAM((char *) FROM_VOID_PARAM(list->items)  + (list->item_bytes * idx));
}

u4_t item_find(list_t *list, void *const_item, item_cmp_t item_cmp, bool *found)
{
    int i;
    *found = false;
    char *item_base = (char *) FROM_VOID_PARAM(list->items);

    for (i = 0; i < list->n_items; i++) {
        //real_printf("item_find %d ", i);
        void *item = item_ptr(list, i);
        *found = item_cmp(const_item, item);
        //real_printf("%s\n", *found? "   FOUND" : "");
        if (*found) {
            break;
        }
    }
    return i;
}

u4_t item_find_grow(list_t *list, void *const_item, item_cmp_t item_cmp, bool *isNew)
{
    bool found;
    int idx = item_find(list, const_item, item_cmp, &found);
    if (!found) {
        list_grow(list, idx);
        list->n_items++;
    }
    *isNew = !found;
    return idx;
}

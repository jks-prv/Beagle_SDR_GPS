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

#pragma once

#include "types.h"

#include <stddef.h>

typedef struct {
    int nmt, hiwat;
    size_t size;
} mem_t;

extern mem_t mem;

// allocated directly via asprintf(), sscanf("%ms"), strdup() etc.
#define kiwi_asfree(ptr, ...) free(ptr)
#define kiwi_asfree_set_null(ptr, ...) free(ptr); ptr = NULL;

//#define MALLOC_DEBUG
#ifdef MALLOC_DEBUG
	void *kiwi_malloc(const char *from, size_t size);
	void *kiwi_calloc(const char *from, size_t nel, size_t size);
	void *kiwi_realloc(const char *from, void *ptr, size_t size);
	void kiwi_free(const char *from, void *ptr);
	char *kiwi_strdup(const char *from, const char *s);
	int kiwi_malloc_stat();
	void mt_dump();
#else
    // semantics of kiwi_malloc() is to clear mem which malloc() doesn't do
	//#define kiwi_malloc(from, size) malloc(size)
	void *kiwi_malloc(const char *from, size_t size);
	#define kiwi_calloc(from, nel, size) calloc(nel, size)
	#define kiwi_realloc(from, ptr, size) realloc(ptr, size)
	#define kiwi_free(from, ptr) free(ptr)
	#define kiwi_strdup(from, s) strdup(s)
	#define kiwi_malloc_stat() 0
	#define mt_dump() 0
#endif

//#define MALLOC_INTERCEPT_PRINTF
//#define MALLOC_INTERCEPT_DEBUG

#ifdef MALLOC_INTERCEPT_PRINTF
    void *kiwi_imalloc(const char *from, size_t size);
    void *kiwi_icalloc(const char *from, size_t nel, size_t size);
    void *kiwi_irealloc(const char *from, void *ptr, size_t size);
    void kiwi_ifree(void *ptr, const char *from);

#elif defined(MALLOC_INTERCEPT_DEBUG)
    #ifndef MALLOC_DEBUG
        #error requires MALLOC_DEBUG
    #endif
    
    #define kiwi_imalloc(from, size) kiwi_malloc(from, size)
    #define kiwi_icalloc(from, nel, size) kiwi_calloc(from, nel, size)
    #define kiwi_irealloc(from, ptr, size) kiwi_realloc(from, ptr, size)
    #define kiwi_ifree(ptr, from) kiwi_free(from, ptr)
#else
    #define kiwi_imalloc(from, size) malloc(size)
    #define kiwi_icalloc(from, nel, size) calloc(nel, size)
    #define kiwi_irealloc(from, ptr, size) realloc(ptr, size)
    #define kiwi_ifree(ptr, from) free(ptr)
#endif

void kiwi_str_redup(char **ptr, const char *from, const char *s);
void *kiwi_table_realloc(const char *id, void *cur_p, int cur_nel, int inc_nel, int el_bytes);


////////////////////////////////
// list/item
////////////////////////////////

typedef struct {
    const char *id;
    u4_t n_items, cur_nel;
    u4_t item_bytes, inc_nel;
	void *items;
} list_t;

list_t *list_init(const char *id, u4_t item_bytes, u4_t inc_nel);
void list_grow(list_t *list, u4_t idx);
void list_free(list_t *list);

typedef bool (item_cmp_t)(const void *, void *);
u4_t item_find(list_t *list, void *const_item, item_cmp_t item_cmp, bool *found);
u4_t item_find_grow(list_t *list, void *const_item, item_cmp_t item_cmp, bool *isNew);
void *item_ptr(list_t *list, u4_t idx);

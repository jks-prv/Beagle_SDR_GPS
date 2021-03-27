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

// Copyright (c) 2021 John Seamons, ZL/KF6VO

#pragma once

#include "types.h"

#include <stddef.h>

//#define MALLOC_INTERCEPT
#ifdef MALLOC_INTERCEPT
    void *kiwi_imalloc(const char *from, size_t size);
    void *kiwi_icalloc(const char *from, size_t nel, size_t size);
    void *kiwi_irealloc(const char *from, void *ptr, size_t size);
    void kiwi_ifree(void *ptr, const char *from = NULL);
#else
    #define kiwi_imalloc(from, size) malloc(size)
    #define kiwi_icalloc(from, nel, size) calloc(nel, size)
    #define kiwi_irealloc(from, ptr, size) realloc(ptr, size)
    #define kiwi_ifree(ptr, ...) free(ptr)
#endif

#define MALLOC_DEBUG
#ifdef MALLOC_DEBUG
	void *kiwi_malloc(const char *from, size_t size);
	void *kiwi_realloc(const char *from, void *ptr, size_t size);
	void kiwi_free(const char *from, void *ptr);
	char *kiwi_strdup(const char *from, const char *s);
	void kiwi_str_redup(char **ptr, const char *from, const char *s);
	int kiwi_malloc_stat();
#else
	#define kiwi_malloc(from, size) malloc(size)
	#define kiwi_realloc(from, ptr, size) realloc(ptr, size)
	#define kiwi_free(from, ptr) free(ptr)
	#define kiwi_strdup(from, s) strdup(s)
	void kiwi_str_redup(char **ptr, const char *from, const char *s);
	#define kiwi_malloc_stat() 0
#endif

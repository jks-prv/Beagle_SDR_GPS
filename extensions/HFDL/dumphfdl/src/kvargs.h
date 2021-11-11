/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

#include <stddef.h>         // ptrdiff_t

typedef struct kvargs_s kvargs;

typedef struct {
	kvargs *result;
	ptrdiff_t err_pos;
	int32_t err;
} kvargs_parse_result;

kvargs *kvargs_new();
kvargs_parse_result kvargs_from_string(char *string);
char *kvargs_get(kvargs const *kv, char const *key);
char const *kvargs_get_errstr(int32_t err);
void kvargs_destroy(kvargs *kv);

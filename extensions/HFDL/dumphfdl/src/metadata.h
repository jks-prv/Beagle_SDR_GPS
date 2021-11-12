/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include <sys/time.h>               // struct timeval

struct metadata {
	struct metadata_vtable *vtable;
	struct timeval rx_timestamp;
};

struct metadata_vtable {
	struct metadata* (*copy)(struct metadata const *);
	void (*destroy)(struct metadata *);
};

struct metadata *metadata_copy(struct metadata const *m);
void metadata_destroy(struct metadata *m);

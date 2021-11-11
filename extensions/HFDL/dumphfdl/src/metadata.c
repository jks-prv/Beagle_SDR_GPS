/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "metadata.h"
#include "util.h"               // ASSERT

struct metadata *metadata_copy(struct metadata const *m) {
	ASSERT(m);
	return m->vtable->copy(m);
}

void metadata_destroy(struct metadata *m) {
	if(m == NULL) {
		return;
	}
	m->vtable->destroy(m);
}

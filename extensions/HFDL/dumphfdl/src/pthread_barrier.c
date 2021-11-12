/*
 * Copyright (c) 2015, Aleksey Demakov
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "pthread_barrier.h"
#include <errno.h>

#ifndef __unused
    #define __unused __attribute__((unused))
#endif

int pthr_barrier_init(pthr_barrier_t *restrict barrier,
		const pthr_barrierattr_t *restrict attr __unused, unsigned count) {
	if (count == 0) {
		errno = EINVAL;
		return -1;
	}

	if (pthr_mutex_init("barrier", &barrier->mutex, 0) < 0) {
		return -1;
	}
	if (pthr_cond_init("barrier", &barrier->cond, &barrier->mutex, 0) < 0) {
		int errno_save = errno;
		pthr_mutex_destroy(&barrier->mutex);
		errno = errno_save;
		return -1;
	}

	barrier->limit = count;
	barrier->count = 0;
	barrier->phase = 0;

	return 0;
}

int pthr_barrier_destroy(pthr_barrier_t *barrier)
{
    pthr_mutex_destroy(&barrier->mutex);
    pthr_cond_destroy(&barrier->cond);
    return 0;
}

int pthr_barrier_wait(pthr_barrier_t *barrier) {
	pthr_mutex_lock(&barrier->mutex);
	barrier->count++;
	if (barrier->count >= barrier->limit) {
		barrier->phase++;
		barrier->count = 0;
		pthr_cond_broadcast(&barrier->cond);
		pthr_mutex_unlock(&barrier->mutex);
		return PTHREAD_BARRIER_SERIAL_THREAD;
	} else {
		unsigned phase = barrier->phase;
		do
			pthr_cond_wait(&barrier->cond, &barrier->mutex);
		while (phase == barrier->phase);
		pthr_mutex_unlock(&barrier->mutex);
		return 0;
	}
}

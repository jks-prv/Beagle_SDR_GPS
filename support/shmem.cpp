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

// Copyright (c) 2019 John Seamons, ZL/KF6VO

#include "types.h"
#include "shmem.h"

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/mman.h>

void shmem_init()
{
    int size = sizeof(non_blocking_shmem_t) + (N_LOG_SAVE * N_LOG_MSG_LEN);
    real_printf("SHMEM bytes=%d\n", size);
    shmem = (non_blocking_shmem_t *) mmap((caddr_t) 0, size, PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, -1, 0);
    assert(shmem != MAP_FAILED);
    scall("mlock", mlock(shmem, size));
    memset(shmem, 0, size);
    shmem->log_save.endp = (char *) shmem + size;

    assert(SIG_MAX_USED <= SIGRTMAX);
}

void sig_arm(int sig, funcPI_t func, int flags)
{
    struct sigaction act;
    act.sa_handler = func;
    act.sa_flags = flags;
    sigemptyset(&act.sa_mask);
    scall("sig_arm", sigaction(sig, &act, NULL));
}
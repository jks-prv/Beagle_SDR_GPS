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

non_blocking_shmem_t *shmem;

void shmem_init()
{
    int size = sizeof(non_blocking_shmem_t) + (N_LOG_SAVE * N_LOG_MSG_LEN);
    real_printf("SHMEM %d kB\n", size/K);
    shmem = (non_blocking_shmem_t *) mmap((caddr_t) 0, size, PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, -1, 0);
    assert(shmem != MAP_FAILED);
    scall("mlock", mlock(shmem, size));
    memset(shmem, 0, size);
    shmem->log_save.endp = (char *) shmem + size;

    assert((SIGRTMIN + SIG_MAX_USED) <= SIGRTMAX);
}

void sig_arm(int sig, funcPI_t handler, int flags)
{
    struct sigaction act;
    act.sa_handler = handler;
    act.sa_flags = flags;
    sigemptyset(&act.sa_mask);
    scall("sig_arm", sigaction(sig, &act, NULL));
}

static void shmem_child_sig_handler(int signo)
{
    //real_printf("CHILD shmem_child_sig_handler GOT sig %d from parent\n", signo);
    shmem_ipc_t *ipc = &shmem->ipc[SIG2IPC(signo)];
    sig_arm(ipc->child_sig, shmem_child_sig_handler);      // rearm

    for (int i=0; i <= ipc->which_hiwat; i++) {
        if (ipc->request[i] > ipc->done[i]) {
            ipc->func(i);
            ipc->done[i] = 1;
        }
    }
}

static void shmem_child_task(void *param)
{
    shmem_ipc_t *ipc = (shmem_ipc_t *) FROM_VOID_PARAM(param);
    //real_printf("CHILD shmem_child_task RUNNING parent_pid=%d\n", ipc->parent_pid);
    set_cpu_affinity(1);
    sig_arm(ipc->child_sig, shmem_child_sig_handler);
    
    while (1) {
        pause();
    }
}

void shmem_ipc_invoke(int child_sig, int which)
{
    shmem_ipc_t *ipc = &shmem->ipc[SIG2IPC(child_sig)];
    assert(which < N_SHMEM_WHICH);
    if (which > ipc->which_hiwat) ipc->which_hiwat = which;
    assert(ipc->request[which] == 0);       // guard against reentrancy
    ipc->request[which] = 1;
    //real_printf("PARENT shmem_ipc_invoke..\n");
    kill(ipc->child_pid, ipc->child_sig);

    while (ipc->done[which] == 0) {
        NextTask("shmem_ipc_wait_child");
    }

    //real_printf("PARENT ..shmem_ipc_invoke\n");
    ipc->request[which] = ipc->done[which] = 0;
}

void shmem_ipc_setup(const char *id, int child_sig, funcPI_t func)
{
    shmem_ipc_t *ipc = &shmem->ipc[SIG2IPC(child_sig)];
    memset(ipc, 0, sizeof(shmem_ipc_t));
    ipc->func = func;
    ipc->child_sig = child_sig;
    ipc->parent_pid = getpid();
    ipc->child_pid = child_task(id, shmem_child_task, NO_WAIT, TO_VOID_PARAM(ipc));
    //real_printf("PARENT shmem_ipc_setup child_pid=%d\n", ipc->child_pid);
}

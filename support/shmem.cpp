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

// Copyright (c) 2019 John Seamons, ZL4VO/KF6VO

#include "types.h"
#include "shmem.h"

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <stdarg.h>
#include <signal.h>
#include <sys/mman.h>

shmem_t *shmem;

void shmem_init()
{
    u4_t size = sizeof(shmem_t) + (N_LOG_SAVE * N_LOG_MSG_LEN);
    u4_t rsize = round_up(size, sysconf(_SC_PAGE_SIZE));
    shmem = (shmem_t *) mmap((caddr_t) 0, rsize, PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, -1, 0);
    assert(shmem != MAP_FAILED);
    scall("mlock", mlock(shmem, rsize));
    memset(shmem, 0, rsize);
    u1_t *shmem_end = ((u1_t *) shmem) + rsize;
    shmem->log_save.endp = (char *) shmem_end;

    // printf_init() hasn't been called yet
    real_printf("SHMEM=%.3f MB: ipc=%.3f spi=%.3f rx=%.3f wf=%.3f wspr=%.3f drm=%.3f\n",
        (float) rsize/M,
        
        (float) sizeof(shmem->ipc)/M,

        #ifdef SPI_SHMEM_DISABLE
            0.,
        #else
            (float) sizeof(shmem->spi_shmem)/M,
        #endif

        #ifdef RX_SHMEM_DISABLE
            0.,
        #else
            (float) sizeof(shmem->rx_shmem)/M,
        #endif

        #ifdef WF_SHMEM_DISABLE
            0.,
        #else
            (float) sizeof(shmem->wf_shmem)/M,
        #endif

        #ifdef WSPR_SHMEM_DISABLE
            0.,
        #else
            (float) sizeof(shmem->wspr_shmem)/M,
        #endif

        #ifdef DRM_SHMEM_DISABLE
            0.
        #else
            (float) sizeof(shmem->drm_shmem)/M
        #endif
    );

    if ((SIGRTMIN + SIG_MAX_USED) > SIGRTMAX) {
        real_printf("SIGRTMIN=%d SIGRTMAX=%d\n", SIGRTMIN, SIGRTMAX);
        assert((SIGRTMIN + SIG_MAX_USED) <= SIGRTMAX);
    }

    #if 0
        for (int i = 0; i < 4096; i++) {
            shmem->spi_shmem.firewall[i] = i;
        }

        real_printf(
            "size=0x%x rsize=0x%x "
            "spi_shmem=%p spi_tx[0]=%p spi_tx[N_SPI_TX-1]=%p so(spi_tx)=%d "
            "spi_tx_end=%p so(spi_shmem_t)=%d spi_end=%p "
            "log_save=%p endp=%p\n",
            size, rsize,
            &shmem->spi_shmem, &shmem->spi_shmem.spi_tx[0], &shmem->spi_shmem.spi_tx[N_SPI_TX-1], sizeof(SPI_MOSI),
            (char *)(&shmem->spi_shmem.spi_tx[N_SPI_TX-1]) + sizeof(SPI_MOSI), sizeof(spi_shmem_t),
            (char *)(&shmem->spi_shmem) + sizeof(spi_shmem_t),
            &shmem->log_save, shmem->log_save.endp);
    #endif
}

void sig_arm(int signal, funcPI_t handler, int flags)
{
    struct sigaction act;
    act.sa_handler = handler;
    act.sa_flags = flags;
    sigemptyset(&act.sa_mask);
    scall("sig_arm", sigaction(signal, &act, NULL));
}

static void shmem_child_sig_handler(int signo)
{
    //real_printf("CHILD shmem_child_sig_handler GOT sig %d from parent\n", signo);
    shmem_ipc_t *ipc = &shmem->ipc[SIG2IPC(signo)];
    ipc->request_rx++;
    sig_arm(ipc->child_sig, shmem_child_sig_handler);      // rearm
    ipc->child_signalled = 1;   // no race with clear in parent
    //if (ipc->child_sig == SIG_IPC_WSPR) real_printf("CHILD sig..\n");
}

#define SIG_CHECK(sig, val) { \
    sigset_t checkmask; \
    sigprocmask (0, NULL, &checkmask); \
    assert(sigismember(&oldmask, ipc->child_sig) == 0); \
}

static void shmem_child_task(void *param)
{
    shmem_ipc_t *ipc = (shmem_ipc_t *) FROM_VOID_PARAM(param);
    //real_printf("CHILD shmem_child_task RUNNING parent_pid=%d\n", ipc->parent_pid);
    set_cpu_affinity(1);
    sig_arm(ipc->child_sig, shmem_child_sig_handler);
    
    // see: www.gnu.org/software/libc/manual/html_node/Sigsuspend.html
    sigset_t newmask, oldmask, checkmask;
    sigemptyset (&newmask);
    sigaddset (&newmask, ipc->child_sig);
    sigprocmask (SIG_BLOCK, &newmask, &oldmask);    // newmask has child_sig blocked, oldmask has it enabled
    assert(sigismember(&oldmask, ipc->child_sig) == 0);     // child_sig should NOT be in oldmask
    SIG_CHECK(ipc->child_sig, 1);   // child_sig should be currently blocked

    while (1) {
        while (!ipc->child_signalled) {
        
            // verify signal IS currently blocked
            //SIG_CHECK(ipc->child_sig, 1);

            // suspends, changing current mask to oldmask with child_sig enabled (temporarily)
            sigsuspend (&oldmask);
            // on return newmask is in effect with child_sig blocked again

            // verify signal IS blocked again
            //SIG_CHECK(ipc->child_sig, 1);
        }

        //SIG_CHECK(ipc->child_sig, 1);
        for (int i=0; i <= ipc->which_hiwat; i++) {
            if (ipc->request[i] > ipc->done[i]) {
                //real_printf("CHILD shmem_child_sig_handler func(%d)..\n", i);
                ipc->request_func[0]++;
                ipc->func(i);
                ipc->request_func[1]++;
                //real_printf("CHILD shmem_child_sig_handler ..func(%d)\n", i);
                ipc->done[i] = 1;
                
                // if the shmem_ipc_invoke() caller is not waiting then _we_ must clear ipc->request
                // so the caller doesn't get a reentrancy fault
                if (ipc->no_wait[i]) {
                    ipc->request[i] = 0;
                }
            }
        }

        ipc->child_signalled = 0;   // no race with set in child since child_sig blocked
        //SIG_CHECK(ipc->child_sig, 1);
        //if (ipc->child_sig == SIG_IPC_WSPR) real_printf("CHILD ..signalled\n");
    }
    
    panic("not reached");
}

void shmem_ipc_invoke(int signal, int which, int wait)
{
    shmem_ipc_t *ipc = &shmem->ipc[SIG2IPC(signal)];
    //int tid = ipc->tid;
    int tid = TaskID();
    assert(which < N_SHMEM_WHICH);
    if (which > ipc->which_hiwat) ipc->which_hiwat = which;

    //assert(ipc->request[which] == 0);       // guard against reentrancy
    int request = ipc->request[which];
    //assert(!TaskIsChild());
    if (TaskIsChild()) {
        lock_dump();
    }
    if (request != 0) {
        real_printf("R %s S%d %s %p[%d]=%d\n", Task_s(tid), signal, ipc->pname, &ipc->request[which], which, request);
        lock_dump();
        //kiwi_backtrace("reentrant", PRINTF_REAL);
        panic("reentrant");
    }

    ipc->request_tx++;
    ipc->request[which] = 1;
    ipc->no_wait[which] = (wait == NO_WAIT);

    kill(ipc->child_pid, ipc->child_sig);
    if (wait == NO_WAIT) return;

    // NB: race between signaling child above and having it finish and issuing wakeup before we sleep below.
    // Use new TaskSleepWakeupTest() to monitor ipc->done[which] in _NextTask() similarly to how
    // deadline detection works (lower overhead than spinning in a "while() NextTask()" here).
    // NB: while needed because we could have been woken up for the wrong reason e.g. CTF_BUSY_HELPER
    if (ipc->done[which] == 1) {
        //real_printf("D %s S%d %p[%d]\n", Task_s(tid), signal, &ipc->request[which], which);
    } else {
        while (ipc->done[which] == 0) {
            //real_printf("s.. %s S%d %p[%d]\n", Task_s(tid), signal, &ipc->request[which], which);
            if (tid != 0) {
                TaskSleepWakeupTest("shmem_ipc_wait_child", &ipc->done[which]);
            } else {
                NextTask("shmem_ipc_wait_child");
            }
            //real_printf("..wu %s S%d\n", Task_s(tid), signal);
        }
        //real_printf("X %s S%d %p[%d]\n", Task_s(tid), signal, &ipc->request[which], which);
    }

    //real_printf("PARENT ..shmem_ipc_invoke\n");
    ipc->request[which] = ipc->done[which] = 0;
}

int shmem_ipc_poll(int signal, int poll_msec, int which)
{
    shmem_ipc_t *ipc = &shmem->ipc[SIG2IPC(signal)];
    assert(which < N_SHMEM_WHICH);
    TaskSleepReasonMsec("shmem_ipc_poll", poll_msec);
    int done = ipc->done[which];
    if (done) ipc->done[which] = ipc->request[which] = 0;
    return done;
}

void shmem_ipc_setup(const char *pname, int signal, funcPI_t func)
{
    assert(!TaskIsChild());
    shmem_ipc_t *ipc = &shmem->ipc[SIG2IPC(signal)];
    memset(ipc, 0, sizeof(shmem_ipc_t));
    kiwi_strncpy(ipc->pname, pname, N_SHMEM_PNAME);
    ipc->tid = TaskID();
    ipc->func = func;
    ipc->child_sig = signal;
    ipc->parent_pid = getpid();
    ipc->child_pid = child_task(ipc->pname, shmem_child_task, NO_WAIT, TO_VOID_PARAM(ipc));
    //real_printf("PARENT shmem_ipc_setup child_pid=%d\n", ipc->child_pid);
}

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

#pragma once

#include "types.h"
#include "kiwi.h"
#include "str.h"
#include "printf.h"
#include "spi.h"
#include "spi_dev.h"
#include "data_pump.h"
#include "net.h"
#include "rx_waterfall.h"
#include "wspr.h"

#ifdef DRM
 #include "DRM.h"
#else
 #define DRM_SHMEM_DISABLE
 #define DRM_MAX_RX 0
#endif

#include <signal.h>


// this is here instead of printf.h because of circular #include problems

#define N_LOG_MSG_LEN   256
#define N_LOG_SAVE      256

typedef struct {
    bool init;
	int idx, not_shown;
	char *arr[N_LOG_SAVE];
	char *endp;
	char mem[1];	// mem allocated starting here; must be last in struct
} log_save_t;

extern log_save_t *log_save_p;


#define N_SHMEM_STATUS              4
#define N_SHMEM_STATUS_STR          256
#define N_SHMEM_STATUS_STR_LARGE    1024

#ifndef linux
 #define SIGRTMIN 0
 #define SIGRTMAX 0
#endif

#define SIG_DEBUG       SIGUSR1
#define SIG_SETUP_TRAMP SIGUSR2

#define SIG_IPC_MIN     SIGRTMIN
#define SIG_IPC_SPI     SIG_IPC_MIN
#define SIG_IPC_WF      (SIG_IPC_SPI + 1)
#define SIG_IPC_WSPR    (SIG_IPC_WF + 1)
#define SIG_IPC_DRM     (SIG_IPC_WSPR + MAX_RX_CHANS)
#define SIG_BACKTRACE   (SIG_IPC_DRM + DRM_MAX_RX)
#define SIG_MAX_USED    (1 + 1 + MAX_RX_CHANS + DRM_MAX_RX + 1)      // done this way because SIGRTMIN is not a constant

#define SIG2IPC(sig)    ((sig) - SIG_IPC_MIN)

typedef struct {
    #define N_SHMEM_PNAME 16
    char pname[N_SHMEM_PNAME];
    int tid;
    funcPI_t func;
    int child_sig, child_signalled;
    int parent_pid, child_pid;
    int which_hiwat;
    #define N_SHMEM_WHICH 32
    u4_t request[N_SHMEM_WHICH], done[N_SHMEM_WHICH];
    u4_t request_tx, request_rx, request_func[2];
} shmem_ipc_t;

typedef struct {
    net_t net_shmem;

    u4_t rv_u4_t[MAX_RX_CHANS];
    u4_t status_u4[N_SHMEM_STATUS][MAX_RX_CHANS];
    double status_f[N_SHMEM_STATUS][MAX_RX_CHANS];
    
    // users: (none currently)
	char status_str[N_SHMEM_STATUS_STR];

	// users: geoloc
	char status_str_large[N_SHMEM_STATUS_STR_LARGE];
	
    shmem_ipc_t ipc[SIG_MAX_USED];
    
    #ifdef SPI_SHMEM_DISABLE
    #else
        // shared with SPI async i/o helper process
        spi_shmem_t spi_shmem;
    #endif

    #ifdef RX_SHMEM_DISABLE
    #else
        rx_shmem_t rx_shmem;
    #endif

    #ifdef WF_SHMEM_DISABLE
    #else
        // shared with waterfall offload process
        wf_shmem_t wf_shmem;
    #endif

    #ifdef WSPR_SHMEM_DISABLE
    #else
        // shared with WSPR offload process
        wspr_shmem_t wspr_shmem;
    #endif

    #ifdef DRM_SHMEM_DISABLE
    #else
        drm_shmem_t drm_shmem;
    #endif

    log_save_t log_save;    // must be last because of var length
} shmem_t;

extern shmem_t *shmem;

void shmem_init();
void sig_arm(int signal, funcPI_t handler, int flags=0);
void shmem_ipc_invoke(int signal, int which=0, int wait=1);
int shmem_ipc_poll(int signal, int poll_msec, int which=0);
void shmem_ipc_setup(const char *pname, int signal, funcPI_t func);

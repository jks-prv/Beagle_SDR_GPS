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

// Copyright (c) 2024 John Seamons, ZL4VO/KF6VO

#pragma once

typedef struct {
    iq_buf_t iq_buf[MAX_RX_CHANS];
} rx_shmem_t;

#include "shmem_config.h"

#ifdef DRM
    // RX_SHMEM_DISABLE defined by DRM.h
#else
    #ifdef MULTI_CORE
        //#define RX_SHMEM_DISABLE_TEST
        #ifdef RX_SHMEM_DISABLE_TEST
            #warning dont forget to remove RX_SHMEM_DISABLE_TEST
            #define RX_SHMEM_DISABLE
        #else
            // shared memory enabled
        #endif
    #else
        #define RX_SHMEM_DISABLE
    #endif
#endif

#include "shmem.h"

#ifdef RX_SHMEM_DISABLE
    extern rx_shmem_t *rx_shmem_p;
    #define RX_SHMEM rx_shmem_p
#else
    #define RX_SHMEM (&shmem->rx_shmem)
#endif

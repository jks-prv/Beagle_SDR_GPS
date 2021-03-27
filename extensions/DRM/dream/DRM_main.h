/******************************************************************************\
 * Technische Universitaet Darmstadt, Institut fuer Nachrichtentechnik
 * Copyright (c) 2004
 *
 * Author(s):
 *	Volker Fischer, Stephane Fillod, Julian Cable
 *
 * Description: main programme for console mode
 *
 *
 ******************************************************************************
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
\******************************************************************************/

#include "DRM.h"
#include "types.h"
#include "misc.h"
#include "str.h"
#include "printf.h"
#include "kiwi_assert.h"
#include "coroutines.h"
#include "data_pump.h"
#include "timer.h"

#ifdef DRM_SHMEM_DISABLE
    void drm_next_task(const char *id);
#else
    #define drm_next_task(...)
#endif

#define DRM_LOWEST_SIGSRATE 24000
#if MAX_SND_RATE > DRM_LOWEST_SIGSRATE
    #error MAX_SND_RATE > DRM_LOWEST_SIGSRATE
#endif

static const char *drm_argv[] = {
    "drm",
    
    // this is the lowest internal processing srate and supports
    // both Kiwi 12k and 20.25k passband modes
    "--sigsrate", STRINGIFY_DEFINE(DRM_LOWEST_SIGSRATE),

    // audio output srate, now set in DRM_loop() to Kiwi 12k or 20.25k (snd_rate)
    //"--audsrate", "12000",

    "-c", "8",
    "-f", "drm.dat",
    //"-f", "/root/kiwi.config/samples/Kuwait.15110.1.12k.iq.wav",
    //"-f", "/root/kiwi.config/samples/Kuwait.12k.iq.be12",
    //"-f", "/root/kiwi.config/samples/RFI.3965.12k.iq.be12",
    "-O", "kiwi",
    "--reverb", "1"
    //, "-w", "/root/dout.wav"
};

//#define USE_MEASURE_TIME
#ifdef USE_MEASURE_TIME
    #define MEASURE_TIME(id, acc, func) \
        { \
            u4_t start = timer_us(); \
            func; \
            drm_t *drm = &DRM_SHMEM->drm[(int) FROM_VOID_PARAM(TaskGetUserParam())]; \
            drm->MeasureTime[acc] += timer_us() - start; \
            static u4_t last_time; \
            u4_t now = timer_sec(); \
            if (now != last_time) { \
                printf("%s%d ", id, drm->MeasureTime[acc]/1000); fflush(stdout); \
                drm->MeasureTime[acc] = 0; \
                last_time = now; \
            } \
        }
#else
    #define MEASURE_TIME(id, acc, func) func;
#endif


void DRM_loop(int rx_chan);

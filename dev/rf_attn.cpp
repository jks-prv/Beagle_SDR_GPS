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

// Copyright (c) 2023 John Seamons, ZL4VO/KF6VO

#include "types.h"
#include "kiwi.h"
#include "config.h"
#include "cfg.h"
#include "fpga.h"
#include "printf.h"

//#define RF_ATTN_DBG
#ifdef RF_ATTN_DBG
	#define prf(fmt, ...) \
	    if (!debug) printf(fmt,  ## __VA_ARGS__);
#else
	#define prf(fmt, ...)
#endif

float rf_attn_validate(float attn_dB)
{
    return CLAMP(attn_dB, 0, 31.5);
}

void rf_attn_set(float attn_dB)
{
    if (kiwi.model == KiwiSDR_1) return;
    attn_dB = rf_attn_validate(attn_dB);
    lprintf("rf_attn %.1f\n", attn_dB);
    bool debug = false;
    
    #define CTRL_ATTN_CLK   CTRL_SER_CLK
    #define CTRL_ATTN_LE    CTRL_SER_LE_CSN
    #define CTRL_ATTN_DATA  CTRL_SER_DATA

    ctrl_set_ser_dev(CTRL_SER_ATTN);
        prf("rf_attn %.1f dB = ", attn_dB);

        // Psemi PE4312 protocol
        do {
            ctrl_clr_set(CTRL_ATTN_CLK | CTRL_ATTN_DATA | CTRL_ATTN_LE, 0);
            int i_dB = (int) (attn_dB * 2);
            for (int i = 5; i >= 0; i--) {
                int b = i_dB & (1 << i);
                ctrl_clr_set(CTRL_ATTN_DATA, b? CTRL_ATTN_DATA : 0);
                ctrl_positive_pulse(CTRL_ATTN_CLK);
                if (i != 0) { prf("%d ", b>>1) } else { prf(".%d", b*5) };
            }
            ctrl_clr_set(CTRL_ATTN_DATA, 0);
            ctrl_positive_pulse(CTRL_ATTN_LE);
            prf("\n");
            //debug = true;
        } while (debug);
    ctrl_clr_ser_dev();
}

void rf_attn_init()
{
    //printf_highlight(1, "rf_attn");
    float attn_dB = cfg_float("init.rf_attn", NULL, CFG_REQUIRED);
    attn_dB = rf_attn_validate(attn_dB);
    lprintf("rf_attn INIT %.1f\n", attn_dB);
    rf_attn_set(attn_dB);
    kiwi.rf_attn_dB = attn_dB;
}

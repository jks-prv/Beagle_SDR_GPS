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

// Copyright (c) 2023 John Seamons, ZL/KF6VO

#include "types.h"
#include "kiwi.h"
#include "config.h"
#include "fpga.h"
#include "support/printf.h"

//#define RF_ATTN_DBG
#ifdef RF_ATTN_DBG
	#define rf_attn_prf(fmt, ...) \
	    printf(fmt,  ## __VA_ARGS__)
#else
	#define rf_attn_prf(fmt, ...)
#endif

void rf_attn_set(float attn_dB)
{
    #define CTRL_ATTN_CLK   CTRL_SER_CLK
    #define CTRL_ATTN_LE    CTRL_SER_LE_CSN
    #define CTRL_ATTN_DATA  CTRL_SER_DATA

    ctrl_set_ser_dev(CTRL_SER_ATTN);
        rf_attn_prf("rf_attn %.1f dB = ", attn_dB);
        ctrl_clr_set(CTRL_ATTN_CLK | CTRL_ATTN_LE, 0);
        ctrl_positive_pulse(CTRL_ATTN_CLK);
        int i_dB = (int) (attn_dB * 2);
        for (int i = 32; i != 0; i >>= 1) {
            int b = i_dB & i;
            ctrl_clr_set(CTRL_ATTN_DATA, b? CTRL_ATTN_DATA : 0);
            ctrl_positive_pulse(CTRL_ATTN_CLK);
            if (i != 1) rf_attn_prf("%d ", b>>1); else rf_attn_prf(".%d", b*5);
        }
        ctrl_positive_pulse(CTRL_ATTN_LE);
        rf_attn_prf("\n");
    ctrl_clr_ser_dev();
}

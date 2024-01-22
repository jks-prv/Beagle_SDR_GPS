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
#include "gps_fe.h"
#include "fpga.h"
#include "support/printf.h"

//#define GPS_FE_DBG
#ifdef GPS_FE_DBG
	#define prf(fmt, ...) \
	    if (!debug) printf(fmt,  ## __VA_ARGS__)
#else
	#define prf(fmt, ...)
#endif

#define MAX_CONF1   0
#define MAX_CONF2   1
#define MAX_CONF3   2
#define MAX_PLLCONF 3
#define MAX_DIV     4
#define MAX_FDIV    5
#define MAX_STRM    6
#define MAX_CLK     7
#define MAX_TEST1   8
#define MAX_TEST2   9

// all registers are 28-bit, sent msb first [27:0] followed by 4-bit reg num [3:0]
// NB: on ARM little-endian, bitfield structs are ordered lsb first

// conf1
enum { ANT_BIAS_OFF=0, ANT_BIAS_ON=1 };
enum { LNA_BIAS_SEL=0, LNA2=1, LNA1=2 };

// conf2
enum { DRV_CMOS=0, DRV_DIFF=1, DRV_ANALOG=2 };
enum { BITS_1=0, BITS_2=2, BITS_3=4 };
enum { FMT_UNSGN=0, FMT_SGN_MAG=1, FMT_2S_COMPL=2 };
enum { AGC_AUTO=0, AGC_MAN=2 };
enum { I_ONLY=0, IQ_BOTH=1 };

                                    // PON pgm = 0 (serial iface) defaults
struct {
    struct {
        u4_t reg_no;
        union {
            struct {
                bf_t fgain      : 1;    // 1
                bf_t fcenx      : 1;    // 1
                bf_t f3or5      : 1;    // 0
                bf_t fbw        : 2;    // 00
                bf_t fcen       : 6;    // 00_1101
                bf_t anten      : 1;    // 1
                bf_t mixen      : 1;    // 1
                bf_t lnamode    : 2;    // 00
                bf_t mixpole    : 1;    // 0
                bf_t resv       : 10;   // 10_0010_1001
                bf_t idle       : 1;    // 0
                bf_t chipen     : 1;    // 1
            };
            u4_t word;
        };
    } conf1 = { MAX_CONF1, { 1, 1, 0, 0, 0xd, ANT_BIAS_ON, 1, LNA_BIAS_SEL, 0, 0x229, 0, 1 }};
        //           2222 2222 1111 1111 1100 0000 0000
        //           7654 3210 9876 5432 1098 7654 3210
        // A2919A3 = 1010_0010_1001_0001_1001_1010_0011 LNA_BIAS_SEL
        // A2939A3 = 1010_0010_1001_0011_1001_1010_0011 LNA2
        //           ei.. .... .... pllm accc cccb b3xg

    struct {
        u4_t reg_no;
        union {
            struct {
                bf_t dieid      : 2;    // 00
                bf_t resv1      : 2;    // 11   NB: table 24 def val col error?
                bf_t drvcfg     : 2;    // 00
                bf_t bits       : 3;    // 010
                bf_t format     : 2;    // 01
                bf_t agcmode    : 2;    // 00
                bf_t resv2      : 2;    // 00
                bf_t gainref    : 12;   // 170. = 0000_1010_1010
                bf_t iqen       : 1;    // 0
            };
            u4_t word;
        };
    } conf2 = { MAX_CONF2, { 0, 3, DRV_CMOS, BITS_2, FMT_SGN_MAG, AGC_AUTO, 0, 170, I_ONLY }};
        //           2222 2222 1111 1111 1100 0000 0000
        //           7654 3210 9876 5432 1098 7654 3210
        // 055028C = 0000_0101_0101_0000_0010_1000_1100
        //           eggg gggg gggg g..a affb bbdd ..dd

    struct {
        u4_t reg_no;
        union {
            struct {
                bf_t strmrst    : 1;    // 0
                bf_t datsyncen  : 1;    // 0
                bf_t timesyncen : 1;    // 1
                bf_t stampen    : 1;    // 1
                bf_t strmbits   : 2;    // 01
                bf_t resv1      : 3;    // 111
                bf_t strmstop   : 1;    // 0
                bf_t strmstart  : 1;    // 0
                bf_t strmen     : 1;    // 0
                bf_t pgaqen     : 1;    // 0
                bf_t pgaien     : 1;    // 1
                bf_t resv2      : 1;    // 1
                bf_t fhipen     : 1;    // 1
                bf_t resv3      : 4;    // 1111
                bf_t hiloaden   : 1;    // 0
                bf_t resv4      : 1;    // 1
                bf_t gainin     : 6;    // 11_1010
            };
            u4_t word;
        };
    } conf3 = { MAX_CONF3, { 0, 0, 1, 1, 1, 7, 0, 0, 0, 0, 1, 1, 1, 0xf, 0, 1, 0x3a }};
        // EAFE1DC

    struct {
        u4_t reg_no;
        union {
            struct {
                bf_t resv1      : 2;    // 00
                bf_t pwrsav     : 1;    // 0
                bf_t int_pll    : 1;    // 1
                bf_t resv2      : 4;    // 0000
                bf_t pfden      : 1;    // 0
                bf_t icp        : 1;    // 0
                bf_t ldmux      : 4;    // 0000
                bf_t resv3      : 5;    // 1_0000
                bf_t ixtal      : 2;    // 01
                bf_t refdiv     : 2;    // 11
                bf_t resv4      : 1;    // 1
                bf_t refouten   : 1;    // 1
                bf_t resv5      : 3;    // 100
            };
            u4_t word;
        };
    } pllconf = { MAX_PLLCONF, { 0, 0, 1, 0, 0, 0, 0, 0x10, 1, 3, 1, 1, 4 }};
        // 9EC0008

    struct {
        u4_t reg_no;
        union {
            struct {
                bf_t resv       : 3;    // 000
                bf_t rdiv       : 10;   // 16.
                bf_t ndiv       : 15;   // 1536.
            };
            u4_t word;
        };
    } div = { MAX_DIV, { 0, 16, 1536 }};
        // 0C00080

    struct {
        u4_t reg_no;
        union {
            struct {
                bf_t resv       : 8;    // 0111_0000
                bf_t fdiv       : 20;   // 0x80000
            };
            u4_t word;
        };
    } fdiv = { MAX_FDIV, { 0x70, 0x80000 }};
        // 8000070

    struct {
        u4_t reg_no;
        union {
            struct {
                bf_t resv       : 28;   // 0x8000000
            };
            u4_t word;
        };
    } strm = { MAX_STRM, { 0x8000000 }};
        // 8000000

    struct {
        u4_t reg_no;
        union {
            struct {
                bf_t mode       : 1;    // 0
                bf_t resv       : 1;    // 1
                bf_t adcclk     : 1;    // 0
                bf_t fclkin     : 1;    // 0
                bf_t m_cnt      : 12;   // 1563.
                bf_t l_cnt      : 12;   // 256.
            };
            u4_t word;
        };
    } clk = { MAX_CLK, { 0, 1, 0, 0, 1563, 256 }};
        // 10061B2

    struct {
        u4_t reg_no;
        union {
            struct {
                bf_t resv       : 28;   // 0x1e0f401
            };
            u4_t word;
        };
    } test1 = { MAX_TEST1, { 0x1e0f401 }};
        // 1E0F401

    struct {
        u4_t reg_no;
        union {
            struct {
                bf_t fcenmsb    : 1;    // 0
                bf_t resv       : 27;   // 0x28c0402
            };
            u4_t word;
        };
    } test2 = { MAX_TEST2, { 0, 0x28c0402 >> 1 }};
        // 28C0402
} max;

static void _write_reg(const char *reg_s, bool debug, int reg_no, u4_t word)
{
    int i, b;

    // MAX2769B protocol
    #define CTRL_GPS_CLK    CTRL_SER_CLK
    #define CTRL_GPS_CSN    CTRL_SER_LE_CSN
    #define CTRL_GPS_DATA   CTRL_SER_DATA

    ctrl_clr_set(CTRL_GPS_CLK | CTRL_GPS_CSN, 0);
    prf("GPS_FE write_reg word: ");
    for (i = 27; i >= 0; i--) {
        b = word & (1 << i);
        ctrl_clr_set(CTRL_GPS_DATA, b? CTRL_GPS_DATA : 0);
        ctrl_positive_pulse(CTRL_GPS_CLK);
        prf("%d%s", b? 1:0, (i != 0 && (i%4) == 0)? "_" : "");
    }
    prf("(%07x) max.%s: ", word, reg_s);
    for (i = 3; i >= 0; i--) {
        b = reg_no & (1 << i);
        ctrl_clr_set(CTRL_GPS_DATA, b? CTRL_GPS_DATA : 0);
        ctrl_positive_pulse(CTRL_GPS_CLK);
        prf("%d", b? 1:0);
    }
    prf("(%d)\n", reg_no);
    ctrl_clr_set(CTRL_GPS_DATA, CTRL_GPS_CSN);
    ctrl_positive_pulse(CTRL_GPS_CLK);
    ctrl_clr_set(CTRL_GPS_CSN, 0);      // CSN returns to zero, but then idles high
    ctrl_clr_set(0, CTRL_GPS_CSN);
    kiwi_msleep(1);
}

void gps_fe_init()
{
    bool debug = false;

    //if (kiwi.model == KiwiSDR_1) return;
    
    #if 0
        prf("GPS_FE reg defaults:\n");
        #define show_reg_default(reg) prf("GPS_FE max." #reg " %07x\n", max.reg.word);
        show_reg_default(conf1);
        show_reg_default(conf2);
        show_reg_default(conf3);
        show_reg_default(pllconf);
        show_reg_default(div);
        show_reg_default(fdiv);
        show_reg_default(strm);
        show_reg_default(clk);
        show_reg_default(test1);
        show_reg_default(test2);
    #endif

    // Need to change max.conf1.lnaconf to LNA2 from PON default LNA_BIAS_SEL.
    // Otherwise passive antennas that don't draw bias current won't be seen on LNA2 port.
    max.conf1.lnamode = LNA2;

    #define write_reg(reg) _write_reg(#reg, debug, max.reg.reg_no, max.reg.word);
    ctrl_set_ser_dev(CTRL_SER_GPS);
        do {
            write_reg(conf1);
            write_reg(conf2);
            write_reg(conf3);
            write_reg(pllconf);
            write_reg(div);
            write_reg(fdiv);
            write_reg(strm);
            write_reg(clk);
            write_reg(test1);
            write_reg(test2);
            //debug = true;
        } while (debug);
    ctrl_clr_ser_dev();
}

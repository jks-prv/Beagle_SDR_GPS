//////////////////////////////////////////////////////////////////////////
// Homemade GPS Receiver
// Copyright (C) 2013 Andrew Holme
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
// http://www.holmea.demon.co.uk/GPS/Main.htm
//////////////////////////////////////////////////////////////////////////

// Copyright (c) 2015-2022 John Seamons, ZL4VO/KF6VO

#include "types.h"
#include "config.h"
#include "kiwi.h"
#include "misc.h"
#include "peri.h"
#include "spi.h"
#include "coroutines.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

// debugging
//#define SHOW_CHECK_PMUX
//#define SHOW_GPIO_STATE

static volatile u4_t *prcm_m, *pmux_m;
volatile u4_t *spi_m, *gpio_m[NGPIO];
static bool init;

#ifdef CPU_AM3359
 static u4_t gpio_base[NGPIO] = { GPIO0_BASE, GPIO1_BASE, GPIO2_BASE, GPIO3_BASE };

 static u4_t gpio_pmux_reg_off[NGPIO][32] = {
 //                0      1      2      3      4      5      6      7      8      9     10     11     12     13     14     15     16     17     18     19     20     21     22     23     24     25     26     27     28     29     30     31
 /* gpio0 */	{ 0x000, 0x000, 0x950, 0x954, 0x958, 0x95c, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x980, 0x984, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x820, 0x824, 0x000, 0x000, 0x828, 0x82c, 0x000, 0x000, 0x870, 0x874 },
 /* gpio1 */	{ 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x830, 0x834, 0x838, 0x83c, 0x840, 0x844, 0x848, 0x84c, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x878, 0x87c, 0x000, 0x000 },
 /* gpio2 */	{ 0x000, 0x88c, 0x890, 0x894, 0x898, 0x89c, 0x000, 0x000, 0x8a8, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000 },
 /* gpio3 */	{ 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000 },
 };
#endif

#ifdef CPU_AM5729
 static u4_t gpio_base[NGPIO] = { GPIO1_BASE, GPIO2_BASE, GPIO3_BASE, GPIO4_BASE, GPIO5_BASE, GPIO6_BASE, GPIO7_BASE, GPIO8_BASE };

 // NB: when both non-zero first entry is ball with GPIO, second should be set "driver off" (mode 15)
 static u4_t gpio_pmux_reg_off[NGPIO*NBALL][32] = {
 //                0       1       2       3       4       5       6       7       8       9      10      11      12      13      14      15      16      17      18      19      20      21      22      23      24      25      26      27      28      29      30      31
 /* gpio1 */	{ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 },
                { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 },

 /* gpio2 */	{ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 },
                { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 },

 /* gpio3 */	{ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x150c, 0x1510, 0x1514, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 },
                { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 },

 /* gpio4 */	{ 0x0000, 0x0000, 0x0000, 0x1570, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x1588, 0x158c, 0x1590, 0x0000, 0x1598, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x15ac, 0x15B0, 0x0000, 0x15b8, 0x15bc, 0x0000, 0x0000 },
                { 0x0000, 0x0000, 0x0000, 0x15b4, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 },

 /* gpio5 */	{ 0x16ac, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 },
                { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 },

 /* gpio6 */	{ 0x0000, 0x0000, 0x0000, 0x0000, 0x16e8, 0x16ec, 0x16f0, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x1680, 0x0000, 0x1688, 0x168c, 0x0000, 0x1694, 0x1698, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 },
                { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x1730, 0x0000, 0x1544, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 },

 /* gpio7 */	{ 0x0000, 0x0000, 0x0000, 0x1440, 0x1444, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x17b4, 0x0000, 0x0000, 0x17c0, 0x17c4, 0x17c8, 0x17cc, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 },
                { 0x0000, 0x0000, 0x0000, 0x157c, 0x1578, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x169c, 0x14f0, 0x16b4, 0x16b8, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 },

 /* gpio8 */	{ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x1620, 0x1624, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 },
                { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x172c, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 },
 };
#endif

#ifdef CPU_TDA4VM
 static u4_t gpio_base[NGPIO] = { GPIO0_BASE };

 // currently ignores second ball assignments since not needed by kiwi
 static u4_t gpio_pmux_reg_off[NGPIO][GPIO_NPINS];      // initialized via loop below
#endif

pin_t eeprom_pins[EE_NPINS];

//					  { bank, bit, pin, eeprom_offset }
gpio_t GPIO_NONE	= { 0xff, 0xff, 0xff, 0xff };


// P9 connector

#ifdef CPU_AM3359
//                        { bank, bit, pin,         eeprom_offset }
    gpio_t SPIn_SCLK	= { GPIO0,  2, PIN(P9, 22),  88 };
    gpio_t SPIn_MISO	= { GPIO0,  3, PIN(P9, 21),  90 };
    gpio_t SPIn_MOSI	= { GPIO0,  4, PIN(P9, 18),  92 };
    gpio_t SPIn_CS0		= { GPIO0,  5, PIN(P9, 17),  94 };
    gpio_t SPIn_CS1		= { GPIO1, 19, PIN(P9, 16), 158 };	// not the actual spi_cs1 from hardware, but our PIO emulation

    gpio_t FPGA_PGM		= { GPIO1, 28, PIN(P9, 12), 160 };
    gpio_t FPGA_INIT	= { GPIO1, 18, PIN(P9, 14), 156 };

    gpio_t P911 		= { GPIO0, 30, PIN(P9, 11), 124 };
    gpio_t P913 		= { GPIO0, 31, PIN(P9, 13), 118 };
    gpio_t P915 		= { GPIO1, 16, PIN(P9, 15), 152 };
    gpio_t CMD_READY    = { GPIO1, 17, PIN(P9, 23), 154 };
    gpio_t SND_INTR		= { GPIO0, 15, PIN(P9, 24), 112 };
    gpio_t P926 		= { GPIO0, 14, PIN(P9, 26), 110 };
#endif

#ifdef CPU_AM5729
//                        { bank, bit, pin,         eeprom_offset }
    gpio_t SPIn_SCLK	= { GPIO7, 14, PIN(P9, 22),  88 };  // second ball
    gpio_t SPIn_MISO	= { GPIO7, 15, PIN(P9, 21),  90 };  // second ball
    gpio_t SPIn_MOSI	= { GPIO7, 16, PIN(P9, 18),  92 };
    gpio_t SPIn_CS0		= { GPIO7, 17, PIN(P9, 17),  94 };
    gpio_t SPIn_CS1		= { GPIO4, 26, PIN(P9, 16), 158 };	// not the actual spi_cs1 from hardware, but our PIO emulation

    gpio_t FPGA_PGM		= { GPIO5,  0, PIN(P9, 12), 160 };
    gpio_t FPGA_INIT	= { GPIO4, 25, PIN(P9, 14), 156 };

    gpio_t P911 		= { GPIO8, 17, PIN(P9, 11), 124 };
    gpio_t P913 		= { GPIO6, 12, PIN(P9, 13), 118 };
    gpio_t P915 		= { GPIO3, 12, PIN(P9, 15), 152 };
    gpio_t CMD_READY    = { GPIO7, 11, PIN(P9, 23), 154 };
    gpio_t SND_INTR		= { GPIO6, 15, PIN(P9, 24), 112 };
    gpio_t P926 		= { GPIO6, 14, PIN(P9, 26), 110 };
#endif

#ifdef CPU_TDA4VM
//                        { bank,  bit, pin,          eeprom_offset }
    gpio_t SPIn_SCLK	= { GPIO0,  38, PIN(P9, 22),  88 };
    gpio_t SPIn_MISO	= { GPIO0,  39, PIN(P9, 21),  90 }; // d0
    gpio_t SPIn_MOSI	= { GPIO0,  40, PIN(P9, 18),  92 }; // d1
    gpio_t SPIn_CS0		= { GPIO0,  28, PIN(P9, 17),  94 };
    gpio_t SPIn_CS1		= { GPIO0,  94, PIN(P9, 16), 158 }; // not the actual spi_cs1 from hardware, but our PIO emulation

    gpio_t FPGA_PGM		= { GPIO0,  45, PIN(P9, 12), 160 };
    gpio_t FPGA_INIT	= { GPIO0,  93, PIN(P9, 14), 156 };

    gpio_t P911 		= { GPIO0,   1, PIN(P9, 11), 124 };
    gpio_t P913 		= { GPIO0,   2, PIN(P9, 13), 118 };
    gpio_t P915 		= { GPIO0,  47, PIN(P9, 15), 152 };
    gpio_t CMD_READY    = { GPIO0,  10, PIN(P9, 23), 154 };
    gpio_t SND_INTR		= { GPIO0, 119, PIN(P9, 24), 112 };
    gpio_t P926 		= { GPIO0, 118, PIN(P9, 26), 110 };
#endif


// P8 connector

#ifdef CPU_AM3359
//                        { bank, bit, pin,         eeprom_offset }
    gpio_t JTAG_TCK		= { GPIO2,  5, PIN(P8,  9), 172 };
    gpio_t JTAG_TMS		= { GPIO2,  4, PIN(P8, 10), 174 };
    gpio_t JTAG_TDI		= { GPIO2,  2, PIN(P8,  7), 170 };
    gpio_t JTAG_TDO		= { GPIO2,  3, PIN(P8,  8), 176 };
    gpio_t P811			= { GPIO1, 13, PIN(P8, 11), 146 };
    gpio_t P812			= { GPIO1, 12, PIN(P8, 12), 144 };
    gpio_t P813			= { GPIO0, 23, PIN(P8, 13), 118 };
    gpio_t P814			= { GPIO0, 26, PIN(P8, 14), 120 };
    gpio_t P815			= { GPIO1, 15, PIN(P8, 15), 150 };
    gpio_t P816			= { GPIO1, 14, PIN(P8, 16), 148 };
    gpio_t P817			= { GPIO0, 27, PIN(P8, 17), 122 };
    gpio_t P818			= { GPIO2,  1, PIN(P8, 18), 168 };
    gpio_t P819			= { GPIO0, 22, PIN(P8, 19), 116 };
    gpio_t P826			= { GPIO1, 29, PIN(P8, 26), 162 };

    gpio_t BOOT_BTN     = { GPIO2,  8, PIN(P8, 43), 182 };
#endif

#ifdef CPU_AM5729
//                        { bank, bit, pin,         eeprom_offset }
    gpio_t JTAG_TDI		= { GPIO6,  5, PIN(P8,  7), 170 };
    gpio_t JTAG_TDO		= { GPIO6,  6, PIN(P8,  8), 176 };
    gpio_t JTAG_TCK		= { GPIO6, 18, PIN(P8,  9), 172 };
    gpio_t JTAG_TMS		= { GPIO6,  4, PIN(P8, 10), 174 };
    gpio_t P811			= { GPIO3, 11, PIN(P8, 11), 146 };
    gpio_t P812			= { GPIO3, 10, PIN(P8, 12), 144 };
    gpio_t P813			= { GPIO4, 11, PIN(P8, 13), 118 };
    gpio_t P814			= { GPIO4, 13, PIN(P8, 14), 120 };
    gpio_t P815			= { GPIO4,  3, PIN(P8, 15), 150 };
    gpio_t P816			= { GPIO4, 29, PIN(P8, 16), 148 };
    gpio_t P817			= { GPIO8, 18, PIN(P8, 17), 122 };
    gpio_t P818			= { GPIO4,  9, PIN(P8, 18), 168 };
    gpio_t P819			= { GPIO4, 10, PIN(P8, 19), 116 };
    gpio_t P826			= { GPIO4, 28, PIN(P8, 26), 162 };
#endif

#ifdef CPU_TDA4VM
//                        { bank,  bit, pin,         eeprom_offset }
    gpio_t JTAG_TCK		= { GPIO0,  17, PIN(P8,  9), 172 };
    gpio_t JTAG_TMS		= { GPIO0,  16, PIN(P8, 10), 174 };
    gpio_t JTAG_TDI		= { GPIO0,  15, PIN(P8,  7), 170 };
    gpio_t JTAG_TDO		= { GPIO0,  14, PIN(P8,  8), 176 };
    gpio_t P811			= { GPIO0,  60, PIN(P8, 11), 146 };
    gpio_t P812			= { GPIO0,  59, PIN(P8, 12), 144 };
    gpio_t P813			= { GPIO0,  89, PIN(P8, 13), 118 };
    gpio_t P814			= { GPIO0,  75, PIN(P8, 14), 120 };
    gpio_t P815			= { GPIO0,  61, PIN(P8, 15), 150 };
    gpio_t P816			= { GPIO0,  62, PIN(P8, 16), 148 };
    gpio_t P817			= { GPIO0,   3, PIN(P8, 17), 122 };
    gpio_t P818			= { GPIO0,   4, PIN(P8, 18), 168 };
    gpio_t P819			= { GPIO0,  88, PIN(P8, 19), 116 };
    gpio_t P826			= { GPIO0,  51, PIN(P8, 26), 162 };
#endif

static char pmux_deco_s[4][128];

static char *pmux_deco(int i, u4_t pmux, gpio_t gpio)
{
    #ifdef CPU_TDA4VM
        int drive = pmux & PMUX_DRIVE;
        kiwi_snprintf_buf(pmux_deco_s[i], "<%s, %s, %s, %s, m%-2d>",
            (drive == PMUX_NOM)? " NOM" : ((drive == PMUX_FAST)? "FAST" : "SLOW"),
            (pmux & PMUX_RXEN)? "RX":"  ", (!(pmux & PMUX_TXDIS))? "TX":"  ",
            (pmux & PMUX_PDIS)? "Px" : ((pmux & PMUX_PU)? "PU":"PD"), pmux & PMUX_MODE);
    #else
        kiwi_snprintf_buf(pmux_deco_s[i], "<%s, %s, %s, %s, m%-2d>",
            (pmux & PMUX_SLOW)? "SLOW":"FAST", (pmux & PMUX_RXEN)? "RX":"  ", GPIO_isOE(gpio)? "OE":"  ",
            (pmux & PMUX_PDIS)? "Px" : ((pmux & PMUX_PU)? "PU":"PD"), pmux & PMUX_MODE);
    #endif
    return pmux_deco_s[i];
}

static bool check_pmux(const char *name, gpio_t gpio, gpio_dir_e dir, u4_t pmux_val1, u4_t pmux_val2)
{
    u4_t _pmux, pmux_reg_off, mode, pmux_pin_attr;
    bool val1_ok, val2_ok;
    bool bad = false;
    
#ifdef CPU_AM3359
    pmux_reg_off = gpio_pmux_reg_off[gpio.bank][gpio.bit];
    check(pmux_reg_off != 0);
    _pmux = pmux_m[pmux_reg_off>>2];
    
    val1_ok = val2_ok = true;
    if (pmux_val1 && _pmux != pmux_val1) val1_ok = false;
    if (pmux_val2 == PMUX_NONE || _pmux != pmux_val2) val2_ok = false;

    if (val1_ok || val2_ok) {
        #ifdef SHOW_CHECK_PMUX
            printf("PMUX %d_%-2d %s.%-2d %-9s 0x%04x OK  got 0x%02x%s want %s0x%02x%s ",
                GPIO_BANK(gpio), gpio.bit, (gpio.pin & P9)? "P9":"P8", gpio.pin & PIN_BITS, name, pmux_reg_off,
                _pmux, pmux_deco(0, _pmux, gpio), val1_ok? "*":" ", pmux_val1, pmux_deco(1, pmux_val1, gpio));
            if (pmux_val2 != PMUX_NONE)
                printf("or %s0x%02x%s ", val2_ok? "*":" ", pmux_val2, pmux_deco(0, pmux_val2, gpio));
            printf("\n");
        #endif
    } else {
        printf("PMUX %d_%-2d %s.%-2d %-9s 0x%04x NOTE got 0x%02x%s want  0x%02x%s ",
            GPIO_BANK(gpio), gpio.bit, (gpio.pin & P9)? "P9":"P8", gpio.pin & PIN_BITS, name, pmux_reg_off,
            _pmux, pmux_deco(0, _pmux, gpio), pmux_val1, pmux_deco(1, pmux_val1, gpio));
        if (pmux_val2 != PMUX_NONE)
            printf("or  0x%02x%s ", pmux_val2, pmux_deco(0, pmux_val2, gpio));
        printf("\n");
        //bad = true;
    }

    #ifdef SHOW_CHECK_PMUX
        printf("\tPMUX check %-9s GPIO %d_%-2d %s.%-2d eeprom %3d/0x%02x has attr 0x%02x %s\n",
            name, gpio.bank, gpio.bit, (gpio.pin & P9)? "P9":"P8", gpio.pin & PIN_BITS, gpio.eeprom_off, gpio.eeprom_off,
            _pmux, pmux_deco(0, _pmux, gpio));
    #endif
    
    pmux_pin_attr = _pmux & PMUX_BITS;
#endif

#ifdef CPU_AM5729
    int p_bank = gpio.bank*2;
    u4_t _pmux2, pmux_reg2_off;

    pmux_reg_off = gpio_pmux_reg_off[p_bank][gpio.bit];
    pmux_reg2_off = gpio_pmux_reg_off[p_bank+1][gpio.bit];

    if (pmux_reg_off == 0 || pmux_reg_off >= MMAP_SIZE) {
        lprintf("PMUX %d_%-2d %s.%-2d %-9s 0x%04x BAD PMUX REG OFFSET\n",
            GPIO_BANK(gpio), gpio.bit, (gpio.pin & P9)? "P9":"P8", gpio.pin & PIN_BITS, name, pmux_reg_off);
        panic("pmux_reg_off");
    }

    if (pmux_reg2_off >= MMAP_SIZE) {
        lprintf("PMUX      %s.%-2d %-9s 0x%04x BAD PMUX2 REG OFFSET\n",
            (gpio.pin & P9)? "P9":"P8", gpio.pin & PIN_BITS, name, pmux_reg2_off);
        panic("pmux_reg2_off");
    }

    _pmux = pmux_m[pmux_reg_off>>2];
    mode = _pmux & PMUX_MODE;
    val1_ok = val2_ok = true;
    if (pmux_val1 && _pmux != pmux_val1) val1_ok = false;
    if (pmux_val2 == PMUX_NONE || _pmux != pmux_val2) val2_ok = false;
    
    /*
    lprintf("PMUX %d_%-2d %s.%-2d %-9s pmux_regs 0x%08x 0x%08x gpio_reg_base 0x%08x gpio_OE 0x%08x\n",
        GPIO_BANK(gpio), gpio.bit, (gpio.pin & P9)? "P9":"P8", gpio.pin & PIN_BITS, name,
        pmux_reg_off, pmux_reg2_off, &GPIO_REVISION(gpio), &GPIO_OE(gpio));
    */

    if (val1_ok || val2_ok) {
        #ifdef SHOW_CHECK_PMUX
            lprintf("PMUX %d_%-2d %s.%-2d %-9s 0x%04x OK  got 0x%08x%s want %s0x%08x%s ",
                GPIO_BANK(gpio), gpio.bit, (gpio.pin & P9)? "P9":"P8", gpio.pin & PIN_BITS, name, pmux_reg_off,
                _pmux, pmux_deco(0, _pmux, gpio), val1_ok? "*":" ", pmux_val1, pmux_deco(1, pmux_val1, gpio));
            if (pmux_val2 != PMUX_NONE)
                lprintf("or %s0x%08x%s ", val2_ok? "*":" ", pmux_val2, pmux_deco(0, pmux_val2, gpio));
            lprintf("\n");
        #endif
    } else {
        //lprintf("PMUX %d_%-2d %s.%-2d %-9s 0x%04x 0x%04x %p %p BAD got 0x%08x%s want 0x%08x%s ",
        //    GPIO_BANK(gpio), gpio.bit, (gpio.pin & P9)? "P9":"P8", gpio.pin & PIN_BITS, name, pmux_reg_off, (&pmux_m[pmux_reg_off>>2] - pmux)*4, &pmux_m[pmux_reg_off>>2], pmux,
        lprintf("PMUX %d_%-2d %s.%-2d %-9s 0x%04x BAD got 0x%08x%s want  0x%08x%s ",
            GPIO_BANK(gpio), gpio.bit, (gpio.pin & P9)? "P9":"P8", gpio.pin & PIN_BITS, name, pmux_reg_off,
            _pmux, pmux_deco(0, _pmux, gpio), pmux_val1, pmux_deco(1, pmux_val1, gpio));
        if (pmux_val2 != PMUX_NONE)
            lprintf("or  0x%08x%s ", pmux_val2, pmux_deco(0, pmux_val2, gpio));
        lprintf("\n");
        bad = true;
    }
    
    // check for second ball (if applicable) being disabled so as not to conflict
    /*
    if (pmux_reg2_off != 0) {
        _pmux2 = pmux_m[pmux_reg2_off>>2];
        if (mode != PMUX_OFF) {
            lprintf("\tPMUX %d_%-2d ball_1 %04x=%08x ball_2 %04x=%08x, WAS EXPECTING ball_2 mode PMUX_OFF=15 got=%d\n",
                GPIO_BANK(gpio), gpio.bit, pmux_reg_off, _pmux, pmux_reg2_off, _pmux2, mode);
        }
    }
    */

    #ifdef SHOW_CHECK_PMUX
        lprintf("\tPMUX %d_%-2d %s.%-2d %-9s eeprom %3d/0x%02x has attr 0x%08x%s\n",
            GPIO_BANK(gpio), gpio.bit, (gpio.pin & P9)? "P9":"P8", gpio.pin & PIN_BITS, name,
            gpio.eeprom_off, gpio.eeprom_off,
            _pmux, pmux_deco(0, _pmux, gpio));
    #endif
    
    pmux_pin_attr = 0;
#endif

#ifdef CPU_TDA4VM
    pmux_reg_off = gpio_pmux_reg_off[gpio.bank][gpio.bit];
    check(pmux_reg_off != 0);
    _pmux = pmux_m[pmux_reg_off>>2];
    val1_ok = val2_ok = true;
    if (pmux_val1 && _pmux != pmux_val1) val1_ok = false;
    if (pmux_val2 == PMUX_NONE || _pmux != pmux_val2) val2_ok = false;
    
    if (val1_ok || val2_ok) {
        #ifdef SHOW_CHECK_PMUX
            lprintf("PMUX %d_%-3d %s.%-2d %-9s 0x%03x OK  got 0x%06x%s want %s0x%06x%s ",
                GPIO_BANK(gpio), gpio.bit, (gpio.pin & P9)? "P9":"P8", gpio.pin & PIN_BITS, name, pmux_reg_off,
                _pmux, pmux_deco(0, _pmux, gpio), val1_ok? "*":" ", pmux_val1, pmux_deco(1, pmux_val1, gpio));
            if (pmux_val2 != PMUX_NONE)
                lprintf("or %s0x%06x%s ", val2_ok? "*":" ", pmux_val2, pmux_deco(0, pmux_val2, gpio));
            lprintf("\n");
        #endif
    } else {
        lprintf("PMUX %d_%-3d %s.%-2d %-9s 0x%03x BAD got 0x%06x%s want  0x%06x%s ",
            GPIO_BANK(gpio), gpio.bit, (gpio.pin & P9)? "P9":"P8", gpio.pin & PIN_BITS, name, pmux_reg_off,
            _pmux, pmux_deco(0, _pmux, gpio), pmux_val1, pmux_deco(1, pmux_val1, gpio));
        if (pmux_val2 != PMUX_NONE)
            lprintf("or  0x%06x%s ", pmux_val2, pmux_deco(0, pmux_val2, gpio));
        lprintf("\n");
        bad = true;
    }

    #ifdef SHOW_CHECK_PMUX
        lprintf("\tPMUX check %-9s GPIO %d_%-2d %s.%-2d eeprom %3d/0x%02x has attr 0x%02x %s\n",
            name, gpio.bank, gpio.bit, (gpio.pin & P9)? "P9":"P8", gpio.pin & PIN_BITS, gpio.eeprom_off, gpio.eeprom_off,
            _pmux, pmux_deco(0, _pmux, gpio));
    #endif
    
    pmux_pin_attr = 0;
#endif

    // generate the per-pin info used by eeprom_write()
    u4_t pin = (gpio.eeprom_off - EE_PINS_OFFSET_BASE)/2;
    check(pin < EE_NPINS);
    pin_t *p = &eeprom_pins[pin];
    p->gpio = gpio;
    p->attrs = PIN_USED | (pmux_pin_attr & PIN_PMUX_BITS);
    p->attrs |= (dir == GPIO_DIR_IN)? PIN_DIR_IN : ( (dir == GPIO_DIR_OUT)? PIN_DIR_OUT : PIN_DIR_BIDIR );
    
    return bad;
}

const char *dir_name[] = { "INPUT", "OUTPUT", "BIDIR" };
static bool any_bad;

void _devio_check(const char *name, gpio_t gpio, gpio_dir_e dir, u4_t pmux_val1, u4_t pmux_val2)
{
	//lprintf("DEVIO setup %s %d_%-2d %s\n", name, GPIO_BANK(gpio), gpio.bit, dir_name[dir]);
	any_bad |= check_pmux(name, gpio, dir, pmux_val1, pmux_val2);
}

void _gpio_setup(const char *name, gpio_t gpio, gpio_dir_e dir, u4_t initial, u4_t pmux_val1, u4_t pmux_val2)
{
	if (!isGPIO(gpio)) return;

	any_bad |= check_pmux(name, gpio, dir, pmux_val1 | PMUX_GPIO, (pmux_val2 != PMUX_NONE)? (pmux_val2 | PMUX_GPIO) : PMUX_NONE);

	GPIO_CLR_IRQ0(gpio);
	GPIO_CLR_IRQ1(gpio);
	
	if (dir == GPIO_DIR_IN) {
		#ifdef SHOW_GPIO_STATE
		    lprintf("GPIO setup %-9s %d_%-2d %s.%-2d INPUT\n", name,
                    GPIO_BANK(gpio), gpio.bit, (gpio.pin & P9)? "P9":"P8", gpio.pin & PIN_BITS);
		#endif
		GPIO_INPUT(gpio);
	} else {    // GPIO_DIR_OUT, GPIO_DIR_BIDIR
		if (initial != GPIO_HIZ) {
		    #ifdef SHOW_GPIO_STATE
                lprintf("GPIO setup %-9s %d_%-2d %s.%-2d %-6s initial=%d\n", name,
                    GPIO_BANK(gpio), gpio.bit, (gpio.pin & P9)? "P9":"P8", gpio.pin & PIN_BITS,
                    (dir == GPIO_DIR_OUT)? "OUTPUT":"BIDIR", initial);
			#endif
			GPIO_WRITE_BIT(gpio, initial);
			GPIO_OUTPUT(gpio);
			GPIO_WRITE_BIT(gpio, initial);
		} else {
		    #ifdef SHOW_GPIO_STATE
			    lprintf("GPIO setup %-9s %d_%-2d %s.%-2d %-6s initial=Z\n", name,
                    GPIO_BANK(gpio), gpio.bit, (gpio.pin & P9)? "P9":"P8", gpio.pin & PIN_BITS,
                    (dir == GPIO_DIR_OUT)? "OUTPUT":"BIDIR");
            #endif
			GPIO_INPUT(gpio);
		}
	}

	spin_ms(10);
}

void peri_init()
{
    int i, mem_fd;

    scall("/dev/mem", mem_fd = open("/dev/mem", O_RDWR|O_SYNC));
    
    if (PRCM_BASE) {
        prcm_m = (volatile u4_t *) mmap(
            NULL,
            MMAP_SIZE,
            PROT_READ|PROT_WRITE,
            MAP_SHARED,
            mem_fd,
            PRCM_BASE
        );
        if (prcm_m == MAP_FAILED) sys_panic("mmap prcm");
    }

	for (i = 0; i < NGPIO; i++) {
		gpio_m[i] = (volatile u4_t *) mmap(
			NULL,
			MMAP_SIZE,
			PROT_READ|PROT_WRITE,
			MAP_SHARED,
			mem_fd,
			gpio_base[i]
		);
        if (gpio_m[i] == MAP_FAILED) sys_panic("mmap gpio");
	}

    pmux_m = (volatile u4_t *) mmap(
        NULL,
        MMAP_SIZE,
        PROT_READ|PROT_WRITE,
        MAP_SHARED,
        mem_fd,
        PMUX_BASE
    );
    if (pmux_m == MAP_FAILED) sys_panic("mmap pmux");

#ifdef CPU_AM3359
	if (!use_spidev || debian_ver >= 9) {
#endif
#ifdef CPU_AM5729
    if (1) {
#endif
#ifdef CPU_TDA4VM
    if (0) {
#endif
		spi_m = (volatile u4_t *) mmap(
			NULL,
			MMAP_SIZE,
			PROT_READ|PROT_WRITE,
			MAP_SHARED,
			mem_fd,
			SPI_BASE
		);
        if (spi_m == MAP_FAILED) sys_panic("mmap spi");
	}

    close(mem_fd);

	// power-up the device logic
#ifdef CPU_AM3359
	PRCM_GPIO0 = PRCM_GPIO1 = PRCM_GPIO2 = PRCM_GPIO3 = PRCM_SPI0 = MODMODE_ENA;
#endif
	
#ifdef CPU_AM5729
    #if 1
        #if 0
            real_printf("PRCM_GPIO2=%p before\n", PRCM_GPIO2);
            real_printf("PRCM_GPIO3=%p before\n", PRCM_GPIO3);
            real_printf("PRCM_GPIO4=%p before\n", PRCM_GPIO4);
            real_printf("PRCM_GPIO5=%p before\n", PRCM_GPIO5);
            real_printf("PRCM_GPIO6=%p before\n", PRCM_GPIO6);
            real_printf("PRCM_GPIO7=%p before\n", PRCM_GPIO7);
            real_printf("PRCM_GPIO8=%p before\n", PRCM_GPIO8);
            real_printf("PRCM_SPI2=%p before\n", PRCM_SPI2);
        #endif
    
        // on boot gpio4, gpio8 and spi2 blocks are powered down.
        PRCM_GPIO2 = PRCM_GPIO3 = PRCM_GPIO4 = PRCM_GPIO5 = PRCM_GPIO6 = PRCM_GPIO7 = PRCM_GPIO8 = MODMODE_GPIO_ENA;
        PRCM_SPI2 = MODMODE_SPI_ENA;
        spin_ms(10);
    
        #if 0
            real_printf("PRCM_GPIO2=%p after\n", PRCM_GPIO2);
            real_printf("PRCM_GPIO3=%p after\n", PRCM_GPIO3);
            real_printf("PRCM_GPIO4=%p after\n", PRCM_GPIO4);
            real_printf("PRCM_GPIO5=%p after\n", PRCM_GPIO5);
            real_printf("PRCM_GPIO6=%p after\n", PRCM_GPIO6);
            real_printf("PRCM_GPIO7=%p after\n", PRCM_GPIO7);
            real_printf("PRCM_GPIO8=%p after\n", PRCM_GPIO8);
            real_printf("PRCM_SPI2=%p after\n", PRCM_SPI2);
        #endif
	#endif
#endif
	
#ifdef CPU_TDA4VM
    u4_t pin, reg;
    for (pin = reg = 0; pin < GPIO_NPINS; pin++, reg += 4) {
        if (reg == 0x48) reg += 4;      // skip 0x48, see data sheet table 6-125 pg 139
        gpio_pmux_reg_off[GPIO0][pin] = reg;
    }
    
    SPIn_SCLK.init();
    SPIn_MISO.init();
    SPIn_MOSI.init();
    SPIn_CS0.init();
    SPIn_CS1.init();

    FPGA_PGM.init();
    FPGA_INIT.init();

    P911.init();
    P913.init();
    P915.init();
    CMD_READY.init();
    SND_INTR.init();
    P926.init();

    JTAG_TCK.init();
    JTAG_TMS.init();
    JTAG_TDI.init();
    JTAG_TDO.init();
    P811.init();
    P812.init();
    P813.init();
    P814.init();
    P815.init();
    P816.init();
    P817.init();
    P818.init();
    P819.init();
    P826.init();
#endif
	
	// Can't set pmux via mmap in a user-mode program.
	// So instead use device tree (dts) mechanism and check expected pmux values here.
	
	// P9 connector
#ifdef CPU_AM3359
    // like BBAI, SPI pmux must be setup for Debian >= 9
	if (!use_spidev || debian_ver >= 9) {
	    lprintf("checking SPI pmux settings..\n");
		devio_check(SPIn_SCLK, GPIO_DIR_OUT, PMUX_IO_PU  | PMUX_SPI, PMUX_SLOW | PMUX_IO_PU  | PMUX_SPI);
		devio_check(SPIn_MISO, GPIO_DIR_IN,  PMUX_IN_PU  | PMUX_SPI, PMUX_SLOW | PMUX_IN_PU  | PMUX_SPI);
		devio_check(SPIn_MOSI, GPIO_DIR_OUT, PMUX_OUT_PU | PMUX_SPI, PMUX_SLOW | PMUX_OUT_PU | PMUX_SPI);
		devio_check(SPIn_CS0,  GPIO_DIR_OUT, PMUX_OUT_PU | PMUX_SPI, PMUX_SLOW | PMUX_OUT_PU | PMUX_SPI);
	}
#endif
	
    if (debian_ver >= 9)
        lprintf("checking GPIO pmux settings..\n");

#if defined(CPU_AM5729) || defined(CPU_TDA4VM)
    // BBAI has always used Debian >= 9
    // BBAI-64 has always used Debian >= 11
    devio_check(SPIn_SCLK, GPIO_DIR_OUT, PMUX_IO_PU  | PMUX_SPI, PMUX_SLOW | PMUX_IO_PU  | PMUX_SPI);
    //devio_check(SPIn_MISO, GPIO_DIR_IN,  PMUX_IN_PD  | PMUX_SPI, PMUX_NONE);
    //devio_check(SPIn_MOSI, GPIO_DIR_OUT, PMUX_OUT_PD | PMUX_SPI, PMUX_SLOW | PMUX_OUT_PD | PMUX_SPI);
    devio_check(SPIn_MISO, GPIO_DIR_IN,  PMUX_IN_PU  | PMUX_SPI, PMUX_NONE);
    devio_check(SPIn_MOSI, GPIO_DIR_OUT, PMUX_OUT_PU | PMUX_SPI, PMUX_SLOW | PMUX_OUT_PD | PMUX_SPI);
    devio_check(SPIn_CS0,  GPIO_DIR_OUT, PMUX_OUT_PU | PMUX_SPI, PMUX_SLOW | PMUX_OUT_PU | PMUX_SPI);
#endif

	gpio_setup(SPIn_CS1, GPIO_DIR_OUT, 1, PMUX_OUT_PU, PMUX_IO_PD);

	gpio_setup(CMD_READY, GPIO_DIR_BIDIR, GPIO_HIZ, PMUX_IO, PMUX_IO_PU);
	gpio_setup(SND_INTR, GPIO_DIR_BIDIR, GPIO_HIZ, PMUX_IO, PMUX_IO_PU);

	gpio_setup(P911, GPIO_DIR_BIDIR, GPIO_HIZ, PMUX_IO, PMUX_IO_PD);
	gpio_setup(P913, GPIO_DIR_BIDIR, GPIO_HIZ, PMUX_IO, PMUX_IO_PD);
	gpio_setup(P915, GPIO_DIR_BIDIR, GPIO_HIZ, PMUX_IO, PMUX_IO_PU);
	gpio_setup(P926, GPIO_DIR_BIDIR, GPIO_HIZ, PMUX_IO, PMUX_IO_PU);
	
	// P8 connector
	gpio_setup(JTAG_TDI, GPIO_DIR_BIDIR, GPIO_HIZ, PMUX_IO_PU, PMUX_IO);
	gpio_setup(JTAG_TDO, GPIO_DIR_BIDIR, GPIO_HIZ, PMUX_IO_PU, PMUX_IO);	// fixme: define as JTAG output
	gpio_setup(JTAG_TCK, GPIO_DIR_BIDIR, GPIO_HIZ, PMUX_IO_PU, PMUX_IO);
	gpio_setup(JTAG_TMS, GPIO_DIR_BIDIR, GPIO_HIZ, PMUX_IO_PU, PMUX_IO);

    gpio_setup(P811, GPIO_DIR_BIDIR, GPIO_HIZ, PMUX_IO_PU, PMUX_IO);
    gpio_setup(P812, GPIO_DIR_BIDIR, GPIO_HIZ, PMUX_IO_PU, PMUX_IO);
    gpio_setup(P813, GPIO_DIR_BIDIR, GPIO_HIZ, PMUX_IO_PU, PMUX_IO);
    gpio_setup(P814, GPIO_DIR_BIDIR, GPIO_HIZ, PMUX_IO_PU, PMUX_IO);
    gpio_setup(P815, GPIO_DIR_BIDIR, GPIO_HIZ, PMUX_IO_PU, PMUX_IO);
    gpio_setup(P816, GPIO_DIR_BIDIR, GPIO_HIZ, PMUX_IO_PU, PMUX_IO);
    gpio_setup(P817, GPIO_DIR_BIDIR, GPIO_HIZ, PMUX_IO_PU, PMUX_IO);
    gpio_setup(P818, GPIO_DIR_BIDIR, GPIO_HIZ, PMUX_IO_PU, PMUX_IO);
    gpio_setup(P819, GPIO_DIR_BIDIR, GPIO_HIZ, PMUX_IO_PU, PMUX_IO);
    gpio_setup(P826, GPIO_DIR_BIDIR, GPIO_HIZ, PMUX_IO_PU, PMUX_IO);
	
#ifdef CPU_AM3359
    gpio_setup(BOOT_BTN, GPIO_DIR_BIDIR, GPIO_HIZ, PMUX_IO_PU, PMUX_IO);
#endif

	//if (any_bad) panic("devio_check() or gpio_setup() FAILED");

	init = TRUE;
}

static gpio_t *idx_2_gpio[] = {
    &P811, &P812, &P813, &P814, &P815, &P816, &P817, &P818, &P819,
    NULL,  NULL,  NULL,  NULL,  NULL,  NULL,  &P826
};

void gpio_test(int gpio_test_pin) {
    gpio_t *gpio;
    int idx = gpio_test_pin - 811;
    if (gpio_test_pin >= 811 && gpio_test_pin <= 826 && (gpio = idx_2_gpio[idx]) != NULL) {
        printf("testing GPIO pin P%d\n", gpio_test_pin);
        gpio_t _gpio = *gpio;
        GPIO_OUTPUT(_gpio);
        while (1) {
            printf("GPIO P%d=0\n", gpio_test_pin);
            GPIO_WRITE_BIT(_gpio, 0);
            TaskSleepSec(1);
            printf("GPIO P%d=1\n", gpio_test_pin);
            GPIO_WRITE_BIT(_gpio, 1);
            TaskSleepSec(1);
        }
    } else {
        printf("RANGE -gpio %d (811-826)\n", gpio_test_pin);
        panic("GPIO test");
    }
}

void peri_free() {
	assert(init);
    if (prcm_m != NULL) munmap((void *) prcm_m, MMAP_SIZE);
    if (pmux_m != NULL) munmap((void *) pmux_m, MMAP_SIZE);
    if (spi_m != NULL) munmap((void *) spi_m,  MMAP_SIZE);
    prcm_m = pmux_m = spi_m = NULL;

    int i;
    for (i = 0; i < NGPIO; i++) {
    	munmap((void *) gpio_m[i], MMAP_SIZE);
    	gpio_m[i] = NULL;
    }
}

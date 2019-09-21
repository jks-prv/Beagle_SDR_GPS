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

// Copyright (c) 2015-2019 John Seamons, ZL/KF6VO

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

arch_cpu_e arch_cpu = ARCH_CPU;

static volatile u4_t *prcm, *pmux;
volatile u4_t *spi, *_gpio[NGPIO];
static bool init;

#ifdef CPU_AM3359
 static u4_t gpio_base[NGPIO] = { GPIO0_BASE, GPIO1_BASE, GPIO2_BASE, GPIO3_BASE };

 static u4_t gpio_pmux_reg_off[NGPIO][32] = {
 //                0      1      2      3      4      5      6      7      8      9     10     11     12     13     14     15     16     17     18     19     20     21     22     23     24     25     26     27     28     29     30     31
 /* gpio0 */	{ 0x000, 0x000, 0x950, 0x954, 0x958, 0x95c, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x980, 0x984, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x820, 0x824, 0x000, 0x000, 0x828, 0x82c, 0x000, 0x000, 0x870, 0x874 },
 /* gpio1 */	{ 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x830, 0x834, 0x838, 0x83c, 0x840, 0x844, 0x848, 0x84c, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x878, 0x87c, 0x000, 0x000 },
 /* gpio2 */	{ 0x000, 0x88c, 0x890, 0x894, 0x898, 0x89c, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000 },
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

pin_t eeprom_pins[EE_NPINS];

//					  { bank, bit, pin, eeprom_offset }
gpio_t GPIO_NONE	= { 0xff, 0xff, 0xff, 0xff };


// P9 connector

#ifdef CPU_AM3359
    gpio_t SPIn_SCLK	= { GPIO0,  2, PIN(P9, 22),  88 };
    gpio_t SPIn_MISO	= { GPIO0,  3, PIN(P9, 21),  90 };
    gpio_t SPIn_MOSI	= { GPIO0,  4, PIN(P9, 18),  92 };
    gpio_t SPIn_CS0		= { GPIO0,  5, PIN(P9, 17),  94 };
    gpio_t SPIn_CS1		= { GPIO1, 19, PIN(P9, 16), 158 };	// not the actual spi_cs1 from hardware, but our PIO emulation

    gpio_t FPGA_PGM		= { GPIO1, 28, PIN(P9, 12), 160 };
    gpio_t FPGA_INIT	= { GPIO1, 18, PIN(P9, 14), 156 };

    gpio_t I2C2_SCL		= { GPIO0, 13, PIN(P9, 19), 106 };
    gpio_t I2C2_SDA		= { GPIO0, 12, PIN(P9, 20), 108 };

    gpio_t P911 		= { GPIO0, 30, PIN(P9, 11), 124 };
    gpio_t P913 		= { GPIO0, 31, PIN(P9, 13), 118 };
    gpio_t P915 		= { GPIO1, 16, PIN(P9, 15), 152 };
    gpio_t P923 		= { GPIO1, 17, PIN(P9, 23), 154 };
    gpio_t SND_INTR		= { GPIO0, 15, PIN(P9, 24), 112 };
    gpio_t P926 		= { GPIO0, 14, PIN(P9, 26), 110 };
#endif

#ifdef CPU_AM5729
    // FIXME: EEPROM per-pin offsets for larger number of CPU_AM5729 GPIOs?
    gpio_t SPIn_SCLK	= { GPIO6, 19, PIN(P9, 22),  88 };
    gpio_t SPIn_MISO	= { GPIO3,  3, PIN(P9, 21),  90 };
    gpio_t SPIn_MOSI	= { GPIO7, 16, PIN(P9, 18),  92 };
    gpio_t SPIn_CS0		= { GPIO7, 17, PIN(P9, 17),  94 };
    gpio_t SPIn_CS1		= { GPIO4, 26, PIN(P9, 16), 158 };	// not the actual spi_cs1 from hardware, but our PIO emulation

    gpio_t FPGA_PGM		= { GPIO5,  0, PIN(P9, 12), 160 };
    gpio_t FPGA_INIT	= { GPIO4, 25, PIN(P9, 14), 156 };

    gpio_t I2C2_SCL		= { GPIO7,  3, PIN(P9, 19), 106 };
    gpio_t I2C2_SDA		= { GPIO7,  4, PIN(P9, 20), 108 };

    gpio_t P911 		= { GPIO8, 17, PIN(P9, 11), 124 };
    gpio_t P913 		= { GPIO6, 12, PIN(P9, 13), 118 };
    gpio_t P915 		= { GPIO3, 12, PIN(P9, 15), 152 };
    gpio_t P923 		= { GPIO7, 11, PIN(P9, 23), 154 };
    gpio_t SND_INTR		= { GPIO6, 15, PIN(P9, 24), 112 };
    gpio_t P926 		= { GPIO6, 14, PIN(P9, 26), 110 };
#endif


// P8 connector

#ifdef CPU_AM3359
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
#endif

#ifdef CPU_AM5729
    // FIXME: EEPROM per-pin offsets for larger number of CPU_AM5729 GPIOs?
    gpio_t JTAG_TCK		= { GPIO6, 18, PIN(P8,  9), 172 };
    gpio_t JTAG_TMS		= { GPIO6,  4, PIN(P8, 10), 174 };
    gpio_t JTAG_TDI		= { GPIO6,  5, PIN(P8,  7), 170 };
    gpio_t JTAG_TDO		= { GPIO6,  6, PIN(P8,  8), 176 };
    gpio_t P811			= { GPIO3, 11, PIN(P8, 11), 146 };
    gpio_t P812			= { GPIO3, 10, PIN(P8, 12), 144 };
    gpio_t P813			= { GPIO4, 11, PIN(P8, 13), 118 };
    gpio_t P814			= { GPIO4, 13, PIN(P8, 14), 120 };
    gpio_t P815			= { GPIO4,  3, PIN(P8, 15), 150 };
    gpio_t P816			= { GPIO4, 28, PIN(P8, 16), 148 };
    gpio_t P817			= { GPIO8, 18, PIN(P8, 17), 122 };
    gpio_t P818			= { GPIO4,  9, PIN(P8, 18), 168 };
    gpio_t P819			= { GPIO4, 10, PIN(P8, 19), 116 };
    gpio_t P826			= { GPIO4, 28, PIN(P8, 26), 162 };
#endif


static void check_pmux(const char *name, gpio_t gpio, gpio_dir_e dir, u4_t pmux_val1, u4_t pmux_val2)
{
    u4_t _pmux, pmux_reg_off, mode, pmux_pin_attr;
    bool val1_ok, val2_ok;
    
#ifdef CPU_AM3359
    pmux_reg_off = gpio_pmux_reg_off[gpio.bank][gpio.bit];
    check(pmux_reg_off != 0);
    _pmux = pmux[pmux_reg_off>>2];
    val1_ok = val2_ok = true;
    if (pmux_val1 && _pmux != pmux_val1) val1_ok = false;
    if (pmux_val2 && _pmux != pmux_val2) val2_ok = false;
    
    if (pmux_val2 && val2_ok) {
        printf("PMUX %d_%-2d got 0x%02x, want 0x%02x or 0x%02x\n", gpio.bank, gpio.bit, _pmux, pmux_val1, pmux_val2);
        printf("PMUX NOTE: pmux_val2 0x%02x matched\n", pmux_val2);
    }
    
    if (!val1_ok && !val2_ok) {
        printf("PMUX %d_%-2d got 0x%02x, want 0x%02x or 0x%02x ---------------------\n", gpio.bank, gpio.bit, _pmux, pmux_val1, pmux_val2);
        //jksd FIXME "apt-get upgrade" loads new cape overlays which change the PMUX values of the unused GPIOs!
        //panic("check_pmux");
    }
    
    #if 1
        printf("\tPMUX check %-9s GPIO %d_%-2d %s.%-2d eeprom %3d/0x%02x has attr 0x%02x <%s, %s%s%s, m%d>\n",
            name, gpio.bank, gpio.bit, (gpio.pin & P9)? "P9":"P8", gpio.pin & PIN_BITS, gpio.eeprom_off, gpio.eeprom_off,
            _pmux, (_pmux & PMUX_SLOW)? "SLOW":"FAST", (_pmux & PMUX_RXEN)? "RX, ":"", GPIO_isOE(gpio)? "OE, ":"",
            (_pmux & PMUX_PDIS)? "PDIS" : ((_pmux & PMUX_PU)? "PU":"PD"), _pmux & PMUX_MODE);
    #endif
    
    pmux_pin_attr = _pmux & PMUX_BITS;
#endif

#ifdef CPU_AM5729
    u4_t bank, _pmux2, pmux_reg2_off;
    bank = gpio.bank * NBALL;
    pmux_reg_off = gpio_pmux_reg_off[bank][gpio.bit];
    check(pmux_reg_off != 0);
    _pmux = pmux[pmux_reg_off>>2];
    mode = _pmux & PMUX_MODE;
    val1_ok = val2_ok = true;
    if (pmux_val1 && _pmux != pmux_val1) val1_ok = false;
    if (pmux_val2 && _pmux != pmux_val2) val2_ok = false;
    
    if (pmux_val2 && val2_ok) {
        printf("PMUX %d_%-2d got 0x%08x, want 0x%08x or 0x%08x\n", gpio.bank+1, gpio.bit, _pmux, pmux_val1, pmux_val2);
        printf("PMUX NOTE: pmux_val2 0x%08x matched\n", pmux_val2);
    }
    
    if (!val1_ok && !val2_ok) {
        printf("PMUX %d_%-2d got 0x%08x, want 0x%08x or 0x%08x ---------------------\n", gpio.bank+1, gpio.bit, _pmux, pmux_val1, pmux_val2);
        //jksd FIXME "apt-get upgrade" loads new cape overlays which change the PMUX values of the unused GPIOs!
        //panic("check_pmux");
    }
    
    // check for second ball (if applicable) being disabled so as not to conflict
    pmux_reg2_off = gpio_pmux_reg_off[bank+1][gpio.bit];
    if (pmux_reg2_off != 0) {
        _pmux2 = pmux[pmux_reg2_off>>2];
        if (mode != PMUX_OFF) {
            printf("PMUX %d_%-2d ball_1 %04x=%08x ball_2 %04x=%08x, WAS EXPECTING ball_2 mode PMUX_OFF=15 got=%d\n",
                gpio.bank+1, gpio.bit, pmux_reg_off, _pmux, pmux_reg2_off, _pmux2, mode);
        }
    }

    #if 1
        printf("\tPMUX check %-9s GPIO %d_%-2d %s.%-2d eeprom %3d/0x%02x has attr 0x%08x <%s, %s%s%s, m%d>\n",
            name, gpio.bank+1, gpio.bit, (gpio.pin & P9)? "P9":"P8", gpio.pin & PIN_BITS, gpio.eeprom_off, gpio.eeprom_off,
            _pmux, (_pmux & PMUX_SLOW)? "SLOW":"FAST", (_pmux & PMUX_RXEN)? "RX, ":"", GPIO_isOE(gpio)? "OE, ":"",
            (_pmux & PMUX_PDIS)? "PDIS" : ((_pmux & PMUX_PU)? "PU":"PD"), mode);
    #endif
    
    // FIXME: how does EEPROM change for increased number of pins?
    pmux_pin_attr = ((_pmux & PMUX_ATTR) >> 16) /* | mode ... */ ;
#endif

    // generate the per-pin info used by eeprom_write()
    u4_t pin = (gpio.eeprom_off - EE_PINS_OFFSET_BASE)/2;
    check(pin < EE_NPINS);
    pin_t *p = &eeprom_pins[pin];
    p->gpio = gpio;
    p->attrs = PIN_USED | (pmux_pin_attr & PIN_PMUX_BITS);
    p->attrs |= (dir == GPIO_DIR_IN)? PIN_DIR_IN : ( (dir == GPIO_DIR_OUT)? PIN_DIR_OUT : PIN_DIR_BIDIR );
}

const char *dir_name[] = { "INPUT", "OUTPUT", "BIDIR" };

void _devio_setup(const char *name, gpio_t gpio, gpio_dir_e dir, u4_t pmux_val)
{
	//printf("DEVIO setup %s %d_%-2d %s\n", name, gpio.bank, gpio.bit, dir_name[dir]);
	check_pmux(name, gpio, dir, pmux_val, 0);
}

void _gpio_setup(const char *name, gpio_t gpio, gpio_dir_e dir, u4_t initial, u4_t pmux_val1, u4_t pmux_val2)
{
	if (!isGPIO(gpio)) return;

	GPIO_CLR_IRQ0(gpio) = 1 << gpio.bit;
	GPIO_CLR_IRQ1(gpio) = 1 << gpio.bit;
	
	if (dir == GPIO_DIR_IN) {
		//printf("GPIO setup %s %d_%-2d INPUT\n", name, gpio.bank, gpio.bit);
		GPIO_INPUT(gpio);
	} else {
		if (initial != GPIO_HIZ) {
			//printf("GPIO setup %s %d_%-2d %s initial=%d\n", name, gpio.bank, gpio.bit,
			//	(dir == GPIO_DIR_OUT)? "OUTPUT":"BIDIR", initial);
			GPIO_WRITE_BIT(gpio, initial);
			GPIO_OUTPUT(gpio);
			GPIO_WRITE_BIT(gpio, initial);
		} else {
			//printf("GPIO setup %s %d_%-2d %s initial=Z\n", name, gpio.bank, gpio.bit,
			//	(dir == GPIO_DIR_OUT)? "OUTPUT":"BIDIR");
			GPIO_INPUT(gpio);
		}
	}
	check_pmux(name, gpio, dir, pmux_val1 | PMUX_GPIO, pmux_val2 | PMUX_GPIO);
	spin_ms(10);
}

void peri_init()
{
    int i, mem_fd;

    mem_fd = open("/dev/mem", O_RDWR|O_SYNC);
    check(mem_fd >= 0);
    
    prcm = (volatile u4_t *) mmap(
        NULL,
        MMAP_SIZE,
        PROT_READ|PROT_WRITE,
        MAP_SHARED,
        mem_fd,
        PRCM_BASE
    );
    check(prcm);

	for (i = 0; i < NGPIO; i++) {
		_gpio[i] = (volatile u4_t *) mmap(
			NULL,
			MMAP_SIZE,
			PROT_READ|PROT_WRITE,
			MAP_SHARED,
			mem_fd,
			gpio_base[i]
		);
		check(_gpio[i]);
	}

    pmux = (volatile u4_t *) mmap(
        NULL,
        MMAP_SIZE,
        PROT_READ|PROT_WRITE,
        MAP_SHARED,
        mem_fd,
        PMUX_BASE
    );
    check(pmux);

	if (!use_spidev) {
		spi = (volatile u4_t *) mmap(
			NULL,
			MMAP_SIZE,
			PROT_READ|PROT_WRITE,
			MAP_SHARED,
			mem_fd,
			SPI_BASE
		);
		check(spi);
	}

    close(mem_fd);

	// power-up the device logic
#ifdef CPU_AM3359
	PRCM_GPIO0 = PRCM_GPIO1 = PRCM_GPIO2 = PRCM_GPIO3 = PRCM_SPI0 = MODMODE_ENA;
#endif
	
#ifdef CPU_AM5729
	PRCM_GPIO2 = PRCM_GPIO3 = PRCM_GPIO4 = PRCM_GPIO5 = PRCM_GPIO6 = PRCM_GPIO7 = PRCM_GPIO8 = PRCM_SPI2 = MODMODE_ENA;
#endif
	
	// Can't set pmux via mmap in a user-mode program.
	// So instead use device tree (dts) mechanism and check expected pmux values here.
	
	// P9 connector
	if (!use_spidev) {
		devio_setup(SPIn_SCLK, GPIO_DIR_OUT, PMUX_IO_PU | PMUX_M0);
		devio_setup(SPIn_MISO, GPIO_DIR_IN, PMUX_IN_PU | PMUX_M0);
		devio_setup(SPIn_MOSI, GPIO_DIR_OUT, PMUX_OUT_PU | PMUX_M0);
		devio_setup(SPIn_CS0, GPIO_DIR_OUT, PMUX_OUT_PU | PMUX_M0);
	}
	gpio_setup(SPIn_CS1, GPIO_DIR_OUT, 1, PMUX_IO_PD, 0);
	
	gpio_setup(P911, GPIO_DIR_BIDIR, GPIO_HIZ, PMUX_IO, PMUX_IO_PU);
	gpio_setup(P913, GPIO_DIR_BIDIR, GPIO_HIZ, PMUX_IO, PMUX_IO_PU);
	gpio_setup(P915, GPIO_DIR_BIDIR, GPIO_HIZ, PMUX_IO, PMUX_IO_PU);
	gpio_setup(P923, GPIO_DIR_BIDIR, GPIO_HIZ, PMUX_IO, PMUX_IO_PU);
	gpio_setup(SND_INTR, GPIO_DIR_BIDIR, GPIO_HIZ, PMUX_IO, PMUX_IO_PU);
	gpio_setup(P926, GPIO_DIR_BIDIR, GPIO_HIZ, PMUX_IO, PMUX_IO_PU);
	
	// P8 connector
	gpio_setup(JTAG_TCK, GPIO_DIR_BIDIR, GPIO_HIZ, PMUX_IO, PMUX_IO_PU);
	gpio_setup(JTAG_TMS, GPIO_DIR_BIDIR, GPIO_HIZ, PMUX_IO, PMUX_IO_PU);
	gpio_setup(JTAG_TDI, GPIO_DIR_BIDIR, GPIO_HIZ, PMUX_IO, PMUX_IO_PU);
	gpio_setup(JTAG_TDO, GPIO_DIR_BIDIR, GPIO_HIZ, PMUX_IO, PMUX_IO_PU);	// fixme: define as JTAG output

	gpio_setup(P811, GPIO_DIR_BIDIR, GPIO_HIZ, PMUX_IO, PMUX_IO_PU);
	gpio_setup(P812, GPIO_DIR_BIDIR, GPIO_HIZ, PMUX_IO, PMUX_IO_PU);
	gpio_setup(P813, GPIO_DIR_BIDIR, GPIO_HIZ, PMUX_IO, PMUX_IO_PU);
	gpio_setup(P814, GPIO_DIR_BIDIR, GPIO_HIZ, PMUX_IO, PMUX_IO_PU);
	gpio_setup(P815, GPIO_DIR_BIDIR, GPIO_HIZ, PMUX_IO, PMUX_IO_PU);
    gpio_setup(P816, GPIO_DIR_BIDIR, GPIO_HIZ, PMUX_IO, PMUX_IO_PU);
	gpio_setup(P817, GPIO_DIR_BIDIR, GPIO_HIZ, PMUX_IO, PMUX_IO_PU);
	gpio_setup(P818, GPIO_DIR_BIDIR, GPIO_HIZ, PMUX_IO, PMUX_IO_PU);
	gpio_setup(P819, GPIO_DIR_BIDIR, GPIO_HIZ, PMUX_IO, PMUX_IO_PU);
	gpio_setup(P826, GPIO_DIR_BIDIR, GPIO_HIZ, PMUX_IO, PMUX_IO_PU);
	
	init = TRUE;
}

void peri_free() {
	assert(init);
    munmap((void *) prcm, MMAP_SIZE);
    munmap((void *) pmux, MMAP_SIZE);
    munmap((void *) spi,  MMAP_SIZE);
    prcm = pmux = spi = NULL;

    int i;
    for (i = 0; i < NGPIO; i++) {
    	munmap((void *) _gpio[i], MMAP_SIZE);
    	_gpio[i] = NULL;
    }
}

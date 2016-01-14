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

#include "types.h"
#include "config.h"
#include "kiwi.h"
#include "misc.h"
#include "peri.h"
#include "spi.h"
#include "coroutines.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

static volatile u4_t *prcm, *pmux;
volatile u4_t *spi, *_gpio[NGPIO];
static bool init;

static u4_t gpio_base[NGPIO] = {
	GPIO0_BASE, GPIO1_BASE, GPIO2_BASE, GPIO3_BASE
};

u4_t gpio_pmux[NGPIO][32] = {
//                0      1      2      3      4      5      6      7      8      9     10     11     12     13     14     15     16     17     18     19     20     21     22     23     24     25     26     27     28     29     30     31
/* gpio0 */	{ 0x000, 0x000, 0x950, 0x954, 0x958, 0x95c, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x980, 0x984, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x820, 0x824, 0x000, 0x000, 0x828, 0x82c, 0x000, 0x000, 0x870, 0x874 },
/* gpio1 */	{ 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x830, 0x834, 0x838, 0x83c, 0x840, 0x844, 0x848, 0x84c, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x878, 0x87c, 0x000, 0x000 },
/* gpio2 */	{ 0x000, 0x88c, 0x890, 0x894, 0x898, 0x89c, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000 },
/* gpio3 */	{ 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000 },
};

gpio_t GPIO_NONE	= { 0xff, 0xff };

// P9 connector
gpio_t SPI0_SCLK	= { GPIO0, 2 };
gpio_t SPI0_MISO	= { GPIO0, 3 };
gpio_t SPI0_MOSI	= { GPIO0, 4 };
gpio_t SPI0_CS0		= { GPIO0, 5 };
gpio_t SPI0_CS1		= { GPIO1, 19 };	// not the actual spi0_cs1 from hardware, but our PIO emulation

gpio_t FPGA_PGM		= { GPIO1, 28 };
gpio_t FPGA_INIT	= { GPIO1, 18 };

gpio_t I2C2_SCL		= { GPIO0, 13 };
gpio_t I2C2_SDA		= { GPIO0, 12 };

gpio_t GPIO0_30		= { GPIO0, 30 };
gpio_t GPIO0_31		= { GPIO0, 31 };
gpio_t GPIO1_16		= { GPIO1, 16 };
gpio_t GPIO1_17		= { GPIO1, 17 };
gpio_t GPIO0_15		= { GPIO0, 15 };
gpio_t GPIO0_14		= { GPIO0, 14 };

// P8 connector
gpio_t JTAG_TCK		= { GPIO2, 5 };
gpio_t JTAG_TMS		= { GPIO2, 4 };
gpio_t JTAG_TDI		= { GPIO2, 2 };
gpio_t JTAG_TDO		= { GPIO2, 3 };
gpio_t P811			= { GPIO1, 13 };
gpio_t P812			= { GPIO1, 12 };
gpio_t P813			= { GPIO0, 23 };
gpio_t P814			= { GPIO0, 26 };
gpio_t P815			= { GPIO1, 15 };
gpio_t P816			= { GPIO1, 14 };
gpio_t P817			= { GPIO0, 27 };
gpio_t P818			= { GPIO2, 1 };
gpio_t P819			= { GPIO0, 22 };
gpio_t P826			= { GPIO1, 29 };

void check_pmux(gpio_t gpio, u4_t pmux_val)
{
	u4_t pmux_reg = gpio_pmux[gpio.bank][gpio.bit];
	check(pmux_reg != 0);
	u4_t _pmux = pmux[pmux_reg>>2];
	if (_pmux != pmux_val) {
		printf("PMUX %d:%d got 0x%02x != want 0x%02x\n", gpio.bank, gpio.bit, _pmux, pmux_val);
		panic("check_pmux");
	}
	printf("\tPMUX check %d:%d is 0x%02x <%s, %s%s%s, m%d>\n", gpio.bank, gpio.bit, _pmux,
		(_pmux & PMUX_SLOW)? "SLOW":"FAST", (_pmux & PMUX_RXEN)? "RX, ":"", GPIO_isOE(gpio)? "OE, ":"",
		(_pmux & PMUX_PDIS)? "PDIS" : ((_pmux & PMUX_PU)? "PU":"PD"), _pmux & 7);
}

const char *dir_name[] = { "INPUT", "OUTPUT", "BIDIR" };

void _devio_setup(const char *name, gpio_t gpio, gpio_dir_e dir, u4_t pmux_val)
{
	printf("DEVIO setup %s %d:%d %s\n", name, gpio.bank, gpio.bit, dir_name[dir]);
	check_pmux(gpio, pmux_val);
}

void _gpio_setup(const char *name, gpio_t gpio, gpio_dir_e dir, u4_t initial, u4_t pmux_val)
{
	if (!isGPIO(gpio)) return;

	GPIO_CLR_IRQ0(gpio) = 1 << gpio.bit;
	GPIO_CLR_IRQ1(gpio) = 1 << gpio.bit;
	
	if (dir == GPIO_DIR_IN) {
		printf("GPIO setup %s %d:%d INPUT\n", name, gpio.bank, gpio.bit);
		GPIO_INPUT(gpio);
	} else {
		if (initial != GPIO_HIZ) {
			printf("GPIO setup %s %d:%d %s initial=%d\n", name, gpio.bank, gpio.bit,
				(dir == GPIO_DIR_OUT)? "OUTPUT":"BIDIR", initial);
			GPIO_WRITE_BIT(gpio, initial);
			GPIO_OUTPUT(gpio);
			GPIO_WRITE_BIT(gpio, initial);
		} else {
			printf("GPIO setup %s %d:%d %s initial=Z\n", name, gpio.bank, gpio.bit,
				(dir == GPIO_DIR_OUT)? "OUTPUT":"BIDIR");
			GPIO_INPUT(gpio);
		}
	}
	check_pmux(gpio, pmux_val | PMUX_M7);
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

#ifdef USE_SPIDEV
#else
    spi = (volatile u4_t *) mmap(
        NULL,
        MMAP_SIZE,
        PROT_READ|PROT_WRITE,
        MAP_SHARED,
        mem_fd,
        SPI0_BASE
    );
    check(spi);
#endif

    close(mem_fd);

	// power-up the device logic
	PRCM_GPIO0 = MODMODE_ENA;
	PRCM_GPIO1 = MODMODE_ENA;
	PRCM_GPIO2 = MODMODE_ENA;
	PRCM_GPIO3 = MODMODE_ENA;
	PRCM_SPI0 = MODMODE_ENA;
	
	// Can't set pmux via mmap in a user-mode program.
	// So instead use device tree (dts) mechanism and check expected pmux value here.
	
	// P9 connector
	devio_setup(SPI0_SCLK, GPIO_DIR_OUT, PMUX_IO_PU | PMUX_M0);
	devio_setup(SPI0_MISO, GPIO_DIR_IN, PMUX_IN_PU | PMUX_M0);
	devio_setup(SPI0_MOSI, GPIO_DIR_OUT, PMUX_OUT_PU | PMUX_M0);
	devio_setup(SPI0_CS0, GPIO_DIR_OUT, PMUX_OUT_PU | PMUX_M0);
	gpio_setup(SPI0_CS1, GPIO_DIR_OUT, 1, PMUX_IO_PD);
	
	gpio_setup(GPIO0_30, GPIO_DIR_BIDIR, GPIO_HIZ, PMUX_IO);
	gpio_setup(GPIO0_31, GPIO_DIR_BIDIR, GPIO_HIZ, PMUX_IO);
	gpio_setup(GPIO1_16, GPIO_DIR_BIDIR, GPIO_HIZ, PMUX_IO);
	gpio_setup(GPIO1_17, GPIO_DIR_BIDIR, GPIO_HIZ, PMUX_IO);
	gpio_setup(GPIO0_15, GPIO_DIR_BIDIR, GPIO_HIZ, PMUX_IO);
	gpio_setup(GPIO0_14, GPIO_DIR_BIDIR, GPIO_HIZ, PMUX_IO);
	
	// P8 connector
	gpio_setup(JTAG_TCK, GPIO_DIR_BIDIR, GPIO_HIZ, PMUX_IO);
	gpio_setup(JTAG_TMS, GPIO_DIR_BIDIR, GPIO_HIZ, PMUX_IO);
	gpio_setup(JTAG_TDI, GPIO_DIR_BIDIR, GPIO_HIZ, PMUX_IO);
	gpio_setup(JTAG_TDO, GPIO_DIR_BIDIR, GPIO_HIZ, PMUX_IO);	// fixme: define as JTAG output

	gpio_setup(P811, GPIO_DIR_BIDIR, GPIO_HIZ, PMUX_IO);
	gpio_setup(P812, GPIO_DIR_BIDIR, GPIO_HIZ, PMUX_IO);
	gpio_setup(P813, GPIO_DIR_BIDIR, GPIO_HIZ, PMUX_IO);
	gpio_setup(P814, GPIO_DIR_BIDIR, GPIO_HIZ, PMUX_IO);
	gpio_setup(P815, GPIO_DIR_BIDIR, GPIO_HIZ, PMUX_IO);
	gpio_setup(P816, GPIO_DIR_BIDIR, GPIO_HIZ, PMUX_IO);
	gpio_setup(P817, GPIO_DIR_BIDIR, GPIO_HIZ, PMUX_IO);
	gpio_setup(P818, GPIO_DIR_BIDIR, GPIO_HIZ, PMUX_IO);
	gpio_setup(P819, GPIO_DIR_BIDIR, GPIO_HIZ, PMUX_IO);
	gpio_setup(P826, GPIO_DIR_BIDIR, GPIO_HIZ, PMUX_IO);
	
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

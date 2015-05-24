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

#define USE_SPIDEV

#include "types.h"
#include "config.h"
#include "wrx.h"
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

#ifdef USE_SPIDEV
	#include <sys/ioctl.h>
	#include <sys/stat.h>
	#ifdef __linux__
		#include <linux/types.h>
		#include <linux/spi/spidev.h>
	#else
		#include "devl_spidev.h"
	#endif
#endif

#define	SPI_DEVNAME	"/dev/spidev1.0"
#define	NOT(bit)	0	// documentation aid

static volatile u4_t *prcm, *pmux, *gpio0, *spi;
static bool init;
static int spi_fd;

int peri_init()
{
    int mem_fd;

    mem_fd = open("/dev/mem", O_RDWR|O_SYNC);
    if (mem_fd<0) return -1;

    prcm = (volatile u4_t *) mmap(
        NULL,
        MMAP_SIZE,
        PROT_READ|PROT_WRITE,
        MAP_SHARED,
        mem_fd,
        PRCM_BASE
    );
    if (!prcm) return -2;

    gpio0 = (volatile u4_t *) mmap(
        NULL,
        MMAP_SIZE,
        PROT_READ|PROT_WRITE,
        MAP_SHARED,
        mem_fd,
        GPIO0_BASE
    );
    if (!gpio0) return -3;

    pmux = (volatile u4_t *) mmap(
        NULL,
        MMAP_SIZE,
        PROT_READ|PROT_WRITE,
        MAP_SHARED,
        mem_fd,
        PMUX_BASE
    );
    if (!pmux) return -4;

#ifdef USE_SPIDEV
	if ((spi_fd = open(SPI_DEVNAME, O_RDWR)) < 0) sys_panic("open spidev");
#else
    spi = (volatile u4_t *) mmap(
        NULL,
        MMAP_SIZE,
        PROT_READ|PROT_WRITE,
        MAP_SHARED,
        mem_fd,
        SPI0_BASE
    );
    if (!spi)  return -5;
#endif

    close(mem_fd);


	PRCM_GPIO0 = MODMODE_ENA;
	PRCM_SPI0 = MODMODE_ENA;
	//printf("CLKS: pmux 0x%x g0 0x%x spi0 0x%x g1 0x%x g2 0x%x g3 0x%x\n",
	//	PRCM_PMUX, PRCM_GPIO0, PRCM_SPI0, PRCM_GPIO1, PRCM_GPIO2, PRCM_GPIO3);
	
	// can't set pmux via mmap -- use device tree (dts) mechanism
	//printf("PMUX: sclk 0x%x miso 0x%x mosi 0x%x cs0 0x%x gpio0_15 0x%x\n",
	//	PMUX_SPI0_SCLK, PMUX_SPI0_MISO, PMUX_SPI0_MOSI, PMUX_SPI0_CS0, PMUX_GPIO0_15);
	assert(PMUX_SPI0_SCLK == PMUX_INOUT);
	assert(PMUX_SPI0_MISO == PMUX_IN);
	assert(PMUX_SPI0_MOSI == PMUX_OUT);
	assert(PMUX_SPI0_CS0  == PMUX_OUT);
	assert(PMUX_GPIO0_15  == (PMUX_INOUT | PMUX_M7));	// GPIO default, not setup by DTS
	
	GPIO_CLR_IRQ0(0) = SPI0_CS1;
	GPIO_CLR_IRQ1(0) = SPI0_CS1;
	GPIO_SET(0) = SPI0_CS1;
	GPIO_OUTPUT(0, SPI0_CS1);
	GPIO_SET(0) = SPI0_CS1;
	spin_ms(10);

#ifdef USE_SPIDEV
	u4_t max_speed;
	if (spi_speed == SPI_48M) max_speed = 48000000; else
	if (spi_speed == SPI_24M) max_speed = 24000000; else
	if (spi_speed == SPI_12M) max_speed = 12000000; else
	if (spi_speed == SPI_6M) max_speed = 6000000; else
		panic("unknown spi_speed");
	if (ioctl(spi_fd, SPI_IOC_WR_MAX_SPEED_HZ, &max_speed) < 0) sys_panic("SPI_IOC_RD_MAX_SPEED_HZ");
	max_speed = 0;
	if (ioctl(spi_fd, SPI_IOC_RD_MAX_SPEED_HZ, &max_speed) < 0) sys_panic("SPI_IOC_RD_MAX_SPEED_HZ");
	char bpw = SPI_BPW;
	if (ioctl(spi_fd, SPI_IOC_WR_BITS_PER_WORD, &bpw) < 0) sys_panic("SPI_IOC_WR_BITS_PER_WORD");
	bpw = -1;
	if (ioctl(spi_fd, SPI_IOC_RD_BITS_PER_WORD, &bpw) < 0) sys_panic("SPI_IOC_RD_BITS_PER_WORD");
	printf("SPIDEV: max_speed %d bpw %d\n", max_speed, bpw);
	u4_t mode = SPI_MODE_0 | NOT(SPI_CS_HIGH) | NOT(SPI_NO_CS) | NOT(SPI_LSB_FIRST);
	if (ioctl(spi_fd, SPI_IOC_WR_MODE, &mode) < 0) sys_panic("SPI_IOC_WR_MODE");
#else
	SPI_CONFIG = IDLEMODE | SOFT_RST | AUTOIDLE;
	spin_ms(10);
	SPI_MODULE = MASTER | GEN_CS | SINGLE_CHAN;
	
	// disable FIFO use by SPI1 before SPI0 can use (see manual)
	SPI1_CTRL = 0;
	SPI1_CONF = 0;
	
	SPI0_CTRL = 0;
	SPI0_CONF = 0;
	SPI_XFERLVL = RX_AFL;
	
	u4_t div = (spi_clkg)? (spi_speed + 1) : (1 << spi_speed);
	printf("SPI: clkg %d speed %d div %d %.3f MHz\n", spi_clkg, spi_speed, div, 48e6 / div / 1e6);
	#if 0
	printf("SPI: config 0x%x status 0x%x irqena 0x%x module 0x%x\n", SPI_CONFIG, SPI_STATUS, SPI_IRQENA, SPI_MODULE);
	printf("SPI1: conf 0x%x stat 0x%x ctrl 0x%x\n", SPI1_CONF, SPI1_STAT, SPI1_CTRL);
	printf("SPI0: conf 0x%x stat 0x%x ctrl 0x%x\n", SPI0_CONF, SPI0_STAT, SPI0_CTRL);
	#endif

    // Reset FPGA
//    GP_SET0 = 1<<FPGA_PROG;
//    while ((GP_LEV0 & (1<<FPGA_INIT_BSY)) != 0);
//    GP_CLR0 = 1<<FPGA_PROG;
//    while ((GP_LEV0 & (1<<FPGA_INIT_BSY)) == 0);

SPI0_CONF = SPI_CONF;
spin_ms(1);
//printf("SPI_CONF: conf 0x%08x stat 0x%x ctrl 0x%x\n", SPI0_CONF, SPI0_STAT, SPI0_CTRL);
#endif

	spi_init();
	init = TRUE;
    return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////
void peri_spi(SPI_SEL sel, SPI_T *mosi, int tx_xfers, SPI_T *miso, int rx_xfers) {
    int rx=0, tx=0;
    u4_t stat;
    
    assert(init);

//printf("PERI %d:%d\n", tx_xfers, rx_xfers);
	if (sel == SPI_BOOT) {
		GPIO_CLR(0) = SPI0_CS1;
		spin_ms(10);
	}
	
	evSpi(EV_SPILOOP, "spiLoop enter", evprintf("%s tx=%d rx=%d", (sel == SPI_BOOT)? "BOOT" : cmds[mosi[0]], tx_xfers, rx_xfers));
//int meas = (mosi[0] == CmdGetWFSamples) || (mosi[0] == CmdGetWFDummy);
//int meas = sel == SPI_BOOT;
int meas = 0;
u4_t t_start, t_xfer1, t_xfer2, t_wait;
if (meas) t_start = timer_us();
//printf("L%d:%d ", tx_xfers, rx_xfers); fflush(stdout);
u4_t rcnt=0, rempty=0, rfull=0;

#ifdef USE_SPIDEV
	int spi_bytes = SPI_X2B(MAX(tx_xfers, rx_xfers));
	struct spi_ioc_transfer spi_tr;
	spi_tr.tx_buf = (unsigned long) mosi;
	spi_tr.rx_buf = (unsigned long) miso;
	spi_tr.len = spi_bytes;
	spi_tr.delay_usecs = 0;
	spi_tr.speed_hz = 48000000;
	spi_tr.bits_per_word = SPI_BPW;		// zero also means 8-bits?
	spi_tr.cs_change = 1;

	int actual_rxbytes;
	if ((actual_rxbytes = ioctl(spi_fd, SPI_IOC_MESSAGE(1), &spi_tr)) < 0) sys_panic("SPI_IOC_MESSAGE");
	assert(actual_rxbytes == spi_bytes);
	if (actual_rxbytes != spi_bytes) printf("actual_rxbytes %d spi_bytes %d\n", actual_rxbytes, spi_bytes);
#else
	SPI0_CONF = FORCE | SPI_CONF;		// add FORCE
	SPI0_CTRL = EN;

    while (tx < tx_xfers) {
    	stat = SPI0_STAT;
        if (!(stat & TX_FULL)) SPI0_TX = mosi[tx++];		// move data only if FIFOs are able to accept
        if (!(stat & RX_EMPTY)) miso[rx++] = SPI0_RX;
        //if (!(stat & RX_EMPTY)) { miso[rx++] = SPI0_RX; if (tx<12) printf("r%04x\n", miso[rx-1]); }
    }
    // at this point all tx data in FIFO but rx data may or may not have been moved out of FIFO
	evSpi(EV_SPILOOP, "spiLoop1", "");
if (meas) t_xfer1 = timer_us();
    
	// if necessary send dummy tx words to force larger number of rx words to be pulled through
	while (tx < rx_xfers) {
//spin_us(2);
    	stat = SPI0_STAT;
		if (!(stat & TX_FULL)) SPI0_TX = 0; tx++;
if (stat & RX_FULL) rfull++;
		if (!(stat & RX_EMPTY)) miso[rx++] = SPI0_RX; else rempty++;
		rcnt++;
	}
	//evSpi(EV_SPILOOP, "spiLoop2", "");
if (meas) t_xfer2 = timer_us();
	
	// handles the case of the FIFOs being slightly out-of-sync given that
	// tx/rx words moved must eventually be equal
	
	//jks
	#ifdef SPI_32
	int spin=0;
	while (rx < rx_xfers) {
    	stat = SPI0_STAT;
if (stat & RX_FULL) rfull++;
    	spin++;
		if (!(stat & RX_EMPTY)) { miso[rx++] = SPI0_RX; spin=0; }
		if ((rx == (rx_xfers-1)) && ((stat & (EOT|RXS)) == (EOT|RXS))) miso[rx++] = SPI0_RX;	// last doesn't show in fifo flags
		if ((spin > 1024) && ((stat & (EOT|RXS)) == (EOT|RXS))) {
			printf("%s STUCK: rfull %d rx %d rx_xfers %d stat %02x\n", TaskName(), rfull, rx, rx_xfers, stat);
			while (rx < rx_xfers) {
				//miso[rx++] = SPI0_RX | 0xf0000000;
				miso[rx++] = SPI0_RX;
			}
			break;
		}
	}
	#else
	while (rx < rx_xfers) {
    	stat = SPI0_STAT;
		if (!(stat & RX_EMPTY)) miso[rx++] = SPI0_RX;
		if ((rx == (rx_xfers-1)) && ((stat & (EOT|RXS)) == (EOT|RXS))) miso[rx++] = SPI0_RX;	// last doesn't show in fifo flags
	}
	#endif

    while ((SPI0_STAT & TX_EMPTY) == 0)		// FIXME: needed for 6 MHz case, why?
    	;

    while ((SPI0_STAT & EOT) == 0)		// FIXME: really needed?
    	;
if (meas) t_wait = timer_us();

	SPI0_CTRL = 0;
	SPI0_CONF = SPI_CONF;		// remove FORCE
#endif

	if (sel == SPI_BOOT) {
		spin_ms(10);
		GPIO_SET(0) = SPI0_CS1;
	}

	evSpi(EV_SPILOOP, "spiLoop exit", "");
if (meas) {
	printf("SPI %d rx_xfers %d rc %d re %d rf %d %7.3f %7.3f %7.3f\n", sel == SPI_BOOT, rx_xfers, rcnt, rempty, rfull,
	(float) (t_xfer1-t_start) / 1000, (float) (t_xfer2-t_start) / 1000, (float) (t_wait-t_start) / 1000);
}
}

///////////////////////////////////////////////////////////////////////////////////////////////

void peri_free() {
	assert(init);
    munmap((void *) pmux, MMAP_SIZE);
    munmap((void *) gpio0, MMAP_SIZE);
    munmap((void *) spi,  MMAP_SIZE);
    pmux = gpio0 = spi = NULL;
}

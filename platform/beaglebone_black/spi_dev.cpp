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
#include "cfg.h"
#include "peri.h"
#include "spi_dev.h"
#include "spi.h"
#include "coroutines.h"
#include "debug.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>

#ifdef __linux__
    #include <linux/types.h>
    #include <linux/spi/spidev.h>
#else
    #include "devl_spidev.h"
#endif

#define	SPI_DEVNAME	"/dev/spidev1.0"
#define	NOT(bit)	0	// documentation aid

static int spi_fd = -1;
static bool init = false;
static int speed;

void spi_dev_init(int spi_clkg, int spi_speed)
{
	// if not overridden in command line, set SPI speed according to configuration param
	if (spi_speed == SPI_48M) {
		bool error;
		int spi_clock = cfg_int("SPI_clock", &error, CFG_OPTIONAL);
		if (error || spi_clock == SPI_48M)
			spi_speed = SPI_48M;
		else
			spi_speed = SPI_24M;
	}
    
	if (use_spidev) {
		lprintf("### using SPI_DEV\n");
	
		if (spi_fd != -1) close(spi_fd);
	
		if ((spi_fd = open(SPI_DEVNAME, O_RDWR)) < 0) sys_panic("open spidev");
	
		u4_t max_speed, check_speed;
		if (spi_speed == SPI_48M) max_speed = 48000000; else
		if (spi_speed == SPI_24M) max_speed = 24000000; else
		if (spi_speed == SPI_12M) max_speed = 12000000; else
		if (spi_speed == SPI_6M) max_speed = 6000000; else
		if (spi_speed == SPI_3M) max_speed = 3000000; else
		if (spi_speed == SPI_1_5M) max_speed = 1500000; else
			panic("unknown spi_speed");
		speed = max_speed;
		if (ioctl(spi_fd, SPI_IOC_WR_MAX_SPEED_HZ, &max_speed) < 0) sys_panic("SPI_IOC_WR_MAX_SPEED_HZ");
		check_speed = 0;
		if (ioctl(spi_fd, SPI_IOC_RD_MAX_SPEED_HZ, &check_speed) < 0) sys_panic("SPI_IOC_RD_MAX_SPEED_HZ");
		check(max_speed == check_speed);
		char bpw = SPI_BPW, check_bpw;
		if (ioctl(spi_fd, SPI_IOC_WR_BITS_PER_WORD, &bpw) < 0) sys_panic("SPI_IOC_WR_BITS_PER_WORD");
		check_bpw = -1;
		if (ioctl(spi_fd, SPI_IOC_RD_BITS_PER_WORD, &check_bpw) < 0) sys_panic("SPI_IOC_RD_BITS_PER_WORD");
		check(bpw == check_bpw);
		printf("SPIDEV: max_speed %d bpw %d\n", check_speed, check_bpw);
		u4_t mode = SPI_MODE_0 | NOT(SPI_CS_HIGH) | NOT(SPI_NO_CS) | NOT(SPI_LSB_FIRST);
		if (ioctl(spi_fd, SPI_IOC_WR_MODE, &mode) < 0) sys_panic("SPI_IOC_WR_MODE");
	} else {
		lprintf("### using SPI PIO\n");
	
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
	
		SPI0_CONF = SPI_CONF(spi_clkg, spi_speed);
		spin_ms(1);
		//printf("SPI_CONF: conf 0x%08x stat 0x%x ctrl 0x%x\n", SPI0_CONF, SPI0_STAT, SPI0_CTRL);
	}

	spi_init();
	init = true;
}

void spi_dev(SPI_SEL sel, SPI_MOSI *mosi, int tx_xfers, SPI_MISO *miso, int rx_xfers)
{
    int rx=0, tx=0;
    u4_t stat;
    SPI_T *txb = mosi->msg;
    SPI_T *rxb = miso->msg;
    
    assert(init);

	if (sel == SPI_BOOT) {
		GPIO_CLR_BIT(SPI0_CS1);
	}
	
	evSpiDev(EC_EVENT, EV_SPILOOP, -1, "spi_dev", evprintf("%s(%d) T%dx R%dx", (sel != SPI_HOST)? "BOOT" : cmds[mosi->data.cmd], mosi->data.cmd, tx_xfers, rx_xfers));

	if (use_spidev) {
		int spi_bytes = SPI_X2B(MAX(tx_xfers, rx_xfers));
		struct spi_ioc_transfer spi_tr;
		memset(&spi_tr, 0, sizeof spi_tr);
		spi_tr.tx_buf = (unsigned long) txb;
		spi_tr.rx_buf = (unsigned long) rxb;
		spi_tr.len = spi_bytes;
		spi_tr.delay_usecs = 0;
		spi_tr.speed_hz = speed;
		spi_tr.bits_per_word = SPI_BPW;		// zero also means 8-bits?
		spi_tr.cs_change = 0;
	
		int actual_rxbytes;
		if ((actual_rxbytes = ioctl(spi_fd, SPI_IOC_MESSAGE(1), &spi_tr)) < 0) sys_panic("SPI_IOC_MESSAGE");
		check(actual_rxbytes == spi_bytes);
		if (actual_rxbytes != spi_bytes) printf("actual_rxbytes %d spi_bytes %d\n", actual_rxbytes, spi_bytes);
	} else {
		SPI0_CONF = FORCE | SPI_CONF(spi_clkg, spi_speed);		// add FORCE
		SPI0_CTRL = EN;
	
		while (tx < tx_xfers) {
			stat = SPI0_STAT;
			if (!(stat & TX_FULL)) {
				SPI0_TX = txb[tx++];		// move data only if FIFOs are able to accept
			}
			if (!(stat & RX_EMPTY)) {
				rxb[rx++] = SPI0_RX;
			}
		}
		// at this point all tx data in FIFO but rx data may or may not have been moved out of FIFO
		// if necessary send dummy tx words to force larger number of rx words to be pulled through
		while (tx < rx_xfers) {
			stat = SPI0_STAT;
			if (!(stat & TX_FULL)) {
				SPI0_TX = 0; tx++;
			}
			if (!(stat & RX_EMPTY)) {
				rxb[rx++] = SPI0_RX;
			}
		}
		
		// handles the case of the FIFOs being slightly out-of-sync given that
		// tx/rx words moved must eventually be equal
		while (rx < rx_xfers) {
			stat = SPI0_STAT;
			if (!(stat & RX_EMPTY)) {
				rxb[rx++] = SPI0_RX;
			}
			if ((rx == (rx_xfers-1)) && ((stat & (EOT|RXS)) == (EOT|RXS))) {
				rxb[rx++] = SPI0_RX;	// last doesn't show in fifo flags
			}
		}
	
		while ((SPI0_STAT & TX_EMPTY) == 0)		// FIXME: needed for 6 MHz case, why?
			;
	
		while ((SPI0_STAT & EOT) == 0)		// FIXME: really needed?
			;
	
		SPI0_CTRL = 0;
		SPI0_CONF = SPI_CONF(spi_clkg, spi_speed);		// remove FORCE
	}

	if (sel == SPI_BOOT) {
		GPIO_SET_BIT(SPI0_CS1);
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////


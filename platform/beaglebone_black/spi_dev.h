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

#pragma once

#include "sitara.h"
#include "spi.h"

// FIXME check: TURBO?, pulls, slew, dev tree & driver interaction

// on BBB spi0_cs1 is not available for general use
// so we use an ordinary gpio line in tandem with cs0


#define SPI_CONFIG		spi[0x110>>2]
#define	IDLEMODE		(0<<3)
#define	SOFT_RST		(1<<1)
#define	AUTOIDLE		(0<<0)

#define SPI_STATUS		spi[0x114>>2]

#define SPI_IRQENA		spi[0x11c>>2]

#define SPI_MODULE		spi[0x128>>2]
#define	FDAA			(1<<8)
#define	MOA				(1<<7)
#define	MASTER			(0<<2)
#define	GEN_CS			(0<<1)
#define	SINGLE_CHAN		(1<<0)

#define SPI0_CONF		spi[0x12c>>2]
#define	PHA				(0<<0)				// sample on rising edge
#define	POL				(0<<1)				// spi clk active high
#define	CLKD(s)			(((s)&0xf)<<2)
#define	SPI_48M			0
#define	SPI_24M			1
#define	SPI_12M			2
#define	SPI_6M			3
#define	SPI_3M			4
#define	SPI_1_5M		5
#define	EPOL_LOW		(1<<6)				// en/cs asserts low
#define	WL_32BITS		(31<<7)
#define	WL_16BITS		(15<<7)
#define	WL_8BITS		(7<<7)
#define	DMAW			(1<<14)
#define	DMAR			(1<<15)
#define	DPE0			(1<<16)				// mosi on d1, so inh tx on d0
#define	TURBO			(1<<19)
#define	FORCE			(1<<20)				// force en/cs to assert (and keep active between words)
#define	USE_FIFOS		(3<<27)
#define	CLKG(s)			(((s)&1)<<29)

#ifdef SPI_8
	#define	SPI_WL	WL_8BITS
	#define	SPI_BPW	8
#endif
#ifdef SPI_16
	#define	SPI_WL	WL_16BITS
	#define	SPI_BPW	16
#endif
#ifdef SPI_32
	#define	SPI_WL	WL_32BITS
	#define	SPI_BPW	32
#endif

#define	SPI_CONF(clkg, speed) \
	(CLKG(clkg) | USE_FIFOS | TURBO | DPE0 | SPI_WL | EPOL_LOW  | CLKD(speed) | POL | PHA)

#define SPI0_STAT		spi[0x130>>2]
#define	RX_FULL			(1<<6)
#define	RX_EMPTY		(1<<5)	// 0x20
#define	TX_FULL			(1<<4)
#define	TX_EMPTY		(1<<3)
#define	EOT				(1<<2)
#define	TXS				(1<<1)
#define	RXS				(1<<0)

#define SPI0_CTRL		spi[0x134>>2]
#define	EXTCLK			(0<<8)
#define	EN				(1<<0)

#define SPI0_TX			spi[0x138>>2]
#define SPI0_RX			spi[0x13c>>2]

#define SPI1_CONF		spi[0x140>>2]
#define SPI1_STAT		spi[0x144>>2]
#define SPI1_CTRL		spi[0x148>>2]

#define SPI_XFERLVL		spi[0x17c>>2]
#define	RX_AFL			(31<<8)				// rx fifo almost full

#define SPI_DAFTX		spi[0x180>>2]
#define SPI_DAFRX		spi[0x1A0>>2]


enum SPI_SEL {
    SPI_FPGA=0,  // Load FPGA
    SPI_BOOT=1,  // Load embedded CPU image
    SPI_HOST=2   // Host messaging
};

void spi_dev_init(int spi_clkg, int spi_speed);
void spi_dev(SPI_SEL sel, SPI_MOSI *mosi, int tx_xfers, SPI_MISO *miso, int rx_xfers);

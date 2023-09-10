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

// Copyright (c) 2015 - 2019 John Seamons, ZL4VO/KF6VO

#pragma once

#include "spi_dev.h"

// FIXME check: TURBO?, pulls, slew, dev tree & driver interaction

// on BBB spi0_cs1 is not available for general use
// so we use an ordinary gpio line in tandem with cs0


#define SPI_CONFIG		spi_m[0x110>>2]
#define	IDLEMODE		(0<<3)
#define	SOFT_RST		(1<<1)
#define	AUTOIDLE		(0<<0)

#define SPI_STATUS		spi_m[0x114>>2]

#define SPI_IRQENA		spi_m[0x11c>>2]

#define SPI_MODULE		spi_m[0x128>>2]
#define	FDAA			(1<<8)
#define	MOA				(1<<7)
#define	MASTER			(0<<2)
#define	GEN_CS			(0<<1)
#define	SINGLE_CHAN		(1<<0)

#define _SPI_CONF       0x12c
#define SPI0_CONF		spi_m[0x12c>>2]
#define SPIn_CONF(n)    spi_m[(0x12c+0x14*(n))>>2]
#define	PHA				(0<<0)				// sample on rising edge
#define	POL				(0<<1)				// spi clk active high
#define	CLKD(s)			(((s)&0xf)<<2)
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
#endif
#ifdef SPI_16
	#define	SPI_WL	WL_16BITS
#endif
#ifdef SPI_32
	#define	SPI_WL	WL_32BITS
#endif

#define	SPI_CONF(clkg, speed) \
	(CLKG(clkg) | USE_FIFOS | TURBO | DPE0 | SPI_WL | EPOL_LOW  | CLKD(speed) | POL | PHA)

#define SPI0_STAT		spi_m[0x130>>2]
#define	RX_FULL			(1<<6)
#define	RX_EMPTY		(1<<5)	// 0x20
#define	TX_FULL			(1<<4)
#define	TX_EMPTY		(1<<3)
#define	EOT				(1<<2)
#define	TXS				(1<<1)
#define	RXS				(1<<0)

#define SPI0_CTRL		spi_m[0x134>>2]
#define SPIn_CTRL(n)    spi_m[(0x134+0x14*(n))>>2]
#define	EXTCLK			(0<<8)
#define	EN				(1<<0)

#define SPI0_TX			spi_m[0x138>>2]
#define SPI0_RX			spi_m[0x13c>>2]

#define SPI1_CONF		spi_m[0x140>>2]
#define SPI1_STAT		spi_m[0x144>>2]
#define SPI1_CTRL		spi_m[0x148>>2]

#define SPI_XFERLVL		spi_m[0x17c>>2]
#define	RX_AFL			(31<<8)				// rx fifo almost full

#define SPI_DAFTX		spi_m[0x180>>2]
#define SPI_DAFRX		spi_m[0x1A0>>2]

void spi_pio_init(int spi_clkg, int spi_speed);
void spi_pio(SPI_MOSI *mosi, int tx_xfers, SPI_MISO *miso, int rx_xfers);

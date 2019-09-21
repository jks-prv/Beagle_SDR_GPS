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

enum SPI_SEL {
    SPI_FPGA=0,  // Load FPGA
    SPI_BOOT=1,  // Load embedded CPU image
    SPI_HOST=2   // Host messaging
};

#ifdef SPI_8
	#define	SPI_BPW	8
#endif
#ifdef SPI_16
	#define	SPI_BPW	16
#endif
#ifdef SPI_32
	#define	SPI_BPW	32
#endif

// values compatible with spi_pio:spi_speed
#define	SPI_48M			0
#define	SPI_24M			1
#define	SPI_12M			2
#define	SPI_6M			3
#define	SPI_3M			4
#define	SPI_1_5M		5

void spi_dev_init(int spi_clkg, int spi_speed);
void spi_dev(SPI_SEL sel, SPI_MOSI *mosi, int tx_xfers, SPI_MISO *miso, int rx_xfers);

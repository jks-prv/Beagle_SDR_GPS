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

#include "peri.h"
#include "spi.h"

enum SPI_SEL {
    SPI_FPGA=0,  // Load FPGA
    SPI_BOOT=1,  // Load embedded CPU image
    SPI_HOST=2   // Host messaging
};

typedef struct {
    SPI_SEL sel;
    SPI_MOSI *mosi;
    int tx_xfers;
    SPI_MISO *miso;
    int rx_xfers;
} spi_dev_ipc_t;

typedef struct {
    spi_dev_ipc_t spi_dev_ipc;
    SPI_MISO dpump_miso;
    SPI_MISO gps_search_miso, gps_channel_miso[GPS_MAX_CHANS], gps_clocks_miso, gps_iqdata_miso, gps_glitches_miso[2];
    SPI_MOSI gps_e1b_code_mosi;
    SPI_MISO wf_miso[MAX_RX_CHANS];
    SPI_MISO misc_miso[2];
    SPI_MISO spi_junk_miso, pingx_miso;
    SPI_MOSI spi_tx[N_SPI_TX];
    SPI_MOSI misc_mosi;
} spi_shmem_t;

#include "shmem_config.h"

#ifdef CPU_TDA4VM   // NB: not MULTI_CORE
        // shared memory enabled
#elif CPU_AM5729    // NB: not MULTI_CORE
    //#define SPI_SHMEM_DISABLE_TEST
    #ifdef SPI_SHMEM_DISABLE_TEST
        #warning dont forget to remove SPI_SHMEM_DISABLE_TEST
        //#define SPI_SHMEM_DISABLE
    #else
        // shared memory enabled
    #endif
#else
    #define SPI_SHMEM_DISABLE
#endif

#include "shmem.h"

#ifdef SPI_SHMEM_DISABLE
    #define SPI_SHMEM   spi_shmem_p
    extern spi_shmem_t  *spi_shmem_p;
#else
    #define SPI_SHMEM   (&shmem->spi_shmem)
#endif

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

#define SPI_SETUP_MODE      0
#define SPI_KIWISDR_1_MODE  0
#define SPI_KIWISDR_2_MODE  1

void spi_dev_init(int spi_clkg, int spi_speed);
void spi_dev_init2();
void spi_dev_mode(int spi_mode);
void spi_dev(SPI_SEL sel, SPI_MOSI *mosi, int tx_xfers, SPI_MISO *miso, int rx_xfers);

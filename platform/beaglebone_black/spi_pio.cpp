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
#include "spi_pio.h"
#include "spi.h"
#include "coroutines.h"
#include "debug.h"

void spi_pio_init(int spi_clkg, int spi_speed)
{
    lprintf("### using SPI_PIO\n");

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

void spi_pio(SPI_MOSI *mosi, int tx_xfers, SPI_MISO *miso, int rx_xfers)
{
    int rx=0, tx=0;
    u4_t stat;
    SPI_T *txb = mosi->msg;
    SPI_T *rxb = miso->msg;
    
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

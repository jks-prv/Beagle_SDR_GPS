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

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sched.h>

#include "types.h"
#include "misc.h"
#include "spi.h"
#include "peri.h"
#include "coroutines.h"

#define BUSY 0x90 // previous request not yet serviced by embedded CPU

///////////////////////////////////////////////////////////////////////////////////////////////

static SPI_MISO junk, *prev = &junk;
static u4_t pcmd;

///////////////////////////////////////////////////////////////////////////////////////////////

static lock_t spi_lock;

void spi_init()
{
	lock_init(&spi_lock);
}

///////////////////////////////////////////////////////////////////////////////////////////////

static void spi_scan(SPI_MOSI *mosi, SPI_MISO *miso=&junk, int rbytes=0) {
	int i;
	
	assert(rbytes <= NSPI_RX);
	
	int tx_bytes = sizeof(SPI_MOSI);
    int tx_xfers = SPI_B2X(tx_bytes);
    
    int rx_bytes = sizeof(miso->status) + rbytes;
    int rx_xfers = SPI_B2X(rx_bytes);

    int arx_xfers = MAX(tx_xfers, prev->len_xfers);
    int arx_bytes = SPI_X2B(arx_xfers);

//jks
#if 0
	static u4_t last_st;
	static float acc_st, acc2_st;
	static int big_rx;
	if (arx_bytes > 1024) big_rx = 1;
	u4_t st = timer_us();
	float inc_st = (float) (st - last_st) / 1000.0;
	acc_st += inc_st;
	if (mosi->cmd == CmdSetWFFreq) acc2_st = 0;
	printf("SCAN +%.3f %6.3f %6.3f %12s %16s tx(%dX=%dB) rx(%dX=%dB) arx(%dX=%dB)\n",
	inc_st, acc_st, acc2_st, TaskName(), cmds[mosi->cmd], tx_xfers, tx_bytes, rx_xfers, rx_bytes, arx_xfers, arx_bytes);
	if (mosi->cmd == CmdDuplex && big_rx) {
		acc_st = 0;
		big_rx = 0;
	} else {
	}
	acc2_st += inc_st;
	last_st = st;
#endif

//memset(miso, 0xee, sizeof(*miso));

    miso->len_xfers = rx_xfers;
    miso->cmd = mosi->cmd;
    rx_xfers = arx_xfers;

	if (mosi->cmd == CmdDuplex) mosi->cmd = CmdDummy;
	if (mosi->cmd == CmdNoDuplex) mosi->cmd = CmdDummy;

    for (;;) {
        peri_spi(SPI_HOST,
            mosi->msg, tx_xfers,   // MOSI: new request
            prev->msg, rx_xfers);  // MISO: response to previous caller's request
        evSpi(EV_SPILOOP, "spiScan", "peri_spi done");

		// fixme: understand why is this needed (hangs w/o it)
        if (spi_delay) spin_us(spi_delay); else usleep(10);
        evSpi(EV_SPILOOP, "spiScan", "spin_us done");
        if (prev->status != BUSY) break; // new request accepted?
        evSpi(EV_SPILOOP, "spiScan", "BUSY -> NextTask");
        NextTaskL("spi_scan"); // wait and try again
    }
    
    //jks
    #if 0
    printf("RX %d: ", prev->len_xfers);
    if (prev == &junk) {
    	printf("(junk) ");
    } else {
    	printf("%s ", cmds[prev->cmd]);
    }
	for (i=0; i<(prev->len_xfers+10); i++) {
		printf("%d:", i);
		#ifdef SPI_8
		printf("%02x ", prev->msg[i]);
		#endif
		#ifdef SPI_16
		printf("%04x ", prev->msg[i]);
		#endif
		#ifdef SPI_32
		printf("%08x ", prev->msg[i]);
		#endif
	}
    printf("\n");
    #endif

	u4_t status = prev->status;
    prev = miso; // next caller collects this for us
pcmd=mosi->cmd;

	//if (status & 0x0fff)
	//printf("st %04x\n", status);
	//if (mosi->cmd == CmdGetRXCount) printf("C");
	//if (mosi->cmd == CmdGetRX) printf("GRX\n");
	static int ff;
	if (status & (1<<SPI_SFT)) {
		rx0_wakeup = 1;
		evSnd(EV_SND, "wakeup", "");
		//printf(".");
		ff = 0;
	} else {
		rx0_wakeup = 0;
		if (!ff) {
			//printf(".");
			ff = 1;
		}
	}
	
	// process rx channel wakeup bits
	for (i=0; i<RX_CHANS; i++) {
		u4_t ch = 1<<(i+SPI_SFT);
		conn_t *c;
		if (status & ch) {
			c = &conns[i*2];
			assert(c->type == STREAM_SOUND);
			//if (c->task && !c->stop_data) printf("wakeup %d\n", c->task);
			//if (c->task && !c->stop_data) { printf("%d:%d ", i, c->task); fflush(stdout); }
			if (c->task && !c->stop_data) TaskWakeup(c->task, FALSE, 0);
		}
	}
}

int rx0_wakeup;

///////////////////////////////////////////////////////////////////////////////////////////////

// NB: tx[1234] are static, rather than on the stack, so DMA works with USE_SPIDEV mode

void spi_set(SPI_CMD cmd, uint16_t wparam, uint32_t lparam) {
    lock_enter(&spi_lock);
	static SPI_MOSI tx1 __attribute__ ((aligned(16)));
	tx1.cmd = cmd; tx1.wparam = wparam; tx1.lparam = lparam;
    spi_scan(&tx1);
    lock_leave(&spi_lock);
}

void spi_set_noduplex(SPI_CMD cmd, uint16_t wparam, uint32_t lparam) {
	lock_enter(&spi_lock);		// block other threads
	static SPI_MOSI tx2 __attribute__ ((aligned(16)));
	tx2.cmd = cmd; tx2.wparam = wparam; tx2.lparam = lparam;
	//evSpi(-EV_SPILOOP, "spiSetNoDuplex", "enter");
    spi_scan(&tx2);				// Send request
	//evSpi(EV_SPILOOP, "spiSetNoDuplex", "sent req");
    
	tx2.cmd = CmdNoDuplex;
    spi_scan(&tx2);				// Collect response to our own request
	//evSpi(EV_SPILOOP, "spiSetNoDuplex", "exit");
    lock_leave(&spi_lock);		// release block
}

void spi_get(SPI_CMD cmd, SPI_MISO *rx, int bytes, uint16_t wparam, uint32_t lparam) {
	lock_enter(&spi_lock);
	static SPI_MOSI tx3 __attribute__ ((aligned(16)));
	tx3.cmd = cmd; tx3.wparam = wparam; tx3.lparam = lparam;
    spi_scan(&tx3, rx, bytes);
    lock_leave(&spi_lock);
    
    rx->status=BUSY;
    while(rx->status==BUSY) {
    	NextTaskL("spi_get"); // wait for response
    }
}

void spi_get_noduplex(SPI_CMD cmd, SPI_MISO *rx, int bytes, uint16_t wparam, uint32_t lparam) {
	lock_enter(&spi_lock);		// block other threads
	static SPI_MOSI tx4 __attribute__ ((aligned(16)));
	tx4.cmd = cmd; tx4.wparam = wparam; tx4.lparam = lparam;
	evSpi(-EV_SPILOOP, "spiGetNoDuplex", "enter");
    spi_scan(&tx4, rx, bytes);	// Send request
	evSpi(EV_SPILOOP, "spiGetNoDuplex", "sent req");

	tx4.cmd = CmdNoDuplex;
    spi_scan(&tx4);				// Collect response to our own request
	evSpi(EV_SPILOOP, "spiGetNoDuplex", "exit");
    lock_leave(&spi_lock);		// release block
}

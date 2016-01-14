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

// Copyright (c) 2015 John Seamons, ZL/KF6VO

#include "types.h"
#include "config.h"
#include "kiwi.h"
#include "misc.h"
#include "timer.h"
#include "web.h"
#include "peri.h"
#include "spi.h"
#include "gps.h"
#include "coroutines.h"
#include "debug.h"
#include "data_pump.h"

#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <sched.h>
#include <math.h>
#include <fftw3.h>

rx_dpump_t rx_dpump[RX_CHANS];
static SPI_MISO dp_miso;
static bool initial_seq;
static u2_t seq;

struct rx_data_t {
#ifdef SND_SEQ_CHECK
	u2_t magic;
	u2_t seq;
#endif
	rx_iq_t iq_t[NRX_SAMPS * RX_CHANS];
} __attribute__((packed));

static float rescale;

#ifdef SND_SEQ_CHECK
static int audio_dropped, last_audio_dropped;
#endif

static void snd_service()
{
	int j;
	
	SPI_MISO *miso = &dp_miso;

	rx_data_t *rxd = (rx_data_t *) &miso->word[0];

	#ifdef SND_SEQ_CHECK
	rxd->magic = 0;
	#endif
	
	// use noduplex here because we don't want to yield
	evDPC(EC_TRIG3, EV_DPUMP, -1, "snd_svc", "CmdGetRX..");
	spi_get_noduplex(CmdGetRX, miso, sizeof(rx_data_t));
	evDPC(EC_EVENT, EV_DPUMP, -1, "snd_svc", "..CmdGetRX");

	evDP(EC_TRIG2, EV_DPUMP, -1, "snd_service", evprintf("SERVICED SEQ %d %%%%%%%%%%%%%%%%%%%%",
		rxd->seq));
	//evDP(EC_TRIG2, EV_DPUMP, 15000, "SND", "SERVICED ----------------------------------------");
	
	#ifdef SND_SEQ_CHECK
	if (rxd->magic != 0x0ff0) {
		evDPC(EC_EVENT, EV_DPUMP, -1, "DATAPUMP", evprintf("BAD MAGIC 0x%04x", rxd->magic));
		if (ev_dump) evDP(EC_DUMP, EV_DPUMP, ev_dump, "DATAPUMP", evprintf("DUMP in %.3f sec", ev_dump/1000.0));
	}
	
	if (!initial_seq) {
		seq = rxd->seq;
		initial_seq = true;
	}
	u2_t new_seq = rxd->seq;
	if (seq != new_seq) {
		//printf("$dp %d:%d(%d)\n", seq, new_seq, new_seq-seq);
		evDPC(EC_EVENT, EV_DPUMP, -1, "SEQ DROP", evprintf("$dp %d:%d(%d)", seq, new_seq, new_seq-seq));
		audio_dropped++;
		//TaskLastRun();
		bool dump = false;
		//bool dump = true;
		//bool dump = (new_seq-seq < 0);
		//bool dump = (audio_dropped == 6);
		if (dump && ev_dump) evNT(EC_DUMP, EV_NEXTTASK, ev_dump, "NextTask", evprintf("DUMP IN %.3f SEC",
			ev_dump/1000.0));
		seq = new_seq;
	}
	seq++;
	#endif

	TYPECPX *i_samps[RX_CHANS];
	for (j=0; j < RX_CHANS; j++) {
		rx_dpump_t *rx = &rx_dpump[j];
		i_samps[j] = rx->in_samps[rx->wr_pos];
	}

	rx_iq_t *iqp = (rx_iq_t*) &rxd->iq_t;

	for (j=0; j<NRX_SAMPS; j++) {

		for (int ch=0; ch < RX_CHANS; ch++) {
			if (rx_chan[ch].enabled) {
				s4_t i, q;
				i = S24_8_16(iqp->i3, iqp->i);
				q = S24_8_16(iqp->q3, iqp->q);
				
				// NB: i&q reversed to get correct sideband polarity; fixme: why?
				// [probably because mixer NCO polarity is wrong, i.e. cos/sin should really be cos/-sin]
				i_samps[ch]->re = q * rescale;
				i_samps[ch]->im = i * rescale;
				i_samps[ch]++;
			}
			iqp++;
		}
	}

	for (int ch=0; ch < RX_CHANS; ch++) {
		if (rx_chan[ch].enabled) {
			rx_dpump_t *rx = &rx_dpump[ch];
			rx->wr_pos = (rx->wr_pos+1) & (N_DPBUF-1);
		}
	}
}

void rx_enable(int chan, bool en)
{
	rx_chan_t *rx = &rx_chan[chan];
	rx->enabled = en;
}

u64_t interrupt_task_last_run;

static void data_pump()
{
	ctrl_clr_set(CTRL_INTERRUPT, 0);
	spi_set(CmdSetRXNsamps, NRX_SAMPS-1);
	evDP(EC_EVENT, EV_DPUMP, -1, "dpump_init", evprintf("INIT: SPI CTRL_INTERRUPT %d",
		GPIO_READ_BIT(GPIO0_15)));

	while (1) {

		evDP(EC_EVENT, EV_DPUMP, -1, "data_pump", evprintf("SLEEPING: SPI CTRL_INTERRUPT %d",
			GPIO_READ_BIT(GPIO0_15)));
		TaskSleep(0);
		ctrl_clr_set(CTRL_INTERRUPT, 0);		// ack interrupt, and updates spi status
		evDP(EC_EVENT, EV_DPUMP, -1, "data_pump", evprintf("WAKEUP: SPI CTRL_INTERRUPT %d",
			GPIO_READ_BIT(GPIO0_15)));

		interrupt_task_last_run = timer_us64();
		evDP(EC_EVENT, EV_DPUMP, -1, "data_pump", evprintf("interrupt last run @%.6f ",
			(float) interrupt_task_last_run / 1000000));
		
		snd_service();
		
		for (int ch=0; ch < RX_CHANS; ch++) {
			rx_chan_t *rx = &rx_chan[ch];
			if (!rx->enabled) continue;
			conn_t *c = rx->conn;
			assert(c);
			if (c->task && !c->stop_data) {
				TaskWakeup(c->task, FALSE, 0);
			}
			#ifdef SND_SEQ_CHECK
			if (audio_dropped != last_audio_dropped) {
				send_msg(c, SM_NO_DEBUG, "MSG audio_dropped=%d", audio_dropped);
			}
			#endif
		}
		#ifdef SND_SEQ_CHECK
		if (audio_dropped != last_audio_dropped) last_audio_dropped = audio_dropped;
		#endif
	}
}

void data_pump_init()
{
	rescale = powf(2, -RXOUT_SCALE + CUTESDR_SCALE);

	CreateTaskF(data_pump, DATAPUMP_PRIORITY, CTF_POLL_INTR);
}

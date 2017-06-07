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
int rx_adc_ovfl;
static SPI_MISO dp_miso;

struct rx_data_t {
	#ifdef SND_SEQ_CHECK
		u2_t magic;
		u2_t snd_seq;
	#endif
	rx_iq_t iq_t[NRX_SAMPS * RX_CHANS];
	u2_t ticks[3];
	u2_t write_ctr_stored, write_ctr_current;
} __attribute__((packed));

static float rescale;
int audio_dropped;
u4_t dpump_resets, dpump_hist[NRX_BUFS];

#ifdef SND_SEQ_CHECK
	static bool initial_seq;
	static u2_t snd_seq;
#endif

static void snd_service()
{
	int j;
	
	SPI_MISO *miso = &dp_miso;

	rx_data_t *rxd = (rx_data_t *) &miso->word[0];

	u4_t diff;

    do {

        #ifdef SND_SEQ_CHECK
            rxd->magic = 0;
        #endif
        
        // use noduplex here because we don't want to yield
        evDPC(EC_TRIG3, EV_DPUMP, -1, "snd_svc", "CmdGetRX..");
    
        // CTRL_INTERRUPT cleared as a side-effect of the CmdGetRX
        spi_get_noduplex(CmdGetRX, miso, sizeof(rx_data_t));
        rx_adc_ovfl = miso->status & SPI_ST_ADC_OVFL;
        
        evDPC(EC_EVENT, EV_DPUMP, -1, "snd_svc", "..CmdGetRX");
        
        evDP(EC_TRIG2, EV_DPUMP, -1, "snd_service", evprintf("SERVICED SEQ %d %%%%%%%%%%%%%%%%%%%%",
            rxd->snd_seq));
        //evDP(EC_TRIG2, EV_DPUMP, 15000, "SND", "SERVICED ----------------------------------------");
        
        #ifdef SND_SEQ_CHECK
            if (rxd->magic != 0x0ff0) {
                evDPC(EC_EVENT, EV_DPUMP, -1, "DATAPUMP", evprintf("BAD MAGIC 0x%04x", rxd->magic));
                if (ev_dump) evDPC(EC_DUMP, EV_DPUMP, ev_dump, "DATAPUMP", evprintf("DUMP in %.3f sec", ev_dump/1000.0));
            }
    
            if (!initial_seq) {
                snd_seq = rxd->snd_seq;
                initial_seq = true;
            }
            u2_t new_seq = rxd->snd_seq;
            if (snd_seq != new_seq) {
                real_printf("#%d %d:%d(%d)\n", audio_dropped, snd_seq, new_seq, new_seq-snd_seq);
                evDPC(EC_EVENT, EV_DPUMP, -1, "SEQ DROP", evprintf("$dp #%d %d:%d(%d)", audio_dropped, snd_seq, new_seq, new_seq-snd_seq));
                audio_dropped++;
                //TaskLastRun();
                bool dump = false;
                //bool dump = true;
                //bool dump = (new_seq-snd_seq < 0);
                //bool dump = (audio_dropped == 2);
                //bool dump = (audio_dropped == 6);
                if (dump && ev_dump) evNT(EC_DUMP, EV_NEXTTASK, ev_dump, "NextTask", evprintf("DUMP IN %.3f SEC",
                    ev_dump/1000.0));
                snd_seq = new_seq;
            }
            snd_seq++;
            bool dump = false;
            //bool dump = (snd_seq == 1000);
            if (dump && ev_dump) evNT(EC_DUMP, EV_NEXTTASK, ev_dump, "NextTask", evprintf("DUMP IN %.3f SEC",
                ev_dump/1000.0));
        #endif
    
        TYPECPX *i_samps[RX_CHANS];
        for (int ch=0; ch < RX_CHANS; ch++) {
            rx_dpump_t *rx = &rx_dpump[ch];
            i_samps[ch] = rx->in_samps[rx->wr_pos];
        }
    
        rx_iq_t *iqp = (rx_iq_t*) &rxd->iq_t;
    
        for (j=0; j<NRX_SAMPS; j++) {
    
            for (int ch=0; ch < RX_CHANS; ch++) {
                if (rx_channels[ch].enabled) {
                    s4_t i, q;
                    i = S24_8_16(iqp->i3, iqp->i);
                    q = S24_8_16(iqp->q3, iqp->q);
                    
                    // NB: I/Q reversed to get correct sideband polarity; fixme: why?
                    // [probably because mixer NCO polarity is wrong, i.e. cos/sin should really be cos/-sin]
                    i_samps[ch]->re = q * rescale + DC_offset_I;
                    i_samps[ch]->im = i * rescale + DC_offset_Q;
                    i_samps[ch]++;
                }
                iqp++;
            }
        }
    
        for (int ch=0; ch < RX_CHANS; ch++) {
            if (rx_channels[ch].enabled) {
                rx_dpump_t *rx = &rx_dpump[ch];
    
                rx->ticks[rx->wr_pos][0] = rxd->ticks[0];
                rx->ticks[rx->wr_pos][1] = rxd->ticks[1];
                rx->ticks[rx->wr_pos][2] = rxd->ticks[2];
    
                #ifdef SND_SEQ_CHECK
                    rx->in_seq[rx->wr_pos] = snd_seq;
                #endif
                
                rx->wr_pos = (rx->wr_pos+1) & (N_DPBUF-1);
            }
        }
        
        u2_t current = rxd->write_ctr_current;
        u2_t stored = rxd->write_ctr_stored;
        if (current >= stored) {
            diff = current - stored;
        } else {
            diff = (0xffff - stored) + current;
        }
        
        if (diff > (NRX_BUFS-1)) {
		    dpump_resets++;
		    //lprintf("DATAPUMP RESET #%d\n", dpump_resets);
            if (ev_dump && dpump_resets > 1)
                evLatency(EC_DUMP, EV_DPUMP, ev_dump, "DATAPUMP", evprintf("DUMP in %.3f sec", ev_dump/1000.0));
            lprintf("DATAPUMP RESET #%d %d %d %d\n", dpump_resets, diff, stored, current);
		    memset(dpump_hist, 0, sizeof(dpump_hist));
            spi_set(CmdSetRXNsamps, NRX_SAMPS);
            diff = 0;
        } else {
            dpump_hist[diff]++;
        }
        
    } while (diff > 1);

}

bool rx_dpump_run;

void rx_enable(int chan, rx_chan_action_e action)
{
	rx_chan_t *rx = &rx_channels[chan];
	
	switch (action) {

	case RX_CHAN_ENABLE: rx->enabled = true; break;
	case RX_CHAN_DISABLE: rx->enabled = false; break;
	case RX_CHAN_FREE: memset(rx, 0, sizeof(rx_chan_t)); break;
	default: panic("rx_enable"); break;

	}

	bool no_users = true;
	for (int i = 0; i < RX_CHANS; i++) {
		rx = &rx_channels[i];
		if (rx->enabled) {
			no_users = false;
			break;
		}
	}
	
	// stop the data pump when the last user leaves
	if (rx_dpump_run && no_users) {
		rx_dpump_run = false;
		spi_set(CmdSetRXNsamps, 0);
		ctrl_clr_set(CTRL_INTERRUPT, 0);
		//printf("#### STOP dpump\n");
	}

	// start the data pump when the first user arrives
	if (!rx_dpump_run && !no_users) {
		rx_dpump_run = true;
		ctrl_clr_set(CTRL_INTERRUPT, 0);
		spi_set(CmdSetRXNsamps, NRX_SAMPS);
		//printf("#### START dpump\n");
	}
}

int rx_chan_free(int *idx)
{
	int i, free_cnt = 0, free_idx = -1;
	rx_chan_t *rx;

	for (i = 0; i < RX_CHANS; i++) {
		rx = &rx_channels[i];
		if (!rx->busy) {
			free_cnt++;
			if (free_idx == -1) free_idx = i;
		}
	}
	
	if (idx != NULL) *idx = free_idx;
	return free_cnt;
}

static void data_pump(void *param)
{
	evDP(EC_EVENT, EV_DPUMP, -1, "dpump_init", evprintf("INIT: SPI CTRL_INTERRUPT %d",
		GPIO_READ_BIT(GPIO0_15)));

	while (1) {

		evDP(EC_EVENT, EV_DPUMP, -1, "data_pump", evprintf("SLEEPING: SPI CTRL_INTERRUPT %d",
			GPIO_READ_BIT(GPIO0_15)));

		TaskSleepReasonUsec("wait for interrupt", 0);

		evDP(EC_EVENT, EV_DPUMP, -1, "data_pump", evprintf("WAKEUP: SPI CTRL_INTERRUPT %d",
			GPIO_READ_BIT(GPIO0_15)));

		snd_service();
		
		for (int ch=0; ch < RX_CHANS; ch++) {
			rx_chan_t *rx = &rx_channels[ch];
			if (!rx->enabled) continue;
			conn_t *c = rx->conn_snd;
			assert(c);
			if (c->task) {
				TaskWakeup(c->task, FALSE, 0);
			}
		}
	}
}

void data_pump_init()
{
	// verify that audio samples will fit in hardware buffers
	#define WORDS_PER_SAMP 3	// 2 * 24b IQ = 3 * 16b
	
	// does a single NRX_SAMPS transfer fit in the SPI buf?
	assert (sizeof(rx_data_t) <= NSPI_RX);	// in bytes
	
	// see rx_dpump_t.in_samps[][]
	assert (FASTFIR_OUTBUF_SIZE > NRX_SAMPS);
	
	// rescale factor from hardware samples to what CuteSDR code is expecting
	rescale = powf(2, -RXOUT_SCALE + CUTESDR_SCALE);

	CreateTaskF(data_pump, 0, DATAPUMP_PRIORITY, CTF_POLL_INTR, 0);
}

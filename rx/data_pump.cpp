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

// Copyright (c) 2015-2024 John Seamons, ZL4VO/KF6VO

#include "types.h"
#include "config.h"
#include "kiwi.h"
#include "valgrind.h"
#include "rx.h"
#include "misc.h"
#include "timer.h"
#include "web.h"
#include "spi.h"
#include "spi_dev.h"
#include "gps.h"
#include "coroutines.h"
#include "debug.h"
#include "data_pump.h"
#include "fpga.h"

#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <sched.h>
#include <math.h>

rx_dpump_t rx_dpump[MAX_RX_CHANS];
dpump_t dpump;

#ifdef RX_SHMEM_DISABLE
    static rx_shmem_t rx_shmem;
    rx_shmem_t *rx_shmem_p = &rx_shmem;
#endif

bool have_snd_users;

#ifdef USE_SDR

struct rx_data_t {
    #ifdef SND_SEQ_CHECK
        struct rx_header_t {
            u2_t magic;
            u2_t snd_seq;
        } hdr;
    #endif
	iq3_t iq_t[MAX_NRX_SAMPS * MAX_RX_CHANS];
} __attribute__((packed));
static rx_data_t *rxd;

// Must copy to intermediate buffer because real_printf() delay would otherwise cause
// buffer being inspected to be overwritten.
//#define DP_DUMP_WB
#ifdef DP_DUMP_WB
    static rx_data_t rxd_debug;
#endif

struct rx_trailer_t {
	u2_t ticks[3];
	u2_t write_ctr_stored, write_ctr_current;
} __attribute__((packed));
static rx_trailer_t *rxt;

// rescale factor from hardware samples to what CuteSDR code is expecting
const TYPEREAL cicf_gain_dB[N_FW_SEL] = {
    4.5, 4.5, 4.5 + 8.6, 4.5, 4.5    // empirical measurement using sig gen
};
static TYPEREAL rescale;

static int rx_xfer_size;
static u4_t last_run_us;

#ifdef SND_SEQ_CHECK
	static bool initial_seq;
	static u2_t snd_seq;
#endif

static void snd_service()
{
	int j;
	SPI_MISO *miso = &SPI_SHMEM->dpump_miso;
	u4_t diff, moved=0;
	
    evLatency(EC_EVENT, EV_DPUMP, 0, "DATAPUMP", "snd_service() BEGIN");
    do {

        #ifdef SND_SEQ_CHECK
            rxd->hdr.magic = 0;
        #endif
        
        // use noduplex here because we don't want to yield
        evDPC(EC_TRIG3, EV_DPUMP, -1, "snd_svc", "CmdGetRX..");
    
        // CTRL_SND_INTR cleared as a side-effect of the CmdGetRX
        spi_get3_noduplex(CmdGetRX, miso, rx_xfer_size, nrx_samps_rem, nrx_samps_loop);
        moved++;
        dpump.rx_adc_ovfl = miso->status & SPI_ADC_OVFL;
        if (dpump.rx_adc_ovfl) dpump.rx_adc_ovfl_cnt++;
        
        evDPC(EC_EVENT, EV_DPUMP, -1, "snd_svc", "..CmdGetRX");
        
        evDP(EC_TRIG2, EV_DPUMP, -1, "snd_service", evprintf("SERVICED SEQ %d %%%%%%%%%%%%%%%%%%%%",
            rxd->snd_seq));
        //evDP(EC_TRIG2, EV_DPUMP, 15000, "SND", "SERVICED ----------------------------------------");
        
        #ifdef SND_SEQ_CHECK
            if (rxd->hdr.magic != 0x0ff0) {
                printf("BAD MAGIC 0x%04x", rxd->hdr.magic)
                evDPC(EC_EVENT, EV_DPUMP, -1, "DATAPUMP", evprintf("BAD MAGIC 0x%04x", rxd->magic));
                if (ev_dump) evDPC(EC_DUMP, EV_DPUMP, ev_dump, "DATAPUMP", evprintf("DUMP in %.3f sec", ev_dump/1000.0));
            }
    
            if (!initial_seq) {
                snd_seq = rxd->hdr.snd_seq;
                initial_seq = true;
            }
            u2_t new_seq = rxd->hdr.snd_seq;
            if (snd_seq != new_seq) {
                real_printf("#%d %d:%d(%d)\n", dpump.audio_dropped, snd_seq, new_seq, new_seq-snd_seq);
                evDPC(EC_EVENT, EV_DPUMP, -1, "SEQ DROP", evprintf("$dp #%d %d:%d(%d)", dpump.audio_dropped, snd_seq, new_seq, new_seq-snd_seq));
                dpump.audio_dropped++;
                //TaskLastRun();
                bool dump = false;
                //bool dump = true;
                //bool dump = (new_seq-snd_seq < 0);
                //bool dump = (dpump.audio_dropped == 2);
                //bool dump = (dpump.audio_dropped == 6);
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
    
        TYPECPX *i_samps[MAX_RX_CHANS];
        TYPECPX24 *i_wb_samps;
        
        for (int ch = 0; ch < rx_chans; ch++) {
            rx_dpump_t *rx = &rx_dpump[ch];
            i_samps[ch] = rx->in_samps[rx->wr_pos];
        }
        if (kiwi.isWB) {
            rx_dpump_t *rx = &rx_dpump[RX_CHAN0];
            i_wb_samps = rx->wb_samps[rx->wr_pos];
            //real_printf("[%d=%p] ", rx->wr_pos, i_wb_samps); fflush(stdout);
        }
            
        iq3_t *iqp = (iq3_t *) &rxd->iq_t;
    
        #if 0
            // check 48-bit ticks counter timestamp
            static int debug_ticks;
            if (debug_ticks >= 1024 && debug_ticks < 1024+8) {
                for (int j=-1; j>-2; j--) {
                    real_printf("debug_iq3 %d %d(%d*%d) %02x%04x %02x%04x\n", j, nrx_samps*rx_wb_buf_chans+j, nrx_samps, rx_wb_buf_chans,
                        rxd->iq_t[nrx_samps*rx_wb_buf_chans+j].i3, rxd->iq_t[nrx_samps*rx_wb_buf_chans+j].i,
                        rxd->iq_t[nrx_samps*rx_wb_buf_chans+j].q3, rxd->iq_t[nrx_samps*rx_wb_buf_chans+j].q);
                }
                real_printf("debug_ticks %04x[2] %04x[1] %04x[0]\n", rxt->ticks[2], rxt->ticks[1], rxt->ticks[0]);
                real_printf("debug_bufcnt %04x\n\n", rxt->write_ctr_stored);
            }
            debug_ticks++;
        #endif
        
        if (kiwi.isWB) {
            #ifdef DP_DUMP_WB
                iq3_t *iqd_p = rxd_debug.iq_t;
                #define DP_DUMP_GO 100
                static u4_t go;
            #endif
    
            //if (rx_channels[RX_CHAN0].data_enabled) { real_printf("."); fflush(stdout); }

            for (j=0; j < nrx_samps; j++) {

                // rx1: wb0..wb[rx_wb_buf_chans-1]
                for (int wb = 0; wb < rx_wb_buf_chans; wb++) {
                    if (rx_channels[RX_CHAN0].data_enabled) {
                        s4_t i, q;
                        //#define WB_PATTERN
                        #ifdef WB_PATTERN

                            // sdr++: zoom in fully to see the picket
                            #define PICKET
                            #ifdef PICKET
                                static u4_t ctr;
                                if ((ctr % 1024) == 0) i = 0x3fff; else i = 0;
                                ctr++;
                                q = 0;
                            #else
                                i = S24_8_16(iqp->i3, iqp->i);
                                q = S24_8_16(iqp->q3, iqp->q);
                            #endif

                            i_wb_samps->re = q;     // preserve bit pattern (i.e. no scaling)
                            i_wb_samps->im = i;
                        #else
                            i = S24_8_16(iqp->i3, iqp->i);
                            q = S24_8_16(iqp->q3, iqp->q);
        
                            // sign extend
                            // for RX2_BITS = 18
                            // 2222 1111 111111
                            // 3210 9876 54321098 76543210
                            // eeee eeSd dddddddd dddddddd
                            //        12 345678
                            //                 12 34567890
                            
                            i_wb_samps->re = q;
                            i_wb_samps->im = i;
                        #endif
                        i_wb_samps++;
                    }

                    #ifdef DP_DUMP_WB
                        if (go == DP_DUMP_GO) {
                            iqd_p->i = iqp->i;
                            iqd_p->i3 = iqp->i3;
                            iqd_p->q = iqp->q;
                            iqd_p->q3 = iqp->q3;
                            iqd_p++;
                        }
                    #endif

                    iqp++;
                }
            }
        
            #ifdef DP_DUMP_WB
                if (go == DP_DUMP_GO) {
                    iqd_p = rxd_debug.iq_t;
                    for (j=0; j < nrx_samps; j++) {
                        real_printf("%04d: ", j);
                        for (int ch=0; ch < rx_wb_buf_chans; ch++) {
                            #define DP_DUMP_HEX
                            #ifdef DP_DUMP_HEX
                                real_printf("[ch%d %02x|%02x|%04x|%04x] ", ch, iqd_p->i3, iqd_p->q3, iqd_p->i, iqd_p->q);
                            #else
                                s4_t i, q;
                                i = S24_8_16(iqd_p->i3, iqd_p->i);
                                q = S24_8_16(iqd_p->q3, iqd_p->q);
                                real_printf("[ch%d %6d|%6d] ", ch, i, q);
                            #endif
                            iqd_p++;
                        }
                        real_printf("\n"); fflush(stdout);
                    }
                }
                
                go++;
            #endif
        } else {

            // NB: I/Q reversed below to get correct sideband polarity
            // i.e. normal case: re=q im=i; spectral_inversion case: re=i im=q
            // Probably because mixer NCO polarity is wrong, i.e. cos/sin should really be cos/-sin
            // but we never took the time to verify this.
            if (kiwi.spectral_inversion) {
                for (j=0; j < nrx_samps; j++) {
    
                    for (int ch=0; ch < rx_chans; ch++) {
                        if (rx_channels[ch].data_enabled) {
                            s4_t i, q;
                            i = S24_8_16(iqp->i3, iqp->i);
                            q = S24_8_16(iqp->q3, iqp->q);
    
                            i_samps[ch]->re = i * rescale + DC_offset_I;
                            i_samps[ch]->im = q * rescale + DC_offset_Q;
                            i_samps[ch]++;
                        }
                        
                        iqp++;
                    }
                }
            } else {
                for (j=0; j < nrx_samps; j++) {
    
                    for (int ch=0; ch < rx_chans; ch++) {
                        if (rx_channels[ch].data_enabled) {
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
            }
        }
    
        int n_dpbuf = kiwi.isWB? N_WB_DPBUF : N_DPBUF;
        
        for (int ch=0; ch < rx_chans; ch++) {
            //if (ch == 1) real_printf("%d:en%d ", ch, rx_channels[ch].data_enabled); fflush(stdout);
            if (rx_channels[ch].data_enabled) {
                rx_dpump_t *rx = &rx_dpump[ch];
                //real_printf("%d:%d ", ch, rx->wr_pos); fflush(stdout);

                rx->ticks[rx->wr_pos] = S16x4_S64(0, rxt->ticks[2], rxt->ticks[1], rxt->ticks[0]);
    
                #ifdef SND_SEQ_CHECK
                    rx->in_seq[rx->wr_pos] = snd_seq;
                #endif
                
                rx->wr_pos = (rx->wr_pos+1) & (n_dpbuf-1);
                rx_channels[ch].wr++;
                
                diff = (rx->wr_pos >= rx->rd_pos)? (rx->wr_pos - rx->rd_pos) : (n_dpbuf - rx->rd_pos + rx->wr_pos);
                if (diff >= n_dpbuf) {
                    dpump.in_hist_resets++;
                    #if 0
                        real_printf("#"); fflush(stdout);
                    #endif
                } else {
                    dpump.in_hist[diff]++;
                }
            }
        }
        
        u2_t current = rxt->write_ctr_current;
        u2_t stored = rxt->write_ctr_stored;
        if (current >= stored) {
            diff = current - stored;
        } else {
            diff = (0xffff - stored) + current;
        }
        
        evLatency(EC_EVENT, EV_DPUMP, 0, "DATAPUMP", evprintf("MOVED %d diff %d sto %d cur %d %.3f msec",
            moved, diff, stored, current, (timer_us() - last_run_us)/1e3));

        if (diff > (nrx_bufs-1)) {
		    dpump.resets++;
		    
		    // dump on excessive latency between runs
		    #ifdef EV_MEAS_DPUMP_LATENCY
                //if (ev_dump /*&& dpump.resets > 1*/) {
                u4_t last = timer_us() - last_run_us;
                if ((ev_dump || bg) && last_run_us != 0 && last >= 40000) {
                    evLatency(EC_EVENT, EV_DPUMP, 0, "DATAPUMP", evprintf("latency %.3f msec", last/1e3));
                    evLatency(EC_DUMP, EV_DPUMP, ev_dump, "DATAPUMP", evprintf("DUMP in %.3f sec", ev_dump/1000.0));
                }
            #endif
            
            #if 0
                #ifndef USE_VALGRIND
                    lprintf("DATAPUMP RESET #%d %5d %5d %5d %.3f msec\n",
                        dpump.resets, diff, stored, current, (timer_us() - last_run_us)/1e3);
                #endif
            #endif
            
            //real_printf("."); fflush(stdout);
            
            spi_set(CmdSetRXNsamps, nrx_samps_wb);
            diff = 0;
        } else {
            dpump.hist[diff]++;
            #if 0
                if (ev_dump && p1 && p2 && dpump.hist[p1] > p2) {
                    printf("DATAPUMP DUMP %d %d %d\n", diff, stored, current);
                    evLatency(EC_DUMP, EV_DPUMP, ev_dump, ">diff",
                        evprintf("MOVED %d, diff %d sto %d cur %d, DUMP", moved, diff, stored, current));
                }
            #endif
        }
        
        last_run_us = timer_us();
        
        if (!itask_run) {
            spi_set(CmdSetRXNsamps, 0);
            ctrl_clr_set(CTRL_SND_INTR, 0);
        }
    } while (diff > 1);
    evLatency(EC_EVENT, EV_DPUMP, 0, "DATAPUMP", evprintf("MOVED %d", moved));

}

static void data_pump(void *param)
{
	evDP(EC_EVENT, EV_DPUMP, -1, "dpump_init", evprintf("INIT: SPI CTRL_SND_INTR %d",
		GPIO_READ_BIT(SND_INTR)));

	while (1) {

		evDP(EC_EVENT, EV_DPUMP, -1, "data_pump", evprintf("SLEEPING: SPI CTRL_SND_INTR %d",
			GPIO_READ_BIT(SND_INTR)));

		//#define MEAS_DATA_PUMP
		#ifdef MEAS_DATA_PUMP
		    u4_t quanta = FROM_VOID_PARAM(TaskSleepReason("wait for interrupt"));
            static u4_t last, cps, max_quanta, sum_quanta;
            u4_t now = timer_sec();
            if (last != now) {
                for (; last < now; last++) {
                    if (last < (now-1))
                        real_printf("d- ");
                    else
                        real_printf("d%d|%d/%d ", cps, sum_quanta/(cps? cps:1), max_quanta);
                    fflush(stdout);
                }
                max_quanta = sum_quanta = 0;
                cps = 0;
            } else {
                if (quanta > max_quanta) max_quanta = quanta;
                sum_quanta += quanta;
                cps++;
            }
        #else
		    TaskSleepReason("wait for interrupt");
        #endif

		evDP(EC_EVENT, EV_DPUMP, -1, "data_pump", evprintf("WAKEUP: SPI CTRL_SND_INTR %d",
			GPIO_READ_BIT(SND_INTR)));
		TaskStat(TSTAT_INCR|TSTAT_ZERO, 0, "dp");

		snd_service();
		
		for (int ch=0; ch < rx_chans; ch++) {
			rx_chan_t *rxc = &rx_channels[ch];
			if (!rxc->chan_enabled) continue;
			if (rxc->wb_task) {
                TaskWakeup(rxc->wb_task);
			}
            conn_t *c = rxc->conn;
            if (c != NULL) {
                assert(c->type == STREAM_SOUND);
                if (c->task) {
                    TaskWakeup(c->task);
                }
            }
		}
	}
}

void data_pump_start_stop()
{
#ifdef USE_SDR
	bool no_users = true;
	for (int i = 0; i < rx_chans; i++) {
        rx_chan_t *rx = &rx_channels[i];
		if (rx->chan_enabled) {
			no_users = false;
			break;
		}
	}
	
	// stop the data pump when the last user leaves
	if (itask_run && no_users) {
		itask_run = false;
		spi_set(CmdSetRXNsamps, 0);
		ctrl_clr_set(CTRL_SND_INTR, 0);
		//real_printf("#### STOP dpump\n");
		last_run_us = 0;
	}

	// start the data pump when the first user arrives
	if (!itask_run && !no_users) {
		itask_run = true;
		
	    for (int i = 0; i < rx_chans; i++) {
            rx_dpump_t *rx = &rx_dpump[i];
            rx->wr_pos = rx->rd_pos = 0;
	    }
		
		ctrl_clr_set(CTRL_SND_INTR, 0);
		spi_set(CmdSetRXNsamps, nrx_samps_wb);
		//real_printf("#### START dpump %d\n", nrx_samps_wb);
		last_run_us = 0;
	}
	
	have_snd_users = !no_users;
#endif
}

void data_pump_init()
{
    #ifdef SND_SEQ_CHECK
        rx_xfer_size = sizeof(rx_data_t::rx_header_t) + (sizeof(iq3_t) * nrx_samps * rx_wb_buf_chans);
    #else
        rx_xfer_size = sizeof(iq3_t) * nrx_samps * rx_wb_buf_chans;
    #endif
	rxd = (rx_data_t *) &SPI_SHMEM->dpump_miso.word[0];
	rxt = (rx_trailer_t *) ((char *) rxd + rx_xfer_size);
	rx_xfer_size += sizeof(rx_trailer_t);
	//printf("rx_trailer_t=%d iq3_t=%d rx_xfer_size=%d rxd=%p rxt=%p nrx_samps_loop=%d nrx_samps_rem=%d\n",
	//    sizeof(rx_trailer_t), sizeof(iq3_t), rx_xfer_size, rxd, rxt, nrx_samps_loop, nrx_samps_rem);

	// verify that audio samples will fit in hardware buffers
	#define WORDS_PER_SAMP 3	// 2 * 24b IQ = 3 * 16b
	
	// does a single nrx_samps transfer fit in the SPI buf?
	check(rx_xfer_size <= SPIBUF_BMAX);	// in bytes
	
    check(nrx_samps < FASTFIR_OUTBUF_SIZE);     // see rx_dpump_t.in_samps[][]
    check(nrx_samps_wb < MAX_WB_SAMPS);         // see rx_dpump_t.wb_samps[][]
	
	rescale = MPOW(2, -RXOUT_SCALE + CUTESDR_SCALE) * (VAL_USE_RX_CICF? MPOW(10, cicf_gain_dB[fw_sel]/20.0) : 1);
	//printf("data pump: fw_sel=%d RXOUT_SCALE=%d CUTESDR_SCALE=%d cicf_gain_dB=%.1f rescale=%.6g VAL_USE_RX_CICF=%d DC_offset_I=%f DC_offset_Q=%f\n", fw_sel, RXOUT_SCALE, CUTESDR_SCALE, cicf_gain_dB[fw_sel], rescale, VAL_USE_RX_CICF, DC_offset_I, DC_offset_Q);

	CreateTaskF(data_pump, 0, DATAPUMP_PRIORITY, CTF_POLL_INTR);
}

void data_pump_startup()
{
	// don't start data pump until first connection so GPS search can run at full speed on startup
	static bool data_pump_started;
	if (!data_pump_started) {
		data_pump_init();
		data_pump_started = true;
	}
}

void data_pump_dump()
{
    rx_dpump_t *rx = &rx_dpump[RX_CHAN0];
    int n_dpbuf = kiwi.isWB? N_WB_DPBUF : N_DPBUF;
    
    u4_t diff = (rx->wr_pos >= rx->rd_pos)? (rx->wr_pos - rx->rd_pos) : (n_dpbuf - rx->rd_pos + rx->wr_pos);
    lprintf("data_pump RX_CHAN0 isWB=%d n_dpbuf=%d rd|wr_pos=%d|%d|%d \n",
        kiwi.isWB, n_dpbuf, rx->rd_pos, rx->wr_pos, diff);
}

#endif

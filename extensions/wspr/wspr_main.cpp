/*
 This file is part of program wsprd, a detector/demodulator/decoder
 for the Weak Signal Propagation Reporter (WSPR) mode.
 
 Copyright 2001-2015, Joe Taylor, K1JT
 
 Much of the present code is based on work by Steven Franke, K9AN,
 which in turn was based on earlier work by K1JT.
 
 Copyright 2014-2015, Steven Franke, K9AN
 
 License: GNU GPL v3
 
 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "types.h"
#include "config.h"
#include "mem.h"
#include "net.h"
#include "rx.h"
#include "web.h"
#include "non_block.h"
#include "wspr.h"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <math.h>
#include <strings.h>

#define WSPR_DATA		0

// wspr_status
#define	NONE		0
#define	IDLE		1
#define	SYNC		2
#define	RUNNING		3
#define	DECODING	4

#ifdef WSPR_SHMEM_DISABLE
    static wspr_shmem_t wspr_shmem;
    wspr_shmem_t *wspr_shmem_p = &wspr_shmem;
#endif

wspr_conf_t wspr_c;

// assigned constants
int nffts, nbins_411, hbins_205;

// computed constants
static float window[NFFT];

const char *status_str[] = { "none", "idle", "sync", "running", "decoding" };

static void wspr_status(wspr_t *w, int status, int resume)
{
    int rx_chan = w->rx_chan;
	wspr_printf("wspr_status: set status %d-%s\n", status, status_str[status]);

    // Send server time to client since that's what sync is based on.
    // Send on each status change to eliminate effects of browser host clock drift.
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    u4_t msec = ts.tv_nsec/1000000;
	ext_send_msg(w->rx_chan, WSPR_DEBUG_MSG, "EXT WSPR_STATUS=%d WSPR_TIME_MSEC=%d=%d",
	    status, ts.tv_sec, msec);

	w->status = status;
	if (resume != NONE) {
		wspr_printf("wspr_status: will resume to %d-%s\n", resume, status_str[resume]);
		w->status_resume = resume;
	}
}

// compute FFTs incrementally during data capture rather than all at once at the beginning of the decode
void WSPR_FFT(void *param)
{
    int rx_chan = (int) FROM_VOID_PARAM(param);
    wspr_t *w = &WSPR_SHMEM->wspr[rx_chan];
    wspr_buf_t *wb = w->buf;
    assert(rx_chan == w->rx_chan);
	int i,j,k;
	int pgrp=0, plast=0;
	
	while (1) {
		
		//wspr_printf("WSPR_FFTtask sleep..\n");
		WSPR_CHECK_ALT(
		    { int cck = (int) FROM_VOID_PARAM(TaskSleep()); assert(rx_chan == cck) },
		    TaskSleep();
		)
		w->fft_runs++;
		if (w->fft_wakeups != w->fft_runs) {
		    //printf("WSPR-%02d: wakeups=%d runs=%d\n", rx_chan, w->fft_wakeups, w->fft_runs);
		    w->fft_runs = w->fft_wakeups;
		}
		//wspr_printf("WSPR_FFTtask ..wakeup\n");
		
		int grp = w->FFTtask_group;
		int first = grp*FPG, last = first+FPG;
	
		if (grp == 0) {
		    wspr_aprintf("FFT pp=%d grp=%d (%d-%d)\n", w->fft_ping_pong, grp, first, last);
		    #if 0
                if (w->not_launched) {
                    printf("WSPR-%02d: decoder missed launch! pgrp=%d/%d plast=%d/%d ============================================\n",
                        rx_chan, pgrp, GROUPS, plast, nffts);
                }
            #endif
		    if (!w->fft_init)
		        w->fft_init = 1;
		    else
		        w->not_launched = 1;
		}
		pgrp = grp; plast = last;
	
		// Do ffts over 2 symbols, stepped by half symbols
	    wspr_array_dim(w->fft_ping_pong, N_PING_PONG);
		WSPR_CPX_t *id = wb->i_data[w->fft_ping_pong], *qd = wb->q_data[w->fft_ping_pong];
	
		//float maxiq = 1e-66, maxpwr = 1e-66;
		//int maxi=0;
		memset(wb->savg, 0, sizeof(wb->savg));
		
		// FIXME: A large noise burst can washout the w->pwr_sampavg which prevents a proper
		// peak list being created in the decoder. An individual signal can be decoded fine
		// in the presence of the burst. But if there is no peak in the list the decoding
		// process is never started! We've seen this problem fairly frequently.
		
		for (i=first; i<last; i++) {
			for (j=0; j<NFFT; j++) {
				k = i*HSPS+j;
				wspr_array_dim(k, TPOINTS);
				w->fftin[j][0] = id[k] * window[j];
				w->fftin[j][1] = qd[k] * window[j];
				//if (i==0) wspr_printf("IN %d %fi %fq\n", j, w->fftin[j][0], w->fftin[j][1]);
			}
			
			//u4_t start = timer_us();
			WSPR_YIELD;
			WSPR_FFTW_EXECUTE(w->fftplan);
			WSPR_YIELD;
			//if (i==0) wspr_printf("512 FFT %.1f us\n", (float)(timer_us()-start));
			
			// NFFT = SPS*2
			// unwrap fftout:
			//      |1---> SPS
			// 2--->|      SPS

			for (j=0; j<NFFT; j++) {
				k = j+SPS;
				if (k > (NFFT-1))
					k -= NFFT;
				float ii = w->fftout[k][0];
				float qq = w->fftout[k][1];
				float pwr = ii*ii + qq*qq;
				//if (i==0) wspr_printf("OUT %d %fi %fq\n", j, ii, qq);
				//if (ii > maxiq) { maxiq = ii; }
				//if (pwr > maxpwr) { maxpwr = pwr; maxi = k; }
				wspr_array_dim(w->fft_ping_pong, N_PING_PONG);
				wspr_array_dim(j, NFFT);
				wspr_array_dim(i, FPG*GROUPS);
				wb->pwr_samp[w->fft_ping_pong][j][i] = pwr;
				wb->pwr_sampavg[w->fft_ping_pong][j] += pwr;
				wb->savg[j] += pwr;
			}
			WSPR_YIELD;
		}
	
		// send spectrum data to client
		float dB, max_dB=-99, min_dB=99;
		renormalize(w, wb->savg, wb->smspec[WSPR_RENORM_FFT], wb->tmpsort[WSPR_RENORM_FFT]);
		
		for (j=0; j<nbins_411; j++) {
			wb->smspec[WSPR_RENORM_FFT][j] = dB = 10*log10(wb->smspec[WSPR_RENORM_FFT][j]) - w->snr_scaling_factor;
			if (dB > max_dB) max_dB = dB;
			if (dB < min_dB) min_dB = dB;
		}
	
		float y, range_dB, pix_per_dB;
		max_dB = 6;
		min_dB = -33;
		range_dB = max_dB - min_dB;
		pix_per_dB = 255.0 / range_dB;
		
		for (j=0; j<nbins_411; j++) {
			#if 0
				wb->smspec[WSPR_RENORM_FFT][j] = -33 + ((j/30) * 3);		// test colormap
			#endif
			y = pix_per_dB * (wb->smspec[WSPR_RENORM_FFT][j] - min_dB);
			if (y > 255.0) y = 255.0;
			if (y < 0) y = 0;
			wb->ws[j+1] = (u1_t) y;
		}
		wb->ws[0] = ((grp == 0) && w->tsync)? 1:0;
	
		if (ext_send_msg_data(w->rx_chan, WSPR_DEBUG_MSG, WSPR_DATA, wb->ws, nbins_411+1) < 0) {
			w->send_error = true;
		}
	
		//wspr_printf("WSPR_FFTtask group %d:%d %d-%d(%d) %f %f(%d)\n", w->fft_ping_pong, grp, first, last, nffts, maxiq, maxpwr, maxi);
		//wspr_printf("  %d-%d(%d)\r", first, last, nffts); fflush(stdout);
		
		if (last < nffts) continue;
	
		w->decode_ping_pong = w->fft_ping_pong;
		wspr_aprintf("FFT ---DECODE -> %d\n", w->decode_ping_pong);
		wspr_printf("\n");
		w->not_launched = 0;
		TaskWakeup(w->WSPR_DecodeTask_id, TWF_CHECK_WAKING, TO_VOID_PARAM(w->rx_chan));
    }
}

void wspr_send_peaks(wspr_t *w, int start, int stop)
{
    char peaks_s[MAX_NPK*(6+1 + LEN_CALL) + 16];
    char *s = peaks_s;
    *s = '\0';
    int j, n;
    
    // update peaks that have changed
    for (j = start; j < stop; j++)
        w->pk_save[j] = w->pk_freq[j];
    
    // complete peak list sent on each update
    for (j = 0; j < w->npk; j++) {
    	int bin_flags = w->pk_save[j].bin0 | w->pk_save[j].flags;
    	n = sprintf(s, "%d:%s:", bin_flags, w->pk_save[j].snr_call); s += n;
    }
	ext_send_msg_encoded(w->rx_chan, WSPR_DEBUG_MSG, "EXT", "WSPR_PEAKS", "%s", peaks_s);
}

void wspr_send_decode(wspr_t *w, int seq)
{
    u4_t printf_type = wspr_c.syslog? (PRINTF_REG | PRINTF_LOG) : PRINTF_REG;
    //bool log_decodes = (wspr_c.syslog || w->autorun);
    bool log_decodes = true;    // always log, it's just PRINTF_LOG that's controlled by the admin page option
    double watts, factor;
    char *W_s;
    const char *units;
    wspr_decode_t *d = &w->deco[seq];

    watts = pow(10.0, (d->dBm - 30)/10.0);
    if (watts >= 1.0) {
        factor = 1.0;
        units = "W";
    } else
    if (watts >= 1e-3) {
        factor = 1e3;
        units = "mW";
    } else
    if (watts >= 1e-6) {
        factor = 1e6;
        units = "uW";
    } else
    if (watts >= 1e-9) {
        factor = 1e9;
        units = "nW";
    } else {
        factor = 1e12;
        units = "pW";
    }
    
    watts *= factor;
    if (watts < 10.0)
        asprintf(&W_s, "%.1f %s", watts, units);
    else
        asprintf(&W_s, "%.0f %s", watts, units);
    
    if (log_decodes && (d->r_valid == 1 || d->r_valid == 2 || d->r_valid == 3)) {
        static int arct;
        if ((arct & 7) == 0) {
            //                   1234 123 1234 123456789 12  123456 1234 12345  123
            //                   0920 -26  0.4  7.040121  0  AE7YQ  DM41 10770   37 (5.0 W)
            rcfprintf(w->rx_chan, printf_type, "WSPR DECODE:  UTC  dB   dT      Freq dF  Call   Grid    km  dBm\n");
        }
        arct++;
    }
    
    // 			Call   Grid    km  dBm
    // TYPE1	c6cccc g4gg kkkkk  ppp (s)
    // TYPE2	...                ppp (s)		; no hash yet
    // TYPE2	ppp/c6cccc         ppp (s)		; 1-3 char prefix
    // TYPE2	c6cccc/ss          ppp (s)		; 1-2 char suffix
    // TYPE3	...  g6gggg kkkkk  ppp (s)		; no hash yet
    // TYPE3	ppp/c6cccc g6gggg kkkkk ppp (s)	; worst case, just let the fields float

    if (d->r_valid == 1) { // TYPE1
        ext_send_msg_encoded(w->rx_chan, WSPR_DEBUG_MSG, "EXT", "WSPR_DECODED",
            "%02d%02d %3.0f %4.1f %9.6f %2d  "
            "<a href='https://www.qrz.com/lookup/%s' target='_blank'>%-6s</a> "
            "<a href='http://www.levinecentral.com/ham/grid_square.php?Grid=%s' target='_blank'>%s</a> "
            "%5d  %3d (%s)",
            d->hour, d->min, d->snr, d->dt_print, d->freq_print, (int) d->drift1,
            d->call, d->call, d->grid, d->grid,
            (int) grid_to_distance_km(d->grid), d->dBm, W_s);
        if (log_decodes)
            rcfprintf(w->rx_chan, printf_type, "WSPR DECODE: %02d%02d %3.0f %4.1f %9.6f %2d  %-6s %s %5d  %3d (%s)\n",
                d->hour, d->min, d->snr, d->dt_print, d->freq_print, (int) d->drift1,
                d->call, d->grid, (int) grid_to_distance_km(d->grid), d->dBm, W_s);
    } else
    
    if (d->r_valid == 2) {	// TYPE2
        if (strcmp(d->call, "...") == 0) {
            ext_send_msg_encoded(w->rx_chan, WSPR_DEBUG_MSG, "EXT", "WSPR_DECODED",
                "%02d%02d %3.0f %4.1f %9.6f %2d  ...                %3d (%s)",
                d->hour, d->min, d->snr, d->dt_print, d->freq_print, (int) d->drift1, d->dBm, W_s);
            if (log_decodes)
                rcfprintf(w->rx_chan, printf_type, "WSPR DECODE: %02d%02d %3.0f %4.1f %9.6f %2d  ...                %3d (%s)\n",
                    d->hour, d->min, d->snr, d->dt_print, d->freq_print, (int) d->drift1, d->dBm, W_s);
        } else {
            ext_send_msg_encoded(w->rx_chan, WSPR_DEBUG_MSG, "EXT", "WSPR_DECODED",
                "%02d%02d %3.0f %4.1f %9.6f %2d  "
                "<a href='https://www.qrz.com/lookup/%s' target='_blank'>%-17s</a>"
                "  %3d (%s)",
               //kkkkk--ppp
                d->hour, d->min, d->snr, d->dt_print, d->freq_print, (int) d->drift1,
                d->call, d->call, d->dBm, W_s);
            if (log_decodes)
                rcfprintf(w->rx_chan, printf_type, "WSPR DECODE: %02d%02d %3.0f %4.1f %9.6f %2d  %-17s  %3d (%s)\n",
                    //kkkkk--ppp
                    d->hour, d->min, d->snr, d->dt_print, d->freq_print, (int) d->drift1,
                    d->call, d->dBm, W_s);
        }
    } else
    
    if (d->r_valid == 3) {	// TYPE3
        if (strcmp(d->call, "...") == 0) {
            ext_send_msg_encoded(w->rx_chan, WSPR_DEBUG_MSG, "EXT", "WSPR_DECODED",
                "%02d%02d %3.0f %4.1f %9.6f %2d  "
                "...  "
                "<a href='http://www.levinecentral.com/ham/grid_square.php?Grid=%s' target='_blank'>%6s</a> "
                "%5d  %3d (%s)",
                d->hour, d->min, d->snr, d->dt_print, d->freq_print, (int) d->drift1,
                d->grid, d->grid, (int) grid_to_distance_km(d->grid), d->dBm, W_s);
            if (log_decodes)
                rcfprintf(w->rx_chan, printf_type, "WSPR DECODE: %02d%02d %3.0f %4.1f %9.6f %2d  ...  %6s %5d  %3d (%s)\n",
                    d->hour, d->min, d->snr, d->dt_print, d->freq_print, (int) d->drift1,
                    d->grid, (int) grid_to_distance_km(d->grid), d->dBm, W_s);
        } else {
            ext_send_msg_encoded(w->rx_chan, WSPR_DEBUG_MSG, "EXT", "WSPR_DECODED",
                "%02d%02d %3.0f %4.1f %9.6f %2d  "
                "<a href='https://www.qrz.com/lookup/%s' target='_blank'>%s</a> "
                "<a href='http://www.levinecentral.com/ham/grid_square.php?Grid=%s' target='_blank'>%s</a> "
                "%d %d (%s)",
                d->hour, d->min, d->snr, d->dt_print, d->freq_print, (int) d->drift1,
                d->call, d->call, d->grid, d->grid,
                (int) grid_to_distance_km(d->grid), d->dBm, W_s);
            if (log_decodes)
                rcfprintf(w->rx_chan, printf_type, "WSPR DECODE: %02d%02d %3.0f %4.1f %9.6f %2d  %s %s %d %d (%s)\n",
                    d->hour, d->min, d->snr, d->dt_print, d->freq_print, (int) d->drift1,
                    d->call, d->grid, (int) grid_to_distance_km(d->grid), d->dBm, W_s);
        }
    }
    
    kiwi_ifree(W_s);
}

void WSPR_Deco(void *param)
{
    int rx_chan = (int) FROM_VOID_PARAM(param);
    wspr_t *w = &WSPR_SHMEM->wspr[rx_chan];
    wspr_buf_t *wb = w->buf;
    assert(rx_chan == w->rx_chan);
    u4_t start=0;

	// fixme: revisit in light of new priority task system
	// this was intended to allow a minimum percentage of runtime
	TaskMinRun(4000);

	while (1) {
	
		wspr_printf("WSPR_DecodeTask sleep..\n");
		if (start) wspr_dprintf("WSPR_DecodeTask decoded %2d %s %5.1f sec (%s decoder)\n",
		    w->uniques, w->abort_decode? "LIMIT":"TOTAL", (float)(timer_ms()-start)/1000.0,
		    w->stack_decoder? "Jelinek":"Fano");
		WSPR_CHECK_ALT(
		    { int cck = (int) FROM_VOID_PARAM(TaskSleep()); assert(rx_chan == cck) },
		    TaskSleep();
		)
	
		w->abort_decode = false;
		start = timer_ms();
		wspr_aprintf("DECO-P wakeup\n");
	
		wspr_status(w, DECODING, NONE);
        #ifdef WSPR_SHMEM_DISABLE
		    wspr_decode(rx_chan);
		#else
		    w->send_peaks_seq_parent = w->send_peaks_seq = 0;
		    w->send_decode_seq_parent = w->send_decode_seq = 0;
            wspr_aprintf("DECO-P INVOKE\n");
            shmem_ipc_invoke(SIG_IPC_WSPR + w->rx_chan, w->rx_chan, NO_WAIT);    // invoke wspr_decode()

            int done;
		    do {
                done = shmem_ipc_poll(SIG_IPC_WSPR + w->rx_chan, POLL_MSEC(250), w->rx_chan);
		        // Use "if" instead of "while" here and a relatively slow polling interval so that peak marker
		        // changes have some persistence.
                if (w->send_peaks_seq_parent < w->send_peaks_seq) {     // make sure each one is processed
                    wspr_send_peaks(w, wb->send_peaks_q[w->send_peaks_seq_parent].start, wb->send_peaks_q[w->send_peaks_seq_parent].stop);
                    w->send_peaks_seq_parent++;
                }
                while (w->send_decode_seq_parent < w->send_decode_seq) {    // make sure each one is processed
                    wspr_send_decode(w, w->send_decode_seq_parent);
                    w->send_decode_seq_parent++;
                }
                if (w->send_decode_seq_parent != w->send_decode_seq)
                    wspr_dprintf("send_decode_seq_parent(%d) != send_decode_seq(%d)\n", w->send_decode_seq_parent, w->send_decode_seq);
            } while (!done);
            
            // process any remaining peak changes
            while (w->send_peaks_seq_parent < w->send_peaks_seq) {     // make sure each one is processed
                wspr_send_peaks(w, wb->send_peaks_q[w->send_peaks_seq_parent].start, wb->send_peaks_q[w->send_peaks_seq_parent].stop);
                w->send_peaks_seq_parent++;
            }
            
            wspr_aprintf("DECO-P DONE\n");
		#endif
	
		if (w->abort_decode)
			wspr_aprintf("DECO-P decoder aborted\n");
	
        // upload spots at the end of the decoding when there is less load on wsprnet.org
        //printf("UPLOAD %d spots RX%d %.6f START\n", w->uniques, w->rx_chan, w->arun_cf_MHz);
        for (int i = 0; i < w->uniques; i++) {
            wspr_decode_t *dp = &w->deco[i];
            if (w->autorun) {
                int year, month, day; utc_year_month_day(&year, &month, &day);
                char *cmd;
                asprintf(&cmd, "curl 'http://wsprnet.org/post?function=wspr&rcall=%s&rgrid=%s&rqrg=%.6f&date=%02d%02d%02d&time=%02d%02d&sig=%.0f&dt=%.1f&drift=%d&tqrg=%.6f&tcall=%s&tgrid=%s&dbm=%s&version=1.3+Kiwi' >/dev/null 2>&1",
                    wspr_c.rcall, wspr_c.rgrid, w->arun_cf_MHz, year%100, month, day, dp->hour, dp->min, dp->snr, dp->dt_print, (int) dp->drift1, dp->freq_print, dp->call, dp->grid, dp->pwr);
                //printf("UPLOAD RX%d %d/%d %s\n", w->rx_chan, i+1, w->uniques, cmd);
                non_blocking_cmd_system_child("kiwi.wsprnet.org", cmd, NO_WAIT);
                kiwi_ifree(cmd);
                w->arun_decoded++;
            } else {
                ext_send_msg_encoded(w->rx_chan, WSPR_DEBUG_MSG, "EXT", "WSPR_UPLOAD",
                    "%02d%02d %3.0f %4.1f %9.6f %2d %s",
                    dp->hour, dp->min, dp->snr, dp->dt_print, dp->freq_print, (int) dp->drift1, dp->c_l_p);
            }
            wspr_d1printf("UPLOAD U%d/%d %s"
                "%02d%02d %3.0f %4.1f %9.6f %2d %s" "\n", i, w->uniques, w->autorun? "autorun ":"",
                dp->hour, dp->min, dp->snr, dp->dt_print, dp->freq_print, (int) dp->drift1, dp->c_l_p);
            TaskSleepMsec(1000);
        }
        //printf("UPLOAD %d spots RX%d %.6f DONE\n", w->uniques, w->rx_chan, w->arun_cf_MHz);

		wspr_status(w, w->status_resume, NONE);
		
		if (w->autorun) {
		    // send wsprstat on startup and every 6 minutes thereafter
		    u4_t now = timer_sec();
		    if (now - w->arun_last_status_sent > MINUTES_TO_SEC(6)) {
		        w->arun_last_status_sent = now;
		        
		        // in case wspr_c.rgrid has changed 
		        kiwi_ifree(w->arun_stat_cmd);
		        #define WSPR_STAT "curl 'http://wsprnet.org/post?function=wsprstat&rcall=%s&rgrid=%s&rqrg=%.6f&tpct=0&tqrg=%.6f&dbm=0&version=1.3+Kiwi' >/dev/null 2>&1"
                asprintf(&w->arun_stat_cmd, WSPR_STAT, wspr_c.rcall, wspr_c.rgrid, w->arun_cf_MHz, w->arun_cf_MHz);
                //printf("AUTORUN %s\n", w->arun_stat_cmd);
                non_blocking_cmd_system_child("kiwi.wsprnet.org", w->arun_stat_cmd, NO_WAIT);
		    }
		    if (w->arun_decoded > w->arun_last_decoded) {
		        input_msg_internal(w->arun_csnd, (char *) "SET geoloc=%d%%20decoded", w->arun_decoded);
		        w->arun_last_decoded = w->arun_decoded;
            }
		}
    }
}

static double frate, fdecimate;
static int int_decimate;

void wspr_data(int rx_chan, int ch, int nsamps, TYPECPX *samps)
{
    wspr_t *w = &WSPR_SHMEM->wspr[rx_chan];
    assert(rx_chan == w->rx_chan);
    WSPR_CHECK(assert(w->magic1 == 0xcafe); assert(w->magic2 == 0xbabe);)
	int i;

    // FIXME: Someday it's possible samp rate will be different between rx_chans
    // if they have different bandwidths. Not possible with current architecture
    // of data pump.
    //frate = ext_update_get_sample_rateHz(rx_chan);
    //fdecimate = frate / FSRATE;
	
	//wspr_printf("WD%d didx %d send_error %d reset %d\n", w->capture, w->didx, w->send_error, w->reset);
	if (w->send_error) {
		wspr_printf("STOP send_error %d\n", w->send_error);
		ext_unregister_receive_iq_samps(w->rx_chan);
		w->send_error = FALSE;
		w->capture = FALSE;
		w->reset = TRUE;
	}

	if (w->reset) {
		w->ping_pong = w->decim = w->didx = w->group = 0;
		w->fi = 0;
		w->tsync = FALSE;
		w->status_resume = IDLE;	// decoder finishes after we stop capturing
		w->reset = FALSE;
	}

	if (!w->capture) {
		return;
	}
	
    int min, sec; utc_hour_min_sec(NULL, &min, &sec);
	if (sec != w->last_sec) {
		if (min&1 && sec == 40)
			w->abort_decode = true;
		
		w->last_sec = sec;
	}
	
    if (w->tsync == FALSE) {		// sync to even minute boundary
        if (!(min&1) && (sec == 0)) {
            w->ping_pong ^= 1;
            wspr_aprintf("DATA SYNC ping_pong %d, %s\n", w->ping_pong, utc_ctime_static());
            w->decim = w->didx = w->group = 0;
            w->fi = 0;
            if (w->status != DECODING)
                wspr_status(w, RUNNING, RUNNING);
            w->tsync = TRUE;
        }
    }
	
	if (w->didx == 0) {
	    wspr_array_dim(w->ping_pong, N_PING_PONG);
    	memset(&w->buf->pwr_sampavg[w->ping_pong][0], 0, sizeof(w->buf->pwr_sampavg[0]));
	}
	
	if (w->group == 0) w->utc[w->ping_pong] = utc_time();
	
    wspr_array_dim(w->ping_pong, N_PING_PONG);
	WSPR_CPX_t *idat = w->buf->i_data[w->ping_pong], *qdat = w->buf->q_data[w->ping_pong];
	//double scale = 1000.0;
	double scale = 1.0;
	
	// NO-NO-NO
	// Can't do fractional decimation via "drop-sampling" like this because of
	// the artifacts generated. Must do fractional "resampling" involving
	// interpolation, decimation and FIR filtering (like the new audio re-sampler).
	//
	// Maybe we were thinking of the old "drop-sampling" *interpolator* used by audio.
	// Although even that relies on the passband FIR doing the necessary pre-interpolation
	// filtering. And a post-interpolation LPF is also required.
	//
	// Much more efficient to raise the audio bandwith to 12 kHz and be able to use
	// *integer* decimation (12k / 32 = 375 Hz) here like we originally did when
	// the WSPR extension was first written (before the audio b/w was changed to accomodate
	// the S4285 extension).
	
	#define FRACTIONAL_DECIMATION 0
	#if FRACTIONAL_DECIMATION

        for (i = trunc(w->fi); i < nsamps;) {
    
            if (w->didx >= TPOINTS) {
                return;
            }
    
            if (w->group == 3) w->tsync = FALSE;	// arm re-sync
            
            WSPR_CPX_t re = (WSPR_CPX_t) samps[i].re/scale;
            WSPR_CPX_t im = (WSPR_CPX_t) samps[i].im/scale;
            idat[w->didx] = re;
            qdat[w->didx] = im;
    
            if ((w->didx % NFFT) == (NFFT-1)) {
                //wspr_printf("SAMPLER pp=%d grp=%d frate=%.1f fdecimate=%.1f rx_chan=%d\n",
                //    w->ping_pong, w->group, frate, fdecimate, rx_chan);
                w->fft_ping_pong = w->ping_pong;
                w->FFTtask_group = w->group-1;
                if (w->group)	// skip first to pipeline
                    TaskWakeup(w->WSPR_FFTtask_id, TWF_CHECK_WAKING, w->rx_chan);
                w->group++;
            }
            w->didx++;
    
            w->fi += fdecimate;
            i = trunc(w->fi);
        }
        w->fi -= nsamps;	// keep bounded

	#else

		for (i=0; i<nsamps; i++) {
	
			// decimate
			if (w->decim++ < (int_decimate-1))
				continue;
			w->decim = 0;
			
			if (w->didx >= TPOINTS)
				return;

    		if (w->group == 3) w->tsync = FALSE;	// arm re-sync
			
			WSPR_CPX_t re = (WSPR_CPX_t) samps[i].re/scale;
			WSPR_CPX_t im = (WSPR_CPX_t) samps[i].im/scale;
			idat[w->didx] = re;
			qdat[w->didx] = im;
	
			if ((w->didx % NFFT) == (NFFT-1)) {
				w->fft_ping_pong = w->ping_pong;
				w->FFTtask_group = w->group-1;
				if (w->group) {     // skip first to pipeline
				    w->fft_wakeups++;
				    TaskWakeup(w->WSPR_FFTtask_id, TWF_CHECK_WAKING, TO_VOID_PARAM(w->rx_chan));
				}
				w->group++;
			}
			w->didx++;
		}

    #endif
}

void wspr_close(int rx_chan)
{
    wspr_t *w = &WSPR_SHMEM->wspr[rx_chan];
    assert(rx_chan == w->rx_chan);
	//wspr_printf("wspr_close\n");
    ext_unregister_receive_iq_samps(w->rx_chan);

	if (w->create_tasks) {
		//wspr_printf("wspr_close TaskRemove FFT%d deco%d\n", w->WSPR_FFTtask_id, w->WSPR_DecodeTask_id);
		TaskRemove(w->WSPR_FFTtask_id);
		TaskRemove(w->WSPR_DecodeTask_id);
		w->create_tasks = false;
	}
}

bool wspr_msgs(char *msg, int rx_chan)
{
    wspr_t *w = &WSPR_SHMEM->wspr[rx_chan];
    assert(rx_chan == w->rx_chan);
	int i, n;
	
	wspr_printf("wspr_msgs <%s>\n", msg);
	
	if (strcmp(msg, "SET ext_server_init") == 0) {
		ext_send_msg(w->rx_chan, WSPR_DEBUG_MSG, "EXT nbins=%d ready", nbins_411);
		wspr_status(w, IDLE, IDLE);
		return true;
	}

	n = sscanf(msg, "SET BFO=%d", &w->bfo);
	if (n == 1) {
		wspr_printf("BFO %d --------------------------------------------------------------\n", w->bfo);
		return true;
	}
	
	n = sscanf(msg, "SET stack_decoder=%d", &i);
    if (n == 1) {
        w->stack_decoder = i;
        return true;
    }
    
	n = sscanf(msg, "SET debug=%d", &i);
    if (n == 1) {
        w->debug = i;
        return true;
    }
    
	float f;
	int d;
	n = sscanf(msg, "SET dialfreq=%f cf_offset=%d", &f, &d);
	if (n == 2) {
		w->dialfreq_MHz = f / kHz;
		w->cf_offset = (float) d;
		wspr_printf("dialfreq_MHz %.6f cf_offset %.0f -------------------------------------------\n", w->dialfreq_MHz, w->cf_offset);
		return true;
	}
	
	if (strcmp(msg, "SET autorun") == 0) {
	    w->autorun = true;
	    return true;
	}

	n = sscanf(msg, "SET capture=%d", &w->capture);
	if (n == 1) {
		if (w->capture) {
			if (!w->create_tasks) {
				w->WSPR_FFTtask_id = CreateTaskF(WSPR_FFT, TO_VOID_PARAM(rx_chan), EXT_PRIORITY, CTF_RX_CHANNEL | (rx_chan & CTF_CHANNEL));
				w->WSPR_DecodeTask_id = CreateTaskF(WSPR_Deco, TO_VOID_PARAM(rx_chan), EXT_PRIORITY_LOW, CTF_RX_CHANNEL | (rx_chan & CTF_CHANNEL));
				w->create_tasks = true;
			}
			
			w->send_error = false;
			w->reset = TRUE;
			ext_register_receive_iq_samps(wspr_data, rx_chan);
			wspr_printf("CAPTURE --------------------------------------------------------------\n");

			wspr_status(w, SYNC, RUNNING);
		} else {
			w->abort_decode = true;
			wspr_close(w->rx_chan);
		}
		return true;
	}
	
	return false;
}

// order matches wspr_autorun_u in wspr.js
// only add new entries to the end so as not to disturb existing values stored in config
static double wspr_cfs[] = {
    137.5, 475.7, 1838.1, 3570.1, 3594.1, 5288.7, 5366.2, 7040.1, 10140.2, 14097.1, 18106.1, 21096.1, 24926.1, 28126.1,
    50294.5, 70092.5, 144490.5, 432301.5, 1296501.5, 6781.5, 13554.5
};

void wspr_reset(wspr_t *w)
{
    w->status_resume = IDLE;
    w->tsync = FALSE;
    w->capture = 0;
    w->last_sec = -1;
    w->abort_decode = false;
    w->send_error = false;
}

static internal_conn_t iconn[MAX_RX_CHANS];

void wspr_autorun(int instance, int band)
{
	#define WSPR_BFO 750
	#define WSPR_FILTER_BW 300
	double center_freq_kHz = wspr_cfs[band];
    double dial_freq_kHz = center_freq_kHz - WSPR_BFO/1e3;
    double if_freq_kHz = dial_freq_kHz - freq_offset;
	double cfo = roundf((center_freq_kHz - floorf(center_freq_kHz)) * 1e3);
	
	double max_freq = freq_offset + ui_srate/1e3;
	if (dial_freq_kHz < freq_offset || dial_freq_kHz > max_freq) {
	    lprintf("WSPR autorun: ERROR band_id=%d dial_freq_kHz %.2f is outside rx range %.2f - %.2f\n",
	        band, dial_freq_kHz, freq_offset, max_freq);
	    return;
	}

	if (internal_conn_setup(ICONN_WS_SND | ICONN_WS_EXT, &iconn[instance], instance, PORT_INTERNAL_WSPR,
        "usb", WSPR_BFO - WSPR_FILTER_BW/2, WSPR_BFO + WSPR_FILTER_BW/2, if_freq_kHz,
        "WSPR-autorun", "0%20decoded", "wspr") == false) return;

    conn_t *csnd = iconn[instance].csnd;
    int chan = csnd->rx_channel;
    wspr_t *w = &WSPR_SHMEM->wspr[chan];
    wspr_reset(w);
    w->arun_csnd = csnd;
    w->arun_cf_MHz = center_freq_kHz / 1e3;
    w->arun_decoded = 0;
    w->arun_last_decoded = -1;

	clprintf(csnd, "WSPR autorun: instance=%d band_id=%d off=%.2f if=%.2f df=%.2f cf=%.2f cfo=%.0f\n",
	    instance, band, freq_offset, if_freq_kHz, dial_freq_kHz, center_freq_kHz, cfo);
	
    conn_t *cext = iconn[instance].cext;
    input_msg_internal(cext, (char *) "SET autorun");
    input_msg_internal(cext, (char *) "SET BFO=%d", WSPR_BFO);
    input_msg_internal(cext, (char *) "SET capture=0");
    input_msg_internal(cext, (char *) "SET dialfreq=%.2f cf_offset=%.0f", dial_freq_kHz, cfo);
    input_msg_internal(cext, (char *) "SET capture=1");

    asprintf(&w->arun_stat_cmd, WSPR_STAT, wspr_c.rcall, wspr_c.rgrid, w->arun_cf_MHz, w->arun_cf_MHz);
    //printf("AUTORUN INIT %s\n", w->arun_stat_cmd);
    non_blocking_cmd_system_child("kiwi.wsprnet.org", w->arun_stat_cmd, NO_WAIT);
    w->arun_last_status_sent = timer_sec();
}

void wspr_autorun_start()
{
    if (*wspr_c.rcall == '\0' || wspr_c.rgrid[0] == '\0') {
        printf("WSPR autorun: reporter callsign and grid square fields must be entered on WSPR section of admin page\n");
        return;
    }

    for (int instance = 0; instance < rx_chans; instance++) {
        bool err;
        int band = cfg_int(stprintf("WSPR.autorun%d", instance), &err, CFG_OPTIONAL);
        if (!err && band != 0) {
            wspr_autorun(instance, band-1);
        }
    }
}

void wspr_autorun_restart()
{
    for (int instance = 0; instance < rx_chans; instance++) {
        internal_conn_shutdown(&iconn[instance]);
    }
    TaskSleepReasonSec("wspr_autorun_stop", 3);     // give time to disconnect
    memset(iconn, 0, sizeof(internal_conn_t));
    wspr_autorun_start();
}

void wspr_main();

ext_t wspr_ext = {
	"wspr",
	wspr_main,
	wspr_close,
	wspr_msgs,
	EXT_NEW_VERSION,
	EXT_FLAGS_HEAVY
};

void wspr_main()
{
	int i;
	
    assert(FSPS == round(SYMTIME * FSRATE));
    assert(SPS == (int) FSPS);
    assert(HSPS == (SPS/2));

    nffts = FPG * floor(GROUPS-1) -1;
    nbins_411 = ceilf(NFFT * BW_MAX / FSRATE) +1;
    hbins_205 = (nbins_411-1)/2;

    // NB: must be called before shmem_ipc_setup() since it initializes referenced, but non-shared, memory
    wspr_init();

	for (i=0; i < rx_chans; i++) {
        wspr_t *w = &WSPR_SHMEM->wspr[i];
		memset(w, 0, sizeof(wspr_t));
		
		WSPR_CHECK(w->magic1 = 0xcafe; w->magic2 = 0xbabe;)
		w->rx_chan = i;
		w->buf = &WSPR_SHMEM->wspr_buf[i];
		
		w->medium_effort = 1;
		w->wspr_type = WSPR_TYPE_2MIN;
		
		w->fftin = (WSPR_FFTW_COMPLEX*) WSPR_FFTW_MALLOC(sizeof(WSPR_FFTW_COMPLEX)*NFFT);
		w->fftout = (WSPR_FFTW_COMPLEX*) WSPR_FFTW_MALLOC(sizeof(WSPR_FFTW_COMPLEX)*NFFT);
		w->fftplan = WSPR_FFTW_PLAN_DFT_1D(NFFT, w->fftin, w->fftout, FFTW_FORWARD, FFTW_ESTIMATE);

	    wspr_reset(w);

        #ifdef WSPR_SHMEM_DISABLE
        #else
            // Needs to be done as separate Linux process per channel because wspr_decode() is
            // long-running and there would be no time-slicing otherwise, i.e. no NextTask() equivalent.
            shmem_ipc_setup(stprintf("kiwi.wspr-%02d", i), SIG_IPC_WSPR + i, wspr_decode);
        #endif
	}
	
	for (i=0; i < NFFT; i++) {
		window[i] = sin(i * K_PI/(NFFT-1));
	}

	ext_register(&wspr_ext);
    frate = ext_update_get_sample_rateHz(-2);
    fdecimate = frate / FSRATE;
    int_decimate = snd_rate / FSRATE;
    wspr_gprintf("WSPR %s decimation: srate=%.6f/%d decim=%.6f/%d sps=%d NFFT=%d nbins_411=%d\n", FRACTIONAL_DECIMATION? "fractional" : "integer",
        frate, snd_rate, fdecimate, int_decimate, SPS, NFFT, nbins_411);

    wspr_autorun_start();
}

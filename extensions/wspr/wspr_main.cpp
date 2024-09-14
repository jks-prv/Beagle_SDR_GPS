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

// Copyright (c) 2024 John Seamons, ZL4VO/KF6VO

#include "types.h"
#include "config.h"
#include "mem.h"
#include "net.h"
#include "rx.h"
#include "rx_util.h"
#include "web.h"
#include "non_block.h"
#include "shmem.h"
#include "coroutines.h"
#include "wspr.h"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <math.h>
#include <strings.h>
#include <sys/mman.h>

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

static int wspr_arun_band[MAX_ARUN_INST];
static int wspr_arun_preempt[MAX_ARUN_INST];
static bool wspr_arun_seen[MAX_ARUN_INST];
static internal_conn_t iconn[MAX_ARUN_INST];

// assigned constants
int nffts, nbins_411, hbins_205;

// computed constants
static float window[NFFT];


#define AUTORUN_BFO 750
#define AUTORUN_FILTER_BW 300

// order matches "cfg value order" wspr.js::wspr.menu_i_to_cfg_i in wspr.js (NOT "menu order" wspr.autorun_u)
// only add new entries to the end so as not to disturb existing values stored in config
// WSPR_BAND_IWBP entry for IWBP makes url param &ext=wspr,iwbp work due to freq range check
#define WSPR_BAND_IWBP 9999
static double wspr_cfs[] = {
    137.5, 475.7, 1838.1, 3570.1, 3594.1, 5288.7, 5366.2, 7040.1, 10140.2, 14097.1,     // 1-10
    18106.1, 21096.1, 24926.1, 28126.1,                 // 11-14
    50294.5, 70092.5, 144490.5, 432301.5, 1296501.5,    // 15-19
    6781.5, 13555.4, WSPR_BAND_IWBP,                    // 20-22
    40681.5, 10489569.5                                 // 23-24
};

// freqs on github.com/HB9VQQ/WSPRBeacon are cf - 1.5 kHz BFO (dial frequencies)
// so we add 1.5 to those to get our cf values (same as regular WSPR wspr_center_freqs above)
#define WSPR_HOP_PERIOD 20
//#define WSPR_HOP_PERIOD 6
static double wspr_cf_IWBP[] = { 1838.1, 3570.1, 5288.7, 7040.1, 10140.2, 14097.1, 18106.1, 21096.1, 24926.1, 28126.1 };
//static double wspr_cf_IWBP[] = { 1838.1, 3570.1, 7040.1 };
//static double wspr_cf_IWBP[] = { 7040.1, 10140.2, 14097.1 };


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
		    //wspr_aprintf("WSPR_FFTtask wakeups=%d runs=%d\n", rx_chan, w->fft_wakeups, w->fft_runs);
		    w->fft_runs = w->fft_wakeups;
		}
		//wspr_printf("WSPR_FFTtask ..wakeup\n");
		
		int grp = w->FFTtask_group;
		int first = grp*FPG, last = first+FPG;
	
		if (grp == 0) {
		    wspr_aprintf("FFT pp=%d grp=%d (%d-%d)\n", w->fft_ping_pong, grp, first, last);
		    #if 0
                if (w->not_launched) {
                    wspr_aprintf("WSPR_FFTtask decoder missed launch! pgrp=%d/%d plast=%d/%d ============================================\n",
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
		
		if (w->iwbp && w->autorun) {
            int min, sec; utc_hour_min_sec(NULL, &min, &sec);
		    //wspr_dprintf("WSPR_FFTtask %s %02d:%02d deco=%.1f tune=%.1f\n", (last < nffts)? "LOOP" : "DONE",
		    //    min, sec, w->arun_deco_cf_MHz*1e3, w->arun_cf_MHz*1e3);
		    int per = ((min&1) * 60) + sec;
		    const int trig = 60 + 50;
		    if (w->prev_per < trig && per >= trig) {
                int deco_hop = (min % WSPR_HOP_PERIOD) / 2;
                double deco_cf_kHz = wspr_cf_IWBP[deco_hop];
		        
                int tune_hop = ((min+1) % WSPR_HOP_PERIOD) / 2;
                double tune_cf_kHz = wspr_cf_IWBP[tune_hop];

		        wspr_dprintf("WSPR IWBP TRIG %02d:%02d deco_cf_kHz=%.2f tune_cf_kHz=%.2f ====\n",
		            min, sec, deco_cf_kHz, tune_cf_kHz);
		                        
	            double cfo = roundf((deco_cf_kHz - floorf(deco_cf_kHz)) * 1e3);
                input_msg_internal(w->arun_cext, (char *) "SET dialfreq=%.2f centerfreq=%.2f cf_offset=%.0f",
                    deco_cf_kHz - AUTORUN_BFO/1e3, deco_cf_kHz, cfo);

                double if_freq_kHz = (tune_cf_kHz - AUTORUN_BFO/1e3) - freq.offset_kHz;
                input_msg_internal(w->arun_csnd, (char *) "SET mod=usb low_cut=%d high_cut=%d freq=%.3f",
                    AUTORUN_BFO - AUTORUN_FILTER_BW/2, AUTORUN_BFO + AUTORUN_FILTER_BW/2, if_freq_kHz);

                w->arun_deco_cf_MHz = deco_cf_kHz / 1e3;
                w->arun_cf_MHz = tune_cf_kHz / 1e3;
		    }
		    w->prev_per = per;
		}
		
		if (last < nffts) continue;
	
		w->decode_ping_pong = w->fft_ping_pong;
		wspr_aprintf("FFT ---DECODE -> %d\n", w->decode_ping_pong);
		wspr_printf("\n");
		w->not_launched = 0;
		TaskWakeupFP(w->WSPR_DecodeTask_id, TWF_CHECK_WAKING, TO_VOID_PARAM(w->rx_chan));
    }
}

void wspr_send_peaks(wspr_t *w, int start, int stop)
{
    char *peaks_s = NULL;
    int j;
    
    // update peaks that have changed
    for (j = start; j < stop; j++)
        w->pk_save[j] = w->pk_freq[j];
    
    // complete peak list sent on each update
    for (j = 0; j < w->npk; j++) {
    	int bin_flags = w->pk_save[j].bin0 | w->pk_save[j].flags;
    	peaks_s = kstr_asprintf(peaks_s, "%d:%s:", bin_flags, w->pk_save[j].snr_call);
    }
	ext_send_msg_encoded(w->rx_chan, WSPR_DEBUG_MSG, "EXT", "WSPR_PEAKS", "%s", kstr_sp(peaks_s));
	kstr_free(peaks_s);
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
        static int arct;    // yes, static because decode msgs from all channels interleaved together
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
            grid_to_distance_km(&wspr_c.r_loc, d->grid), d->dBm, W_s);
        if (log_decodes)
            rcfprintf(w->rx_chan, printf_type, "%s DECODE: %02d%02d %3.0f %4.1f %9.6f %2d  %-6s %s %5d  %3d (%s)\n",
                w->iwbp? "IWBP" : "WSPR",
                d->hour, d->min, d->snr, d->dt_print, d->freq_print, (int) d->drift1,
                d->call, d->grid, grid_to_distance_km(&wspr_c.r_loc, d->grid), d->dBm, W_s);
    } else
    
    if (d->r_valid == 2) {	// TYPE2
        if (strcmp(d->call, "...") == 0) {
            ext_send_msg_encoded(w->rx_chan, WSPR_DEBUG_MSG, "EXT", "WSPR_DECODED",
                "%02d%02d %3.0f %4.1f %9.6f %2d  ...                %3d (%s)",
                d->hour, d->min, d->snr, d->dt_print, d->freq_print, (int) d->drift1, d->dBm, W_s);
            if (log_decodes)
                rcfprintf(w->rx_chan, printf_type, "%s DECODE: %02d%02d %3.0f %4.1f %9.6f %2d  ...                %3d (%s)\n",
                    w->iwbp? "IWBP" : "WSPR",
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
                rcfprintf(w->rx_chan, printf_type, "%s DECODE: %02d%02d %3.0f %4.1f %9.6f %2d  %-17s  %3d (%s)\n",
                    //kkkkk--ppp
                    w->iwbp? "IWBP" : "WSPR",
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
                d->grid, d->grid, grid_to_distance_km(&wspr_c.r_loc, d->grid), d->dBm, W_s);
            if (log_decodes)
                rcfprintf(w->rx_chan, printf_type, "%s DECODE: %02d%02d %3.0f %4.1f %9.6f %2d  ...  %6s %5d  %3d (%s)\n",
                    w->iwbp? "IWBP" : "WSPR",
                    d->hour, d->min, d->snr, d->dt_print, d->freq_print, (int) d->drift1,
                    d->grid, grid_to_distance_km(&wspr_c.r_loc, d->grid), d->dBm, W_s);
        } else {
            ext_send_msg_encoded(w->rx_chan, WSPR_DEBUG_MSG, "EXT", "WSPR_DECODED",
                "%02d%02d %3.0f %4.1f %9.6f %2d  "
                "<a href='https://www.qrz.com/lookup/%s' target='_blank'>%s</a> "
                "<a href='http://www.levinecentral.com/ham/grid_square.php?Grid=%s' target='_blank'>%s</a> "
                "%d %d (%s)",
                d->hour, d->min, d->snr, d->dt_print, d->freq_print, (int) d->drift1,
                d->call, d->call, d->grid, d->grid,
                grid_to_distance_km(&wspr_c.r_loc, d->grid), d->dBm, W_s);
            if (log_decodes)
                rcfprintf(w->rx_chan, printf_type, "%s DECODE: %02d%02d %3.0f %4.1f %9.6f %2d  %s %s %d %d (%s)\n",
                    w->iwbp? "IWBP" : "WSPR",
                    d->hour, d->min, d->snr, d->dt_print, d->freq_print, (int) d->drift1,
                    d->call, d->grid, grid_to_distance_km(&wspr_c.r_loc, d->grid), d->dBm, W_s);
        }
    }
    
    kiwi_asfree(W_s);
}

// Pass result json back to main process via shmem->status_u4
// since _upload_task runs in context of child_task()'s child process.
static int _upload_task(void *param)
{
	nbcmd_args_t *args = (nbcmd_args_t *) param;
	int rx_chan = args->func_param;
    wspr_t *w = &WSPR_SHMEM->wspr[rx_chan];
	char *sp = kstr_sp(args->kstr);
    sp = strstr(sp, "<body>"); sp += 6 + SPACE_FOR_NL + SPACE_FOR_NULL;
    char *eol = strchr(sp, '\n'); *eol = '\0';
    int added = 0, total = 0;
    sscanf(sp, " %d out of %d", &added, &total);
    shmem->status_u4[N_SHMEM_ST_WSPR][rx_chan][0] = added;
    shmem->status_u4[N_SHMEM_ST_WSPR][rx_chan][1] = total;
    return 0;
}

static void wspr_update_spot_count(int rx_chan)
{
    wspr_t *w = &WSPR_SHMEM->wspr[rx_chan];
    if (w->autorun) {
        const char *pre = w->arun_csnd->arun_preempt? (wspr_c.GPS_update_grid? ",%20pre" : ",%20preemptable") : "";
        const char *rgrid = wspr_c.GPS_update_grid? stprintf(",%%20%s", wspr_c.rgrid) : "";
        input_msg_internal(w->arun_csnd, (char *) "SET geoloc=%d%%20decoded%s%s", w->arun_decoded, pre, rgrid);
    }
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
        // extension: spots uploaded via javascript (see ext_send_msg_encoded() below)
        // autorun: spots uploaded below via curl
        #define CURL_UPLOADS
        //#define TEST_UPLOADS
        
        #define WSPR_SPOT "curl -sL 'http://wsprnet.org/post?function=wspr&" \
            "rcall=%s&rgrid=%s&rqrg=%.6f&date=%02d%02d%02d&time=%02d%02d&sig=%.0f&" \
            "dt=%.1f&drift=%d&tqrg=%.6f&tcall=%s&tgrid=%s&dbm=%s&version=1.4A+Kiwi'%s"
        int year, month, day; utc_year_month_day(&year, &month, &day);
        char *cmd;

        double rqrg = w->autorun? w->arun_deco_cf_MHz : w->centerfreq_MHz;
        wspr_ulprintf("%s UPLOAD %d spots RX%d %.4f START%s\n", w->iwbp? "IWBP" : "WSPR", w->uniques, w->rx_chan, rqrg, w->autorun? " AUTORUN" : "");
        for (int i = 0; i < w->uniques; i++) {
            wspr_decode_t *dp = &w->deco[i];
            if (strcmp(dp->call, "...") == 0) continue;
            
            if (w->autorun) {
                asprintf(&cmd, WSPR_SPOT, wspr_c.rcall, wspr_c.rgrid, rqrg, year%100, month, day,
                    dp->hour, dp->min, dp->snr, dp->dt_print, (int) dp->drift1, dp->freq_print, dp->call, dp->grid, dp->pwr,
                    wspr_c.spot_log? "" : " >/dev/null 2>&1");
                #ifdef TEST_UPLOADS
                    wspr_printf("WSPR UPLOAD RX%d %d/%d %s\n", w->rx_chan, i+1, w->uniques, cmd);
                #else
                    if (wspr_c.spot_log) {
                        non_blocking_cmd_func_forall("kiwi.wsprnet.org", cmd, _upload_task, w->rx_chan, POLL_MSEC(250));
                        rcprintf(w->rx_chan, "%s UPLOAD: %s\n", w->iwbp? "IWBP" : "WSPR", cmd);
                        int added = shmem->status_u4[N_SHMEM_ST_WSPR][rx_chan][0];
                        int total = shmem->status_u4[N_SHMEM_ST_WSPR][rx_chan][1];
                        rcprintf(w->rx_chan, "%s UPLOAD: wsprnet.org said: \"%d out of %d spot(s) added\" [%s]\n", w->iwbp? "IWBP" : "WSPR", added, total, dp->call);
                    } else {
                        non_blocking_cmd_system_child("kiwi.wsprnet.org", cmd, NO_WAIT);
                    }
                #endif
                kiwi_asfree(cmd);
                w->arun_decoded++;
            } else {
                #ifdef CURL_UPLOADS
                    wspr_printf("WSPR UPLOAD %02d%02d %3.0f %4.1f %9.6f %2d %s\n",
                        dp->hour, dp->min, dp->snr, dp->dt_print, dp->freq_print, (int) dp->drift1, dp->c_l_p);

                    // make the dp->call field invalid so wsprnet.org rejects the upload
                    #ifdef TEST_UPLOADS
                        strcpy(dp->call, "...");
                    #endif
                    asprintf(&cmd, WSPR_SPOT, wspr_c.rcall, wspr_c.rgrid, rqrg, year%100, month, day,
                        dp->hour, dp->min, dp->snr, dp->dt_print, (int) dp->drift1, dp->freq_print, dp->call, dp->grid, dp->pwr,
                        wspr_c.spot_log? "" : " >/dev/null 2>&1");
                    if (wspr_c.spot_log) {
                        non_blocking_cmd_func_forall("kiwi.wsprnet.org", cmd, _upload_task, w->rx_chan, POLL_MSEC(250));
                        rcprintf(w->rx_chan, "%s UPLOAD: %s\n", w->iwbp? "IWBP" : "WSPR", cmd);
                        int added = shmem->status_u4[N_SHMEM_ST_WSPR][rx_chan][0];
                        int total = shmem->status_u4[N_SHMEM_ST_WSPR][rx_chan][1];
                        rcprintf(w->rx_chan, "%s UPLOAD: wsprnet.org said: \"%d out of %d spot(s) added\" [%s]\n", w->iwbp? "IWBP" : "WSPR", added, total, dp->call);
                    } else {
                        non_blocking_cmd_system_child("kiwi.wsprnet.org", cmd, NO_WAIT);
                    }
                    kiwi_asfree(cmd);
                #else
                    //printf("WSPR #%d skip_upload %d\n", i, w->skip_upload);
                    if (w->skip_upload == 0) {
                        ext_send_msg_encoded(w->rx_chan, WSPR_DEBUG_MSG, "EXT", "WSPR_UPLOAD",
                            "%02d%02d %3.0f %4.1f %9.6f %2d %s",
                            dp->hour, dp->min, dp->snr, dp->dt_print, dp->freq_print, (int) dp->drift1, dp->c_l_p);
                    }
                #endif
            }
            wspr_printf("%s UPLOAD U%d/%d %s"
                "%02d%02d %3.0f %4.1f %9.6f %2d %s" "\n",
                w->iwbp? "IWBP" : "WSPR", i, w->uniques, w->autorun? "autorun ":"",
                dp->hour, dp->min, dp->snr, dp->dt_print, dp->freq_print, (int) dp->drift1, dp->c_l_p);
            TaskSleepMsec(1000);
        }
        wspr_ulprintf("%s UPLOAD %d spots RX%d %.4f DONE\n", w->iwbp? "IWBP" : "WSPR", w->uniques, w->rx_chan, rqrg);
        if (w->skip_upload > 0) w->skip_upload--;
        //printf("WSPR skip_upload=%d\n", w->skip_upload);

		wspr_status(w, w->status_resume, NONE);
		
		if (w->autorun) {
		    // send wsprstat on startup and every 6 minutes thereafter
		    u4_t now = timer_sec();
		    if (now - w->arun_last_status_sent > MINUTES_TO_SEC(6)) {
		        w->arun_last_status_sent = now;
		        
		        // in case wspr_c.rgrid has changed 
		        kiwi_asfree(w->arun_stat_cmd);
		        #define WSPR_STAT "curl -L 'http://wsprnet.org/post?function=wsprstat&rcall=%s&rgrid=%s&rqrg=%.6f&tpct=0&tqrg=%.6f&dbm=0&version=1.4A+Kiwi' >/dev/null 2>&1"
                asprintf(&w->arun_stat_cmd, WSPR_STAT, wspr_c.rcall, wspr_c.rgrid, w->arun_cf_MHz, w->arun_cf_MHz);
                //printf("AUTORUN %s\n", w->arun_stat_cmd);
                non_blocking_cmd_system_child("kiwi.wsprnet.org", w->arun_stat_cmd, NO_WAIT);
		    }
		    if (w->arun_decoded > w->arun_last_decoded) {
		        wspr_update_spot_count(w->rx_chan);
		        w->arun_last_decoded = w->arun_decoded;
            }
		}
    }
}

static double frate, fdecimate;
static int int_decimate;

void wspr_data(int rx_chan, int instance, int nsamps, TYPECPX *samps)
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
                    TaskWakeupFP(w->WSPR_FFTtask_id, TWF_CHECK_WAKING, w->rx_chan);
                w->group++;
            }
            w->didx++;
    
            w->fi += fdecimate;
            i = trunc(w->fi);
        }
        w->fi -= nsamps;	// keep bounded

	#else

        if (w->test) {
            if (w->s2p >= wspr_c.s2p_end) {
                ext_send_msg(w->rx_chan, false, "EXT test_done");
                w->test = false;
                return;
            }

            for (i=0; i<nsamps; i++) {
    
                // pushback audio samples
                WSPR_CPX_t re = (WSPR_CPX_t) (s4_t) (s2_t) FLIP16(*w->s2p);
                samps[i].re = re;
                w->s2p++;
                WSPR_CPX_t im = (WSPR_CPX_t) (s4_t) (s2_t) FLIP16(*w->s2p);
                samps[i].im = im;
                w->s2p++;

                // decimate
                if (w->decim++ < (int_decimate-1))
                    continue;
                w->decim = 0;
            
                if (w->didx >= TPOINTS)
                    return;

                if (w->group == 3) w->tsync = FALSE;	// arm re-sync
            
                idat[w->didx] = re;
                qdat[w->didx] = im;
    
                if ((w->didx % NFFT) == (NFFT-1)) {
                    w->fft_ping_pong = w->ping_pong;
                    w->FFTtask_group = w->group-1;
                    if (w->group) {     // skip first to pipeline
                        w->fft_wakeups++;
                        TaskWakeupFP(w->WSPR_FFTtask_id, TWF_CHECK_WAKING, TO_VOID_PARAM(w->rx_chan));
                    }
                    w->group++;
                }
                w->didx++;
            }

            int pct = w->nsamps * 100 / wspr_c.tsamps;
            w->nsamps += nsamps;
            pct += 3;
            if (pct > 100) pct = 100;
            ext_send_msg(w->rx_chan, false, "EXT bar_pct=%d", pct);
        } else {
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
                        TaskWakeupFP(w->WSPR_FFTtask_id, TWF_CHECK_WAKING, TO_VOID_PARAM(w->rx_chan));
                    }
                    w->group++;
                }
                w->didx++;
            }
        }

    #endif
}

void wspr_reset(wspr_t *w)
{
    w->status_resume = IDLE;
    w->tsync = FALSE;
    w->capture = 0;
    w->last_sec = -1;
    w->abort_decode = false;
    w->send_error = false;
    w->autorun = false;
    w->iwbp = false;
}

void wspr_close(int rx_chan)
{
    rx_util_t *r = &rx_util;
    wspr_t *w = &WSPR_SHMEM->wspr[rx_chan];
    assert(rx_chan == w->rx_chan);
    //rcprintf(rx_chan, "WSPR: close rx_chan=%d autorun=%d arun_which=%d create_tasks=%d\n",
    //    rx_chan, w->autorun, r->arun_which[rx_chan], w->create_tasks);
    ext_unregister_receive_iq_samps(w->rx_chan);

	if (w->autorun) {
        internal_conn_shutdown(&iconn[w->instance]);
	}

	if (w->create_tasks) {
		//wspr_printf("wspr_close TaskRemove FFT%d deco%d\n", w->WSPR_FFTtask_id, w->WSPR_DecodeTask_id);
		TaskRemove(w->WSPR_FFTtask_id);
		TaskRemove(w->WSPR_DecodeTask_id);
		w->create_tasks = false;
	}
	
	wspr_reset(w);
    r->arun_which[rx_chan] = ARUN_NONE;
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
    
    if (strcmp(msg, "SET IWBP") == 0) {
        wspr_dprintf("SET IWBP\n");
        w->iwbp = true;
        return true;
    }
    
	float df, cf;
	int d;
	n = sscanf(msg, "SET dialfreq=%f centerfreq=%f cf_offset=%d", &df, &cf, &d);
	if (n == 3) {
		w->dialfreq_MHz = df / kHz;
		w->centerfreq_MHz = cf / kHz;
		w->cf_offset = (float) d;
		wspr_printf("dialfreq_MHz=%.6f centerfreq_MHz=%.6f cf_offset=%.0f\n", w->dialfreq_MHz, w->centerfreq_MHz, w->cf_offset);
		return true;
	}
	
	if (strcmp(msg, "SET autorun") == 0) {
	    w->autorun = true;
        wspr_c.freq_offset_Hz = freq.offset_Hz;
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
	
	int test;
	n = sscanf(msg, "SET test=%d", &test);
	if (n == 1) {
        w->s2p = wspr_c.s2p_start;
        w->test = test? true:false;
        w->nsamps = 0;

        // skip several upload cycles to make sure the test decodes don't get uploaded
        if (w->test) w->skip_upload = 4;
        //printf("WSPR SET TEST test=%d skip_upload=%d\n", w->test, w->skip_upload);
		return true;
	}
	
	return false;
}

void wspr_update_rgrid(char *rgrid)
{
    kiwi_strncpy(wspr_c.rgrid, rgrid, LEN_GRID);
    wspr_set_latlon_from_grid((char *) wspr_c.rgrid);
    printf("wspr_c.rgrid %s\n", wspr_c.rgrid);
    
    // update grid shown in user lists when there haven't been any new spots recently
    for (int ch = 0; ch < MAX_RX_CHANS; ch++) {
        wspr_t *w = &WSPR_SHMEM->wspr[ch];
        if (w->autorun)
            wspr_update_spot_count(ch);
    }
}

// catch changes to reporter call/grid from admin page WSPR config (also called during initialization)
bool wspr_update_vars_from_config(bool called_at_init_or_restart)
{
    int i, n;
    rx_util_t *r = &rx_util;
    bool update_cfg = false;
    char *s;
    
    cfg_default_object("WSPR", "{}", &update_cfg);
    
    // Changing reporter call on admin page requires restart. This is because of
    // conditional behavior at startup, e.g. uploads enabled because valid call is now present
    // or autorun tasks starting for the same reason.
    // wspr_c.rcall is still updated here to handle the initial assignment and
    // manual changes from WSPR admin page.
    
    cfg_default_string("WSPR.callsign", "", &update_cfg);
    s = (char *) cfg_string("WSPR.callsign", NULL, CFG_REQUIRED);
    kiwi_ifree(wspr_c.rcall, "wspr_main rcall");
	wspr_c.rcall = kiwi_str_encode(s);
	cfg_string_free(s);

    cfg_default_string("WSPR.grid", "", &update_cfg);
    s = (char *) cfg_string("WSPR.grid", NULL, CFG_REQUIRED);
	kiwi_strncpy(wspr_c.rgrid, s, LEN_GRID);
	cfg_string_free(s);
    wspr_set_latlon_from_grid((char *) wspr_c.rgrid);

    // Make sure WSPR.autorun holds *correct* count of non-preemptable autorun processes.
    // For the benefit of refusing enable of public listing if there are no non-preemptable autoruns.
    // If Kiwi was previously configured for a larger rx_chans, and more than rx_chans worth
    // of autoruns were enabled, then with a reduced rx_chans it is essential not to count
    // the ones beyond the rx_chans limit. That's why "i < rx_chans" appears below and
    // not MAX_RX_CHANS.
    if (called_at_init_or_restart) {
        int num_autorun = 0, num_non_preempt = 0;
        for (int instance = 0; instance < rx_chans; instance++) {
            int autorun = cfg_default_int(stprintf("WSPR.autorun%d", instance), 0, &update_cfg);
            int preempt = cfg_default_int(stprintf("WSPR.preempt%d", instance), 0, &update_cfg);
            //cfg_default_int(stprintf("WSPR.start%d", instance), 0, &update_cfg);
            //cfg_default_int(stprintf("WSPR.stop%d", instance), 0, &update_cfg);
            //printf("WSPR.autorun%d=%d(band=%d) WSPR.preempt%d=%d\n", instance, autorun, autorun-1, instance, preempt);
            if (autorun) num_autorun++;
            if (autorun && (preempt == 0)) num_non_preempt++;
            wspr_arun_band[instance] = autorun;
            wspr_arun_preempt[instance] = preempt;
        }
        if (wspr_c.rcall == NULL || *wspr_c.rcall == '\0' || wspr_c.rgrid[0] == '\0') {
            printf("WSPR autorun: reporter callsign and grid square fields must be entered on WSPR section of admin page\n");
            num_autorun = num_non_preempt = 0;
        }
        wspr_c.num_autorun = num_autorun;
        cfg_update_int("WSPR.autorun", num_non_preempt, &update_cfg);
        //printf("WSPR autorun: num_autorun=%d WSPR.autorun=%d(non-preempt) rx_chans=%d\n", num_autorun, num_non_preempt, rx_chans);
    }

    wspr_c.GPS_update_grid = cfg_default_bool("WSPR.GPS_update_grid", false, &update_cfg);
    wspr_c.syslog = cfg_default_bool("WSPR.syslog", false, &update_cfg);
    wspr_c.spot_log = cfg_default_bool("WSPR.spot_log", false, &update_cfg);

	//printf("wspr_update_vars_from_config: rcall <%s> wspr_c.rgrid=<%s> wspr_c.GPS_update_grid=%d\n", wspr_c.rcall, wspr_c.rgrid, wspr_c.GPS_update_grid);
    return update_cfg;
}
    
void wspr_autorun(int instance, bool initial)
{
    rx_util_t *r = &rx_util;
    int band = wspr_arun_band[instance]-1;
	double center_freq_kHz, tune_freq_kHz;
	bool iwbp = (wspr_cfs[band] == WSPR_BAND_IWBP);

    if (iwbp) {
        int min;
        utc_hour_min_sec(NULL, &min, NULL);
        int hop = (min % WSPR_HOP_PERIOD) / 2;
        tune_freq_kHz = center_freq_kHz = wspr_cf_IWBP[hop];    // decode freq
    } else {
        tune_freq_kHz = center_freq_kHz = wspr_cfs[band];
    }

    double dial_freq_kHz = center_freq_kHz - AUTORUN_BFO/1e3;
    double if_freq_kHz = (tune_freq_kHz - AUTORUN_BFO/1e3) - freq.offset_kHz;
	double cfo = roundf((center_freq_kHz - floorf(center_freq_kHz)) * 1e3);
	
	if (!rx_freq_inRange(dial_freq_kHz)) {
	    if (!wspr_arun_seen[instance]) {
            printf("WSPR autorun: ERROR band=%d dial_freq_kHz %.2f is outside rx range %.2f - %.2f\n",
                band, dial_freq_kHz, freq.offset_kHz, freq.offmax_kHz);
	        wspr_arun_seen[instance] = true;
	    }
	    return;
	}

    bool preempt = (wspr_arun_preempt[instance] != ARUN_PREEMPT_NO);
    char *ident_user;
    asprintf(&ident_user, "%s-autorun", iwbp? "IWBP" : "WSPR");
    char *geoloc;
    const char *pre = preempt? (wspr_c.GPS_update_grid? ",%20pre" : ",%20preemptable") : "";
    const char *rgrid = wspr_c.GPS_update_grid? stprintf(",%%20%s", wspr_c.rgrid) : "";
    asprintf(&geoloc, "0%%20decoded%s%s", pre, rgrid);

	bool ok = internal_conn_setup(ICONN_WS_SND | ICONN_WS_EXT, &iconn[instance], instance, PORT_BASE_INTERNAL_WSPR,
        WS_FL_IS_AUTORUN | (initial? WS_FL_INITIAL : 0),
        "usb", AUTORUN_BFO - AUTORUN_FILTER_BW/2, AUTORUN_BFO + AUTORUN_FILTER_BW/2, if_freq_kHz,
        ident_user, geoloc, "wspr");
    free(ident_user); free(geoloc);
    if (!ok) {
        //printf("WSPR autorun: internal_conn_setup() FAILED instance=%d band=%d dial=%.2f\n",
	    //    instance, band, dial_freq_kHz);
        return;
    }

    conn_t *csnd = iconn[instance].csnd;
    csnd->arun_preempt = preempt;
    int rx_chan = csnd->rx_channel;
    r->arun_which[rx_chan] = ARUN_WSPR;
    r->arun_band[rx_chan] = band;
    wspr_t *w = &WSPR_SHMEM->wspr[rx_chan];
    iconn[instance].param = w;
    wspr_reset(w);
    //w->debug = 1;
    w->instance = instance;
    w->rx_chan = rx_chan;
    w->arun_csnd = csnd;
    w->arun_deco_cf_MHz = center_freq_kHz / 1e3;
    w->arun_cf_MHz = tune_freq_kHz / 1e3;
    w->arun_decoded = 0;
    w->arun_last_decoded = -1;

	clprintf(csnd, "WSPR autorun: START instance=%d rx_chan=%d band=%d%s off=%.2f if=%.2f tf=%.2f df=%.2f cf=%.2f cfo=%.0f\n",
	    instance, rx_chan, band, iwbp? "(IWBP)" : "",
	    freq.offset_kHz, if_freq_kHz, tune_freq_kHz, dial_freq_kHz, center_freq_kHz, cfo);
	
    conn_t *cext = iconn[instance].cext;
    w->arun_cext = cext;
    w->abort_decode = true;
    input_msg_internal(cext, (char *) "SET autorun");
    input_msg_internal(cext, (char *) "SET BFO=%d", AUTORUN_BFO);
    w->iwbp = iwbp? true : false;
    if (iwbp) input_msg_internal(cext, (char *) "SET IWBP");
    input_msg_internal(cext, (char *) "SET dialfreq=%.2f centerfreq=%.2f cf_offset=%.0f", dial_freq_kHz, center_freq_kHz, cfo);
    input_msg_internal(cext, (char *) "SET capture=1");     // ext task created here

    asprintf(&w->arun_stat_cmd, WSPR_STAT, wspr_c.rcall, wspr_c.rgrid, w->arun_cf_MHz, w->arun_cf_MHz);
    //printf("AUTORUN INIT %s\n", w->arun_stat_cmd);
    non_blocking_cmd_system_child("kiwi.wsprnet.org", w->arun_stat_cmd, NO_WAIT);
    w->arun_last_status_sent = timer_sec();
}

void wspr_autorun_start(bool initial)
{
    rx_util_t *r = &rx_util;
    if (wspr_c.num_autorun == 0) {
        //printf("WSPR autorun_start: none configured\n");
        return;
    }

    for (int instance = 0; instance < rx_chans; instance++) {
        int band = wspr_arun_band[instance];
        if (band == ARUN_REG_USE) continue;     // "regular use" menu entry
        band--;     // make array index
        
        // Is this instance already running on any channel?
        // This loop should never exclude wspr_autorun() when called from wspr_autorun_restart()
        // because all instances were just stopped.
        // When called from rx_autorun_restart_victims() it functions normally.
        int rx_chan;
        for (rx_chan = 0; rx_chan < rx_chans; rx_chan++) {
            if (r->arun_which[rx_chan] == ARUN_WSPR && r->arun_band[rx_chan] == band) {
                //printf("WSPR autorun: instance=%d band=%d %.2f already running on rx%d\n",
                //    instance, band, wspr_cfs[band], rx_chan);
                break;
            }
        }
        if (rx_chan == rx_chans) {
            // arun_{which,band} set only after wspr_autorun():internal_conn_setup() succeeds
            //printf("WSPR autorun: instance=%d band=%d %.2f START rx%d\n",
            //    instance, band, wspr_cfs[band], rx_chan);
            wspr_autorun(instance, initial);
        }
    }

    wspr_c.arun_restart_offset = false;
}

void wspr_autorun_restart()
{
    int rx_chan;
    rx_util_t *r = &rx_util;
    wspr_t *wspr_p[MAX_ARUN_INST];

    printf("WSPR autorun: RESTART\n");
    wspr_c.arun_suspend_restart_victims = true;
        // shutdown all
        for (int rx_chan = 0; rx_chan < rx_chans; rx_chan++) {
            wspr_p[rx_chan] = NULL;
            if (r->arun_which[rx_chan] == ARUN_WSPR) {
                wspr_t *w = &WSPR_SHMEM->wspr[rx_chan];
                wspr_p[rx_chan] = w;
                //printf("WSPR autorun STOP1 ch=%d w=%p\n", rx_chan, w);
                w->abort_decode = true;     // for MULTI_CORE it's important to stop the decode process
                internal_conn_shutdown(&iconn[w->instance]);
                r->arun_which[rx_chan] = ARUN_NONE;
            }
        }
        rx_autorun_clear();
    
        // MULTI_CORE: has to be long enough that wspr_p[rx_chan]->abort_decode above is seen by decode process
        #ifdef MULTI_CORE
            #define WSPR_PROCESS_ABORT_WAIT 8
        #else
            #define WSPR_PROCESS_ABORT_WAIT 3
        #endif
        TaskSleepReasonSec("wspr_autorun_stop", WSPR_PROCESS_ABORT_WAIT);

        // reset only autorun instances identified above (there may be non-autorun WSPR extensions running)
        for (int rx_chan = 0; rx_chan < rx_chans; rx_chan++) {
            if (wspr_p[rx_chan] != NULL) {
                printf(RED "WSPR autorun STOP2 ch=%d w=%p" NONL, rx_chan, wspr_p[rx_chan]);
                wspr_reset(wspr_p[rx_chan]);
            }
        }
        memset(iconn, 0, sizeof(iconn));
        memset(wspr_arun_seen, 0, sizeof(wspr_arun_seen));

        // bring wspr_arun_band[] and wspr_arun_preempt[] up-to-date
        wspr_update_vars_from_config(true);
        
        // XXX Don't start autorun here. Let rx_autorun_restart_victims() do it
        // because there might be other extension autorun restarting at the same time.
        // And starting too soon while the others are in the middle of kicking their EXT tasks
        // causes conflicts. rx_autorun_restart_victims() correctly waits for all extension
        // *.arun_suspend_restart_victims to become false.
        
        // restart all enabled
        //wspr_autorun_start(true);
    wspr_c.arun_suspend_restart_victims = false;
}

void wspr_poll(int rx_chan)
{
    wspr_t *w = &WSPR_SHMEM->wspr[rx_chan];
    
    // detect when freq offset changed so autorun can be restarted
    if (w->autorun && !wspr_c.arun_restart_offset && wspr_c.freq_offset_Hz != freq.offset_Hz) {
        wspr_c.arun_restart_offset = true;
        wspr_autorun_restart();
    }
}

void wspr_main();

ext_t wspr_ext = {
	"wspr",
	wspr_main,
	wspr_close,
	wspr_msgs,
	EXT_NEW_VERSION,
	EXT_FLAGS_HEAVY,
	wspr_poll
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
    frate = ext_update_get_sample_rateHz(ADC_CLK_TYP);
    fdecimate = frate / FSRATE;
    int_decimate = snd_rate / FSRATE;
    wspr_gprintf("WSPR %s decimation: srate=%.6f/%d decim=%.6f/%d sps=%d NFFT=%d nbins_411=%d\n", FRACTIONAL_DECIMATION? "fractional" : "integer",
        frate, snd_rate, fdecimate, int_decimate, SPS, NFFT, nbins_411);

    wspr_autorun_start(true);

    const char *fn = cfg_string("WSPR.test_file", NULL, CFG_OPTIONAL);
    if (!fn || *fn == '\0') return;
    char *fn2;
    asprintf(&fn2, "%s/samples/%s", DIR_CFG, fn);
    cfg_string_free(fn);
    printf("WSPR: mmap %s\n", fn2);
    int fd = open(fn2, O_RDONLY);
    if (fd < 0) {
        printf("WSPR: open failed\n");
        return;
    }
    off_t fsize = kiwi_file_size(fn2);
    kiwi_asfree(fn2);
    char *file = (char *) mmap(NULL, fsize, PROT_READ, MAP_PRIVATE, fd, 0);
    if (file == MAP_FAILED) {
        printf("WSPR: mmap failed\n");
        return;
    }
    close(fd);
    int words = fsize/2;
    wspr_c.s2p_start = (s2_t *) file;
    u4_t off = *(wspr_c.s2p_start + 3);
    off = FLIP16(off);
    printf("WSPR: off=%d size=%ld\n", off, fsize);
    off /= 2;
    wspr_c.s2p_start += off;
    words -= off;
    wspr_c.s2p_end = wspr_c.s2p_start + words;
    wspr_c.tsamps = words / NIQ;
}

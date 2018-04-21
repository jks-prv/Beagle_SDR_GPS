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

#include "wspr.h"
#include "net.h"
#include "web.h"
#include "non_block.h"

#ifndef EXT_WSPR
	void wspr_main() {}
#else

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

wspr_conf_t wspr_c;

static wspr_t wspr[RX_CHANS];

// assigned constants
int nffts, nbins_411, hbins_205;

// computed constants
static float window[NFFT];

const char *status_str[] = { "none", "idle", "sync", "running", "decoding" };

static void wspr_status(wspr_t *w, int status, int resume)
{
	wprintf("WSPR RX%d wspr_status: set status %d-%s\n", w->rx_chan, status, status_str[status]);

    // Send server time to client since that's what sync is based on.
    // Send on each status change to eliminate effects of browser host clock drift.
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    u4_t msec = ts.tv_nsec/1000000;
	ext_send_msg(w->rx_chan, WSPR_DEBUG_MSG, "EXT WSPR_STATUS=%d WSPR_TIME_MSEC=%d=%d",
	    status, ts.tv_sec, msec);

	w->status = status;
	if (resume != NONE) {
		wprintf("WSPR RX%d wspr_status: will resume to %d-%s\n", w->rx_chan, resume, status_str[resume]);
		w->status_resume = resume;
	}
}

// compute FFTs incrementally during data capture rather than all at once at the beginning of the decode
void WSPR_FFT(void *param)
{
	int i,j,k;
	
	while (1) {
		
		//wprintf("WSPR_FFTtask sleep..\n");
		int rx_chan = (int) FROM_VOID_PARAM(TaskSleep());
	
		wspr_t *w = &wspr[rx_chan];
		int grp = w->FFTtask_group;
		int first = grp*FPG, last = first+FPG;
		//wprintf("WSPR FFT pp=%d grp=%d (%d-%d)\n", w->fft_ping_pong, grp, first, last);
	
		// Do ffts over 2 symbols, stepped by half symbols
		WSPR_CPX_t *id = w->i_data[w->fft_ping_pong], *qd = w->q_data[w->fft_ping_pong];
	
		//float maxiq = 1e-66, maxpwr = 1e-66;
		//int maxi=0;
		float savg[NFFT];
		memset(savg, 0, sizeof(savg));
		
		// FIXME: A large noise burst can washout the w->pwr_sampavg which prevents a proper
		// peak list being created in the decoder. An individual signal can be decoded fine
		// in the presence of the burst. But if there is no peak in the list the decoding
		// process is never started! We've seen this problem fairly frequently.
		
		for (i=first; i<last; i++) {
			for (j=0; j<NFFT; j++) {
				k = i*HSPS+j;
				w->fftin[j][0] = id[k] * window[j];
				w->fftin[j][1] = qd[k] * window[j];
				//if (i==0) wprintf("IN %d %fi %fq\n", j, w->fftin[j][0], w->fftin[j][1]);
			}
			
			//u4_t start = timer_us();
			TRY_YIELD;
			WSPR_FFTW_EXECUTE(w->fftplan);
			TRY_YIELD;
			//if (i==0) wprintf("512 FFT %.1f us\n", (float)(timer_us()-start));
			
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
				//if (i==0) wprintf("OUT %d %fi %fq\n", j, ii, qq);
				//if (ii > maxiq) { maxiq = ii; }
				//if (pwr > maxpwr) { maxpwr = pwr; maxi = k; }
				w->pwr_samp[w->fft_ping_pong][j][i] = pwr;
				w->pwr_sampavg[w->fft_ping_pong][j] += pwr;
				savg[j] += pwr;
			}
			TRY_YIELD;
		}
	
		// send spectrum data to client
		float smspec[nbins_411], dB, max_dB=-99, min_dB=99;
		renormalize(w, savg, smspec);
		
		for (j=0; j<nbins_411; j++) {
			smspec[j] = dB = 10*log10(smspec[j]) - w->snr_scaling_factor;
			if (dB > max_dB) max_dB = dB;
			if (dB < min_dB) min_dB = dB;
		}
	
		float y, range_dB, pix_per_dB;
		max_dB = 6;
		min_dB = -33;
		range_dB = max_dB - min_dB;
		pix_per_dB = 255.0 / range_dB;
		u1_t ws[nbins_411+1];
		
		for (j=0; j<nbins_411; j++) {
			#if 0
				smspec[j] = -33 + ((j/30) * 3);		// test colormap
			#endif
			y = pix_per_dB * (smspec[j] - min_dB);
			if (y > 255.0) y = 255.0;
			if (y < 0) y = 0;
			ws[j+1] = (u1_t) y;
		}
		ws[0] = ((grp == 0) && w->tsync)? 1:0;
	
		if (ext_send_msg_data(w->rx_chan, WSPR_DEBUG_MSG, WSPR_DATA, ws, nbins_411+1) < 0) {
			w->send_error = true;
		}
	
		//wprintf("WSPR_FFTtask group %d:%d %d-%d(%d) %f %f(%d)\n", w->fft_ping_pong, grp, first, last, nffts, maxiq, maxpwr, maxi);
		//wprintf("  %d-%d(%d)\r", first, last, nffts); fflush(stdout);
		
		if (last < nffts) continue;
	
		w->decode_ping_pong = w->fft_ping_pong;
		wprintf("WSPR ---DECODE -> %d\n", w->decode_ping_pong);
		wprintf("\n");
		TaskWakeup(w->WSPR_DecodeTask_id, TRUE, TO_VOID_PARAM(w->rx_chan));
    }
}

void wspr_send_peaks(wspr_t *w, pk_t *pk, int npk)
{
    char peaks_s[NPK*(6+1 + LEN_CALL) + 16];
    char *s = peaks_s;
    *s = '\0';
    int j, n;
    for (j=0; j < npk; j++) {
    	int bin_flags = pk[j].bin0 | pk[j].flags;
    	n = sprintf(s, "%d:%s:", bin_flags, pk[j].snr_call); s += n;
    }
	ext_send_msg_encoded(w->rx_chan, WSPR_DEBUG_MSG, "EXT", "WSPR_PEAKS", "%s", peaks_s);
}

void WSPR_Deco(void *param)
{
    u4_t start=0;

	// fixme: revisit in light of new priority task system
	// this was intended to allow a minimum percentage of runtime
	TaskParams(4000);		//FIXME does this really still work?

	while (1) {
	
		wprintf("WSPR_DecodeTask sleep..\n");
		if (start) wprintf("WSPR_DecodeTask TOTAL %.1f sec\n", (float)(timer_ms()-start)/1000.0);
	
		int rx_chan = (int) FROM_VOID_PARAM(TaskSleep());
		wspr_t *w = &wspr[rx_chan];
	
		w->abort_decode = false;
		start = timer_ms();
		wprintf("WSPR_DecodeTask wakeup\n");
	
		wspr_status(w, DECODING, NONE);
		wspr_decode(w);
	
		if (w->abort_decode)
			wprintf("decoder aborted\n");
	
		wspr_status(w, w->status_resume, NONE);
		
		if (w->autorun) {
		    // send wsprstat on startup and every 6 minutes thereafter
		    u4_t now = timer_sec();
		    if (now - w->arun_last_status_sent > MINUTES_TO_SEC(6)) {
		        w->arun_last_status_sent = now;
                //printf("%s\n", w->arun_stat_cmd);
                non_blocking_cmd_system_child("kiwi.wsp", w->arun_stat_cmd, NO_WAIT);
		    }
		    if (w->arun_decoded > w->arun_last_decoded) {
		        input_msg_internal(w->arun_csnd, (char *) "SET geo=%d%%20decoded", w->arun_decoded);
		        w->arun_last_decoded = w->arun_decoded;
            }
		}
    }
}

static double frate;
static int int_decimate;

void wspr_data(int rx_chan, int ch, int nsamps, TYPECPX *samps)
{
	wspr_t *w = &wspr[rx_chan];
	int i;

    // FIXME: Someday it's possible samp rate will be different between rx_chans
    // if they have different bandwidths. Not possible with current architecture
    // of data pump.
    double frate = ext_update_get_sample_rateHz(rx_chan);
    double fdecimate = frate / FSRATE;
	
	//wprintf("WD%d didx %d send_error %d reset %d\n", w->capture, w->didx, w->send_error, w->reset);
	if (w->send_error) {
		wprintf("RX%d STOP send_error %d\n", w->rx_chan, w->send_error);
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
	
	time_t t;
	time(&t); struct tm tm; gmtime_r(&t, &tm);
	if (tm.tm_sec != w->last_sec) {
		if (tm.tm_min&1 && tm.tm_sec == 40)
			w->abort_decode = true;
		
		w->last_sec = tm.tm_sec;
	}
	
    if (w->tsync == FALSE) {		// sync to even minute boundary
        if (!(tm.tm_min&1) && (tm.tm_sec == 0)) {
            w->ping_pong ^= 1;
            wprintf("WSPR SYNC ping_pong %d, %s", w->ping_pong, ctime(&t));
            w->decim = w->didx = w->group = 0;
            w->fi = 0;
            if (w->status != DECODING)
                wspr_status(w, RUNNING, RUNNING);
            w->tsync = TRUE;
        }
    }
	
	if (w->didx == 0) {
    	memset(&w->pwr_sampavg[w->ping_pong][0], 0, sizeof(w->pwr_sampavg[0]));
	}
	
	if (w->group == 0) w->utc[w->ping_pong] = t;
	
	WSPR_CPX_t *idat = w->i_data[w->ping_pong], *qdat = w->q_data[w->ping_pong];
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
                //wprintf("WSPR SAMPLER pp=%d grp=%d frate=%.1f fdecimate=%.1f rx_chan=%d\n",
                //    w->ping_pong, w->group, frate, fdecimate, rx_chan);
                w->fft_ping_pong = w->ping_pong;
                w->FFTtask_group = w->group-1;
                if (w->group) TaskWakeup(w->WSPR_FFTtask_id, TRUE, w->rx_chan);	// skip first to pipeline
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
				if (w->group) TaskWakeup(w->WSPR_FFTtask_id, TRUE, TO_VOID_PARAM(w->rx_chan));	// skip first to pipeline
				w->group++;
			}
			w->didx++;
		}

    #endif
}

void wspr_close(int rx_chan)
{
	wspr_t *w = &wspr[rx_chan];
	//wprintf("WSPR wspr_close RX%d\n", rx_chan);
	if (w->create_tasks) {
		//wprintf("WSPR wspr_close TaskRemove FFT%d deco%d\n", w->WSPR_FFTtask_id, w->WSPR_DecodeTask_id);
		TaskRemove(w->WSPR_FFTtask_id);
		TaskRemove(w->WSPR_DecodeTask_id);
		w->create_tasks = false;
	}
}

bool wspr_msgs(char *msg, int rx_chan)
{
	wspr_t *w = &wspr[rx_chan];
	int n;
	
	wprintf("WSPR wspr_msgs RX%d <%s>\n", rx_chan, msg);
	
	if (strcmp(msg, "SET ext_server_init") == 0) {
		w->rx_chan = rx_chan;
		ext_send_msg(w->rx_chan, WSPR_DEBUG_MSG, "EXT nbins=%d ready", nbins_411);
		wspr_status(w, IDLE, IDLE);
		return true;
	}

	char *r_grid_m = NULL;
	n = sscanf(msg, "SET reporter_grid=%16ms", &r_grid_m);
	if (n == 1) {
		//wprintf("SET reporter_grid=%s\n", r_grid_m);
		set_reporter_grid(r_grid_m);
		free(r_grid_m);
		return true;
	}

	n = sscanf(msg, "SET BFO=%d", &wspr_c.bfo);
	if (n == 1) {
		wprintf("WSPR BFO %d --------------------------------------------------------------\n", wspr_c.bfo);
		return true;
	}

	float f;
	int d;
	n = sscanf(msg, "SET dialfreq=%f cf_offset=%d", &f, &d);
	if (n == 2) {
		w->dialfreq_MHz = f / kHz;
		w->cf_offset = (float) d;
		wprintf("WSPR dialfreq_MHz %.6f cf_offset %.0f -------------------------------------------\n", w->dialfreq_MHz, w->cf_offset);
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
				w->WSPR_FFTtask_id = CreateTaskF(WSPR_FFT, 0, EXT_PRIORITY, CTF_RX_CHANNEL | (rx_chan & CTF_CHANNEL), 0);
				w->WSPR_DecodeTask_id = CreateTaskF(WSPR_Deco, 0, EXT_PRIORITY, CTF_RX_CHANNEL | (rx_chan & CTF_CHANNEL), 0);
				w->create_tasks = true;
			}
			
			w->send_error = false;
			w->reset = TRUE;
			ext_register_receive_iq_samps(wspr_data, rx_chan);
			wprintf("WSPR CAPTURE --------------------------------------------------------------\n");

			wspr_status(w, SYNC, RUNNING);
		} else {
			w->abort_decode = true;
			ext_unregister_receive_iq_samps(w->rx_chan);
			wspr_close(w->rx_chan);
		}
		return true;
	}
	
	return false;
}

static double wspr_cfs[] = {
    137.5, 475.7, 1838.1, 3570.1, 3594.1, 5288.7, 5366.2, 7040.1, 10140.2, 14097.1, 18106.1, 21096.1, 24926.1, 28126.1
};

static struct mg_connection wspr_snd_mc[RX_CHANS], wspr_ext_mc[RX_CHANS];

void wspr_autorun(int which, int idx)
{
	#define WSPR_BFO 750
	#define WSPR_FILTER_BW 300
	double center_freq_kHz = wspr_cfs[idx-1];
    double dial_freq_kHz = center_freq_kHz - WSPR_BFO/1e3;
	double cfo = roundf((center_freq_kHz - floorf(center_freq_kHz)) * 1e3);
	
    struct mg_connection *mc;
    
    mc = &wspr_snd_mc[which];
    mc->connection_param = NULL;
    asprintf((char **) &mc->uri, "%d/SND", 1138+which);
    kiwi_strncpy(mc->remote_ip, "127.0.0.1", NET_ADDRSTRLEN);
    mc->remote_port = mc->local_port = ddns.port;
    conn_t *csnd = rx_server_websocket(WS_INTERNAL_CONN, mc);

    int chan = csnd->rx_channel;
    wspr_t *w = &wspr[chan];
    w->arun_csnd = csnd;
    w->arun_cf_MHz = center_freq_kHz / 1e3;
    w->arun_last_decoded = -1;

	printf("wspr_autorun: which=%d RX%d idx=%d cf=%.2f df=%.2f cfo=%.0f\n", which, chan, idx, center_freq_kHz, dial_freq_kHz, cfo);

    input_msg_internal(csnd, (char *) "SET auth t=kiwi p=");
	input_msg_internal(csnd, (char *) "SET AR OK in=12000 out=44100");
	input_msg_internal(csnd, (char *) "SET agc=1 hang=0 thresh=-100 slope=6 decay=1000 manGain=50");
    input_msg_internal(csnd, (char *) "SET mod=usb low_cut=%d high_cut=%d freq=%.2f",
        WSPR_BFO - WSPR_FILTER_BW/2, WSPR_BFO + WSPR_FILTER_BW/2, dial_freq_kHz);
    input_msg_internal(csnd, (char *) "SET ident_user=WSPR-autorun");
    input_msg_internal(csnd, (char *) "SET geo=0%%20decoded");

    mc = &wspr_ext_mc[which];
    mc->connection_param = NULL;
    asprintf((char **) &mc->uri, "%d/EXT", 1138+which);
    kiwi_strncpy(mc->remote_ip, "127.0.0.1", NET_ADDRSTRLEN);
    mc->remote_port = mc->local_port = ddns.port;
    conn_t *cext = rx_server_websocket(WS_INTERNAL_CONN, mc);
    
    input_msg_internal(cext, (char *) "SET auth t=kiwi p=");
    input_msg_internal(cext, (char *) "SET ext_switch_to_client=wspr first_time=1 rx_chan=%d", chan);
    input_msg_internal(cext, (char *) "SET autorun");
    input_msg_internal(cext, (char *) "SET BFO=%d", WSPR_BFO);
    input_msg_internal(cext, (char *) "SET capture=0");
    input_msg_internal(cext, (char *) "SET dialfreq=%.2f cf_offset=%.0f", dial_freq_kHz, cfo);
    input_msg_internal(cext, (char *) "SET reporter_grid=RF82ci");
    input_msg_internal(cext, (char *) "SET capture=1");

    asprintf(&w->arun_stat_cmd, "curl 'http://wsprnet.org/post?function=wsprstat&rcall=%s&rgrid=%s&rqrg=%.6f&tpct=0&tqrg=%.6f&dbm=0&version=1.3+Kiwi' >/dev/null 2>&1",
        wspr_c.rcall, wspr_c.rgrid, w->arun_cf_MHz, w->arun_cf_MHz);
    //printf("%s\n", w->arun_stat_cmd);
    non_blocking_cmd_system_child("kiwi.wsp", w->arun_stat_cmd, NO_WAIT);
    w->arun_last_status_sent = timer_sec();
}

void wspr_main();

ext_t wspr_ext = {
	"wspr",
	wspr_main,
	wspr_close,
	wspr_msgs,
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

	for (i=0; i < RX_CHANS; i++) {
		wspr_t *w = &wspr[i];
		memset(w, 0, sizeof(wspr_t));
		
		w->medium_effort = 1;
		w->wspr_type = WSPR_TYPE_2MIN;
		
		w->fftin = (WSPR_FFTW_COMPLEX*) WSPR_FFTW_MALLOC(sizeof(WSPR_FFTW_COMPLEX)*NFFT);
		w->fftout = (WSPR_FFTW_COMPLEX*) WSPR_FFTW_MALLOC(sizeof(WSPR_FFTW_COMPLEX)*NFFT);
		w->fftplan = WSPR_FFTW_PLAN_DFT_1D(NFFT, w->fftin, w->fftout, FFTW_FORWARD, FFTW_ESTIMATE);
	
		w->status_resume = IDLE;
		w->tsync = FALSE;
		w->capture = 0;
		w->last_sec = -1;
		w->abort_decode = false;
		w->send_error = false;
	}
	
    wspr_init();

	for (i=0; i < NFFT; i++) {
		window[i] = sin(i * K_PI/(NFFT-1));
	}

	ext_register(&wspr_ext);
    frate = ext_update_get_sample_rateHz(-2);
    double fdecimate = frate / FSRATE;
    int_decimate = SND_RATE / FSRATE;
    wprintf("WSPR %s decimation: srate=%.6f/%d decim=%.6f/%d sps=%d NFFT=%d nbins_411=%d\n", FRACTIONAL_DECIMATION? "fractional" : "integer",
        frate, SND_RATE, fdecimate, int_decimate, SPS, NFFT, nbins_411);
}

#endif

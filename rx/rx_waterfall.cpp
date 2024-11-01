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

// Copyright (c) 2014 John Seamons, ZL4VO/KF6VO
// 24.483 z7 in&out

#include "types.h"
#include "config.h"
#include "kiwi.h"
#include "clk.h"
#include "misc.h"
#include "nbuf.h"
#include "web.h"
#include "spi.h"
#include "gps.h"
#include "coroutines.h"
#include "debug.h"
#include "data_pump.h"
#include "cfg.h"
#include "datatypes.h"
#include "ext_int.h"
#include "rx_noise.h"
#include "noiseproc.h"
#include "dx.h"
#include "non_block.h"
#include "noise_blank.h"
#include "str.h"
#include "mem.h"
#include "rx_waterfall.h"
#include "rx_util.h"
#include "options.h"

#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <sched.h>
#include <math.h>
#include <fftw3.h>

#ifdef USE_SDR

//#define WF_INFO
//#define WF_APER_INFO
#define TR_WF_CMDS      0
#define SM_WF_DEBUG		false
//#define WF_SPEC_INV_DEBUG

//#define SHOW_MAX_MIN_IQ
//#define SHOW_MAX_MIN_PWR
//#define SHOW_MAX_MIN_DB

#define MAX_FFT_USED	MAX(WF_C_NFFT / WF_USING_HALF_FFT, WF_WIDTH)

#define	MAX_START(z)	((WF_WIDTH << MAX_ZOOM) - (WF_WIDTH << (MAX_ZOOM - z)))

#define WF_NSPEEDS 5
static const int wf_fps[] = { WF_SPEED_OFF, WF_SPEED_1FPS, WF_SPEED_SLOW, WF_SPEED_MED, WF_SPEED_FAST };

static const char *interp_s[] = { "max", "min", "last", "drop", "cma" };

#ifdef WF_SHMEM_DISABLE
    static wf_shmem_t wf_shmem;
    wf_shmem_t *wf_shmem_p = &wf_shmem;
#endif

// FIXME: doesn't work yet because currently no way to use SPI from LINUX_CHILD_PROCESS()
//#define WF_IPC_SAMPLE_WF

#if defined(WF_SHMEM_DISABLE) || !defined(WF_IPC_SAMPLE_WF)
    #define WFSleepReasonMsec(r, t) TaskSleepReasonMsec(r, t)
    #define WFSleepReasonUsec(r, t) TaskSleepReasonUsec(r, t)
    #define WFNextTask(r) NextTask(r)
#else
    // WF_IPC_SAMPLE_WF
    #define WFSleepReasonMsec(r, t) kiwi_msleep(t)
    #define WFSleepReasonUsec(r, t) kiwi_usleep(t)
    #define WFNextTask(r)
#endif

#define	WF_OUT_HDR	((int) (sizeof(wf_pkt_t) - sizeof(out->un)))
#define	WF_OUT_NOM	((int) (WF_OUT_HDR + sizeof(out->un.buf)))
		
// if entries here are ordered by wf_cmd_key_e then the reverse lookup (str_hash_t *)->hashes[key].name
// will work as a debugging aid
static str_hashes_t wf_cmd_hashes[] = {
    { "~~~~~~~~~", STR_HASH_MISS },
    { "SET zoom=", CMD_SET_ZOOM },
    { "SET maxdb", CMD_SET_MAX_MIN_DB },
    { "SET cmap=", CMD_SET_CMAP },
    { "SET aper=", CMD_SET_APER },
    { "SET band=", CMD_SET_BAND },
    { "SET scale", CMD_SET_SCALE },
    { "SET wf_sp", CMD_SET_WF_SPEED },
    { "SET send_", CMD_SEND_DB },
    { "SET ext_b", CMD_EXT_BLUR },
    { "SET inter", CMD_INTERPOLATE },
    { "SET windo", CMD_WF_WINDOW_FUNC },
    { 0 }
};

static str_hash_t wf_cmd_hash;

void c2s_waterfall_init()
{
	int i;
	
    wf_cmd_hash.max_hash_len = 9;
    str_hash_init("wf", &wf_cmd_hash, wf_cmd_hashes);

	// do these here, rather than the beginning of c2s_waterfall(), because they take too long
	// and cause the data pump to overrun
	for (i=0; i < MAX_WF_CHANS; i++) {
	    fft_t *fft = &WF_SHMEM->fft_inst[i];
		fft->hw_dft_plan = fftwf_plan_dft_1d(WF_C_NSAMPS, fft->hw_c_samps, fft->hw_fft, FFTW_FORWARD, FFTW_ESTIMATE);
	}
	
	const float adc_scale_decim = powf(2, -16);		// gives +/- 0.5 float samples
	//const float adc_scale_decim = powf(2, -15);		// gives +/- 1.0 float samples

    // window functions (adc_scale is folded in here since it's constant)
	
	//#define WINDOW_GAIN		2.0
	#define WINDOW_GAIN		1.0
	
	for (int winf = 0; winf < N_WF_WINF; winf++) {
        float *window = WF_SHMEM->window_function[winf];

        for (i=0; i < WF_C_NSAMPS; i++) {
            window[i] = adc_scale_decim * WINDOW_GAIN;
        
            switch (winf) {
        
            case WINF_WF_HANNING:
                window[i] *= (0.5 - 0.5 * cos( (K_2PI*i)/(float)(WF_C_NSAMPS-1) ));
                break;
            
            case WINF_WF_HAMMING:
                window[i] *= (0.54 - 0.46 * cos( (K_2PI*i)/(float)(WF_C_NSAMPS-1) ));
                break;

            case WINF_WF_BLACKMAN_HARRIS:
                window[i] *= (0.35875
                    - 0.48829 * cos( (K_2PI*i)/(float)(WF_C_NSAMPS-1) )
                    + 0.14128 * cos( (2.0*K_2PI*i)/(float)(WF_C_NSAMPS-1) )
                    - 0.01168 * cos( (3.0*K_2PI*i)/(float)(WF_C_NSAMPS-1) ));
                break;

            case WINF_WF_NONE: default:
                break;
            }
        }
    }

    // compensates for small droop at top end of waterfall/spectrum display
    //real_printf("WF CIC_comp:\n");
    for (i=0; i < WF_C_NSAMPS; i++) {

        // CIC compensating filter
        const TYPEREAL f = fabs(fmod(TYPEREAL(i)/WF_C_NSAMPS+0.5f, 1.0f) - 0.5f);
        const TYPEREAL p1 = -2.969f;
        const TYPEREAL p2 = 36.26f;
        const TYPEREAL sincf = f ? MSIN(f*K_PI)/(f*K_PI) : 1.0f;
        float cic_comp = pow(sincf, -5) + p1*exp(p2*(f-0.5f));
        WF_SHMEM->CIC_comp[i] = 0.5 + cic_comp / 2.0;   // scaling value is empirical
        //real_printf("[%.3f %.3f] ", cic_comp, WF_SHMEM->CIC_comp[i]);
    }
    //real_printf("\n\n");

	WF_SHMEM->n_chunks = (int) ceilf((float) WF_C_NSAMPS / NWF_SAMPS);
	
	assert(WF_C_NSAMPS == WF_C_NFFT);
	assert(WF_C_NSAMPS <= 8192);	// hardware sample buffer length limitation
	
#ifdef WF_SHMEM_DISABLE
#else
    #ifdef WF_IPC_SAMPLE_WF
        void sample_wf(int rx_chan);
        shmem_ipc_setup("kiwi.waterfall", SIG_IPC_WF, sample_wf);
    #else
        void compute_frame(int rx_chan);
        shmem_ipc_setup("kiwi.waterfall", SIG_IPC_WF, compute_frame);
    #endif
#endif
}

void c2s_waterfall_compression(int rx_chan, bool compression)
{
    //printf("WF%d: compression=%d\n", rx_chan, compression);
	WF_SHMEM->wf_inst[rx_chan].compression = compression;
}

void c2s_waterfall_no_sync(int rx_chan, bool no_sync)
{
    //printf("WF%d: no_sync=%d\n", rx_chan, no_sync);
	WF_SHMEM->wf_inst[rx_chan].no_sync = no_sync;
}

#define	CMD_ZOOM	0x01
#define	CMD_START	0x02
#define	CMD_DB		0x04
#define	CMD_SPEED	0x08
#define	CMD_ALL		(CMD_ZOOM | CMD_START | CMD_DB | CMD_SPEED)


CNoiseProc m_NoiseProc_wf[MAX_RX_CHANS];


void c2s_waterfall_setup(void *param)
{
	conn_t *conn = (conn_t *) param;
	int rx_chan = conn->rx_channel;

	send_msg(conn, SM_WF_DEBUG, "MSG center_freq=%d bandwidth=%d adc_clk_nom=%.0f", (int) ui_srate/2, (int) ui_srate, ADC_CLOCK_NOM);
	send_msg(conn, SM_WF_DEBUG, "MSG kiwi_up=1 rx_chan=%d", rx_chan);       // rx_chan needed by extint_send_extlist() on js side
	extint_send_extlist(conn);

    // If not wanting a wf (!conn->isWF_conn) send wf_chans=0 to force audio FFT to be used.
    // But need to send actual value via wf_chans_real for use elsewhere.
	send_msg(conn, SM_WF_DEBUG, "MSG wf_fft_size=1024 wf_fps=%d wf_fps_max=%d zoom_max=%d rx_chans=%d wf_chans=%d wf_chans_real=%d wf_cal=%d wf_setup",
		WF_SPEED_FAST, WF_SPEED_MAX, MAX_ZOOM, rx_chans, conn->isWF_conn? wf_chans:0, wf_chans, waterfall_cal);
	if (do_gps && !do_sdr) send_msg(conn, SM_WF_DEBUG, "MSG gps");

    dx_last_community_download();
}

void c2s_waterfall(void *param)
{
	conn_t *conn = (conn_t *) param;
	conn->wf_cmd_recv_ok = false;
	rx_common_init(conn);
	int rx_chan = conn->rx_channel;
	wf_inst_t *wf;
	rx_chan_t *rxc = &rx_channels[rx_chan];
	int i, j, k, n;
	//float adc_scale_samps = powf(2, -ADC_BITS);

	bool new_map = false, new_scale_mask = false;
	bool spectral_inversion = kiwi.spectral_inversion;
	int wband=-1, _wband, zoom=-1, _zoom, scale=1, _scale, _speed, cmap, aper, algo, _dvar, _pipe;
	float start=-1, _start, cf, aper_param;
	float samp_wait_us;
	float off_freq, off_freq_inv;
	float HZperStart = ui_srate / (WF_WIDTH << MAX_ZOOM);
	u64_t i_offset;
	int tr_cmds = 0;
	u4_t cmd_recv = 0;
	double adc_clock_corrected = 0;
	u4_t dx_update_seq = 0;
	int wf_cal = waterfall_cal;
	u4_t aper_pan_timer = 0;
	
	wf = &WF_SHMEM->wf_inst[rx_chan];
	memset(wf, 0, sizeof(wf_inst_t));
	wf->conn = conn;
	wf->rx_chan = rx_chan;
	wf->compression = true;
	wf->isWF = (rx_chan < wf_chans && conn->isWF_conn);
	wf->isFFT = !wf->isWF;
    wf->mark = timer_ms();
    wf->prev_start = wf->prev_zoom = -1;
    wf->snd = &snd_inst[rx_chan];

    wf->check_overlapped_sampling = true;
    int n_chunks = WF_SHMEM->n_chunks;
    strncpy(wf->out.id4, "W/F ", 4);

	#define WF_IQ_T 4
	assert(sizeof(iq_t) == WF_IQ_T);
	#if !((NWF_SAMPS * WF_IQ_T) <= SPIBUF_B)
		#error !((NWF_SAMPS * WF_IQ_T) <= SPIBUF_B)
	#endif

	//clprintf(conn, "WF INIT conn: %p mc: %p %s:%d %s\n",
	//	conn, conn->mc, conn->remote_ip, conn->remote_port, conn->mc->uri);

    #if 0
        if (strcmp(conn->remote_ip, "") == 0)
            cprintf(conn, "WF INIT conn: %p mc: %p %s:%d %s\n",
                conn, conn->mc, conn->remote_ip, conn->remote_port, conn->mc->uri);
    #endif

	//evWFC(EC_DUMP, EV_WF, 10000, "WF", "DUMP 10 SEC");
	
	nbuf_t *nb = NULL;

	while (TRUE) {

        // admin spectral inversion setting changed
        if (spectral_inversion != kiwi.spectral_inversion) {
            new_map = wf->new_map = wf->new_map2 = TRUE;
            spectral_inversion = kiwi.spectral_inversion;
        }
		
		// reload freq NCO if adc clock has been corrected
		// reload freq NCO if spectral inversion changed
		if (start >= 0 && zoom != -1 && (adc_clock_corrected != conn->adc_clock_corrected || new_map)) {
			adc_clock_corrected = conn->adc_clock_corrected;
			off_freq = start * HZperStart;
            off_freq_inv = ((float) MAX_START(zoom) - start) * HZperStart;
			i_offset = (u64_t) (s64_t) ((spectral_inversion? off_freq_inv : off_freq) / conn->adc_clock_corrected * pow(2,48));
			i_offset = -i_offset;
			if (wf->isWF)
			    spi_set3(CmdSetWFFreq, rx_chan, (i_offset >> 16) & 0xffffffff, i_offset & 0xffff);
			//printf("WF%d freq updated due to ADC clock correction\n", rx_chan);
		}

		if (nb) web_to_app_done(conn, nb);
		n = web_to_app(conn, &nb);
				
		if (n) {
			char *cmd = nb->buf;
			cmd[n] = 0;		// okay to do this -- see nbuf.c:nbuf_allocq()

    		TaskStat(TSTAT_INCR|TSTAT_ZERO, 0, "cmd");
    		
    		#if 0
                if (strcmp(conn->remote_ip, "") == 0 /* && strcmp(cmd, "SET keepalive") != 0 */)
                    cprintf(conn, "WF <%s> cmd_recv 0x%x/0x%x\n", cmd, cmd_recv, CMD_ALL);
			#endif

			// SECURITY: this must be first for auth check
            bool keep_alive;
			if (rx_common_cmd(STREAM_WATERFALL, conn, cmd, &keep_alive)) {
                if ((conn->ip_trace || (TR_WF_CMDS && tr_cmds < 32)) && !keep_alive) {
                    clprintf(conn, "WF #%02d [rx_common_cmd] <%s> cmd_recv 0x%x/0x%x\n", tr_cmds, cmd, cmd_recv, CMD_ALL);
                    tr_cmds++;
                }
				continue;
			}
			
            if (conn->ip_trace || (TR_WF_CMDS && tr_cmds < 32)) {
                clprintf(conn, "WF #%02d <%s> cmd_recv 0x%x/0x%x\n", tr_cmds, cmd, cmd_recv, CMD_ALL);
                tr_cmds++;
            }

            u2_t key = str_hash_lookup(&wf_cmd_hash, cmd);
            bool did_cmd = false;
            
            if (conn->ext_cmd != NULL)
                conn->ext_cmd(key, cmd, rx_chan);

            switch (key) {

            case CMD_SET_ZOOM: {
                bool zoom_start_chg = false, zoom_chg = false, start_chg = false;
                
                if (kiwi_str_begins_with(cmd, "SET zoom=")) {
                    did_cmd = true;
                    if (sscanf(cmd, "SET zoom=%d start=%f", &_zoom, &_start) == 2) {
                        //cprintf(conn, "WF: zoom=%d/%d start=%.3f(%.1f)\n", _zoom, zoom, _start, _start * HZperStart / kHz);
                        _zoom = CLAMP(_zoom, 0, MAX_ZOOM);
                        float halfSpan_Hz = (ui_srate / (1 << _zoom)) / 2;
                        cf = (_start * HZperStart) + halfSpan_Hz;
                        #ifdef OPTION_HONEY_POT
                            cprintf(conn, "HONEY_POT W/F cf=%.3f\n", cf / kHz);
                        #endif
                        zoom_start_chg = true;
                    } else
                    if (sscanf(cmd, "SET zoom=%d cf=%f", &_zoom, &cf) == 2) {
                        _zoom = CLAMP(_zoom, 0, MAX_ZOOM);
                        float halfSpan_Hz = (ui_srate / (1 << _zoom)) / 2;
                        cf *= kHz;
                        _start = (cf - halfSpan_Hz) / HZperStart;
                        //cprintf(conn, "WF: zoom=%d cf=%.3f start=%.3f halfSpan=%.3f\n", _zoom, cf/kHz, _start * HZperStart / kHz, halfSpan_Hz/kHz);
                        zoom_start_chg = true;
                    }
                }
            
                if (!zoom_start_chg) break;
                conn->freqHz = cf;      // for logging purposes
                
                // changing waterfall resets inactivity timer
                conn_t *csnd = conn_other(conn, STREAM_SOUND);
                if (csnd && conn->freqChangeLatch) {
                    csnd->last_tune_time = timer_sec();
                }
                
                if (zoom != _zoom) {
                    zoom = _zoom;
                    zoom_chg = true;
                    
                    #define CIC1_DECIM 0x0001
                    #define CIC2_DECIM 0x0100
                    u2_t decim, r1, r2;
                    
                    #ifdef USE_WF_NEW
                        // currently 11-levels of zoom: z0-z10, MAX_ZOOM == 10
                        // z0-10: R = 2,4,8,16,32,64,128,256,512,1024,2048 for MAX_ZOOM == 10
                        r1 = zoom + 1;
                        r2 = 1;		// R2 = 1
                        decim = ?;
                    #else
                        // NB: because we only use half of the FFT with CIC can zoom one level less
                        int zm1 = (WF_USING_HALF_CIC == 2)? (zoom? (zoom-1) : 0) : zoom;

                        #ifdef USE_WF_1CIC
                
                            // currently 15-levels of zoom: z0-z14, MAX_ZOOM == 14
                            if (zm1 == 0) {
                                // z0-1: R = 1,1
                                r1 = 0;
                            } else {
                                // z2-14: R = 2,4,8,16,32,64,128,256,512,1k,2k,4k,8k for MAX_ZOOM = 14
                                r1 = zm1;
                            }
                    
                            // hardware limitation
                            assert(r1 >= 0 && r1 <= 15);
                            assert(WF_1CIC_MAXD <= 32768);
                            decim = CIC1_DECIM << r1;
                        #else
                            // currently 15-levels of zoom: z0-z14, MAX_ZOOM == 14
                            if (zm1 == 0) {
                                // z0-1: R = 1 (R1 = R2 = 1)
                                r1 = r2 = 0;
                            } else
                            if (zm1 <= WF_2CIC_POW2) {
                                // z2-8: R = 2,4,8,16,32,64,128 (R1 = 1; R2 = 2,4,8,16,32,64,128)
                                r1 = 0;
                                r2 = zm1;
                            } else {
                                // z9-14: R = 128,256,512,1k,2k,4k (R1 = 2,4,8,16,32,64; R2 = 128)
                                r1 = zm1 - WF_2CIC_POW2;
                                r2 = WF_2CIC_POW2;
                            }
                    
                            // hardware limitation
                            assert(r1 >= 0 && r1 <= 7);
                            assert(r2 >= 0 && r2 <= 7);
                            assert(WF_2CIC_MAXD <= 127);
                            decim = (CIC2_DECIM << r2) | (CIC1_DECIM << r1);
                        #endif
                    #endif
                    
                    samp_wait_us =  WF_C_NSAMPS * (1 << zm1) / conn->adc_clock_corrected * 1000000.0;
                    wf->chunk_wait_us = (int) ceilf(samp_wait_us / n_chunks);
                    wf->samp_wait_ms = (int) ceilf(samp_wait_us / 1000);
                    #ifdef WF_INFO
                    if (!bg) cprintf(conn, "---- WF%d Z%d zm1 %d/%d R%04x n_chunks %d samp_wait_us %.1f samp_wait_ms %d chunk_wait_us %d\n",
                        rx_chan, zoom, zm1, 1<<zm1, decim, n_chunks, samp_wait_us, wf->samp_wait_ms, wf->chunk_wait_us);
                    #endif
                
                    new_map = wf->new_map = wf->new_map2 = TRUE;
                
                    if (wf->nb_enable[NB_BLANKER] && wf->nb_enable[NB_WF]) wf->nb_param_change[NB_BLANKER] = true;
                
                    // when zoom changes reevaluate if overlapped sampling might be needed
                    wf->check_overlapped_sampling = true;
                
                    if (wf->isWF)
                        spi_set(CmdSetWFDecim, rx_chan, decim);
                
                    // We've seen cases where the wf connects, but the sound never does.
                    // So have to check for conn->other being valid.
                    conn_t *csnd = conn_other(conn, STREAM_SOUND);
                    if (csnd) {
                        csnd->zoom = zoom;		// set in the AUDIO conn
                    }
                    conn->zoom = zoom;      // for logging purposes
                
                    //jksd
                    //printf("ZOOM z=%d ->z=%d\n", zoom, wf->zoom);
                    //wf->prev_zoom = (wf->zoom == -1)? zoom : wf->zoom;
                    wf->zoom = zoom;
                    cmd_recv |= CMD_ZOOM;
                }
            
                if (start != _start) start_chg = true;
                start = _start;
                //cprintf(conn, "WF: START %.0f ", start);
                int maxstart = MAX_START(zoom);
                start = CLAMP(start, 0, maxstart);
                
                //printf(" CLAMPED %.0f %.3f\n", start, start * HZperStart / kHz);

                off_freq = start * HZperStart;
                off_freq_inv = ((float) maxstart - start) * HZperStart;
            
                #ifdef USE_WF_NEW
                    #error spectral_inversion
                    off_freq += conn->adc_clock_corrected / (4<<zoom);
                #endif
            
                i_offset = (u64_t) (s64_t) ((spectral_inversion? off_freq_inv : off_freq) / conn->adc_clock_corrected * pow(2,48));
                i_offset = -i_offset;

                #ifdef WF_INFO
                if (!bg) cprintf(conn, "WF z%d OFFSET %.3f kHz i_offset 0x%012llx\n",
                    zoom, off_freq/kHz, i_offset);
                #endif
            
                if (wf->isWF)
                    spi_set3(CmdSetWFFreq, rx_chan, (i_offset >> 16) & 0xffffffff, i_offset & 0xffff);
                //jksd
                //printf("START s=%d ->s=%d\n", start, wf->start);
                //wf->prev_start = (wf->start == -1)? start : wf->start;
                wf->start = start;
                new_scale_mask = true;
                cmd_recv |= CMD_START;
            
                send_msg(conn, SM_NO_DEBUG, "MSG zoom=%d start=%d", zoom, (u4_t) start);
                //printf("waterfall: send zoom %d start %d\n", zoom, u_start);
                //jksd
                //wf->flush_wf_pipe = 6;
                //printf("flush_wf_pipe %d\n", debug_v);
                //wf->flush_wf_pipe = debug_v;
                
                // this also catches "start=" changes from panning
                if (wf->aper == AUTO && start_chg && !zoom_chg) {
                    aper_pan_timer = wf->mark;
                    #ifdef WF_APER_INFO
                        printf("waterfall: start_chg aper_pan_timer=%d\n", aper_pan_timer);
                    #endif
                }
                if (wf->aper == AUTO && zoom_chg) {
                    wf->avg_clear = 1;
                    wf->need_autoscale++;
                    #ifdef WF_APER_INFO
                        printf("waterfall: zoom_chg need_autoscale++=%d algo=%d\n", wf->need_autoscale, wf->aper_algo);
                    #endif
                }
                break;
            }
            
            case CMD_SET_MAX_MIN_DB:
                i = sscanf(cmd, "SET maxdb=%d mindb=%d", &wf->maxdb, &wf->mindb);
                if (i == 2) {
                    did_cmd = true;
                    #ifdef WF_APER_INFO
                        printf("waterfall: maxdb=%d mindb=%d\n", wf->maxdb, wf->mindb);
                    #endif
                    cmd_recv |= CMD_DB;
                }
                break;

            case CMD_SET_CMAP:
                if (sscanf(cmd, "SET cmap=%d", &cmap) == 1) {
                    //waterfall_cmap(conn, wf, cmap);
                    did_cmd = true;
                }
                break;

            case CMD_SET_APER:
                if (sscanf(cmd, "SET aper=%d algo=%d param=%f", &aper, &algo, &aper_param) == 3) {
                    #ifdef WF_APER_INFO
                        printf("### WF n/d/s=%d/%d/%d aper=%d algo=%d param=%f\n",
                            wf->need_autoscale, wf->done_autoscale, wf->sent_autoscale, aper, algo, aper_param);
                    #endif
                    wf->aper = aper;
                    wf->aper_algo = algo;
                    if (aper == AUTO) {
                        wf->aper_param = aper_param;
                        wf->avg_clear = 1;
                        wf->need_autoscale++;
                    }
                    did_cmd = true;
                }
                break;
            
            case CMD_INTERPOLATE:
                if (sscanf(cmd, "SET interp=%d", &wf->interp) == 1) {
                    if ((int) wf->interp >= WF_CIC_COMP) {
                        wf->cic_comp = true;
                        wf->interp = (wf_interp_t) ((int) wf->interp - WF_CIC_COMP);
                    } else {
                        wf->cic_comp = false;
                    }
                    if (wf->interp < 0 || wf->interp > WF_CMA) wf->interp = WF_MAX;
                    cprintf(conn, "WF interp=%d(%s) cic_comp=%d\n", wf->interp, interp_s[wf->interp], wf->cic_comp);
                    did_cmd = true;                
                }
                break;

            case CMD_WF_WINDOW_FUNC:
                if (sscanf(cmd, "SET window_func=%d", &i) == 1) {
                    if (i < 0 || i >= N_WF_WINF) i = 0;
                    wf->window_func = i;
                    cprintf(conn, "WF window_func=%d\n", wf->window_func);
                    did_cmd = true;                
                }
                break;

        #if 0
            // not currently used
            case CMD_SET_BAND:
                i = sscanf(cmd, "SET band=%d", &_wband);
                if (i == 1) {
                    did_cmd = true;
                    //printf("waterfall: band=%d\n", _wband);
                    if (wband != _wband) {
                        wband = _wband;
                        //printf("waterfall: BAND %d\n", wband);
                    }
                }
                break;

            // not currently used
            case CMD_SET_SCALE:
                i = sscanf(cmd, "SET scale=%d", &_scale);
                if (i == 1) {
                    did_cmd = true;
                    //printf("waterfall: scale=%d\n", _scale);
                    if (scale != _scale) {
                        scale = _scale;
                        //printf("waterfall: SCALE %d\n", scale);
                    }
                }
                break;
        #endif

            case CMD_SET_WF_SPEED:
                i = sscanf(cmd, "SET wf_speed=%d", &_speed);
                if (i == 1) {
                    did_cmd = true;
                    //printf("WF wf_speed=%d\n", _speed);
                    if (_speed == -1) _speed = WF_NSPEEDS-1;
                    if (_speed >= 0 && _speed < WF_NSPEEDS)
                        wf->speed = _speed;
                    send_msg(conn, SM_NO_DEBUG, "MSG wf_fps=%d", wf_fps[wf->speed]);
                    cmd_recv |= CMD_SPEED;
                }
                break;

            case CMD_SEND_DB:
                i = sscanf(cmd, "SET send_dB=%d", &wf->send_dB);
                if (i == 1) {
                    did_cmd = true;
                    //cprintf(conn, "WF send_dB=%d\n", wf->send_dB);
                }
                break;

			// FIXME: keep these from happening in the first place?
            case CMD_EXT_BLUR:
                int ch;
                i = sscanf(cmd, "SET ext_blur=%d", &ch);
                if (i == 1) {
                    did_cmd = true;
                }
                break;
            
            default:
                if (conn->mc != NULL)
                    cprintf(conn, "#### WF key=%d DEFAULT CASE <%s>\n", key, cmd);
                did_cmd = true;     // force skip
                break;
 
            }   // switch
            
		    if (did_cmd) continue;

			if (conn->mc != NULL) {
                cprintf(conn, "#### WF hash=0x%04x key=%d \"%s\"\n", wf_cmd_hash.cur_hash, key, cmd);
			    cprintf(conn, "WF BAD PARAMS: sl=%d %d|%d|%d [%s] ip=%s ####################################\n",
			        strlen(cmd), cmd[0], cmd[1], cmd[2], cmd, conn->remote_ip);
			    conn->unknown_cmd_recvd++;
			}
			
			continue;       // keep checking until no cmds in queue
        }

		check(nb == NULL);
		
		if (do_gps && !do_sdr) {
			wf_pkt_t *out = &wf->out;
			int *ns_bin = ClockBins();
			int max=0;
			
			for (n=0; n<1024; n++) if (ns_bin[n] > max) max = ns_bin[n];
			if (max == 0) max=1;
			u1_t *bp = out->un.buf;
			for (n=0; n<1024; n++) {
				*bp++ = (u1_t) (int) (-256 + (ns_bin[n] * 255 / max));	// simulate negative dBm
			}
			int delay = 10000 - (timer_ms() - wf->mark);
			if (delay > 0) TaskSleepReasonMsec("wait frame", delay);
			wf->mark = timer_ms();
			app_to_web(conn, (char*) &out, WF_OUT_NOM);
		}
		
		if (!do_sdr) {
			NextTask("WF skip");
			continue;
		}

		if (conn->stop_data) {
			//clprintf(conn, "WF stop_data rx_server_remove()\n");
			rx_enable(rx_chan, RX_CHAN_FREE);
			rx_server_remove(conn);
			panic("shouldn't return");
		}

		// no keep-alive seen for a while or the bug where the initial cmds are not received and the connection hangs open
		// and locks-up a receiver channel
		conn->keep_alive = timer_sec() - conn->keepalive_time;
		bool keepalive_expired = (conn->keep_alive > KEEPALIVE_SEC);
		bool connection_hang = (conn->keepalive_count > 4 && cmd_recv != CMD_ALL);
		if (keepalive_expired || connection_hang || conn->kick) {
			//if (keepalive_expired) clprintf(conn, "WF KEEP-ALIVE EXPIRED\n");
			//if (connection_hang) clprintf(conn, "WF CONNECTION HANG\n");
			//if (conn->kick) clprintf(conn, "WF KICK\n");
		
			// Ask sound task to stop (must not do while, for example, holding a lock).
			// We've seen cases where the sound connects, then times out. But the wf has never connected.
			// So have to check for conn->other being valid.
			conn_t *csnd = conn_other(conn, STREAM_SOUND);
			if (csnd) {
				csnd->stop_data = TRUE;
			} else {
				rx_enable(rx_chan, RX_CHAN_FREE);		// there is no SND, so free rx_chan[] now
			}
			
			//clprintf(conn, "WF rx_server_remove()\n");
			rx_server_remove(conn);
			panic("shouldn't return");
		}

        // FIXME: until we figure out if no WF cmds are needed when no wf is present just occasionally wake up and check
		if (rx_chan >= wf_num) {
			TaskSleepMsec(500);
			continue;
		}
		
        // Handle LOG_ARRIVED and missing ident for WF-only connections.
        bool too_much = ((cmd_recv & CMD_SET_ZOOM) && (timer_sec() > (conn->arrival + 15)));
        if (conn->isMaster && !conn->arrived && (conn->ident || too_much)) {
            if (!conn->ident)
			    kiwi_str_redup(&conn->ident_user, "user", (char *) "(no identity)");
            rx_loguser(conn, LOG_ARRIVED);
            conn->arrived = TRUE;
        }

		// Don't process any waterfall data until we've received all necessary commands.
		// Also, stop waterfall if speed is zero.
		if (cmd_recv != CMD_ALL || wf->speed == WF_SPEED_OFF) {
			TaskSleepMsec(100);
			continue;
		}
		
		if (!conn->wf_cmd_recv_ok) {
			#ifdef TR_WF_CMDS
				clprintf(conn, "WF cmd_recv ALL 0x%x/0x%x\n", cmd_recv, CMD_ALL);
			#endif
			conn->wf_cmd_recv_ok = true;
		}
		
        if (wf->isFFT) {
            TaskSleepMsec(250);
            continue;
        }
        
        // coalesse the repeated start_chg resulting from a WF pan/scroll
        if (aper_pan_timer && wf->mark > (aper_pan_timer + 1000)) {
            wf->avg_clear = 1;
            wf->need_autoscale++;
            #ifdef WF_APER_INFO
                printf("waterfall: start_chg mark=%d need_autoscale++=%d algo=%d\n", wf->mark, wf->need_autoscale, wf->aper_algo);
            #endif
            aper_pan_timer = 0;
        }
        
		wf->fft_used = WF_C_NFFT / WF_USING_HALF_FFT;		// the result is contained in the first half of a complex FFT
		
		// if any CIC is used (z != 0) only look at half of it to avoid the aliased images
		#ifdef USE_WF_NEW
			wf->fft_used /= WF_USING_HALF_CIC;
		#else
			if (zoom != 0) wf->fft_used /= WF_USING_HALF_CIC;
		#endif
		
		float span = conn->adc_clock_corrected / 2 / (1<<zoom);
		float disp_fs = ui_srate / (1<<zoom);
		
		// NB: plot_width can be greater than WF_WIDTH because it relative to the ratio of the
		// (adc_clock_corrected/2) / ui_srate, which can be > 1 (hence plot_width_clamped).
		// All this is necessary because we might be displaying less than what adc_clock_corrected/2 implies because
		// of using third-party obtained frequency scale images in our UI (this is not currently an issue).
		wf->plot_width = WF_WIDTH * span / disp_fs;
		wf->plot_width_clamped = (wf->plot_width > WF_WIDTH)? WF_WIDTH : wf->plot_width;
		
		if (new_map) {
			assert(wf->fft_used <= MAX_FFT_USED);

            #ifdef USE_WF_NEW
                #define WF_FFT_UNWRAP_NEEDED
            #endif

			wf->fft_used_limit = 0;

			if (wf->fft_used >= wf->plot_width) {
				// FFT >= plot

                #ifdef WF_FFT_UNWRAP_NEEDED
                    // IQ reverse unwrapping (X)
                    #error spectral_inversion
                    for (i=wf->fft_used/2,j=0; i<wf->fft_used; i++,j++) {
                        wf->fft2wf_map[i] = wf->plot_width * j/wf->fft_used;
                    }
                    for (i=0; i<wf->fft_used/2; i++,j++) {
                        wf->fft2wf_map[i] = wf->plot_width * j/wf->fft_used;
                    }
                #else
                    // no unwrap
                    #ifdef WF_SPEC_INV_DEBUG
                        real_printf("$INV NORMAL_SAMPLE SETUP %s z%d fft_used %d plot_width %d\n",
                        spectral_inversion? "REV":"NORM", zoom, wf->fft_used, wf->plot_width);
                    #endif
                    for (i=0; i < wf->fft_used; i++) {
                        j = wf->plot_width * i/wf->fft_used;
                        if (spectral_inversion)
                            j = (j < WF_WIDTH)? (WF_WIDTH-1 - j) : -1;
                        wf->fft2wf_map[i] = j;
                        #ifdef WF_SPEC_INV_DEBUG
                            real_printf("%d|%.2f=%d ", i, (float)i/wf->fft_used, j);
                        #endif
                    }
                #endif
                //for (i=0; i<wf->fft_used; i++) printf("%d>%d ", i, wf->fft2wf_map[i]);

                // Not like above where fft2wf_map[] can map multiple FFT values per pixel for use
                // in e.g. averaging. Only a single value is needed based on plot width due to
                // the fact this is drop sampling.
                int fft_used_inv = roundf((float) wf->fft_used * (wf->plot_width_clamped-1)/wf->plot_width);
                #ifdef WF_SPEC_INV_DEBUG
                    if (wf->interp == WF_DROP) 
                        real_printf("$INV DROP_SAMPLE SETUP %s z%d fft_used %d plot_width_clamped %d fft_used_inv %d\n",
                            spectral_inversion? "REV":"NORM", zoom, wf->fft_used, wf->plot_width_clamped, fft_used_inv);
                #endif
                for (i=0; i < wf->plot_width_clamped; i++) {
                    j = roundf((float) wf->fft_used * i/wf->plot_width);
                    if (spectral_inversion) j = fft_used_inv - j;
                    #ifdef WF_SPEC_INV_DEBUG
                        if (wf->interp == WF_DROP) 
                            real_printf("%d|%.2f=%d ", i, (float)i/wf->plot_width, j);
                    #endif
                    wf->drop_sample[i] = j;
                }

                #ifdef WF_SPEC_INV_DEBUG
                    real_printf("\n\n");
                    wf->trigger = true;
                #endif
			} else {
				// FFT < plot

				#ifdef WF_FFT_UNWRAP_NEEDED
					#error spectral_inversion
					for (i=0; i<wf->plot_width_clamped; i++) {
						int t = wf->fft_used * i/wf->plot_width;
						if (t >= wf->fft_used/2) t -= wf->fft_used/2; else t += wf->fft_used/2;
						wf->wf2fft_map[i] = t;
					}
				#else
					// no unwrap
					for (i=0; i<wf->plot_width_clamped; i++) {
					    j = spectral_inversion? (wf->plot_width-1 - i) : i;
						wf->wf2fft_map[i] = wf->fft_used * j/wf->plot_width;
					}
				#endif
				//for (i=0; i<wf->plot_width_clamped; i++) printf("%d:%d ", i, wf->wf2fft_map[i]);
			}
			//printf("\n");
			
			#ifdef WF_INFO
            if (!bg)
                cprintf(conn, "WF NEW_MAP z%d i%d cic%d fft_used %d/%d span %.1f disp_fs %.1f plot_width %d/%d FFT %s plot\n",
                    zoom, wf->interp, wf->cic_comp, wf->fft_used, WF_C_NFFT, span/kHz, disp_fs/kHz, wf->plot_width_clamped, wf->plot_width,
                    (wf->plot_width_clamped < wf->fft_used)? ">=":"<");
			#endif
			
			//send_msg(conn, SM_NO_DEBUG, "MSG plot_width=%d", wf->plot_width);
			new_map = FALSE;
		}
		
        // admin requested that all clients get updated cfg (e.g. admin changed dx type menu)
        if (rxc->cfg_update_seq != cfg_cfg.update_seq) {
            rxc->cfg_update_seq = cfg_cfg.update_seq;
            rx_server_send_config(conn);
        }

        // get client to request updated dx list because admin edited masked list
        // or made any other change to dx label list
		if (dx_update_seq != dx.update_seq) {
            send_msg(conn, false, "MSG request_dx_update");
		    dx_update_seq = dx.update_seq;
		    new_scale_mask = true;
		}
		
		// forward admin changes of waterfall cal to client side
		if (waterfall_cal != wf_cal) {
            send_msg(conn, false, "MSG wf_cal=%d", waterfall_cal);
		    wf_cal = waterfall_cal;
		}
		
		if (new_scale_mask) {
			// FIXME: Is this right? Why is this so strange?
			float fft_scale;
			float maxmag = zoom? wf->fft_used : wf->fft_used/2;
			//jks
			//fft_scale = 20.0 / (maxmag * maxmag);
			//wf->fft_offset = 0;
			
			// makes GEN attn 0 dB = 0 dBm
			fft_scale = 5.0 / (maxmag * maxmag);
			wf->fft_offset = zoom? -0.08 : -0.8;
			
			// fixes GEN z0/z1+ levels, but breaks regular z0/z1+ noise floor continuity -- why?
			//float maxmag = zoom? wf->fft_used : wf->fft_used/4;
			//fft_scale = (zoom? 2.0 : 5.0) / (maxmag * maxmag);
			
			// apply masked frequencies
			if (dx.masked_len != 0 && !(conn->other != NULL && conn->other->tlimit_exempt_by_pwd)) {
                for (i=0; i < wf->plot_width_clamped; i++) {
                    float scale = fft_scale;
                    int f = roundf((wf->start + (i << (MAX_ZOOM - zoom))) * HZperStart);
                    for (j=0; j < dx.masked_len; j++) {
                        dx_mask_t *dmp = &dx.masked_list[j];
                        //cprintf(conn, "MASKED %.2f|%.2f|%.2f %s\n", dmp->masked_lo/1e3, f/1e3, dmp->masked_hi/1e3, (f >= dmp->masked_lo && f <= dmp->masked_hi)? "Y":"N");
                        if (f >= dmp->masked_lo && f <= dmp->masked_hi) {
                            scale = 0;
                            break;
                        }
                    }
                    wf->fft_scale[i] = scale;
                    wf->fft_scale_div2[i] = scale / 2;
                }
			} else {
                for (i=0; i < wf->plot_width_clamped; i++) {
                    wf->fft_scale[i] = fft_scale;
                    wf->fft_scale_div2[i] = fft_scale / 2;
                }
			}

		    new_scale_mask = false;
		}

        void sample_wf(int rx_chan);
        #ifdef WF_SHMEM_DISABLE
            sample_wf(rx_chan);
        #else
            #ifdef WF_IPC_SAMPLE_WF
                shmem_ipc_invoke(SIG_IPC_WF, wf->rx_chan);      // invoke sample_wf()
            #else
                sample_wf(rx_chan);
            #endif
        #endif
	}
}

void sample_wf(int rx_chan)
{
	wf_inst_t *wf = &WF_SHMEM->wf_inst[rx_chan];
    int k;
    fft_t *fft = &WF_SHMEM->fft_inst[rx_chan];
    u4_t now, diff;
    u64_t now64, deadline;
    
    #ifdef SHOW_MAX_MIN_IQ
        static void *IQi_state;
        static void *IQf_state;
        if (wf->new_map2) {
            if (IQi_state != NULL) kiwi_ifree(IQi_state, "wf iq");
            IQi_state = NULL;
            if (IQf_state != NULL) kiwi_ifree(IQf_state, "wf iq");
            IQf_state = NULL;
        }
    #endif

    // create waterfall
    
    assert(wf_fps[wf->speed] != 0);
    int desired = 1000 / wf_fps[wf->speed];

    // desired frame rate greater than what full sampling can deliver, so start overlapped sampling
    if (wf->check_overlapped_sampling) {
        wf->check_overlapped_sampling = false;

        if (wf->samp_wait_ms >= 2*desired) {
            wf->overlapped_sampling = true;
            
            #ifdef WF_INFO
            if (!bg) printf("---- WF%d OLAP z%d samp_wait %d >= %d(2x) desired %d\n",
                rx_chan, wf->zoom, wf->samp_wait_ms, 2*desired, desired);
            #endif
            
            evWFC(EC_TRIG1, EV_WF, -1, "WF", "OVERLAPPED CmdWFReset");
            spi_set(CmdWFReset, rx_chan, WF_SAMP_RD_RST | WF_SAMP_WR_RST | WF_SAMP_CONTIN);
            WFSleepReasonMsec("fill pipe", wf->samp_wait_ms+1);		// fill pipeline
        } else {
            wf->overlapped_sampling = false;

            #ifdef WF_INFO
            if (!bg) printf("---- WF%d NON-OLAP z%d samp_wait %d < %d(2x) desired %d\n",
                rx_chan, wf->zoom, wf->samp_wait_ms, 2*desired, desired);
            #endif
        }
    }
    
    SPI_CMD first_cmd;
    if (wf->overlapped_sampling) {
        //
        // Start reading immediately at synchronized write address plus a small offset to get to
        // the old part of the buffer.
        // This presumes zoom factor isn't so large that buffer fills too slowly and we
        // overrun reading the last little bit (offset part). Also presumes that we can read the
        // first part quickly enough that the write doesn't catch up to us.
        //
        // CmdGetWFContSamps asserts WF_SAMP_SYNC | WF_SAMP_CONTIN in kiwi.sdr.asm code
        //
        first_cmd = CmdGetWFContSamps;
    } else {
        evWFC(EC_TRIG1, EV_WF, -1, "WF", "NON-OVERLAPPED CmdWFReset");
        spi_set(CmdWFReset, rx_chan, WF_SAMP_RD_RST | WF_SAMP_WR_RST);
        deadline = timer_us64() + wf->chunk_wait_us*2;
        first_cmd = CmdGetWFSamples;
    }

    SPI_MISO *miso = &SPI_SHMEM->wf_miso[rx_chan];
    s4_t ii, qq;
    iq_t *iqp;

    int chunk, sn;
    int n_chunks = WF_SHMEM->n_chunks;

    for (chunk=0, sn=0; sn < WF_C_NSAMPS; chunk++) {
        assert(chunk < n_chunks);

        if (wf->overlapped_sampling) {
            evWF(EC_TRIG1, EV_WF, -1, "WF", "CmdGetWFContSamps");
        } else {
            // wait until current chunk is available in WF sample buffer
            now64 = timer_us64();
            if (now64 < deadline) {
                diff = deadline - now64;
                if (diff) {
                    evWF(EC_EVENT, EV_WF, -1, "WF", "TaskSleep wait chunk buffer");
                    WFSleepReasonUsec("wait chunk", diff);
                    evWF(EC_EVENT, EV_WF, -1, "WF", "TaskSleep wait chunk buffer done");
                }
            }
            deadline += wf->chunk_wait_us;
        }
    
        if (chunk == 0) {
            spi_get_noduplex(first_cmd, miso, NWF_SAMPS * sizeof(iq_t), rx_chan);
        } else
        if (chunk < n_chunks-1) {
            spi_get_noduplex(CmdGetWFSamples, miso, NWF_SAMPS * sizeof(iq_t), rx_chan);
        }

        evWFC(EC_EVENT, EV_WF, -1, "WF", evprintf("%s SAMPLING chunk %d",
            wf->overlapped_sampling? "OVERLAPPED":"NON-OVERLAPPED", chunk));
        
        iqp = (iq_t*) &(miso->word[0]);
	    float *window = WF_SHMEM->window_function[wf->window_func];
        
        for (k=0; k<NWF_SAMPS; k++) {
            if (sn >= WF_C_NSAMPS) break;
            ii = (s4_t) (s2_t) iqp->i;
            qq = (s4_t) (s2_t) iqp->q;
            iqp++;

            float fi = ((float) ii) * window[sn];
            float fq = ((float) qq) * window[sn];
            
            #ifdef SHOW_MAX_MIN_IQ
                print_max_min_stream_i(&IQi_state, P_MAX_MIN_DEMAND, "IQi", k, 2, ii, qq);
                print_max_min_stream_f(&IQf_state, P_MAX_MIN_DEMAND, "IQf", k, 2, (double) fi, (double) fq);
            #endif
            
            fft->hw_c_samps[sn][I] = fi;
            fft->hw_c_samps[sn][Q] = fq;
            sn++;
        }
    }

    #ifndef EV_MEAS_WF
        static int wf_cnt;
        evWFC(EC_EVENT, EV_WF, -1, "WF", evprintf("WF %d: loop done", wf_cnt));
        wf_cnt++;
    #endif

    if (wf->nb_enable[NB_CLICK]) {
        now = timer_sec();
        if (now != wf->last_noise_pulse) {
            wf->last_noise_pulse = now;
            TYPEREAL pulse = wf->nb_param[NB_CLICK][NB_PULSE_GAIN] * 0.49;
            for (int i=0; i < wf->nb_param[NB_CLICK][NB_PULSE_SAMPLES]; i++) {
                fft->hw_c_samps[i][I] = pulse;
                fft->hw_c_samps[i][Q] = 0;
            }
        }
    }

    if (wf->nb_enable[NB_BLANKER] && wf->nb_enable[NB_WF]) {
        if (wf->nb_param_change[NB_BLANKER]) {
            //u4_t srate = round(conn->adc_clock_corrected) / (1 << (zoom+1));
            u4_t srate = WF_C_NSAMPS;
            //printf("NB WF sr=%d usec=%.0f th=%.0f\n", srate, wf->nb_param[NB_BLANKER][0], wf->nb_param[NB_BLANKER][1]);
            m_NoiseProc_wf[rx_chan].SetupBlanker("WF", srate, wf->nb_param[NB_BLANKER]);
            wf->nb_param_change[NB_BLANKER] = false;
            wf->nb_setup = true;
        }

        if (wf->nb_setup)
            m_NoiseProc_wf[rx_chan].ProcessBlankerOneShot(WF_C_NSAMPS, (TYPECPX*) fft->hw_c_samps, (TYPECPX*) fft->hw_c_samps);
    }

    // contents of WF DDC pipeline is uncertain when mix freq or decim just changed
    //jksd
    //if (wf->flush_wf_pipe) {
    //	wf->flush_wf_pipe--;
    //} else {
        void compute_frame(int rx_chan);
        #ifdef WF_SHMEM_DISABLE
            compute_frame(rx_chan);
        #else
            #ifdef WF_IPC_SAMPLE_WF
                compute_frame(rx_chan);
            #else
                shmem_ipc_invoke(SIG_IPC_WF, rx_chan);      // invoke compute_frame()
            #endif
        #endif

        #ifndef WF_IPC_SAMPLE_WF
            if (wf->aper == AUTO && wf->done_autoscale > wf->sent_autoscale) {
                wf->sent_autoscale++;

                if (wf->last_noise != wf->noise || wf->last_signal != wf->signal) {
                    #ifdef WF_APER_INFO
                        printf("### SENT d/s=%d/%d algo=%d %d:%d\n",
                            wf->done_autoscale, wf->sent_autoscale, wf->aper_algo, wf->noise, wf->signal);
                    #endif
                    send_msg(wf->conn, false, "MSG maxdb=%d", wf->signal);
                    send_msg(wf->conn, false, "MSG mindb=%d", wf->noise);
                    wf->last_noise = wf->noise;
                    wf->last_signal = wf->signal;
                } else {
                    #ifdef WF_APER_INFO
                        printf("### SAME algo=%d %d:%d\n", wf->aper_algo, wf->noise, wf->signal);
                    #endif
                }

                if (wf->aper_algo != OFF) {
                    wf->need_autoscale++;   // go again
                    #ifdef WF_APER_INFO
                        printf("waterfall: SENT need_autoscale++=%d algo=%d\n", wf->need_autoscale, wf->aper_algo);
                    #endif
                }
            }
        #endif
        
        wf_pkt_t *out = &wf->out;
        app_to_web(wf->conn, (char*) out, WF_OUT_HDR + wf->out_bytes);
        waterfall_bytes[rx_chan] += wf->out_bytes;
        waterfall_bytes[rx_chans] += wf->out_bytes; // [rx_chans] is the sum of all waterfalls
        waterfall_frames[rx_chan]++;
        waterfall_frames[rx_chans]++;       // [rx_chans] is the sum of all waterfalls
        wf->waterfall_frames++;
        evWF(EC_EVENT, EV_WF, -1, "WF", "compute_frame: done");
    
        #if 0
            static u4_t last_time[MAX_RX_CHANS];
            now = timer_ms();
            printf("WF%d: %d %.3fs seq-%d\n", rx_chan, WF_OUT_HDR + wf->out_bytes,
                (float) (now - last_time[rx_chan]) / 1e3, wf->out.seq);
            last_time[rx_chan] = now;
        #endif
    //}

    now = timer_ms();
    int actual = now - wf->mark;
    int delay = desired - actual;
    //printf("%d %d %d\n", delay, actual, desired);
    
    // full sampling faster than needed by frame rate
    if (desired > actual) {
        evWF(EC_EVENT, EV_WF, -1, "WF", "TaskSleep wait FPS");
        WFSleepReasonMsec("wait frame", delay);
        evWF(EC_EVENT, EV_WF, -1, "WF", "TaskSleep wait FPS done");
    } else {
        WFNextTask("loop");
    }
    now = timer_ms();
    wf->mark = now;
    diff = now - wf->last_frames_ms;
    if (diff > 10000) {
        float secs = (float) diff / 1e3;
        int fps = (int) roundf((float) wf->waterfall_frames / secs);
        TaskStat2(TSTAT_SET, fps, "fps");
        wf->waterfall_frames = 0;
        wf->last_frames_ms = now;
    }
}

static void aperture_auto(wf_inst_t *wf, u1_t *bp)
{
    int i, j, rx_chan = wf->rx_chan;
    if (wf->need_autoscale <= wf->done_autoscale) return;
    bool single_shot = (wf->aper_algo == OFF);

    // FIXME: for audio FFT needs to be further limited to just passband
    int start = 0, stop = APER_PWR_LEN, len;
    if (rx_chan >= wf_chans) start = 256, stop = 768;   // audioFFT
    
    if (wf->avg_clear) {
        for (i = start; i < stop; i++)
            wf->avg_pwr[i] = dB_wire_to_dBm(bp[i]);
        wf->report_sec = timer_sec();
        wf->last_noise = wf->last_signal = -1;
        wf->avg_clear = 0;
    } else {
        int algo = wf->aper_algo;
        float param = wf->aper_param;
        
        if (single_shot) {
            algo = MMA;
            param = 8.0;
        }
        
        switch (algo) {
        
        case IIR:
            for (i = start; i < stop; i++) {
                float pwr = dB_wire_to_dBm(bp[i]);
                float iir_gain = 1.0 - expf(-param * pwr/255.0);
                if (iir_gain <= 0.01) iir_gain = 0.01;  // enforce minimum decay rate
                wf->avg_pwr[i] += (pwr - wf->avg_pwr[i]) * iir_gain;
            }
            break;

        case MMA:
            for (i = start; i < stop; i++) {
                float pwr = dB_wire_to_dBm(bp[i]);
                wf->avg_pwr[i] = ((wf->avg_pwr[i] * (param-1)) + pwr) / param;
            }
            break;

        case EMA:
            for (i = start; i < stop; i++) {
                float pwr = dB_wire_to_dBm(bp[i]);
                wf->avg_pwr[i] += (pwr - wf->avg_pwr[i]) / param;
            }
            break;
        }
    }
    
    u4_t now = timer_sec();
    int delay = single_shot? 1 : 3;
    if (now < wf->report_sec + delay) return;
    wf->report_sec = now;

    wf->done_autoscale++;

    #define RESOLUTION_dB 5
    int band[APER_PWR_LEN];
    len = 0;
    for (i = start; i < stop; i++) {
        int b = ((int) floorf(wf->avg_pwr[i] / RESOLUTION_dB)) * RESOLUTION_dB;
        if (b <= -190) continue;    // disregard masked areas
        band[len] = b;
        len++;
    }
    
    int max_count = 0, max_dBm = -999, min_dBm;
    if (len) {
        qsort(band, len, sizeof(int), qsort_intcomp);
        int last = band[0], same = 0;

        for (i = 0; i <= len; i++) {
            if (i == len || band[i] != last) {
                #ifdef WF_APER_INFO
                    //printf("%4d: %d\n", last, same);
                #endif
                if (same > max_count) max_count = same, min_dBm = last;
                if (last > max_dBm) max_dBm = last;
                if (i == len) break;
                same = 1;
                last = band[i];
            } else {
                same++;
            }
        }
    } else {
        max_dBm = -110;
        min_dBm = -120;
    }

    #ifdef WF_APER_INFO
        printf("### APER_AUTO n/d=%d/%d rx%d algo=%d max_count=%d dBm=%d:%d\n",
            wf->need_autoscale, wf->done_autoscale, rx_chan, wf->aper_algo, max_count, min_dBm, max_dBm);
    #endif
    if (max_dBm < -80) max_dBm = -80;
    wf->signal = max_dBm;   // headroom applied on js side
    wf->noise = min_dBm;
}

void compute_frame(int rx_chan)
{
	wf_inst_t *wf = &WF_SHMEM->wf_inst[rx_chan];
	int i;
	wf_pkt_t *out = &wf->out;
	float pwr[MAX_FFT_USED];
    fft_t *fft = &WF_SHMEM->fft_inst[rx_chan];
    
    // don't use compression for zoom level zero because of bad interaction
    // of narrow strong carriers with compression algorithm
    bool use_compression = (wf->compression && wf->zoom != 0);
		
    //TaskStat2(TSTAT_INCR|TSTAT_ZERO, 0, "frm");

	//NextTask("FFT1");
	evWF(EC_EVENT, EV_WF, -1, "WF", "compute_frame: FFT start");
	fftwf_execute(fft->hw_dft_plan);
	evWF(EC_EVENT, EV_WF, -1, "WF", "compute_frame: FFT done");
	//NextTask("FFT2");

	u1_t *buf_p = use_compression? out->un.buf2 : out->un.buf;
	u1_t *bp = buf_p;
			
	if (!wf->fft_used_limit) wf->fft_used_limit = wf->fft_used;

	// zero-out the DC component in lowest bins (around -90 dBFS)
	// otherwise when scrolling wf it will move but then not continue at the new location
	// Blackman-Harris window DC component is a little wider for z=0
	int bin_dc_offset = (wf->zoom == 0 && wf->window_func == WINF_WF_BLACKMAN_HARRIS)? 4:2;    
	for (i = 0; i < bin_dc_offset; i++) pwr[i] = 0;
	
	#ifdef SHOW_MAX_MIN_PWR
        static void *FFT_state;
        static void *pwr_state;
        static void *dB_state;
        static void *buf_state;
        if (wf->new_map2) {
            if (FFT_state != NULL) kiwi_ifree(FFT_state, "wf minmax");
            FFT_state = NULL;
            if (pwr_state != NULL) kiwi_ifree(pwr_state, "wf minmax");
            pwr_state = NULL;
            if (dB_state != NULL) kiwi_ifree(dB_state, "wf minmax");
            dB_state = NULL;
            if (buf_state != NULL) kiwi_ifree(buf_state, "wf minmax");
            buf_state = NULL;
            wf->new_map2 = false;
        }
	#endif

    if (wf->zoom <= 1) {    // don't apply compensation when CIC not in use
        for (i = bin_dc_offset; i < wf->fft_used_limit; i++) {
            float re = fft->hw_fft[i][I], im = fft->hw_fft[i][Q];
            pwr[i] = re*re + im*im;

            #ifdef SHOW_MAX_MIN_PWR
                //print_max_min_stream_f(&FFT_state, P_MAX_MIN_DEMAND, "FFT", i, 2, (double) re, (double) im);
                //print_max_min_stream_f(&pwr_state, P_MAX_MIN_DEMAND, "pwr", i, 1, (double) pwr[i]);
            #endif
        }
    } else {
        bool no_cic_comp = (wf->overlapped_sampling || !wf->cic_comp);
        for (i = bin_dc_offset; i < wf->fft_used_limit; i++) {
            float re, im;
            if (no_cic_comp) {
                re = fft->hw_fft[i][I]; im = fft->hw_fft[i][Q];
            } else {
                float comp = WF_SHMEM->CIC_comp[i];
                re = ((float) fft->hw_fft[i][I]) * comp; im = ((float) fft->hw_fft[i][Q]) * comp;
            }
            pwr[i] = re*re + im*im;

            #ifdef SHOW_MAX_MIN_PWR
                //print_max_min_stream_f(&FFT_state, P_MAX_MIN_DEMAND, "FFT", i, 2, (double) re, (double) im);
                //print_max_min_stream_f(&pwr_state, P_MAX_MIN_DEMAND, "pwr", i, 1, (double) pwr[i]);
            #endif
        }
    }
		
	// fixme proper power-law scaling..
	
	// from the tutorials at http://www.fourier-series.com/fourierseries2/flash_programs/DFT_windows/index.html
	// recall:
	// pwr = mag*mag			
	// pwr = i*i + q*q
	// mag = sqrt(i*i + q*q) = sqrt(pwr)
	// pwr = mag*mag = i*i + q*q
	// pwr(dB) = 10 * log10(pwr)		i.e. no sqrt() needed
	// mag[amp](dB) = 20 * log10(mag[ampl])
	// pwr gain = pow10(db/10)
	// mag[ampl] gain = pow10(db/20)
	// *** mag_db == pwr_db ***
	
	// with 'bc -l', l() = log base e
	// log10(n) = l(n)/l(10)
	// 10^x = e^(x*ln(10)) = e(x*l(10))
	
    #ifdef WF_INFO
	    float max_dB = wf->maxdb;
	    float min_dB = wf->mindb;
	    float range_dB = max_dB - min_dB;
	    float pix_per_dB = 255.0 / range_dB;
	#endif

	int bin=0, _bin=-1;
	float p, dB;

    float pwr_out[WF_WIDTH];
    memset(pwr_out, 0, sizeof(pwr_out));

    u1_t cma_avgs[WF_WIDTH];
    if (wf->interp == WF_CMA) memset(cma_avgs, 0, sizeof(cma_avgs));
	
	#define LTRIG 0
	//#define LTRIG 300
	#if LTRIG
	    static int dbg_bin;
	    dbg_bin++;
        if (dbg_bin == LTRIG) {
            real_printf("bins:\n");
            for (i=0; i<wf->fft_used_limit; i++)
                real_printf("%d ", bin);
            real_printf("\n");
        }
    #endif

	if (wf->fft_used >= wf->plot_width) {
		// FFT >= plot
		
		if (wf->interp == WF_DROP) {
            #ifdef WF_SPEC_INV_DEBUG
                if (wf->trigger) {
                    real_printf("$INV DROP_SAMPLE TRIG %s plot_width_clamped=%d\n",
                    kiwi.spectral_inversion? "REV":"NORM", wf->plot_width_clamped);
                }
            #endif
            
            for (i=0; i < wf->plot_width_clamped; i++) {
                #ifdef WF_SPEC_INV_DEBUG
                    if (wf->trigger) real_printf("%d|%d ", i, wf->drop_sample[i]);
                #endif
                pwr_out[i] = pwr[wf->drop_sample[i]];
            }
		} else {
            #ifdef WF_SPEC_INV_DEBUG
                if (wf->trigger) {
                    real_printf("$INV NORMAL_SAMPLE TRIG %s fft_used_limit=%d\n",
                    kiwi.spectral_inversion? "REV":"NORM", wf->fft_used_limit);
                }
            #endif
            
            for (i=0; i < wf->fft_used_limit; i++) {
                p = pwr[i];
                bin = wf->fft2wf_map[i];
                if (bin >= WF_WIDTH || bin < 0) {
                    if (wf->new_map) {
            
                        #ifdef WF_INFO
                        if (!bg) printf(">= FFT: Z%d WF_C_NSAMPS %d i %d fft_used %d plot_width %d pix_per_dB %.3f range %.0f:%.0f\n",
                            wf->zoom, WF_C_NSAMPS, i, wf->fft_used, wf->plot_width, pix_per_dB, max_dB, min_dB);
                        #endif
                        wf->new_map = FALSE;
                    }
                    #ifdef WF_SPEC_INV_DEBUG
                        if (wf->trigger) real_printf("###%d### ", i);
                    #endif
                    
                    wf->fft_used_limit = i;		// we now know what the limit is
                    break;
                }
                #ifdef WF_SPEC_INV_DEBUG
                    if (wf->trigger) real_printf("%d|%d ", i, bin);
                #endif
            
                #if LTRIG
                    if (dbg_bin == LTRIG)
                        real_printf("%d:%.0f(%.1e) ", bin, dB_fast(p * wf->fft_scale[0]) + wf->fft_offset, p);
                #endif

                if (bin == _bin) {

                    // Using the max or min value when multiple FFT values per bin gives low-level artifacts!
                    switch (wf->interp) {
                        case WF_CMA:    pwr_out[bin] += p; cma_avgs[bin]++; break;
                        case WF_MAX:    if (p > pwr_out[bin]) pwr_out[bin] = p; break; // gives dark lines
                        case WF_MIN:    if (p < pwr_out[bin]) pwr_out[bin] = p; break; // gives bright lines
                        case WF_LAST:   pwr_out[bin] = p; break;  // using last value (random min/max) seems okay
                        default:        break;
                    }
                } else {
                    if (wf->interp == WF_CMA) {
                        pwr_out[bin] = p;
                        cma_avgs[bin] = 1;
                    } else {
                        pwr_out[bin] = p;
                    }
                    _bin = bin;
                }
            }
        }

		#ifdef SHOW_MAX_MIN_DB
            float dBs_f[WF_WIDTH];
            int dBs[WF_WIDTH];
		#endif

        #if LTRIG
            if (dbg_bin == LTRIG) {
                real_printf("\n\navgs:\n");
                for (i=0; i<WF_WIDTH; i++)
                    real_printf("%d ", cma_avgs[i]);
                real_printf("\n\nout:\n");
            }
        #endif

		for (i=0; i<WF_WIDTH; i++) {
            #if 0
                if (dbg_bin == LTRIG)
                    real_printf("%.6e ", wf->fft_scale[i]);
            #endif

            float scale;
            if (wf->interp == WF_CMA) {
			    // A fft2wf_map[] value causing two FFT values to average into one bin is common.
			    // So we fold avgs == 2 into wf->fft_scale_div2 computed earlier and save the divide.
			    int avgs = cma_avgs[i];
			    scale = (avgs == 1)? wf->fft_scale[i] : ((avgs == 2)? wf->fft_scale_div2[i] : (wf->fft_scale[i] / avgs));
                p = pwr_out[i];
			} else {
			    scale = wf->fft_scale[i];
                p = pwr_out[i];
			}

			dB = dB_fast(p * scale) + wf->fft_offset;

            #if LTRIG
                if (dbg_bin == LTRIG)
                    real_printf("%d:%.0f ", i, dB);
            #endif

			#if 0
                if (i >= 300 && i <= 400 && (i%5) == 0) dB = -110;
            #endif
			#if 0
                if (i == 508) printf("Z%d ", wf->zoom);
                if (i >= 509 && i <= 513) printf("%d ", (int) dB);
                if (i == 514) printf("\n");
            #endif
            #if 0
                //jks
                if (i == 506) printf("Z%d ", wf->zoom);
                if (i >= 507 && i <= 515) {
                    float peak_dB = dB_fast(pwr_out[i] * wf->fft_scale[i]) + wf->fft_offset;
                    printf("%d:%.1f|%.1f ", i, dB, peak_dB);
                }
                if (i == 516) printf("\n");
            #endif

			#ifdef SHOW_MAX_MIN_DB
			    dBs_f[i] = dB;
			#endif
			#ifdef SHOW_MAX_MIN_PWR
			    print_max_min_stream_f(&dB_state, P_MAX_MIN_DEMAND, "dB", i, 1, (double) dB);
			#endif

			// We map 0..-200 dBm to (u1_t) 255..55
			// If we map it the reverse way, (u1_t) 0..255 => 0..-255 dBm (which is more natural), then the
			// noise in the bottom bits due to the ADPCM compression will effect the high-order dBm bits
			// which is bad.
			if (dB > 0) dB = 0;
			if (dB < -200.0) dB = -200.0;
			dB--;
			*bp++ = (u1_t) (int) dB;

			#ifdef SHOW_MAX_MIN_DB
			    dBs[i] = *(bp-1);
			#endif
			#ifdef SHOW_MAX_MIN_PWR
			    print_max_min_stream_i(&buf_state, P_MAX_MIN_DEMAND, "buf", i, 1, (int) *(bp-1));
			#endif
		}

        #if LTRIG
            if (dbg_bin == LTRIG)
                real_printf("\n\n");
        #endif

		#ifdef SHOW_MAX_MIN_DB
            //jks
            printf("Z%d dB_f: ", wf->zoom);
            for (i=505; i<514; i++) {
                printf("%d:%.3f ", i, dBs_f[i]);
            }
            printf("\n");
            print_max_min_f("dB_f", dBs_f, WF_WIDTH);
            printf("Z%d dB: ", wf->zoom);
            for (i=505; i<514; i++) {
                printf("%d:%d ", i, dBs[i]);
            }
            printf("\n");
            print_max_min_i("dB", dBs, WF_WIDTH);
		#endif
	} else {
		// FFT < plot
		if (wf->new_map) {
			//printf("< FFT: Z%d WF_C_NSAMPS %d fft_used %d plot_width_clamped %d pix_per_dB %.3f range %.0f:%.0f\n",
			//	wf->zoom, WF_C_NSAMPS, wf->fft_used, wf->plot_width_clamped, pix_per_dB, max_dB, min_dB);
			wf->new_map = FALSE;
		}

		for (i=0; i<wf->plot_width_clamped; i++) {
			p = wf->wf2fft_map[i];
			
			dB = dB_fast(p * wf->fft_scale[i]) + wf->fft_offset;
			if (dB > 0) dB = 0;
			if (dB < -200.0) dB = -200.0;
			dB--;
			*bp++ = (u1_t) (int) dB;
		}
	}

    #ifdef WF_SPEC_INV_DEBUG
        if (wf->trigger) {
            real_printf("\n\n$INV TRIG END\n");
            wf->trigger = false;
        }
    #endif
	
	if (wf->flush_wf_pipe) {
		out->x_bin_server = (wf->prev_start == -1)? wf->start : wf->prev_start;
		out->flags_x_zoom_server = (wf->prev_zoom == -1)? wf->zoom : wf->prev_zoom;
		wf->flush_wf_pipe--;
		if (wf->flush_wf_pipe == 0) {
			//jksd
			printf("PIPE start P%d/C%d zoom P%d/C%d\n", wf->prev_start, wf->start, wf->prev_zoom, wf->zoom);
			wf->prev_start = wf->start;
			wf->prev_zoom = wf->zoom;
		}
	} else {
		out->x_bin_server = wf->start;
		out->flags_x_zoom_server = wf->zoom;
	}
	
	evWF(EC_EVENT, EV_WF, -1, "WF", "compute_frame: fill out buf");

    if (wf->aper == AUTO)
        aperture_auto(wf, buf_p);

	ima_adpcm_state_t adpcm_wf;
	
	if (use_compression) {
		memset(out->un.adpcm_pad, out->un.buf2[0], sizeof(out->un.adpcm_pad));
		memset(&adpcm_wf, 0, sizeof(ima_adpcm_state_t));
		encode_ima_adpcm_u8_e8(out->un.buf, out->un.buf, ADPCM_PAD + WF_WIDTH, &adpcm_wf);
		wf->out_bytes = (ADPCM_PAD + WF_WIDTH) * sizeof(u1_t) / 2;
		out->flags_x_zoom_server |= WF_FLAGS_COMPRESSION;
	} else {
		wf->out_bytes = WF_WIDTH * sizeof(u1_t);
	}

	// sync this waterfall line to audio packet currently going out
	out->seq = wf->snd_seq;
	//if (out->seq != wf->snd->seq)
	//{ real_printf("%d ", wf->snd->seq - out->seq); fflush(stdout); }
	//{ real_printf("ws%d,%d ", out->seq, wf->snd->seq); fflush(stdout); }
	
	// v1.462: don't release this yet
	//if (wf->no_sync) out->flags_x_zoom_server |= WF_FLAGS_NO_SYNC;
}

void c2s_waterfall_shutdown(void *param)
{
    conn_t *c = (conn_t*)(param);
    if (c && c->mc)
        rx_server_websocket(WS_MODE_CLOSE, c->mc);
}

#endif

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

// Copyright (c) 2014 John Seamons, ZL/KF6VO
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
#include "rx_waterfall.h"
#include "noiseproc.h"
#include "dx.h"
#include "non_block.h"
#include "shmem.h"
#include "noise_blank.h"

#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <sched.h>
#include <math.h>
#include <fftw3.h>

//#define WF_INFO
//#define TR_WF_CMDS
#define SM_WF_DEBUG		false

//#define SHOW_MAX_MIN_IQ
//#define SHOW_MAX_MIN_PWR
//#define SHOW_MAX_MIN_DB

#define MAX_FFT_USED	MAX(WF_C_NFFT / WF_USING_HALF_FFT, WF_WIDTH)

#define	MAX_START(z)	((WF_WIDTH << MAX_ZOOM) - (WF_WIDTH << (MAX_ZOOM - z)))

#define WF_NSPEEDS 5
static const int wf_fps[] = { WF_SPEED_OFF, WF_SPEED_1FPS, WF_SPEED_SLOW, WF_SPEED_MED, WF_SPEED_FAST };

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

struct iq_t {
	u2_t i, q;
} __attribute__((packed));

#define	SO_OUT_HDR	((int) (sizeof(wf_pkt_t) - sizeof(out->un)))
#define	SO_OUT_NOM	((int) (SO_OUT_HDR + sizeof(out->un.buf)))
		
void c2s_waterfall_init()
{
	int i;
	
	// do these here, rather than the beginning of c2s_waterfall(), because they take too long
	// and cause the data pump to overrun
	for (i=0; i < MAX_WF_CHANS; i++) {
	    fft_t *fft = &WF_SHMEM->fft_inst[i];
		fft->hw_dft_plan = fftwf_plan_dft_1d(WF_C_NSAMPS, fft->hw_c_samps, fft->hw_fft, FFTW_FORWARD, FFTW_ESTIMATE);
	}

	float adc_scale_decim = powf(2, -16);		// gives +/- 0.5 float samples
	//float adc_scale_decim = powf(2, -15);		// gives +/- 1.0 float samples

    // window functions (adc_scale is folded in here since it's constant)
	
	//#define WINDOW_GAIN		2.0
	#define WINDOW_GAIN		1.0
	
	float *window = WF_SHMEM->window_function;

	for (i=0; i < WF_C_NSAMPS; i++) {
    	window[i] = adc_scale_decim;
    	window[i] *= WINDOW_GAIN *
    	
	    // Hanning
    	#if 1
    	    (0.5 - 0.5 * cos( (K_2PI*i)/(float)(WF_C_NSAMPS-1) ));
    	#endif

	    // Hamming
    	#if 0
    	    (0.54 - 0.46 * cos( (K_2PI*i)/(float)(WF_C_NSAMPS-1) ));
    	#endif

	    // Blackman-Harris
    	#if 0
    	    (0.35875
    	        - 0.48829 * cos( (K_2PI*i)/(float)(WF_C_NSAMPS-1) )
    	        + 0.14128 * cos( (2.0*K_2PI*i)/(float)(WF_C_NSAMPS-1) )
    	        - 0.01168 * cos( (3.0*K_2PI*i)/(float)(WF_C_NSAMPS-1) )
    	    );
    	#endif

	    // no window function
    	#if 0
    	    1.0;
    	#endif
    }
    
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
	WF_SHMEM->wf_inst[rx_chan].compression = compression;
}

#define	CMD_ZOOM	0x01
#define	CMD_START	0x02
#define	CMD_DB		0x04
#define	CMD_SPEED	0x08
#define	CMD_ALL		(CMD_ZOOM | CMD_START | CMD_DB | CMD_SPEED)

void c2s_waterfall_setup(void *param)
{
	conn_t *conn = (conn_t *) param;
	int rx_chan = conn->rx_channel;

	send_msg(conn, SM_WF_DEBUG, "MSG center_freq=%d bandwidth=%d adc_clk_nom=%.0f", (int) ui_srate/2, (int) ui_srate, ADC_CLOCK_NOM);
	send_msg(conn, SM_WF_DEBUG, "MSG kiwi_up=1 rx_chan=%d", rx_chan);       // rx_chan needed by extint_send_extlist() on js side
	extint_send_extlist(conn);

    // If not wanting a wf (!conn->isWF_conn) send wf_chans=0 to force audio FFT to be used.
    // But need to send actual value via wf_chans_real for use elsewhere.
	send_msg(conn, SM_WF_DEBUG, "MSG wf_fft_size=1024 wf_fps=%d wf_fps_max=%d zoom_max=%d rx_chans=%d wf_chans=%d wf_chans_real=%d color_map=%d wf_setup",
		WF_SPEED_FAST, WF_SPEED_MAX, MAX_ZOOM, rx_chans, conn->isWF_conn? wf_chans:0, wf_chans,
		color_map? (~conn->ui->color_map)&1 : conn->ui->color_map);
	if (do_gps && !do_sdr) send_msg(conn, SM_WF_DEBUG, "MSG gps");
}

void c2s_waterfall(void *param)
{
	conn_t *conn = (conn_t *) param;
	conn->wf_cmd_recv_ok = false;
	rx_common_init(conn);
	int rx_chan = conn->rx_channel;
	wf_inst_t *wf;
	int i, j, k, n;
	//float adc_scale_samps = powf(2, -ADC_BITS);

	bool new_map = false, new_scale_mask = false;
	int wband=-1, _wband, zoom=-1, _zoom, scale=1, _scale, _speed, _dvar, _pipe;
	float start=-1, _start, cf;
	float samp_wait_us;
	float off_freq, HZperStart = ui_srate / (WF_WIDTH << MAX_ZOOM);
	u4_t i_offset;
	int tr_cmds = 0;
	u4_t cmd_recv = 0;
	int adc_clk_corrections = 0;
	int masked_seq = 0;
	
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

	#define SO_IQ_T 4
	assert(sizeof(iq_t) == SO_IQ_T);
	#if !((NWF_SAMPS * SO_IQ_T) <= SPIBUF_B)
		#error !((NWF_SAMPS * SO_IQ_T) <= SPIBUF_B)
	#endif

	//clprintf(conn, "W/F INIT conn: %p mc: %p %s:%d %s\n",
	//	conn, conn->mc, conn->remote_ip, conn->remote_port, conn->mc->uri);

    #if 0
        if (strcmp(conn->remote_ip, "") == 0)
            cprintf(conn, "W/F INIT conn: %p mc: %p %s:%d %s\n",
                conn, conn->mc, conn->remote_ip, conn->remote_port, conn->mc->uri);
    #endif

	//evWFC(EC_DUMP, EV_WF, 10000, "WF", "DUMP 10 SEC");

	nbuf_t *nb = NULL;
	while (TRUE) {

		// reload freq NCO if adc clock has been corrected
		if (start >= 0 && adc_clk_corrections != clk.adc_clk_corrections) {
			adc_clk_corrections = clk.adc_clk_corrections;
			off_freq = start * HZperStart;
			i_offset = (u4_t) (s4_t) (off_freq / conn->adc_clock_corrected * pow(2,32));
			i_offset = -i_offset;
			if (wf->isWF)
			    spi_set(CmdSetWFFreq, rx_chan, i_offset);
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
                    cprintf(conn, "W/F <%s> cmd_recv 0x%x/0x%x\n", cmd, cmd_recv, CMD_ALL);
			#endif

			// SECURITY: this must be first for auth check
			if (rx_common_cmd("W/F", conn, cmd))
				continue;
			
			#ifdef TR_WF_CMDS
				if (tr_cmds++ < 32) {
					clprintf(conn, "W/F #%02d <%s> cmd_recv 0x%x/0x%x\n", tr_cmds, cmd, cmd_recv, CMD_ALL);
				} else {
					//cprintf(conn, "W/F <%s> cmd_recv 0x%x/0x%x\n", cmd, cmd_recv, CMD_ALL);
				}
			#endif

            bool zoom_start_chg = false;
            if (kiwi_str_begins_with(cmd, "SET zoom=")) {
                if (sscanf(cmd, "SET zoom=%d start=%f", &_zoom, &_start) == 2) {
                    //cprintf(conn, "WF: zoom=%d/%d start=%.3f(%.1f)\n", _zoom, zoom, _start, _start * HZperStart / kHz);
                    _zoom = CLAMP(_zoom, 0, MAX_ZOOM);
                    zoom_start_chg = true;
                } else
                if (sscanf(cmd, "SET zoom=%d cf=%f", &_zoom, &cf) == 2) {
                    _zoom = CLAMP(_zoom, 0, MAX_ZOOM);
                    float halfSpan_Hz = (ui_srate / (1 << _zoom)) / 2;
                    _start = (cf * kHz - halfSpan_Hz) / HZperStart;
                    //cprintf(conn, "WF: zoom=%d cf=%.3f start=%.3f halfSpan=%.3f\n", _zoom, cf, _start * HZperStart / kHz, halfSpan_Hz/kHz);
                    zoom_start_chg = true;
                }
            }
			
            if (zoom_start_chg) {
				if (zoom != _zoom) {
				    zoom = _zoom;
				    
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
					
					// We've seen cases where the w/f connects, but the sound never does.
					// So have to check for conn->other being valid.
					conn_t *csnd = conn->other;
					if (csnd && csnd->type == STREAM_SOUND && csnd->rx_channel == conn->rx_channel) {
						csnd->zoom = zoom;		// set in the AUDIO conn
					}
					
					//jksd
					//printf("ZOOM z=%d ->z=%d\n", zoom, wf->zoom);
					//wf->prev_zoom = (wf->zoom == -1)? zoom : wf->zoom;
					wf->zoom = zoom;
					cmd_recv |= CMD_ZOOM;
				}
				
                start = _start;
                //cprintf(conn, "WF: START %.0f ", start);
                int maxstart = MAX_START(zoom);
                start = CLAMP(start, 0, maxstart);
                //printf(" CLAMPED %.0f %.3f\n", start, start * HZperStart / kHz);

                off_freq = start * HZperStart;
                
                #ifdef USE_WF_NEW
                    off_freq += conn->adc_clock_corrected / (4<<zoom);
                #endif
                
                i_offset = (u4_t) (s4_t) (off_freq / conn->adc_clock_corrected * pow(2,32));
                i_offset = -i_offset;

                #ifdef WF_INFO
                if (!bg) cprintf(conn, "W/F z%d OFFSET %.3f kHz i_offset 0x%08x\n",
                    zoom, off_freq/kHz, i_offset);
                #endif
                
                if (wf->isWF)
                    spi_set(CmdSetWFFreq, rx_chan, i_offset);
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
				
				continue;
			}
			
			i = sscanf(cmd, "SET maxdb=%d mindb=%d", &wf->maxdb, &wf->mindb);
			if (i == 2) {
				#ifdef WF_INFO
				printf("waterfall: maxdb=%d mindb=%d\n", wf->maxdb, wf->mindb);
				#endif
				cmd_recv |= CMD_DB;
				continue;
			}

			i = sscanf(cmd, "SET band=%d", &_wband);
			if (i == 1) {
				//printf("waterfall: band=%d\n", _wband);
				if (wband != _wband) {
					wband = _wband;
					//printf("waterfall: BAND %d\n", wband);
				}
				continue;
			}

			i = sscanf(cmd, "SET scale=%d", &_scale);
			if (i == 1) {
				//printf("waterfall: scale=%d\n", _scale);
				if (scale != _scale) {
					scale = _scale;
					//printf("waterfall: SCALE %d\n", scale);
				}
				continue;
			}

			i = sscanf(cmd, "SET wf_speed=%d", &_speed);
			if (i == 1) {
				//printf("W/F wf_speed=%d\n", _speed);
				if (_speed == -1) _speed = WF_NSPEEDS-1;
				if (_speed >= 0 && _speed < WF_NSPEEDS)
				    wf->speed = _speed;
			    send_msg(conn, SM_NO_DEBUG, "MSG wf_fps=%d", wf_fps[wf->speed]);
				cmd_recv |= CMD_SPEED;
				continue;
			}

			i = sscanf(cmd, "SET send_dB=%d", &wf->send_dB);
			if (i == 1) {
				//cprintf(conn, "W/F send_dB=%d\n", wf->send_dB);
				continue;
			}

			// FIXME: keep these from happening in the first place?
			int ch;
			i = sscanf(cmd, "SET ext_blur=%d", &ch);
			if (i == 1) {
				continue;
			}

			if (conn->mc != NULL) {
			    cprintf(conn, "W/F BAD PARAMS: sl=%d %d|%d|%d [%s] ip=%s ####################################\n",
			        strlen(cmd), cmd[0], cmd[1], cmd[2], cmd, conn->remote_ip);
			    conn->unknown_cmd_recvd++;
			}
			
			continue;
		} else {
			assert(nb == NULL);
		}
		
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
			app_to_web(conn, (char*) &out, SO_OUT_NOM);
		}
		
		if (!do_sdr) {
			NextTask("WF skip");
			continue;
		}

		if (conn->stop_data) {
			//clprintf(conn, "W/F stop_data rx_server_remove()\n");
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
			//if (keepalive_expired) clprintf(conn, "W/F KEEP-ALIVE EXPIRED\n");
			//if (connection_hang) clprintf(conn, "W/F CONNECTION HANG\n");
			//if (conn->kick) clprintf(conn, "W/F KICK\n");
		
			// Ask sound task to stop (must not do while, for example, holding a lock).
			// We've seen cases where the sound connects, then times out. But the w/f has never connected.
			// So have to check for conn->other being valid.
			conn_t *csnd = conn->other;
			if (csnd && csnd->type == STREAM_SOUND && csnd->rx_channel == conn->rx_channel) {
				csnd->stop_data = TRUE;
			} else {
				rx_enable(rx_chan, RX_CHAN_FREE);		// there is no SND, so free rx_chan[] now
			}
			
			//clprintf(conn, "W/F rx_server_remove()\n");
			rx_server_remove(conn);
			panic("shouldn't return");
		}

        // FIXME: until we figure out if no WF cmds are needed when no wf is present just occasionally wake up and check
		if (rx_chan >= wf_num) {
			TaskSleepMsec(500);
			continue;
		}
		
		// Don't process any waterfall data until we've received all necessary commands.
		// Also, stop waterfall if speed is zero.
		if (cmd_recv != CMD_ALL || wf->speed == WF_SPEED_OFF) {
			TaskSleepMsec(100);
			continue;
		}
		
		if (!conn->wf_cmd_recv_ok) {
			#ifdef TR_WF_CMDS
				clprintf(conn, "W/F cmd_recv ALL 0x%x/0x%x\n", cmd_recv, CMD_ALL);
			#endif
			conn->wf_cmd_recv_ok = true;
		}
		
        if (wf->isFFT) {
            TaskSleepMsec(250);
            continue;
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
				// >= FFT than plot
				#ifdef WF_FFT_UNWRAP_NEEDED
					// IQ reverse unwrapping (X)
					for (i=wf->fft_used/2,j=0; i<wf->fft_used; i++,j++) {
						wf->fft2wf_map[i] = wf->plot_width * j/wf->fft_used;
					}
					for (i=0; i<wf->fft_used/2; i++,j++) {
						wf->fft2wf_map[i] = wf->plot_width * j/wf->fft_used;
					}
				#else
					// no unwrap
					for (i=0; i<wf->fft_used; i++) {
						wf->fft2wf_map[i] = wf->plot_width * i/wf->fft_used;
					}
				#endif
				//for (i=0; i<wf->fft_used; i++) printf("%d>%d ", i, wf->fft2wf_map[i]);
			} else {
				// < FFT than plot
				#ifdef WF_FFT_UNWRAP_NEEDED
					for (i=0; i<wf->plot_width_clamped; i++) {
						int t = wf->fft_used * i/wf->plot_width;
						if (t >= wf->fft_used/2) t -= wf->fft_used/2; else t += wf->fft_used/2;
						wf->wf2fft_map[i] = t;
					}
				#else
					// no unwrap
					for (i=0; i<wf->plot_width_clamped; i++) {
						wf->wf2fft_map[i] = wf->fft_used * i/wf->plot_width;
					}
				#endif
				//for (i=0; i<wf->plot_width_clamped; i++) printf("%d:%d ", i, wf->wf2fft_map[i]);
			}
			//printf("\n");
			
			#ifdef WF_INFO
			if (!bg) cprintf(conn, "W/F NEW_MAP z%d fft_used %d/%d span %.1f disp_fs %.1f plot_width %d/%d %s FFT than plot\n",
				zoom, wf->fft_used, WF_C_NFFT, span/kHz, disp_fs/kHz, wf->plot_width_clamped, wf->plot_width,
				(wf->plot_width_clamped < wf->fft_used)? ">=":"<");
			#endif
			
			send_msg(conn, SM_NO_DEBUG, "MSG plot_width=%d", wf->plot_width);
			new_map = FALSE;
		}
		
		if (masked_seq != dx.masked_seq) {
            send_msg(conn, false, "MSG request_dx_update");     // get client to request updated dx list
		    masked_seq = dx.masked_seq;
		    new_scale_mask = true;
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
                        dx_t *dxp = &dx.list[dx.masked_idx[j]];
                        if (f >= dxp->masked_lo && f <= dxp->masked_hi) {
                            scale = 0;
                            break;
                        }
                    }
                    wf->fft_scale[i] = scale;
                }
			} else {
                for (i=0; i < wf->plot_width_clamped; i++)
                    wf->fft_scale[i] = fft_scale;
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
    u64_t now, deadline;
    
    #ifdef SHOW_MAX_MIN_IQ
    static void *IQi_state;
    static void *IQf_state;
    if (wf->new_map2) {
        if (IQi_state != NULL) free(IQi_state);
        IQi_state = NULL;
        if (IQf_state != NULL) free(IQf_state);
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
            now = timer_us64();
            if (now < deadline) {
                u4_t diff = deadline - now;
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
	    float *window = WF_SHMEM->window_function;
        
        for (k=0; k<NWF_SAMPS; k++) {
            if (sn >= WF_C_NSAMPS) break;
            ii = (s4_t) (s2_t) iqp->i;
            qq = (s4_t) (s2_t) iqp->q;
            iqp++;

            float fi = ((float) ii) * window[sn];
            float fq = ((float) qq) * window[sn];
            
            #ifdef SHOW_MAX_MIN_IQ
            print_max_min_stream_i(&IQi_state, "IQi", k, 2, ii, qq);
            print_max_min_stream_f(&IQf_state, "IQf", k, 2, (double) fi, (double) fq);
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
        u4_t now = timer_sec();
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
            m_NoiseProc[rx_chan][NB_WF].SetupBlanker("WF", srate, wf->nb_param[NB_BLANKER]);
            wf->nb_param_change[NB_BLANKER] = false;
            wf->nb_setup = true;
        }

        if (wf->nb_setup)
            m_NoiseProc[rx_chan][NB_WF].ProcessBlankerOneShot(WF_C_NSAMPS, (TYPECPX*) fft->hw_c_samps, (TYPECPX*) fft->hw_c_samps);
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
                shmem_ipc_invoke(SIG_IPC_WF, wf->rx_chan);      // invoke compute_frame()
            #endif
        #endif
        
        wf_pkt_t *out = &wf->out;
        app_to_web(wf->conn, (char*) out, SO_OUT_HDR + wf->out_bytes);
        waterfall_bytes[rx_chan] += wf->out_bytes;
        waterfall_bytes[rx_chans] += wf->out_bytes; // [rx_chans] is the sum of all waterfalls
        waterfall_frames[rx_chan]++;
        waterfall_frames[rx_chans]++;       // [rx_chans] is the sum of all waterfalls
        evWF(EC_EVENT, EV_WF, -1, "WF", "compute_frame: done");
    
        #if 0
            static u4_t last_time[MAX_RX_CHANS];
            u4_t now = timer_ms();
            printf("WF%d: %d %.3fs seq-%d\n", rx_chan, SO_OUT_HDR + wf->out_bytes,
                (float) (now - last_time[rx_chan]) / 1e3, wf->out.seq);
            last_time[rx_chan] = now;
        #endif
    //}

    int actual = timer_ms() - wf->mark;
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
    wf->mark = timer_ms();
}

void compute_frame(int rx_chan)
{
	wf_inst_t *wf = &WF_SHMEM->wf_inst[rx_chan];
	int i;
	wf_pkt_t *out = &wf->out;
	u1_t comp_in_buf[WF_WIDTH];
	float pwr[MAX_FFT_USED];
    fft_t *fft = &WF_SHMEM->fft_inst[rx_chan];
		
    //TaskStat2(TSTAT_INCR|TSTAT_ZERO, 0, "frm");

	//NextTask("FFT1");
	evWF(EC_EVENT, EV_WF, -1, "WF", "compute_frame: FFT start");
	fftwf_execute(fft->hw_dft_plan);
	evWF(EC_EVENT, EV_WF, -1, "WF", "compute_frame: FFT done");
	//NextTask("FFT2");

	u1_t *bp = (wf->compression)? out->un.buf2 : out->un.buf;
			
	if (!wf->fft_used_limit) wf->fft_used_limit = wf->fft_used;

	// zero-out the DC component in bin zero/one (around -90 dBFS)
	// otherwise when scrolling w/f it will move but then not continue at the new location
	pwr[0] = 0;
	pwr[1] = 0;
	
	#ifdef SHOW_MAX_MIN_PWR
	static void *FFT_state;
	static void *pwr_state;
	static void *dB_state;
	static void *buf_state;
	if (wf->new_map2) {
		if (FFT_state != NULL) free(FFT_state);
		FFT_state = NULL;
		if (pwr_state != NULL) free(pwr_state);
		pwr_state = NULL;
		if (dB_state != NULL) free(dB_state);
		dB_state = NULL;
		if (buf_state != NULL) free(buf_state);
		buf_state = NULL;
		wf->new_map2 = false;
	}
	#endif
	
	for (i=2; i < wf->fft_used_limit; i++) {
		float re = fft->hw_fft[i][I], im = fft->hw_fft[i][Q];
		pwr[i] = re*re + im*im;

		#ifdef SHOW_MAX_MIN_PWR
		//print_max_min_stream_f(&FFT_state, "FFT", i, 2, (double) re, (double) im);
		//print_max_min_stream_f(&pwr_state, "pwr", i, 1, (double) pwr[i]);
		#endif
	}
		
	// fixme proper power-law scaling..
	
	// from the tutorials at http://www.fourier-series.com/fourierseries2/flash_programs/DFT_windows/index.html
	// recall:
	// pwr = mag*mag			
	// pwr = i*i + q*q
	// mag = sqrt(i*i + q*q)
	// pwr = mag*mag = i*i + q*q
	// pwr(dB) = 10 * log10(pwr)		i.e. no sqrt() needed
	// pwr(dB) = 20 * log10(mag[ampl])
	// pwr gain = pow10(db/10)
	// mag[ampl] gain = pow10(db/20)
	
	// with 'bc -l', l() = log base e
	// log10(n) = l(n)/l(10)
	// 10^x = e^(x*ln(10)) = e(x*l(10))
	
    #ifdef WF_INFO
	    float max_dB = wf->maxdb;
	    float min_dB = wf->mindb;
	    float range_dB = max_dB - min_dB;
	    float pix_per_dB = 255.0 / range_dB;
	#endif

	int bin=0, _bin=-1, bin2pwr[WF_WIDTH];
	float p, dB, pwr_out_sum[WF_WIDTH], pwr_out_peak[WF_WIDTH];

    //#define WF_CMA
    #ifndef WF_CMA
	    memset(pwr_out_peak, 0, sizeof(pwr_out_peak));
	#endif

	if (wf->fft_used >= wf->plot_width) {
		// >= FFT than plot
		
		int avgs=0;
		for (i=0; i<wf->fft_used_limit; i++) {
			p = pwr[i];
			bin = wf->fft2wf_map[i];
			if (bin >= WF_WIDTH) {
				if (wf->new_map) {
				
					#ifdef WF_INFO
					if (!bg) printf(">= FFT: Z%d WF_C_NSAMPS %d i %d fft_used %d plot_width %d pix_per_dB %.3f range %.0f:%.0f\n",
						wf->zoom, WF_C_NSAMPS, i, wf->fft_used, wf->plot_width, pix_per_dB, max_dB, min_dB);
					#endif
					wf->new_map = FALSE;
				}
				wf->fft_used_limit = i;		// we now know what the limit is
				break;
			}
			
			if (bin == _bin) {
				#ifdef WF_CMA
					pwr_out_cma[bin] = (pwr_out_cma[bin] * avgs) + p;
					avgs++;
					pwr_out_cma[bin] /= avgs;
				#else
					if (p > pwr_out_peak[bin]) pwr_out_peak[bin] = p;
					pwr_out_sum[bin] += p;
				#endif
			} else {
				#ifdef WF_CMA
					avgs = 1;
				#endif
				pwr_out_peak[bin] = p;
				pwr_out_sum[bin] = p;
				_bin = bin;
			}
		}

		#ifdef SHOW_MAX_MIN_DB
		float dBs_f[WF_WIDTH];
		int dBs[WF_WIDTH];
		#endif

		for (i=0; i<WF_WIDTH; i++) {
			p = pwr_out_peak[i];
			//p = pwr_out_sum[i];

			dB = 10.0 * log10f(p * wf->fft_scale[i] + (float) 1e-30) + wf->fft_offset;
#if 0
//jks
if (i == 506) printf("Z%d ", wf->zoom);
if (i >= 507 && i <= 515) {
	float peak_dB = 10.0 * log10f(pwr_out_peak[i] * wf->fft_scale[i] + (float) 1e-30) + wf->fft_offset;
	printf("%d:%.1f|%.1f ", i, dB, peak_dB);
}
if (i == 516) printf("\n");
#endif

			#ifdef SHOW_MAX_MIN_DB
			dBs_f[i] = dB;
			#endif
			#ifdef SHOW_MAX_MIN_PWR
			print_max_min_stream_f(&dB_state, "dB", i, 1, (double) dB);
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
			print_max_min_stream_i(&buf_state, "buf", i, 1, (int) *(bp-1));
			#endif
		}

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
		// < FFT than plot
		if (wf->new_map) {
			//printf("< FFT: Z%d WF_C_NSAMPS %d fft_used %d plot_width_clamped %d pix_per_dB %.3f range %.0f:%.0f\n",
			//	wf->zoom, WF_C_NSAMPS, wf->fft_used, wf->plot_width_clamped, pix_per_dB, max_dB, min_dB);
			wf->new_map = FALSE;
		}

		for (i=0; i<wf->plot_width_clamped; i++) {
			p = pwr[wf->wf2fft_map[i]];
			
			dB = 10.0 * log10f(p * wf->fft_scale[i] + (float) 1e-30) + wf->fft_offset;
			if (dB > 0) dB = 0;
			if (dB < -200.0) dB = -200.0;
			dB--;
			*bp++ = (u1_t) (int) dB;
		}
	}
	
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

	ima_adpcm_state_t adpcm_wf;
	
	if (wf->compression) {
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
}

void c2s_waterfall_shutdown(void *param)
{
    conn_t *c = (conn_t*)(param);
    if (c && c->mc)
        rx_server_websocket(WS_MODE_CLOSE, c->mc);
}

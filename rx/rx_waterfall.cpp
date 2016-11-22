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
#include "misc.h"
#include "nbuf.h"
#include "web.h"
#include "peri.h"
#include "spi.h"
#include "gps.h"
#include "coroutines.h"
#include "pru_realtime.h"
#include "debug.h"
#include "data_pump.h"
#include "cfg.h"
#include "datatypes.h"
#include "ext_int.h"

#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <sched.h>
#include <math.h>
#include <fftw3.h>

//#define SHOW_MAX_MIN_IQ
//#define SHOW_MAX_MIN_PWR
//#define SHOW_MAX_MIN_DB
#define WF_INFO

#ifdef USE_WF_NEW
#define	WF_USING_HALF_FFT	1	// the result is contained in the first half of the complex FFT
#define	WF_USING_HALF_CIC	1	// only use half of the remaining FFT after a CIC
#define	WF_BETTER_LOOKING	1	// increase in FFT size for better looking display
#else
#define	WF_USING_HALF_FFT	2	// the result is contained in the first half of the complex FFT
#define	WF_USING_HALF_CIC	2	// only use half of the remaining FFT after a CIC
#define	WF_BETTER_LOOKING	2	// increase in FFT size for better looking display
#endif

#define WF_OUTPUT	1024	// conceptually same as WF_WIDTH although not required
#define WF_C_NFFT	(WF_OUTPUT * WF_USING_HALF_FFT * WF_USING_HALF_CIC * WF_BETTER_LOOKING)	// worst case FFT size needed
#define WF_C_NSAMPS	WF_C_NFFT

#define	WF_WIDTH		1024	// width of waterfall display

#define MAX_FFT_USED	MAX(WF_C_NFFT / WF_USING_HALF_FFT, WF_WIDTH)

//#define	MAX_ZOOM		10
#define	MAX_ZOOM		11
#define	MAX_START(z)	((WF_WIDTH << MAX_ZOOM) - (WF_WIDTH << (MAX_ZOOM - z)))

static const int wf_fps[] = { WF_SPEED_SLOW, WF_SPEED_MED, WF_SPEED_FAST };

static float window_function_c[WF_C_NSAMPS];

struct iq_t {
	u2_t i, q;
} __attribute__((packed));

static struct wf_t {
	conn_t *conn;
	int fft_used, plot_width, plot_width_clamped;
	int maxdb, mindb, send_dB;
	float fft_scale, fft_offset;
	u2_t fft2wf_map[WF_C_NFFT / WF_USING_HALF_FFT];		// map is 1:1 with fft
	u2_t wf2fft_map[WF_WIDTH];							// map is 1:1 with plot
	int mark, slow, zoom, start, fft_used_limit;
	bool new_map, new_map2, compression;
	int flush_wf_pipe;
	SPI_MISO hw_miso;
} wf_inst[WF_CHANS];

static struct fft_t {
	fftwf_plan hw_dft_plan;
	fftwf_complex *hw_c_samps;
	fftwf_complex *hw_fft;
} fft_inst[WF_CHANS];

struct wf_pkt_t {
	char id[4];
	u4_t x_bin_server;
	#define WF_FLAGS_COMPRESSION 0x00010000
	u4_t flags_x_zoom_server;
	u4_t seq;
	union {
		u1_t buf[WF_WIDTH];
		struct {
			#define ADPCM_PAD 10
			u1_t adpcm_pad[ADPCM_PAD];
			u1_t buf2[WF_WIDTH];
		};
	} un;
} __attribute__((packed));

#define	SO_OUT_HDR	((int) (sizeof(out) - sizeof(out.un)))
#define	SO_OUT_NOM	((int) (SO_OUT_HDR + sizeof(out.un.buf)))
		
void compute_frame(wf_t *wf, fft_t *fft);

static int n_chunks;

void w2a_waterfall_init()
{
	int i;
	
	assert(RX_CHANS == WF_CHANS);
	
	// do these here, rather than the beginning of w2a_waterfall(), because they take too long
	// and cause the data pump to overrun
	fft_t *fft;
	for (fft = fft_inst; fft < &fft_inst[WF_CHANS]; fft++) {
		fft->hw_c_samps = (fftwf_complex*) fftwf_malloc(sizeof(fftwf_complex) * (WF_C_NSAMPS));
		fft->hw_fft = (fftwf_complex*) fftwf_malloc(sizeof(fftwf_complex) * (WF_C_NFFT));
		fft->hw_dft_plan = fftwf_plan_dft_1d(WF_C_NSAMPS, fft->hw_c_samps, fft->hw_fft, FFTW_FORWARD, FFTW_MEASURE);
	}

	#define WF_BITS 16
	float adc_scale_decim = powf(2, -16);		// gives +/- 0.5 float samples
	//float adc_scale_decim = powf(2, -15);		// gives +/- 1.0 float samples

    // window functions (adc_scale is folded in here since it's constant)
	// Hanning creates a 1 MHz carrier when using the generator? (but Hamming is okay) fixme: still true?
	
	// Hanning
	#define	WINDOW_COEF1	0.5
	#define	WINDOW_COEF2	0.5
	
	// Hamming
	//#define	WINDOW_COEF1	0.54
	//#define	WINDOW_COEF2	0.46
	
	//#define WINDOW_GAIN		2.0
	#define WINDOW_GAIN		1.0

	for (i=0; i < WF_C_NSAMPS; i++) {
    	window_function_c[i] = adc_scale_decim;
    	window_function_c[i] *= WINDOW_GAIN * (WINDOW_COEF1 - WINDOW_COEF2 * cos( (K_2PI*i)/(float)(WF_C_NSAMPS-1) ));
    }
    
	n_chunks = (int) ceilf((float) WF_C_NSAMPS / NWF_SAMPS);
	
	assert(WF_C_NSAMPS == WF_C_NFFT);
	assert(WF_C_NSAMPS <= 8192);	// hardware sample buffer length limitation
}

void w2a_waterfall_compression(int rx_chan, bool compression)
{
	wf_inst[rx_chan].compression = compression;
}

#define	CMD_ZOOM	0x01
#define	CMD_START	0x02
#define	CMD_DB		0x04
#define	CMD_SLOW	0x08
#define	CMD_ALL		(CMD_ZOOM | CMD_START | CMD_DB | CMD_SLOW)

void w2a_waterfall(void *param)
{
	conn_t *conn = (conn_t *) param;
	int rx_chan = conn->rx_channel;
	wf_t *wf;
	fft_t *fft;
	int i, j, k, n;
	//float adc_scale_samps = powf(2, -ADC_BITS);

	bool new_map, overlapped_sampling = false;
	int wband, _wband, zoom=-1, _zoom, scale=1, _scale, _slow, _dvar, _pipe;
	float start=-1, _start;
	bool do_send_msg = FALSE;
	u2_t decim;
	float samp_wait_us;
	int samp_wait_ms, chunk_wait_us;
	u64_t now, deadline;
	float off_freq, HZperStart = ui_srate / (WF_WIDTH << MAX_ZOOM);
	u4_t i_offset;
	int tr_cmds = 0;
	u4_t cmd_recv = 0;
	bool cmd_recv_ok = false;
	u4_t ka_time = timer_sec();
	int adc_clk_corr = 0;
	
	assert(rx_chan < WF_CHANS);
	wf = &wf_inst[rx_chan];
	memset(wf, 0, sizeof(wf_t));
	wf->conn = conn;
	wf->compression = true;

	fft = &fft_inst[rx_chan];

	send_msg(conn, SM_NO_DEBUG, "MSG center_freq=%d bandwidth=%d", (int) ui_srate/2, (int) ui_srate);
	send_msg(conn, SM_NO_DEBUG, "MSG kiwi_up=1");
	extint_send_extlist(conn);
	u4_t adc_clock_i = roundf(adc_clock);

	// negative fft_fps means allow w/f queue to grow unbounded (i.e. will catch-up on recovery from network stall)
	send_msg(conn, SM_NO_DEBUG, "MSG fft_size=1024 fft_fps=-20 zoom_max=%d rx_chans=%d rx_chan=%d color_map=%d fft_setup",
		MAX_ZOOM, RX_CHANS, rx_chan, color_map? (~conn->ui->color_map)&1 : conn->ui->color_map);
	if (do_gps && !do_sdr) send_msg(conn, SM_NO_DEBUG, "MSG gps");

    wf->mark = timer_ms();
    
	#define SO_IQ_T 4
	assert(sizeof(iq_t) == SO_IQ_T);
	#if !((NWF_SAMPS * SO_IQ_T) <= NSPI_RX)
		#error !((NWF_SAMPS * SO_IQ_T) <= NSPI_RX)
	#endif

	//clprintf(conn, "W/F INIT conn: %p mc: %p %s:%d %s\n",
	//	conn, conn->mc, conn->mc->remote_ip, conn->mc->remote_port, conn->mc->uri);

	//evWFC(EC_DUMP, EV_WF, 10000, "WF", "DUMP 10 SEC");

	nbuf_t *nb = NULL;
	while (TRUE) {

		// reload freq NCO if adc clock has been corrected
		if (start >= 0 && adc_clk_corr != gps.adc_clk_corr) {
			adc_clk_corr = gps.adc_clk_corr;
			off_freq = start * HZperStart;
			i_offset = (u4_t) (s4_t) (off_freq / adc_clock * pow(2,32));
			i_offset = -i_offset;
			spi_set(CmdSetWFFreq, rx_chan, i_offset);
			//printf("WF%d freq updated due to ADC clock correction\n", rx_chan);
		}

		if (nb) web_to_app_done(conn, nb);
		n = web_to_app(conn, &nb);
				
		if (n) {
			char *cmd = nb->buf;
			cmd[n] = 0;		// okay to do this -- see nbuf.c:nbuf_allocq()

			ka_time = timer_sec();

			if (rx_common_cmd("W/F", conn, cmd))
				continue;
			
			#if 0
			if (tr_cmds++ < 32) {
				clprintf(conn, "W/F #%02d <%s> cmd_recv 0x%x/0x%x\n", tr_cmds, cmd, cmd_recv, CMD_ALL);
			} else {
				//cprintf(conn, "W/F <%s> cmd_recv 0x%x/0x%x\n", cmd, cmd_recv, CMD_ALL);
			}
			#endif

			i = sscanf(cmd, "SET zoom=%d start=%f", &_zoom, &_start);
			if (i == 2) {
				//printf("waterfall: zoom=%d/%d start=%.3f(%.1f)\n",
				//	_zoom, zoom, _start, _start * HZperStart / kHz);
				if (zoom != _zoom) {
					zoom = _zoom;
					if (zoom < 0) zoom = 0;
					if (zoom > MAX_ZOOM) zoom = MAX_ZOOM;
					
					// currently 12-levels of zoom: z0-z11, MAX_ZOOM == 11

#ifdef USE_WF_NEW
					decim = 0x0002 << zoom;		// z0-10 R = 2,4,8,16,32,64,128,256,512,1024,2048
#else
					// NB: because we only use half of the FFT with CIC can zoom one level less
					int zm1 = (WF_USING_HALF_CIC == 2)? (zoom? (zoom-1) : 0) : zoom;

					#ifdef USE_WF_1CIC
						if (zm1 == 0) {
							decim = 0x0001;		// R = 1
						} else {
							decim = 1 << zm1;	// R = 2,4,8,16,32,64,128,256,512 for MAX_ZOOM = 10
												// R = 2,4,8,16,32,64,128,256,512,1024 for MAX_ZOOM = 11
						}
					#else
						if (zm1 == 0) {
							decim = 0x0101;		// R = 1 (R1 = R2 = 1)
						} else
						if (zm1 <= 5) {
							decim = 0x0001 | (0x0100 << zm1);		// R = 2,4,8,16,32 (R1 = 1; R2 = 2,4,8,16,32)
						} else {
							decim = (0x0001 << (zm1-5)) | 0x2000;	// R = 64,128,256,512 (R1 = 2,4,8,16; R2 = 32)
						}
					#endif
#endif
					samp_wait_us =  WF_C_NSAMPS * (1 << zm1) / adc_clock * 1000000.0;
					chunk_wait_us = (int) ceilf(samp_wait_us / n_chunks);
					samp_wait_ms = (int) ceilf(samp_wait_us / 1000);
					#ifdef WF_INFO
					if (!bg) cprintf(conn, "---- WF%d Z%d zm1 %d/%d R%d n_chunks %d samp_wait_us %.1f samp_wait_ms %d chunk_wait_us %d\n",
						rx_chan, zoom, zm1, 1<<zm1, decim, n_chunks, samp_wait_us, samp_wait_ms, chunk_wait_us);
					#endif
					
					do_send_msg = TRUE;
					new_map = wf->new_map = wf->new_map2 = TRUE;
					
					// when zoom changes reevaluate if overlapped sampling might be needed
					overlapped_sampling = false;
					
					spi_set(CmdSetWFDecim, rx_chan, decim);
					
					// We've seen cases where the w/f connects, but the sound never does.
					// So have to check for conn->other being valid.
					conn_t *csnd = conn->other;
					if (csnd && csnd->type == STREAM_SOUND && csnd->rx_channel == conn->rx_channel) {
						csnd->zoom = zoom;		// set in the AUDIO conn
					}
					
					wf->zoom = zoom;
					cmd_recv |= CMD_ZOOM;
				}
				
				//if (start != _start) {
					start = _start;
					//printf("waterfall: START %f ", start);
					if (start < 0) {
						printf(", clipped to 0");
						start = 0;
					}
					int maxstart = MAX_START(zoom);
					if (start > maxstart) {
						printf(", clipped to %d", maxstart);
						start = maxstart;
					}
					//printf("\n");

					off_freq = start * HZperStart;
					
					#ifdef USE_WF_NEW
						off_freq += adc_clock / (4<<zoom);
					#endif
					
					i_offset = (u4_t) (s4_t) (off_freq / adc_clock * pow(2,32));
					i_offset = -i_offset;

					#ifdef WF_INFO
					if (!bg) cprintf(conn, "W/F z%d OFFSET %.3f kHz i_offset 0x%08x\n",
						zoom, off_freq/kHz, i_offset);
					#endif
					
					spi_set(CmdSetWFFreq, rx_chan, i_offset);
					do_send_msg = TRUE;
					cmd_recv |= CMD_START;
					wf->start = start;
				//}
				
				if (do_send_msg) {
					send_msg(conn, SM_NO_DEBUG, "MSG zoom=%d start=%d", zoom, (u4_t) start);
					//printf("waterfall: send zoom %d start %d\n", zoom, u_start);
					do_send_msg = FALSE;
					wf->flush_wf_pipe = 1;
				}
				
				continue;
			}
			
			i = sscanf(cmd, "SET maxdb=%d mindb=%d", &wf->maxdb, &wf->mindb);
			if (i == 2) {
				//printf("waterfall: maxdb=%d mindb=%d\n", wf->maxdb, wf->mindb);
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

			i = sscanf(cmd, "SET slow=%d", &_slow);
			if (i == 1) {
				//printf("waterfall: slow=%d\n", _slow);
				if (wf->slow != _slow) {
					/*
					if (!conn->first_slow && wf_max) {
						wf->slow = 2;
						conn->first_slow = TRUE;
					} else {
					*/
						wf->slow = _slow;
						//wf->slow = 0;
					//}
					//printf("waterfall: SLOW %d\n", wf->slow);
				}
				cmd_recv |= CMD_SLOW;
				continue;
			}

			i = sscanf(cmd, "SET dvar=%d", &_dvar);
			if (i == 1) {
				continue;
			}

			i = sscanf(cmd, "SET send_dB=%d", &wf->send_dB);
			if (i == 1) {
				//cprintf(conn, "W/F send_dB=%d\n", wf->send_dB);
				continue;
			}

			cprintf(conn, "W/F BAD PARAMS: <%s> ####################################\n", cmd);
			continue;
		} else {
			assert(nb == NULL);
		}
		
		if (do_gps && !do_sdr) {
			wf_pkt_t out;
			int *ns_bin = ClockBins();
			int max=0;
			
			for (n=0; n<1024; n++) if (ns_bin[n] > max) max = ns_bin[n];
			if (max == 0) max=1;
			u1_t *bp = out.un.buf;
			for (n=0; n<1024; n++) {
				*bp++ = (u1_t) (int) (-256 + (ns_bin[n] * 255 / max));	// simulate negative dBm
			}
			int delay = 10000 - (timer_ms() - wf->mark);
			if (delay > 0) TaskSleepS("wait frame", delay * 1000);
			wf->mark = timer_ms();
			strncpy(out.id, "FFT ", 4);
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

		// no keep-alive seen for a while or the bug where an initial cmds are not received and the connection hangs open
		// and locks-up a receiver channel
		conn->keep_alive = timer_sec() - ka_time;
		bool keepalive_expired = (conn->keep_alive > KEEPALIVE_SEC);
		bool connection_hang = (conn->keepalive_count > 4 && cmd_recv != CMD_ALL);
		if (keepalive_expired || connection_hang) {
			//if (keepalive_expired) clprintf(conn, "W/F KEEP-ALIVE EXPIRED\n");
			//if (connection_hang) clprintf(conn, "W/F CONNECTION HANG\n");
		
			// Ask sound task to stop (must not do while, for example, holding a lock).
			// We've seen cases where the sound connects, then times out. But the w/f has never connected.
			// So have to check for conn->other being valid.
			conn_t *cwf = conn->other;
			if (cwf && cwf->type == STREAM_SOUND && cwf->rx_channel == conn->rx_channel) {
				cwf->stop_data = TRUE;
			} else {
				rx_enable(rx_chan, RX_CHAN_FREE);		// there is no SND, so free rx_chan[] now
			}
			
			//clprintf(conn, "W/F rx_server_remove()\n");
			rx_server_remove(conn);
			panic("shouldn't return");
		}

		if (rx_chan >= wf_num) {		// for debug
			NextTask("skip");
			continue;
		}
		
		// don't process any waterfall data until we've received all necessary commands
		if (cmd_recv != CMD_ALL) {
			TaskSleep(100000);
			continue;
		}
		if (!cmd_recv_ok) {
			//clprintf(conn, "W/F cmd_recv ALL 0x%x/0x%x\n", cmd_recv, CMD_ALL);
			cmd_recv_ok = true;
		}
		
		wf->fft_used = WF_C_NFFT / WF_USING_HALF_FFT;		// the result is contained in the first half of a complex FFT
		
		// if any CIC is used (z != 0) only look at half of it to avoid the aliased images
		#ifdef USE_WF_NEW
			wf->fft_used /= WF_USING_HALF_CIC;
		#else
			if (zoom != 0) wf->fft_used /= WF_USING_HALF_CIC;
		#endif
		
		float span = adc_clock / 2 / (1<<zoom);
		float disp_fs = ui_srate / (1<<zoom);
		
		// NB: plot_width can be greater than WF_WIDTH because it relative to the ratio of the
		// (adc_clock/2) / ui_srate, which can be > 1 (hence plot_width_clamped).
		// All this is necessary because we might be displaying less than what adc_clock/2 implies because
		// of using third-party obtained frequency scale images in our UI.
		wf->plot_width = WF_WIDTH * span / disp_fs;
		wf->plot_width_clamped = (wf->plot_width > WF_WIDTH)? WF_WIDTH : wf->plot_width;
		
		if (new_map) {
			assert(wf->fft_used <= MAX_FFT_USED);
			wf->fft_used_limit = 0;

			if (wf->fft_used >= wf->plot_width) {
				// >= FFT than plot
				#ifdef USE_WF_NEW
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
				#ifdef USE_WF_NEW
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
			
			// FIXME: Is this right? Why is this so strange?
			float maxmag = zoom? wf->fft_used : wf->fft_used/2;
			//jks
			//wf->fft_scale = 20.0 / (maxmag * maxmag);
			//wf->fft_offset = 0;
			
			// makes GEN attn 0 dB = 0 dBm
			wf->fft_scale = 5.0 / (maxmag * maxmag);
			wf->fft_offset = zoom? -0.08 : -0.8;
			
			// fixes GEN z0/z1+ levels, but breaks regular z0/z1+ noise floor continuity -- why?
			//float maxmag = zoom? wf->fft_used : wf->fft_used/4;
			//wf->fft_scale = (zoom? 2.0 : 5.0) / (maxmag * maxmag);

			#ifdef WF_INFO
			if (!bg) cprintf(conn, "W/F NEW_MAP z%d fft_used %d/%d span %.1f disp_fs %.1f maxmag %.0f fft_scale %.1e plot_width %d/%d %s FFT than plot\n",
				zoom, wf->fft_used, WF_C_NFFT, span/kHz, disp_fs/kHz, maxmag, wf->fft_scale, wf->plot_width_clamped, wf->plot_width,
				(wf->plot_width_clamped < wf->fft_used)? ">=":"<");
			#endif
			
			send_msg(conn, SM_NO_DEBUG, "MSG plot_width=%d", wf->plot_width);
			new_map = FALSE;
		}

		
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
		
		// desired frame rate greater than what full sampling can deliver, so start overlapped sampling
		assert(wf_fps[wf->slow] != 0);
		int desired = 1000 / wf_fps[wf->slow];

		if (!overlapped_sampling && samp_wait_ms > desired) {
			overlapped_sampling = true;
			
			#ifdef WF_INFO
			if (!bg) printf("---- WF%d OLAP z%d desired %d, samp_wait %d\n",
				rx_chan, zoom, desired, samp_wait_ms);
			#endif
			
			evWFC(EC_TRIG1, EV_WF, -1, "WF", "OVERLAPPED CmdWFReset");
			spi_set(CmdWFReset, rx_chan, WF_SAMP_RD_RST | WF_SAMP_WR_RST | WF_SAMP_CONTIN);
			TaskSleepS("fill pipe", (samp_wait_ms+1) * 1000);		// fill pipeline
		}
		
		SPI_CMD first_cmd;
		if (overlapped_sampling) {
			//
			// Start reading immediately at synchronized write address plus a small offset to get to
			// the old part of the buffer.
			// This presumes zoom factor isn't so large that buffer fills too slowly and we
			// overrun reading the last little bit (offset part). Also presumes that we can read the
			// first part quickly enough that the write doesn't catch up to us.
			//
			// CmdGetWFContSamps asserts WF_SAMP_SYNC | WF_SAMP_CONTIN in kiwi.asm code
			//
			first_cmd = CmdGetWFContSamps;
		} else {
			evWFC(EC_TRIG1, EV_WF, -1, "WF", "NON-OVERLAPPED CmdWFReset");
			spi_set(CmdWFReset, rx_chan, WF_SAMP_RD_RST | WF_SAMP_WR_RST);
			deadline = timer_us64() + chunk_wait_us*2;
			first_cmd = CmdGetWFSamples;
		}

		SPI_MISO *miso = &(wf->hw_miso);
		s4_t ii, qq;
		iq_t *iqp;

		int chunk, sn;

		for (chunk=0, sn=0; sn < WF_C_NSAMPS; chunk++) {
			assert(chunk < n_chunks);

			if (overlapped_sampling) {
				evWF(EC_TRIG1, EV_WF, -1, "WF", "CmdGetWFContSamps");
			} else {
				// wait until current chunk is available in WF sample buffer
				now = timer_us64();
				if (now < deadline) {
					u4_t diff = deadline - now;
					if (diff) {
						evWF(EC_EVENT, EV_WF, -1, "WF", "TaskSleep wait chunk buffer");
						TaskSleepS("wait chunk", diff);
						evWF(EC_EVENT, EV_WF, -1, "WF", "TaskSleep wait chunk buffer done");
					}
				}
				deadline += chunk_wait_us;
			}
		
			if (chunk == 0) {
				spi_get_noduplex(first_cmd, miso, NWF_SAMPS * sizeof(iq_t), rx_chan);
			} else
			if (chunk < n_chunks-1) {
				spi_get_noduplex(CmdGetWFSamples, miso, NWF_SAMPS * sizeof(iq_t), rx_chan);
			}

			evWFC(EC_EVENT, EV_WF, -1, "WF", evprintf("%s SAMPLING chunk %d",
				overlapped_sampling? "OVERLAPPED":"NON-OVERLAPPED", chunk));
			
			iqp = (iq_t*) &(miso->word[0]);
			
			for (k=0; k<NWF_SAMPS; k++) {
				if (sn >= WF_C_NSAMPS) break;
				ii = (s4_t) (s2_t) iqp->i;
				qq = (s4_t) (s2_t) iqp->q;
				iqp++;

				float fi = ((float) ii) * window_function_c[sn];
				float fq = ((float) qq) * window_function_c[sn];
				
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

		// contents of WF DDC pipeline is uncertain when mix freq or decim just changed
		if (wf->flush_wf_pipe) {
			wf->flush_wf_pipe--;
		} else {
			compute_frame(wf, fft);
		}

		int actual = timer_ms() - wf->mark;
		int delay = desired - actual;
		//printf("%d %d %d\n", delay, actual, desired);
		
		// full sampling faster than needed by frame rate
		if (desired > actual) {
			evWF(EC_EVENT, EV_WF, -1, "WF", "TaskSleep wait FPS");
			TaskSleepS("wait frame", delay * 1000);
			evWF(EC_EVENT, EV_WF, -1, "WF", "TaskSleep wait FPS done");
		} else {
			NextTask("loop");
		}
		wf->mark = timer_ms();
	}
}

void compute_frame(wf_t *wf, fft_t *fft)
{
	int i;
	wf_pkt_t out;
	u1_t comp_in_buf[WF_WIDTH];
	float pwr[MAX_FFT_USED];
		
    TaskStatU(0, 0, NULL, TSTAT_INCR|TSTAT_ZERO, 0, "fps");

	//NextTask("FFT1");
	evWF(EC_EVENT, EV_WF, -1, "WF", "compute_frame: FFT start");
	fftwf_execute(fft->hw_dft_plan);
	evWF(EC_EVENT, EV_WF, -1, "WF", "compute_frame: FFT done");
	//NextTask("FFT2");

	u1_t *bp = (wf->compression)? out.un.buf2 : out.un.buf;
			
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
	
	float max_dB = wf->maxdb;
	float min_dB = wf->mindb;
	float range_dB = max_dB - min_dB;
	float pix_per_dB = 255.0 / range_dB;

	int bin=0, _bin=-1, bin2pwr[WF_WIDTH];
	float p, dB, pwr_out_sum[WF_WIDTH], pwr_out_peak[WF_WIDTH];

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
			
			//#define CMA
			if (bin == _bin) {
				#ifdef CMA
					pwr_out_cma[bin] = (pwr_out_cma[bin] * avgs) + p;
					avgs++;
					pwr_out_cma[bin] /= avgs;
				#else
					if (p > pwr_out_peak[bin]) pwr_out_peak[bin] = p;
					pwr_out_sum[bin] += p;
				#endif
			} else {
				#ifdef CMA
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

			dB = 10.0 * log10f(p * wf->fft_scale + (float) 1e-30) + wf->fft_offset;
#if 0
//jks
if (i == 506) printf("Z%d ", wf->zoom);
if (i >= 507 && i <= 515) {
	float peak_dB = 10.0 * log10f(pwr_out_peak[i] * wf->fft_scale + (float) 1e-30) + wf->fft_offset;
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
			// If we map it the reverse way, (u1_t) 0..255 => 0..-255 dBm (which is more natural), then we get
			// noise in the bottom bins due to funny interaction of the reversed values with the
			// ADPCM compression for reasons we don't understand.
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
			
			dB = 10.0 * log10f(p * wf->fft_scale + (float) 1e-30) + wf->fft_offset;
			if (dB > 0) dB = 0;
			if (dB < -200.0) dB = -200.0;
			dB--;
			*bp++ = (u1_t) (int) dB;
		}
	}
	
	strncpy(out.id, "FFT ", 4);
	out.x_bin_server = wf->start;
	out.flags_x_zoom_server = wf->zoom;
	evWF(EC_EVENT, EV_WF, -1, "WF", "compute_frame: fill out buf");

	int bytes;
	ima_adpcm_state_t adpcm_wf;
	
	if (wf->compression) {
		memset(out.un.adpcm_pad, out.un.buf2[0], sizeof(out.un.adpcm_pad));
		memset(&adpcm_wf, 0, sizeof(ima_adpcm_state_t));
		encode_ima_adpcm_u8_e8(out.un.buf, out.un.buf, ADPCM_PAD + WF_WIDTH, &adpcm_wf);
		bytes = (ADPCM_PAD + WF_WIDTH) * sizeof(u1_t) / 2;
		out.flags_x_zoom_server |= WF_FLAGS_COMPRESSION;
	} else {
		bytes = WF_WIDTH * sizeof(u1_t);
	}

	// sync this waterfall line to audio packet currently going out
	int rx_chan = wf->conn->rx_channel;
	snd_t *snd = &snd_inst[rx_chan];
	out.seq = snd->seq;

	app_to_web(wf->conn, (char*) &out, SO_OUT_HDR + bytes);
	waterfall_bytes += bytes;
	waterfall_frames[RX_CHANS]++;
	waterfall_frames[rx_chan]++;
	evWF(EC_EVENT, EV_WF, -1, "WF", "compute_frame: done");

	#if 0
		static u4_t last_time[RX_CHANS];
		u4_t now = timer_ms();
		printf("WF%d: %d %.3fs seq-%d\n", rx_chan, SO_OUT_HDR + bytes,
			(float) (now - last_time[rx_chan]) / 1e3, out.seq);
		last_time[rx_chan] = now;
	#endif
}

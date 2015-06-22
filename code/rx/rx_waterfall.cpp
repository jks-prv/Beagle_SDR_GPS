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

#include "types.h"
#include "config.h"
#include "wrx.h"
#include "misc.h"
#include "web.h"
#include "peri.h"
#include "spi.h"
#include "gps.h"
#include "coroutines.h"
#include "pru_realtime.h"

#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <sched.h>
#include <math.h>
#include <fftw3.h>

//jks
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

#define	MAX_ZOOM		10
#define	MAX_START(z)	((WF_WIDTH << MAX_ZOOM) - (WF_WIDTH << (MAX_ZOOM - z)))

static const int wf_fps[] = { WF_SPEED_SLOW, WF_SPEED_MED, WF_SPEED_FAST, WF_SPEED_OTHERS };

static float window_function_c[WF_C_NSAMPS];

struct wf_t {
	bool init;
	SPI_MISO wf_miso;
	fftwf_plan wf_dft_plan;
	fftwf_complex *wf_c_samps;
	fftwf_complex *wf_fft;
} wf_inst[WF_CHANS];

// w/f transfer pipeline overlap delay
int pipe_zoom_delay[MAX_ZOOM+1] =
//      1,   1,   2,   4,   8,  16,  32,  64, 128, 256, 512		// R
//	   <1,  <1,  <1,   1,   2,   4,   8,  16,  32,  64, 128		// acquisition time (ms)
//	  >99, >99, >99, >99, >99, >99, >99,  64,  32,  16,   8		// acquisition rate (Hz)
#ifdef SPI_32
	{  30,  30,  30,  30,  30,  30,  30,  30,  30,  50,  80 };
#endif		
#ifdef SPI_16
	{  30,  30,  30,  30,  30,  30,  30,  30,  30,  50,  80 };	// TBD
#endif		
#ifdef SPI_8
	{  30,  30,  30,  30,  30,  30,  30,  30,  30,  50,  80 };	// TBD
#endif		

static lock_t waterfall_hw_lock;

void w2a_waterfall_init()
{
	int i;
	
#ifdef USE_WF_PUSH24
	#define WF_BITS 24
	float adc_scale_decim = powf(2, -24);		// gives +/- 0.5 float samples
#else
	#define WF_BITS 16
	float adc_scale_decim = powf(2, -16);		// gives +/- 0.5 float samples
	//float adc_scale_decim = powf(2, -15);		// gives +/- 1.0 float samples
#endif

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
    	window_function_c[i] *= WINDOW_GAIN * (WINDOW_COEF1 - WINDOW_COEF2 * cos( (_2PI*i)/(float)(WF_C_NSAMPS-1) ));
    }
    
	lock_init(&waterfall_hw_lock);
	
	assert(WF_C_NSAMPS == WF_C_NFFT);
	assert(WF_C_NSAMPS <= 8192);	// hardware sample buffer length limitation
}

void w2a_waterfall(void *param)
{
	conn_t *conn = (conn_t *) param;
	int wf_chan;
	wf_t *wf;
	int i, j, k, n, b;
	int mark, delay;
	char cmd[256];
	float max_pwr, min_pwr;
	//float adc_scale_samps = powf(2, -ADC_BITS);

	bool new_map;
	u4_t i_offset = -1;
	int maxdb = 0, mindb = 0;
	int wband, _wband, zoom=-1, _zoom, scale=1, _scale, slow=-1, _slow, dvar=WF_BITS, _dvar=WF_BITS, pipe=1, _pipe, send_dB=0;
	float start=-1, _start;
	int _adc_ampl, adc_ampl;
	bool do_send_msg = FALSE;
	u2_t decim;
	int show_stats = 0;
	float samp_wait, pipe_delay, pipe_samp_wait;
	//u4_t t_prev, t_start, t_get, t_stop;
	u4_t us0, t_loop0;
	float t_fft, t_xfer, t_wait, t_loop;
	float HZperStart = conn->ui->ui_srate / (WF_WIDTH << MAX_ZOOM);

	#define PRE 8
	int pre = PRE;
	u1_t wf1[PRE + WF_WIDTH/2];
	float pwr[MAX_FFT_USED];
	u2_t fft2wf_map[WF_C_NFFT / WF_USING_HALF_FFT];		// map is 1:1 with fft
	u2_t wf2fft_map[WF_WIDTH];							// map is 1:1 with plot
	
	if (WF_CHANS > 1) {
		assert(RX_CHANS == WF_CHANS);
		wf_chan = conn->rx_channel;
		assert(wf_chan < WF_CHANS);
	} else {
		wf_chan = 0;	// just one wf h/w channel shared across multiple rx channels via a lock
	}
	wf = &wf_inst[wf_chan];

	clprintf(conn, "W/F INIT conn %p init %d wf %p\n", conn, wf->init, wf);
	if (!wf->init) {
		wf->wf_c_samps = (fftwf_complex*) fftwf_malloc(sizeof(fftwf_complex) * (WF_C_NSAMPS));
		wf->wf_fft = (fftwf_complex*) fftwf_malloc(sizeof(fftwf_complex) * (WF_C_NFFT));
		wf->wf_dft_plan = fftwf_plan_dft_1d(WF_C_NSAMPS, wf->wf_c_samps, wf->wf_fft, FFTW_FORWARD,
			//FFTW_ESTIMATE);
			FFTW_MEASURE);
		wf->init = true;
	}

	send_msg(conn, "MSG center_freq=%d bandwidth=%d", (int) conn->ui->ui_srate/2, (int) conn->ui->ui_srate);
	send_msg(conn, "MSG use_wf_comp=%d wrx_up=%d", VAL_USE_WF_COMP, SMETER_CALIBRATION + /* bias */ 100);
	u4_t adc_clock_i = roundf(adc_clock);
	send_msg(conn, "MSG fft_size=1024 fft_fps=-20 zoom_max=%d rx_chans=%d adc_clock=%d color_map=%d fft_setup",
		MAX_ZOOM, RX_CHANS, adc_clock_i, color_map? (~conn->ui->color_map)&1 : conn->ui->color_map);

    mark = timer_ms();
    
	while (TRUE) {

		n = web_to_app(conn, cmd, sizeof(cmd));
				
		if (n) {			
			cmd[n] = 0;

//foo
#if 0
if (strcmp(conn->mc->remote_ip, "103.1.181.74") == 0) {
	cprintf(conn, "W/F IGNORE <%s>\n", cmd);
	continue;
}
#endif
			cprintf(conn, "W/F <%s>\n", cmd);

			i = sscanf(cmd, "SET zoom=%d start=%f", &_zoom, &_start);
			if (i == 2) {
				//printf("waterfall: zoom=%d start=%.3f(%.1f)\n",
				//	_zoom, _start, _start * HZperStart / KHz);
				if (zoom != _zoom) {
					zoom = _zoom;
					if (zoom < 0) zoom = 0;
					if (zoom > MAX_ZOOM) zoom = MAX_ZOOM;

#ifdef USE_WF_NEW
					//jks
					decim = 0x0002 << zoom;		// z0-10 R = 2,4,8,16,32,64,128,256,512,1024,2048
#else
					// NB: because we only use half of the FFT with CIC can zoom one level less
					int zm1 = (WF_USING_HALF_CIC == 2)? (zoom? (zoom-1) : 0) : zoom;

					#ifdef USE_WF_1CIC
						if (zm1 == 0) {
							decim = 0x0001;		// R = 1
						} else {
							decim = 1 << zm1;	// R = 2,4,8,16,32,64,128,256,512 for MAX_ZOOM = 10
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
					samp_wait =  WF_C_NSAMPS * (1 << zoom) / adc_clock * 1000;
					if (wf_pipe)
						pipe_delay = wf_pipe;
					else
					if (pipe != 1)
						pipe_delay = pipe;
					else
						pipe_delay = pipe_zoom_delay[zoom];
					
					//jks
					#if 0
					if (!bg) cprintf(conn, "W/F ZOOM %d pipe %.0f decim 0x%04x samp_wait %.3f/%.3f ms\n",
						zoom, pipe_delay, decim, samp_wait - pipe_delay, samp_wait);
					#endif
					do_send_msg = TRUE;
					new_map = TRUE;
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

					float off_freq = start * HZperStart;
					
					//jks
					#ifdef USE_WF_NEW
						off_freq += adc_clock / (4<<zoom);
					#endif
					
					i_offset = (u4_t) (s4_t) (off_freq / adc_clock * pow(2,32));
					i_offset = -i_offset;
					//jks
					#if 0
					if (!bg) cprintf(conn, "W/F z%d OFFSET %.3f KHz i_offset 0x%08x\n",
						zoom, off_freq/KHz, i_offset);
					#endif
					do_send_msg = TRUE;
				//}
				
				if (do_send_msg) {
					send_msg(conn, "MSG zoom=%d start=%d", zoom, (u4_t) start);
					//printf("waterfall: send META zoom %d start %d\n", zoom, u_start);
					do_send_msg = FALSE;
				}
				
				continue;
			}
			
			i = sscanf(cmd, "SET maxdb=%d mindb=%d", &maxdb, &mindb);
			if (i == 2) {
				//printf("waterfall: maxdb=%d mindb=%d\n", maxdb, mindb);
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
				if (slow != _slow) {
					/*
					if (!conn->first_slow && wf_max) {
						slow = 2;
						conn->first_slow = TRUE;
					} else {
					*/
						slow = _slow;
					//}
					//printf("waterfall: SLOW %d\n", slow);
				}
				continue;
			}

			i = sscanf(cmd, "SET dvar=%d", &_dvar);
			if (i == 1) {
				if (dvar <= WF_BITS) cprintf(conn, "W/F dvar=%d 0x%08x\n", _dvar, ~((1 << (WF_BITS - _dvar)) - 1));
				continue;
			}

			i = sscanf(cmd, "SET pipe=%d", &_pipe);
			if (i == 1) {
				pipe = _pipe;
				//printf("waterfall: pipe=%d\n", pipe);
				continue;
			}

			i = sscanf(cmd, "SET send_dB=%d", &send_dB);
			if (i == 1) {
				//cprintf(conn, "W/F send_dB=%d\n", send_dB);
				continue;
			}

			char name[32];
			n = sscanf(cmd, "SERVER DE CLIENT %32s", name);
			if (n == 1) {
				continue;
			}

			// we see these sometimes; not part of our protocol
			if (strcmp(cmd, "PING")) continue;

			// we see these at the close of a connection; not part of our protocol
			if (strcmp(cmd, "?")) continue;

			cprintf(conn, "W/F BAD PARAMS: <%s>\n", cmd);
		}
		
		if (!do_wrx)
			continue;

		if (conn->stop_data) {
			clprintf(conn, "W/F rx_server_remove()\n");
			rx_server_remove(conn);
			panic("shouldn't return");
		}

		if (conn->rx_channel >= wf_num) {
			NextTask();
			continue;
		}
		
		int fft_used = WF_C_NFFT;
		fft_used /= WF_USING_HALF_FFT;		// the result is contained in the first half of a complex FFT
		
		// if any CIC is used (z != 0) only look at half of it to avoid the aliased images
		#ifdef USE_WF_NEW
			fft_used /= WF_USING_HALF_CIC;
		#else
			if (zoom != 0) fft_used /= WF_USING_HALF_CIC;
		#endif
		
		float span = adc_clock / 2 / (1<<zoom);
		float disp_fs = conn->ui->ui_srate / (1<<zoom);
		
		// NB: plot_width can be greater than WF_WIDTH because it relative to the ratio of the
		// (adc_clock/2) / conn->ui->ui_srate, which can be > 1 (hence plot_width_clamped).
		// All this is necessary because we might be displaying less than adc_clock/2 implies because
		// of using third-party obtained frequency scale images in our UI.
		int plot_width = WF_WIDTH * span / disp_fs;
		int plot_width_clamped = (plot_width > WF_WIDTH)? WF_WIDTH : plot_width;
		
		float fft_scale;
		
		if (new_map) {
			assert(fft_used <= MAX_FFT_USED);

			if (fft_used >= plot_width) {
				// >= FFT than plot
				//jks
				#ifdef USE_WF_NEW
					// IQ reverse unwrapping (X)
					for (i=fft_used/2,j=0; i<fft_used; i++,j++) {
						fft2wf_map[i] = plot_width * j/fft_used;
					}
					for (i=0; i<fft_used/2; i++,j++) {
						fft2wf_map[i] = plot_width * j/fft_used;
					}
				#else
					// no unwrap
					for (i=0; i<fft_used; i++) {
						fft2wf_map[i] = plot_width * i/fft_used;
					}
				#endif
				//for (i=0; i<fft_used; i++) printf("%d>%d ", i, fft2wf_map[i]);
			} else {
				// < FFT than plot
				#ifdef USE_WF_NEW
					for (i=0; i<plot_width_clamped; i++) {
						int t = fft_used * i/plot_width;
						if (t >= fft_used/2) t -= fft_used/2; else t += fft_used/2;
						wf2fft_map[i] = t;
					}
				#else
					// no unwrap
					for (i=0; i<plot_width_clamped; i++) {
						wf2fft_map[i] = fft_used * i/plot_width;
					}
				#endif
				//for (i=0; i<plot_width_clamped; i++) printf("%d:%d ", i, wf2fft_map[i]);
			}
			//printf("\n");
			
			// fixme: is this right?
			float maxmag = zoom? fft_used : fft_used/2;
			fft_scale = 10.0 * 2.0 / (maxmag * maxmag);

			#if 0	//jks
			if (!bg) cprintf(conn, "W/F NEW_MAP z%d fft_used %d/%d span %.1f disp_fs %.1f fft_scale %.1e plot_width %d/%d %s FFT than plot\n",
				zoom, fft_used, WF_C_NFFT, span/KHz, disp_fs/KHz, fft_scale, plot_width_clamped, plot_width,
				(plot_width_clamped < fft_used)? ">=":"<");
			#endif

			send_msg(conn, "MSG plot_width=%d", plot_width);
			if (meas) show_stats = 8;
			new_map = FALSE;
		}

		
		// create waterfall
		
		NextTaskL("loop start");
		if (WF_CHANS == 1) lock_enter(&waterfall_hw_lock);

		spi_set_noduplex(CmdSetWFFreq, 0, i_offset);
		spi_set_noduplex(CmdSetWFDecim, decim);
		
		int sn = 0;

		us0 = timer_us();
		evWf(EV_WFSAMPLE, "wf", "CmdWFSample");
//if(z10)printf("samp\n");
		spi_set_noduplex(CmdWFSample);
		//t_prev = t_start;
		//t_start = timer_us();
		pipe_samp_wait = samp_wait - ((show_stats>4)? 0 : pipe_delay);
//printf("zoom %d pipe_samp_wait %d\n", zoom, (int)pipe_samp_wait);
		if (pipe_samp_wait > 1.0) {
			//printf("samp_wait %d\n", (u4_t) pipe_samp_wait);
			yield_ms(pipe_samp_wait);
		}
		t_wait = (float) (timer_us() - us0) / 1000.0;

		SPI_MISO *miso = &(wf->wf_miso);
		s4_t ii, qq;
		
		struct iq_t {
			#ifdef USE_WF_PUSH24
				u1_t q3, i3;	// NB: endian swap
			#endif
			u2_t i, q;
		} __attribute__((packed));
		iq_t *iqp;

		us0 = timer_us();
//if (zoom==10) ev(EV_TRIGGER, "", "");
		evWf(-EV_WFSAMPLE, "wf", "CmdGetWFSamples first");
		for (; sn < WF_C_NSAMPS;) {

			//t_get = timer_us();
			
			#ifdef USE_WF_PUSH24
				#define SO_IQ_T 6
			#else
				#define SO_IQ_T 4
			#endif
			
			assert(sizeof(iq_t) == SO_IQ_T);
			#if !((NWF_SAMPS * SO_IQ_T) <= NSPI_RX)
				#error !((NWF_SAMPS * SO_IQ_T) <= NSPI_RX)
			#endif

			spi_get_noduplex(CmdGetWFSamples, miso, NWF_SAMPS * sizeof(iq_t));
			//spi_get(CmdGetWFSamples, miso, NWF_SAMPS * sizeof(iq_t));
			evWf(EV_WFSAMPLE, "wf", "CmdGetWFSamples");
			iqp = (iq_t*) &(miso->word[0]);
			
			for (k=0; k<NWF_SAMPS; k++) {
				if (sn >= WF_C_NSAMPS) break;

				#ifdef USE_WF_PUSH24
					//assert(iqp->i3 == 0);
					//assert(iqp->q3 == 0);
					ii = S24_16_8(iqp->i, iqp->i3);
					qq = S24_16_8(iqp->q, iqp->q3);
				#else
					ii = (s4_t) (s2_t) iqp->i;
					qq = (s4_t) (s2_t) iqp->q;
				#endif
				if (dvar <= WF_BITS) {
					u4_t mask = ~((1 << (WF_BITS - dvar)) - 1);
					ii &= mask;
					qq &= mask;
				}
				iqp++;
				
				wf->wf_c_samps[sn][I] = ((float) ii) * window_function_c[sn];
				wf->wf_c_samps[sn][Q] = ((float) qq) * window_function_c[sn];
//jks
//printf("s%d#%d:%d|%d ", wf_chan, sn, ii, qq);
				sn++;
			}
			
			NextTaskL("loop");
		}
		evWf(EV_WFSAMPLE, "wf", "CmdGetWFSamples last");

		if (dvar != _dvar) dvar = _dvar;
		t_xfer = (float) (timer_us() - us0) / 1000.0;

		//t_stop = timer_us();
		if (WF_CHANS == 1) lock_leave(&waterfall_hw_lock);
		NextTaskL("loop end");
		
		//if ((conn->rx_channel==do_slice) && (conn->nloop++==50)) StartSlice();

		u1_t wf2[PRE + WF_WIDTH];
		//memset(wf2, 0, sizeof(wf2));
		
		us0 = timer_us();
		fftwf_execute(wf->wf_dft_plan);
		t_fft = (float) (timer_us() - us0) / 1000.0;
		NextTaskL("FFT");

		int mi=-1;
		//memset(pwr, 0, sizeof(pwr));

		for (i=0; i<fft_used; i++) {
			float p = wf->wf_fft[i][I]*wf->wf_fft[i][I] + wf->wf_fft[i][Q]*wf->wf_fft[i][Q];
			//p = sqrt(p);
			//if (p > max_pwr) { max_pwr = p; mi=i; }
			//if (p < min_pwr) min_pwr = p;
			pwr[i]=p;
//jks
//printf("p%d#%d:%.0f ", wf_chan, i, p);
		}
		NextTaskL("pwr");
		
		// zero-out the DC component in bin zero/one (around -90 dBFS)
		// otherwise when scrolling w/f it will move but then not continue at the new location
		pwr[0] = 0;
		pwr[1] = 0;
//printf("FFT max %.3f @%d min %.3f\n", max_pwr, mi, min_pwr);
		
		// fixme proper power-law scaling..
		
		// from the tutorials at http://www.fourier-series.com/fourierseries2/flash_programs/DFT_windows/index.html
		// recall:
		// pwr = mag*mag			
		// pwr = i*i + q*q
		// mag = sqrt(i*i + q*q)
		// pwr = mag*mag = i*i + q*q
		// pwr dB = 10 * log10(pwr)		i.e. no sqrt() needed
		// pwr dB = 20 * log10(mag)
		
		double max_dB = maxdb;
		double min_dB = mindb;
		double range_dB = max_dB - min_dB;
		double pix_per_dB = 255.0 / range_dB;

		int bin=0, _bin=-1;

		float p, dB, y, maxy;
		if (fft_used >= plot_width) {
			// >= FFT than plot
//jks
//printf(">= FFT: fft_used %d pix_per_dB %.3f range %.0f:%.0f\n", fft_used, pix_per_dB, max_dB, min_dB);
			maxy = 0;
			for (i=0; i<fft_used; i++) {
				p = pwr[i];
				
				dB = 10.0 * log10 (p * fft_scale + 1e-60);
				y = dB + SMETER_CALIBRATION;
				if (send_dB) {
					if (y > 127.0) y = 127.0;
					if (y < -128.0) y = -128.0;
				} else {
					y = (y - min_dB) * pix_per_dB;
					if (y > 255.0) y = 255.0;
					if (y < 0) y = 0;
				}
				
				bin = fft2wf_map[i];
				if (bin < WF_WIDTH) {
					if (bin == _bin) {
						if (p > max_pwr) {
							wf2[pre+bin] = (u1_t) (int) y;
							max_pwr = p;
						}
					} else {
						wf2[pre+bin] = (u1_t) (int) y;
						max_pwr = p;
						_bin = bin;
					}
				}
			}
		} else {
			// < FFT than plot
//jks
//printf("< FFT: fft_used %d pix_per_dB %.3f range %.0f:%.0f\n", fft_used, pix_per_dB, max_dB, min_dB);
			for (i=0; i<plot_width_clamped; i++) {
				p = pwr[wf2fft_map[i]];
				
				dB = 10.0 * log10 (p * fft_scale + 1e-60);
				y = dB + SMETER_CALIBRATION;
				if (send_dB) {
					if (y > 127.0) y = 127.0;
					if (y < -128.0) y = -128.0;
				} else {
					y = (y - min_dB) * pix_per_dB;
					if (y > 255.0) y = 255.0;
					if (y < 0) y = 0;
				}

				wf2[pre+i] = (u1_t) (int) y;
			}
		}

		// simple compression
		NextTaskL("plot");
	
		if (VAL_USE_WF_COMP) {
			for (i=0; i<WF_WIDTH; i+=2) {
				u1_t byte = (wf2[pre+i] >> 4) | (wf2[pre+i+1] & 0xf0);
				if (i==0 && byte==0xff) byte = 0xfe;	// avoid meta flag
				wf1[pre+i/2] = byte;
			}
		} else {
			if (wf2[pre+0]==0xff) wf2[pre+0] = 0xfe;	// avoid meta flag
		}

		delay = wf_fps[slow]? (1000/wf_fps[slow] - (timer_ms() - mark)) : 0;
//jks
//printf("slow %d delay %d\n", slow, delay);
		if (delay > 0) TaskSleep(delay * 1000);
		mark = timer_ms();
		#if 0
		conn->wf_frames++;
		conn->wf_loop = t_start - t_prev;
		conn->wf_lock = t_stop - t_start;
		conn->wf_get = t_stop - t_get;
		#endif

		NextTaskL("a2w begin");
		if (VAL_USE_WF_COMP) {
			if (pre) strncpy((char*) wf1, "FFT ", 4);
			app_to_web(conn, (char*) wf1, pre+WF_WIDTH/2);
			waterfall_bytes += WF_WIDTH/2;
		} else {
			if (pre) strncpy((char*) wf2, "FFT ", 4);
			app_to_web(conn, (char*) wf2, pre+WF_WIDTH);
			waterfall_bytes += WF_WIDTH;
		}
		waterfall_frames++;
		NextTaskL("a2w end");

		t_loop = (float) (timer_us() - t_loop0) / 1000.0;
		t_loop0 = timer_us();
		if (show_stats) {
			cprintf(conn, "W/F z%d delay %d pipe %.0f swait %.1f wait %.1f xfer %.1f fft %d %.1f loop %.1f/%.1f ms %.1f fps %.3f mbps\n",
				zoom, delay, (show_stats>4)? 0:pipe_delay, (pipe_samp_wait<0)? 0:pipe_samp_wait, t_wait, t_xfer, WF_C_NSAMPS, t_fft, t_wait + t_xfer + t_fft, t_loop, 1.0/(t_loop/1000.0),
				WF_C_NSAMPS*4*8 * 1000.0/t_xfer / 1000000.0);
			show_stats--;
		}
	}
}

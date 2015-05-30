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
#include "timer.h"
#include "web.h"
#include "peri.h"
#include "spi.h"
#include "gps.h"
#include "coroutines.h"
#include "pru_realtime.h"
#include "datatypes.h"
#include "agc.h"
#include "fir.h"
#include "fastfir.h"
#include "wspr.h"

#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/errno.h>
#include <time.h>
#include <sched.h>
#include <math.h>
#include <limits.h>

const char *mode_s[] = { "am", "ssb" };

#define	LPF_1K0		0
#define	LPF_2K7		1
#define	LPF_4K0		2

static SPI_MISO rx_miso[RX_CHANS];
char guard_rx1[32*K];		// fixme: remove
static TYPECPX samples[RX_CHANS][FASTFIR_OUTBUF_SIZE];
float g_genfreq, g_genampl, g_mixfreq;

u4_t rx0_twoke;

#if WSPR_NSAMPS == 45000
	static int wspr_sn, wspr_sn2;
	extern TYPECPX wspr_samps[WSPR_NSAMPS];
#endif

void w2a_sound_init()
{
}

void w2a_sound(void *param)
{
	conn_t *conn = (conn_t *) param;
	conn_t *cw = conn+1;
	assert(cw->type == STREAM_WATERFALL);
	assert(cw->rx_channel == conn->rx_channel);
	int rx_chan = conn->rx_channel;
	int j, k, n;
	static u4_t ncnt[RX_CHANS];
	char cmd[256];	// fixme: better bounds protection
	char name[256];
	
	double freq=-1, _freq, gen=0, _gen, locut=0, _locut, hicut=0, _hicut, mix;
	int mode, squelch=-1, _squelch, autonotch=-1, _autonotch, genattn=0, _genattn, mute, wspr, keepalive;
	double z1 = 0;
	int lpf=-1;
	bool wspr_inited=FALSE;

	double frate = adc_clock / (RX1_DECIM * RX2_DECIM);
	int rate = (int) round(frate);
	double gen_phase_acc=0, gen_phase_inc = _2PI * (tone+100*rx_chan) / frate;
	
	#define ATTACK_TIMECONST .01	// attack time in seconds
	double sMeterAlpha = 1.0 - exp(-1.0/(frate * ATTACK_TIMECONST));
	double sMeterAvg = 0;

	send_msg(conn, "MSG center_freq=%d bandwidth=%d", (int) conn->ui->ui_srate/2, (int) conn->ui->ui_srate);
	send_msg(conn, "MSG audio_rate=%d audio_setup", rate);

	if (do_wrx) {
		spi_set(CmdSetGen, 0, 0);
		spi_set(CmdSetGenAttn, 0, 0);
		//printf("SOUND ENABLE channel %d\n", rx_chan);
		spi_set(CmdEnableRX, rx_chan, 1);
	}

	int agc = 1, _agc, hang = 0, _hang;
	int thresh = -90, _thresh, manGain = 0, _manGain, slope = 0, _slope, decay = 50, _decay;
	u4_t ka_time = timer_ms();
	char hdr[6]; strncpy(hdr, "AUD ", 4);

	while (TRUE) {
		float f_phase;
		u4_t i_phase;
		
		n = web_to_app(conn, cmd, sizeof(cmd));
		
		if (n) {
			cmd[n] = 0;
			ka_time = timer_ms();

			printf("sound %d: <%s>\n", rx_chan, cmd);

			n = sscanf(cmd, "SET keepalive=%d", &keepalive);
			if (n == 1) {
				continue;
			}

			//printf("sound %d: <%s>\n", rx_chan, cmd);

			n = sscanf(cmd, "SET mod=%127s low_cut=%lf high_cut=%lf freq=%lf", name, &_locut, &_hicut, &_freq);
			if (n == 4) {
				//printf("sound %d: f=%.3f lo=%.3f hi=%.3f mode=%s\n", rx_chan, _freq, _locut, _hicut, name);

				if (freq != _freq) {
					freq = _freq;
					f_phase = freq * KHz / adc_clock;
					i_phase = f_phase * pow(2,32);
					printf("sound %d: FREQ %.3f KHz i_phase 0x%08x\n", rx_chan, freq, i_phase);
					if (do_wrx) spi_set(CmdSetRXFreq, rx_chan, i_phase);
				}
				
				mode = str2enum(name, mode_s, ARRAY_LEN(mode_s));
				assert(mode != ENUM_BAD);
			
				if (hicut != _hicut || locut != _locut) {
					hicut = _hicut; locut = _locut;
					
					// primary filtering
					int fmax = frate/2 - 1;
					if (hicut > fmax) hicut = fmax;
					if (locut < -fmax) locut = -fmax;
					
					// bw for LPF or post AM det is max of hi/lo filter cuts
					float bw = fmaxf(fabsf(hicut), fabsf(locut));
					if (bw > frate/2) bw = frate/2;
					int _lpf = (bw<=1000)? LPF_1K0 : ((bw<=2700)? LPF_2K7 : LPF_4K0);
					if (lpf != _lpf) {
						lpf = _lpf;
						send_msg(conn, "MSG lpf=%d", lpf);
					}
					printf("sound %d: LOcut %.0f HIcut %.0f BW %.0f/%.0f LPF %d\n", rx_chan, locut, hicut, bw, frate/2, lpf);
					
					#define CW_OFFSET 0		// fixme: how is cw offset handled exactly?
					m_FastFIR[rx_chan].SetupParameters(locut, hicut, CW_OFFSET, frate);
					
					// post AM detector filter
					m_AM_FIR[rx_chan].InitLPFilter(0, 1.0, 50.0, bw, bw*1.8, frate);
				}
				
				double nomfreq = freq;
				if ((hicut-locut) < 1000) nomfreq += (hicut+locut)/2/KHz;	// cw filter correction
				nomfreq = round(nomfreq*KHz)/KHz;
				conn->freqHz = round(nomfreq*KHz/10.0)*10;	// round 10 Hz
				conn->mode = mode;
				
				continue;
			}
			
			strcpy(name, &cmd[9]);		// can't use sscanf because name might have embedded spaces
			n = (strncmp(cmd, "SET name=", 9) == 0);
			bool noname = (strcmp(cmd, "SET name=") == 0 || strcmp(cmd, "SET name=(null)") == 0);
			if (n || noname) {
				bool setUserIP = false;
				if (noname && !conn->user) setUserIP = true;
				if (noname && conn->user && strcmp(conn->user, conn->mc->remote_ip)) setUserIP = true;
				if (setUserIP) {
					wrx_str_redup(&conn->user, "user", conn->mc->remote_ip);
					conn->isUserIP = TRUE;
				}
				if (!noname) {
					mg_url_decode(name, 128, name, 128, 0);		// dst=src is okay because length dst always <= src
					wrx_str_redup(&conn->user, "user", name);
					conn->isUserIP = FALSE;
				}
				
				if (!conn->arrived) {
					//loguser(conn, LOG_ARRIVED);
					loguser(conn, "(ARRIVED)");
					conn->arrived = TRUE;
					//conn->arrival = timer_sec();
				}
			
				continue;
			}

			n = sscanf(cmd, "SET gen=%lf mix=%lf", &_gen, &mix);
			if (n == 2) {
				//printf("MIX %f %d\n", mix, (int) mix);
				if (gen != _gen) {
					gen = _gen;
					f_phase = gen * KHz / adc_clock;
					i_phase = f_phase * pow(2,32);
					//printf("sound %d: GEN  %.3f KHz phase %.3f 0x%08x\n",
					//	rx_chan, gen, f_phase, i_phase);
					if (do_wrx) spi_set(CmdSetGen, 0, i_phase);
					if (do_wrx) clr_set_ctrl(CTRL_USE_GEN, gen? CTRL_USE_GEN:0);
					if (rx_chan == 0) g_genfreq = gen * KHz / conn->ui->ui_srate;
				}
				if (rx_chan == 0) g_mixfreq = mix;
			
				continue;
			}

			n = sscanf(cmd, "SET genattn=%d", &_genattn);
			if (n == 1) {
				if (genattn != _genattn) {
					genattn = _genattn;
					if (do_wrx) spi_set(CmdSetGenAttn, 0, (u4_t) genattn);
					//printf("===> CmdSetGenAttn %d\n", genattn);
					if (rx_chan == 0) g_genampl = genattn / (float)((1<<17)-1);
				}
			
				continue;
			}

			n = sscanf(cmd, "SET agc=%d hang=%d thresh=%d slope=%d decay=%d manGain=%d",
				&_agc, &_hang, &_thresh, &_slope, &_decay, &_manGain);
			if (n == 6) {
				agc = _agc;
				hang = _hang;
				thresh = _thresh;
				slope = _slope;
				decay = _decay;
				manGain = _manGain;
				//printf("m_Agc..\n");
				m_Agc[rx_chan].SetParameters(agc, hang, thresh, manGain, slope, decay, frate);
				continue;
			}

			n = sscanf(cmd, "SET squelch=%d", &_squelch);
			if (n == 1) {
				squelch = _squelch;		// fixme
				continue;
			}

			n = sscanf(cmd, "SET autonotch=%d", &_autonotch);
			if (n == 1) {
				autonotch = _autonotch;		// fixme
				wf_olap = autonotch? 8:1;
				printf("wf_olap=%d\n", wf_olap);
				continue;
			}

			n = sscanf(cmd, "SET geo=%127s", name);
			if (n == 1) {
				mg_url_decode(name, 128, name, 128, 0);		// dst=src is okay because length dst always <= src
				wrx_str_redup(&conn->geo, "geo", name);
				continue;
			}

			n = sscanf(cmd, "SET mute=%d", &mute);
			if (n == 1) {
				printf("mute %d\n", mute);
				continue;
			}

			n = strncmp(cmd, "SET geojson=", 12);
			if (n == 0) {
				mg_url_decode(cmd+12, 256-12, name, 256-12, 0);
				lprintf("rx%d geojson: <%s>\n", conn->rx_channel, name);
				continue;
			}

			n = strncmp(cmd, "SET browser=", 12);
			if (n == 0) {
				mg_url_decode(cmd+12, 256-12, name, 256-12, 0);
				lprintf("rx%d browser: <%s>\n", conn->rx_channel, name);
				continue;
			}

			n = sscanf(cmd, "SET wspr=%d", &wspr);
			if (n == 1) {
				if (!wspr_inited) {
					wspr_init(cw, frate);	// can't do before waterfall has started
					wspr_inited = TRUE;
				}
				continue;
			}

			n = sscanf(cmd, "SERVER DE CLIENT %s", name);
			if (n == 1) {
				continue;
			}

			printf("sound %d: BAD PARAMS: <%s>\n", rx_chan, cmd);
		}
		
		if (!do_wrx) {
			NextTask();
			continue;
		}

		if ((timer_ms() - ka_time) > KEEPALIVE_MS) {
		
			// ask waterfall task to stop (must do while not, for example, holding a lock)
			conn_t *cw = conn+1;
			assert(cw->type == STREAM_WATERFALL);
			assert(cw->rx_channel == conn->rx_channel);
			cw->stop_data = TRUE;
			
			rx_server_remove(conn);
			panic("shouldn't return");
		}

		if (rx_chan >= rx_num) {
			NextTask();
			continue;
		}
		
		char *bp;
		SPI_MISO *miso = &rx_miso[rx_chan], cpu;

		char buf_s1[FASTFIR_OUTBUF_SIZE*4 + 4+2];		// fixme: larger
		bp = &buf_s1[4+2];
		u2_t bc=0;

		while (bc < 1024) {		// fixme: larger?
			s4_t i, q;
			
			struct iq_t {
				u2_t i, q;
				u1_t q3, i3;	// NB: endian swap
			} __attribute__((packed));
			iq_t *iqp;

			//if ((conn->rx_channel==do_slice) && (conn->nloop++==512)) StartSlice();
			TaskSleep(0);
			//printf("%d", rx_chan); fflush(stdout);
			evSnd(EV_SND, "woke", "");

			//if (NextTaskM()) sprintf(NextTaskM(), "cnt %d", cnt);
			//NextTaskL("CmdGetRX");
			spi_get_noduplex(CmdGetRX, miso, NRX_SAMPS * sizeof(iq_t), 1<<rx_chan, rx_chan);
			//spi_get(CmdGetRX, miso, NRX_SAMPS * sizeof(iq_t), 1<<rx_chan, rx_chan);
			evSnd(EV_SND, "get", "");
			rx0_twoke = timer_us();
			iqp = (iq_t*) &(miso->word[0]);
			TYPECPX *samps = samples[rx_chan];
			double re, im;

			#define	RXOUT_SCALE	(RXO_BITS-1)	// s24 -> +/- 1.0
			#define	CUTESDR_SCALE	15			// +/- 1.0 -> +/- 32.0K (s16 equivalent)
			
			for (j=0; j<NRX_SAMPS; j++) {
				
				if (tone) {
					re = cos(gen_phase_acc);
					im = sin(gen_phase_acc);
					re /= 2;
					im /= 2;
					gen_phase_acc += gen_phase_inc;
					
					i = re * (pow(2, RXOUT_SCALE)-1);
					q = im * (pow(2, RXOUT_SCALE)-1);
					i >>= 1;
					q >>= 1;

					static s4_t maxi, mini;
					if (i > maxi) { maxi = i; printf("MAXI 0x%08x\n", maxi); }
					if (i < mini) { mini = i; printf("mini 0x%08x\n", mini); }
				} else {
					i = S24_8_16(iqp->i3, iqp->i);
					q = S24_8_16(iqp->q3, iqp->q);
				}

				// NB: i&q reversed to get correct sideband polarity; fixme: why?
				// [probably because mixer NCO polarity is wrong, i.e. cos/sin should really be cos/-sin]
				samps->re = q * pow(2, -RXOUT_SCALE + CUTESDR_SCALE);
				samps->im = i * pow(2, -RXOUT_SCALE + CUTESDR_SCALE);
				samps++; iqp++; ncnt[rx_chan]++;
			}
			
			samps = samples[rx_chan];
			int nsamp = NRX_SAMPS;

			nsamp = m_FastFIR[rx_chan].ProcessData(nsamp, samps, samps);
			//printf("bc %d nsamps %d\n", bc, nsamp);
			if (!nsamp) continue;
			for (j=0; j<nsamp; j++) {

				// s-meter from CuteSDR
				#define MAX_VAL ((float) ((1 << (CUTESDR_SCALE-2)) - 1))
				#define MAX_PWR (MAX_VAL * MAX_VAL)
				double pwr = samps->re*samps->re + samps->im*samps->im;
				double pwr_dB = 10.0 * log10((pwr / MAX_PWR) + 1e-60);
				sMeterAvg = (1.0 - sMeterAlpha)*sMeterAvg + sMeterAlpha*pwr_dB;
				samps++;
			}
			
			samps = samples[rx_chan];
			
			u4_t wspr_freq = round(freq * KHz);
			#if WSPR_NSAMPS == 1
				wspr_data(wspr, wspr_freq, nsamp, samps);
			#endif
			#if WSPR_NSAMPS == 45000
				if (wspr == 1) wspr_data(1, wspr_freq, nsamp, samps);
				if (wspr == 2) {
					if (wspr_sn < (WSPR_NSAMPS+0)) {
						wspr_data(2, wspr_freq, nsamp, &wspr_samps[wspr_sn]); wspr_sn += nsamp;
					} else {
						wspr_sn = wspr_sn2 = 0;
						wspr = 0;
					}
				}
			#endif
			
			m_Agc[rx_chan].ProcessData(nsamp, samps, samps);
			TYPEREAL r_samps[FASTFIR_OUTBUF_SIZE];

			for (j=0; j<nsamp; j++) {

				if (mode == MODE_AM) {
					// AM detector from CuteSDR
					double pwr = samps->re*samps->re + samps->im*samps->im;
					double mag = sqrt(pwr);
					#define DC_ALPHA 0.99
					double z0 = mag + (z1 * DC_ALPHA);
					r_samps[j] = z0-z1;
					z1 = z0;
				} else

				if (mode == MODE_SSB) {
					r_samps[j] = samps->re;
				}

				samps++;
			}

			if (mode == MODE_AM) {
				m_AM_FIR[rx_chan].ProcessFilter(nsamp, r_samps, r_samps);
			}

			for (j=0; j<nsamp; j++) {
				//#define SCALE_AUDIO 0x7fff
				#define SCALE_AUDIO 1		// i.e. CuteSDR floating point and s16 audio use same scaling
				s4_t audio = r_samps[j] * SCALE_AUDIO;
				*bp++ = (audio>>8) & 0xff; bc++;	// choose a network byte-order (big endian)
				*bp++ = (audio>>0) & 0xff; bc++;
			}

			#if 0
			static u4_t last_time[RX_CHANS];
			if (ncnt[rx_chan] >= rate) {
				u4_t now = timer_ms();
				printf("sound %d: %d KHz %.3f secs ", rx_chan, rate, (float) (now - last_time[rx_chan]) / 1e3);
				SPI_MISO status;
				spi_get_noduplex(CmdGetStatus, &status, 2);
				if (status.word[0] & STAT_OVFL) {
					printf("OVERFLOW ==============================================");
					spi_set_noduplex(CmdClrRXOvfl);
				}
				printf("\n");
				ncnt[rx_chan] = 0;
				last_time[rx_chan] = now;
			}
			#endif
		}
		
		if (tone) gen_phase_acc = fmod(gen_phase_acc, _2PI);

		NextTaskL("a2w begin");
		
		#define SMETER_BIAS 127.0
		
		// send s-meter data with each audio packet
		double sMeter_dBm = sMeterAvg + SMETER_CALIBRATION;
		if (sMeter_dBm < -127.0) sMeter_dBm = -127.0; else
		if (sMeter_dBm >    3.4) sMeter_dBm =    3.4;
		u2_t sMeter = (u2_t) ((sMeter_dBm + SMETER_BIAS) * 10);
		assert(sMeter <= 0x0fff);
		hdr[4] = (sMeter >> 8) & 0xf;
		hdr[5] = sMeter & 0xff;

		//printf("S%d ", bc+2); fflush(stdout);
		strncpy(buf_s1, hdr, 6);
		app_to_web(conn, buf_s1, bc+6);
		audio_bytes += bc+2;
		
		NextTaskL("a2w end");
	}
}

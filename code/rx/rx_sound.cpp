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
#include "kiwi.h"
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
#include "debug.h"
#include "data_pump.h"

#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/errno.h>
#include <time.h>
#include <sched.h>
#include <math.h>
#include <limits.h>

const char *mode_s[6] = { "am", "amn", "usb", "lsb", "cw", "cwn" };

#define	LPF_1K0		0
#define	LPF_2K7		1
#define	LPF_4K0		2

float g_genfreq, g_genampl, g_mixfreq;

#ifdef APP_WSPR
	#if WSPR_NSAMPS == 45000
		static int wspr_sn, wspr_sn2;
		extern TYPECPX wspr_samps[WSPR_NSAMPS];
	#endif
#endif

void w2a_sound_init()
{
	assert(RX_CHANS <= MAX_WU);
	//evSnd(EC_DUMP, EV_SND, 10000, "rx task", "overrun");
	
	if (do_sdr) {
		spi_set(CmdSetGen, 0, 0);
		spi_set(CmdSetGenAttn, 0, 0);
	}
}

#define CMD_FREQ		0x01
#define CMD_MODE		0x02
#define CMD_PASSBAND	0x04
#define CMD_AGC			0x08
#define	CMD_AR_OK		0x10

void w2a_sound(void *param)
{
	conn_t *conn = (conn_t *) param;
	int rx_chan = conn->rx_channel;
	int j, k, n, len, slen;
	static u4_t ncnt[RX_CHANS];
	char cmd[256];	// fixme: better bounds protection
	char name[256];
	const char *s;
	
	double freq=-1, _freq, gen=0, _gen, locut=0, _locut, hicut=0, _hicut, mix;
	int mode, squelch=-1, _squelch, autonotch=-1, _autonotch, genattn=0, _genattn, mute, wspr, keepalive;
	double z1 = 0;
	int lpf=-1;
	bool wspr_inited=FALSE;

	double frate = adc_clock / (RX1_DECIM * RX2_DECIM);
	int rate = (int) floor(frate);
	#define ATTACK_TIMECONST .01	// attack time in seconds
	float sMeterAlpha = 1.0 - expf(-1.0/((float) frate * ATTACK_TIMECONST));
	float sMeterAvg = 0;
	
	// don't start data pump until first connection so GPS search can run at full speed on startup
	static bool data_pump_started;
	if (!data_pump_started) {
		data_pump_init();
		data_pump_started = true;
	}

	send_msg(conn, SM_DEBUG, "MSG center_freq=%d bandwidth=%d", (int) conn->ui->ui_srate/2, (int) conn->ui->ui_srate);
	send_msg(conn, SM_DEBUG, "MSG audio_rate=%d audio_setup", rate);
	send_msg(conn, SM_DEBUG, "MSG client_ip=%s", conn->mc->remote_ip);

	if (do_sdr) {
		//printf("SOUND ENABLE channel %d\n", rx_chan);
		rx_enable(rx_chan, true);
	}

	int agc = 1, _agc, hang = 0, _hang;
	int thresh = -90, _thresh, manGain = 0, _manGain, slope = 0, _slope, decay = 50, _decay;
	int arate_in, arate_out;
	u4_t ka_time = timer_ms(), keepalive_count = 0;
	int adc_clk_corr = 0;
	
	int tr_cmds = 0;
	u4_t cmd_recv = 0;

	clprintf(conn, "SND INIT conn %p\n", conn);
	
	while (TRUE) {
		float f_phase;
		u4_t i_phase;
		
		// reload freq NCO if adc clock has been corrected
		if (freq >= 0 && adc_clk_corr != gps.adc_clk_corr) {
			adc_clk_corr = gps.adc_clk_corr;
			f_phase = freq * KHz / adc_clock;
			i_phase = f_phase * pow(2,32);
			if (do_sdr) spi_set(CmdSetRXFreq, rx_chan, i_phase);
			//printf("SND%d freq updated due to ADC clock correction\n", rx_chan);
		}

		n = web_to_app(conn, cmd, sizeof(cmd));
		
		if (n) {
			cmd[n] = 0;

//foo
#if 0
if (strcmp(conn->mc->remote_ip, "103.1.181.74") == 0) {
	cprintf(conn, "SND IGNORE <%s>\n", cmd);
	continue;
}
#endif
			ka_time = timer_ms();

			evDP(EC_EVENT, EV_DPUMP, -1, "SND", evprintf("SND: %s", cmd));

			n = sscanf(cmd, "SET keepalive=%d", &keepalive);
			if (n == 1) {
				keepalive_count++;
				if (tr_cmds++ < 16) {
					clprintf(conn, "SND #%02d <%s>\n", tr_cmds, cmd);
				}
				continue;
			}

			n = sscanf(cmd, "SET geo=%127s", name);
			if (n == 1) {
				len = strlen(name);
				mg_url_decode(name, len, name, len+1, 0);		// dst=src is okay because length dst always <= src
				kiwi_str_redup(&conn->geo, "geo", name);

				// SECURITY
				// Guarantee that geo doesn't contain a double quote which could escape the
				// string argument when ajax response is constructed in rx_server_request().
				char *cp;
				while ((cp = strchr(conn->geo, '"')) != NULL) {
					*cp = '\'';
				}

				continue;
			}

			s = "SET geojson=";
			slen = strlen(s);
			n = strncmp(cmd, s, slen);
			if (n == 0) {
				len = strlen(cmd) - slen;
				mg_url_decode(cmd+slen, len, name, len+1, 0);
				clprintf(conn, "SND geo: <%s>\n", name);
				continue;
			}
			
			s = "SET browser=";
			slen = strlen(s);
			n = strncmp(cmd, s, slen);
			if (n == 0) {
				len = strlen(cmd) - slen;
				mg_url_decode(cmd+slen, len, name, len+1, 0);
				clprintf(conn, "SND browser: <%s>\n", name);
				continue;
			}

			//foo
			if (tr_cmds++ < 16) {
				clprintf(conn, "SND #%02d <%s> cmd_recv 0x%x/0x%x\n", tr_cmds, cmd, cmd_recv,
					CMD_FREQ | CMD_MODE | CMD_PASSBAND | CMD_AGC | CMD_AR_OK);
			} else {
				//cprintf(conn, "SND <%s> cmd_recv 0x%x/0x%x\n", cmd, cmd_recv,
				//	CMD_FREQ | CMD_MODE | CMD_PASSBAND | CMD_AGC | CMD_AR_OK);
			}

			n = sscanf(cmd, "SET dbgAudioStart=%d", &k);
			if (n == 1) {
				continue;
			}

			n = sscanf(cmd, "SET mod=%127s low_cut=%lf high_cut=%lf freq=%lf", name, &_locut, &_hicut, &_freq);
			if (n == 4) {
				//cprintf(conn, "SND f=%.3f lo=%.3f hi=%.3f mode=%s\n", _freq, _locut, _hicut, name);

				if (freq != _freq) {
					freq = _freq;
					f_phase = freq * KHz / adc_clock;
					i_phase = f_phase * pow(2,32);
					//cprintf(conn, "SND FREQ %.3f KHz i_phase 0x%08x\n", freq, i_phase);
					if (do_sdr) spi_set(CmdSetRXFreq, rx_chan, i_phase);
					cmd_recv |= CMD_FREQ;
				}
				
				mode = str2enum(name, mode_s, ARRAY_LEN(mode_s));
				cmd_recv |= CMD_MODE;
				if (mode == ENUM_BAD) {
					clprintf(conn, "SND bad mode <%s>\n", name);
					mode = MODE_AM;
				}
			
				if (hicut != _hicut || locut != _locut) {
					hicut = _hicut; locut = _locut;
					
					// primary passband filtering
					int fmax = frate/2 - 1;
					if (hicut > fmax) hicut = fmax;
					if (locut < -fmax) locut = -fmax;
					
					// bw for LPF or post AM det is max of hi/lo filter cuts
					float bw = fmaxf(fabsf(hicut), fabsf(locut));
					if (bw > frate/2) bw = frate/2;
					int _lpf = (bw<=1000)? LPF_1K0 : ((bw<=2700)? LPF_2K7 : LPF_4K0);
					if (lpf != _lpf) {
						lpf = _lpf;
						send_msg(conn, SM_DEBUG, "MSG lpf=%d", lpf);
					}
					//cprintf(conn, "SND LOcut %.0f HIcut %.0f BW %.0f/%.0f LPF %d\n", rx_chan, locut, hicut, bw, frate/2, lpf);
					
					#define CW_OFFSET 0		// fixme: how is cw offset handled exactly?
					m_FastFIR[rx_chan].SetupParameters(locut, hicut, CW_OFFSET, frate);
					
					// post AM detector filter
					m_AM_FIR[rx_chan].InitLPFilter(0, 1.0, 50.0, bw, bw*1.8, frate);
					cmd_recv |= CMD_PASSBAND;
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
					kiwi_str_redup(&conn->user, "user", conn->mc->remote_ip);
					conn->isUserIP = TRUE;
				}
				if (!noname) {
					len = strlen(name);
					mg_url_decode(name, len, name, len+1, 0);		// dst=src is okay because length dst always <= src
					kiwi_str_redup(&conn->user, "user", name);
					conn->isUserIP = FALSE;
				}
				
				clprintf(conn, "SND name: <%s>\n", cmd);
				if (!conn->arrived) {
					loguser(conn, LOG_ARRIVED);
					conn->arrived = TRUE;
				}
			
				// SECURITY
				// Guarantee that user name doesn't contain a double quote which could escape the
				// string argument when ajax response is constructed in rx_server_request().
				char *cp;
				while ((cp = strchr(conn->user, '"')) != NULL) {
					*cp = '\'';
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
					if (do_sdr) spi_set(CmdSetGen, 0, i_phase);
					if (do_sdr) ctrl_clr_set(CTRL_USE_GEN, gen? CTRL_USE_GEN:0);
					if (rx_chan == 0) g_genfreq = gen * KHz / conn->ui->ui_srate;
				}
				if (rx_chan == 0) g_mixfreq = mix;
			
				continue;
			}

			n = sscanf(cmd, "SET genattn=%d", &_genattn);
			if (n == 1) {
				if (genattn != _genattn) {
					genattn = _genattn;
					if (do_sdr) spi_set(CmdSetGenAttn, 0, (u4_t) genattn);
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
				cmd_recv |= CMD_AGC;
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
				//printf("wf_olap=%d\n", wf_olap);
				continue;
			}

			n = sscanf(cmd, "SET mute=%d", &mute);
			if (n == 1) {
				//printf("mute %d\n", mute);
				continue;
			}

			n = sscanf(cmd, "SET wspr=%d", &wspr);
			if (n == 1) {
				if (!wspr_inited) {
					#ifdef APP_WSPR
					wspr_init(cw, frate);	// can't do before waterfall has started
					#endif
					wspr_inited = TRUE;
				}
				continue;
			}

			n = sscanf(cmd, "SET UAR in=%d out=%d", &arate_in, &arate_out);
			if (n == 2) {
				//clprintf(conn, "UAR in=%d out=%d\n", arate_in, arate_out);
				continue;
			}

			n = sscanf(cmd, "SET AR OK in=%d out=%d", &arate_in, &arate_out);
			if (n == 2) {
				//clprintf(conn, "AR OK in=%d out=%d\n", arate_in, arate_out);
				cmd_recv |= CMD_AR_OK;
				continue;
			}

			n = sscanf(cmd, "SET spi_delay=%d", &j);
			if (n == 1) {
				assert(j == 1 || j == -1);
				spi_delay += j;
				continue;
			}
			
			n = sscanf(cmd, "SERVER DE CLIENT %s", name);
			if (n == 1) {
				continue;
			}
			
			// we see these sometimes; not part of our protocol
			if (strcmp(cmd, "PING") == 0) continue;

			// we see these at the close of a connection; not part of our protocol
			if (strcmp(cmd, "?") == 0) continue;

			clprintf(conn, "SND BAD PARAMS: <%s> ####################################\n", cmd);
		}
		
		if (!do_sdr) {
			NextTask("SND skip");
			continue;
		}

		if (rx_chan >= rx_num) {
			TaskSleep(1000000);
			continue;
		}
		
		// no keep-alive seen for a while or the bug where an initial freq is never set and the connection hangs open
		// and locks-up a receiver channel
		bool keepalive_expired = ((timer_ms() - ka_time) > KEEPALIVE_MS);
		bool connection_hang = (keepalive_count > 4 && conn->freqHz == 0);
		if (keepalive_expired || connection_hang) {
			if (keepalive_expired) clprintf(conn, "SND KEEP-ALIVE EXPIRED\n");
			if (connection_hang) clprintf(conn, "SND CONNECTION HANG\n");
		
			// ask waterfall task to stop (must not do while, for example, holding a lock)
			conn_t *cwf = conn->other;
			assert(cwf);
			assert(cwf->type == STREAM_WATERFALL);
			assert(cwf->rx_channel == conn->rx_channel);
			cwf->stop_data = TRUE;
			
			rx_enable(rx_chan, false);
			clprintf(conn, "SND rx_server_remove()\n");
			rx_server_remove(conn);
			panic("shouldn't return");
		}

		// don't process any audio data until we've received all necessary commands
		if (cmd_recv != (CMD_FREQ | CMD_MODE | CMD_PASSBAND | CMD_AGC | CMD_AR_OK)) {
			TaskSleep(100000);
			continue;
		}
		
		char *bp;
		rx_dpump_t *rx = &rx_dpump[rx_chan];

		struct rx_t {
			char id[4];
			char smeter[2];
			char buf[FASTFIR_OUTBUF_SIZE * sizeof(u2_t)];
		} out;
		
		bp = &out.buf[0];
		u2_t bc=0;
		strncpy(out.id, "AUD ", 4);

		while (bc < 1024) {		// fixme: larger?

			while (rx->wr_pos == rx->rd_pos) {
				evSnd(EC_EVENT, EV_SND, -1, "rx_snd", "sleeping");
				TaskSleep(0);
			}
			
        	TaskStatU(TSTAT_INCR|TSTAT_ZERO, 0, "aud", 0, 0, NULL);

			TYPECPX *i_samps = rx->in_samps[rx->rd_pos];
			rx->rd_pos = (rx->rd_pos+1) & (N_DPBUF-1);
			
			TYPECPX *f_samps = rx->samples[SBUF_FIR];
			int ns_in = NRX_SAMPS, ns_out;
			
			ns_out = m_FastFIR[rx_chan].ProcessData(ns_in, i_samps, f_samps);
			//printf("%d:%d ", ns_in, ns_out); fflush(stdout);
			if (!ns_out) {
				continue;
			}
			
			for (j=0; j<ns_out; j++) {

				// s-meter from CuteSDR
				#define MAX_VAL ((float) ((1 << (CUTESDR_SCALE-2)) - 1))
				#define MAX_PWR (MAX_VAL * MAX_VAL)
				float re = (float) f_samps->re, im = (float) f_samps->im;
				float pwr = re*re + im*im;
				float pwr_dB = 10.0 * log10f((pwr / MAX_PWR) + 1e-30);
				sMeterAvg = (1.0 - sMeterAlpha)*sMeterAvg + sMeterAlpha*pwr_dB;
				f_samps++;
			}
			
			f_samps = rx->samples[SBUF_FIR];
			
			#ifdef APP_WSPR
			u4_t wspr_freq = round(freq * KHz);
			#if WSPR_NSAMPS == 1
				wspr_data(wspr, wspr_freq, ns_out, f_samps);
			#endif
			#if WSPR_NSAMPS == 45000
				if (wspr == 1) wspr_data(1, wspr_freq, ns_out, f_samps);
				if (wspr == 2) {
					if (wspr_sn < (WSPR_NSAMPS+0)) {
						wspr_data(2, wspr_freq, ns_out, &wspr_samps[wspr_sn]); wspr_sn += ns_out;
					} else {
						wspr_sn = wspr_sn2 = 0;
						wspr = 0;
					}
				}
			#endif
			#endif
			
			TYPECPX *a_samps = rx->samples[SBUF_AGC];
			m_Agc[rx_chan].ProcessData(ns_out, f_samps, a_samps);

			// AM detector from CuteSDR
			if (mode == MODE_AM || mode == MODE_AMN) {
				for (j=0; j<ns_out; j++) {
					double pwr = a_samps->re*a_samps->re + a_samps->im*a_samps->im;
					double mag = sqrt(pwr);
					#define DC_ALPHA 0.99
					double z0 = mag + (z1 * DC_ALPHA);
					a_samps->re = z0-z1;
					z1 = z0;
					a_samps++;
				}
				a_samps = rx->samples[SBUF_AGC];
				
				// clean up residual noise left by detector
				m_AM_FIR[rx_chan].ProcessFilterRealOnly(ns_out, a_samps, a_samps);
			}

			for (j=0; j<ns_out; j++) {
				//#define SCALE_AUDIO 0x7fff
				#define SCALE_AUDIO 1		// i.e. CuteSDR floating point and s16 audio use same scaling
				s4_t audio = a_samps->re * SCALE_AUDIO;
				*bp++ = (audio>>8) & 0xff; bc++;	// choose a network byte-order (big endian)
				*bp++ = (audio>>0) & 0xff; bc++;
				a_samps++;
			}

			#if 0
			static u4_t last_time[RX_CHANS];
			static int nctr;
			ncnt[rx_chan] += ns_out;
			int nbuf = ncnt[rx_chan] / rate;
			if (nbuf >= nctr) {
				nctr++;
				u4_t now = timer_ms();
				printf("SND%d: %d %d %.3fs\n", rx_chan, rate, nbuf, (float) (now - last_time[rx_chan]) / 1e3);
				
				#if 0
				static SPI_MISO status;
				spi_get(CmdGetStatus, &status, 2);
				if (status.word[0] & STAT_OVFL) {
					//printf("OVERFLOW ==============================================");
					spi_set(CmdClrRXOvfl);
				}
				#endif

				//ncnt[rx_chan] = 0;
				last_time[rx_chan] = now;
			}
			#endif
		}

		NextTask("a2w begin");
		
		#define SMETER_BIAS 127.0
		
		// send s-meter data with each audio packet
		float sMeter_dBm = sMeterAvg + SMETER_CALIBRATION;
		if (sMeter_dBm < -127.0) sMeter_dBm = -127.0; else
		if (sMeter_dBm >    3.4) sMeter_dBm =    3.4;
		u2_t sMeter = (u2_t) ((sMeter_dBm + SMETER_BIAS) * 10);
		assert(sMeter <= 0x0fff);
		out.smeter[0] = (sMeter >> 8) & 0xf;
		out.smeter[1] = sMeter & 0xff;

		//printf("S%d ", bc); fflush(stdout);
		app_to_web(conn, (char*) &out, sizeof(out.id) + sizeof(out.smeter) + bc);
		audio_bytes += sizeof(out.smeter) + bc;
		
		NextTask("a2w end");
	}
}

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
#include "clk.h"
#include "misc.h"
#include "str.h"
#include "timer.h"
#include "nbuf.h"
#include "web.h"
#include "peri.h"
#include "spi.h"
#include "gps.h"
#include "coroutines.h"
#include "pru_realtime.h"
#include "cuteSDR.h"
#include "agc.h"
#include "fir.h"
#include "fmdemod.h"
#include "debug.h"
#include "data_pump.h"
#include "cfg.h"
#include "mongoose.h"
#include "ima_adpcm.h"
#include "ext_int.h"

#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/errno.h>
#include <time.h>
#include <sched.h>
#include <math.h>
#include <limits.h>

//#define TR_SND_CMDS
#define SM_SND_DEBUG	false

snd_t snd_inst[RX_CHANS];

const char *mode_s[N_MODE] = { "am", "amn", "usb", "lsb", "cw", "cwn", "nbfm", "iq" };
const char *modu_s[N_MODE] = { "AM", "AMN", "USB", "LSB", "CW", "CWN", "NBFM", "IQ" };

float g_genfreq, g_genampl, g_mixfreq;

void c2s_sound_init()
{
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
#define	CMD_ALL			(CMD_FREQ | CMD_MODE | CMD_PASSBAND | CMD_AGC | CMD_AR_OK)

void c2s_sound_setup(void *param)
{
	conn_t *conn = (conn_t *) param;
	double frate = ext_update_get_sample_rateHz(-1);

	send_msg(conn, SM_SND_DEBUG, "MSG center_freq=%d bandwidth=%d", (int) ui_srate/2, (int) ui_srate);
	send_msg(conn, SM_SND_DEBUG, "MSG audio_rate=%d sample_rate=%.3f", SND_RATE, frate);
	send_msg(conn, SM_SND_DEBUG, "MSG client_ip=%s", conn->mc->remote_ip);
}

void c2s_sound(void *param)
{
	conn_t *conn = (conn_t *) param;
	int rx_chan = conn->rx_channel;
	snd_t *snd = &snd_inst[rx_chan];
	rx_dpump_t *rx = &rx_dpump[rx_chan];
	
	int j, k, n, len, slen;
	static u4_t ncnt[RX_CHANS];
	const char *s;
	
	double freq=-1, _freq, gen=-1, _gen, locut=0, _locut, hicut=0, _hicut, mix;
	int mode=-1, _mode, autonotch=-1, _autonotch, genattn=0, _genattn, mute;
	double z1 = 0;

	double frate = ext_update_get_sample_rateHz(rx_chan);      // FIXME: do this in loop to get incremental changes
	//printf("### frate %f SND_RATE %d\n", frate, SND_RATE);
	#define ATTACK_TIMECONST .01	// attack time in seconds
	float sMeterAlpha = 1.0 - expf(-1.0/((float) frate * ATTACK_TIMECONST));
	float sMeterAvg_dB = 0;
	bool compression = true;
	
	snd->seq = 0;
	
    #ifdef SND_SEQ_CHECK
        snd->snd_seq_init = false;
    #endif
    
	m_FmDemod[rx_chan].SetSampleRate(rx_chan, frate);
	m_FmDemod[rx_chan].SetSquelch(0, 0);
	
	// don't start data pump until first connection so GPS search can run at full speed on startup
	static bool data_pump_started;
	if (do_sdr && !data_pump_started) {
		data_pump_init();
		data_pump_started = true;
	}
	
	if (do_sdr) {
		//printf("SOUND ENABLE channel %d\n", rx_chan);
		rx_enable(rx_chan, RX_CHAN_ENABLE);
	}

	int agc = 1, _agc, hang = 0, _hang;
	int thresh = -90, _thresh, manGain = 0, _manGain, slope = 0, _slope, decay = 50, _decay;
	int arate_in, arate_out, acomp;
	u4_t ka_time = timer_sec();
	int adc_clk_corrections = 0;
	
	int tr_cmds = 0;
	u4_t cmd_recv = 0;
	bool cmd_recv_ok = false, change_LPF = false;
	
	memset(&rx->adpcm_snd, 0, sizeof(ima_adpcm_state_t));

	//clprintf(conn, "SND INIT conn: %p mc: %p %s:%d %s\n",
	//	conn, conn->mc, conn->mc->remote_ip, conn->mc->remote_port, conn->mc->uri);
	
	nbuf_t *nb = NULL;
	char name[1024];

	while (TRUE) {
		float f_phase;
		u4_t i_phase;
		
		// reload freq NCO if adc clock has been corrected
		if (freq >= 0 && adc_clk_corrections != conn->adc_clk_corrections) {
			adc_clk_corrections = conn->adc_clk_corrections;
			f_phase = freq * kHz / conn->adc_clock_corrected;
			i_phase = f_phase * pow(2,32);
			if (do_sdr) spi_set(CmdSetRXFreq, rx_chan, i_phase);
			//printf("SND%d freq updated due to ADC clock correction\n", rx_chan);
		}

		if (nb) web_to_app_done(conn, nb);
		n = web_to_app(conn, &nb);
				
		if (n) {
			char *cmd = nb->buf;
			cmd[n] = 0;		// okay to do this -- see nbuf.c:nbuf_allocq()

			ka_time = timer_sec();
    		TaskStatU(TSTAT_INCR|TSTAT_ZERO, 0, "cmd", 0, 0, NULL);

			evDP(EC_EVENT, EV_DPUMP, -1, "SND", evprintf("SND: %s", cmd));
			
			// SECURITY: this must be first for auth check
			if (rx_common_cmd("SND", conn, cmd))
				continue;
			
			#ifdef TR_SND_CMDS
				if (tr_cmds++ < 32) {
					clprintf(conn, "SND #%02d <%s> cmd_recv 0x%x/0x%x\n", tr_cmds, cmd, cmd_recv, CMD_ALL);
				} else {
					//cprintf(conn, "SND <%s> cmd_recv 0x%x/0x%x\n", cmd, cmd_recv, CMD_ALL);
				}
			#endif

			n = sscanf(cmd, "SET dbgAudioStart=%d", &k);
			if (n == 1) {
				continue;
			}

			n = sscanf(cmd, "SET mod=%127s low_cut=%lf high_cut=%lf freq=%lf", name, &_locut, &_hicut, &_freq);
			if (n == 4 && do_sdr) {
				//cprintf(conn, "SND f=%.3f lo=%.3f hi=%.3f mode=%s\n", _freq, _locut, _hicut, name);

				bool new_freq = false;
				if (freq != _freq) {
					freq = _freq;
					f_phase = freq * kHz / conn->adc_clock_corrected;
					i_phase = f_phase * pow(2,32);
					//cprintf(conn, "SND FREQ %.3f kHz i_phase 0x%08x\n", freq, i_phase);
					if (do_sdr) spi_set(CmdSetRXFreq, rx_chan, i_phase);
					cmd_recv |= CMD_FREQ;
					new_freq = true;
				}
				
				_mode = str2enum(name, mode_s, ARRAY_LEN(mode_s));
				cmd_recv |= CMD_MODE;

				if (_mode == NOT_FOUND) {
					clprintf(conn, "SND bad mode <%s>\n", name);
					_mode = MODE_AM;
				}
				
				bool new_nbfm = false;
				if (mode != _mode) {
					mode = _mode;
					if (mode == MODE_NBFM)
						new_nbfm = true;
				}

				if (mode == MODE_NBFM && (new_freq || new_nbfm)) {
					m_FmDemod[rx_chan].Reset();
					conn->last_sample.re = conn->last_sample.im = 0;
				}
			
				if (hicut != _hicut || locut != _locut) {
					hicut = _hicut; locut = _locut;
					
					// primary passband filtering
					int fmax = frate/2 - 1;
					if (hicut > fmax) hicut = fmax;
					if (locut < -fmax) locut = -fmax;
					
					// bw for post AM det is max of hi/lo filter cuts
					float bw = fmaxf(fabs(hicut), fabs(locut));
					if (bw > frate/2) bw = frate/2;
					//cprintf(conn, "SND LOcut %.0f HIcut %.0f BW %.0f/%.0f\n", rx_chan, locut, hicut, bw, frate/2);
					
					#define CW_OFFSET 0		// fixme: how is cw offset handled exactly?
					m_FastFIR[rx_chan].SetupParameters(locut, hicut, CW_OFFSET, frate);
					conn->half_bw = bw;
					
					// post AM detector filter
					// FIXME: not needed if we're doing convolver-based LPF in javascript due to decompression?
					m_AM_FIR[rx_chan].InitLPFilter(0, 1.0, 50.0, bw, bw*1.8, frate);
					cmd_recv |= CMD_PASSBAND;
					
					change_LPF = true;
				}
				
				double nomfreq = freq;
				if ((hicut-locut) < 1000) nomfreq += (hicut+locut)/2/kHz;	// cw filter correction
				nomfreq = round(nomfreq*kHz)/kHz;
				
				conn->freqHz = round(nomfreq*kHz/10.0)*10;	// round 10 Hz
				conn->mode = mode;
				
				continue;
			}
			
			n = sscanf(cmd, "SET gen=%lf mix=%lf", &_gen, &mix);
			if (n == 2) {
				//printf("MIX %f %d\n", mix, (int) mix);
				if (gen != _gen) {
					gen = _gen;
					f_phase = gen * kHz / conn->adc_clock_corrected;
					i_phase = f_phase * pow(2,32);
					//printf("sound %d: GEN %.3f kHz phase %.3f 0x%08x\n",
					//	rx_chan, gen, f_phase, i_phase);
					if (do_sdr) spi_set(CmdSetGen, 0, i_phase);
					if (do_sdr) ctrl_clr_set(CTRL_USE_GEN, gen? CTRL_USE_GEN:0);
					if (rx_chan == 0) g_genfreq = gen * kHz / ui_srate;
				}
				if (rx_chan == 0) g_mixfreq = mix;
			
				continue;
			}

			n = sscanf(cmd, "SET genattn=%d", &_genattn);
			if (n == 1) {
				if (genattn != _genattn) {
					genattn = _genattn;
					if (do_sdr) spi_set(CmdSetGenAttn, 0, (u4_t) genattn);
					//printf("===> CmdSetGenAttn %d 0x%x\n", genattn, genattn);
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
				//printf("AGC %d hang=%d thresh=%d slope=%d decay=%d manGain=%d srate=%.1f\n",
				//	agc, hang, thresh, slope, decay, manGain, frate);
				m_Agc[rx_chan].SetParameters(agc, hang, thresh, manGain, slope, decay, frate);
				cmd_recv |= CMD_AGC;
				continue;
			}

			int squelch, squelch_max;
			n = sscanf(cmd, "SET squelch=%d max=%d", &squelch, &squelch_max);
			if (n == 2) {
				m_FmDemod[rx_chan].SetSquelch(squelch, squelch_max);
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

			n = sscanf(cmd, "SET UAR in=%d out=%d", &arate_in, &arate_out);
			if (n == 2) {
				//clprintf(conn, "UAR in=%d out=%d\n", arate_in, arate_out);
				continue;
			}

			n = sscanf(cmd, "SET AR OK in=%d out=%d", &arate_in, &arate_out);
			if (n == 2) {
				//clprintf(conn, "AR OK in=%d out=%d\n", arate_in, arate_out);
				if (arate_out) cmd_recv |= CMD_AR_OK;
				continue;
			}

			n = sscanf(cmd, "SET underrun=%d", &j);
			if (n == 1) {
				conn->audio_underrun++;
				printf("SND%d: audio underrun %d %s -------------------------\n",
					rx_chan, conn->audio_underrun, conn->user);
				if (ev_dump) evNT(EC_DUMP, EV_NEXTTASK, ev_dump, "NextTask", evprintf("DUMP IN %.3f SEC",
					ev_dump/1000.0));
				continue;
			}

			#ifdef SND_SEQ_CHECK
				int _seq, _sequence;
				n = sscanf(cmd, "SET seq=%d sequence=%d", &_seq, &_sequence);
				if (n == 2) {
					conn->sequence_errors++;
					printf("SND%d: audio.js SEQ got %d, expecting %d, %s -------------------------\n",
						rx_chan, _seq, _sequence, conn->user);
					continue;
				}
			#endif
			
			if (conn->mc != NULL) {
			    clprintf(conn, "SND BAD PARAMS: sl=%d c0=%d c1=%d c2=%d <%s> ####################################\n",
			        strlen(cmd), cmd[0], cmd[1], cmd[2], cmd);
			}
			
			continue;
		} else {
			assert(nb == NULL);
		}
		
		if (!do_sdr) {
			NextTask("SND skip");
			continue;
		}

		if (rx_chan >= rx_num) {
			TaskSleepMsec(1000);
			continue;
		}
		
		if (conn->stop_data) {
			//clprintf(conn, "SND stop_data rx_server_remove()\n");
			rx_enable(rx_chan, RX_CHAN_FREE);
			rx_server_remove(conn);
			panic("shouldn't return");
		}

		// no keep-alive seen for a while or the bug where an initial cmds are not received and the connection hangs open
		// and locks-up a receiver channel
		conn->keep_alive = timer_sec() - ka_time;
		bool keepalive_expired = (conn->keep_alive > KEEPALIVE_SEC);
		bool connection_hang = (conn->keepalive_count > 4 && cmd_recv != CMD_ALL);
		if (keepalive_expired || connection_hang || conn->inactivity_timeout || conn->kick) {
			//if (keepalive_expired) clprintf(conn, "SND KEEP-ALIVE EXPIRED\n");
			//if (connection_hang) clprintf(conn, "SND CONNECTION HANG\n");
		
			// Ask waterfall task to stop (must not do while, for example, holding a lock).
			// We've seen cases where the sound connects, then times out. But the w/f has never connected.
			// So have to check for conn->other being valid.
			conn_t *cwf = conn->other;
			if (cwf && cwf->type == STREAM_WATERFALL && cwf->rx_channel == conn->rx_channel) {
				cwf->stop_data = TRUE;
				
				// only in sound task: disable data pump channel
				rx_enable(rx_chan, RX_CHAN_DISABLE);	// W/F will free rx_chan[]
			} else {
				rx_enable(rx_chan, RX_CHAN_FREE);		// there is no W/F, so free rx_chan[] now
			}
			
			//clprintf(conn, "SND rx_server_remove()\n");
			rx_server_remove(conn);
			panic("shouldn't return");
		}

		// don't process any audio data until we've received all necessary commands
		if (cmd_recv != CMD_ALL) {
			TaskSleepMsec(100);
			continue;
		}
		if (!cmd_recv_ok) {
			#ifdef TR_SND_CMDS
				clprintf(conn, "SND cmd_recv ALL 0x%x/0x%x\n", cmd_recv, CMD_ALL);
			#endif
			cmd_recv_ok = true;
		}
		
		u1_t *bp_real, *bp_iq;
		snd_pkt_t out_pkt;
		
		#define	SND_FLAG_SMETER		0x00
		#define	SND_FLAG_LPF		0x10
		#define	SND_FLAG_ADC_OVFL	0x20
		
		bp_real = &out_pkt.buf_real[0];
		bp_iq = &out_pkt.buf_iq[0];
		u2_t bc = 0;
		strncpy(out_pkt.h.id, "SND ", 4);

		ext_receive_S_meter_t receive_S_meter = ext_users[rx_chan].receive_S_meter;

		while (bc < 1024) {		// fixme: larger?

			while (rx->wr_pos == rx->rd_pos) {
				evSnd(EC_EVENT, EV_SND, -1, "rx_snd", "sleeping");
				TaskSleepReasonUsec("check pointers", 0);
			}
			
        	TaskStatU(0, 0, NULL, TSTAT_INCR|TSTAT_ZERO, 0, "aud");

			TYPECPX *i_samps = rx->in_samps[rx->rd_pos];

			//jksd
			#if 0
                u2_t *tp = rx->ticks[rx->rd_pos];
                static u64_t last_ticks;
                static u4_t tick_seq;
                u64_t ticks = ((u64_t) tp[2]<<32) | (tp[1]<<16) | tp[0];
                u64_t diff_ticks = time_diff48(ticks, last_ticks);
                if ((tick_seq % 32) == 0) printf("ticks %012llx %012llx | %012llx %012llx #%d GPST %f off %.1f\n",
                    ticks, diff_ticks,
                    time_diff48(ticks, clk.ticks), clk.ticks, conn->adc_clk_corrections, clk.gps_secs, clk.offset);
                last_ticks = ticks;
                tick_seq++;
			#endif

		    #ifdef SND_SEQ_CHECK
		        if (rx->in_seq[rx->rd_pos] != snd->snd_seq) {
		            if (!snd->snd_seq_init) {
		                snd->snd_seq_init = true;
		            } else {
		                real_printf("rx%d: got %d expecting %d\n", rx_chan, rx->in_seq[rx->rd_pos], snd->snd_seq);
		            }
		            snd->snd_seq = rx->in_seq[rx->rd_pos];
		        }
		        snd->snd_seq++;
		    #endif
		    
			rx->rd_pos = (rx->rd_pos+1) & (N_DPBUF-1);
			
			TYPECPX *f_samps = &rx->iq_samples[rx->iq_wr_pos][0];
			rx->iq_seqnum[rx->iq_wr_pos] = rx->iq_seq;
			rx->iq_seq++;
			int ns_in = NRX_SAMPS, ns_out;
			
			ns_out = m_FastFIR[rx_chan].ProcessData(rx_chan, ns_in, i_samps, f_samps);

			// FIR has a pipeline delay: ns_in|ns_out = 85|512 85|0 85|0 85|0 85|0 85|0 85|512 ... (85*6 = 510)
			//real_printf("%d|%d ", ns_in, ns_out); fflush(stdout);
			if (!ns_out) {
				continue;
			}

			rx->iq_wr_pos = (rx->iq_wr_pos+1) & (N_DPBUF-1);

			TYPECPX *f_sa = f_samps;
			for (j=0; j<ns_out; j++) {

				// S-meter from CuteSDR
				// FIXME: Why is SND_MAX_VAL less than CUTESDR_MAX_VAL again?
				// And does this explain the need for SMETER_CALIBRATION?
				// Can't remember how this evolved..
				#define SND_MAX_VAL ((float) ((1 << (CUTESDR_SCALE-2)) - 1))
				#define SND_MAX_PWR (SND_MAX_VAL * SND_MAX_VAL)
				float re = (float) f_sa->re, im = (float) f_sa->im;
				float pwr = re*re + im*im;
				float pwr_dB = 10.0 * log10f((pwr / SND_MAX_PWR) + 1e-30);
				sMeterAvg_dB = (1.0 - sMeterAlpha)*sMeterAvg_dB + sMeterAlpha*pwr_dB;
				f_sa++;
			
				// S-meter value in audio packet is sent less often than if we send it from here
				if (receive_S_meter != NULL && (j == 0 || j == ns_out/2))
					receive_S_meter(rx_chan, sMeterAvg_dB + S_meter_cal);
			}
			
			if (ext_users[rx_chan].receive_iq != NULL && mode != MODE_NBFM)
				ext_users[rx_chan].receive_iq(rx_chan, 0, ns_out, f_samps);
			
			if (ext_users[rx_chan].receive_iq_tid != (tid_t) NULL && mode != MODE_NBFM)
				TaskWakeup(ext_users[rx_chan].receive_iq_tid, TRUE, TO_VOID_PARAM(rx_chan));

			TYPEMONO16 *r_samps;
			
            if (mode != MODE_IQ) {
                r_samps = &rx->real_samples[rx->real_wr_pos][0];
                rx->real_seqnum[rx->real_wr_pos] = rx->real_seq;
                rx->real_seq++;
            }
			
			// AM detector from CuteSDR
			if (mode == MODE_AM || mode == MODE_AMN) {
				TYPECPX *a_samps = rx->agc_samples;
				m_Agc[rx_chan].ProcessData(ns_out, f_samps, a_samps);

				TYPEREAL *d_samps = rx->demod_samples;

				for (j=0; j<ns_out; j++) {
					double pwr = a_samps->re*a_samps->re + a_samps->im*a_samps->im;
					double mag = sqrt(pwr);
					#define DC_ALPHA 0.99
					double z0 = mag + (z1 * DC_ALPHA);
					*d_samps = z0-z1;
					z1 = z0;
					d_samps++;
					a_samps++;
				}
				d_samps = rx->demod_samples;
				
				// clean up residual noise left by detector
				// the non-FFT FIR has no pipeline delay issues
				m_AM_FIR[rx_chan].ProcessFilter(ns_out, d_samps, r_samps);
			} else
			
			if (mode == MODE_NBFM) {
				TYPEREAL *d_samps = rx->demod_samples;
				TYPECPX *a_samps = rx->agc_samples;
				m_Agc[rx_chan].ProcessData(ns_out, f_samps, a_samps);
				int sq_nc_open;
				
				// FM demod from CSDR: https://github.com/simonyiszk/csdr
				// also see: http://www.embedded.com/design/configurable-systems/4212086/DSP-Tricks--Frequency-demodulation-algorithms-
                #define fmdemod_quadri_K 0.340447550238101026565118445432744920253753662109375
                float i = a_samps->re, q = a_samps->im;
                float iL = conn->last_sample.re, qL = conn->last_sample.im;
                *d_samps = SND_MAX_VAL * fmdemod_quadri_K * (i*(q-qL) - q*(i-iL)) / (i*i + q*q);
                conn->last_sample = a_samps[ns_out-1];
                a_samps++; d_samps++;
                
                for (j=1; j < ns_out; j++) {
                    i = a_samps->re, q = a_samps->im;
                    iL = a_samps[-1].re, qL = a_samps[-1].im;
                    *d_samps = SND_MAX_VAL * fmdemod_quadri_K * (i*(q-qL) - q*(i-iL)) / (i*i + q*q);
                    a_samps++; d_samps++;
                }
                
                d_samps = rx->demod_samples;

                // use the noise squelch from CuteSDR
                sq_nc_open = m_FmDemod[rx_chan].PerformNoiseSquelch(ns_out, d_samps, r_samps);
				
				if (sq_nc_open != 0) {
					send_msg(conn, SM_NO_DEBUG, "MSG squelch=%d", (sq_nc_open == 1)? 1:0);
				}
			} else
			
			if (mode != MODE_IQ) {      // sideband modes
				m_Agc[rx_chan].ProcessData(ns_out, f_samps, r_samps);
			}

			if (mode == MODE_IQ) {
				m_Agc[rx_chan].ProcessData(ns_out, f_samps, f_samps);

                for (j=0; j<ns_out; j++) {
                    // can cast TYPEREAL directly to s2_t due to choice of CUTESDR_SCALE
                    s2_t re = (s2_t) f_samps->re, im = (s2_t) f_samps->im;
                    *bp_iq++ = (re >> 8) & 0xff; bc++;	// choose a network byte-order (big endian)
                    *bp_iq++ = (re >> 0) & 0xff; bc++;
                    *bp_iq++ = (im >> 8) & 0xff; bc++;
                    *bp_iq++ = (im >> 0) & 0xff; bc++;
                    f_samps++;
                }
		    } else {
                rx->real_wr_pos = (rx->real_wr_pos+1) & (N_DPBUF-1);
    
                if (ext_users[rx_chan].receive_real != NULL)
                    ext_users[rx_chan].receive_real(rx_chan, 0, ns_out, r_samps);
                
                if (ext_users[rx_chan].receive_real_tid != (tid_t) NULL)
                    TaskWakeup(ext_users[rx_chan].receive_real_tid, TRUE, TO_VOID_PARAM(rx_chan));
    
                if (compression) {
                    encode_ima_adpcm_i16_e8(r_samps, bp_real, ns_out, &rx->adpcm_snd);
                    bp_real += ns_out/2;		// fixed 4:1 compression
                    bc += ns_out/2;
                } else {
                    for (j=0; j<ns_out; j++) {
                        *bp_real++ = (*r_samps >> 8) & 0xff; bc++;	// choose a network byte-order (big endian)
                        *bp_real++ = (*r_samps >> 0) & 0xff; bc++;
                        r_samps++;
                    }
                }
            }
			
			#if 0
                static u4_t last_time[RX_CHANS];
                static int nctr;
                ncnt[rx_chan] += ns_out * (compression? 4:1);
                int nbuf = ncnt[rx_chan] / SND_RATE;
                if (nbuf >= nctr) {
                    nctr++;
                    u4_t now = timer_ms();
                    printf("SND%d: %d %d %.3fs\n", rx_chan, SND_RATE, nbuf, (float) (now - last_time[rx_chan]) / 1e3);
                    
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

		NextTask("s2c begin");
				
		// send s-meter data with each audio packet
		#define SMETER_BIAS 127.0
		float sMeter_dBm = sMeterAvg_dB + S_meter_cal;
		if (sMeter_dBm < -127.0) sMeter_dBm = -127.0; else
		if (sMeter_dBm >    3.4) sMeter_dBm =    3.4;
		u2_t sMeter = (u2_t) ((sMeter_dBm + SMETER_BIAS) * 10);
		assert(sMeter <= 0x0fff);
		out_pkt.h.smeter[0] = SND_FLAG_SMETER | ((sMeter >> 8) & 0xf);
		out_pkt.h.smeter[1] = sMeter & 0xff;
		
		if (rx_adc_ovfl) out_pkt.h.smeter[0] |= SND_FLAG_ADC_OVFL;

		if (change_LPF) {
			out_pkt.h.smeter[0] |= SND_FLAG_LPF;
			change_LPF = false;
		}
		
		// send sequence number that waterfall syncs to on client-side
		snd->seq++;
		out_pkt.h.seq = snd->seq;
		
		//printf("hdr %d S%d\n", sizeof(out_pkt.h), bc); fflush(stdout);
		int bytes = sizeof(out_pkt.h) + bc;
		app_to_web(conn, (char*) &out_pkt, bytes);
		audio_bytes += sizeof(out_pkt.h.smeter) + bc;
		
		#if 0
			static u4_t last_time[RX_CHANS];
			u4_t now = timer_ms();
			printf("SND%d: %d %.3fs seq-%d\n", rx_chan, bytes,
				(float) (now - last_time[rx_chan]) / 1e3, out_pkt.h.seq);
			last_time[rx_chan] = now;
		#endif
		
		#if 0
            static u4_t last_time[RX_CHANS];
            static int nctr;
            ncnt[rx_chan] += bc * (compression? 4:1);
            int nbuf = ncnt[rx_chan] / SND_RATE;
            if (nbuf >= nctr) {
                nctr++;
                u4_t now = timer_ms();
                printf("SND%d: %d %d %.3fs\n", rx_chan, SND_RATE, nbuf, (float) (now - last_time[rx_chan]) / 1e3);
                
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

		NextTask("s2c end");
	}
}

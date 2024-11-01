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

// Copyright (c) 2014-2023 John Seamons, ZL4VO/KF6VO

#include "types.h"
#include "options.h"
#include "config.h"
#include "kiwi.h"
#include "mode.h"
#include "printf.h"
#include "rx.h"
#include "rx_util.h"
#include "clk.h"
#include "mem.h"
#include "misc.h"
#include "str.h"
#include "timer.h"
#include "nbuf.h"
#include "web.h"
#include "spi.h"
#include "gps.h"
#include "coroutines.h"
#include "cuteSDR.h"
#include "rx_noise.h"
#include "teensy.h"
#include "debug.h"
#include "data_pump.h"
#include "cfg.h"
#include "mongoose.h"
#include "ima_adpcm.h"
#include "ext_int.h"
#include "rx.h"
#include "lms.h"
#include "dx.h"
#include "noise_blank.h"
#include "rx_sound.h"
#include "rx_sound_cmd.h"
#include "rx_waterfall.h"
#include "rx_filter.h"
#include "wdsp.h"
#include "fpga.h"
#include "rf_attn.h"

#ifdef DRM
 #include "DRM.h"
#endif

#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/errno.h>
#include <time.h>
#include <sched.h>
#include <math.h>
#include <limits.h>

#include <algorithm>

void rx_sound_set_freq(conn_t *conn, double freq, bool spectral_inversion)
{
    int ch = conn? conn->rx_channel : RX_CHAN0;
    double freq_kHz = freq * kHz;
    double freq_inv_kHz = ui_srate - freq_kHz;
    double adc_clock_corrected = conn? conn->adc_clock_corrected : clk.adc_clock_corrected;
    double f_phase = (spectral_inversion? freq_inv_kHz : freq_kHz) / adc_clock_corrected;
    u64_t i_phase = (u64_t) round(f_phase * pow(2,48));
    //printf("SND UPD rx%d freq %.3f kHz i_phase 0x%08x|%08x clk %.6f(%d)\n", ch,
    //    freq, PRINTF_U64_ARG(i_phase), adc_clock_corrected, clk.adc_clk_corrections);

    if (do_sdr) {
        spi_set3(CmdSetRXFreq, ch, (u4_t) ((i_phase >> 16) & 0xffffffff), (u2_t) (i_phase & 0xffff));
    }
}

static void rx_gen_disable(snd_t *s)
{
    g_genampl = s->genattn = 0;
    s->gen_enable = false;
    if (do_sdr) {
        spi_set3(CmdSetGenFreq, 0, 0, 0);
        spi_set(CmdSetGenAttn, 0, 0);
        ctrl_clr_set(CTRL_USE_GEN | CTRL_STEN, 0);
    }
}

void rx_gen_set_freq(conn_t *conn, snd_t *s)
{
    int rx_chan = conn->rx_channel;
    if (rx_chan != 0 || !s->gen_enable) return;
    u4_t self_test = (s->gen < 0 && !kiwi.ext_clk)? CTRL_STEN : 0;
    double gen_freq = fabs(s->gen);
    double f_phase = gen_freq * kHz / conn->adc_clock_corrected;
    u64_t i_phase = (u64_t) round(f_phase * pow(2,48));
    //cprintf(conn, "%s %.3f kHz phase %.3f 0x%012llx self_test=%d\n", gen_freq? "GEN_ON":"GEN_OFF", gen_freq, f_phase, i_phase, self_test? 1:0);
    if (do_sdr) {
        spi_set3(CmdSetGenFreq, rx_chan, (u4_t) ((i_phase >> 16) & 0xffffffff), (u2_t) (i_phase & 0xffff));
        ctrl_clr_set(CTRL_USE_GEN | CTRL_STEN, gen_freq? (CTRL_USE_GEN | self_test) : 0);
    }
    g_genfreq = gen_freq * kHz / ui_srate;
}

void rx_sound_cmd(conn_t *conn, double frate, int n, char *cmd)
{
    int j, k;
	int rx_chan = conn->rx_channel;

	snd_t *s = &snd_inst[rx_chan];
	wf_inst_t *wf = &WF_SHMEM->wf_inst[rx_chan];

    cmd[n] = 0;		// okay to do this -- see nbuf.c:nbuf_allocq()

    TaskStat(TSTAT_INCR|TSTAT_ZERO, 0, "cmd");

    evDP(EC_EVENT, EV_DPUMP, -1, "SND", evprintf("SND: %s", cmd));
    
    #if 0
        if (strcmp(conn->remote_ip, "") == 0 /* && strcmp(cmd, "SET keepalive") != 0 */)
            cprintf(conn, "SND <%s> cmd_recv 0x%x/0x%x\n", cmd, s->cmd_recv, CMD_ALL);
    #endif

    // SECURITY: this must be first for auth check
    bool keep_alive;
    if (rx_common_cmd(STREAM_SOUND, conn, cmd, &keep_alive)) {
        if ((conn->ip_trace || (TR_SND_CMDS && s->tr_cmds < 32)) && !keep_alive) {
            clprintf(conn, "SND #%02d [rx_common_cmd] <%s> cmd_recv 0x%x/0x%x\n", s->tr_cmds, cmd, s->cmd_recv, CMD_ALL);
            s->tr_cmds++;
        }
        return;
    }

    if (conn->ip_trace || (TR_SND_CMDS && s->tr_cmds < 32)) {
        clprintf(conn, "SND #%02d <%s> cmd_recv 0x%x/0x%x\n", s->tr_cmds, cmd, s->cmd_recv, CMD_ALL);
        s->tr_cmds++;
    }

    u2_t key = str_hash_lookup(&snd_cmd_hash, cmd);
    bool did_cmd = false;
    
    if (conn->ext_cmd != NULL)
        conn->ext_cmd(key, cmd, rx_chan);
    
    switch (key) {

    case CMD_AUDIO_START:
        n = sscanf(cmd, "SET dbgAudioStart=%d", &k);
        if (n == 1) {
            did_cmd = true;
        }
        break;

    case CMD_TUNE: {
        double _freq, _locut, _hicut;
        int _mode;
        char *mode_m = NULL;
        n = sscanf(cmd, "SET mod=%16ms low_cut=%lf high_cut=%lf freq=%lf param=%d", &mode_m, &_locut, &_hicut, &_freq, &s->mparam);
        if ((n == 4 || n == 5) && do_sdr) {
            did_cmd = true;
            //cprintf(conn, "SND f=%.3f lo=%.3f hi=%.3f mode=%s\n", _freq, _locut, _hicut, mode_m);

            bool new_freq = false;
            if (s->freq != _freq) {
                s->freq = _freq;
                rx_sound_set_freq(conn, s->freq, s->spectral_inversion);
                if (do_sdr) {
                    #ifdef SND_FREQ_SET_IQ_ROTATION_BUG_WORKAROUND
                        if (first_freq_trig) {
                            first_freq_set = true;
                            first_freq_time = timer_ms();
                            first_freq_trig = false;
                        }
                    #endif
                }
                s->cmd_recv |= CMD_FREQ;
                new_freq = true;
                s->change_freq_mode = s->check_masked = true;
            }
        
            bool no_mode_change = (strcmp(mode_m, "x") == 0);
            if (!no_mode_change) {
                _mode = rx_mode2enum(mode_m);
                s->cmd_recv |= CMD_MODE;

                if (_mode == NOT_FOUND) {
                    clprintf(conn, "SND bad mode <%s>\n", mode_m);
                    _mode = MODE_AM;
                    s->change_freq_mode = s->check_masked = true;
                }
            
                if (_mode == MODE_DRM && !DRM_enable && !conn->isLocal) {
                    clprintf(conn, "SND DRM not enabled, forcing mode to IQ\n", mode_m);
                    _mode = MODE_IQ;
                }
        
                bool new_nbfm = false;
                if (s->mode != _mode || n == 5) {

                    // when switching out of IQ or DRM modes: reset AGC, compression state
                    bool IQ_or_DRM_or_stereo = (mode_flags[s->mode] & IS_STEREO);
                    bool new_IQ_or_DRM_or_stereo = (mode_flags[_mode] & IS_STEREO);
            
                    if (IQ_or_DRM_or_stereo && !new_IQ_or_DRM_or_stereo && (s->cmd_recv & CMD_AGC)) {
                        //cprintf(conn, "SND out IQ mode -> reset AGC, compression\n");
                        m_Agc[rx_chan].SetParameters(s->agc, s->hang, s->thresh, s->manGain, s->slope, s->decay, frate);
                        memset(&s->adpcm_snd, 0, sizeof(ima_adpcm_state_t));
                    }
            
                    s->isSAM = mode_flags[_mode] & IS_SAM;
                    if (s->isSAM && n == 5) {
                        s->SAM_mparam = s->mparam & MODE_FLAGS_SAM;
                        //cprintf(conn, "SAM DC_block=%d fade_leveler=%d chan_null=%d\n",
                        //    (s->SAM_mparam & DC_BLOCK)? 1:0, (s->SAM_mparam & FADE_LEVELER)? 1:0, s->SAM_mparam & CHAN_NULL_WHICH);
                    }

                    // reset SAM demod on non-SAM to SAM transition
                    if (s->isSAM && ((mode_flags[s->mode] & IS_SAM) == 0)) {
                        //cprintf(conn, "SAM_PLL_RESET\n");
                        wdsp_SAM_PLL(rx_chan, PLL_RESET);
                    }

                    s->isChanNull = false;
                    s->specAF_instance = SND_INSTANCE_FFT_PASSBAND;

                    s->mode = _mode;
                    if (mode_flags[s->mode] & IS_NBFM)
                        new_nbfm = true;
                    s->change_freq_mode = s->check_masked = true;
                    //cprintf(conn, "SND mode %s\n", mode_m);
                }

                if ((mode_flags[s->mode] & IS_NBFM) && (new_freq || new_nbfm)) {
                    m_Squelch[rx_chan].Reset();
                    conn->last_sample.re = conn->last_sample.im = 0;
                }
            }
    
            bool no_pb_change = (_hicut == 0 && _locut == 0);
            if (!no_pb_change && (s->hicut != _hicut || s->locut != _locut)) {
                s->hicut = _hicut; s->locut = _locut;
            
                // primary passband filtering
                int fmax = frate/2 - 1;
                if (s->hicut > fmax) s->hicut = fmax;
                if (s->locut < -fmax) s->locut = -fmax;
            
                // normalized passband
                if (s->locut <= 0 && s->hicut >= 0) {     // straddles carrier
                    s->norm_locut = 0.0;
                    s->norm_hicut = MAX(-s->locut, s->hicut);
                    s->norm_pbc = 0;
                } else {
                    if (s->locut > 0) {
                        s->norm_locut = s->locut;
                        s->norm_hicut = s->hicut;
                    } else {
                        s->norm_hicut = -s->locut;
                        s->norm_locut = -s->hicut;
                    }
                    s->norm_pbc = s->norm_locut + (s->norm_hicut - s->norm_locut);
                }
            
                // hbw for post AM det is max of hi/lo filter cuts
                float hbw = fmaxf(fabs(s->hicut), fabs(s->locut));
                if (hbw > frate/2) hbw = frate/2;
                //cprintf(conn, "SND LOcut %.0f HIcut %.0f HBW %.0f/%.0f\n", s->locut, s->hicut, hbw, frate/2);
            
                #define CW_OFFSET 0		// fixme: how is cw offset handled exactly?
                m_PassbandFIR[rx_chan].SetupParameters(SND_INSTANCE_FFT_PASSBAND, s->locut, s->hicut, CW_OFFSET, frate);
                m_chan_null_FIR[rx_chan].SetupParameters(SND_INSTANCE_FFT_CHAN_NULL, s->locut, s->hicut, CW_OFFSET, frate);
                conn->half_bw = hbw;
            
                // post AM detector filter
                // FIXME: not needed if we're doing convolver-based LPF in javascript due to decompression?
                float stop = hbw*1.8;
                if (stop > frate/2) stop = frate/2;
                m_AM_FIR[rx_chan].InitLPFilter(0, 1.0, 50.0, hbw, stop, frate);
                s->cmd_recv |= CMD_PASSBAND;
            
                s->change_LPF = s->check_masked = true;
            }
        
            double nomfreq = s->freq;
            if (!no_pb_change && (s->hicut - s->locut) < 1000) nomfreq += (s->hicut + s->locut)/2/kHz;	// cw filter correction
            nomfreq = round(nomfreq*kHz);
            if (!no_mode_change) conn->mode = s->mode;
        
            // if freq change result of a scan don't let this reset inactivity timeout
            if (s->mparam & MODE_FLAGS_SCAN) {
                conn->freqChangeLatch = false;
            } else {
                conn->freqChangeLatch = true;
            }

            conn->freqHz = round(nomfreq/10.0)*10;	// round 10 Hz
            if (conn->freqChangeLatch && (conn->freqHz != conn->last_freqHz)) {
                conn->last_tune_time = timer_sec();
                conn->last_freqHz = conn->freqHz;
            }
        }
        kiwi_asfree(mode_m);
        break;
    }
    
    case CMD_RF_ATTN:
	    float rf_attn_dB;
        if (sscanf(cmd, "SET rf_attn=%f", &rf_attn_dB) == 1) {
        
            // enforce access to rf attn since javascript can be manipulated on client side
            bool deny = false;
            ext_auth_e auth = ext_auth(rx_chan);
            int allow = cfg_int("rf_attn_allow", NULL, CFG_REQUIRED);
            if (allow == RF_ATTN_ALLOW_LOCAL_ONLY && auth != AUTH_LOCAL) deny = true;
            if (allow == RF_ATTN_ALLOW_LOCAL_OR_PASSWORD_ONLY && auth == AUTH_USER) deny = true;
            clprintf(conn, "rf_attn SET %.1f auth|allow|deny=%d|%d|%d\n", rf_attn_dB, auth, allow, deny);
            rf_attn_dB = rf_attn_validate(rf_attn_dB);

            if (!deny) {
                // update s->rf_attn_dB here so we don't send UI update to ourselves
                kiwi.rf_attn_dB = s->rf_attn_dB = rf_attn_dB;
                rf_attn_set(rf_attn_dB);
            } else {
                clprintf(conn, "rf_attn DENY\n");
            }
            did_cmd = true;                
        }
        break;
    
    case CMD_SND_WINDOW_FUNC:
        if (sscanf(cmd, "SET window_func=%d", &n) == 1) {
            if (n < 0 || n >= N_SND_WINF) n = 0;
            s->window_func = n;
            m_PassbandFIR[rx_chan].SetupWindowFunction(s->window_func);
            //cprintf(conn, "SND window_func=%d\n", s->window_func);
            did_cmd = true;                
        }
        break;

    case CMD_SPEC:
        if (sscanf(cmd, "SET spc_=%d", &n) == 1) {
            if (n < 0 || n >= N_SND_SPEC) n = 0;
            //cprintf(conn, "SND spec=%d\n", n);
            s->specAF_FFT = (n == SPEC_SND_AF)? specAF_FFT : NULL;
            did_cmd = true;                
        }
        break;

    case CMD_COMPRESSION: {
        int _comp;
        n = sscanf(cmd, "SET compression=%d", &_comp);
        if (n == 1) {
            did_cmd = true;
            //printf("compression %d\n", _comp);
            if (_comp && (s->compression != _comp)) {      // when enabling compression reset AGC, compression state
                if (s->cmd_recv & CMD_AGC)
                    m_Agc[rx_chan].SetParameters(s->agc, s->hang, s->thresh, s->manGain, s->slope, s->decay, frate);
                memset(&s->adpcm_snd, 0, sizeof(ima_adpcm_state_t));
            }
            s->compression = _comp;
        }
        break;
    }

    case CMD_REINIT:
        if (strcmp(cmd, "SET reinit") == 0) {
            did_cmd = true;
            //cprintf(conn, "SND restart\n");
            if (s->cmd_recv & CMD_AGC)
                m_Agc[rx_chan].SetParameters(s->agc, s->hang, s->thresh, s->manGain, s->slope, s->decay, frate);
            memset(&s->adpcm_snd, 0, sizeof(ima_adpcm_state_t));
            s->restart = true;
        }
        break;

    case CMD_LITTLE_ENDIAN:
        if (strcmp(cmd, "SET little-endian") == 0) {
            did_cmd = true;
            //cprintf(conn, "SND little-endian\n");
            s->little_endian = true;
        }
        break;

    case CMD_GEN_FREQ:
        n = sscanf(cmd, "SET gen=%lf", &s->gen);
        if (n == 1) {
            did_cmd = true;
            if (rx_chan == 0) {
                //cprintf(conn, "SET gen=%lf\n", &s->gen);
                if (s->gen == 0 && s->genattn == 0) {
                    rx_gen_disable(s);
                } else {
                    if (s->gen != 0 && !s->gen_enable) s->gen_enable = true;
                    rx_gen_set_freq(conn, s);
                    if (s->gen == 0 && s->gen_enable) s->gen_enable = false;
                }
            }
        }
        break;

    case CMD_GEN_ATTN:
        int _genattn;
        n = sscanf(cmd, "SET genattn=%d", &_genattn);
        if (n == 1) {
            did_cmd = true;
            if (rx_chan == 0) {
                if (s->gen == 0 && _genattn == 0) {
                    rx_gen_disable(s);
                } else
                if (s->genattn != _genattn) {
                    s->genattn = _genattn;
                    if (do_sdr) spi_set(CmdSetGenAttn, 0, (u4_t) s->genattn);
                    //cprintf(conn, "GEN_ATTN %d 0x%x\n", s->genattn, s->genattn);
                    g_genampl = s->genattn / (float) ((1 << 17) - 1);
                }
            }
        }
        break;

    case CMD_SET_AGC:
        n = sscanf(cmd, "SET agc=%d hang=%d thresh=%d slope=%d decay=%d manGain=%d",
            &s->_agc, &s->_hang, &s->_thresh, &s->_slope, &s->_decay, &s->_manGain);
        if (n == 6) {
            did_cmd = true;
            s->agc = s->_agc;
            s->hang = s->_hang;
            s->thresh = s->_thresh;
            s->slope = s->_slope;
            s->decay = s->_decay;
            s->manGain = s->_manGain;
            //cprintf(conn, "AGC %d hang=%d thresh=%d slope=%d decay=%d manGain=%d srate=%.1f\n",
            //	s->agc, s->hang, s->thresh, s->slope, s->decay, s->manGain, frate);
            m_Agc[rx_chan].SetParameters(s->agc, s->hang, s->thresh, s->manGain, s->slope, s->decay, frate);
            s->cmd_recv |= CMD_AGC;
        }
        break;

    case CMD_SQUELCH: {
        int _squelch;
        float _squelch_param;
        n = sscanf(cmd, "SET squelch=%d param=%f", &_squelch, &_squelch_param);
        if (n == 1 || n == 2) {     // directTDoA still sends old API "squelch=0 max=0"
            did_cmd = true;
            if (n == 2) {
                s->squelch = _squelch;
                s->squelched = false;
                //cprintf(conn, "SND SET squelch=%d param=%.2f %s\n", s->squelch, _squelch_param, mode_lc[s->mode]);
                if (mode_flags[s->mode] & IS_NBFM) {
                    m_Squelch[rx_chan].SetSquelch(s->squelch, _squelch_param);
                } else {
                    float squelch_tail = _squelch_param;
                    s->tail_delay = roundf(squelch_tail * snd_rate / LOOP_BC);
                    s->squelch_on_seq = -1;
                    s->sq_changed = true;
                    //cprintf(conn, " squelch_tail=%.2f tail_delay=%d", squelch_tail, s->tail_delay);
                }
                //cprintf(conn, "\n");
            }
        }
        break;
    }
    
    case CMD_SAM_PLL:
        int type;
        n = sscanf(cmd, "SET sam_pll=%d", &type);
        if (n == 1) {
            did_cmd = true;
            //cprintf(conn, "sam_pll=%d\n", type);
            wdsp_SAM_PLL(rx_chan, type);
        }
        break;

    case CMD_NB_ALGO:
        n = sscanf(cmd, "SET nb algo=%d", &s->nb_algo);
        if (n == 1) {
            did_cmd = true;
            //cprintf(conn, "nb: algo=%d\n", s->nb_algo);
            memset(s->nb_enable, 0, sizeof(s->nb_enable));
            memset(wf->nb_enable, 0, sizeof(wf->nb_enable));
        }
        break;

    case CMD_NR_ALGO:
        n = sscanf(cmd, "SET nr algo=%d", &s->nr_algo);
        if (n == 1) {
            did_cmd = true;
            //cprintf(conn, "nr: algo=%d\n", s->nr_algo);
            memset(s->nr_enable, 0, sizeof(s->nr_enable));
        }
        break;

    int n_type, n_en;
    int n_param;
    float n_pval;

    case CMD_NB_TYPE:
        if (sscanf(cmd, "SET nb type=%d en=%d", &n_type, &n_en) == 2) {
            did_cmd = true;
            //cprintf(conn, "nb: type=%d en=%d\n", n_type, n_en);
            s->nb_enable[n_type] = n_en;
            wf->nb_enable[n_type] = n_en;
        } else

        if (sscanf(cmd, "SET nb type=%d param=%d pval=%f", &n_type, &n_param, &n_pval) == 3) {
            did_cmd = true;
            //cprintf(conn, "nb: type=%d param=%d pval=%.9f\n", n_type, n_param, n_pval);
            s->nb_param[n_type][n_param] = n_pval;

            if (s->nb_algo == NB_STD || n_type == NB_CLICK) {
                wf->nb_param[n_type][n_param] = n_pval;
                wf->nb_param_change[n_type] = true;
            }

            if (n_type == NB_BLANKER) {
                switch (s->nb_algo) {
                    case NB_STD: m_NoiseProc_snd[rx_chan].SetupBlanker("SND", frate, s->nb_param[n_type]); break;
                    case NB_WILD: nb_Wild_init(rx_chan, s->nb_param[n_type]); break;
                }
            }
        }
        
        break;

    case CMD_NR_TYPE:
        if (sscanf(cmd, "SET nr type=%d en=%d", &n_type, &n_en) == 2) {
            did_cmd = true;
            //cprintf(conn, "nr: type=%d en=%d\n", n_type, n_en);
            s->nr_enable[n_type] = n_en;
        } else

        if (sscanf(cmd, "SET nr type=%d param=%d pval=%f", &n_type, &n_param, &n_pval) == 3) {
            did_cmd = true;
            //cprintf(conn, "nr: type=%d param=%d pval=%.9f\n", n_type, n_param, n_pval);
            s->nr_param[n_type][n_param] = n_pval;

            switch (s->nr_algo) {
                case NR_WDSP: wdsp_ANR_init(rx_chan, (nr_type_e) n_type, s->nr_param[n_type]); break;
                case NR_ORIG: m_LMS[rx_chan][n_type].Initialize((nr_type_e) n_type, s->nr_param[n_type]); break;
                case NR_SPECTRAL: nr_spectral_init(rx_chan, s->nr_param[n_type]); break;
            }
        }
        
        break;

    case CMD_MUTE:
        n = sscanf(cmd, "SET mute=%d", &s->mute);
        if (n == 1) {
            did_cmd = true;
            //printf("mute %d\n", s->mute);
            // FIXME: stop audio stream to save bandwidth?
        }

    case CMD_OVLD_MUTE:
        n = sscanf(cmd, "SET ovld_mute=%d", &s->mute_overload);
        if (n == 1) {
            did_cmd = true;
            //printf("ovld_mute %d\n", s->mute);
            // FIXME: stop audio stream to save bandwidth?
        }

    // dsp.stackexchange.com/questions/34605/biquad-cookbook-formula-for-broadcast-fm-de-emphasis
    case CMD_DE_EMP: {
        int _de_emp, _nfm;
        n = sscanf(cmd, "SET de_emp=%d nfm=%d", &_de_emp, &_nfm);
        if (n == 1 || n == 2) {
            did_cmd = true;
            if (n == 1) {
                _nfm = (mode_flags[s->mode] & IS_NBFM);
                //cprintf(conn, "DEEMP: _de_emp=%d mode=%d _nfm=%d (old kiwiclient API)\n", _de_emp, s->mode, _nfm);
            } else {
                //cprintf(conn, "DEEMP: _de_emp=%d _nfm=%d\n", _de_emp, _nfm);
            }
            if (_nfm) s->deemp_nfm = _de_emp; else s->deemp = _de_emp;

            if (_de_emp) {
                if (_nfm) {
                    // -20 dB @ 4 kHz
                    //cprintf(conn, "DEEMP: NFM %d %s\n", (snd_rate == SND_RATE_4CH)? 12000:20250, (_de_emp == 1)? "-LF":"+LF");
                    const TYPEREAL *pCoef =
                        (snd_rate == SND_RATE_4CH)? nfm_deemp_12000[_de_emp-1] : nfm_deemp_20250[_de_emp-1];
                    m_nfm_deemp_FIR[rx_chan].InitConstFir(N_DEEMP_TAPS, pCoef, frate);
                } else {
                    //#define TEST_AM_SSB_BIQUAD
                    #ifdef TEST_AM_SSB_BIQUAD
                        TYPEREAL a0, a1, a2, b0, b1, b2;
                        double Fs = frate;
                        double T1 = (_de_emp == 1)? 0.000075 : 0.000050;
                        double z1 = -exp(-1.0/(Fs*T1));
                        double p1 = 1.0 + z1;
                        a0 = 1.0;
                        a1 = p1;
                        a2 = 0;
                        b0 = 2.0;   // remove filter gain
                        b1 = z1;
                        b2 = 0;
                        m_deemp_Biquad[rx_chan].InitFilterCoef(b0, b1, b2, a0, a1, a2);
                        cprintf(conn, "DEEMP: AM/SSB %d %d uS [%f %f %f %f %f %f]\n",
                            snd_rate, (_de_emp == 1)? 75:50, a0, a1, a2, b0, b1, b2);
                    #else
                        //cprintf(conn, "DEEMP: AM/SSB %d %d uS\n", (snd_rate == SND_RATE_4CH)? 12000:20250, (_de_emp == 1)? 75:50);
                        const TYPEREAL *pCoef =
                            (snd_rate == SND_RATE_4CH)? am_ssb_deemp_12000[_de_emp-1] : am_ssb_deemp_20250[_de_emp-1];
                        m_am_ssb_deemp_FIR[rx_chan].InitConstFir(N_DEEMP_TAPS, pCoef, frate);
                    #endif
                }
            }
            //else cprintf(conn, "DEEMP: %sOFF\n", _nfm? "NFM ":"");
        }
        break;
    }

    case CMD_TEST:
        n = sscanf(cmd, "SET test=%d", &s->test);
        if (n == 1) {
            did_cmd = true;
            cprintf(conn, "test %d\n", s->test);
            test_flag = s->test;
        }
        break;

    case CMD_UAR:
	    int arate_in, arate_out;
        n = sscanf(cmd, "SET UAR in=%d out=%d", &arate_in, &arate_out);
        if (n == 2) {
            did_cmd = true;
            //clprintf(conn, "UAR in=%d out=%d\n", arate_in, arate_out);
        }
        break;

    case CMD_AR_OKAY:
        n = sscanf(cmd, "SET AR OK in=%d out=%d", &arate_in, &arate_out);
        if (n == 2) {
            did_cmd = true;
            //clprintf(conn, "AR OK in=%d out=%d\n", arate_in, arate_out);
            if (arate_out) s->cmd_recv |= CMD_AR_OK;
        }
        break;

    case CMD_UNDERRUN:
        n = sscanf(cmd, "SET underrun=%d", &j);
        if (n == 1) {
            did_cmd = true;
            conn->audio_underrun++;
            cprintf(conn, "SND: audio underrun %d %s -------------------------\n",
                conn->audio_underrun, conn->ident_user);
            //if (ev_dump) evNT(EC_DUMP, EV_NEXTTASK, ev_dump, "NextTask", evprintf("DUMP IN %.3f SEC",
            //	ev_dump/1000.0));
        }
        break;

#ifdef SND_SEQ_CHECK
    case CMD_SEQ: {
        int _seq, _sequence;
        n = sscanf(cmd, "SET seq=%d sequence=%d", &_seq, &_sequence);
        if (n == 2) {
            did_cmd = true;
            conn->sequence_errors++;
            printf("SND%d: audio.js SEQ got %d, expecting %d, %s -------------------------\n",
                rx_chan, _seq, _sequence, conn->ident_user);
        }
    }
#endif
    
    // still sent by directTDoA -- ignore
    case CMD_LMS_AUTONOTCH:
        if (kiwi_str_begins_with(cmd, "SET lms_autonotch"))
            did_cmd = true;
        break;

    default:
        // have to check for old API below
        did_cmd = false;
        break;

    }   // switch
    
    if (did_cmd) return;

    // kiwiclient has used "SET nb=" in the past which is shorter than the max_hash_len
    // so must be checked manually
    int nb, th;
    n = sscanf(cmd, "SET nb=%d th=%d", &nb, &th);
    if (n == 2) {
        s->nb_param[NB_BLANKER][NB_GATE] = nb;
        s->nb_param[NB_BLANKER][NB_THRESHOLD] = th;
        s->nb_enable[NB_BLANKER] = nb? 1:0;
        //cprintf(conn, "NB gate=%d thresh=%d\n", nb, th);

        if (nb) m_NoiseProc_snd[rx_chan].SetupBlanker("SND", frate, s->nb_param[NB_BLANKER]);
        return;
    }

    if (conn->mc != NULL) {
        cprintf(conn, "#### SND hash=0x%04x key=%d \"%s\"\n", snd_cmd_hash.cur_hash, key, cmd);
        cprintf(conn, "SND BAD PARAMS: sl=%d %d|%d|%d [%s] ip=%s ####################################\n",
            strlen(cmd), cmd[0], cmd[1], cmd[2], cmd, conn->remote_ip);
        conn->unknown_cmd_recvd++;
    }
}

/* -*- c++ -*- */
/* 
 * Copyright 2015 Milen Rangelov <gat3way@gmail.com>
 * 
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 * 
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this software; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street,
 * Boston, MA 02110-1301, USA.
 */

/*

gr-ale is written by Milen Rangelov (gat3way@gmail.com) and licensed under the GNU General Public License

Significant portions of source code were based on the LinuxALE project (under GNU License):

 * Copyright (C) 2000 - 2001 
 *   Charles Brain (chbrain@dircon.co.uk)
 *   Ilkka Toivanen (pile@aimo.kareltek.fi)

*/

#pragma once

#define NR                          17
#define FFT_SIZE                    64
#define MOD_64                      64
#define SYMBOLS_PER_WORD            49
#define VOTE_BUFFER_LENGTH          48
#define NOT_WORD_SYNC               0
#define WORD_SYNC                   1
#define BITS_PER_SYMBOL             3
#define VOTE_ARRAY_LENGTH           (SYMBOLS_PER_WORD * BITS_PER_SYMBOL)
#define PI                          M_PI
#define BAD_VOTE_THRESHOLD          25
#define SYNC_ERROR_THRESHOLD        1

// FFT_SIZE depends on audio sample rate
#define SAMP_RATE_SPS               8000
#define SYMBOL_PERIOD_MS            8       // per the ALE spec
#define SYMBOLS_PER_SEC             125     // 1 / SEC(SYMBOL_PERIOD_MS)
#define SAMPS_PER_SYMBOL            (SAMP_RATE_SPS / SYMBOLS_PER_SEC)
//assert(SAMPS_PER_SYMBOL == FFT_SIZE);

#include "decode_ff.h"
#include "caudioresample.h"     // from DRM/dream/resample

#ifdef STANDALONE_TEST
    double ext_update_get_sample_rateHz(int rx_chan) { return 0; }
    int ext_send_msg(int rx_chan, bool debug, const char *msg, ...) { return 0; }
#else
    #include "ext.h"
#endif

namespace ale {

    typedef struct {
        double real;
        double imag;
    } Complex;

    typedef struct {
        cmd_e cmd;
        u4_t data;
        char cmd_s[256];
    } cmd_t;
    
    
    class decode_ff_impl {
    private:
        double  fft_cs_twiddle[FFT_SIZE];   // init
        double  fft_ss_twiddle[FFT_SIZE];   // init
        double  fft_history[FFT_SIZE];   // init
        Complex fft_out[FFT_SIZE];   // init
        double  fft_mag[FFT_SIZE];   // init
        int     fft_history_offset;   // init

        // sync information
        double mag_sum[NR][FFT_SIZE];   // init
        //double mag_history[NR][FFT_SIZE][SYMBOLS_PER_WORD];
        int    mag_history_offset;   // init
        int    word_sync[NR];   // init

        // worker data
        //int started[NR];    // if other than DATA has arrived
        int bits[NR][VOTE_ARRAY_LENGTH];   // init
        int input_buffer_pos[NR];   // init
        int word_sync_position[NR];   // init

        // protocol data
        char thru[4];
        char to[4];
        char twas[4];
        char from[4];
        char tis[4];

        char data[4];
        char rep[4];

        char cmd[4];

        #define N_CUR 256
        char current[N_CUR];
        char current2[N_CUR];
        int in_cmd, binary;   // init
        bool ascii_38_ok, ascii_64_ok, ascii_nl_ok;
        
        cmd_t cmds[16];
        int cmd_cnt;

        int ber[NR];   // init
        int lastber;   // init
        int bestpos;   // init
        int inew;       // no init

        int ithru;
        int ito;
        int itwas;
        int ifrom;
        int itis;

        int idata;
        int irep;

        int icmd;
        
        int state;   // init
        int state_count;   // init
        int stage_num;   // init

        int last_symbol[NR];   // init
        int last_sync_position[NR];   // init
        int nsym;   // init
        int sample_count;   // init
        int activity_cnt;
        int active;
        int cmd_freq_Hz;
        int secs, timer_samps;
        int dsp;
        double frequency;

        #define N_SAMP_BUF 1024
        #ifdef STANDALONE_TEST
            u2_t sbuf[N_SAMP_BUF];
            float fbuf[N_SAMP_BUF];
        #endif
        
        int rx_chan;
        bool use_UTC;

        u4_t golay_encode(u4_t data);
        u4_t golay_decode(u4_t code, int *errors);
        int decode_word (u4_t word, int nr, int berw, int caller);
        u4_t modem_de_interleave_and_fec(int *input, int *errors);
        int modem_new_symbol(int sym, int nr);
        void log(char *current, char *current2, int state, int ber, const char *from);
    
    protected:
        // resampler
        CAudioResample *ResampleObj;
        CVector<_REAL> vecTempResBufIn;
        CVector<_REAL> vecTempResBufOut;

    public:
        decode_ff_impl();
        ~decode_ff_impl();
        void modem_init(int rx_chan, bool use_new_resampler, float f_srate, int n_samps, bool use_UTC = true);
        void modem_reset();
        void set_display(int display);
        void set_freq(double freq);
        void run_standalone(int fileno, int offset);
        void do_modem(float *sample, int length);
        void do_modem_resampled(short *sample, int length);
    };

} // namespace ale

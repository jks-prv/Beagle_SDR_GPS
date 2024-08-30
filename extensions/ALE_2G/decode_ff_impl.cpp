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


#ifdef KIWI
#else
    #include "types.h"
    #include <stdlib.h>
    #define STANDALONE_TEST
#endif

// this is here before Kiwi includes to prevent our "#define printf ALT_PRINTF" mechanism being disturbed
#include "decode_ff_impl.h"

#ifdef STANDALONE_TEST

#define P_MAX_MIN_DEMAND    0x00
#define P_MAX_MIN_RANGE     0x01
#define P_MAX_MIN_DUMP      0x02

typedef struct {
	int min_i, max_i;
	double min_f, max_f;
	int min_idx, max_idx;
} print_max_min_int_t;

static void *mag_state;

static print_max_min_int_t *print_max_min_init(void **state)
{
	print_max_min_int_t **pp = (print_max_min_int_t **) state;
	if (*pp == NULL) {
		*pp = (print_max_min_int_t *) malloc(sizeof(print_max_min_int_t));
		print_max_min_int_t *p = *pp;
		memset(p, 0, sizeof(*p));
		p->min_i = 0x7fffffff; p->max_i = 0x80000000;
		p->min_f = 1e38; p->max_f = -1e38;
		p->min_idx = p->max_idx = -1;
	}
	return *pp;
}

void print_max_min_stream_f(void **state, int flags, const char *name, int index=0, int nargs=0, ...)
{
	print_max_min_int_t *p = print_max_min_init(state);
	bool dump = (flags & P_MAX_MIN_DUMP);
	bool update = false;

	va_list ap;
	va_start(ap, nargs);
	if (!dump) for (int i=0; i < nargs; i++) {
		double arg_f = va_arg(ap, double);
		if (arg_f > p->max_f) {
			p->max_f = arg_f;
			p->max_idx = index;
			update = true;
		}
		if (arg_f < p->min_f) {
			p->min_f = arg_f;
			p->min_idx = index;
			update = true;
		}
	}
	va_end(ap);
	
	if (dump || ((flags & P_MAX_MIN_RANGE) && update)) {
		//printf("min/max %s: %e(%d)..%e(%d)\n", name, p->min_f, p->min_idx, p->max_f, p->max_idx);
		printf("min/max %s: %f(%d)..%f(%d)\n", name, p->min_f, p->min_idx, p->max_f, p->max_idx);
	}
}
#endif

#ifdef KIWI
    #include "types.h"
    #include "printf.h"
#else
    #include "standalone_test/config.h"

    #define real_printf(fmt, ...)
    #define NextTask(s)
#endif

#include <math.h>
#include <time.h>
#include <stdio.h>
#include <unistd.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

//#define CHECK_ARRAY_DIM
#ifdef CHECK_ARRAY_DIM
    #define decode_ff_array_dim(ai, dim) \
        if (!((ai) >= (0) && (ai) < (dim))) { \
            printf("array bounds check failed: \"%s\"(%d) >= 0 AND < \"%s\"(%d) %s line %d\n", \
                #ai, ai, #dim, dim, __FILE__, __LINE__); \
            exit(-1); \
        }
#else
    #define decode_ff_array_dim(ai, dim)
#endif

#ifdef KIWI
    #ifdef HOST
        #define cprintf(cond_d, cond_c, color, fmt, ...) \
            kiwi_snprintf_buf(log_buf, "%s" fmt "%s\n", (dsp >= cond_c)? color : "", ## __VA_ARGS__, (dsp >= cond_c)? NORM : ""); \
            cprintf_msg(cond_d);
    #else
        #define cprintf(cond_d, cond_c, color, fmt, ...) \
            if (dsp >= cond_d) { \
                bool use_color = (dsp >= cond_c); \
                printf("%s" fmt "%s\n", use_color? color : "", ## __VA_ARGS__, (dsp >= cond_c)? NORM : ""); \
            }
    #endif
#else
    // standalone
    #if 0
        #define cprintf(cond_d, cond_c, color, fmt, ...) \
            if (dsp >= cond_d) { \
                bool use_color = (dsp >= cond_c); \
                printf("%s" fmt NORM "\n", use_color? color : "", ## __VA_ARGS__); \
            }
    #else
        #define cprintf(cond_d, cond_c, color, fmt, ...) \
            kiwi_snprintf_buf(log_buf, "%s" fmt NORM "\n", (dsp >= cond_c)? color : "", ## __VA_ARGS__); \
            cprintf_msg(cond_d);
    #endif
#endif

#define DEBUG_PRINTF
#ifdef DEBUG_PRINTF
    #define dprintf(fmt, ...) \
        if (dsp == DBG) { \
            kiwi_snprintf_buf(dpf_buf, fmt, ## __VA_ARGS__); \
            dprintf_msg(); \
        }
#else
    #define dprintf(fmt, ...)
#endif

static char *cdeco(int idx, char c)
{
    static char s[16][16];
    kiwi_snprintf_buf(s[idx], "%s(%02x)", ASCII[c], (c & 0xff));
    return s[idx];
}

static int slot(double freq)
{
    if (fabs(freq - 23982.60) < 0.001) return 0;
    if (fabs(freq - 24000.60) < 0.001) return 1;
    if (fabs(freq - 24013.00) < 0.001) return 2;
    return 999;
}

namespace ale {

	decode_ff_impl::decode_ff_impl(): ResampleObj(nullptr)
	{
        if (SAMPS_PER_SYMBOL != FFT_SIZE) { printf("SAMPS_PER_SYMBOL != FFT_SIZE\n"); exit(-1); }
        if (NFREQ <= HALF_FFT_SIZE) { printf("NFREQ <= HALF_FFT_SIZE\n"); exit(-1); }

	    for (int i=0; i < FFT_SIZE; i++) {
            fft_cs_twiddle[i] = cos((-2.0*PI * i) / FFT_SIZE);
            fft_ss_twiddle[i] = sin((-2.0*PI * i) / FFT_SIZE);
        }

        dft_plan = fftwf_plan_dft_1d(FFT_SIZE, dft_in, dft_out, FFTW_FORWARD, FFTW_ESTIMATE);
	}

	decode_ff_impl::~decode_ff_impl() { }

	u4_t decode_ff_impl::golay_encode(u4_t data)
	{
	    u4_t code;
	    code = data;
	    code <<= 12;
	    decode_ff_array_dim(data, 4096);
	    code += encode_table[data];
	    return code;
	}

	u4_t decode_ff_impl::golay_decode(u4_t code, int *errors)
	{
	    u4_t syndrome;
	    u4_t data;
	    u4_t parity;

	    data   = code >> 12;
	    parity = code & 0x00000FFFL;

	    decode_ff_array_dim(data, 4096);
	    syndrome = parity ^ encode_table[data];

	    decode_ff_array_dim(syndrome, 4096);
	    data     = data ^ (error[syndrome] & 0x0FFF);
	    *errors  = (error[syndrome] & 0xF000) >> 12;
	    return   data;
	}

	u4_t decode_ff_impl::modem_de_interleave_and_fec(int *input, int *errors)
	{
	    int i;
	    u4_t worda;
	    u4_t wordb;
	    u4_t error_a, error_b;

	    worda = wordb = 0;

	    for (i = 0; i < VOTE_BUFFER_LENGTH; ) {
	        decode_ff_array_dim(i, VOTE_BUFFER_LENGTH);
            worda = input[i++]? (worda << 1) +1 : worda << 1;
	        decode_ff_array_dim(i, VOTE_BUFFER_LENGTH);
            wordb = input[i++]? (wordb << 1) +1 : wordb << 1; 
	    }
	    
	    wordb = wordb ^ 0x000FFF;
	    worda = golay_decode((long) worda, (int*) &error_a);
	    wordb = golay_decode((long) wordb, (int*) &error_b);

	    if (error_a > error_b) 
		    *errors = error_a;
	    else
		    *errors = error_b;

	    worda = (worda << 12) + wordb;
	    return (worda);
	}

	int decode_ff_impl::modem_new_symbol(int sym, int nt)
	{
	    int i, j, rv;
	    int majority_vote_array[VOTE_BUFFER_LENGTH];
	    int bad_votes, sum, errors;
	    u4_t word = 0;

	    nt_new = nt;

        decode_ff_array_dim(sym, NSYMBOLS+1);
        decode_ff_array_dim(nt, NTIME);

        j = input_buffer_pos[nt];
        decode_ff_array_dim(j, VOTE_ARRAY_LENGTH);
	    bits[nt][j] = (sym & 4) ? 1 : 0;
	    input_buffer_pos[nt] = (input_buffer_pos[nt]+1) % VOTE_ARRAY_LENGTH;

        j = input_buffer_pos[nt];
        decode_ff_array_dim(j, VOTE_ARRAY_LENGTH);
	    bits[nt][j] = (sym & 2) ? 1 : 0;
	    input_buffer_pos[nt] = (input_buffer_pos[nt]+1) % VOTE_ARRAY_LENGTH;

        j = input_buffer_pos[nt];
        decode_ff_array_dim(j, VOTE_ARRAY_LENGTH);
	    bits[nt][j] = (sym & 1) ? 1 : 0;
	    input_buffer_pos[nt] = (input_buffer_pos[nt]+1) % VOTE_ARRAY_LENGTH;

	    bad_votes = 0;
	    for (i=0; i < VOTE_BUFFER_LENGTH; i++) {
	        j = (i + input_buffer_pos[nt]) % VOTE_ARRAY_LENGTH;
            decode_ff_array_dim(j, VOTE_ARRAY_LENGTH);
            sum  = bits[nt][j];

            j = (i + input_buffer_pos[nt] + SYMBOLS_PER_WORD) % VOTE_ARRAY_LENGTH;
            decode_ff_array_dim(j, VOTE_ARRAY_LENGTH);
            sum += bits[nt][j];

            j = (i + input_buffer_pos[nt] + (2*SYMBOLS_PER_WORD)) % VOTE_ARRAY_LENGTH;
            decode_ff_array_dim(j, VOTE_ARRAY_LENGTH);
            sum += bits[nt][j];

            if ((sum == 1) || (sum == 2)) bad_votes++; 
            decode_ff_array_dim(sum, 4);
            majority_vote_array[i] = vote_lookup[sum];
	    }

	    ber[nt] = HIGH_BER;
	    rv = 0;

	    if (word_sync[nt] == NOT_WORD_SYNC) {
            if (bad_votes <= BAD_VOTE_THRESHOLD) {
                word = modem_de_interleave_and_fec(majority_vote_array, &errors);
                if (errors <= SYNC_ERROR_THRESHOLD) {
                    /* int err = */ decode_word(word, nt, bad_votes, 0);
                    word_sync[nt] = IS_WORD_SYNC;
                    word_sync_position[nt] = input_buffer_pos[nt];
                    rv = 1;
                }
            }
	    } else {
            if (input_buffer_pos[nt] == word_sync_position[nt]) {
                word = modem_de_interleave_and_fec(majority_vote_array, &errors);
                decode_word(word, nt, bad_votes, 1);
                rv = 2;
            } else {
                word_sync[nt] = NOT_WORD_SYNC;
            }
	    }
	    
	    return rv;
	}

    #define N_MSG 256
    static int zs, zz;
    
	int decode_ff_impl::decode_word(u4_t w, int nt, int berw, int caller)
	{
	    u1_t a, b, c, preamble;
	    int rv = 1;
	    bool word_debug = false;
	    
	    char message[N_MSG];
	    char *s = message;
	    *s = '\0';
        cmd_t *cp = &cmds[cmd_cnt];
        current_word = w;

        // 24-bit word
        // ppp aaaaaaa bbbbbbb ccccccc
        //  21      14       7       0
	    preamble = bf(w,23,21);
	    a = bf(w,20,14);
	    b = bf(w,13,7);
	    c = bf(w,6,0);

        //#define TEST_AMD
        #ifdef TEST_AMD
            if (zs == 0)
                preamble = CMD;
            else
                preamble = (zs & 1)? DATA : REP;
            c = '0'+zz;
            zz++;
            if (zz >= 10) zz = 0;
        #endif

        ascii_38_ok = ((ASCII_Set[a] == ASCII_38) && (ASCII_Set[b] == ASCII_38) && (ASCII_Set[c] == ASCII_38));
        ascii_64_ok = ((ASCII_Set[a]  & ASCII_64) && (ASCII_Set[b]  & ASCII_64) && (ASCII_Set[c]  & ASCII_64));
        ascii_nl_ok = (
            ((ASCII_Set[a] & ASCII_64) || (ASCII_Set[a] == ASCII_NL)) &&
            ((ASCII_Set[b] & ASCII_64) || (ASCII_Set[b] == ASCII_NL)) &&
            ((ASCII_Set[c] & ASCII_64) || (ASCII_Set[c] == ASCII_NL))
        );

	    if (preamble == CMD) {
            ber[nt] = berw;
            if (word_debug) kiwi_snprintf_ptr(s, N_MSG, "CMD a38=%d a64=%d %s %s %s 0x%04x %d %2x %2x %2x", ascii_38_ok, ascii_64_ok,
                cdeco(0,a), cdeco(1,b), cdeco(2,c),
                w, bf(w,23,21), bf(w,20,14), bf(w,13,7), bf(w,6,0));
            kiwi_snprintf_buf(cmd, "%c%c%c", a,b,c);
	        icmd = 1;
	        if (gdebug & 1) printf("%scmd=1 %s%s\n", GREEN, cmd, NORM);
	    } else
	    
	    if (preamble == DATA) {
	        bool ign = false;
            if (ascii_38_ok || (ascii_nl_ok && in_cmd && (cp->cmd == AMD || cp->cmd == DTM))) {
                ber[nt] = berw;
                kiwi_snprintf_buf(data, "%c%c%c", a,b,c);
                idata = 1;
            } else {
                activity_cnt = 0;       // prevent false activity triggering
                ign = true;
            }
            if (word_debug) kiwi_snprintf_ptr(s, N_MSG, "DATA a38=%d a64=%d %s %s %s 0x%04x %d %2x %2x %2x %s", ascii_38_ok, ascii_64_ok,
                cdeco(0,a), cdeco(1,b), cdeco(2,c),
                w, bf(w,23,21), bf(w,20,14), bf(w,13,7), bf(w,6,0), ign? "IGNORED":"");
	    } else
	    
	    if (preamble == REP) {
	        bool ign = false;
            if (ascii_38_ok || (ascii_nl_ok && in_cmd && (cp->cmd == AMD || cp->cmd == DTM))) {
                ber[nt] = berw;
                kiwi_snprintf_buf(rep, "%c%c%c", a,b,c);
                irep = 1;
            } else {
                activity_cnt = 0;       // prevent false activity triggering
                ign = true;
            }
            if (word_debug) kiwi_snprintf_ptr(s, N_MSG, "REP a38=%d a64=%d %s %s %s 0x%04x %d %2x %2x %2x %s", ascii_38_ok, ascii_64_ok,
                cdeco(0,a), cdeco(1,b), cdeco(2,c),
                w, bf(w,23,21), bf(w,20,14), bf(w,13,7), bf(w,6,0), ign? "IGNORED":"");
	    } else
	    
	    {
	        // not AQC-ALE protocol
		    if (ascii_38_ok) {
                ber[nt] = berw;
                if (word_debug) kiwi_snprintf_ptr(s, N_MSG, "%s %c%c%c 0x%04x %d %2x %2x %2x",
                    preamble_types[preamble], a,b,c, w, bf(w,23,21), bf(w,20,14), bf(w,13,7), bf(w,6,0));

                switch (preamble) {
                    case TO:
                        kiwi_snprintf_buf(to, "%c%c%c", a,b,c);
                        ito = 1;
                        break;
                    case TWAS:
                        kiwi_snprintf_buf(twas, "%c%c%c", a,b,c);
                        itwas = 1;
                        break;
                    case FROM:
                        kiwi_snprintf_buf(from, "%c%c%c", a,b,c);
                        ifrom = 1;
                        break;
                    case TIS:
                        kiwi_snprintf_buf(tis, "%c%c%c", a,b,c);
                        itis = 1;
                        break;
                    default:
                        break;
                }
            } else {
	            // AQC-ALE protocol

                ber[nt] = berw;
                int adf = b20(w);
                u2_t p = bf(w,19,4);
                u2_t dx = bf(w,3,0);

                // ppp  d pppppppppppppppp xxxx
                //  21 20 19             4 3  0
                //        15             0
                
                //                                      111111111122222222223333333333
                //                            0123456789012345678901234567890123456789
                const char *packed_2_ascii = "*0123456789?@ABCDEFGHIJKLMNOPQRSTUVWXYZ_";
                const char ascii_2_packed[128] = {  // 7654
                     0,  0,  0,  0,  0,  0,  0,  0, // 0101
                     1,  2,  3,  4,  5,  6,  7,  8, // 0110
                     9, 10,  0,  0,  0,  0,  0, 11, // 0111
                    12, 13, 14, 15, 16, 17, 18, 19, // 1000
                    20, 21, 22, 23, 24, 25, 26, 27, // 1001
                    28, 29, 30, 31, 32, 33, 34, 35, // 1010
                    36, 37, 38,  0,  0,  0,  0, 39, // 1011
                };

                //  0  (    41  )    42  *    43  +    44  ,    45  -    46  .    47  /
                //  8  0    49  1    50  2    51  3    52  4    53  5    54  6    55  7
                // 16  8    17  9    18  :    19  ;    20  <    21  =    22  >    23  ?
                // 24  @    25  A    26  B    27  C    28  D    29  E    30  F    31  G
                // 32  H    33  I    34  J    35  K    36  L    37  M    38  N    39  O
                // 40  P    41  Q    42  R    43  S    44  T    45  U    46  V    47  W
                // 44  X    49  Y    50  Z    51  [    52  \    53  ]    54  ^    55  _


// 128: ic0 hc0 NTIME05 [00:00:28] $AQC [TWAS] 0x63a895 p=3 [adf=0 adf_ok=1] [14985(0x3a89) dx=5] 9='8' 14='B' 25='M' regen=14985(0x3a89)

                int ci = p % 40;
                char c = packed_2_ascii[ci];
                p /= 40;
                int bi = p % 40;
                char b = packed_2_ascii[bi];
                p /= 40;
                int ai = p % 40;
                char a = packed_2_ascii[ai];
                u1_t pa = ascii_2_packed[a - 40];
                u1_t pb = ascii_2_packed[b - 40];
                u1_t pc = ascii_2_packed[c - 40];
                u4_t regen = pa*1600 + pb*40 + pc;

                if (word_debug) kiwi_snprintf_ptr(s, N_MSG, "$AQC %s 0x%04x p=%d [adf=%d adf_ok=%d] [%d(0x%x) dx=%d] %s %s %s",
                    aqc_preamble_types[preamble],
                    w, bf(w,23,21), adf, adf == b15(p), p, p, dx,
                    cdeco(0,a), cdeco(1,b), cdeco(2,c));
                //kiwi_snprintf_ptr(s, N_MSG, " %d='%c' %d='%c' %d='%c' regen=%d(0x%x)", ai,a, bi,b, ci,c, regen, regen);

                rv = -1;
            }
	    }
	    
        if (word_debug && (dsp >= DBG || (dsp >= CMDS && icmd))) {
            //cprintf(ALL, ALL, icmd? YELLOW : "", "%d: ic%d NTIME%02d %s", zs, in_cmd, nt, message);
            //cprintf(ALL, ALL, icmd? YELLOW : "", "%5d: >icmd%d caller%d ic%d NTIME%02d %s", nsym, icmd, caller, in_cmd, nt, message);
            cprintf(ALL, ALL, icmd? YELLOW : "", ">icmd%d caller%d ic%d NTIME%02d %s", icmd, caller, in_cmd, nt, message);
        }
        
        zs++;
	    return rv;
	}

	void decode_ff_impl::log(char *cur, char *cur2, int state, int ber, const char *from)
	{
	    int i;
	    char message[N_MSG];
        zs=0;
        bool event = false;
        
	    dprintf("LOG from state=%s stage_num=%d from=%s ic%d\n",
	        state_s[state], stage_num, from, in_cmd);
        prev_data2[0] = '\0';

	    if (in_cmd) {
	        cmd_cnt++;
	        in_cmd = binary = 0;
	    }

	    if (state != S_CMD) {
            for (i=0; i < N_CUR; i++) if (cur [i] =='@') cur [i] = 0;
            for (i=0; i < N_CUR; i++) if (cur2[i] =='@') cur2[i] = 0;
        }

	    switch (state) {
            case S_TWAS:
                cprintf(ALL, DBG, CYAN, "[Sounding THIS WAS] [From: %s] [His BER: %d]", cur, ber);
                event = true;
                break;

            case S_TIS:
                cprintf(ALL, DBG, CYAN, "[Sounding THIS IS] [From: %s] [His BER: %d]", cur, ber);
                event = true;
                break;

            case S_TO:
                cprintf(ALL, DBG, CYAN, "[To: %s] [His BER: %d]", cur, ber);
                ext_send_msg(rx_chan, false, "EXT call_est_test");
                event = true;
                break;

            case S_CALL_EST:
                if (stage_num == 1) {
                    cprintf(ALL, DBG, CYAN, "[Call] [From: %s] [To: %s] [His BER: %d]", cur, cur2, ber);
                } else
                if (stage_num == 2) {
                    cprintf(ALL, DBG, CYAN, "[Call ACK] [From: %s] [To: %s] [His BER: %d]", cur, cur2, ber);
                } else
                if (stage_num == 3) {
                    cprintf(ALL, DBG, CYAN, "[Call EST] [From: %s] [To: %s] [His BER: %d]", cur, cur2, ber);
		            ext_send_msg(rx_chan, false, "EXT call_est");
                }
                event = true;
		        break;

		    default:
		        dprintf("LOG called from unhandled state %s?\n", state_s[state]);
		        break;
	    }
	    
	    for (i = 0; i < cmd_cnt; i++) {
	        cmd_t *cp = &cmds[i];
	        
            u1_t a = cp->cmd_s[0], b = cp->cmd_s[1], c = cp->cmd_s[2];
            u4_t w = (a << 14) | (b << 7) | c;
            u1_t d = cp->cmd_s[3], e = cp->cmd_s[4], f = cp->cmd_s[5];
            u4_t w2 = (d << 14) | (e << 7) | f;
            char *s = message;
            *s = '\0';
            int cond_d = CMDS;
            const char *cmd_type = "CMD";

            if (cp->cmd == AMD) {
                // AMD - automatic message display
                s += kiwi_snprintf_ptr(s, N_MSG, "AMD: \"%s", cp->cmd_s);

                // stuffed with space char, so trim trailing spaces
                char *e = s + strlen(s) -1;
                while (*e == ' ' || *e == '\r' || *e == '\n') e--;
                *(e+1) = '"';
                *(e+2) = '\0';
                cmd_type = "MSG";
                cond_d = DX;
                event = true;
            } else

            if (cp->cmd == DTM) {
                // DTM - data text message
                s += kiwi_snprintf_ptr(s, N_MSG, "DTM: \"%s", cp->cmd_s);

                // stuffed with space char, so trim trailing spaces
                char *e = s + strlen(s) - 1;
                while (*e == ' ' || *e == '\r' || *e == '\n') e--;
                *(e+1) = '"';
                *(e+2) = '\0';
                cmd_type = "MSG";
                cond_d = DX;
                event = true;
            } else

            if (cp->cmd == LQA) {
                // LQA
                kiwi_snprintf_ptr(s, N_MSG, "LQA: reply=%u MP=%u SINAD=%u BER=%u", b13(w2), bf(w2,12,10), bf(w2,9,5), bf(w2,4,0));
                event = true;
            } else

            if (cp->cmd == FRQ) {
                kiwi_snprintf_ptr(s, N_MSG, "Freq: con=%d b4(0)=%d %d_%d_%d_%d_%d.%d_%d",
                    bf(w,13,8), b20(w2), bf(w2,19,16), bf(w2,15,12), bf(w2,11,8), bf(w2,7,4), bf(w2,3,0), bf(w,7,4), bf(w,3,0));
                event = true;
            } else
            
            if (a == 'd') {     // data text message
                u1_t kd = bf(w,13,10);
                u2_t dc = bf(w,9,0);
                kiwi_snprintf_ptr(s, N_MSG, "DTM: kd=0x%x dc=0x%x", kd, dc); 
                event = true;
            } else

            if (cp->cmd == CRC) {
                kiwi_snprintf_ptr(s, N_MSG, "CRC");
            } else

            {   // OTH
                kiwi_snprintf_ptr(s, N_MSG, "Other: %s %s %s", cdeco(0,a), cdeco(1,b), cdeco(2,c));
            }
            
            cprintf(cond_d, DX, MAGENTA, "[%s] [%s] [His BER: %d]", cmd_type, message, ber);
	    }
	    
        if (event) ext_send_msg(rx_chan, false, "EXT event");
        cmd_cnt = 0;
        state = S_START;
        memset(cur,'\0', N_CUR);
	}

    void decode_ff_impl::cprintf_msg(int cond_d)
    {
        bool debug = false;
        bool have_msg = false;
	    time_t timestamp;
	    struct tm *tm;
	    timestamp = use_UTC? time(NULL) : secs;
	    tm = gmtime(&timestamp);
        char buf[256];
        
        if (cond_d != SHOW_DX) log_buf_empty = false;
        if (dsp != DX) have_msg = true;
        
        if (debug) printf("     dsp=%s(%d) cond_d=%s(%d) log_buf_empty=%d\n", dsp_s[dsp], dsp, dsp_s[cond_d], cond_d, log_buf_empty);
        if (cond_d != SHOW_DX && cond_d != CMDS && (dsp == DX || dsp == CMDS)) {
            have_msg = true;
            strcpy(locked_msg, log_buf);
            if (debug) printf("lock: %s", log_buf);
        }
        
        bool show_locked = (cond_d == SHOW_DX && !log_buf_empty);
        if (show_locked || (have_msg && dsp >= cond_d)) {
            if (use_UTC)
                kiwi_snprintf_buf(buf, "[%d-%02d-%02d %02d:%02d:%02d %.2f] %s",
                    1900 + tm->tm_year, tm->tm_mon + 1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec,
                    frequency, show_locked? locked_msg : log_buf);
            else
                kiwi_snprintf_buf(buf, "[%02d:%02d:%02d] %s", tm->tm_hour, tm->tm_min, tm->tm_sec,
                    show_locked? locked_msg : log_buf);
	    
            #ifdef KIWI
                #ifdef HOST
                    ext_send_msg_encoded(rx_chan, false, "EXT", "chars", buf);
                #endif
            #else
                printf(">>>>  %s", buf);
            #endif
            
            log_buf_empty = true;
            if (debug) printf("     log_buf_empty=%d\n", log_buf_empty);
        }
    }

    void decode_ff_impl::dprintf_msg()
    {
        char buf[256];
	    time_t timestamp;
	    struct tm *tm;
	    timestamp = use_UTC? time(NULL) : secs;
	    tm = gmtime(&timestamp);
	    
        if (use_UTC)
            kiwi_snprintf_buf(buf, "[%d-%02d-%02d %02d:%02d:%02d %.2f] debug: %s %s",
                1900 + tm->tm_year, tm->tm_mon + 1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec,
                frequency, state_s[state], dpf_buf);
        else
            kiwi_snprintf_buf(buf, "[%02d:%02d:%02d] debug: %s %s", tm->tm_hour, tm->tm_min, tm->tm_sec,
                state_s[state], dpf_buf);
	    
        #ifdef KIWI
            #ifdef HOST
                ext_send_msg_encoded(rx_chan, false, "EXT", "chars", buf);
            #endif
        #else
            int sl = strlen(buf);
            if (buf[sl-1] == '\n') buf[sl-1] = '\0';
            printf(">>>>  %s\n", buf);
        #endif
    }

	void decode_ff_impl::do_modem2()
	{
        if (ito2) {
            if ((state != S_START) && (state != S_TO)) log(current, current2, state, lastber, "S_TO");
            if (in_cmd) cmd_cnt++;
            in_cmd = binary = 0;
            strcpy(current, to2);
            dprintf("To: %s <%s>\n", to, current);
            state = S_TO;
            state_count = 0;
            lastber = bestber;
        }

        if (itwas2) {
            if ((state != S_START) /*&& (state != S_TWAS)*/) log(current, current2, state, lastber, "S_TWAS");
            if (in_cmd) cmd_cnt++;
            in_cmd = binary = 0;
            //dprintf("TWAS: twas2= <%s> %d %s %s %s %s\n", twas2, (int) strlen(twas2),
            //    cdeco(0,twas2[0]), cdeco(1,twas2[1]), cdeco(2,twas2[2]), cdeco(3,twas2[3]));
            strcpy(current, twas2);
            //dprintf("TWAS: %s %d current= <%s> %d %s %s %s %s %s %s\n", twas, (int) strlen(twas), current, (int) strlen(current),
            //    cdeco(0,current[0]), cdeco(1,current[1]), cdeco(2,current[2]), cdeco(3,current[3]), cdeco(4,current[4]), cdeco(5,current[5]));
            dprintf("TWAS: %s <%s>\n", twas, current);
            state = S_TWAS;
            state_count = 0;
            lastber = bestber;
        }

        if (itis2) {
            if ((state != S_START) && (state != S_TO) && (state != S_TIS) && (state != S_CALL_EST)) log(current, current2, state, lastber, "S_TIS");
            if (in_cmd) cmd_cnt++;
            in_cmd = binary = 0;
            dprintf("TIS: %s\n", tis);
            if (state == S_TO) {
                memset(s1,0,4);
                memset(s2,0,4);
                strncpy(s1, current, 3);
                strncpy(s2, current2, 3);
                strcpy(current2, current);
                state = S_CALL_EST;
                if ((stage_num == 0) || (stage_num > 3)) stage_num = 0;
                if (stage_num > 0) {
                    dprintf("stage=%d tis=%s, s2=%s, current=%s, s1=%s\n", stage_num, tis, s2, current, s1);
                    if ((strncmp(tis2, s2, 3) == 0) && (strncmp(current, s1, 3) == 0)) {
                        stage_num++;
                    } else {
                        stage_num = 1;
                    }
                } else {
                    stage_num = 1;
                }
                state_count = 0;
            } else {
              state = S_TIS;
              state_count = 0;
            }
            strcpy(current, tis2);
            lastber = bestber;
        }

        if (ifrom2) {
            if ((state != S_START) && (state != S_FROM)) log(current, current2, state, lastber, "S_FROM");
            if (in_cmd) cmd_cnt++;
            in_cmd = binary = 0;
            strcpy(current, from2);
            dprintf("From: %s <%s>\n", from, current);
            state = S_FROM;
            state_count = 0;
            lastber = bestber;
        }

        if (icmd2 && in_cmd) cmd_cnt++;     // multiple cmds back-to-back in one frame are possible
        cmd_t *cp = &cmds[cmd_cnt];

        if (idata2) {
            char *s = in_cmd? cmds[cmd_cnt].cmd_s : current;
            if ((state > S_START) && (state < S_END)) {
                dprintf("%sidata2: CMP prev_data2=<%s> data2=<%s>%s\n", YELLOW, prev_data2, data2, NORM);
                if (strcmp(data2, prev_data2) == 0) {
                    dprintf("%sidata2: SKIP DUP%s\n", YELLOW, NORM);
                } else {
                    strcat(s, data2);
                    dprintf("%sidata2: ADD DATA%s\n", YELLOW, NORM);
                }
                strcpy(prev_data2, data2);
            }
            if (in_cmd && binary) {
                dprintf("Data: %s ic%d a64=%d %02x %02x %02x %02x %02x %02x ...\n",
                    data, in_cmd, ascii_64_ok, s[0], s[1], s[2], s[3], s[4], s[5]);
            } else {
                dprintf("Data: %s ic%d a64=%d <%s>\n",
                    data, in_cmd, ascii_64_ok, s);
            }
            state_count = 0;
            lastber = bestber;
        }

        if (irep2) {
            char *s = in_cmd? cmds[cmd_cnt].cmd_s : current;
            if ((state > S_START) && (state < S_END)) {
                strcat(s, rep2);
            }
            if (in_cmd && binary) {
                dprintf("Rep: %s ic%d a64=%d %02x %02x %02x %02x %02x %02x ...\n",
                    rep, in_cmd, ascii_64_ok, s[0], s[1], s[2], s[3], s[4], s[5]);
            } else {
                dprintf("Data: %s ic%d a64=%d <%s>\n",
                    rep, in_cmd, ascii_64_ok, s);
            }
            state_count = 0;
            lastber = bestber;
        }

        if (icmd2) {
            in_cmd = 1;
            dprintf("CMD: state=%s\n", state_s[state]);
            //if ((state != S_START) && (state != S_CMD)) log(current, current2, state, lastber, "S_CMD");
            u1_t a = cmd[0], b = cmd[1], c = cmd[2];
            u4_t w = (a << 14) | (b << 7) | c;
            strcpy(cp->cmd_s, cmd);
            
            if (ascii_64_ok) {
                cp->cmd = AMD;
                // ascii_64_ok means all 3 chars are part of message
            } else {
                cp->data = w;
                binary = 1;

                switch (a) {
                
                    case '\"':
                        cp->cmd = ALQA;
                        break;
                
                    case 'a':
                        cp->cmd = LQA;
                        break;
                
                    case 'b':
                        cp->cmd = DATA_BLK;
                        break;
                
                    case 'c':
                        cp->cmd = CHAN;
                        break;
                
                    case 'd':
                        cp->cmd = DTM;
                        cp->cmd_s[0] = '\0';    // prepare for subsequent appending by data/rep
                        binary = 0;
                        break;
                
                    case 'f':
                        cp->cmd = FRQ;
                        break;
                
                    case 'm':
                        cp->cmd = MODE_SEL;
                        break;
                
                    case 'n':
                        cp->cmd = NOISE_RPT;
                        break;
                
                    case 'p':
                        cp->cmd = PWR_CTRL;
                        break;
                
                    case 'r':
                        cp->cmd = LQA_RPT;
                        break;
                
                    case 't':
                        cp->cmd = SCHED;
                        break;
                
                    case 'v':
                        cp->cmd = VERS_CAP;
                        break;
                
                    case 'x':
                    case 'y':
                    case 'z':
                    case '{':
                        cp->cmd = CRC;
                        break;
                
                    case '|':
                        cp->cmd = USER_FUNC;
                        break;
                
                    case '~':
                        cp->cmd = TIME_EXCH;
                        break;
                
                    default:
                        cp->cmd = OTH;
                        break;
                }
            }
            
            if (cp->cmd == AMD) {
                dprintf("CMD: %s %s %s %s\n", CMD_types[cp->cmd], cdeco(0,a), cdeco(1,b), cdeco(2,c));
            } else {
                dprintf("CMD: %s\n", CMD_types[cp->cmd]);
            }

            state_count = 0;
            lastber = bestber;
        }
	}

	void decode_ff_impl::do_modem1(bool eof)
	{
	    int i,n;
	    
	    ALE_REAL new_sample;
	    ALE_REAL old_sample;
	    ALE_REAL temp_real;
	    ALE_REAL temp_imag;
	    
	    int length = HALF_FFT_SIZE;
	    int temppos = 0;
	    
	    memset(to2, 0, sizeof(to2));
	    memset(from2, 0, sizeof(from2));
	    memset(tis2, 0, sizeof(tis2));
	    memset(twas2, 0, sizeof(twas2));
	    memset(data2, 0, sizeof(data2));
	    memset(rep2, 0, sizeof(rep2));
	    memset(cmd2, 0, sizeof(cmd2));

	    if (eof) {
            log(current, current2, state, lastber, "EOF");
            if (dsp == DX || dsp == CMDS) cprintf_msg(SHOW_DX);
	        dprintf("--- EOF ---\n");
            return;
        }

        //if (!notify) { dprintf("--- MAG %s ---\n", calc_mag? "SQRT" : "SIMPLE"); notify = true; }

        //#define ALE_2G_USE_FFTW

	    for (int ipos = 0; ipos < length; ipos++) {

            new_sample = inbuf[ipos];

            decode_ff_array_dim(fft_history_offset, FFT_SIZE);
            old_sample = fft_history[fft_history_offset];
            fft_history[fft_history_offset] = new_sample;

            #ifdef ALE_2G_USE_FFTW
                for (n=0; n < HALF_FFT_SIZE; n++) {
                    ...
                }
                fftwf_execute(dft_plan);
                for (n=0; n < HALF_FFT_SIZE; n++) {
                    ...
                }
            #else
                for (n=0; n < HALF_FFT_SIZE; n++) {
                    temp_real       = fft_out[n].real - old_sample + new_sample;
                    temp_imag       = fft_out[n].imag;
                    fft_out[n].real = (temp_real * fft_cs_twiddle[n]) - (temp_imag * fft_ss_twiddle[n]);
                    fft_out[n].imag = (temp_real * fft_ss_twiddle[n]) + (temp_imag * fft_cs_twiddle[n]);
                
                    #if 1
                        #ifdef ALG_MAG_INT
                            fft_mag[n] = (int) roundf(sqrt((fft_out[n].real * fft_out[n].real) + (fft_out[n].imag * fft_out[n].imag)) * 5);
                        #else
                            fft_mag[n] = sqrt((fft_out[n].real * fft_out[n].real) + (fft_out[n].imag * fft_out[n].imag)) * 5;
                        #endif
                        #ifdef STANDALONE_TEST
                            //print_max_min_stream_f(&mag_state, P_MAX_MIN_DEMAND, "mag", n, 1, (double) fft_mag[n]);
                        #endif
                    #else
                        if (calc_mag) {
                            fft_mag[n] = sqrtf((fft_out[n].real * fft_out[n].real) + (fft_out[n].imag * fft_out[n].imag)) * 5;
                        } else {
                            // since fft_mag is only used for relative comparisons maybe it doesn't have to be the precise definition of magnitude?
                            fft_mag[n] = (fft_out[n].real * fft_out[n].real) + (fft_out[n].imag * fft_out[n].imag);
                        }
                    #endif
                    // drop fft_out[]
                }
            #endif

	        int nt, nf;
            ALE_MAG max_magnitude = 0;
            int max_freq_offset = 0;
            // window FFT result: HALF_FFT_SIZE = FFT_SIZE/2 = 32 => [0 (1..27) 28,29,30,31]
            for (nf = 1; nf <= HALF_FFT_SIZE - 5; nf++) {
                if ((fft_mag[nf] > max_magnitude)) {
                    max_magnitude = fft_mag[nf];
                    max_freq_offset = nf;
                }
            }
            #ifdef STANDALONE_TEST
                //print_max_min_stream_f(&mag_state, P_MAX_MIN_DUMP, "mag");
            #endif
            // drop fft_mag[]

            decode_ff_array_dim(samp_count_per_symbol, FFT_SIZE);
            for (nt = 0; nt < NTIME; nt++) {
                mag_sum[nt][samp_count_per_symbol] += max_magnitude;
                #ifdef STANDALONE_TEST
                    //print_max_min_stream_f(&mag_state, P_MAX_MIN_DEMAND, "mag_sum", (n * NTIME) + samp_count_per_symbol, 1, (double) mag_sum[nt][samp_count_per_symbol]);
                #endif
                //mag_history[nt][samp_count_per_symbol][mag_hist_offset_per_word] = max_magnitude;
            }
            // drop max_magnitude (first use)

            #define ORIGINAL_VERY_SLOW_CODE
            #ifdef ORIGINAL_VERY_SLOW_CODE
                for (nt = 0; nt < NTIME; nt++) {
                    if (word_sync[nt] == NOT_WORD_SYNC) {
                        max_magnitude = 0;
                        for (nf = 0; nf < FFT_SIZE; nf++) {
                            if (mag_sum[nt][nf] > max_magnitude) {
                                max_magnitude = mag_sum[nt][nf];
                                last_sync_position[nt] = nf;
                            }
                        }
                    }
                }
            #else
                // Made-up code for testing that eliminates the excessive overhead of the above
                // while hopefully not causing the optimizer to throw it all away.
                nt = (ipos % NTIME) >> 1;
                if (mag_sum[nt][ipos] && word_sync[nt]) last_sync_position[nt] = ipos >> 1;
            #endif
            // drop max_magnitude (second use)
            // last use of mag_sum[][] until zeroed at end of SYMBOLS_PER_WORD

            decode_ff_array_dim(max_freq_offset, NFREQ);
            for (nt = 0; nt < NTIME; nt++) {
                if (samp_count_per_symbol == last_sync_position[nt]) {
                    last_symbol[nt] = g_symbol_lookup[nt][max_freq_offset];  // g_symbol_lookup[NTIME][NFREQ]
                }
            }
            // drop last_sync_position[] max_freq_offset

            // done once per FFT effectively
            if (samp_count_per_symbol == 0) {
                ito = ifrom = itis = itwas = irep = idata = icmd = 0;
                ito2 = ifrom2 = itis2 = itwas2 = irep2 = idata2 = icmd2 = 0;
                bestber = HIGH_BER;

                int sym_rv0_cnt = 0;
                activity_cnt = 0;

                for (nt = 0; nt < NTIME; nt++) {
                    int sym_rv = modem_new_symbol(last_symbol[nt], nt);
                    if (sym_rv) { sym_rv0_cnt++; activity_cnt++; }

                    bool isBestBER = (ber[nt] < bestber);
                    bool isBestPosition = ((bestpos == 0) || (bestpos == nt));
                    bool best = (isBestBER && isBestPosition);
                    bool word = (ito || ifrom || itis || itwas || irep || idata || icmd);

                    if (gdebug & 2
                        && sym_rv
                    ) {
                        u4_t w = current_word;
                        u1_t preamble = bf(w,23,21);
                        u1_t a = bf(w,20,14);
                        u1_t b = bf(w,13,7);
                        u1_t c = bf(w,6,0);
                        printf("%5d: LSYM%d NTIME%2d %ssym_rv%d%s // ber[n]|%2d < bestber|%2d %s // bestpos=%2d|0|%2d %s // w=%06x %-6s %s %s %s // %s %s\n",
                            nsym, last_symbol[nt], nt, sym_rv? YELLOW : "", sym_rv, NORM,
                            ber[nt], bestber, isBestBER? (CYAN "Y" NORM) : "N",
                            bestpos, nt, isBestPosition? (CYAN "Y" NORM) : "N",
                            w, preamble_types[preamble], cdeco(0,a), cdeco(1,b), cdeco(2,c),
                            (sym_rv)? (GREEN "WORD" NORM) : "",
                            best? (MAGENTA "BEST" NORM) : "");
                    }

                    #define ORIG_BEST_BER
                    #ifdef ORIG_BEST_BER
                        if (best) {
                            decode_ff_array_dim(nt_new, NTIME);
                            bestber = ber[nt_new];
                            temppos = nt;
                        }
                    #else
                        if (isBestBER) {
                            decode_ff_array_dim(nt_new, NTIME);
                            bestber = ber[nt_new];
                        }
                        if (best) {
                            temppos = nt;
                        }
                    #endif
                    
                    nsym++;
                }
                
                // KiwiSDR fix: require two good symbols in NTIME loop before advancing iXXX state
                if (sym_rv0_cnt >= 2) {
                    ito2 = ito;     memcpy(to2, to, 3);
                    ifrom2 = ifrom; memcpy(from2, from, 3);
                    itis2 = itis;   memcpy(tis2, tis, 3);
                    itwas2 = itwas; memcpy(twas2, twas, 3);
                    irep2 = irep;   memcpy(rep2, rep, 3);
                    idata2 = idata; memcpy(data2, data, 3);
                    icmd2 = icmd;   memcpy(cmd2, cmd, 3);
                    if (gdebug & 1 && icmd) printf("%sicmd2=%d icmd=%d cmd2=%s cmd=%s%s\n", RED, icmd2, icmd, cmd2, cmd, NORM);
                }
                
                if (activity_cnt >= 2 && !active) {
                    ext_send_msg(rx_chan, false, "EXT active=%.2f", frequency);
                    //real_printf("#%d#", slot(frequency)); fflush(stdout);
                    //snd_send_msg_encoded(rx_chan, true, "MSG", "audio_flags2", "active:%.2f", frequency);
                    active = 1;
                }
        
                if (temppos != 0) bestpos = temppos;
                nt_new = bestpos;
                decode_ff_array_dim(nt_new, NTIME);
            
                ale::decode_ff_impl::do_modem2();

                // after 2 seconds of no state changes output log entry
                state_count += FFT_SIZE;
                if ((state_count >= (2 * SAMP_RATE_SPS)) && state) {
                    log(current, current2, state, lastber, "QUIET");
                    if (dsp == DX || dsp == CMDS) cprintf_msg(SHOW_DX);
                    dprintf("--- QUIET ---\n");
                    state = S_START;
                    state_count = 0;
                    active = 0;
                    ext_send_msg(rx_chan, false, "EXT active=0");
                    //real_printf("///"); fflush(stdout);
                    //snd_send_msg_encoded(rx_chan, true, "MSG", "audio_flags2", "active:0");
                    //modem_reset();
                }

            }   // samp_count_per_symbol == 0

            fft_history_offset = (fft_history_offset + 1) % FFT_SIZE;

            samp_count_per_symbol = (samp_count_per_symbol + 1) % FFT_SIZE;   // same as % SAMPS_PER_SYMBOL
            if (samp_count_per_symbol == 0) {
                mag_hist_offset_per_word = (mag_hist_offset_per_word + 1) % SYMBOLS_PER_WORD;

                if (mag_hist_offset_per_word == 0) {
                    #ifdef STANDALONE_TEST
                        //print_max_min_stream_f(&mag_state, P_MAX_MIN_DUMP, "mag_sum");
                    #endif
                    memset(mag_sum, 0, sizeof(mag_sum));
                }
            }
	    }

        timer_samps += length;
        secs = timer_samps / SAMP_RATE_SPS;     // keep track of realtime in file (in seconds)
	}

	void decode_ff_impl::do_modem(float *sample, int length)
	{
	    if (length == 0) {
            ale::decode_ff_impl::do_modem1(true);
	        return;
	    }
	    
	    for (int i = 0; i < length; i++) {
	        inbuf[inbuf_i++] = sample[i];
	        if (inbuf_i == HALF_FFT_SIZE) {
                ale::decode_ff_impl::do_modem1();
                inbuf_i = 0;
	        }
	    }
	}
	
    void decode_ff_impl::do_modem_resampled(short *sample, int length)
    {
        int i;
        
        #ifdef STANDALONE_TEST
        #else
            if (ResampleObj == nullptr) return;
            const int iOutputBlockSize = ResampleObj->iOutputBlockSize;

            for (i = 0; i < length; i++) {
                vecTempResBufIn[i] = sample[i];
            }
        
            ResampleObj->Resample(vecTempResBufIn, vecTempResBufOut);
            //printf("."); fflush(stdout);
            ale::decode_ff_impl::do_modem(&vecTempResBufOut[0], iOutputBlockSize);
        #endif
    }
    
	void decode_ff_impl::run_standalone(int fileno, int offset)
	{
	    #ifdef STANDALONE_TEST
	        printf("----------------------------------------------------- %d\n", offset);
	        #ifdef ALE_MAG_INT
	            printf("ALE_MAG_INT\n");
	        #else
	            printf("ALE_MAG_REAL\n");
	        #endif
	        
	        static FILE *fp;
	        if (fp == NULL) {
                printf("file #%d = %s, calc_mag = %d\n", fileno, sa_fn[fileno], calc_mag);
                fp = fopen(sa_fn[fileno], "r");
                if (fp == NULL) { printf("fopen failed: %s\n", sa_fn[fileno]); exit(-1); }
	            timer_samps = 0;
	            notify = false;
            } else {
                rewind(fp);
            }
            
            fread(sbuf, 2, 3, fp);
            u2_t off;
            fread(&off, 2, 1, fp);
            off = FLIP16(off);
            #define OFFSET_TO_MAKE_WORK 4
            if (offset < 0) offset = OFFSET_TO_MAKE_WORK;
            u2_t off2 = off - 8 + offset;
            printf("off=%d(0x%x) off2=%d(0x%x) offset=%d\n", off, off, off2+8, off2+8, offset);
            fread(sbuf, 1, off2, fp);

            int n;
            int first = 1;
            do {
                n = fread(sbuf, 2, N_SAMP_BUF, fp);
                if (n != N_SAMP_BUF) break;
                for (int i = 0; i < n; i += 1) {
                    fbuf[i] = (float) (s2_t) FLIP16(sbuf[i]);
                    if (first) { first = 0; printf("first=0x%x\n", FLIP16(sbuf[i])); }
                }
                //printf("%d ", n); fflush(stdout);
                ale::decode_ff_impl::do_modem(fbuf, n);
            } while (n > 0);
            
            ale::decode_ff_impl::do_modem(fbuf, 0);
            fclose(fp);
            fp = NULL;
        #endif
	}
	
	void decode_ff_impl::set_freq(double freq)
	{
	    // ignore any "SET tune=" that was in-flight while signal detection is active
	    if (active) return;
	    
	    // about to change frequency -- suppress any impending activity detection
	    activity_cnt = active = 0;
	    frequency = freq;
	}

	void decode_ff_impl::set_display(int display)
	{
	    dsp = display;
	    if (dsp == DBG)
	        gdebug = 0;
	        //gdebug = 7;
	}

	void decode_ff_impl::modem_reset()
	{
        inbuf_i = 0;
	    activity_cnt = active = 0;
	    in_cmd = binary = 0;
	    cmd_cnt = 0;
	    bestpos = 0;
	    lastber = HIGH_BER;
	    fft_history_offset = 0;
	    stage_num = 0; state_count = 0; state = S_START;
	    samp_count_per_symbol = 0;
	    nsym = 0;
	    mag_hist_offset_per_word = 0;

        memset(fft_out, 0, sizeof(fft_out));
        memset(fft_mag, 0, sizeof(fft_mag));
        memset(fft_history, 0, sizeof(fft_history));
        memset(mag_sum, 0, sizeof(mag_sum));
        memset(bits, 0, sizeof(bits));
        memset(word_sync, NOT_WORD_SYNC, sizeof(word_sync));
        memset(last_symbol, 0, sizeof(last_symbol));
        memset(last_sync_position, 0, sizeof(last_sync_position));
        memset(ber, HIGH_BER, sizeof(ber));
        memset(input_buffer_pos, 0, sizeof(input_buffer_pos));
        memset(word_sync_position, 0, sizeof(word_sync_position));
	}

	void decode_ff_impl::modem_init(int rx_chan, bool use_new_resampler, float f_srate, int n_samps, bool use_UTC)
	{
	    int i;
	    
	    this->rx_chan = rx_chan;
	    this->use_UTC = use_UTC;
	    calc_mag = true;
        notify = false;
	    timer_samps = 0;
	    frequency = 0;
        log_buf_empty = true;

        ale::decode_ff_impl::modem_reset();

        #ifdef STANDALONE_TEST
            // there is never a resampler when running standalone (use an 8k file)
        #else
            if (use_new_resampler) {
                // resampler
                ResampleObj = new CAudioResample();
                float ratio = SAMP_RATE_SPS / f_srate;
            
                const int fixedInputSize = n_samps;
                ResampleObj->Init(fixedInputSize, ratio);
                const int iMaxInputSize = ResampleObj->GetMaxInputSize();
                vecTempResBufIn.Init(iMaxInputSize, (_REAL) 0.0);

                const int iMaxOutputSize = ResampleObj->iOutputBlockSize;
                vecTempResBufOut.Init(iMaxOutputSize, (_REAL) 0.0);

                ResampleObj->Reset();
                //printf("### using NEW resampler: ratio=%f srate=%f n_samps=%d iOutputBlockSize=%d\n",
                //    ratio, f_srate, n_samps, ResampleObj->iOutputBlockSize);
            } else {
                printf("### using OLD resampler\n");
            }
        #endif
	}

} /* namespace ale */


#ifdef STANDALONE_TEST
    
int main(int argc, char *argv[])
{
    int i;
    //printf("encode_table=%d error=%d\n", ARRAY_LEN(encode_table), ARRAY_LEN(error));
    
    #define ARG(s) (strcmp(argv[ai], s) == 0)
    #define ARGP() argv[++ai]
    #define ARGB(v) (v) = true;
    #define ARGL(v) if (ai+1 < argc) (v) = strtol(argv[++ai], 0, 0);
    
    int display = DX;
    //int display = CMDS;
    //int display = ALL;
    int fileno = 0, repeat = 1;
    //int offset = -1;
    int offset = 0;
    bool use_new_resampler = false;     // there is never a resampler when running standalone (use an 8k file)
    bool compare_mag = false;
    bool compare_dsp_all = false;

    ale::decode_ff_impl decode;
    decode.modem_init(0, use_new_resampler, 0, N_SAMP_BUF, false);   // false = use file realtime instead of current UTC

    for (int ai = 1; ai < argc; ) {
        if (ARG("-f") || ARG("--file")) ARGL(fileno);
        if (ARG("-o") || ARG("--offset")) ARGL(offset);
        if (ARG("-r") || ARG("--repeat")) ARGL(repeat);
        if (ARG("-m") || ARG("--mag")) ARGB(compare_mag);
        if (ARG("-y")) ARGB(compare_dsp_all);

        // display
        if (ARG("-x") || ARG("--dx")) display = DX;
        if (ARG("-c") || ARG("--cmds")) display = CMDS;
        if (ARG("-a") || ARG("--all")) display = ALL;
        if (ARG("-d") || ARG("--debug")) display = DBG;
        ai++;
    }
    
    int ars = ARRAY_LEN(sa_fn);
    if (fileno >= ars) {
        printf("range error: --file 0..%d\n", ars-1);
        exit(-1);
    }
    

    int start = (fileno < 0)? 0 : fileno, stop = (fileno < 0)? (ars-1) : fileno;
    for (int f = start; f <= stop; f++) {
        if (offset >= 0) {
            for (i = 0; i < repeat; i++) {
                decode.set_display(display);

                decode.calc_mag = true;
                decode.run_standalone(f, offset);
                decode.modem_reset();

                if (compare_mag) {
                    decode.calc_mag = false;
                    decode.run_standalone(f, offset);
                    decode.modem_reset();
                }
                
                if (compare_dsp_all) {
                    decode.set_display(ALL);

                    decode.calc_mag = true;
                    decode.run_standalone(f, offset);
                    decode.modem_reset();

                    if (compare_mag) {
                        decode.calc_mag = false;
                        decode.run_standalone(f, offset);
                        decode.modem_reset();
                    }
                }
            }
        } else {
            decode.set_display(display);

            for (offset = 0; offset <= 32; offset += 4) {
                decode.run_standalone(f, offset);
                decode.modem_reset();
            }
        }
    }
}
#endif

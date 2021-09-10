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
    #define STANDALONE_TEST
#endif

// this is here before Kiwi includes to prevent our "#define printf ALT_PRINTF" mechanism being disturbed
#include "decode_ff_impl.h"

int gdebug = 0;

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
            if (dsp >= cond_d) { \
                rclprintf(rx_chan, fmt "\n", ## __VA_ARGS__); \
                if (dsp >= cond_c) \
                    ext_send_msg_encoded(rx_chan, false, "EXT", "chars", "%s" fmt NORM "\n", color, ## __VA_ARGS__); \
                else \
                    ext_send_msg_encoded(rx_chan, false, "EXT", "chars", fmt "\n", ## __VA_ARGS__); \
            }
    #else
        #define cprintf(cond_d, cond_c, color, fmt, ...) \
            if (dsp >= cond_d) { \
                if (dsp >= cond_c) \
                    printf("%s" fmt NORM "\n", color, ## __VA_ARGS__); \
                else \
                    printf(fmt "\n", ## __VA_ARGS__); \
            }
    #endif
#else
    // standalone
    #define cprintf(cond_d, cond_c, color, fmt, ...) \
        if (dsp >= cond_d) { \
            if (dsp >= cond_c) \
                printf("%s" fmt NORM "\n", color, ## __VA_ARGS__); \
            else \
                    printf(fmt "\n", ## __VA_ARGS__); \
            }
#endif

#define DEBUG_PRINTF
#ifdef DEBUG_PRINTF
    #ifdef KIWI
        #define dprintf(fmt, ...) \
            /* if (dsp == DBG) real_printf(fmt, ## __VA_ARGS__) */ \
            if (dsp == DBG) ext_send_msg_encoded(rx_chan, false, "EXT", "chars", fmt, ## __VA_ARGS__);
    #else
        // standalone
        #define dprintf(fmt, ...) \
            if (dsp == DBG) printf(fmt, ## __VA_ARGS__)
    #endif
#else
    #define dprintf(fmt, ...)
#endif

static char *cdeco(int idx, char c)
{
    static char s[16][16];
    sprintf(s[idx], "%c(%02x)", (c >= ' ' && c < 0x7f)? c : '?', c);
    return s[idx];
}

int slot(double freq)
{
    if (fabs(freq - 23982.60) < 0.001) return 0;
    if (fabs(freq - 24000.60) < 0.001) return 1;
    if (fabs(freq - 24013.00) < 0.001) return 2;
    return 999;
}

namespace ale {

	decode_ff_impl::decode_ff_impl(): ResampleObj(nullptr) { }

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
            worda = input[i++]? (worda << 1) +1 : worda << 1 ;
	        decode_ff_array_dim(i, VOTE_BUFFER_LENGTH);
            wordb = input[i++]? (wordb << 1) +1 : wordb << 1 ; 
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

	int decode_ff_impl::modem_new_symbol(int sym, int nr)
	{
	    int majority_vote_array[VOTE_BUFFER_LENGTH];
	    int bad_votes, sum, errors, i;
	    u4_t word = 0;
	    int j, rv;

	    inew = nr;

        decode_ff_array_dim(sym, 8+1);
        decode_ff_array_dim(nr, NR);

        j = input_buffer_pos[nr];
        decode_ff_array_dim(j, VOTE_ARRAY_LENGTH);
	    bits[nr][j] = (sym & 4) ? 1 : 0;
	    input_buffer_pos[nr] = (input_buffer_pos[nr]+1) % VOTE_ARRAY_LENGTH;

        j = input_buffer_pos[nr];
        decode_ff_array_dim(j, VOTE_ARRAY_LENGTH);
	    bits[nr][j] = (sym & 2) ? 1 : 0;
	    input_buffer_pos[nr] = (input_buffer_pos[nr]+1) % VOTE_ARRAY_LENGTH;

        j = input_buffer_pos[nr];
        decode_ff_array_dim(j, VOTE_ARRAY_LENGTH);
	    bits[nr][j] = (sym & 1) ? 1 : 0;
	    input_buffer_pos[nr] = (input_buffer_pos[nr]+1) % VOTE_ARRAY_LENGTH;

	    bad_votes = 0;
	    for (i=0; i < VOTE_BUFFER_LENGTH; i++) {
	        j = (i + input_buffer_pos[nr]) % VOTE_ARRAY_LENGTH;
            decode_ff_array_dim(j, VOTE_ARRAY_LENGTH);
            sum  = bits[nr][j];

            j = (i + input_buffer_pos[nr] + SYMBOLS_PER_WORD) % VOTE_ARRAY_LENGTH;
            decode_ff_array_dim(j, VOTE_ARRAY_LENGTH);
            sum += bits[nr][j];

            j = (i + input_buffer_pos[nr] + (2*SYMBOLS_PER_WORD)) % VOTE_ARRAY_LENGTH;
            decode_ff_array_dim(j, VOTE_ARRAY_LENGTH);
            sum += bits[nr][j];

            if ((sum == 1) || (sum == 2)) bad_votes++; 
            decode_ff_array_dim(sum, 4);
            majority_vote_array[i] = vote_lookup[sum];
	    }

	    ber[nr] = 26;
	    rv = 0;

	    if (word_sync[nr] == NOT_WORD_SYNC) {
            if (bad_votes <= BAD_VOTE_THRESHOLD) {
                word = modem_de_interleave_and_fec(majority_vote_array, &errors);
                if (errors <= SYNC_ERROR_THRESHOLD) {
                    /*int err = */ decode_word(word, nr, bad_votes, 0);
                    word_sync[nr] = WORD_SYNC;
                    word_sync_position[nr] = input_buffer_pos[nr];
                    rv = 1;
                }
            }
	    } else {
            if (input_buffer_pos[nr] == word_sync_position[nr]) {
                word = modem_de_interleave_and_fec(majority_vote_array, &errors);
                decode_word(word, nr, bad_votes, 1);
                rv = 2;
            } else {
                word_sync[nr] = NOT_WORD_SYNC;
            }
	    }
	    
	    return rv;
	}

    static int zs, zz;
    
	int decode_ff_impl::decode_word(u4_t w, int nr, int berw, int caller)
	{
	    u1_t a, b, c, preamble;
	    int rv = 1;
	    
	    time_t timestamp;
	    struct tm *tm;
	    char message[256];
	    char *s = message;
	    *s = '\0';
	    int n;
        cmd_t *cp = &cmds[cmd_cnt];

	    timestamp = use_UTC? time(NULL) : secs;
	    tm = gmtime(&timestamp);
	    s += sprintf(s, "[%02d:%02d:%02d] ", tm->tm_hour, tm->tm_min, tm->tm_sec);

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
            ber[nr] = berw;
            s += sprintf(s, "CMD a38=%d a64=%d %s %s %s 0x%04x %d %2x %2x %2x", ascii_38_ok, ascii_64_ok,
                cdeco(0,a), cdeco(1,b), cdeco(2,c),
                w, bf(w,23,21), bf(w,20,14), bf(w,13,7), bf(w,6,0));
            sprintf(cmd, "%c%c%c", a,b,c);
	        icmd = 1;
	        if (gdebug >= 1) printf("%scmd=1 %s%s\n", GREEN, cmd, NORM);
	    } else
	    
	    if (preamble == DATA) {
            s += sprintf(s, "DATA a38=%d a64=%d %s %s %s 0x%04x %d %2x %2x %2x", ascii_38_ok, ascii_64_ok,
                cdeco(0,a), cdeco(1,b), cdeco(2,c),
                w, bf(w,23,21), bf(w,20,14), bf(w,13,7), bf(w,6,0));
            if (ascii_38_ok || (ascii_nl_ok && in_cmd && (cp->cmd == AMD || cp->cmd == DTM))) {
                ber[nr] = berw;
                sprintf(data, "%c%c%c", a,b,c);
                idata = 1;
            } else {
                s += sprintf(s, " IGNORED");
                activity_cnt = 0;       // prevent false activity triggering
            }
	    } else
	    
	    if (preamble == REP) {
            s += sprintf(s, "REP a38=%d a64=%d %s %s %s 0x%04x %d %2x %2x %2x", ascii_38_ok, ascii_64_ok,
                cdeco(0,a), cdeco(1,b), cdeco(2,c),
                w, bf(w,23,21), bf(w,20,14), bf(w,13,7), bf(w,6,0));
            if (ascii_38_ok || (ascii_nl_ok && in_cmd && (cp->cmd == AMD || cp->cmd == DTM))) {
                ber[nr] = berw;
                sprintf(rep, "%c%c%c", a,b,c);
                irep = 1;
            } else {
                s += sprintf(s, " IGNORED");
                activity_cnt = 0;       // prevent false activity triggering
            }
	    } else
	    
	    {
	        // not AQC-ALE protocol
		    if (ascii_38_ok) {
                ber[nr] = berw;
                s += sprintf(s, "%s %c%c%c 0x%04x %d %2x %2x %2x",
                    preamble_types[preamble], a,b,c, w, bf(w,23,21), bf(w,20,14), bf(w,13,7), bf(w,6,0));

                switch (preamble) {
                    case TO:
                        sprintf(to, "%c%c%c", a,b,c);
                        ito = 1;
                        break;
                    case TWAS:
                        sprintf(twas, "%c%c%c", a,b,c);
                        itwas = 1;
                        break;
                    case FROM:
                        sprintf(from, "%c%c%c", a,b,c);
                        ifrom = 1;
                        break;
                    case TIS:
                        sprintf(tis, "%c%c%c", a,b,c);
                        itis = 1;
                        break;
                    default:
                        break;
                }
            } else {
	            // AQC-ALE protocol

                int adf = b20(w);
                u2_t p = bf(w,19,4);
                u2_t dx = bf(w,3,0);
                s += sprintf(s, "$AQC %s 0x%04x p=%d [adf=%d adf_ok=%d] [%d(0x%x) dx=%d]", aqc_preamble_types[preamble],
                    w, bf(w,23,21), adf, adf == b15(p), p, p, dx);
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


// 128: ic0 hc0 NR05 [00:00:28] $AQC [TWAS] 0x63a895 p=3 [adf=0 adf_ok=1] [14985(0x3a89) dx=5] 9='8' 14='B' 25='M' regen=14985(0x3a89)

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
                //s += sprintf(s, " %d='%c' %d='%c' %d='%c' regen=%d(0x%x)", ai,a, bi,b, ci,c, regen, regen);
                s += sprintf(s, " %s %s %s", cdeco(0,a), cdeco(1,b), cdeco(2,c));
                rv = -1;
            }
	    }
	    
	    if (dsp >= DBG || (dsp >= CMDS && icmd))
            //cprintf(CALLS, CALLS, icmd? YELLOW : "", "%d: ic%d NR%02d %s", zs, in_cmd, nr, message);
            //cprintf(CALLS, CALLS, icmd? YELLOW : "", "%5d: >icmd%d caller%d ic%d NR%02d %s", nsym, icmd, caller, in_cmd, nr, message);
            cprintf(CALLS, CALLS, icmd? YELLOW : "", ">icmd%d caller%d ic%d NR%02d %s", icmd, caller, in_cmd, nr, message);
        zs++;

	    return rv;
	}

	void decode_ff_impl::log(char *current, char *current2, int state, int ber, const char *from)
	{
	    struct tm *tm;
	    time_t timestamp;
	    int i;
	    char message[256];
        zs=0;
        bool event = false;
        
	    timestamp = use_UTC? time(NULL) : secs;
	    tm = gmtime(&timestamp);
	    dprintf("LOG from state=%s stage_num=%d from=%s ic%d\n",
	        state_s[state], stage_num, from, in_cmd);
	    if (in_cmd) {
	        cmd_cnt++;
	        in_cmd = binary = 0;
	    }

	    if (state != S_CMD) {
            for (i=0; i < N_CUR; i++) if (current [i] =='@') current [i] = 0;
            for (i=0; i < N_CUR; i++) if (current2[i] =='@') current2[i] = 0;
        }

	    switch (state) {
            case S_TWAS:
                cprintf(CALLS, DBG, CYAN, "[%02d:%02d:%02d] [FRQ %.2f] [Sounding THIS WAS] [From: %s ] [His BER: %d]",
                    tm->tm_hour, tm->tm_min, tm->tm_sec, frequency, current, ber);
                event = true;
                break;

            case S_TIS:
                cprintf(CALLS, DBG, CYAN, "[%02d:%02d:%02d] [FRQ %.2f] [Sounding THIS IS] [From: %s ] [His BER: %d]",
                    tm->tm_hour, tm->tm_min, tm->tm_sec, frequency, current, ber);
                event = true;
                break;

            case S_TO:
                cprintf(CALLS, DBG, CYAN, "[%02d:%02d:%02d] [FRQ %.2f] [To: %s ] [His BER: %d]",
                    tm->tm_hour, tm->tm_min, tm->tm_sec, frequency, current, ber);
                ext_send_msg(rx_chan, true, "EXT call_est_test");
                event = true;
                break;

            case S_CALL_EST:
                if (stage_num == 1) {
                    cprintf(CALLS, DBG, CYAN, "[%02d:%02d:%02d] [FRQ %.2f] [Call] [From: %s ] [To: %s] [His BER: %d]",
                        tm->tm_hour, tm->tm_min, tm->tm_sec, frequency, current, current2, ber);
                } else
                if (stage_num == 2) {
                    cprintf(CALLS, DBG, CYAN, "[%02d:%02d:%02d] [FRQ %.2f] [Call ACK] [From: %s ] [To: %s] [His BER: %d]",
                        tm->tm_hour, tm->tm_min, tm->tm_sec, frequency, current, current2, ber);
                } else
                if (stage_num == 3) {
                    cprintf(CALLS, DBG, CYAN, "[%02d:%02d:%02d] [FRQ %.2f] [Call EST] [From: %s ] [To: %s] [His BER: %d]",
                        tm->tm_hour, tm->tm_min, tm->tm_sec, frequency, current, current2, ber);
		            ext_send_msg(rx_chan, false, "EXT call_est");
                }
                event = true;
		        break;

		    default:
		        dprintf("LOG #### called from unhandled state %s?\n", state_s[state]);
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

            if (cp->cmd == AMD) {
                // AMD - automatic message display
                s += sprintf(s, "AMD: \"%s", cp->cmd_s);
                // stuffed with space char, so trim trailing spaces
                char *e = s + strlen(s) -1;
                while (*e == ' ' || *e == '\r' || *e == '\n') e--;
                *(e+1) = '"';
                *(e+2) = '\0';
                cond_d = MSGS;
                event = true;
            } else

            if (cp->cmd == DTM) {
                // DTM - data text message
                s += sprintf(s, "DTM: \"%s", cp->cmd_s);
                // stuffed with space char, so trim trailing spaces
                char *e = s + strlen(s) - 1;
                while (*e == ' ' || *e == '\r' || *e == '\n') e--;
                *(e+1) = '"';
                *(e+2) = '\0';
                cond_d = MSGS;
                event = true;
            } else

            if (cp->cmd == LQA) {
                // LQA
                s += sprintf(s, "LQA: reply=%u MP=%u SINAD=%u BER=%u", b13(w2), bf(w2,12,10), bf(w2,9,5), bf(w2,4,0));
                event = true;
            } else

            if (cp->cmd == FRQ) {
                s += sprintf(s, "freq: con=%d b4(0)=%d %d_%d_%d_%d_%d.%d_%d",
                    bf(w,13,8), b20(w2), bf(w2,19,16), bf(w2,15,12), bf(w2,11,8), bf(w2,7,4), bf(w2,3,0), bf(w,7,4), bf(w,3,0));
                event = true;
            } else
            
            if (a == 'd') {     // data text message
                u1_t kd = bf(w,13,10);
                u2_t dc = bf(w,9,0);
                s += sprintf(s, "DTM kd=0x%x dc=0x%x", kd, dc); 
                event = true;
            } else

            if (cp->cmd == CRC) {
                s += sprintf(s, "CRC");
            } else

            {   // OTH
                s += sprintf(s, "other: %s %s %s", cdeco(0,a), cdeco(1,b), cdeco(2,c));
            }
            
            cprintf(cond_d, CALLS, MAGENTA, "[%02d:%02d:%02d] [FRQ %.2f] [CMD] [%s] [His BER: %d]",
                tm->tm_hour, tm->tm_min, tm->tm_sec, frequency, message, ber);
	    }

        if (event) ext_send_msg(rx_chan, false, "EXT event");
        cmd_cnt = 0;
        state = S_START;
        memset(current,'\0', N_CUR);
	}

	void decode_ff_impl::do_modem(float *sample, int length)
	{
	    int i,j,k,n, max_offset=0;
	    double new_sample;
	    double old_sample;
	    double temp_real;
	    double temp_imag;
	    double max_magnitude = 0;
	    char s1[4], s2[4];
	    int bestber = 26;
	    int ito2, itwas2, ifrom2, itis2, irep2, idata2, icmd2;
	    char to2[4] = {0,0,0,0}, twas2[4] = {0,0,0,0}, from2[4] = {0,0,0,0}, tis2[4] = {0,0,0,0};
	    char data2[4] = {0,0,0,0}, rep2[4] = {0,0,0,0};
	    char cmd2[4] = {0,0,0,0};
	    int temppos = 0;

        //printf("%d ", length); fflush(stdout);
	    if (length == 0) {
            log(current, current2, state, lastber, "EOF");
            return;
        }

	    for (i=0; i < length; i++) {

            new_sample = sample[i];

            decode_ff_array_dim(fft_history_offset, FFT_SIZE);
            old_sample = fft_history[fft_history_offset];
            fft_history[fft_history_offset] = new_sample;

            for (n=0; n < FFT_SIZE/2; n++) {
                temp_real       = fft_out[n].real - old_sample + new_sample;
                temp_imag       = fft_out[n].imag;
                fft_out[n].real = (temp_real * fft_cs_twiddle[n]) - (temp_imag * fft_ss_twiddle[n]);
                fft_out[n].imag = (temp_real * fft_ss_twiddle[n]) + (temp_imag * fft_cs_twiddle[n]);
                fft_mag[n]      = sqrt((fft_out[n].real * fft_out[n].real) + (fft_out[n].imag * fft_out[n].imag)) * 5;
            }

            max_magnitude = 0;
            for (n = 1; n <= 27; n++) {
                if ((fft_mag[n] > max_magnitude)) {
                    max_magnitude = fft_mag[n];
                    max_offset    = n;
                }
            }

            decode_ff_array_dim(sample_count, FFT_SIZE);
            for (n=0; n < NR; n++) {
                mag_sum[n][sample_count] += max_magnitude;
                //mag_history[n][sample_count[n]][mag_history_offset[n]] = max_magnitude;
            }

            for (j=0; j < NR; j++) {
                if (word_sync[j] == NOT_WORD_SYNC) {
                    max_magnitude = 0;
                    for (n=0; n < FFT_SIZE; n++) {
                        if (mag_sum[j][n] > max_magnitude) {
                            max_magnitude = mag_sum[j][n];
                            last_sync_position[j] = n;
                        }
                    }
                }
            }

            decode_ff_array_dim(max_offset, 33);
            decode_ff_array_dim(sample_count, FFT_SIZE);
            for (n=0; n < NR; n++) {
                if (sample_count == last_sync_position[n]) {
                    last_symbol[n] = g_symbol_lookup[n][max_offset];
                }
            }

            ito = ifrom = itis = itwas = irep = idata = icmd = 0;
            bestber = 26;
            ito2 = ifrom2 = itis2 = itwas2 = irep2 = idata2 = icmd2 = 0;

            if ((length & 0xf) == 0)
                NextTask("ale_2g_task");

            if (sample_count == 0) {
                int sym_rv0_cnt = 0;
                activity_cnt = 0;

                for (n=0; n < NR; n++) {
                    int sym_rv = modem_new_symbol(last_symbol[n], n);
                    if (sym_rv) { sym_rv0_cnt++; activity_cnt++; }

                    bool isBestBER = (ber[n] < bestber);
                    bool isBestPosition = ((bestpos == 0) || (bestpos == n));
                    bool best = (isBestBER && isBestPosition);
                    bool word = (ito || ifrom || itis || itwas || irep || idata || icmd);

                    if (gdebug >= 2)
                    printf("%5d: SYM%d NR%2d sym_rv%d // ber[n]|%2d < bestber|%2d %s // bestpos=%2d|0|%2d %s // %s %s\n",
                        nsym, last_symbol[n], n, sym_rv,
                        ber[n], bestber, isBestBER? (CYAN "Y" NORM) : "N",
                        bestpos, n, isBestPosition? (CYAN "Y" NORM) : "N",
                        (sym_rv)? (GREEN "WORD" NORM) : "",
                        best? (MAGENTA "BEST" NORM) : "");

                    #define ORIG_BEST_BER
                    #ifdef ORIG_BEST_BER
                        if (best) {
                            decode_ff_array_dim(inew, NR);
                            bestber = ber[inew];
                            temppos = n;
                        }
                    #else
                        if (isBestBER) {
                            decode_ff_array_dim(inew, NR);
                            bestber = ber[inew];
                        }
                        if (best) {
                            temppos = n;
                        }
                    #endif
                    
                    nsym++;
                }
                
                // KiwiSDR fix: require two good symbols in NR loop before advancing iXXX state
                
                if (sym_rv0_cnt >= 2) {
                    ito2 = ito;     memcpy(to2, to, 3);
                    ifrom2 = ifrom; memcpy(from2, from, 3);
                    itis2 = itis;   memcpy(tis2, tis, 3);
                    itwas2 = itwas; memcpy(twas2, twas, 3);
                    irep2 = irep;   memcpy(rep2, rep, 3);
                    idata2 = idata; memcpy(data2, data, 3);
                    
                    icmd2 = icmd;   memcpy(cmd2, cmd, 3);
                    if (gdebug >= 1 && icmd) printf("%sicmd2=%d icmd=%d cmd2=%s cmd=%s%s\n", RED, icmd2, icmd, cmd2, cmd, NORM);
                }
                
                if (activity_cnt >= 2 && !active) {
                    ext_send_msg(rx_chan, false, "EXT active=%.2f", frequency);
                    //real_printf("#%d#", slot(frequency)); fflush(stdout);
                    //ext_send_snd_msg(rx_chan, true, "MSG audio_flags2=active:%.2f", frequency);
                    active = 1;
                }
            }
        
            if (temppos != 0) bestpos = temppos;
            inew = bestpos;
            decode_ff_array_dim(inew, NR);

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
                strcpy(current, twas2);
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
                    strcat(s, data2);
                }
                if (in_cmd && binary) {
                    dprintf("Data: %s ic%d a64=%d %02x %02x %02x %02x %02x %02x ...\n",
                        data, in_cmd, ascii_64_ok, s[0], s[1], s[2], s[3], s[4], s[5]);
                } else {
                    dprintf("Data: %s ic%d a64=%d <%s> %lu\n",
                        data, in_cmd, ascii_64_ok, s, strlen(s));
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
                    dprintf("Data: %s ic%d a64=%d <%s> %lu\n",
                        rep, in_cmd, ascii_64_ok, s, strlen(s));
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
                    switch (a) {
                    
                        case 'd':
                            cp->cmd = DTM;
                            cp->cmd_s[0] = '\0';    // prepare for subsequent appending by data/rep
                            break;
                    
                        case 'a':
                            cp->cmd = LQA;
                            cp->data = w;
                            binary = 1;
                            break;
                    
                        case 'f':
                            cp->cmd = FRQ;
                            cp->data = w;
                            binary = 1;
                            break;
                    
                        case 'x':
                            cp->cmd = CRC;
                            cp->data = w;
                            binary = 1;
                            break;
                    
                        default:
                            #if 1
                                cp->cmd = AMD;      // assume noise during AMD
                            #else
                                cp->cmd = OTH;
                                cp->data = w;
                                binary = 1;
                            #endif
                            break;
                    }
                }
                
                dprintf("CMD: %s\n", CMD_types[cp->cmd]);
                state_count = 0;
                lastber = bestber;
            }

            state_count++;

            // after 2 seconds of no state changes output log entry
            if ((state_count == 16000) && state) {
                log(current, current2, state, lastber, "2_SEC");
                state = S_START;
                state_count = 0;
                active = 0;
                ext_send_msg(rx_chan, false, "EXT active=0");
                //real_printf("///"); fflush(stdout);
                //ext_send_snd_msg(rx_chan, true, "MSG audio_flags2=active:0");
                
                #if 0
                    printf("#### 2 SEC RESET ####\n");
                    modem_reset();
                #endif
            }

            fft_history_offset = (fft_history_offset + 1) % FFT_SIZE;
            sample_count = (sample_count + 1) % MOD_64; 

            if (sample_count == 0) {
                mag_history_offset = (mag_history_offset + 1) % SYMBOLS_PER_WORD;

                if (mag_history_offset == 0) {
                    for (n=0; n < NR; n++) {
                        for (j=0; j < FFT_SIZE; j++) {
                            mag_sum[n][j] = 0;
                        }
                    }
                }
            }
	    }

        timer_samps += length;
        secs = timer_samps / SAMP_RATE_SPS;   // keep track of realtime in file (in seconds)
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
	        static FILE *fp;
	        if (fp == NULL) {
                printf("file #%d = %s\n", fileno, sa_fn[fileno]);
                fp = fopen(sa_fn[fileno], "r");
                if (fp == NULL) { printf("fopen failed: %s\n", sa_fn[fileno]); exit(-1); }
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
	}

	void decode_ff_impl::modem_init(int rx_chan, bool use_new_resampler, float f_srate, int n_samps, bool use_UTC)
	{
	    int i,j,k;
	    
	    this->rx_chan = rx_chan;
	    this->use_UTC = use_UTC;
	    timer_samps = 0;
	    frequency = 0;
	    activity_cnt = active = 0;
	    
        #ifdef STANDALONE_TEST
        #else
	        //printf("### modem_init use_new_resampler=%d srate=%f dsp=%d\n", use_new_resampler, f_srate, dsp);
	    #endif

	    in_cmd = binary = 0;
	    cmd_cnt = 0;

	    bestpos = 0;
	    lastber = 26;

	    for (i=0; i < FFT_SIZE; i++) {
            fft_cs_twiddle[i] = cos((-2.0*PI * i) / FFT_SIZE);
            fft_ss_twiddle[i] = sin((-2.0*PI * i) / FFT_SIZE);
            fft_history[i]    = 0;
	    }
	    fft_history_offset = 0;
	    
	    stage_num = 0; state_count = 0; state = S_START;
	    sample_count = 0;
	    nsym = 0;
	    mag_history_offset = 0;

	    for (i=0; i < NR; i++) {
            word_sync[i] = NOT_WORD_SYNC;
            last_symbol[i] = 0;
            last_sync_position[i] = 0;
            ber[i] = 26;

            for (j=0; j < FFT_SIZE; j++) {
                mag_sum[i][j] = 0;
                fft_mag[j] = fft_out[j].real = fft_out[j].imag = 0;
                //for (k=0; k < SYMBOLS_PER_WORD; k++) mag_history[i][j][k] = 0;
            }
            for (k=0; k < VOTE_ARRAY_LENGTH; k++) bits[i][k] = 0;
            input_buffer_pos[i] = 0;
            word_sync_position[i] = 0;
	    }
	    
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

	void decode_ff_impl::modem_reset()
	{
	    int i,j,k;
	    
        //printf("### modem_reset\n");
    
	    activity_cnt = active = 0;

    // no diff at all
    #if 1
	    in_cmd = binary = 0;
	    cmd_cnt = 0;

	    bestpos = 0;
	    lastber = 26;
	#endif

    // ber diff, diff again when comb w/ above
    #if 1
	    for (i=0; i < FFT_SIZE; i++) {
            fft_cs_twiddle[i] = cos((-2.0*PI * i) / FFT_SIZE);
            fft_ss_twiddle[i] = sin((-2.0*PI * i) / FFT_SIZE);
            fft_history[i]    = 0;
	    }
	    fft_history_offset = 0;
	#endif
	
	// msg errs!
    #if 1
	    stage_num = 0; state_count = 0; state = S_START;
	    sample_count = 0;
	    nsym = 0;
	    mag_history_offset = 0;
	#endif

    // no msg at all!
    #if 1
	    for (i=0; i < NR; i++) {
            word_sync[i] = NOT_WORD_SYNC;
            last_symbol[i] = 0;
            last_sync_position[i] = 0;
            ber[i] = 26;

            for (j=0; j < FFT_SIZE; j++) {
                mag_sum[i][j] = 0;
                fft_mag[j] = fft_out[j].real = fft_out[j].imag = 0;
                //for (k=0; k < SYMBOLS_PER_WORD; k++) mag_history[i][j][k] = 0;
            }
            for (k=0; k < VOTE_ARRAY_LENGTH; k++) bits[i][k] = 0;
            input_buffer_pos[i] = 0;
            word_sync_position[i] = 0;
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
    #define ARGL(v) if (ai+1 < argc) (v) = strtol(argv[++ai], 0, 0);
    
    int display = CALLS;
    int fileno = 0, offset = -1, repeat = 1;
    bool use_new_resampler = false;     // there is never a resampler when running standalone (use an 8k file)

    for (int ai = 1; ai < argc; ) {
        if (ARG("-f") || ARG("--file")) ARGL(fileno);
        if (ARG("-o") || ARG("--offset")) ARGL(offset);
        if (ARG("-r") || ARG("--repeat")) ARGL(repeat);

        // display
        if (ARG("-m") || ARG("--msgs")) display = MSGS;
        if (ARG("-c") || ARG("--cmds")) display = CMDS;
        if (ARG("-d") || ARG("--debug")) display = DBG;
        ai++;
    }
    
    int ars = ARRAY_LEN(sa_fn);
    if (fileno >= ars) {
        printf("range error: --file 0..%d\n", ars-1);
        exit(-1);
    }
    
    ale::decode_ff_impl decode;
    decode.set_display(display);
    decode.modem_init(0, use_new_resampler, 0, N_SAMP_BUF, false);   // false = use file realtime instead of current UTC

    if (offset >= 0) {
	    for (i = 0; i < repeat; i++) {
            decode.run_standalone(fileno, offset);
        }
    } else {
	    for (offset = 0; offset <= 32; offset += 4) {
            decode.run_standalone(fileno, offset);
            //decode.modem_reset();
        }
    }
}
#endif

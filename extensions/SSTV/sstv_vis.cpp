/*
 * slowrx - an SSTV decoder
 * * * * * * * * * * * * * *
 * 
 * Copyright (c) 2007-2013, Oona Räisänen (OH2EIQ [at] sral.fi)
 */

//#define DEBUG
#include "sstv.h"

/* 
 *
 * Detect VIS & frequency shift
 *
 * Each bit (start/data/stop) lasts 30 ms
 * 1900 Hz leader is 300  ms
 *
 */

u1_t sstv_get_vis(sstv_chan_t *e)
{
    int       selmode;
    int       VIS = 0, VIS2 = 0, Parity = 0, Parity2 = 0;
    u4_t      HedrPtr = 0;
    const int FFTLen = 2048;
    int       i=0, j=0, k=0;
    u4_t      MaxBin = 0;

    #define N_POWER 2048
    #define N_HDRBUF 100
    #define N_TONE 100
    SSTV_REAL Power[N_POWER] = {0}, HeaderBuf[N_HDRBUF] = {0}, tone[N_TONE] = {0}, Hann[SSTV_MS_2_MAX_SAMPS(20)] = {0};
    bool      gotvis = false;
    u1_t      Bit[16] = {0}, ParityBit = 0, ParityBit2 = 0;
    int       nbits = 0;
    u1_t      mode = UNKNOWN;
    int       tone_win = 45;    // 450 ms 8-bit VIS
    
    memset(e->fft.in2k, 0, sizeof(SSTV_REAL) * FFTLen);
    
    // Create 20ms Hann window
    int samps_20ms = SSTV_MS_2_SAMPS(20);
    for (i = 0; i < samps_20ms; i++) Hann[i] = 0.5 * (1 - SSTV_MCOS( (2 * M_PI * (SSTV_REAL)i) / (samps_20ms-1) ));
    
    printf("SSTV: Waiting for header\n");
    ext_send_msg_encoded(e->rx_chan, false, "EXT", "mode_name", "waiting for signal");
    
    int samps_10ms = SSTV_MS_2_SAMPS(10);
    
    while (true) {
    
        if (e->reset) return UNKNOWN;

        // Read 10 ms from sound card
        sstv_pcm_read(e, samps_10ms);
        
        // Apply Hann window
        for (i = 0; i < samps_20ms; i++)
            e->fft.in2k[i] = e->pcm.Buffer[e->pcm.WindowPtr + i - samps_10ms] / 32768.0 * Hann[i];
        
        // FFT of last 20 ms
        SSTV_FFTW_EXECUTE(e->fft.Plan2048);
        
        // Find the bin with most power
        MaxBin = 0;
        for (i = 0; i <= GET_BIN(6000, FFTLen); i++) {
            assert_array_dim(i, N_POWER);
            Power[i] = POWER(e->fft.out2k[i]);
            assert_array_dim(MaxBin, N_POWER);
            if ((i >= GET_BIN(500,FFTLen) && i < GET_BIN(3300,FFTLen)) && (MaxBin == 0 || Power[i] > Power[MaxBin]))
                MaxBin = i;
        }
        
        // Find the peak frequency by Gaussian interpolation
        assert_array_dim(MaxBin, N_POWER);
        SSTV_REAL pwr_mb = Power[MaxBin];
        assert_array_dim(MaxBin-1, N_POWER);
        SSTV_REAL pwr_mo = Power[MaxBin-1];
        assert_array_dim(MaxBin+1, N_POWER);
        SSTV_REAL pwr_po = Power[MaxBin+1];
        assert_array_dim(HedrPtr, N_HDRBUF);

        if (MaxBin > GET_BIN(500, FFTLen) && MaxBin < GET_BIN(3300, FFTLen) && pwr_mb > 0 && pwr_po > 0 && pwr_mo > 0) {
            HeaderBuf[HedrPtr] = MaxBin + (SSTV_MLOG(pwr_po / pwr_mo )) / (2 * SSTV_MLOG( SSTV_MPOW(pwr_mb, 2) / (pwr_po * pwr_mo)));
        } else {
            assert_array_dim((HedrPtr-1) % N_HDRBUF, N_HDRBUF);
            HeaderBuf[HedrPtr] = HeaderBuf[(HedrPtr-1) % N_HDRBUF];
        }
        
        // In Hertz
        HeaderBuf[HedrPtr] = HeaderBuf[HedrPtr] / FFTLen * sstv.nom_rate;
        
        // Header buffer holds (N_HDRBUF * 10) msec
        HedrPtr = (HedrPtr + 1) % N_HDRBUF;

        // Frequencies in the last (tone_win * 10) msec
        int hp;
        for (i = tone_win-1, hp = HedrPtr-1; i >= 0; i--) {
            if (hp < 0) hp = N_HDRBUF-1;
            assert_array_dim(hp, N_HDRBUF);
            assert_array_dim(i, N_TONE);
            tone[i] = HeaderBuf[hp];
            hp--;
        }
        
        // Is there a pattern that looks like (the end of) a calibration header + VIS?
        // Tolerance ±25 Hz
        e->pic.HeaderShift = 0;
        gotvis = false;
        
        for (i = 0; i < 3; i++) {
            if (e->pic.HeaderShift != 0) break;
            for (j = 0; j < 3; j++) {   // 30 ms = 10 ms * 3
                assert_array_dim(22*3+i, N_TONE);
                if ( (tone[1*3+i]  > tone[0+j] - 25  && tone[1*3+i]  < tone[0+j] + 25)  && // 1900 Hz leader
                     (tone[2*3+i]  > tone[0+j] - 25  && tone[2*3+i]  < tone[0+j] + 25)  && // 1900 Hz leader
                     (tone[3*3+i]  > tone[0+j] - 25  && tone[3*3+i]  < tone[0+j] + 25)  && // 1900 Hz leader
                     (tone[4*3+i]  > tone[0+j] - 25  && tone[4*3+i]  < tone[0+j] + 25)  && // 1900 Hz leader
                     (tone[5*3+i]  > tone[0+j] - 725 && tone[5*3+i]  < tone[0+j] - 675)    // 1200 Hz start bit
                   ) {
        
                    // Attempt to read VIS
        
                    for (k = 0; k < 16; k++) {
                        assert_array_dim(6*3+i+3*k, N_TONE);
                        if (tone[6*3+i+3*k] > tone[0+j] - 625 && tone[6*3+i+3*k] < tone[0+j] - 575) { // 1300 Hz
                            Bit[k] = 0;
                            //real_printf("%d:0 ", k); fflush(stdout);
                        } else
                        if (tone[6*3+i+3*k] > tone[0+j] - 825 && tone[6*3+i+3*k] < tone[0+j] - 775) { // 1100 Hz
                            Bit[k] = 1;
                            //real_printf("%d:1 ", k); fflush(stdout);
                        } else {    // erroneous bit
                            //real_printf("%d:X ", k); fflush(stdout);
                            break;
                        }
                    }
                    
                    // if a stop bit in the k == 8 bit position then this is a standard VIS-8
                    // start d0 d1 d2 d3 d4 d5 d6 d7 stop
                    //                            k7   k8
                    if (k == 8) {
                        if (tone[14*3+i] > tone[0+j] - 725 && tone[14*3+i] < tone[0+j] - 675) { // 1200 Hz stop bit
                            printf("SSTV: STOP bit => VIS-8\n");
                            nbits = 8;
                            gotvis = true;
                        }
                    }

                    // if a data bit in the d8 bit position (loop break at k == 9) then this might be an extended VIS-16
                    // commit to making the capture window larger
                    // start d0 d1 d2 d3 d4 d5 d6 d7 | d8  x
                    //                            k7 | k8 k9
                    if (k == 9) {
                        printf("SSTV: tone_win => 70\n");
                        tone_win = 70;
                        break;
                    }

                    // if a stop bit in the k == 16 bit position then this is an extended VIS-16
                    // start d0 d1 d2 d3 d4 d5 d6 d7 d8 d9 d10 d11 d12 d13 d14 d15 stop
                    //                                                         k15  k16
                    if (k == 16) {
                        if (tone[22*3+i] > tone[0+j] - 725 && tone[22*3+i] < tone[0+j] - 675) { // 1200 Hz stop bit
                            printf("SSTV: STOP bit => VIS-16\n");
                            nbits = 16;
                            gotvis = true;
                        }
                    }                    

                    if (gotvis) {
                        e->pic.HeaderShift = tone[0+j] - 1900;
                
                        VIS = Bit[0] + (Bit[1] << 1) + (Bit[2] << 2) + (Bit[3] << 3) + (Bit[4] << 4) + (Bit[5] << 5) + (Bit[6] << 6);
                        assert_array_dim(VIS, 0x80);
                        ParityBit = Bit[7];
                        Parity = Bit[0] ^ Bit[1] ^ Bit[2] ^ Bit[3] ^ Bit[4] ^ Bit[5] ^ Bit[6];

                        if (nbits == 16) {
                            VIS2 = Bit[8] + (Bit[9] << 1) + (Bit[10] << 2) + (Bit[11] << 3) + (Bit[12] << 4) + (Bit[13] << 5) + (Bit[14] << 6);
                            assert_array_dim(VIS2, 0x80);
                            ParityBit2 = Bit[15];
                            Parity2 = Parity ^ Bit[8] ^ Bit[9] ^ Bit[10] ^ Bit[11] ^ Bit[12] ^ Bit[13] ^ Bit[14];
                        }
                
                        if (nbits == 8)
                            printf("SSTV: VIS %d (0x%02x) @ %+d Hz\n", VIS, VIS, e->pic.HeaderShift);
                        else
                            printf("SSTV: VISX %d|%d (0x%02x|0x%02x) @ %+d Hz\n", VIS, VIS2, VIS, VIS2, e->pic.HeaderShift);
                
                        if (VISmap[VIS] == R12BW) Parity = !Parity;
                
                        if (nbits == 8 && Parity != ParityBit) {
                            printf("SSTV: Parity fail\n");
                            gotvis = false;
                        } else
                        if (nbits == 16 && Parity2 != ParityBit2) {
                            printf("SSTV: Parity2 fail\n");
                            gotvis = false;
                        } else
                        if (VISmap[VIS] == UNKNOWN) {
                            printf("SSTV: Unknown VIS\n");
                            ext_send_msg_encoded(e->rx_chan, false, "EXT", "status", "unknown VIS-8 code 0x%02x", VIS);
                            gotvis = false;
                        } else
                        if (nbits == 16 && (VISmap[VIS] != VISX || VISXmap[VIS2] == UNKNOWN)) {
                            ext_send_msg_encoded(e->rx_chan, false, "EXT", "status", "unknown VIS-16 code 0x%02x%02x", VIS, VIS2);
                            printf("SSTV: Unknown VISX\n");
                            gotvis = false;
                        } else {
                            ModeSpec_t *m;
                            if (nbits == 8) {
                                mode = VISmap[VIS];
                                m = &ModeSpec[mode];
                            } else {
                                mode = VISXmap[VIS2];
                                m = &ModeSpec[mode];
                            }
                            bool skip = (e->mmsstv_only && (mode < MR73 || mode > MP175));
                            printf("SSTV: \"%s\"%s\n", m->Name, skip? " (skipped)":"");
                            if (skip) {
                                ext_send_msg_encoded(e->rx_chan, false, "EXT", "status", "skipping %s", m->Name);
                                return UNKNOWN;
                            }
                            if (m->unsupported) {
                                printf("SSTV: mode not supported\n");
                                ext_send_msg_encoded(e->rx_chan, false, "EXT", "status", "mode not supported: %s", m->Name);
                                return UNKNOWN;
                            }
                            if (m->ImgWidth != 320) {
                                printf("SSTV: mode width %d not supported\n", m->ImgWidth);
                                ext_send_msg_encoded(e->rx_chan, false, "EXT", "status", "mode width %d not supported: %s", m->ImgWidth, m->Name);
                                return UNKNOWN;
                            }
                            ext_send_msg_encoded(e->rx_chan, false, "EXT", "mode_name", "%s", m->Name);
                            ext_send_msg_encoded(e->rx_chan, false, "EXT", "fsk_id", "");
                            break;
                        }
                    }
                }
            }
        }
        
        if (gotvis) break;
        
        e->pcm.WindowPtr += samps_10ms;
        NextTask("sstv get_vis");
    }

    // Skip rest of stop bit
    int stop_bit_samps = SSTV_MS_2_SAMPS((e->test && mode == S2)? 30:20);
    sstv_pcm_read(e, stop_bit_samps);
    e->pcm.WindowPtr += stop_bit_samps;

    if (mode == UNKNOWN)
        printf("SSTV: No VIS found\n");
    return mode;
}

/*
 * slowrx - an SSTV decoder
 * * * * * * * * * * * * * *
 * 
 * Copyright (c) 2007-2013, Oona Räisänen (OH2EIQ [at] sral.fi)
 */

#include "sstv.h"

/* 
 *
 * Detect VIS & frequency shift
 *
 * Each bit lasts 30 ms
 *
 */

u1_t sstv_get_vis(sstv_chan_t *e)
{
    int       selmode;
    int       VIS = 0, Parity = 0;
    u4_t      HedrPtr = 0;
    const int FFTLen = 2048;
    u4_t      i=0, j=0, k=0, MaxBin = 0;
    #define N_POWER 2048
    #define N_HDRBUF 100
    #define N_TONE 100
    SSTV_REAL Power[N_POWER] = {0}, HeaderBuf[N_HDRBUF] = {0}, tone[N_TONE] = {0}, Hann[SSTV_MS_2_MAX_SAMPS(20)] = {0};
    bool      gotvis = false;
    u1_t      Bit[8] = {0}, ParityBit = 0;
    
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
            assert_array_dim((HedrPtr-1) % 45, N_HDRBUF);
            HeaderBuf[HedrPtr] = HeaderBuf[(HedrPtr-1) % 45];
        }
        
        // In Hertz
        HeaderBuf[HedrPtr] = HeaderBuf[HedrPtr] / FFTLen * sstv.nom_rate;
        
        // Header buffer holds 45 * 10 msec = 450 msec
        HedrPtr = (HedrPtr + 1) % 45;
        
        // Frequencies in the last 450 msec
        for (i = 0; i < 45; i++) {
            assert_array_dim((HedrPtr + i) % 45, N_HDRBUF);
            assert_array_dim(i, N_TONE);
            tone[i] = HeaderBuf[(HedrPtr + i) % 45];
        }
        
        // Is there a pattern that looks like (the end of) a calibration header + VIS?
        // Tolerance ±25 Hz
        e->pic.HeaderShift = 0;
        gotvis = false;
        
        for (i = 0; i < 3; i++) {
            if (e->pic.HeaderShift != 0) break;
            for (j = 0; j < 3; j++) {
                assert_array_dim(14*3+i, N_TONE);
                if ( (tone[1*3+i]  > tone[0+j] - 25  && tone[1*3+i]  < tone[0+j] + 25)  && // 1900 Hz leader
                     (tone[2*3+i]  > tone[0+j] - 25  && tone[2*3+i]  < tone[0+j] + 25)  && // 1900 Hz leader
                     (tone[3*3+i]  > tone[0+j] - 25  && tone[3*3+i]  < tone[0+j] + 25)  && // 1900 Hz leader
                     (tone[4*3+i]  > tone[0+j] - 25  && tone[4*3+i]  < tone[0+j] + 25)  && // 1900 Hz leader
                     (tone[5*3+i]  > tone[0+j] - 725 && tone[5*3+i]  < tone[0+j] - 675) && // 1200 Hz start bit
                                                                                           // ...8 VIS bits...
                     (tone[14*3+i] > tone[0+j] - 725 && tone[14*3+i] < tone[0+j] - 675)    // 1200 Hz stop bit
                   ) {
        
                    // Attempt to read VIS
        
                    gotvis = true;
                    
                    for (k = 0; k < 8; k++) {
                        assert_array_dim(6*3+i+3*k, N_TONE);
                        if (tone[6*3+i+3*k] > tone[0+j] - 625 && tone[6*3+i+3*k] < tone[0+j] - 575)
                            Bit[k] = 0;
                        else
                        if (tone[6*3+i+3*k] > tone[0+j] - 825 && tone[6*3+i+3*k] < tone[0+j] - 775)
                            Bit[k] = 1;
                        else { // erroneous bit
                            gotvis = false;
                            break;
                        }
                    }
                    
                    if (gotvis) {
                        e->pic.HeaderShift = tone[0+j] - 1900;
                
                        VIS = Bit[0] + (Bit[1] << 1) + (Bit[2] << 2) + (Bit[3] << 3) + (Bit[4] << 4) + (Bit[5] << 5) + (Bit[6] << 6);
                        ParityBit = Bit[7];
                
                        printf("SSTV: VIS %d (0x%02x) @ %+d Hz\n", VIS, VIS, e->pic.HeaderShift);
                
                        Parity = Bit[0] ^ Bit[1] ^ Bit[2] ^ Bit[3] ^ Bit[4] ^ Bit[5] ^ Bit[6];
                
                        assert_array_dim(VIS, 0x80);
                        if (VISmap[VIS] == R12BW) Parity = !Parity;
                
                        if (Parity != ParityBit) {
                            printf("SSTV: Parity fail\n");
                            gotvis = false;
                        } else if (VISmap[VIS] == UNKNOWN) {
                            printf("SSTV: Unknown VIS\n");
                            gotvis = false;
                        } else {
                            ModeSpec_t  *m = &ModeSpec[VISmap[VIS]];
                            printf("SSTV: \"%s\"\n", m->Name);
                            if (m->ImgWidth != 320) {
                                printf("SSTV: mode not supported\n");
                                ext_send_msg_encoded(e->rx_chan, false, "EXT", "status", "mode not supported: %s", m->Name);
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
    int stop_bit_samps = SSTV_MS_2_SAMPS(e->test? 30:20);
    sstv_pcm_read(e, stop_bit_samps);
    e->pcm.WindowPtr += stop_bit_samps;

    if (VISmap[VIS] != UNKNOWN)
        return VISmap[VIS];
    else
        printf("SSTV: No VIS found\n");
    return UNKNOWN;
}

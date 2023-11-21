/*
 * slowrx - an SSTV decoder
 * * * * * * * * * * * * * *
 * 
 * Copyright (c) 2007-2013, Oona Räisänen (OH2EIQ [at] sral.fi)
 */

#include "types.h"
#include "mem.h"
#include "sstv.h"

void sstv_video_once(sstv_chan_t *e)
{
    e->image = (image_t *) kiwi_imalloc("sstv_video_once", sizeof(image_t));
    assert(e->image != NULL);
}

void sstv_video_init(sstv_chan_t *e, SSTV_REAL rate, u1_t mode)
{
    e->pic.Rate = rate;
    e->pic.Mode = mode;
    ModeSpec_t *m = &ModeSpec[mode];
    e->pic.modespec = m;

    SSTV_REAL spp = rate * m->LineTime / m->ImgWidth;
    //e->fm_sample_interval = debug_v? debug_v : ((int) (spp * 3/4));
    //printf("SSTV: spp=%.1f fm_sample_interval=%d %s\n", spp, e->fm_sample_interval, debug_v? "(v=)":"");
    e->fm_sample_interval = (int) (spp * 3/4);
    printf("SSTV: sstv_video_init rate=%.3f spp=%.1f fm_sample_interval=%d\n", rate, spp, e->fm_sample_interval);
    
    // Allocate space for cached Lum
    e->StoredLum_len = (int) ((m->LineTime * m->NumLines + 1) * sstv.nom_rate);
    e->StoredLum = (u1_t *) kiwi_icalloc("sstv_video_init", e->StoredLum_len, sizeof(u1_t));
    assert(e->StoredLum != NULL);

    // Allocate space for sync signal
    // m->NumLines+1 to handle indicies beyond nominal range due to Rate/nom_rate adjustment in sstv_sync_find()
    e->HasSync_len = (int) (m->LineTime * (m->NumLines+1) / (13.0 / sstv.nom_rate));
    e->HasSync = (bool *) kiwi_icalloc("sstv_video_init", e->HasSync_len, sizeof(bool));
    assert(e->HasSync != NULL);
}

void sstv_video_done(sstv_chan_t *e)
{
    //printf("SSTV: sstv_video_done\n");
    kiwi_ifree(e->StoredLum, "sstv_video_done"); e->StoredLum = NULL;
    kiwi_ifree(e->HasSync, "sstv_video_done"); e->HasSync = NULL;
    kiwi_ifree(e->PixelGrid, "sstv_video_done"); e->PixelGrid = NULL;
    kiwi_ifree(e->pixels, "sstv_video_done"); e->pixels = NULL;
}


/* Demodulate the video signal & store all kinds of stuff for later stages
 *  Mode:      M1, M2, S1, S2, R72, R36...
 *  Rate:      exact sampling rate used
 *  Skip:      number of PCM samples to skip at the beginning (for sync phase adjustment)
 *  Redraw:    false = Apply windowing and FFT to the signal, true = Redraw from cached FFT data
 *  returns:   true when finished, false when aborted
 */
bool sstv_video_get(sstv_chan_t *e, const char *from, int Skip, bool Redraw)
{

    u4_t        MaxBin = 0;
    u4_t        VideoPlusNoiseBins=0, ReceiverBins=0, NoiseOnlyBins=0;
    int         n=0;
    u4_t        SyncSampleNum;
    int         i=0, j=0;
    const int   FFTLen = 1024;
    u4_t        WinLength=0;
    int         SyncTargetBin;
    int         SampleNum, Length, NumChans;
    int         x = 0, y = 0, tx=0, k=0;
    SSTV_REAL   Freq = 0, InterpFreq = 0;
    //SSTV_REAL   PrevFreq = 0;
    int         NextSNRtime = 0, NextSyncTime = 0;
    SSTV_REAL   Praw, Psync;
    SSTV_REAL   Power[1024] = {0};
    SSTV_REAL   Pvideo_plus_noise=0, Pnoise_only=0, Pnoise=0, Psignal=0;
    SSTV_REAL   SNR = 0;
    SSTV_REAL   ChanStart[4] = {0}, ChanLen[4] = {0};
    u1_t        Channel = 0, WinIdx = 0;

    u1_t        Mode = e->pic.Mode;
    ModeSpec_t  *m = &ModeSpec[Mode];
    SSTV_REAL   Rate = e->pic.Rate;
    
    printf("SSTV: sstv_video_get %s %.3f Hz (hdr %+d), skip %d smp (%.1f ms)\n",
        from, Rate, e->pic.HeaderShift, Skip, Skip * (1e3 / Rate));

    memset(e->image, 0, sizeof(image_t));
    
    // too big to keep on stack now that stacks are smaller
    #define HANN_J 7
    #define HANN_I 1024
    #define Hann(j, i) HannA[(j) * HANN_I + (i)]
    SSTV_REAL *HannA = (SSTV_REAL *) kiwi_icalloc("sstv_video_get", HANN_J * HANN_I, sizeof(SSTV_REAL));
    check(HannA != NULL);
    
    if (!Redraw) {
        ext_send_msg(e->rx_chan, false, "EXT img_width=%d", m->ImgWidth);
        ext_send_msg_encoded(e->rx_chan, false, "EXT", "new_img", "%s", m->ShortName);
    } else {
        ext_send_msg(e->rx_chan, false, "EXT redraw");
    }

    PixelGrid_t *PixelGrid;
    kiwi_ifree(e->PixelGrid, "sstv_video_get");
    e->PixelGrid_len = m->ImgWidth * m->NumLines * 3;
    PixelGrid = (PixelGrid_t *) kiwi_icalloc("sstv_video_get", e->PixelGrid_len, sizeof(PixelGrid_t));
    e->PixelGrid = PixelGrid;

    // Initialize Hann windows of different lengths
    u2_t HannLens[7] = { 48, 64, 96, 128, 256, 512, 1024 };
    for (j = 0; j < 7; j++) {
        for (i = 0; i < HannLens[j]; i++) {
            Hann(j,i) = 0.5 * (1 - SSTV_MCOS( (2 * M_PI * i) / (HannLens[j] - 1)) );
        }
        NextTask("sstv Hann");
    }


    // Starting times of video channels on every line, counted from beginning of line
    SSTV_REAL Tpixels;
    
    switch (m->format) {

    case FMT_420:
        // Sp00s[12]
        ChanLen[0]   = m->PixelTime * m->ImgWidth * 2;
        ChanLen[1]   = ChanLen[2] = m->PixelTime * m->ImgWidth;
        ChanStart[0] = m->SyncTime + m->PorchTime;
        ChanStart[1] = ChanStart[0] + ChanLen[0] + m->SeptrTime;
        ChanStart[2] = ChanStart[1];
        break;

    case FMT_422:
        // Sp00s1s2
        ChanLen[0]   = m->PixelTime * m->ImgWidth * 2;
        ChanLen[1]   = ChanLen[2] = m->PixelTime * m->ImgWidth;
        ChanStart[0] = m->SyncTime + m->PorchTime;
        ChanStart[1] = ChanStart[0] + ChanLen[0] + m->SeptrTime;
        ChanStart[2] = ChanStart[1] + ChanLen[1] + m->SeptrTime;
        break;

    case FMT_242:
        // S0112
        Tpixels      = m->PixelTime * m->ImgWidth * 3.0 / 4.0;
        ChanLen[0]   = ChanLen[2] = Tpixels;
        ChanLen[1]   = Tpixels * 2;
        ChanStart[0] = m->SyncTime + m->PorchTime;
        ChanStart[1] = ChanStart[0] + ChanLen[0];
        ChanStart[2] = ChanStart[1] + ChanLen[1];
        break;

    case FMT_REV:
        // s0s1Sp2
        ChanLen[0]   = ChanLen[1] = ChanLen[2] = m->PixelTime * m->ImgWidth;
        ChanStart[0] = m->SeptrTime;
        ChanStart[1] = ChanStart[0] + ChanLen[0] + m->SeptrTime;
        ChanStart[2] = ChanStart[1] + ChanLen[1] + m->SyncTime + m->PorchTime;
        break;

    case FMT_DEFAULT:
    default:
        // Sp0s1s2
        ChanLen[0]   = ChanLen[1] = ChanLen[2] = m->PixelTime * m->ImgWidth;
        ChanStart[0] = m->SyncTime + m->PorchTime;
        ChanStart[1] = ChanStart[0] + ChanLen[0] + m->SeptrTime;
        ChanStart[2] = ChanStart[1] + ChanLen[1] + m->SeptrTime;
        break;
    }

    // Number of channels per line
    if (m->format == FMT_BW)
        NumChans = 1;
    else
    if (m->format == FMT_420)
        NumChans = 2;
    else
        NumChans = 3;

    // Plan ahead the time instants (in samples) at which to take pixels out
    int PixelIdx = 0;
  
    for (y=0; y < m->NumLines; y++) {
        for (Channel=0; Channel < NumChans; Channel++) {
            for (x=0; x < m->ImgWidth; x++) {
                assert_array_dim(PixelIdx, e->PixelGrid_len);

                if (m->format == FMT_420) {
                    if (Channel == 1) {
                        if (y % 2 == 0)
                            PixelGrid[PixelIdx].Channel = 1;
                        else
                            PixelGrid[PixelIdx].Channel = 2;
                    } else
                        PixelGrid[PixelIdx].Channel = 0;
                } else {
                    PixelGrid[PixelIdx].Channel = Channel;
                }
        
                SSTV_REAL time = y * m->LineTime + ChanStart[Channel] +
                    ((SSTV_REAL) x - 0.5)/m->ImgWidth * ChanLen[PixelGrid[PixelIdx].Channel];
                PixelGrid[PixelIdx].Time = (int) SSTV_MROUND(Rate * time) + Skip;
                //if (x == 0 && Channel == 0)
                //    { real_printf("y%d|%.3f ", y, time); fflush(stdout); }

                PixelGrid[PixelIdx].X = x;
                PixelGrid[PixelIdx].Y = y;
                PixelGrid[PixelIdx].Last = false;
                PixelIdx++;
            } // x, width
            NextTask("sstv pixelgrid");
        } // Channel
    } // y, line

    assert_array_dim(PixelIdx-1, e->PixelGrid_len);
    PixelGrid[PixelIdx-1].Last = true;

    //for (k=0; k < 1024; k++) { real_printf("%d|%d ", k, PixelGrid[k].Time); fflush(stdout); }

    // set PixelIdx to first pixel that has a positive time defined
    for (k=0; k < PixelIdx; k++) {
        assert_array_dim(k, e->PixelGrid_len);
        if (PixelGrid[k].Time >= 0) {
            PixelIdx = k;
            if (PixelIdx != 0)
                printf("SSTV: FIRST PixelIdx=%d\n", PixelIdx);
            break;
        }
    }
    assert_array_dim(PixelIdx, e->PixelGrid_len);

    u1_t *pixels, *p;
    kiwi_ifree(e->pixels, "sstv_video_get");
    e->pixels_len = m->ImgWidth * m->NumLines * 3;
    pixels = (u1_t *) kiwi_icalloc("sstv_video_get", e->pixels_len, sizeof(u1_t));
    assert(pixels != NULL);
    e->pixels = pixels;

    Length        = m->LineTime * m->NumLines * sstv.nom_rate;
    SyncTargetBin = GET_BIN(1200+e->pic.HeaderShift, FFTLen);
    SyncSampleNum = 0;


//////////////////////////////////////////////////////////////////////////


    // Loop through signal
    //printf("SSTV: video len=%d lines=%d width=%d\n", Length, m->NumLines, m->ImgWidth);

    for (SampleNum = 0; SampleNum < Length; SampleNum++) {
    
    if (e->reset) { kiwi_ifree(HannA, "sstv_video_get"); return false; }

    if (!Redraw) {

        /*** Read ahead from sound card ***/

        if (e->pcm.WindowPtr == 0 || e->pcm.WindowPtr >= PCM_BUFLEN - PCM_BUFLEN/4) sstv_pcm_read(e, PCM_BUFLEN/2);
     

        /*** Store the sync band for later adjustments ***/

        if (SampleNum == NextSyncTime) {
 
            Praw = Psync = 0;

            memset(e->fft.in1k, 0, sizeof(SSTV_REAL) * FFTLen);
       
            // Hann window
            for (i = 0; i < 64; i++)
                e->fft.in1k[i] = e->pcm.Buffer[e->pcm.WindowPtr+i-32] / 32768.0 * Hann(1,i);

            SSTV_FFTW_EXECUTE(e->fft.Plan1024);
            NextTask("sstv FFT sync");

            for (i=GET_BIN(1500+e->pic.HeaderShift,FFTLen); i<=GET_BIN(2300+e->pic.HeaderShift, FFTLen); i++)
                Praw += POWER(e->fft.out1k[i]);

            for (i=SyncTargetBin-1; i<=SyncTargetBin+1; i++)
                Psync += POWER(e->fft.out1k[i]) * (1- .5*abs(SyncTargetBin-i));

            Praw  /= (GET_BIN(2300+e->pic.HeaderShift, FFTLen) - GET_BIN(1500+e->pic.HeaderShift, FFTLen));
            Psync /= 2.0;

            // If there is more than twice the amount of power per Hz in the
            // sync band than in the video band, we have a sync signal here
            //assert_array_dim(SyncSampleNum, e->HasSync_len);
            if (/* SyncSampleNum >= 0 && */ SyncSampleNum < e->HasSync_len) {
                e->HasSync[SyncSampleNum] = (Psync > 2*Praw)? true:false;
            } else {
                lprintf("SSTV: HasSync SET mode=%d SyncSampleNum=%d HasSync_len=%d\n",
                    Mode, SyncSampleNum, e->HasSync_len);
            }
    
            NextSyncTime += 13;
            SyncSampleNum++;
        }


        /*** Estimate SNR ***/

        if (SampleNum == NextSNRtime) {
        
            memset(e->fft.in1k, 0, sizeof(SSTV_REAL) * FFTLen);
    
            // Apply Hann window
            for (i = 0; i < FFTLen; i++)
                e->fft.in1k[i] = e->pcm.Buffer[e->pcm.WindowPtr + i - FFTLen/2] / 32768.0 * Hann(6,i);
    
            SSTV_FFTW_EXECUTE(e->fft.Plan1024);
            NextTask("sstv FFT SNR");
    
            // Calculate video-plus-noise power (1500-2300 Hz)
    
            Pvideo_plus_noise = 0;
            for (n = GET_BIN(1500+e->pic.HeaderShift, FFTLen); n <= GET_BIN(2300+e->pic.HeaderShift, FFTLen); n++)
                Pvideo_plus_noise += POWER(e->fft.out1k[n]);
    
            // Calculate noise-only power (400-800 Hz + 2700-3400 Hz)
    
            Pnoise_only = 0;
            for (n = GET_BIN(400+e->pic.HeaderShift,  FFTLen); n <= GET_BIN(800+e->pic.HeaderShift, FFTLen);  n++)
                Pnoise_only += POWER(e->fft.out1k[n]);
    
            for (n = GET_BIN(2700+e->pic.HeaderShift, FFTLen); n <= GET_BIN(3400+e->pic.HeaderShift, FFTLen); n++)
                Pnoise_only += POWER(e->fft.out1k[n]);
    
            // Bandwidths
            VideoPlusNoiseBins = GET_BIN(2300, FFTLen) - GET_BIN(1500, FFTLen) + 1;
    
            NoiseOnlyBins      = GET_BIN(800,  FFTLen) - GET_BIN(400,  FFTLen) + 1 +
                                 GET_BIN(3400, FFTLen) - GET_BIN(2700, FFTLen) + 1;
    
            ReceiverBins       = GET_BIN(3400, FFTLen) - GET_BIN(400,  FFTLen);
    
            // Eq 15
            Pnoise  = Pnoise_only * (1.0 * ReceiverBins / NoiseOnlyBins);
            Psignal = Pvideo_plus_noise - Pnoise_only * (1.0 * VideoPlusNoiseBins / NoiseOnlyBins);
    
            // Lower bound to -20 dB
            SNR = ((Psignal / Pnoise < .01) ? -20 : 10 * SSTV_MLOG10(Psignal / Pnoise));
    
            NextSNRtime += 256;
        }


        /*** FM demodulation ***/

        if (SampleNum % e->fm_sample_interval == 0) {   // Take FFT every fm_sample_interval samples

            //PrevFreq = Freq;
    
            // Adapt window size to SNR
    
            if    (!e->adaptive) WinIdx = 0;
            else if (SNR >=  20) WinIdx = 0;
            else if (SNR >=  10) WinIdx = 1;
            else if (SNR >=   9) WinIdx = 2;
            else                 WinIdx = 3;
            
            /*  these are too cpu intensive for the Kiwi
            else if (SNR >=   3) WinIdx = 3;
            else if (SNR >=  -5) WinIdx = 4;
            else if (SNR >= -10) WinIdx = 5;
            else                 WinIdx = 6;
            */
        
            // Minimum winlength can be doubled for Scottie DX
            if (Mode == SDX && WinIdx < 6) WinIdx++;
    
            memset(e->fft.in1k, 0, sizeof(SSTV_REAL) * FFTLen);
            memset(Power,  0, sizeof(SSTV_REAL) * 1024);
    
            // Apply window function
            
            WinLength = HannLens[WinIdx];
            for (i = 0; i < WinLength; i++)
                e->fft.in1k[i] = e->pcm.Buffer[e->pcm.WindowPtr + i - WinLength/2] / 32768.0 * Hann(WinIdx,i);
    
            SSTV_FFTW_EXECUTE(e->fft.Plan1024);
            NextTask("sstv FFT FM");

            MaxBin = 0;
          
            // Find the bin with most power
            for (n = GET_BIN(1500 + e->pic.HeaderShift, FFTLen) - 1; n <= GET_BIN(2300 + e->pic.HeaderShift, FFTLen) + 1; n++) {
                assert(n < 1024);
                Power[n] = POWER(e->fft.out1k[n]);
                if (MaxBin == 0 || Power[n] > Power[MaxBin]) MaxBin = n;
            }

            // Find the peak frequency by Gaussian interpolation
            if (MaxBin > GET_BIN(1500 + e->pic.HeaderShift, FFTLen) - 1 && MaxBin < GET_BIN(2300 + e->pic.HeaderShift, FFTLen) + 1) {
                Freq = MaxBin + (SSTV_MLOG( Power[MaxBin + 1] / Power[MaxBin - 1] )) /
                    (2 * SSTV_MLOG( SSTV_MPOW(Power[MaxBin], 2) / (Power[MaxBin + 1] * Power[MaxBin - 1])));
                // In Hertz
                Freq = Freq / FFTLen * sstv.nom_rate;
            } else {
                // Clip if out of bounds
                Freq = ( (MaxBin > GET_BIN(1900 + e->pic.HeaderShift, FFTLen)) ? 2300 : 1500 ) + e->pic.HeaderShift;
            }
        } /* endif FM SampleNum % fm_sample_interval */

        // Linear interpolation of (chronologically) intermediate frequencies, for redrawing
        //InterpFreq = PrevFreq + (Freq-PrevFreq) * ...  // TODO!

        // Calculate luminency & store for later use
        assert_array_dim(SampleNum, e->StoredLum_len);
        e->StoredLum[SampleNum] = clip((Freq - (1500 + e->pic.HeaderShift)) / 3.1372549);

        e->pcm.WindowPtr++;
    } /* endif !Redraw */


//////////////////////////////////////////////////////////////////////////


    if (PixelIdx < e->PixelGrid_len && SampleNum == PixelGrid[PixelIdx].Time) {

        x = PixelGrid[PixelIdx].X;
        y = PixelGrid[PixelIdx].Y;
        Channel = PixelGrid[PixelIdx].Channel;
      
        // Store pixel
        assert_array_dim(SampleNum, e->StoredLum_len);
        (*e->image)[x][y][Channel] = e->StoredLum[SampleNum];

        // Some modes have R-Y & B-Y channels that are twice the height of the Y channel
        if (Channel > 0 && m->format == FMT_420)
            (*e->image)[x][y+1][Channel] = e->StoredLum[SampleNum];

        // Calculate and draw pixels to pixbuf on line change
        if (x == m->ImgWidth-1 || PixelGrid[PixelIdx].Last) {

            u1_t *pixrow = pixels + y*m->ImgWidth;
            for (tx = 0; tx < m->ImgWidth; tx++) {
                p = pixrow + tx * 3;
                assert_array_dim((y*m->ImgWidth + tx*3), e->pixels_len);

                switch (m->ColorEnc) {

                case RGB:
                    p[0] = (*e->image)[tx][y][0];
                    p[1] = (*e->image)[tx][y][1];
                    p[2] = (*e->image)[tx][y][2];
                    break;
    
                case GBR:
                    p[0] = (*e->image)[tx][y][2];
                    p[1] = (*e->image)[tx][y][0];
                    p[2] = (*e->image)[tx][y][1];
                    break;

                case YUV: {
                    u1_t Y = (*e->image)[tx][y][0];
                    u1_t U = (*e->image)[tx][y][1];
                    u1_t V = (*e->image)[tx][y][2];
                    p[0] = clip((100 * Y + 140 * U - 17850) / 100.0);
                    p[1] = clip((100 * Y -  71 * U - 33 * V + 13260) / 100.0);
                    p[2] = clip((100 * Y + 178 * V - 22695) / 100.0);
                    break;
                }

                case BW:
                    p[0] = p[1] = p[2] = (*e->image)[tx][y][0];
                    break;
                }
            }
        
            if (Channel >= NumChans-1) {
                //real_printf("%s%d ", Redraw? "R":"L", y); fflush(stdout);
                int _snr = MIN(SNR, 127);
                _snr = MAX(_snr, -128);
                
                // double-up for 120/128 line modes
                for (int i = m->LineHeight; i > 0; i--)
                    ext_send_msg_data2(e->rx_chan, false, Redraw? 1:0, (u1_t) _snr+128, pixrow, m->ImgWidth*3 * sizeof(u1_t));
                if (Redraw) TaskSleepReasonMsec("sstv redraw", 10);
            }
        }

        PixelIdx++;

    } /* endif (PixelIdx < e->PixelGrid_len && SampleNum == PixelGrid[PixelIdx].Time) */

    } // for SampleNum

    kiwi_ifree(HannA, "sstv_video_get");
    return true;
}

#include "sstv.h"

void sstv_video_once(sstv_chan_t *e)
{
    e->Image_len = 800*616*3;
    e->Image = (u1_t *) calloc(e->Image_len, sizeof(u1_t));
    assert(e->Image != NULL);
}

#if 0
u1_t IMAGE(int x, int y, int c)
{
    sstv_chan_t *e = &sstv_chan[0];
    int i = (x)*(616*3) + (y)*3 + (c);
    if (i < 0 || i >= e->Image_len) { printf("i=%d x=%d y=%d c=%d\n", i, x, y, c); panic("IMAGE"); }
    assert(e->Image != NULL);
    return e->Image[i];
}

void IMAGE_s(int x, int y, int c, u1_t v)
{
    sstv_chan_t *e = &sstv_chan[0];
    int i = (x)*(616*3) + (y)*3 + (c);
    if (i < 0 || i >= e->Image_len) { printf("i=%d x=%d y=%d c=%d\n", i, x, y, c); panic("IMAGE_s"); }
    assert(e->Image != NULL);
    e->Image[i] = v;
}
#endif

void sstv_video_init(sstv_chan_t *e, SSTV_REAL rate, u1_t mode)
{
    e->pic.Rate = rate;
    e->pic.Mode = mode;
    ModeSpec_t  *m = &ModeSpec[mode];
    SSTV_REAL spp = rate * m->LineTime / m->ImgWidth;
    e->fm_sample_interval = debug_v? debug_v : ((int) (spp * 3/4));
    printf("SSTV: spp=%.1f fm_sample_interval=%d %s\n", spp, e->fm_sample_interval, debug_v? "(v=)":"");
    
    // Allocate space for cached Lum
    e->StoredLum_len = (int) ((m->LineTime * m->NumLines + 1) * sstv.nom_rate);
    e->StoredLum = (u1_t *) calloc(e->StoredLum_len, sizeof(u1_t));
    assert(e->StoredLum != NULL);

    // Allocate space for sync signal
    e->HasSync_len = (int) (m->LineTime * m->NumLines / (13.0 / sstv.nom_rate) +1);
    e->HasSync = (bool *) calloc(e->HasSync_len, sizeof(bool));
    assert(e->HasSync != NULL);
}

void sstv_video_done(sstv_chan_t *e)
{
    printf("SSTV: sstv_video_done\n");
    free(e->StoredLum); e->StoredLum = NULL;
    free(e->HasSync); e->HasSync = NULL;
    free(e->PixelGrid); e->PixelGrid = NULL;
    free(e->pixels); e->pixels = NULL;
}


/* Demodulate the video signal & store all kinds of stuff for later stages
 *  Mode:      M1, M2, S1, S2, R72, R36...
 *  Rate:      exact sampling rate used
 *  Skip:      number of PCM samples to skip at the beginning (for sync phase adjustment)
 *  Redraw:    FALSE = Apply windowing and FFT to the signal, TRUE = Redraw from cached FFT data
 *  returns:   TRUE when finished, FALSE when aborted
 */
bool sstv_video_get(sstv_chan_t *e, int Skip, bool Redraw)
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
    SSTV_REAL   Hann[7][1024] = {{0}};
    SSTV_REAL   Freq = 0, PrevFreq = 0, InterpFreq = 0;
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
    
    printf("SSTV: sstv_video_get redraw=%d skip=%d mode=%d rate=%.1f\n", Redraw, Skip, Mode, Rate);

    // x:800 y:616 c:3
    #define IMAGE(x,y,c) e->Image[(x)*(616*3) + (y)*3 + (c)]
    assert(e->Image != NULL);
    memset(e->Image, 0, e->Image_len * sizeof(u1_t));
    
    if (!Redraw) {
        ext_send_msg(e->rx_chan, false, "EXT img_width=%d", m->ImgWidth);
        ext_send_msg(e->rx_chan, false, "EXT new_img");
    } else {
        ext_send_msg(e->rx_chan, false, "EXT redraw");
    }

  PixelGrid_t *PixelGrid;
  free(e->PixelGrid);
  e->PixelGrid_len = m->ImgWidth * m->NumLines * 3;
  PixelGrid = (PixelGrid_t *) calloc(e->PixelGrid_len, sizeof(PixelGrid_t));
  e->PixelGrid = PixelGrid;

    // Initialize Hann windows of different lengths
    u2_t HannLens[7] = { 48, 64, 96, 128, 256, 512, 1024 };
    for (j = 0; j < 7; j++) {
        //real_printf("Hann %d\n", HannLens[j]);
        for (i = 0; i < HannLens[j]; i++) {
            Hann[j][i] = 0.5 * (1 - SSTV_MCOS( (2 * M_PI * i) / (HannLens[j] - 1)) );
            //real_printf("%.1e ", Hann[j][i]);
        }
        NextTask("sstv Hann");
        //real_printf("\n");
    }


  // Starting times of video channels on every line, counted from beginning of line
  switch (Mode) {

    case R36:
    case R24:
      ChanLen[0]   = m->PixelTime * m->ImgWidth * 2;
      ChanLen[1]   = ChanLen[2] = m->PixelTime * m->ImgWidth;
      ChanStart[0] = m->SyncTime + m->PorchTime;
      ChanStart[1] = ChanStart[0] + ChanLen[0] + m->SeptrTime;
      ChanStart[2] = ChanStart[1];
      break;

    case S1:
    case S2:
    case SDX:
      ChanLen[0]   = ChanLen[1] = ChanLen[2] = m->PixelTime * m->ImgWidth;
      ChanStart[0] = m->SeptrTime;
      ChanStart[1] = ChanStart[0] + ChanLen[0] + m->SeptrTime;
      ChanStart[2] = ChanStart[1] + ChanLen[1] + m->SyncTime + m->PorchTime;
      break;

    default:
      ChanLen[0]   = ChanLen[1] = ChanLen[2] = m->PixelTime * m->ImgWidth;
      ChanStart[0] = m->SyncTime + m->PorchTime;
      ChanStart[1] = ChanStart[0] + ChanLen[0] + m->SeptrTime;
      ChanStart[2] = ChanStart[1] + ChanLen[1] + m->SeptrTime;
      break;

  }

  // Number of channels per line
  switch(Mode) {
    case R24BW:
    case R12BW:
    case R8BW:
      NumChans = 1;
      break;
    case R24:
    case R36:
      NumChans = 2;
      break;
    default:
      NumChans = 3;
      break;
  }

  // Plan ahead the time instants (in samples) at which to take pixels out
  int PixelIdx = 0;
  
  for (y=0; y < m->NumLines; y++) {
    for (Channel=0; Channel < NumChans; Channel++) {
      for (x=0; x < m->ImgWidth; x++) {
        assert(PixelIdx < e->PixelGrid_len);

        if (Mode == R36 || Mode == R24) {
            if (Channel == 1) {
                if (y % 2 == 0) PixelGrid[PixelIdx].Channel = 1;
            else
                PixelGrid[PixelIdx].Channel = 2;
            } else
                PixelGrid[PixelIdx].Channel = 0;
        } else {
            PixelGrid[PixelIdx].Channel = Channel;
        }
        
        PixelGrid[PixelIdx].Time = (int) SSTV_MROUND(Rate * (y * m->LineTime + ChanStart[Channel] +
          (1.0*(x-.5)/m->ImgWidth * ChanLen[PixelGrid[PixelIdx].Channel]))) + Skip;

        PixelGrid[PixelIdx].X = x;
        PixelGrid[PixelIdx].Y = y;
        
        PixelGrid[PixelIdx].Last = FALSE;

        PixelIdx++;
      } // x, width
        NextTask("sstv pixelgrid");
    } // Channel
  } // y, line

  PixelGrid[PixelIdx-1].Last = TRUE;

    // set PixelIdx to first pixel that has a time defined
    for (k=0; k < PixelIdx; k++) {
        assert(k < e->PixelGrid_len);
        if (PixelGrid[k].Time >= 0) {
            PixelIdx = k;
            //printf("SSTV: FIRST PixelIdx=%d\n", PixelIdx);
            break;
        }
    }
    assert(PixelIdx < e->PixelGrid_len);

        /*case PD50:
        case PD90:
        case PD120:
        case PD160:
        case PD180:
        case PD240:
        case PD290:
          if (CurLineTime >= ChanStart[2] + ChanLen[2]) Channel = 3; // ch 0 of even line
          else if (CurLineTime >= ChanStart[2])         Channel = 2;
          else if (CurLineTime >= ChanStart[1])         Channel = 1;
          else                                          Channel = 0;
          break;*/

    u1_t *pixels, *p;
    free(e->pixels);
    e->pixels_len = m->ImgWidth * m->NumLines * 3;
    pixels = (u1_t *) calloc(e->pixels_len, sizeof(u1_t));
    assert(pixels != NULL);
    e->pixels = pixels;

  Length        = m->LineTime * m->NumLines * sstv.nom_rate;
  SyncTargetBin = GET_BIN(1200+e->pic.HedrShift, FFTLen);
  Abort         = FALSE;
  SyncSampleNum = 0;


//////////////////////////////////////////////////////////////////////////


    // Loop through signal
    //printf("SSTV: video len=%d lines=%d width=%d\n", Length, m->NumLines, m->ImgWidth);

    for (SampleNum = 0; SampleNum < Length; SampleNum++) {

    if (!Redraw) {

      /*** Read ahead from sound card ***/

      if (e->pcm.WindowPtr == 0 || e->pcm.WindowPtr >= PCM_BUFLEN - PCM_BUFLEN/4) sstv_pcm_read(e, PCM_BUFLEN/2);
     

      /*** Store the sync band for later adjustments ***/

      if (SampleNum == NextSyncTime) {
 
        Praw = Psync = 0;

        memset(e->fft.in, 0, sizeof(SSTV_REAL)*FFTLen);
       
        // Hann window
        for (i = 0; i < 64; i++) e->fft.in[i] = e->pcm.Buffer[e->pcm.WindowPtr+i-32] / 32768.0 * Hann[1][i];

        SSTV_FFTW_EXECUTE(e->fft.Plan1024);
        NextTask("sstv FFT sync");

        for (i=GET_BIN(1500+e->pic.HedrShift,FFTLen); i<=GET_BIN(2300+e->pic.HedrShift, FFTLen); i++)
          Praw += POWER(e->fft.out[i]);

        for (i=SyncTargetBin-1; i<=SyncTargetBin+1; i++)
          Psync += POWER(e->fft.out[i]) * (1- .5*abs(SyncTargetBin-i));

        Praw  /= (GET_BIN(2300+e->pic.HedrShift, FFTLen) - GET_BIN(1500+e->pic.HedrShift, FFTLen));
        Psync /= 2.0;

        // If there is more than twice the amount of power per Hz in the
        // sync band than in the video band, we have a sync signal here
        assert(SyncSampleNum < e->HasSync_len);
        e->HasSync[SyncSampleNum] = (Psync > 2*Praw);

        NextSyncTime += 13;
        SyncSampleNum ++;

      }


      /*** Estimate SNR ***/

      if (SampleNum == NextSNRtime) {
        
        memset(e->fft.in, 0, sizeof(SSTV_REAL)*FFTLen);

        // Apply Hann window
        for (i = 0; i < FFTLen; i++) e->fft.in[i] = e->pcm.Buffer[e->pcm.WindowPtr + i - FFTLen/2] / 32768.0 * Hann[6][i];

        SSTV_FFTW_EXECUTE(e->fft.Plan1024);
        NextTask("sstv FFT SNR");

        // Calculate video-plus-noise power (1500-2300 Hz)

        Pvideo_plus_noise = 0;
        for (n = GET_BIN(1500+e->pic.HedrShift, FFTLen); n <= GET_BIN(2300+e->pic.HedrShift, FFTLen); n++)
          Pvideo_plus_noise += POWER(e->fft.out[n]);

        // Calculate noise-only power (400-800 Hz + 2700-3400 Hz)

        Pnoise_only = 0;
        for (n = GET_BIN(400+e->pic.HedrShift,  FFTLen); n <= GET_BIN(800+e->pic.HedrShift, FFTLen);  n++)
          Pnoise_only += POWER(e->fft.out[n]);

        for (n = GET_BIN(2700+e->pic.HedrShift, FFTLen); n <= GET_BIN(3400+e->pic.HedrShift, FFTLen); n++)
          Pnoise_only += POWER(e->fft.out[n]);

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
        //if (SNR < 10) real_printf("S%.0f ", SNR); fflush(stdout);

        NextSNRtime += 256;
      }


      /*** FM demodulation ***/

      if (SampleNum % e->fm_sample_interval == 0) { // Take FFT every 6 samples

        PrevFreq = Freq;

        // Adapt window size to SNR

        if      (!Adaptive)  WinIdx = 0;
        
        else if (SNR >=  20) WinIdx = 0;
        else if (SNR >=  10) WinIdx = 1;
        else if (SNR >=   9) WinIdx = 2;
        else if (SNR >=   3) WinIdx = 3;
        else if (SNR >=  -5) WinIdx = 4;
        else if (SNR >= -10) WinIdx = 5;
        else                 WinIdx = 6;

        // Minimum winlength can be doubled for Scottie DX
        if (Mode == SDX && WinIdx < 6) WinIdx++;

        memset(e->fft.in, 0, sizeof(SSTV_REAL)*FFTLen);
        memset(Power,  0, sizeof(SSTV_REAL)*1024);

        // Apply window function
        
        WinLength = HannLens[WinIdx];
        for (i = 0; i < WinLength; i++) e->fft.in[i] = e->pcm.Buffer[e->pcm.WindowPtr + i - WinLength/2] / 32768.0 * Hann[WinIdx][i];

        SSTV_FFTW_EXECUTE(e->fft.Plan1024);
        NextTask("sstv FFT FM");

        MaxBin = 0;
          
        // Find the bin with most power
        for (n = GET_BIN(1500 + e->pic.HedrShift, FFTLen) - 1; n <= GET_BIN(2300 + e->pic.HedrShift, FFTLen) + 1; n++) {
            assert(n < 1024);
          Power[n] = POWER(e->fft.out[n]);
          if (MaxBin == 0 || Power[n] > Power[MaxBin]) MaxBin = n;

        }

        // Find the peak frequency by Gaussian interpolation
        if (MaxBin > GET_BIN(1500 + e->pic.HedrShift, FFTLen) - 1 && MaxBin < GET_BIN(2300 + e->pic.HedrShift, FFTLen) + 1) {
          Freq = MaxBin +            (SSTV_MLOG( Power[MaxBin + 1] / Power[MaxBin - 1] )) /
                           (2 * SSTV_MLOG( SSTV_MPOW(Power[MaxBin], 2) / (Power[MaxBin + 1] * Power[MaxBin - 1])));
          // In Hertz
          Freq = Freq / FFTLen * sstv.nom_rate;
        } else {
          // Clip if out of bounds
          Freq = ( (MaxBin > GET_BIN(1900 + e->pic.HedrShift, FFTLen)) ? 2300 : 1500 ) + e->pic.HedrShift;
        }

      } /* endif (SampleNum % 6 == 0) */

      // Linear interpolation of (chronologically) intermediate frequencies, for redrawing
      //InterpFreq = PrevFreq + (Freq-PrevFreq) * ...  // TODO!

      // Calculate luminency & store for later use
      assert(SampleNum < e->StoredLum_len);
      e->StoredLum[SampleNum] = clip((Freq - (1500 + e->pic.HedrShift)) / 3.1372549);

        e->pcm.WindowPtr++;
    } /* endif (!Redraw) */


//////////////////////////////////////////////////////////////////////////


    if (0 && PixelIdx >= e->PixelGrid_len) {
        printf("Redraw=%d SampleNum=%d PixelIdx=%d\n", Redraw, SampleNum, PixelIdx);
        panic("sstv");
    }
    
    if (PixelIdx < e->PixelGrid_len && SampleNum == PixelGrid[PixelIdx].Time) {

      x = PixelGrid[PixelIdx].X;
      y = PixelGrid[PixelIdx].Y;
      Channel = PixelGrid[PixelIdx].Channel;
//if (y >= 255)
//printf("x=%d y=%d c=%d PixelIdx=%d SampleNum=%d\n", x, y, Channel, PixelIdx, SampleNum);
      
      // Store pixel
      assert(SampleNum < e->StoredLum_len);
      IMAGE(x,y,Channel) = e->StoredLum[SampleNum];
      //IMAGE_s(x,y,Channel,e->StoredLum[SampleNum]);

      // Some modes have R-Y & B-Y channels that are twice the height of the Y channel
      if (Channel > 0 && (Mode == R36 || Mode == R24))
        IMAGE(x,y+1,Channel) = e->StoredLum[SampleNum];
        //IMAGE_s(x,y+1,Channel,e->StoredLum[SampleNum]);

      // Calculate and draw pixels to pixbuf on line change
      if (x == m->ImgWidth-1 || PixelGrid[PixelIdx].Last) {
//printf("LAST\n");

        u1_t *pixrow = pixels + y*m->ImgWidth;
        for (tx = 0; tx < m->ImgWidth; tx++) {
          p = pixrow + tx * 3;
          assert((y*m->ImgWidth + tx*3) < e->pixels_len);

          switch(m->ColorEnc) {

            case RGB:
              p[0] = IMAGE(tx,y,0);
              p[1] = IMAGE(tx,y,1);
              p[2] = IMAGE(tx,y,2);
              break;

            case GBR:
            #if 1
              p[0] = IMAGE(tx,y,2);
              p[1] = IMAGE(tx,y,0);
              p[2] = IMAGE(tx,y,1);
            #else
                p[0]=p[1]=p[2]=0;
                #if 1
                if(y==tx||y==tx+1||y==tx-1)p[0]=255;
                if(tx<=((y-32<0)?0:(y-32)))p[1]=255;
                if(y<=((tx-32<0)?0:(tx-32)))p[2]=255;
                #else
                if(tx<100)p[0]=255;else
                if(tx<200)p[1]=255;else
                    p[2]=255;
                #endif
            #endif
              break;

            case YUV:
              p[0] = clip((100 * IMAGE(tx,y,0) + 140 * IMAGE(tx,y,1) - 17850) / 100.0);
              p[1] = clip((100 * IMAGE(tx,y,0) -  71 * IMAGE(tx,y,1) - 33 *
                  IMAGE(tx,y,2) + 13260) / 100.0);
              p[2] = clip((100 * IMAGE(tx,y,0) + 178 * IMAGE(tx,y,2) - 22695) / 100.0);
              break;

            case BW:
              p[0] = p[1] = p[2] = IMAGE(tx,y,0);
              break;

          }
        }
        
        if (Channel == NumChans-1) {
            //real_printf("%s%d ", Redraw? "R":"L", y); fflush(stdout);
            ext_send_msg_data(e->rx_chan, false, Redraw? 1:0, pixrow, m->ImgWidth*3 * sizeof(u1_t));
            if (Redraw) TaskSleepReasonMsec("sstv redraw", 10);
        }
      }

      PixelIdx++;


    } /* endif (SampleNum == PixelGrid[PixelIdx].Time) */
    
    if (Abort) {
      return FALSE;
    }

  }

  return TRUE;
}

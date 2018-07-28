/******************************************************************************
 *
 * Project:  OpenCPN
 * Purpose:  weather fax Plugin
 * Author:   Sean D'Epagnier
 *
 ***************************************************************************
 *   Copyright (C) 2015 by Sean D'Epagnier                                 *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301,  USA.         *
 ***************************************************************************
 */

 // adapted from github.com/seandepagnier/weatherfax_pi

#include "FaxDecoder.h"
#include "ext.h"
#include "misc.h"
#include "debug.h"

#include <math.h>
#include <complex>

#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

FaxDecoder m_FaxDecoder[MAX_RX_CHANS];

/* Note: the decoding algorithms are adapted from yahfax (on sourceforge)
   which was an improved adaptation of hamfax. */

static int minus_int(const void *a, const void *b)
{
    return *(const int*)a - *(const int*)b;
}

static int median(int *x, int n)
{
     qsort(x, n, sizeof *x, minus_int);
     return x[n/2];
}

static TYPEREAL apply_firfilter(FaxDecoder::firfilter *filter, TYPEREAL sample)
{
// Narrow, middle and wide fir low pass filter from ACfax
     const int buffer_count = 17;
     const TYPEREAL lpfcoeff[3][17]={
          { -7,-18,-15, 11, 56,116,177,223,240,223,177,116, 56, 11,-15,-18, -7},
          {  0,-18,-38,-39,  0, 83,191,284,320,284,191, 83,  0,-39,-38,-18,  0},
          {  6, 20,  7,-42,-74,-12,159,353,440,353,159,-12,-74,-42,  7, 20,  6}};

     const TYPEREAL *c = lpfcoeff[filter->bandwidth];
     const TYPEREAL *c_end=lpfcoeff[filter->bandwidth] + ((sizeof *lpfcoeff) / (sizeof **lpfcoeff));
     TYPEREAL* const b_begin=filter->buffer;
     TYPEREAL* const b_end=filter->buffer + buffer_count;
     TYPEREAL sum=0;

     TYPEREAL *current = filter->buffer + filter->current;
     // replace oldest value with current
     *current=sample;

     // convolution
     while(current!=b_end)
          sum+=(*current++)*(*c++);
     current=b_begin;
     while(c!=c_end)
          sum+=(*current++)*(*c++);

     // point again to oldest value
     if(--current<b_begin)
          current=b_end-1;

     filter->current = current - filter->buffer;
     return sum;
}

void FaxDecoder::UpdateSampleRate()
{
    m_SamplesPerSec_frac = ext_update_get_sample_rateHz(m_rx_chan);
    m_SamplesPerSec_nom = SND_RATE;
    m_SampleRateRatio = m_SamplesPerSec_frac / m_SamplesPerSec_nom;
}

void FaxDecoder::ProcessSamples(s2_t *samps, int nsamps, float shift)
{
    int i = 0;
    
    if (m_bEndDecoding) return;
    
    if (shift) m_skip = shift * m_SamplesPerLine;

    if (m_skip) {
        int skip = MIN(nsamps, m_skip);
        nsamps -= skip;
        samps = &samps[skip];
        //printf("FAX m_skip %d skip %d\n", m_skip, skip);
        m_skip -= skip;
    }

    while (i < nsamps) {
        for (; i < nsamps && m_samp_idx < m_SamplesPerLine;) {
            samples[m_samp_idx] = samps[i];
            m_samp_idx++;
            m_fi += m_SampleRateRatio;
            i = trunc(m_fi);
        }
        
        if (m_samp_idx == m_SamplesPerLine) {
		    NextTask("DecodeFax BD");
            if (ev_dump) evLatency(EC_TRIG_ACCUM_ON, EV_EXT, ev_dump, "FAX", evprintf("rx%d fax task cycle time", m_rx_chan));
            DecodeFax();
            if (ev_dump) evLatency(EC_TRIG_ACCUM_OFF, EV_EXT, ev_dump, "FAX", evprintf("rx%d fax task cycle time", m_rx_chan));
		    NextTask("DecodeFax AD");
            m_samp_idx = 0;
        }
    }
    m_fi -= nsamps;     // keep bounded
}

static float mm_mag[8192], mm_y[8192], mm_xo[8192], mm_x[8192];
static u1_t mm_d[8192];
static int frame;
static u1_t scope[4][1024];
static TYPEREAL normalize_sample = 1.0/32768.0;

void FaxDecoder::DemodulateData()
{
     double f=0, ph_inc;
     int i;
     int tslice0 = m_SamplesPerLine/4, tslice1 = m_SamplesPerLine/2, tslice3 = m_SamplesPerLine*3/4;

    // update sps for mixers
    UpdateSampleRate();

    if (m_SamplesPerSec_frac != m_SamplesPerSec_frac_prev) {
        ext_send_msg(m_rx_chan, false, "EXT fax_sps_changed");
        if (m_rx_chan == 0) printf("FAX rx%d sps %.12e %.12e diff=%.3e\n", m_rx_chan,
            m_SamplesPerSec_frac, m_SamplesPerSec_frac_prev, m_SamplesPerSec_frac - m_SamplesPerSec_frac_prev);
        m_SamplesPerSec_frac_prev = m_SamplesPerSec_frac;
    }
    
    ph_inc = m_carrier/m_SamplesPerSec_frac;
    //printf("FaxDecoder::DecodeFax m_rx_chan=%d m_SamplesPerSec_nom=%.1f\n", m_rx_chan, m_SamplesPerSec_nom);

    //printf("%f .. %f .. %f | %f .. %f .. %f | %f\n", MASIN(-1), MASIN(0), MASIN(1), MASIN(-1)/K_2PI, MASIN(0)/K_2PI, MASIN(1)/K_2PI, K_2PI);
    //printf("DemodulateData srate= %.3f %.3f car=%.3f dev=%.3f ph_inc=%.3f\n",
    //    m_SamplesPerSec_nom, m_SamplesPerSec_frac, m_carrier, m_deviation, ph_inc);

    for (i=0; i < m_SamplesPerLine; i++) {

        if (i == tslice0 || i == tslice1 || i == tslice3)
            NextTask("DemodulateData");

        // mix to carrier so start/stop/black/white freqs will be relative to zero

        TYPEREAL samp = samples[i] * normalize_sample;      // -1..0..1

        TYPEREAL Icur = apply_firfilter(firfilters+0, samp*MCOS(K_2PI*f));
        TYPEREAL Qcur = apply_firfilter(firfilters+1, samp*MSIN(K_2PI*f));

        f += ph_inc;
        if (f > 1.0) f -= 1.0;      // keep bounded
        
        TYPEREAL mag = MSQRT(Qcur*Qcur + Icur*Icur);
        mm_mag[i] = mag;
        //if (i>=100&&i<104)
        //real_printf("%7.1f %7.1f %6.3f ", Icur, mag, Icur/mag);
        Icur /= mag;
        Qcur /= mag;

        //if (i==100) { real_printf("%.0f ", mag); fflush(stdout); }
        //if (mag > 0.1) {
        if (1) {
        
            TYPEREAL x = -1.3 * (Icur*(Qcur-Qprev) - Qcur*(Icur-Iprev)) * (m_SamplesPerSec_nom/m_deviation/8);
            //if (i==100) { real_printf("%.1f ", x); fflush(stdout); }
        
            if (x < -1.0) x = -1.0; else if (x > 1.0) x = 1.0;      // clamp

            x = x/2.0 + 0.5;
            data[i] = (u1_t)(x*255.0);
            mm_d[i] = data[i];
            
            //if (i<48) real_printf("%3d ", data[i]);
        } else {
            data[i] = 255;      // force noise floor to white?
        }

        data_i[i] = data[i];

        Iprev = Icur;
        Qprev = Qcur;
    }
    NextTask("DemodulateData");
     
    //real_printf("\n");
    
    //print_max_min_i("data", data_i, n);
    //print_max_min_f("mag", mm_mag, n);
    //print_max_min_f("y", mm_y, n);
    //print_max_min_f("xo", mm_xo, n);
    //print_max_min_f("x", mm_x, n);
    //print_max_min_u1("data", mm_d, n);

    #if 0
    if ((frame&3) == 3) {
        ext_send_msg_data(m_rx_chan, false, FAX_MSG_CLEAR, NULL, 0);
        ext_send_msg_data(m_rx_chan, false, 0, &scope[0][0], 1024);
        ext_send_msg_data(m_rx_chan, false, 1, &scope[1][0], 1024);
    }
    frame++;
    #endif
}

bool FaxDecoder::DecodeFax()
{
    const int phasingSkipLines = 2;
    
    DemodulateData();

//jksx
#if 0
    enum Header hty;
    hty = DetectLineType(data, m_SamplesPerLine);
    if (hty == START) printf("FAX lineType=START\n");
    else
    if (hty == STOP) printf("FAX lineType=STOP\n");
    else
    //printf("FAX lineType=IMAGE\n");
    return true;
#endif
    
    enum Header type;
#if 0
    type = DetectLineType(data, m_SamplesPerLine);
if (type == START) printf("FAX lineType=START\n");
else
if (type == STOP) printf("FAX lineType=STOP\n");
#endif
    if(m_bSkipHeaderDetection)
        type = IMAGE;

    /* accumulate how many start or stop lines we are getting */
    if(type == lasttype && type != IMAGE)
        typecount++;
    else {
        typecount--; /* can deal with noisy input if we had a miss rather than reset here */
        if(typecount < 0)
            typecount = 0;
    }
    lasttype = type;

    if(type != IMAGE) { /* if type is start or stop */
        /* require 4 less lines than there really are to handle
           noise and also misalignment on first and last lines */
        const int leewaylines = 4;

        if(typecount == m_StartLength*m_lpm/60.0 - leewaylines) {
            if(type == START/* && m_imageline < 100*/) {
                /* prepare for phasing */
                /* image start detected, reset image at 0 lines  */
                if(!m_bIncludeHeadersInImages) {
                    m_imageline = 0;
                    imgpos = 0;
                }

                phasingLinesLeft = m_phasingLines;
                gotstart = true;
            } else {
                // exit at stop
                goto done;
            }
        }
    }

    /* throw away first 2 lines of phasing because we are not sure
       if they are misaligned start lines */
    if(phasingLinesLeft > 0 && phasingLinesLeft <= m_phasingLines - phasingSkipLines)
        phasingPos[phasingLinesLeft-1] = FaxPhasingLinePosition(data, m_SamplesPerLine);

    if(type == IMAGE && phasingLinesLeft >= -phasingSkipLines)
        if(!--phasingLinesLeft) /* decrement each phasing line */
            phasingSkipData = median(phasingPos, m_phasingLines - phasingSkipLines);

    /* go past the phasing lines we skipping to make sure we are in the image */
    if(m_bIncludeHeadersInImages || (type == IMAGE && phasingLinesLeft < -phasingSkipLines)) {
        if(m_imageline >= height) {
            height *= 2;
            m_imgdata = (u1_t*)realloc(m_imgdata, m_imagewidth*height*m_imagecolors);
        }
       
        DecodeImageLine(data, m_SamplesPerLine, m_imgdata+imgpos);
        
        int skiplen = ((phasingSkipData-phasingSkippedData)%m_SamplesPerLine)*m_imagewidth/m_SamplesPerLine;
        phasingSkippedData = phasingSkipData; /* reset skipped position */
            
        imgpos += (m_imagewidth-skiplen)*m_imagecolors;
        m_imageline++;
    }
done:

     /* put leftover data into an image */
     if((m_bIncludeHeadersInImages || gotstart) &&
        m_imageline > 10) { /* throw away really short images */
         int is = m_imagewidth*m_imageline*m_imagecolors;
     }

     CloseInput();

     return true;
}

/* perform fourier transform at a specific frequency to look for start/stop */
TYPEREAL FaxDecoder::FourierTransformSub(u1_t* buffer, int buffer_len, int freq)
{
    int tslice0 = buffer_len/4, tslice1 = buffer_len/2, tslice3 = buffer_len*3/4;

    TYPEREAL k = -2 * M_PI * freq * 60.0 / m_lpm / buffer_len;
    TYPEREAL retr = 0, reti = 0;
    for (int n=0; n < buffer_len; n++) {
        retr += buffer[n]*MCOS(k*n);
        reti += buffer[n]*MSIN(k*n);

        if (n == tslice0 || n == tslice1 || n == tslice3)
            NextTask("FourierTransformSub");
    }
    return MSQRT(retr*retr + reti*reti);
}

/* see if the fourier transform at the start and stop frequencies reveils header */
FaxDecoder::Header FaxDecoder::DetectLineType(u1_t* buffer, int buffer_len)
{
     const int threshold = 5; /* 5 is pretty arbitrary but works in practice even with lots of noise */
     TYPEREAL start_det = FourierTransformSub(buffer, buffer_len, m_StartFrequency) / buffer_len;
     TYPEREAL stop_det = FourierTransformSub(buffer, buffer_len, m_StopFrequency) / buffer_len;
    //printf("start_det=%.2f stop_det=%.2f\n", start_det, stop_det);
     if (start_det > threshold)
         return START;
     if (stop_det > threshold)
         return STOP;
     return IMAGE;
}

/* detect start position from phasing line. 
   using 7% of the image (image should have 5% black 95% white)
   wide of a ^ shaped wedge, find positon it fits to the minimum.

   This isn't very fast, but only is used for phasing lines */
int FaxDecoder::FaxPhasingLinePosition(u1_t *image, int imagewidth)
{
    int n = imagewidth * .07;
    int i;
    int mintotal = -1, min = 0;
    for(i = 0; i<imagewidth; i++) {
        int total = 0, j;
        for(j = 0; j<n; j++)
            total += (n/2-abs(j-n/2))*(255-image[(i+j)%imagewidth]);
        if(total < mintotal || mintotal == -1) {
            mintotal = total;
            min = i;
        }
    }

    return (min+n/2) % imagewidth;
}

/* decode a single line of fax data from buffer placing it in image pointer
   buffer should contain m_SamplesPerSec_nom*60.0/m_lpm*colors bytes
   image will contain imagewidth*colors bytes
*/
void FaxDecoder::DecodeImageLine(u1_t* buffer, int buffer_len, u1_t *image)
{
    //int n = m_SamplesPerSec_nom*60.0/m_lpm;
    int spl = m_SamplesPerLine;

    if (buffer_len != spl) {
        printf("m_SamplesPerSec_nom=%.1f buffer_len=%d spl=%d\n", m_SamplesPerSec_nom, buffer_len, spl);
        panic("DecodeImageLine requires specific buffer length");
    }

    int i, c;

    #if 0
        int stats[3];
        stats[0] = stats[1] = stats[2] = 0;
        for (i = 0; i < spl; i++) {
            if (buffer[i] == 0) stats[0]++;
            else
            if (buffer[i] == 255) stats[2]++;
            else {
                stats[1]++;
                //printf("%3d@%d\n", buffer[i], i);
            }
        }
        printf("%4d: %4d | %4d | %4d\n", spl, stats[0], stats[1], stats[2]);
    #endif

    for (i = 0; i < m_imagewidth; i++) {
        int pixel;
        
        int firstsample = spl * i/m_imagewidth;
        int lastsample = spl * (i+1)/m_imagewidth - 1;
        
        int pixelSamples = 0, sample = firstsample;
        pixel = 0;

        do {
            pixel += buffer[sample];
            pixelSamples++;
        } while (sample++ < lastsample);

        pixel /= pixelSamples;
        if (0 && i < 512) {
            float p = ((float) pixel)/255.0*2.0 - 1.0;
            p *= 1.6;
            if (p > 1.0) p = 1.0; else if (p < -1.0) p = -1.0;
            p = (p + 1.0)/2.0;
            image[i] = p*255;
        } else {
            image[i] = pixel;
        }
        //if (i < 12) real_printf("%3d|%3d ", pixel, image[i]);
    }
    //real_printf("\n");

    NextTask("DecodeImageLine 1");
    ext_send_msg_data(m_rx_chan, false, FAX_MSG_DRAW, image, m_imagewidth);
    FileWrite(image, m_imagewidth);
    NextTask("DecodeImageLine 2");
}

void FaxDecoder::InitializeImage()
{
    height = m_imgsize / 2 / m_SamplesPerSec_nom / 60.0 * m_lpm;
    imgpos = 0;

    if(height == 0) /* for unknown size, start out at 256 */
        height = 256;

    FreeImage();
    //printf("InitializeImage h=%d\n", height);
    m_imgdata = (u1_t*)malloc(m_imagewidth*height*m_imagecolors);

    m_imageline = 0;
    lasttype = IMAGE;
    typecount = 0;

    gotstart = false;
}

void FaxDecoder::FreeImage()
{
     free(m_imgdata);
     m_imageline = 0;
}

void FaxDecoder::CloseInput()
{
}

void FaxDecoder::SetupBuffers()
{
    // initial approx sps to set samplesPerMin/Line
    UpdateSampleRate();
    
    double samplesPerMin = m_SamplesPerSec_nom * 60.0;
    m_SamplesPerLine = samplesPerMin / m_lpm;
    m_BytesPerLine = m_SamplesPerLine * 2;
    
    printf("FAX rx%d SamplesPerSec=%.3f/%.0f lpm=%d SamplesPerLine=%d\n",
        m_rx_chan, m_SamplesPerSec_frac, m_SamplesPerSec_nom, m_lpm, m_SamplesPerLine);
    
    samples = new s2_t[m_SamplesPerLine];
    m_samp_idx = 0;
    m_fi = 0;
    data = new u1_t[m_SamplesPerLine];
    data_i = new int[m_SamplesPerLine];
    datadouble = new double[m_SamplesPerLine];

    phasingPos = new int[m_phasingLines];
    phasingLinesLeft = phasingSkipData = phasingSkippedData = 0;

    // we reset at start and stop, so if already offset phasing will follow
    if(m_offset)
        phasingLinesLeft = m_phasingLines;
}

void FaxDecoder::CleanUpBuffers()
{
     delete [] samples;
     delete [] data;
     delete [] datadouble;
     delete [] phasingPos;
}

bool FaxDecoder::Configure(int rx_chan, int imagewidth, int BitsPerPixel, int carrier,
                           int deviation, enum firfilter::Bandwidth bandwidth,
                           double minus_saturation_threshold,
                           bool bSkipHeaderDetection, bool bIncludeHeadersInImages,
                           bool reset)
{
    m_rx_chan = rx_chan;
    m_BitsPerPixel = BitsPerPixel;
    m_carrier = carrier;
    m_deviation = deviation;
    m_minus_saturation_threshold = minus_saturation_threshold;
    m_bSkipHeaderDetection = bSkipHeaderDetection;
    m_bIncludeHeadersInImages = bIncludeHeadersInImages;
    printf("FAX Configure rx_chan=%d car=%.3f dev=%.3f\n", rx_chan, m_carrier, m_deviation);
    
    m_imagecolors = 1;

    /* TODO: add options? */
    m_lpm = 120;
    m_bFM = true;
    m_StartFrequency = 300;
    m_StopFrequency = 450;
    m_StartLength = 5;
    m_StopLength = 5;
    m_phasingLines = 40;
    m_offset = 0;
    m_imgsize = 0;

    firfilters[0] = firfilter(bandwidth);
    firfilters[1] = firfilter(bandwidth);

    if (reset) {
        CleanUpBuffers();
        CloseInput();
        SetupBuffers();
    }

    /* must reset if image width changes */
    if(m_imagewidth != imagewidth || reset) {
        m_imagewidth = imagewidth;
        InitializeImage();
    }
    
    m_bEndDecoding = false;
    return true;
}

// SECURITY:
// Little bit of a security hole: Can look at previously saved fax images by downloading the fixed filename.
// Not a big deal really.

void FaxDecoder::FileOpen()
{
    FileClose();
    asprintf(&m_fn, "/root/kiwi.config/fax.ch%d.pgm", m_rx_chan);
    m_file = pgm_file_open(m_fn, &m_offset, m_imagewidth, 0, 255);

    if (m_file <= 0) {
        printf("FAX rx%d open FAILED %s\n", m_rx_chan, m_fn);
    } else {
        printf("FAX rx%d open %s\n", m_rx_chan, m_fn);
    }
}

void FaxDecoder::FileWrite(u1_t *data, int datalen)
{
    if (m_file <= 0) return;
    //printf("len=%d m_fax_line=%d\n", datalen, m_fax_line);
    fwrite(data, datalen, 1, m_file);
    m_fax_line++;
    ext_send_msg(m_rx_chan, false, "EXT fax_record_line=%d", m_fax_line);
}

void FaxDecoder::FileClose()
{
    if (m_file <= 0) return;
    
    fflush(m_file);
    pgm_file_height(m_file, m_offset, m_fax_line);
    fclose(m_file);
    printf("FAX rx%d %s wrote %d lines\n", m_rx_chan, m_fn, m_fax_line);
    if (m_fn) free(m_fn); m_fn = NULL;
    m_file = NULL;
    m_fax_line = 0;
}

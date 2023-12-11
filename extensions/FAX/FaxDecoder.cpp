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

#include "types.h"
#include "FaxDecoder.h"
#include "ext.h"
#include "mem.h"
#include "misc.h"
#include "debug.h"

#include <math.h>

#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

//#define FAX_PRINTF
#ifdef FAX_PRINTF
	#define faxprintf(fmt, ...) \
		rcprintf(m_rx_chan, fmt, ## __VA_ARGS__)
#else
	#define faxprintf(fmt, ...)
#endif

FaxDecoder m_FaxDecoder[MAX_RX_CHANS];

/* Note: the decoding algorithms are adapted from yahfax (on sourceforge)
   which was an improved adaptation of hamfax. */

static TYPEREAL apply_firfilter(FaxDecoder::firfilter *filter, TYPEREAL sample)
{
// Narrow, middle and wide fir low pass filter from ACfax
    #define TAP_COUNT 17
    const int tap_count = TAP_COUNT;
    const TYPEREAL lpfcoeff[3][TAP_COUNT] = {
          { -7, -18, -15,  11,  56, 116, 177, 223, 240, 223, 177, 116,  56,  11, -15, -18,  -7},
          {  0, -18, -38, -39,   0,  83, 191, 284, 320, 284, 191,  83,   0, -39, -38, -18,   0},
          {  6,  20,   7, -42, -74, -12, 159, 353, 440, 353, 159, -12, -74, -42,   7,  20,   6}
    };

    const TYPEREAL *c = lpfcoeff[filter->bandwidth];
    const TYPEREAL *c_end = lpfcoeff[filter->bandwidth] + ((sizeof *lpfcoeff) / (sizeof **lpfcoeff));
    TYPEREAL* const b_begin = filter->buffer;
    TYPEREAL* const b_end = filter->buffer + tap_count;
    TYPEREAL sum = 0;
    
    TYPEREAL *current = filter->buffer + filter->current;
    // replace oldest value with current
    *current = sample;
    
    // convolution
    while (current != b_end)
        sum += (*current++) * (*c++);
    current = b_begin;
    while (c != c_end)
        sum += (*current++) * (*c++);
    
    // point again to oldest value
    if (--current < b_begin)
        current = b_end-1;
    
    filter->current = current - filter->buffer;
    return sum;
}

void FaxDecoder::UpdateSampleRate()
{
    m_SamplesPerSec_frac = ext_update_get_sample_rateHz(m_rx_chan);
    m_SamplesPerSec_nom = snd_rate;
    m_SampleRateRatio = m_SamplesPerSec_frac / m_SamplesPerSec_nom;
}

/* perform fourier transform at a specific frequency to look for start/stop */
TYPEREAL FaxDecoder::FourierTransformSub(u1_t* buffer, int samps_per_line, int buffer_len, int freq)
{
    int i, n;
    TYPEREAL k = -2 * M_PI * freq * 60.0 / m_lpm / samps_per_line;
    TYPEREAL retr = 0, reti = 0;
    
    for (n=0, i=1; n < buffer_len; n++, i++) {
        retr += buffer[n]*MCOS(k*n);
        reti += buffer[n]*MSIN(k*n);

        if ((i & 0xf) == 0)
            NextTaskFast("FourierTransformSub");
    }
    return MSQRT(retr*retr + reti*reti);
}

/* see if the fourier transform at the start and stop frequencies reveils header */
FaxDecoder::Header FaxDecoder::DetectLineType(u1_t* buffer, int samps_per_line, int buffer_len)
{
     const int threshold = 5; /* 5 is pretty arbitrary but works in practice even with lots of noise */
     TYPEREAL start_det = FourierTransformSub(buffer, samps_per_line, buffer_len, m_Start_IOC576_Frequency) / buffer_len;
     TYPEREAL stop_det = FourierTransformSub(buffer, samps_per_line, buffer_len, m_StopFrequency) / buffer_len;
    //faxprintf("FAX start_det=%.2f stop_det=%.2f\n", start_det, stop_det);

     if (start_det > threshold)
         return START;
     if (stop_det > threshold)
         return STOP;
     return IMAGE;
}

/* detect start position from phasing line
   using 7% of the image (image should have 5% black 95% white)
   with a ^ shaped wedge, find positon it fits to the minimum.

   This isn't very fast, but only is used for phasing lines */
int FaxDecoder::FaxPhasingLinePosition(u1_t *image, int samplesPerLine)
{
    int n = samplesPerLine * .07;
    int i;
    int mintotal = -1, min = 0;
    
    // reduce computational load by making loops below sparse 
    #define PIXEL_RESOLUTION 4
    int sampsIncr = samplesPerLine/m_imagewidth * PIXEL_RESOLUTION;
    
    //u4_t start = timer_us();
    for (i = 0; i<samplesPerLine; i += sampsIncr) {
        int total = 0, j;
        for (j = 0; j<n; j += PIXEL_RESOLUTION)
            total += (n/2 - abs(j-n/2)) * (255 - image[(i+j) % samplesPerLine]);
        if (total < mintotal || mintotal == -1) {
            mintotal = total;
            min = i;
        }
        NextTaskFast("FaxPhasingLinePosition");
    }

    //faxprintf("FAX PhasingLinePosition iter=%d usec=%u\n", samplesPerLine/sampsIncr*n/PIXEL_RESOLUTION, timer_us()-start);
    return (samplesPerLine? ((min+n/2) % samplesPerLine) : 0);
}

bool FaxDecoder::DecodeFaxLine()
{
    const int phasingSkipLines = 2;
    
    DemodulateData();

    enum Header type;
    if (m_bSkipHeaderDetection) {
        type = IMAGE;
    } else {
        // processing all the line samples for low LPM is too expensive
        int buffer_len = MIN(m_SamplesPerLine, 3000);
        type = DetectLineType(m_demod_data, m_SamplesPerLine, buffer_len);
        NextTaskFast("DetectLineType");
    }

    /* accumulate how many start or stop lines we are getting */
    if (type == lasttype && type != IMAGE)
        typecount++;
    else {
        typecount--; /* can deal with noisy input if we had a miss rather than reset here */
        if (typecount < 0)
            typecount = 0;
    }
    lasttype = type;

    if (type != IMAGE) { /* if type is start or stop */
        /* require 4 less lines than there really are to handle
           noise and also misalignment on first and last lines */
        const int leewaylines = 4;

        faxprintf("FAX L%d %s cnt=%d prepare=%d\n", m_imageline, (type == START)? "START":"STOP", typecount,
            typecount == m_StartStopLength*m_lpm/60.0 - leewaylines);
        if (typecount == m_StartStopLength*m_lpm/60.0 - leewaylines) {
            if (type == START /* && m_imageline < 100 */) {
                /* prepare for phasing */
                /* image start detected, reset image at 0 lines  */
                if (!m_bIncludeHeadersInImages) {
                    m_imageline = 0;
                    imgpos = 0;
                    m_lineIncrAcc = 0;
                }

                phasingLinesLeft = m_phasingLines;
                phasingSkipData = 0;
                have_phasing = false;
                if (m_autostopped) {
                    ext_send_msg(m_rx_chan, false, "EXT fax_autostopped=0");
                    m_autostopped = false;
                    faxprintf("FAX L%d AUTOSTOPPED=0\n", m_imageline);
                }
            } else {
                // type == STOP
                if (m_autostop) {
                    ext_send_msg(m_rx_chan, false, "EXT fax_autostopped=1");
                    m_autostopped = true;
                    faxprintf("FAX L%d AUTOSTOPPED=1\n", m_imageline);
                }
            }
        }
    }

    /* throw away first 2 lines of phasing because we are not sure
       if they are misaligned start lines */
    if (m_use_phasing && phasingLinesLeft > 0 && phasingLinesLeft <= m_phasingLines - phasingSkipLines) {
        phasingPos[phasingLinesLeft-1] = FaxPhasingLinePosition(m_demod_data, m_SamplesPerLine);
        faxprintf("FAX L%d phasingPos[%d]=%d\n", m_imageline, phasingLinesLeft-1, phasingPos[phasingLinesLeft-1]);
    }

    if (m_use_phasing && type == IMAGE && phasingLinesLeft >= -phasingSkipLines) {
        if (--phasingLinesLeft == 0) {  /* decrement each phasing line */
        
            // DetectLineType() sometimes returns START during IMAGEs with sparse black text on a white background.
            // If the extension was started in the middle of a fax transmission this can result in a false phasing align.
            // Filter that out by looking at the 10%/90% distribution width of the phasing data.
            int ten_pct = 10, ninety_pct = 90;
            phasingSkipData = median_i(phasingPos, m_phasingLines - phasingSkipLines, &ten_pct, &ninety_pct);
            rcprintf(m_rx_chan, "FAX L%d SET phasingSkipData=%d 10%%=%d 90%%=%d\n", m_imageline, phasingSkipData, ten_pct, ninety_pct);
            if ((ninety_pct - ten_pct) > m_SamplesPerLine/6) {
                faxprintf("FAX L%d BAD phasingSkipData\n", m_imageline);
                phasingSkipData = 0;
            }
        }
    }

    /* go past the phasing lines we are skipping to make sure we are in the image */
    if (m_bIncludeHeadersInImages || !m_use_phasing || (type == IMAGE && phasingLinesLeft < -phasingSkipLines)) {
        if (m_imageline >= height) {
            height *= 2;
            m_imgdata = (u1_t*) kiwi_irealloc("DecodeFaxLine", m_imgdata, m_imagewidth*height*m_imagecolors);
        }

        /*
            if (m_imageline == 10) {
                m_autostopped = true;
                ext_send_msg(m_rx_chan, false, "EXT fax_autostopped=1");
                faxprintf("FAX L%d TEST AUTOSTOPPED=1\n", m_imageline);
            }
            if (m_imageline == 20) {
                m_autostopped = false;
                ext_send_msg(m_rx_chan, false, "EXT fax_autostopped=0");
                faxprintf("FAX L%d TEST AUTOSTOPPED=0\n", m_imageline);
            }
        */

        if (!m_autostopped)
            DecodeImageLine(m_demod_data, m_SamplesPerLine, m_imgdata+imgpos);
        
        phasingSkipData %= m_SamplesPerLine;
        if (phasingSkipData && m_use_phasing && !have_phasing) {
            m_skip = phasingSkipData;
            have_phasing = true;
            ext_send_msg(m_rx_chan, false, "EXT fax_phased");
            faxprintf("FAX L%d USE phasingSkipData=%d\n", m_imageline, phasingSkipData);
        }
        
        imgpos += m_imagewidth*m_imagecolors;
        m_imageline++;
    }

    return true;
}

void FaxDecoder::DemodulateData()
{
    double f=0, ph_inc;
    int i, j, pixel;

    // update sps for mixers
    UpdateSampleRate();

    if (m_SamplesPerSec_frac != m_SamplesPerSec_frac_prev) {
        ext_send_msg(m_rx_chan, false, "EXT fax_sps_changed");
        //if (m_rx_chan == 0) faxprintf("FAX sps %.12e %.12e diff=%.3e\n", m_rx_chan,
        //    m_SamplesPerSec_frac, m_SamplesPerSec_frac_prev, m_SamplesPerSec_frac - m_SamplesPerSec_frac_prev);
        m_SamplesPerSec_frac_prev = m_SamplesPerSec_frac;
    }
    
    ph_inc = m_carrier/m_SamplesPerSec_frac;

    NextTaskFast("DemodulateData start");
    TYPEREAL scale = -1.3 * (m_SamplesPerSec_nom/m_deviation/8);
    for (i=0, j=1; i < m_SamplesPerLine; i++, j++) {

        if ((j & 0xf) == 0)
            NextTaskFast("DemodulateData loop");

        // mix to carrier so start/stop/black/white freqs will be relative to zero

        static TYPEREAL normalize_sample = 1.0/32768.0;
        TYPEREAL samp = m_samples[i] * normalize_sample;      // -1..0..1

        TYPEREAL Icur = apply_firfilter(firfilters+0, samp*MCOS(K_2PI*f));
        TYPEREAL Qcur = apply_firfilter(firfilters+1, samp*MSIN(K_2PI*f));

        f += ph_inc;
        if (f > 1.0) f -= 1.0;      // keep bounded
        
        TYPEREAL mag = MSQRT(Qcur*Qcur + Icur*Icur);
        Icur /= mag;
        Qcur /= mag;

        TYPEREAL x = (Icur*(Qcur-Qprev) - Qcur*(Icur-Iprev)) * scale;
        x = x/2.0 + 0.5;
        pixel = x*255.0;
        pixel = (pixel < 0)? 0 : ((pixel > 255)? 255 : pixel);   // clamp
        m_demod_data[i] = (u1_t) pixel;

        Iprev = Icur;
        Qprev = Qcur;
    }
    NextTaskFast("DemodulateData end");
}

/*
    Decode a single line of fax data from buffer placing it in image pointer.
    Buffer should contain m_SamplesPerSec_nom*60.0/m_lpm*colors bytes.
    Image will contain imagewidth*colors bytes.
*/
void FaxDecoder::DecodeImageLine(u1_t* buffer, int buffer_len, u1_t *image)
{
    int pixel;
    //int n = m_SamplesPerSec_nom*60.0/m_lpm;
    int spl = m_SamplesPerLine;

    if (buffer_len != spl) {
        faxprintf("FAX m_SamplesPerSec_nom=%.1f buffer_len=%d spl=%d\n", m_SamplesPerSec_nom, buffer_len, spl);
        panic("DecodeImageLine requires specific buffer length");
    }

    int i, j;

    NextTaskFast("DecodeImageLine start");
    
    for (i = 0; i < m_imagewidth; i++) {
        
        int firstsample = spl * i/m_imagewidth;
        int lastsample = spl * (i+1)/m_imagewidth - 1;
        
        int pixelSamples = 0, sample = firstsample;
        pixel = 0;

        do {
            pixel += buffer[sample];
            pixelSamples++;
        } while (sample++ < lastsample);

        pixel /= pixelSamples;
        image[i] = pixel;

        if ((i & 0xff) == 0)
            NextTaskFast("DecodeImageLine loop");
    }
    
    bool emit = false;
    if (m_debug) {
        emit = true;
    } else {
        double m_lineNextBlend, m_linePrevBlend;
        if (m_lineIncrAcc >= 1.0) {
            m_lineIncrAcc -= 1.0;     // keep bounded
            if (m_imageline != 0 && m_lineIncrAcc != 0) {
                m_lineNextBlend = m_lineIncrAcc/m_lineBlend;
                m_linePrevBlend = 1.0 - m_lineNextBlend;
                j = -m_imagewidth;
                for (i = 0; i < m_imagewidth; i++, j++) {
                    pixel = roundf((TYPEREAL)image[i] * m_lineNextBlend + (TYPEREAL)image[j] * m_linePrevBlend);
                    pixel = MIN(255, pixel);
                    m_outImage[i] = pixel;
                }
                m_lineBlend = m_lineIncrFrac;
            }
            emit = true;
        } else {
            m_lineBlend += m_lineIncrFrac;
        }
        m_lineIncrAcc += m_lineIncrFrac;
    }

    if (emit) {
        //real_printf("%.2f ", m_lineNextBlend); fflush(stdout);
        NextTaskFast("DecodeImageLine send");
        ext_send_msg_data(m_rx_chan, false, FAX_MSG_DRAW, m_debug? image : m_outImage, m_imagewidth);
        FileWrite(m_outImage, m_imagewidth);
    }
    NextTaskFast("DecodeImageLine end");
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
        faxprintf("FAX m_skip %d skip %d\n", m_skip, skip);
        m_skip -= skip;
    }

    while (i < nsamps) {
        for (; i < nsamps && m_samp_idx < m_SamplesPerLine;) {
            m_samples[m_samp_idx] = samps[i];
            m_samp_idx++;
            m_fi += m_SampleRateRatio;
            i = trunc(m_fi);
        }
        
        if (m_samp_idx == m_SamplesPerLine) {
		    NextTaskFast("ProcessSamples 1");
            if (ev_dump) evLatency(EC_TRIG_ACCUM_ON, EV_EXT, ev_dump, "FAX", evprintf("rx%d fax task cycle time", m_rx_chan));
            DecodeFaxLine();
            if (ev_dump) evLatency(EC_TRIG_ACCUM_OFF, EV_EXT, ev_dump, "FAX", evprintf("rx%d fax task cycle time", m_rx_chan));
		    NextTaskFast("ProcessSamples 2");
            m_samp_idx = 0;
        }

        if ((i & 0xff) == 0)
            NextTaskFast("ProcessSamples loop");
    }
    m_fi -= nsamps;     // keep bounded
}

void FaxDecoder::InitializeImage()
{
    height = m_imgsize / 2 / m_SamplesPerSec_nom / 60.0 * m_lpm;
    imgpos = 0;

    if (height == 0) /* for unknown size, start out at 256 */
        height = 256;

    FreeImage();
    m_imgdata  = (u1_t*) kiwi_imalloc("InitializeImage", m_imagewidth*height*m_imagecolors);
    m_outImage = (u1_t*) kiwi_imalloc("InitializeImage", m_imagewidth*m_imagecolors);

    lasttype = IMAGE;
    typecount = 0;

    m_autostopped = false;
}

bool FaxDecoder::Configure(int rx_chan, int lpm, int imagewidth, int BitsPerPixel, int carrier,
                           int deviation, enum firfilter::Bandwidth bandwidth,
                           double minus_saturation_threshold,
                           bool bIncludeHeadersInImages, bool use_phasing, bool autostop,
                           int debug, bool reset)
{
    m_rx_chan = rx_chan;
    m_BitsPerPixel = BitsPerPixel;
    m_carrier = carrier;
    m_deviation = deviation;
    m_minus_saturation_threshold = minus_saturation_threshold;
    m_bIncludeHeadersInImages = bIncludeHeadersInImages;
    m_use_phasing = use_phasing;
    m_autostop = autostop;
    m_bSkipHeaderDetection = (m_use_phasing || m_autostop)? false : true;
    
    m_imagecolors = 1;

    m_lpm = lpm;
    m_bFM = true;
    m_Start_IOC576_Frequency = 300;
    m_Start_IOC288_Frequency = 675;
    m_StopFrequency = 450;
    m_StartStopLength = 5;
    m_phasingLines = 40;
    m_offset = 0;
    m_imgsize = 0;

    firfilters[0] = firfilter(bandwidth);
    firfilters[1] = firfilter(bandwidth);

    if (reset) {
        CleanUpBuffers();
        SetupBuffers();
    }

    /* must reset if image width changes */
    if (m_imagewidth != imagewidth || reset) {
        m_imagewidth = imagewidth;
        InitializeImage();
    }
    
    m_lineIncrFrac = m_imagewidth / (M_PI * 576);
    m_bEndDecoding = false;
    m_debug = debug;
    rcprintf(rx_chan, "FAX Configure lpm=%d car=%.3f dev=%.3f debug=%d\n", m_lpm, m_carrier, m_deviation, m_debug);

    return true;
}

void FaxDecoder::SetupBuffers()
{
    // initial approx sps to set samplesPerMin/Line
    UpdateSampleRate();
    
    double samplesPerMin = m_SamplesPerSec_nom * 60.0;
    m_SamplesPerLine = samplesPerMin / m_lpm;
    m_BytesPerLine = m_SamplesPerLine * 2;
    
    faxprintf("FAX SamplesPerSec=%.3f/%.0f lpm=%d SamplesPerLine=%d\n",
        m_SamplesPerSec_frac, m_SamplesPerSec_nom, m_lpm, m_SamplesPerLine);
    
    m_samples = new s2_t[m_SamplesPerLine];
    m_samp_idx = 0;
    m_fi = 0;
    m_demod_data = new u1_t[m_SamplesPerLine];

    phasingPos = new int[m_phasingLines];
    phasingLinesLeft = phasingSkipData = 0;
    have_phasing = false;

    // we reset at start and stop, so if already offset phasing will follow
    if (m_offset)
        phasingLinesLeft = m_phasingLines;
}

void FaxDecoder::FreeImage()
{
    kiwi_ifree(m_imgdata, "FreeImage");
    kiwi_ifree(m_outImage, "FreeImage");
    m_imageline = 0;
    m_lineIncrAcc = 0;
}

void FaxDecoder::CleanUpBuffers()
{
     delete [] m_samples;
     delete [] m_demod_data;
     delete [] phasingPos;
}

// SECURITY:
// Little bit of a security hole: Can look at previously saved fax images by downloading the fixed filename.
// Not a big deal really.

void FaxDecoder::FileOpen()
{
    FileClose();
    asprintf(&m_fn, DIR_DATA "/fax.ch%d.pgm", m_rx_chan);
    m_file = pgm_file_open(m_fn, &m_offset, m_imagewidth, 0, 255);

    if (m_file == NULL) {
        faxprintf("FAX open FAILED %s\n", m_fn);
    } else {
        faxprintf("FAX open %s\n", m_fn);
    }
}

void FaxDecoder::FileWrite(u1_t *data, int datalen)
{
    if (m_file == NULL) return;
    //faxprintf("len=%d m_fax_line=%d\n", datalen, m_fax_line);
    fwrite(data, datalen, 1, m_file);
    m_fax_line++;
    ext_send_msg(m_rx_chan, false, "EXT fax_record_line=%d", m_fax_line);
}

void FaxDecoder::FileClose()
{
    if (m_file == NULL) return;
    
    fflush(m_file);
    pgm_file_height(m_file, m_offset, m_fax_line);
    fclose(m_file);
    faxprintf("FAX %s wrote %d lines\n", m_fn, m_fax_line);
    if (m_fn) kiwi_asfree(m_fn, "FileClose"); m_fn = NULL;
    m_file = NULL;
    m_fax_line = 0;
}

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
#include "kiwi.h"
#include "datatypes.h"

#define FAX_MSG_CLEAR   255
#define FAX_MSG_DRAW    254
#define FAX_MSG_SCOPE   0       // channel 0, 1, 2, 3

class FaxDecoder
{
public:

    struct firfilter {
        enum Bandwidth {NARROW, MIDDLE, WIDE};
        firfilter() {}
    firfilter(enum Bandwidth b) : bandwidth(b), current(0)
        { for(int i=0; i<17; i++) buffer[i] = 0; }
        enum Bandwidth bandwidth;
        TYPEREAL buffer[17];
        int current;
    };

    FaxDecoder() {}
        
    ~FaxDecoder() { FreeImage(); CleanUpBuffers(); }

    bool Configure(int rx_chan, int imagewidth, int BitsPerPixel, int carrier,
                   int deviation, enum firfilter::Bandwidth bandwidth,
                   double minus_saturation_threshold,
                   bool bSkipHeaderDetection, bool bIncludeHeadersInImages, bool reset);

    void ProcessSamples(s2_t *samps, int nsamps, float shift);
    void FileOpen();
    void FileWrite(u1_t *data, int datalen);
    void FileClose();
    
    bool DecodeFaxFromFilename();
    bool DecodeFaxFromDSP();
    bool DecodeFaxFromPortAudio();

    void InitializeImage();
    void FreeImage();

    u1_t *m_imgdata;
    int m_imageline;
    int m_imagewidth;
    double m_minus_saturation_threshold;
    double *datadouble;

private:
    bool DecodeFax();
    void DemodulateData();

    void CloseInput();
    void SetupBuffers();
    void CleanUpBuffers();

    int m_rx_chan;
    char *m_fn;
    //int m_file;
    FILE *m_file;
    int m_fax_line;
    bool m_bEndDecoding;        /* flag to end decoding thread */
    double m_SamplesPerSec_nom;
    double m_SamplesPerSec_frac, m_SamplesPerSec_frac_prev;
    double m_SampleRateRatio, m_fi;
    int m_SamplesPerLine, m_skip;
    int m_BytesPerLine;

    /* internal state machine */
    TYPEREAL Iprev, Qprev;
    s2_t *samples;
    int m_samp_idx;
    u1_t *data;
    int *data_i;

    enum Header {IMAGE, START, STOP};

    TYPEREAL FourierTransformSub(u1_t* buffer, int buffer_len, int freq);
    Header DetectLineType(u1_t* buffer, int buffer_len);
    void DecodeImageLine(u1_t* buffer, int buffer_len, u1_t *image);
    int FaxPhasingLinePosition(u1_t *image, int imagewidth);
    void UpdateSampleRate();

    /* fax settings */
    int m_BitsPerPixel;
    double m_carrier, m_deviation;
    struct firfilter firfilters[2];
    bool m_bSkipHeaderDetection;
    bool m_bIncludeHeadersInImages;
    int m_imagecolors;
    int m_lpm;
    bool m_bFM;
    int m_StartFrequency, m_StopFrequency;
    int m_StartLength, m_StopLength;
    int m_phasingLines;
    int m_offset;
    int m_imgsize;

    int height, imgpos;

    Header lasttype;
    int typecount;

    bool gotstart;

    int *phasingPos;
    int phasingLinesLeft, phasingSkipData, phasingSkippedData;
};

extern FaxDecoder m_FaxDecoder[MAX_RX_CHANS];

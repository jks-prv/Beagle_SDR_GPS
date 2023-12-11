// ----------------------------------------------------------------------------
// Copyright (C) 2014
//              David Freese, W1HKJ
//
// This file is part of fldigi
//
// fldigi is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 3 of the License, or
// (at your option) any later version.
//
// fldigi is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
// ----------------------------------------------------------------------------

// Copyright (c) 2023 John Seamons, ZL4VO/KF6VO

#pragma once

#include "types.h"
#include "datatypes.h"
#include "rx_sound.h"
#include "AudioResample.h"      // from DRM/dream/resample

#include <fftw3.h>

#define RSID_SAMPLE_RATE    11025.0

#define RSID_FFT_SAMPLES 	512
#define RSID_FFT_SIZE       1024
#define RSID_ARRAY_SIZE	 	(RSID_FFT_SIZE * 2)
#define RSID_BUFFER_SIZE	(RSID_ARRAY_SIZE * 2)

#define RSID_NSYMBOLS    15
#define RSID_NTIMES      (RSID_NSYMBOLS * 2)
#define RSID_PRECISION   2.7 // detected frequency precision in Hz

// each rsid symbol has a duration equal to 1024 samples at 11025 Hz sample rate
#define RSID_SYMLEN		(1024.0 / RSID_SAMPLE_RATE) // 0.09288 // duration of each rsid symbol

typedef float rs_fft_type;
typedef std::complex<rs_fft_type> rs_cpx_type;

typedef intptr_t trx_mode;
struct RSIDs { unsigned short rs; trx_mode mode; const char* name; };

inline double hamming(double x) {
    return 0.54 - 0.46 * cos(2 * M_PI * x);
}

#define RSID_FFTW_COMPLEX fftwf_complex
#define RSID_FFTW_MALLOC fftwf_malloc
#define RSID_FFTW_ALLOC_REAL fftwf_alloc_real
#define RSID_FFTW_ALLOC_COMPLEX fftwf_alloc_complex
#define RSID_FFTW_FREE fftwf_free
#define RSID_FFTW_PLAN fftwf_plan
#define RSID_FFTW_PLAN_DFT_1D fftwf_plan_dft_1d
#define RSID_FFTW_PLAN_DFT_R2C_1D fftwf_plan_dft_r2c_1d
#define RSID_FFTW_DESTROY_PLAN fftwf_destroy_plan
#define RSID_FFTW_EXECUTE fftwf_execute
#define RSID_FFTW_NORM(c) (c[I]*c[I] + c[Q]*c[Q])

class cRsId {

    public:
        cRsId();
        ~cRsId();
        
        char rsidName[32];
        double rsidFreq;
        time_t rsidTime;
        
        bool WideSearch;
        
        void init(int _rx_chan, int fixedInputSize);
        void receive(int len, TYPEMONO16 *samps);

    protected:
        enum { ERRS_LOW=0, ERRS_MED=1, ERRS_HIGH=2 };

    private:
        int rx_chan;
        snd_t *snd;
        int tr;

        // Table of precalculated Reed Solomon symbols
        unsigned char   *pCodes1;
        unsigned char   *pCodes2;

        bool found1;
        bool found2;

        static const RSIDs rsid_ids_1[];
        static const int rsid_ids_size1;
        static const int Squares[];
        static const int indices[];

        static const RSIDs rsid_ids_2[];
        static const int rsid_ids_size2;

	    double rsid_secondary_time_out;
	    int     hamming_resolution;

        // Span of FFT bins, in which the RSID will be searched for
        int		nBinLow;
        int		nBinHigh;

        float			aInputSamples[RSID_BUFFER_SIZE];
        rs_fft_type		fftwindow[RSID_ARRAY_SIZE];
        rs_cpx_type		aFFTcmplx[RSID_ARRAY_SIZE];
        rs_fft_type		aFFTAmpl[RSID_FFT_SIZE];

        bool	bPrevTimeSliceValid;
        int		iPrevDistance;
        int		iPrevBin;
        int		iPrevSymbol;

        int		fft_buckets[RSID_NTIMES][RSID_FFT_SIZE];

        bool	bPrevTimeSliceValid2;
        int		iPrevDistance2;
        int		iPrevBin2;
        int		iPrevSymbol2;
        
        // resampler
        CAudioResample *ResampleObj;
        CVector<_REAL> vecTempResBufIn;
        CVector<_REAL> vecTempResBufOut;
	    int inptr;

        RSID_FFTW_COMPLEX *ComplexIO;
        RSID_FFTW_PLAN ComplexFFT;

        void    reset();
	    void	search(void);
        void    Encode(int code, unsigned char *rsid);
        void	CalculateBuckets(const rs_fft_type *pSpectrum, int iBegin, int iEnd);
        inline  int HammingDistance(int iBucket, unsigned char *p2);
        bool	search_amp(int &bin_out, int &symbol_out, unsigned char *pcode_table );
        void	apply(int iBin, int iSymbol, int extended );
};

extern cRsId m_RsId[MAX_RX_CHANS];

// ----------------------------------------------------------------------------
//
//	rsid.cxx
//
// Copyright (C) 2008-2012
//		Dave Freese, W1HKJ
// Copyright (C) 2009-2012
//		Stelios Bounanos, M0GLD
// Copyright (C) 2012
//		John Douyere, VK2ETA
//
// This file is part of fldigi.
//
// Fldigi is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Fldigi is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with fldigi.  If not, see <http://www.gnu.org/licenses/>.
// ----------------------------------------------------------------------------

// Copyright (c) 2023 John Seamons, ZL4VO/KF6VO

#include "types.h"
#include "mode.h"
#include "cuteSDR.h"
#include "datatypes.h"
#include "printf.h"
#include "rx_sound.h"
#include "timer.h"
#include "ext.h"

#include "fldigi_complex.h"
#include "fldigi_globals.h"
#include "rsid.h"
#include "rsid_defs.h"

#define RSWINDOW 1
#define unlikely(x) x

const int cRsId::Squares[] = {
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
	0,  2,  4,  6,  8, 10, 12, 14,  9, 11, 13, 15,  1,  3,  5,  7,
	0,  3,  6,  5, 12, 15, 10,  9,  1,  2,  7,  4, 13, 14, 11,  8,
	0,  4,  8, 12,  9, 13,  1,  5, 11, 15,  3,  7,  2,  6, 10, 14,
	0,  5, 10, 15, 13,  8,  7,  2,  3,  6,  9, 12, 14, 11,  4,  1,
	0,  6, 12, 10,  1,  7, 13, 11,  2,  4, 14,  8,  3,  5, 15,  9,
	0,  7, 14,  9,  5,  2, 11, 12, 10, 13,  4,  3, 15,  8,  1,  6,
	0,  8,  9,  1, 11,  3,  2, 10, 15,  7,  6, 14,  4, 12, 13,  5,
	0,  9, 11,  2, 15,  6,  4, 13,  7, 14, 12,  5,  8,  1,  3, 10,
	0, 10, 13,  7,  3,  9, 14,  4,  6, 12, 11,  1,  5, 15,  8,  2,
	0, 11, 15,  4,  7, 12,  8,  3, 14,  5,  1, 10,  9,  2,  6, 13,
	0, 12,  1, 13,  2, 14,  3, 15,  4,  8,  5,  9,  6, 10,  7, 11,
	0, 13,  3, 14,  6, 11,  5,  8, 12,  1, 15,  2, 10,  7,  9,  4,
	0, 14,  5, 11, 10,  4, 15,  1, 13,  3,  8,  6,  7,  9,  2, 12,
	0, 15,  7,  8, 14,  1,  9,  6,  5, 10,  2, 13, 11,  4, 12,  3
};

const int cRsId::indices[] = {
	2, 4, 8, 9, 11, 15, 7, 14, 5, 10, 13, 3
};

cRsId m_RsId[MAX_RX_CHANS];

cRsId::cRsId()
{
    ResampleObj = new CAudioResample();

	if (RSWINDOW) {
		for (int i = 0; i < RSID_ARRAY_SIZE; i++)
		fftwindow[i] = hamming(1.0 * i / RSID_ARRAY_SIZE);
	}

	pCodes1 = new unsigned char[rsid_ids_size1 * RSID_NSYMBOLS];
	memset(pCodes1, 0, sizeof(pCodes1) * sizeof(unsigned char));

	pCodes2 = new unsigned char[rsid_ids_size2 * RSID_NSYMBOLS];
	memset(pCodes2, 0, sizeof(pCodes2) * sizeof(unsigned char));

	// Initialization of assigned mode/submode IDs.
	unsigned char *c;
	for (int i = 0; i < rsid_ids_size1; i++) {
		c = pCodes1 + i * RSID_NSYMBOLS;
		Encode(rsid_ids_1[i].rs, c);
	}

	for (int i = 0; i < rsid_ids_size2; i++) {
		c = pCodes2 + i * RSID_NSYMBOLS;
		Encode(rsid_ids_2[i].rs, c);
	}

    ComplexIO = RSID_FFTW_ALLOC_COMPLEX(RSID_ARRAY_SIZE);
    ComplexFFT = RSID_FFTW_PLAN_DFT_1D(RSID_ARRAY_SIZE, ComplexIO, ComplexIO, FFTW_FORWARD, FFTW_ESTIMATE);
}

cRsId::~cRsId()
{
}

void cRsId::init(int _rx_chan, int fixedInputSize)
{
    rx_chan = _rx_chan;
    snd = &snd_inst[rx_chan];

    float ratio =  RSID_SAMPLE_RATE / ext_update_get_sample_rateHz(rx_chan);
    ResampleObj->Init(fixedInputSize, ratio);
    const int iMaxInputSize = ResampleObj->GetMaxInputSize();
    vecTempResBufIn.Init(iMaxInputSize, (_REAL) 0.0);
    const int iMaxOutputSize = ResampleObj->iOutputBlockSize;
    vecTempResBufOut.Init(iMaxOutputSize, (_REAL) 0.0);
    ResampleObj->Reset();

	nBinLow = 3;
	nBinHigh = RSID_FFT_SIZE - 32; // - RSID_NTIMES - 2
    
    rsidTime = 0;
    WideSearch = true;
	//rcprintf(rx_chan, "RSID: init\n");
    reset();
}

void cRsId::reset()
{
	rsid_secondary_time_out = 0;
	inptr = RSID_FFT_SIZE;
	hamming_resolution = ERRS_MED;
        
	iPrevDistance = iPrevDistance2 = 99;
	bPrevTimeSliceValid = bPrevTimeSliceValid2 = false;
	found1 = found2 = false;

	memset(aInputSamples, 0, RSID_BUFFER_SIZE * sizeof(float));
	memset(aFFTcmplx, 0, RSID_ARRAY_SIZE * sizeof(rs_cpx_type));
	memset(aFFTAmpl, 0, RSID_FFT_SIZE * sizeof(rs_fft_type));
	memset(fft_buckets, 0, RSID_NTIMES * RSID_FFT_SIZE * sizeof(int));
	
	//rcprintf(rx_chan, "RSID: reset\n");
}

void cRsId::Encode(int code, unsigned char *rsid)
{
	rsid[0] = code >> 8;
	rsid[1] = (code >> 4) & 0x0f;
	rsid[2] = code & 0x0f;
	for (int i = 3; i < RSID_NSYMBOLS; i++)
		rsid[i] = 0;
	for (int i = 0; i < 12; i++) {
		for (int j = RSID_NSYMBOLS - 1; j > 0; j--)
			rsid[j] = rsid[j - 1] ^ Squares[(rsid[j] << 4) + indices[i]];
		rsid[0] = Squares[(rsid[0] << 4) + indices[i]];
	}
}

void cRsId::CalculateBuckets(const rs_fft_type *pSpectrum, int iBegin, int iEnd)
{
	rs_fft_type Amp = 0.0, AmpMax = 0.0;
	int iBucketMax = iBegin - 2;
	int j;

	for (int i = iBegin; i < iEnd; i += 2) {
		if (iBucketMax == i - 2) {
			AmpMax = pSpectrum[i];
			iBucketMax = i;
			for (j = i + 2; j < i + RSID_NTIMES + 2; j += 2) {
				Amp = pSpectrum[j];
				if (Amp > AmpMax) {
					AmpMax = Amp;
					iBucketMax = j;
				}
			}
		}
		else {
			j = i + RSID_NTIMES;
			Amp = pSpectrum[j];
			if (Amp > AmpMax) {
				AmpMax    = Amp;
				iBucketMax = j;
			}
		}
		fft_buckets[RSID_NTIMES - 1][i] = (iBucketMax - i) >> 1;
	}
}

inline int cRsId::HammingDistance(int iBucket, unsigned char *p2)
{
	int dist = 0;
	for (int i = 0, j = 1; i < RSID_NSYMBOLS; i++, j += 2) {
		if (fft_buckets[j][iBucket] != p2[i]) {
			++dist;
			if (dist > hamming_resolution)
				break;
		}
	}
	return dist;
}

bool cRsId::search_amp(int &bin_out, int &symbol_out, unsigned char *pcode)
{
	int i, j;
	int iDistanceMin = 1000;  // infinity
	int iDistance    = 1000;
	int iBin         = -1;
	int iSymbol      = -1;

	int tblsize;
	const RSIDs *prsid;

	if (pcode == pCodes1) {
		tblsize = rsid_ids_size1;
		prsid = rsid_ids_1;
	} else {
		tblsize = rsid_ids_size2;
		prsid = rsid_ids_2;
	}

	unsigned char *pc = 0;
	for (i = 0; i < tblsize; i++) {
		pc = pcode + i * RSID_NSYMBOLS;
		for (j = nBinLow; j < nBinHigh - RSID_NTIMES; j++) {
			iDistance = HammingDistance(j, pc);
			if (iDistance < iDistanceMin) {
				iDistanceMin = iDistance;
				iSymbol  	 = prsid[i].rs;
				iBin		 = j;
				if (iDistanceMin == 0) break;
			}
		}
	}

	if (iDistanceMin <= hamming_resolution) {
		symbol_out	= iSymbol;
		bin_out		= iBin;
		return true;
	}

	return false;
}

void cRsId::apply(int iBin, int iSymbol, int extended)
{
	double rsidfreq = 0, currfreq = 0;
	int n, mbin = NUM_MODES;

	int tblsize;
	const RSIDs *p_rsid;

	if (extended) {
		tblsize = rsid_ids_size2;
		p_rsid = rsid_ids_2;
	}
	else {
		tblsize = rsid_ids_size1;
		p_rsid = rsid_ids_1;
	}

	currfreq = ext_get_displayed_freq_kHz(rx_chan) * kHz;
	//rcprintf(rx_chan, "RSID: %.2f bin=%d\n", currfreq, iBin);
	rsidfreq = (iBin + RSID_NSYMBOLS - 0.5) * RSID_SAMPLE_RATE / 2048.0;

	for (n = 0; n < tblsize; n++) {
		if (p_rsid[n].rs == iSymbol) {
			mbin = p_rsid[n].mode;
			break;
		}
	}

	if (mbin == NUM_MODES) {
		char msg[50];
		if (n < tblsize) // RSID known but unimplemented
			snprintf(msg, sizeof(msg), "RSID: %s unimplemented", p_rsid[n].name);
		else // RSID unknown; shouldn't  happen
			snprintf(msg, sizeof(msg), "RSID: code %d unknown", iSymbol);
		rcprintf(rx_chan, "RSID: %s\n", msg);
		return;
	}

	rcprintf(rx_chan, "RSID: %s @ %0.1f Hz\n", p_rsid[n].name, rsidfreq);
	strcpy(rsidName, p_rsid[n].name);
	rsidFreq = rsidfreq;
	rsidTime = utc_time();
}

void cRsId::search(void)
{
	if (WideSearch) {
		nBinLow = 3;
		nBinHigh = RSID_FFT_SIZE - 32;
	} else {
		//float centerfreq = ext_get_displayed_freq_kHz(rx_chan) * kHz;
		float centerfreq = snd->norm_pbc;
		float bpf = 1.0 * RSID_ARRAY_SIZE / RSID_SAMPLE_RATE;
		nBinLow = (int)((centerfreq  - 100.0 * 2) * bpf);
		nBinHigh = (int)((centerfreq  + 100.0 * 2) * bpf);
	}
	if (nBinLow < 3) nBinLow = 3;
	if (nBinHigh > RSID_FFT_SIZE - 32) nBinHigh = RSID_FFT_SIZE - 32;

    //rcprintf(rx_chan, "RSID: %s\n", mode_uc[ext_get_mode(rx_chan)]);
    // FIXME: what to do about IQ mode?
	bool bReverse = mode_flags[snd->mode] & IS_LSB;
	if (bReverse) {
		nBinLow  = RSID_FFT_SIZE - nBinHigh;
		nBinHigh = RSID_FFT_SIZE - nBinLow;
	}
    //rcprintf(rx_chan, "RSID: bin lo|hi %d|%d\n", nBinLow, nBinHigh);

	if (RSWINDOW) {
		//for (int i = 0; i < RSID_ARRAY_SIZE; i++)
		//  aFFTcmplx[i] = cmplx(aInputSamples[i] * fftwindow[i], 0);
		for (int i = 0; i < RSID_ARRAY_SIZE; i++) {
			ComplexIO[i][I] = aInputSamples[i] * fftwindow[i];
			ComplexIO[i][Q] = 0;
		}
	} else {
		//for (int i = 0; i < RSID_ARRAY_SIZE; i++)
		//  aFFTcmplx[i] = cmplx(aInputSamples[i], 0);
		for (int i = 0; i < RSID_ARRAY_SIZE; i++) {
			ComplexIO[i][I] = aInputSamples[i];
			ComplexIO[i][Q] = 0;
		}
	}

	RSID_FFTW_EXECUTE(ComplexFFT);

	memset(aFFTAmpl, 0, sizeof(aFFTAmpl));

	static const double pscale = 4.0 / (RSID_FFT_SIZE * RSID_FFT_SIZE);

	if (unlikely(bReverse)) {
		for (int i = 0; i < RSID_FFT_SIZE; i++)
			//aFFTAmpl[RSID_FFT_SIZE - 1 - i] = norm(aFFTcmplx[i]) * pscale;
			aFFTAmpl[RSID_FFT_SIZE - 1 - i] = RSID_FFTW_NORM(ComplexIO[i]) * pscale;
	} else {
		for (int i = 0; i < RSID_FFT_SIZE; i++)
			//aFFTAmpl[i] = norm(aFFTcmplx[i]) * pscale;
			aFFTAmpl[i] = RSID_FFTW_NORM(ComplexIO[i]) * pscale;
	}

	int bucket_low = 3;
	int bucket_high = RSID_FFT_SIZE - 32;
	if (bReverse) {
		bucket_low  = RSID_FFT_SIZE - bucket_high;
		bucket_high = RSID_FFT_SIZE - bucket_low;
	}

	memmove(fft_buckets,
			&(fft_buckets[1][0]),
			(RSID_NTIMES - 1) * RSID_FFT_SIZE * sizeof(int));
	memset(&(fft_buckets[RSID_NTIMES - 1][0]), 0, RSID_FFT_SIZE * sizeof(int));

	CalculateBuckets(aFFTAmpl, bucket_low,  bucket_high - RSID_NTIMES);
	CalculateBuckets(aFFTAmpl, bucket_low + 1, bucket_high - RSID_NTIMES);

	int symbol_out_1 = -1;
	int bin_out_1    = -1;
	int symbol_out_2 = -1;
	int bin_out_2    = -1;

	if (rsid_secondary_time_out <= 0) {
		found1 = search_amp(bin_out_1, symbol_out_1, pCodes1);
		if (found1) {
			if (symbol_out_1 != RSID_ESCAPE) {
				if (bReverse)
					bin_out_1 = 1024 - bin_out_1 - 31;
				apply(bin_out_1, symbol_out_1, 0);
				reset();
				return;
			} else {
				// 10 rsid_gap + 15 symbols + 2 for timing errors
				rsid_secondary_time_out = 27 * RSID_SYMLEN;
				return;
			}
		} else
			return;
	}

	found2 = search_amp(bin_out_2, symbol_out_2, pCodes2);
	if (found2) {
		if (symbol_out_2 != RSID_NONE2) {
			if (bReverse)
				bin_out_2 = 1024 - bin_out_2 - 31;
			apply(bin_out_2, symbol_out_2, 1);
		}
		reset();
	}

}

void cRsId::receive(int len, TYPEMONO16 *samps)
{
    int i;
    if (len != FASTFIR_OUTBUF_SIZE) panic("len");
    u4_t start = timer_ms();

	if (rsid_secondary_time_out > 0) {
		rsid_secondary_time_out -= 1.0 * len / ext_update_get_sample_rateHz(rx_chan);
		if (rsid_secondary_time_out <= 0) {
			rcprintf(rx_chan, "RSID: Secondary timed out\n");
			reset();
		}
	}

    for (i = 0; i < len; i++) {
        vecTempResBufIn[i] = (float) samps[i] / 32768.0F;   // +/- 32k => +/- 1.0
    }
    
    ResampleObj->Resample(vecTempResBufIn, vecTempResBufOut);

    for (i = 0; i < ResampleObj->iOutputBlockSize; i++) {
        aInputSamples[inptr+i] = vecTempResBufOut[i];
    }

    inptr += ResampleObj->iOutputBlockSize;

    while (inptr >= RSID_ARRAY_SIZE) {
        search();
        memmove(&aInputSamples[0], &aInputSamples[RSID_FFT_SAMPLES],
                (RSID_BUFFER_SIZE - RSID_FFT_SAMPLES) * sizeof(float));
        inptr -= RSID_FFT_SAMPLES;
    }
    
    //u4_t now = timer_ms(); if ((tr & 0xf) == 0) printf("%.3f\n", TIME_DIFF_MS(now, start)); tr++;
}

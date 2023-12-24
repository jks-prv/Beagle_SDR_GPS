//////////////////////////////////////////////////////////////////////
// fastfir.cpp: implementation of the CFastFIR class.
//
// This class implements a FIR Bandpass filter using a FFT convolution algorithm
// The filter is complex and is specified with 3 parameters:
// sample frequency, Hicut and Lowcut frequency
//
//Uses FFT overlap and save method of implementing the FIR.
//For best performance use FIR size   4*FIR <= FFT <= 8*FIR
//If need output to be power of 2 then FIR must = 1/2FFT size
//
// History:
//	2010-09-15  Initial creation MSW
//	2011-03-27  Initial release
//	2011-11-03  Fixed m_pFFTOverlapBuf initialization bug
//////////////////////////////////////////////////////////////////////
//==========================================================================================
// + + +   This Software is released under the "Simplified BSD License"  + + +
//Copyright 2010 Moe Wheatley. All rights reserved.
//
//Redistribution and use in source and binary forms, with or without modification, are
//permitted provided that the following conditions are met:
//
//   1. Redistributions of source code must retain the above copyright notice, this list of
//	  conditions and the following disclaimer.
//
//   2. Redistributions in binary form must reproduce the above copyright notice, this list
//	  of conditions and the following disclaimer in the documentation and/or other materials
//	  provided with the distribution.
//
//THIS SOFTWARE IS PROVIDED BY Moe Wheatley ``AS IS'' AND ANY EXPRESS OR IMPLIED
//WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
//FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL Moe Wheatley OR
//CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
//CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
//SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
//ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
//NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
//ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
//The views and conclusions contained in the software and documentation are those of the
//authors and should not be interpreted as representing official policies, either expressed
//or implied, of Moe Wheatley.
//==========================================================================================

// Copyright (c) 2018-2023 Christoph Mayer, DL1CH
// Copyright (c) 2018-2023 John Seamons, ZL4VO/KF6VO

#include "cuteSDR.h"
#include "fastfir.h"
#include "ext_int.h"
#include "misc.h"
#include "simd.h"
#include "rx_sound.h"


//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CFastFIR::CFastFIR()
{
	int i;
	m_InBufInPos = (CONV_FIR_SIZE - 1);

	for (i=0; i < CONV_FFT_SIZE; i++) {
		m_pFFTBuf[i].re = 0.0;
		m_pFFTBuf[i].im = 0.0;
		m_pFFTBuf_pre[i].re = 0.0;
		m_pFFTBuf_pre[i].im = 0.0;

		// CIC compensating filter
        const TYPEREAL f = fabs(fmod(TYPEREAL(i)/CONV_FFT_SIZE+0.5f, 1.0f) - 0.5f);
        const TYPEREAL p1 = (snd_rate == SND_RATE_3CH ? -3.107f : -2.969f);
        const TYPEREAL p2 = (snd_rate == SND_RATE_3CH ? 32.04f  : 36.26f );
        const TYPEREAL sincf = f ? MSIN(f*K_PI)/(f*K_PI) : 1.0f;
        m_CIC_coeffs[i] = pow(sincf, -5) + p1*exp(p2*(f-0.5f));
	}

    SetupWindowFunction(-1);
    for (i=0; i < CONV_FIR_SIZE; i++) {
        m_pFFTOverlapBuf[i].re = 0.0;
        m_pFFTOverlapBuf[i].im = 0.0;
    }
    
	m_FFT_CoefPlan = MFFTW_PLAN_DFT_1D(CONV_FFT_SIZE, (MFFTW_COMPLEX*) m_pFilterCoef, (MFFTW_COMPLEX*) m_pFilterCoef, FFTW_FORWARD, FFTW_ESTIMATE);
	m_FFT_FwdPlan = MFFTW_PLAN_DFT_1D(CONV_FFT_SIZE, (MFFTW_COMPLEX*) m_pFFTBuf, (MFFTW_COMPLEX*) m_pFFTBuf, FFTW_FORWARD, FFTW_ESTIMATE);
	m_FFT_RevPlan = MFFTW_PLAN_DFT_1D(CONV_FFT_SIZE, (MFFTW_COMPLEX*) m_pFFTBuf, (MFFTW_COMPLEX*) m_pFFTBuf, FFTW_BACKWARD, FFTW_ESTIMATE);
	
	m_FLoCut = -1.0;
	m_FHiCut = 1.0;
	m_Offset = 1.0;
	m_SampleRate = 1.0;
	m_do_CIC_comp = (VAL_CICF_DECIM_BY_2 == 2)? false : true;
}

CFastFIR::~CFastFIR()
{
}

// create window function for windowed sinc low pass filter design
void CFastFIR::SetupWindowFunction(int window_func)
{
    int i;
    if (window_func < 0)
        window_func = WINF_SND_BLACKMAN_NUTTALL;
    else
        printf("CFastFIR::SetupWindowFunction %d\n", window_func);

    for (i=0; i < CONV_FIR_SIZE; i++) {

        switch (window_func) {

        case WINF_SND_BLACKMAN_NUTTALL:
            m_pWindowTbl[i] = (0.3635819
                - 0.4891775*MCOS( (K_2PI*i)/(CONV_FIR_SIZE-1) )
                + 0.1365995*MCOS( (2.0*K_2PI*i)/(CONV_FIR_SIZE-1) )
                - 0.0106411*MCOS( (3.0*K_2PI*i)/(CONV_FIR_SIZE-1) ) );
            break;

        case WINF_SND_BLACKMAN_HARRIS:
            m_pWindowTbl[i] = (0.35875
                - 0.48829*MCOS( (K_2PI*i)/(CONV_FIR_SIZE-1) )
                + 0.14128*MCOS( (2.0*K_2PI*i)/(CONV_FIR_SIZE-1) )
                - 0.01168*MCOS( (3.0*K_2PI*i)/(CONV_FIR_SIZE-1) ) );
            break;

        case WINF_SND_NUTTALL:
            m_pWindowTbl[i] = (0.355768
                - 0.487396*MCOS( (K_2PI*i)/(CONV_FIR_SIZE-1) )
                + 0.144232*MCOS( (2.0*K_2PI*i)/(CONV_FIR_SIZE-1) )
                - 0.012604*MCOS( (3.0*K_2PI*i)/(CONV_FIR_SIZE-1) ) );
            break;

        case WINF_SND_HANNING:
            m_pWindowTbl[i] = (0.5
                - 0.5 * MCOS( (K_2PI*i)/(CONV_FIR_SIZE-1) ) );
            break;
    
        case WINF_SND_HAMMING:
            m_pWindowTbl[i] = (0.54
                - 0.46 * MCOS( (K_2PI*i)/(CONV_FIR_SIZE-1) ) );
            break;
        }
    }
}

void CFastFIR::SetupCICFilter(bool do_CIC_comp)
{
    m_do_CIC_comp = do_CIC_comp;
    //printf("CFastFIR::SetupCICFilter do_CIC_comp=%d\n", do_CIC_comp);
    
    for (int i = 0; i < CONV_FFT_SIZE; i++) {
        m_pFilterCoef_CIC[i].re = m_pFilterCoef[i].re * (do_CIC_comp? m_CIC_coeffs[i] : 1.0);
        m_pFilterCoef_CIC[i].im = m_pFilterCoef[i].im * (do_CIC_comp? m_CIC_coeffs[i] : 1.0);
        m_CIC[i] = do_CIC_comp? m_CIC_coeffs[i] : 1.0;
    }
}

//////////////////////////////////////////////////////////////////////
//  Call to setup filter parameters
// SampleRate in Hz
// FLowcut is low cutoff frequency of filter in Hz
// FHicut is high cutoff frequency of filter in Hz
// Offset is the CW tone offset frequency
// cutoff frequencies range from -SampleRate/2 to +SampleRate/2
//  HiCut must be greater than LowCut
//		example to make 2700Hz USB filter:
//	SetupParameters( 100, 2800, 0, 48000);
//////////////////////////////////////////////////////////////////////
void CFastFIR::SetupParameters(int instance, TYPEREAL FLoCut, TYPEREAL FHiCut,
								TYPEREAL Offset, TYPEREAL SampleRate)
{
    int i;
    m_instance = instance;
    
	if( (FLoCut==m_FLoCut) && (FHiCut==m_FHiCut) &&
		(Offset==m_Offset) && (SampleRate==m_SampleRate) )
	{
		return;		//return if no changes
	}
	m_FLoCut = FLoCut;
	m_FHiCut = FHiCut;
	m_Offset = Offset;
	m_SampleRate = SampleRate;

	FLoCut += Offset;
	FHiCut += Offset;
	//sanity check on filter parameters
	if( (FLoCut >= FHiCut) ||
		(FLoCut >= SampleRate/2.0) ||
		(FLoCut <= -SampleRate/2.0) ||
		(FHiCut >= SampleRate/2.0) ||
		(FHiCut <= -SampleRate/2.0) )
	{
		return;
	}
	//calculate some normalized filter parameters
	TYPEREAL nFL = FLoCut/SampleRate;
	TYPEREAL nFH = FHiCut/SampleRate;
	TYPEREAL nFc = (nFH-nFL)/2.0;		//prototype LP filter cutoff
	TYPEREAL nFs = K_2PI*(nFH+nFL)/2.0;		//2 PI times required frequency shift (FHiCut+FLoCut)/2
	TYPEREAL fCenter = 0.5*(TYPEREAL)(CONV_FIR_SIZE-1);	//floating point center index of FIR filter

	for (i=0; i < CONV_FFT_SIZE; i++)		//zero pad entire coefficient buffer to FFT size
	{
		m_pFilterCoef[i].re = m_pFilterCoef_CIC[i].re = 0.0;
		m_pFilterCoef[i].im = m_pFilterCoef_CIC[i].im = 0.0;
	}

	//create LP FIR windowed sinc, MSIN(x)/x complex LP filter coefficients
	for (i=0; i < CONV_FIR_SIZE; i++)
	{
		TYPEREAL x = (TYPEREAL) i - fCenter;
		TYPEREAL z;
		if ((TYPEREAL) i == fCenter) {	//deal with odd size filter singularity where sin(0)/0==1
			z = 2.0 * nFc;
		} else {
			z = (TYPEREAL) MSIN(K_2PI * x*nFc) / (K_PI*x) * m_pWindowTbl[i];
		}

		//shift lowpass filter coefficients in frequency by (hicut+lowcut)/2 to form bandpass filter anywhere in range
		// (also scales by 1/FFTsize since inverse FFT routine scales by FFTsize)
		m_pFilterCoef[i].re = z * MCOS(nFs * x) / (TYPEREAL) CONV_FFT_SIZE;
		m_pFilterCoef[i].im = z * MSIN(nFs * x) / (TYPEREAL) CONV_FFT_SIZE;
	}

	//convert FIR coefficients to frequency domain by taking forward FFT
	MFFTW_EXECUTE(m_FFT_CoefPlan);

    SetupCICFilter(m_do_CIC_comp);
}

///////////////////////////////////////////////////////////////////////////////
//   Process 'InLength' complex samples in 'InBuf'.
//  returns number of complex samples placed in OutBuf
//number of samples returned in general will not be equal to the number of
//input samples due to FFT block size processing.
//600ns/samp
///////////////////////////////////////////////////////////////////////////////
int CFastFIR::ProcessData(int rx_chan, int InLength, TYPECPX* InBuf, TYPECPX* OutBuf)
{
    //print_max_min_c("FIRin", InBuf, InLength);

    ext_users_t *ext_u = &ext_users[rx_chan];
    ext_receive_FFT_samps_t receive_FFT = ext_u->receive_FFT;
    int receive_FFT_instance = m_instance;
    bool receive_FFT_pre = (receive_FFT != NULL && (ext_u->FFT_flags & PRE_FILTERED));
    bool receive_FFT_post = (receive_FFT != NULL && (ext_u->FFT_flags & POST_FILTERED));

	snd_t *snd = &snd_inst[rx_chan];
    ext_receive_FFT_samps_t specAF_FFT = snd->specAF_FFT;
    bool specAF_FFT_post = (specAF_FFT != NULL && snd->specAF_instance == m_instance);

    int i = 0;
    int j;
    int len = InLength;
    int outpos = 0;

	if (!InLength)	// if nothing to do
		return 0;

	while (len--) {
		j = m_InBufInPos - (CONV_FFT_SIZE - CONV_FIR_SIZE + 1);
		if (j >= 0) {   // keep copy of last CONV_FIR_SIZE-1 samples for overlap save
			m_pFFTOverlapBuf[j] = InBuf[i];
		}

		m_pFFTBuf[m_InBufInPos++] = InBuf[i++];

        // perform FFT -> complexMultiply by FIR coefficients -> inverse FFT on filled FFT input buffer
		if (m_InBufInPos >= CONV_FFT_SIZE) {
			//print_max_min_c("preFFT", m_pFFTBuf, CONV_FFT_SIZE);
			MFFTW_EXECUTE(m_FFT_FwdPlan);

            bool buf_modified = false;
			if (receive_FFT_pre) {
                //print_max_min_c("postFFT", m_pFFTBuf, CONV_FFT_SIZE);
                simd_multiply_cfc(CONV_FFT_SIZE,
                                  reinterpret_cast<const fftwf_complex *>(m_pFFTBuf),
                                  m_CIC,
                                  reinterpret_cast<fftwf_complex *>(m_pFFTBuf_pre));
                buf_modified = receive_FFT(rx_chan, receive_FFT_instance, PRE_FILTERED, CONV_FFT_TO_OUTBUF_RATIO, CONV_FFT_SIZE, m_pFFTBuf_pre);
            }

            if (buf_modified) {
                simd_multiply_ccc(CONV_FFT_SIZE,
                                  reinterpret_cast<const fftwf_complex *>(m_pFilterCoef),
                                  reinterpret_cast<const fftwf_complex *>(m_pFFTBuf_pre),
                                  reinterpret_cast<      fftwf_complex *>(m_pFFTBuf));
            } else {
                //CpxMpy(CONV_FFT_SIZE, m_pFilterCoef_CIC, m_pFFTBuf, m_pFFTBuf);
                simd_multiply_ccc(CONV_FFT_SIZE,
                                  reinterpret_cast<const fftwf_complex *>(m_pFilterCoef_CIC),
                                  reinterpret_cast<const fftwf_complex *>(m_pFFTBuf),
                                  reinterpret_cast<      fftwf_complex *>(m_pFFTBuf));
            }

			if (receive_FFT_post)
				receive_FFT(rx_chan, receive_FFT_instance, POST_FILTERED, CONV_FFT_TO_OUTBUF_RATIO, CONV_FFT_SIZE, m_pFFTBuf);
			if (specAF_FFT_post)
				specAF_FFT(rx_chan, receive_FFT_instance, POST_FILTERED, CONV_FFT_TO_OUTBUF_RATIO, CONV_FFT_SIZE, m_pFFTBuf);

			MFFTW_EXECUTE(m_FFT_RevPlan);

            if (OutBuf != NULL) {
                for (j = (CONV_FIR_SIZE-1); j < CONV_FFT_SIZE; j++) {
                    // copy FFT output into OutBuf minus CONV_FIR_SIZE-1 samples at beginning
                    OutBuf[outpos++] = m_pFFTBuf[j];
                }
            }

			for (j=0; j < (CONV_FIR_SIZE - 1); j++) {
			    // copy overlap buffer into start of fft input buffer
				m_pFFTBuf[j] = m_pFFTOverlapBuf[j];
			}

			//reset input position to data start position of fft input buffer
			m_InBufInPos = CONV_FIR_SIZE - 1;
		}
	}

	return outpos;	//return number of output samples processed and placed in OutBuf
}

///////////////////////////////////////////////////////////////////////////////
//   Complex multiply N point array m with src and place in dest.  
// src and dest can be the same buffer.
///////////////////////////////////////////////////////////////////////////////
inline void CFastFIR::CpxMpy(int N, TYPECPX* m, TYPECPX* src, TYPECPX* dest)
{
	for (int i=0; i < N; i++) {
		TYPEREAL sr = src[i].re;
		TYPEREAL si = src[i].im;
		dest[i].re = m[i].re * sr - m[i].im * si;
		dest[i].im = m[i].re * si + m[i].im * sr;
	}
}

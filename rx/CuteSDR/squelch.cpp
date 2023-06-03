// Implementation of the CSquelch class.
//
// This class takes I/Q baseband data and performs FM and non-FM mode squelch
//
// History:
//	2011-01-17  Initial creation MSW
//	2011-03-27  Initial release
//	2011-08-07  Modified FIR filter initialization to force fixed size
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

#include "squelch.h"
#include "datatypes.h"
#include "rx_sound.h"

//#define FMPLL_RANGE 15000.0	//maximum deviation limit of PLL
#define FMPLL_RANGE 9600.0	//maximum deviation limit of PLL
#define VOICE_BANDWIDTH 3000.0

#define FMPLL_BW	VOICE_BANDWIDTH		//natural frequency ~loop bandwidth
#define FMPLL_ZETA	0.707				//PLL Loop damping factor

#define FMDC_ALPHA	0.001	//time constant for DC removal filter

#define MAX_FMOUT (K_AMPMAX * K_AMPMAX)

//#define SQUELCH_MAX 3000.0		//roughly the maximum noise average with no signal
#define SQUELCH_MAX CLIPPER_NBFM_VAL    //roughly the maximum noise average with no signal
#define SQUELCHAVE_TIMECONST .02
#define SQUELCH_HYSTERESIS 50.0

#define DEMPHASIS_TIME 80e-6

/////////////////////////////////////////////////////////////////////////////////
//	Construct squelch object
/////////////////////////////////////////////////////////////////////////////////
CSquelch::CSquelch()
{
	Reset();
}

void CSquelch::Reset()
{
	//printf("Squelch reset\n");
	m_FreqErrorDC = 0.0;
	m_NcoPhase = 0.0;
	m_NcoFreq = 0.0;

	m_SquelchAve = 0.0;
	m_SquelchState = true;
	m_DeemphasisAve = 0.0;
	m_SetSquelch = false;
}


/////////////////////////////////////////////////////////////////////////////////
// Sets sample rate and adjusts any parameters that are affected.
/////////////////////////////////////////////////////////////////////////////////
void CSquelch::SetupParameters(int rx_chan, TYPEREAL samplerate)
{
	m_rx_chan = rx_chan;
	m_SampleRate = samplerate;

	TYPEREAL norm = K_2PI/m_SampleRate;	//to normalize Hz to radians

	//initialize the PLL
	m_NcoLLimit = -FMPLL_RANGE * norm;		//clamp FM PLL NCO
	m_NcoHLimit = FMPLL_RANGE * norm;
	m_PllAlpha_P = 2.0 * FMPLL_ZETA * FMPLL_BW * norm;
	m_PllBeta_F = (m_PllAlpha_P * m_PllAlpha_P) / (4.0 * FMPLL_ZETA * FMPLL_ZETA);

	// computed values cause loop instability for our low sample rate (e.g. values are > 1)
	m_PllBeta_F = 0.002;
	m_PllAlpha_P = MSQRT(m_PllBeta_F);

	//DC removal filter time constant
	m_DcAlpha = (1.0 - MEXP(-1.0/(m_SampleRate*FMDC_ALPHA)) );

	//initialize some noise squelch items
	m_SquelchHPFreq = VOICE_BANDWIDTH;
	m_SquelchAlpha = (1.0-MEXP(-1.0/(m_SampleRate*SQUELCHAVE_TIMECONST)) );

	m_DeemphasisAlpha = (1.0-MEXP(-1.0/(m_SampleRate*DEMPHASIS_TIME)) );

	//m_LpFir.InitLPFilter(0, 1.0, 50.0, VOICE_BANDWIDTH, 1.6*VOICE_BANDWIDTH, m_SampleRate);

	InitNoiseSquelch();
	//printf("PLL SR %f norm %f Alpha %.9f Beta %.9f\n", m_SampleRate, norm, m_PllAlpha_P, m_PllBeta_F);

	Reset();
}


/////////////////////////////////////////////////////////////////////////////////
// Sets squelch threshold based on 'Value' which goes from 0 (fully open) to 99 (always muted).
/////////////////////////////////////////////////////////////////////////////////
void CSquelch::SetSquelch(int Value, int SquelchMax)
{
    m_SquelchValue = Value;
	if (SquelchMax == 0) SquelchMax = SQUELCH_MAX;
	m_SquelchThreshold = (TYPEREAL) (SquelchMax - (( SquelchMax * Value) / 99));
	m_SetSquelch = true;
    //printf("SQ-SET th=%.0f max=%d val=%d ===================================\n", m_SquelchThreshold, SquelchMax, Value);
}


/////////////////////////////////////////////////////////////////////////////////
// Sets up Highpass noise filter parameters based on input filter BW
/////////////////////////////////////////////////////////////////////////////////
void CSquelch::InitNoiseSquelch()
{
	m_HpFir.InitHPFilter(0, 1.0, 50.0, m_SquelchHPFreq*.8, m_SquelchHPFreq*.65, m_SampleRate);
//	m_HpFir.InitHPFilter(0, 1.0, 50.0, VOICE_BANDWIDTH*2.0, VOICE_BANDWIDTH, m_SampleRate);
}


int CSquelch::PerformNonFMSquelch(int InLength, TYPEMONO16* pInData, TYPEMONO16* pOutData)
{
    return 0;
}


/////////////////////////////////////////////////////////////////////////////////
// Performs noise squelch by reading the noise power above the voice frequencies
/////////////////////////////////////////////////////////////////////////////////
int CSquelch::PerformFMSquelch(int InLength, TYPEREAL* pInData, TYPEMONO16* pOutData)
{
	int nsq_nc_sq = 0;

	if (InLength>MAX_SQBUF_SIZE)
		return 0;

	TYPEREAL sqbuf[MAX_SQBUF_SIZE];
	
	// high pass filter to get the high frequency noise above the voice
	m_HpFir.ProcessFilter(InLength, pInData, sqbuf);

	for (int i=0; i<InLength; i++) {
		TYPEREAL mag = MFABS(sqbuf[i]); // get magnitude of High pass filtered data
		// exponential filter squelch magnitude
		m_SquelchAve = (1.0-m_SquelchAlpha)*m_SquelchAve + m_SquelchAlpha*mag;
	}

	// perform squelch compare to threshold using some Hysteresis
	#if 0
		static int lp;
		if (((lp++) % 16) == 0)
			printf("SQ th %f av %f %s\n", m_SquelchThreshold, m_SquelchAve, m_SquelchState? "SQ'd":"OPEN");
	#endif
	
	if (m_SquelchValue == 0) {
	    // force no squelch (always open)
        if (m_SquelchState == true) nsq_nc_sq = -1;
        m_SquelchState = false;
	} else
	
	if (m_SquelchThreshold == 0) {
		// force squelch if zero (strong signal threshold)
		if (m_SquelchState == false) nsq_nc_sq = 1;
		m_SquelchState = true;
	} else
	
	if (m_SquelchState)	{
		if (m_SquelchAve < (m_SquelchThreshold - SQUELCH_HYSTERESIS)) {
            // squelched => not squelched
			nsq_nc_sq = -1;
			m_SquelchState = false;
		}
	} else {
		if (m_SquelchAve >= (m_SquelchThreshold + SQUELCH_HYSTERESIS)) {
		    // not squelched => squelched
			nsq_nc_sq = 1;
			m_SquelchState = true;
		} else {
		}
	}
	
    if (m_SquelchState) {
        // silence output if squelched
        for (int i=0; i<InLength; i++)
            pOutData[i] = 1;    // non-zero to keep FF silence detector from being tripped
    } else {
        // low pass filter audio if squelch is open
        //ProcessDeemphasisFilter(InLength, pOutData, pOutData);
        //m_LpFir.ProcessFilter(InLength, pInData, pOutData);

        // Don't LPF here. Let our de-emphasis elsewhere handle it.
        for (int i=0; i<InLength; i++)
            pOutData[i] = (TYPEMONO16) pInData[i];   
    }
	
    if (m_SetSquelch) {
        nsq_nc_sq = m_SquelchState? 1 : -1;
        m_SetSquelch = false;
    }

	#if 0
		if (nsq_nc_sq != 0) {
			printf("SQ-CHG th=%.0f av=%.0f %s\n", m_SquelchThreshold, m_SquelchAve, m_SquelchState? "SQ'd":"OPEN");
		}
	#endif
	
	return nsq_nc_sq;
}

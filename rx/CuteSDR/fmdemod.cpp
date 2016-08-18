// fmdemod.cpp: implementation of the CFmDemod class.
//
//  This class takes I/Q baseband data and performs
// FM demodulation and squelch
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

#include "fmdemod.h"
//#include "gui/testbench.h"
#include "datatypes.h"
//#include <QDebug>

CFmDemod m_FmDemod[RX_CHANS];

#define FMPLL_RANGE 15000.0	//maximum deviation limit of PLL
#define VOICE_BANDWIDTH 3000.0

#define FMPLL_BW	VOICE_BANDWIDTH		//natural frequency ~loop bandwidth
#define FMPLL_ZETA	0.707				//PLL Loop damping factor

#define FMDC_ALPHA	0.001	//time constant for DC removal filter

//#define MAX_FMOUT 100000.0
#define MAX_FMOUT (K_AMPMAX * K_AMPMAX)

#define SQUELCH_MAX 100000.0		//roughly the maximum noise average with no signal
#define SQUELCHAVE_TIMECONST .02
#define SQUELCH_HYSTERESIS 50.0

#define DEMPHASIS_TIME 80e-6

/////////////////////////////////////////////////////////////////////////////////
//	Construct FM demod object
/////////////////////////////////////////////////////////////////////////////////
CFmDemod::CFmDemod()
{
	Reset();
}

void CFmDemod::Reset()
{
	printf("NBFM reset\n");
	m_FreqErrorDC = 0.0;
	m_NcoPhase = 0.0;
	m_NcoFreq = 0.0;

	m_SquelchAve = 0.0;
	m_SquelchState = true;
	m_DeemphasisAve = 0.0;
}


/////////////////////////////////////////////////////////////////////////////////
// Sets sample rate and adjusts any parameters that are affected.
/////////////////////////////////////////////////////////////////////////////////
void CFmDemod::SetSampleRate(int rx_chan, TYPEREAL samplerate)
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
	m_PllAlpha_P = sqrt(m_PllBeta_F);

	m_OutGain = MAX_FMOUT/m_NcoHLimit;	//audio output level gain value

	//DC removal filter time constant
	m_DcAlpha = (1.0 - exp(-1.0/(m_SampleRate*FMDC_ALPHA)) );

	//initialize some noise squelch items
	m_SquelchHPFreq = VOICE_BANDWIDTH;
	m_SquelchAlpha = (1.0-exp(-1.0/(m_SampleRate*SQUELCHAVE_TIMECONST)) );

	m_DeemphasisAlpha = (1.0-exp(-1.0/(m_SampleRate*DEMPHASIS_TIME)) );

	m_LpFir.InitLPFilter(0,1.0,50.0,VOICE_BANDWIDTH, 1.6*VOICE_BANDWIDTH, m_SampleRate);

	InitNoiseSquelch();
	//printf("PLL SR %f norm %f Alpha %.9f Beta %.9f Gain %f\n", m_SampleRate, norm, m_PllAlpha_P, m_PllBeta_F, m_OutGain);

	Reset();
}


/////////////////////////////////////////////////////////////////////////////////
// Sets squelch threshold based on 'Value' which goes from 0 (fully open) to 99 (always muted).
/////////////////////////////////////////////////////////////////////////////////
void CFmDemod::SetSquelch(int Value)
{
	m_SquelchThreshold = (TYPEREAL)(SQUELCH_MAX - (( SQUELCH_MAX*Value)/99));
//printf("SQ th %f %d ===================================\n", m_SquelchThreshold, Value);
}


/////////////////////////////////////////////////////////////////////////////////
// Sets up Highpass noise filter parameters based on input filter BW
/////////////////////////////////////////////////////////////////////////////////
void CFmDemod::InitNoiseSquelch()
{
	m_HpFir.InitHPFilter(0, 1.0, 50.0, m_SquelchHPFreq*.8, m_SquelchHPFreq*.65, m_SampleRate);
//	m_HpFir.InitHPFilter(0, 1.0, 50.0, VOICE_BANDWIDTH*2.0, VOICE_BANDWIDTH, m_SampleRate);
}


/////////////////////////////////////////////////////////////////////////////////
// Performs noise squelch by reading the noise power above the voice frequencies
/////////////////////////////////////////////////////////////////////////////////
int CFmDemod::PerformNoiseSquelch(int InLength, TYPEREAL* pTmpData, TYPEMONO16* pOutData)
{
	int sq_nc_open = 0;

	if(InLength>MAX_SQBUF_SIZE)
		return 0;

	TYPEREAL sqbuf[MAX_SQBUF_SIZE];

	//high pass filter to get the high frequency noise above the voice
	m_HpFir.ProcessFilter(InLength, pTmpData, sqbuf);
//g_pTestBench->DisplayData(InLength, sqbuf, m_SampleRate,PROFILE_6);

	for(int i=0; i<InLength; i++)
	{
		TYPEREAL mag = fabs( sqbuf[i] );	//get magnitude of High pass filtered data
		// exponential filter squelch magnitude
		m_SquelchAve = (1.0-m_SquelchAlpha)*m_SquelchAve + m_SquelchAlpha*mag;
//g_pTestBench->DisplayData(1, &m_SquelchAve, m_SampleRate,PROFILE_3);
	}

	//perform squelch compare to threshold using some Hysteresis
	#ifdef NBFM_PLL_DEBUG
		static int lp;
		if (((lp++)%16)==0)printf("SQ th %f av %f %s\n", m_SquelchThreshold, m_SquelchAve, m_SquelchState? "SQ'd":"OPEN");
	#endif
	
	if(0==m_SquelchThreshold)
	{	//force squelch if zero(strong signal threshold)
		if (m_SquelchState == false) sq_nc_open = -1;
		m_SquelchState = true;
	}
	else if(m_SquelchState)	//if in squelched state
	{
		if(m_SquelchAve < (m_SquelchThreshold-SQUELCH_HYSTERESIS)) {
			sq_nc_open = 1;
			m_SquelchState = false;
		}
	}
	else
	{
		if(m_SquelchAve >= (m_SquelchThreshold+SQUELCH_HYSTERESIS)) {
			sq_nc_open = -1;
			m_SquelchState = true;
		}
	}
	
	if(m_SquelchState)
	{	//zero output if squelched
		for(int i=0; i<InLength; i++)
			pOutData[i] = 0;
	}
	else
	{	//low pass filter audio if squelch is open
//		ProcessDeemphasisFilter(InLength, pOutData, pOutData);
		m_LpFir.ProcessFilter(InLength, pTmpData, pOutData);
//g_pTestBench->DisplayData(InLength, pOutData, m_SampleRate,PROFILE_6);
	}
	
	return sq_nc_open;
}

/////////////////////////////////////////////////////////////////////////////////
//	Process FM demod MONO version, TYPEMONO16 out version (for NBFM demodulator where only real signal output is needed)
/////////////////////////////////////////////////////////////////////////////////
int CFmDemod::ProcessData(int InLength, TYPEREAL FmBW, TYPECPX* pInData, TYPEREAL* pTmpData, TYPEMONO16* pOutData)
{
	if(m_SquelchHPFreq != FmBW)
	{	//update squelch HP filter cutoff from main filter BW
		m_SquelchHPFreq = FmBW;
		//printf("NBFM half_bw %f ------------------------------------\n", FmBW);
		InitNoiseSquelch();
	}

#ifdef NBFM_PLL_DEBUG
	TYPECPX oscIQ[512];
	if (ext_users[m_rx_chan].receive_iq != NULL)
		ext_users[m_rx_chan].receive_iq(m_rx_chan, 0, InLength, pInData);
#endif

	for(int i=0; i<InLength; i++)
	{
		TYPECPX osc;
		//osc.re = cos(m_NcoPhase);
		//osc.im = sin(m_NcoPhase);

		// re/im swapped to match swapping of signal done in data_pump.cpp
		osc.re = sin(m_NcoPhase);
		osc.im = -cos(m_NcoPhase);		// small instability unless this is negative!

#ifdef NBFM_PLL_DEBUG
		oscIQ[i] = osc;
#endif

		//complex multiply input sample by NCO's sin and cos
		TYPEREAL re = pInData[i].re, im = pInData[i].im;
		TYPECPX tmp;
		tmp.re = osc.re * re - osc.im * im;
		tmp.im = osc.re * im + osc.im * re;

		//find current sample phase after being shifted by NCO frequency
		TYPEREAL phzerror = -atan2(tmp.im, tmp.re);

		//create new NCO frequency term
		TYPEREAL FreqAdj = m_PllBeta_F * phzerror;
		TYPEREAL PhaseAdj = m_PllAlpha_P * phzerror;
		
		m_NcoFreq += FreqAdj;		//  radians per sampletime

		//clamp NCO frequency so doesn't get out of lock range
		if(m_NcoFreq > m_NcoHLimit)
			m_NcoFreq = m_NcoHLimit;
		else if(m_NcoFreq < m_NcoLLimit)
			m_NcoFreq = m_NcoLLimit;

		//update NCO phase with new value
		m_NcoPhase += m_NcoFreq + PhaseAdj;

		//LP filter the NCO frequency term to get DC offset value
		m_FreqErrorDC = (1.0-m_DcAlpha)*m_FreqErrorDC + m_DcAlpha*m_NcoFreq;

		//subtract out DC term to get FM audio
		pTmpData[i] = (m_NcoFreq-m_FreqErrorDC)*m_OutGain;
	}

#ifdef NBFM_PLL_DEBUG
	if (ext_users[m_rx_chan].receive_iq != NULL)
		ext_users[m_rx_chan].receive_iq(m_rx_chan, 1, InLength, oscIQ);
#endif

//g_pTestBench->DisplayData(InLength, pTmpData, m_SampleRate, PROFILE_3);
	m_NcoPhase = fmod(m_NcoPhase, K_2PI);	//keep radian counter bounded
#if 1
	return PerformNoiseSquelch(InLength, pTmpData, pOutData);	//calculate squelch
#else
	m_LpFir.ProcessFilter(InLength, pTmpData, pOutData);
	return 0;
#endif
}

/////////////////////////////////////////////////////////////////////////////////
//	Process InLength InBuf[] samples and place in OutBuf[]
//MONO version
/////////////////////////////////////////////////////////////////////////////////
void CFmDemod::ProcessDeemphasisFilter(int InLength, TYPEREAL* InBuf, TYPEREAL* OutBuf)
{
	for(int i=0; i<InLength; i++)
	{
		m_DeemphasisAve = (1.0-m_DeemphasisAlpha)*m_DeemphasisAve + m_DeemphasisAlpha*InBuf[i];
		OutBuf[i] = m_DeemphasisAve;
	}
}

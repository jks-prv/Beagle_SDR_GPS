//////////////////////////////////////////////////////////////////////
// noiseproc.cpp: implementation of the CNoiseProc class.
//
//  This class implements various noise processing functions.
//   Impulse blanker, Noise Reduction, and Notch
//
// History:
//	2011-01-06  Initial creation MSW
//	2011-03-27  Initial release(not implemented yet)
//	2013-07-28  Added single/double precision math macros
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

#include "rx_noise.h"
#include "noiseproc.h"

//////////////////////////////////////////////////////////////////////
// Local Defines
//////////////////////////////////////////////////////////////////////
#define MAX_GATE 4096		//abt 1 mSec at 2MHz
#define MAX_DELAY 8192		//at least WF_C_NSAMPS for fixed samples case
#define MAX_AVE 32768		//abt 10 mSec at 2MHz

#define MAGAVE_TIME 0.005

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CNoiseProc::CNoiseProc()
{
	m_DelayBuf = new TYPECPX[MAX_DELAY];
	m_DelayBuf_r = new TYPEMONO16[MAX_DELAY];
	m_MagBuf = new TYPEREAL[MAX_AVE];
	m_MagBuf_r = new TYPEMONO16[MAX_AVE];

	m_pIgnoreData = new TYPECPX[MAX_DELAY];
	m_pZeroData = new TYPECPX[MAX_DELAY];
	for (int i=0; i<MAX_DELAY ; i++) {
	    m_pZeroData[i].re = 0.0;
	    m_pZeroData[i].im = 0.0;
	}
}

CNoiseProc::~CNoiseProc()
{
	if (m_DelayBuf)
		delete [] m_DelayBuf;
	if (m_DelayBuf_r)
		delete [] m_DelayBuf_r;
	if (m_MagBuf)
		delete [] m_MagBuf;
	if (m_MagBuf_r)
		delete [] m_MagBuf_r;
	if (m_pIgnoreData)
		delete [] m_pIgnoreData;
	if (m_pZeroData)
		delete [] m_pZeroData;
}

void CNoiseProc::SetupBlanker(const char *id, TYPEREAL SampleRate, TYPEREAL nb_param[NOISE_PARAMS])
{
    TYPEREAL GateUsec = nb_param[NB_GATE], Threshold = nb_param[NB_THRESHOLD];
    m_id = id;
    m_GateUsec = GateUsec;
    
    if (SampleRate != 0) {
        m_GateSamples = GateUsec * 1e-6 * SampleRate;
        if (m_GateSamples < 3)
            m_GateSamples = 3;       // needed to keep audio from clicking for short gate times
        else
        if (m_GateSamples > MAX_GATE)
            m_GateSamples = MAX_GATE;
    
        m_MagSamples = MAGAVE_TIME * SampleRate;
        if (m_MagSamples < 1)
            m_MagSamples = 1;
        else
        if (m_MagSamples > MAX_AVE)
            m_MagSamples = MAX_AVE;
        
        if (Threshold < 0)
            Threshold = 0;
        else
        if (Threshold > 100)
            Threshold = 100;
    
        m_Ratio = .005*(Threshold) * (TYPEREAL) m_MagSamples;
    
        m_DelaySamples = m_GateSamples/2;
        if (m_DelaySamples < 1)
            m_DelaySamples = 1;

        //printf("noiseproc %s sr=%.0f usec=%.0f th=%.0f gs=%d ms=%d ds=%d ratio=%f\n",
        //    id, SampleRate, GateUsec, Threshold, m_GateSamples, m_MagSamples, m_DelaySamples, m_Ratio);
    }

	m_Dptr = 0;
	m_Mptr = 0;
	m_BlankCounter = 0;
	m_MagAveSum = 0.0;
	m_MagAveSum_r = 0;
	m_LastMsg = 0;

	for (int i=0; i<MAX_DELAY ; i++)
	{
		m_DelayBuf[i].re = 0.0;
		m_DelayBuf[i].im = 0.0;
		m_DelayBuf_r[i] = 0;
	}
	
	for (int i=0; i<MAX_AVE ; i++)
	{
		m_MagBuf[i] = 0.0;
		m_MagBuf_r[i] = 0;
	}
}

void CNoiseProc::ProcessBlanker(int InLength, TYPECPX* pInData, TYPECPX* pOutData)
{
    TYPECPX new_samp;
    TYPECPX oldest;
    int msg=0;
    
	for (int i=0; i<InLength; i++)
	{
		new_samp = pInData[i];

		//calculate peak magnitude
		TYPEREAL mre = MFABS(new_samp.re);
		TYPEREAL mim = MFABS(new_samp.im);
		TYPEREAL mag = (mre > mim) ? mre : mim;

		//calc moving average of "m_MagSamples"
		m_MagAveSum -= m_MagBuf[m_Mptr];	//subtract oldest sample
		m_MagAveSum += mag;					//add new sample
		m_MagBuf[m_Mptr++] = mag;			//stick in buffer

		if (m_Mptr > m_MagSamples)
			m_Mptr = 0;

		//pull out oldest sample from delay buffer and put in new one
		oldest = m_DelayBuf[m_Dptr];
		m_DelayBuf[m_Dptr++] = new_samp;
		if(m_Dptr > m_DelaySamples)
			m_Dptr = 0;

		if (mag * m_Ratio > m_MagAveSum)
		{
			m_BlankCounter = m_GateSamples;
			#if 0
			    m_Blanked++;
                int now = timer_sec();
                if (m_LastMsg != now) {
                    printf("noiseproc %s blank=%d usec=%.0f gs=%d mag=%.1e mag*ratio=%.1e > avg=%.1e\n",
                        m_id, m_Blanked, m_GateUsec, m_BlankCounter, mag, mag * m_Ratio, m_MagAveSum);
                    m_LastMsg = now;
                    m_Blanked = 0;
                }
			#endif
		}

		if (m_BlankCounter)
		{
			m_BlankCounter--;
			pOutData[i].re = 0.0;
			pOutData[i].im = 0.0;
			//pOutData[i].re = pOutData[i].im = m_MagAveSum;
		}
		else
		{
			pOutData[i] = oldest;
		}
	}
}

void CNoiseProc::ProcessBlanker(int InLength, TYPEMONO16* pInData, TYPEMONO16* pOutData)
{
    TYPEMONO16 new_samp;
    TYPEMONO16 oldest;
    int msg=0;
    
	for (int i=0; i<InLength; i++)
	{
		new_samp = pInData[i];

		//calculate peak magnitude
		TYPEMONO16 mag = new_samp;

		//calc moving average of "m_MagSamples"
		m_MagAveSum_r -= m_MagBuf_r[m_Mptr];	//subtract oldest sample
		m_MagAveSum_r += mag;					//add new sample
		m_MagBuf[m_Mptr++] = mag;			//stick in buffer

		if (m_Mptr > m_MagSamples)
			m_Mptr = 0;

		//pull out oldest sample from delay buffer and put in new one
		oldest = m_DelayBuf_r[m_Dptr];
		m_DelayBuf_r[m_Dptr++] = new_samp;
		if(m_Dptr > m_DelaySamples)
			m_Dptr = 0;

		if (MROUND(mag * m_Ratio) > m_MagAveSum_r)
		{
			m_BlankCounter = m_GateSamples;
			#if 0
			    m_Blanked++;
                int now = timer_sec();
                if (m_LastMsg != now) {
                    printf("noiseproc %s blank=%d usec=%.0f gs=%d mag=%d mag*ratio=%.1f > avg=%d\n",
                        m_id, m_Blanked, m_GateUsec, m_BlankCounter, mag, mag * m_Ratio, m_MagAveSum_r);
                    m_LastMsg = now;
                    m_Blanked = 0;
                }
			#endif
		}

		if (m_BlankCounter)
		{
			m_BlankCounter--;
			pOutData[i] = 0;
		}
		else
		{
			pOutData[i] = oldest;
		}
	}
}

void CNoiseProc::ProcessBlankerOneShot(int InLength, TYPECPX* pInData, TYPECPX* pOutData)
{
    ProcessBlanker(m_DelaySamples, pInData, m_pIgnoreData);
    pInData += m_DelaySamples;
    InLength -= m_DelaySamples;
    ProcessBlanker(InLength, pInData, pOutData);
    pOutData += InLength;
    ProcessBlanker(m_DelaySamples, m_pZeroData, pOutData);
}

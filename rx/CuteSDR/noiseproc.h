//////////////////////////////////////////////////////////////////////
// noiseproc.h: interface for the CNoiseProc class.
//
//  This class implements various noise reduction and blanker functions.
//
// History:
//	2011-01-06  Initial creation MSW
//	2011-03-27  Initial release
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
//=============================================================================
#ifndef NOISEPROC_H
#define NOISEPROC_H

#include "datatypes.h"
#include "kiwi.h"
#include "rx.h"
#include "noise_blank.h"

class CNoiseProc
{
public:
	CNoiseProc();
	virtual ~CNoiseProc();

	void SetupBlanker(const char *id, TYPEREAL SampleRate, TYPEREAL nb_param[NOISE_PARAMS]);
	void ProcessBlanker(int InLength, TYPECPX* pInData, TYPECPX* pOutData);
	void ProcessBlanker(int InLength, TYPEMONO16* pInData, TYPEMONO16* pOutData);
	void ProcessBlankerOneShot(int InLength, TYPECPX* pInData, TYPECPX* pOutData);

private:
    const char *m_id;
    TYPEREAL m_GateUsec;
	TYPECPX* m_DelayBuf;
	TYPEMONO16* m_DelayBuf_r;
	TYPEREAL* m_MagBuf;
	TYPEMONO16* m_MagBuf_r;
	TYPECPX* m_pIgnoreData;
	TYPECPX* m_pZeroData;
	int m_Dptr;
	int m_Mptr;
	int m_BlankCounter;
	int m_DelaySamples;
	int m_MagSamples;
	int m_GateSamples;
	TYPEREAL m_Ratio;
	TYPEREAL m_MagAveSum;
	TYPEMONO16 m_MagAveSum_r;
	int m_Blanked;
	int m_LastMsg;
};

extern CNoiseProc m_NoiseProc[MAX_RX_CHANS][2];
#define NB_SND  0
#define NB_WF   1

#endif //  NOISEPROC_H

//////////////////////////////////////////////////////////////////////
// Interface for the CSquelch class.
//
// History:
//	2011-01-17  Initial creation MSW
//	2011-03-27  Initial release
/////////////////////////////////////////////////////////////////////

#pragma once

#include "datatypes.h"
#include "fir.h"
//#include "iir.h"

#define MAX_SQBUF_SIZE 1024

class CSquelch
{
public:
	CSquelch();
	
	void Reset();
	void SetupParameters(int rx_chan, TYPEREAL samplerate);
	void SetSquelch(int Value, int SquelchMax);		//call with range of 0 to 99 to set squelch threshold
	int PerformFMSquelch(int InLength, TYPEREAL* pInData, TYPEMONO16* pOutData);
	int PerformNonFMSquelch(int InLength, TYPEMONO16* pInData, TYPEMONO16* pOutData);

private:
	void InitNoiseSquelch();

	int m_rx_chan;
	bool m_SquelchState, m_SetSquelch;
	TYPEREAL m_SampleRate;
	TYPEREAL m_SquelchHPFreq;
	TYPEREAL m_FreqErrorDC;
	TYPEREAL m_DcAlpha;
	TYPEREAL m_NcoPhase;
	TYPEREAL m_NcoFreq;
	TYPEREAL m_NcoAcc;
	TYPEREAL m_NcoLLimit;
	TYPEREAL m_NcoHLimit;
	TYPEREAL m_PllAlpha_P;
	TYPEREAL m_PllBeta_F;

	TYPEREAL m_SquelchValue;
	TYPEREAL m_SquelchThreshold;
	TYPEREAL m_SquelchAve;
	TYPEREAL m_SquelchAlpha;

	TYPEREAL m_OutBuf[MAX_SQBUF_SIZE];

	TYPEREAL m_DeemphasisAve;
	TYPEREAL m_DeemphasisAlpha;

	CFir m_HpFir;
	CFir m_LpFir;

};

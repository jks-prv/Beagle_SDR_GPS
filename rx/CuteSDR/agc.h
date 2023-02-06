//////////////////////////////////////////////////////////////////////
// agc.h: interface for the CAgc class.
//
//  This class implements an automatic gain function.
//
// History:
//	2010-09-15  Initial creation MSW
//	2011-03-27  Initial release
//////////////////////////////////////////////////////////////////////
#ifndef AGCX_H
#define AGCX_H

#include "datatypes.h"
#include "kiwi.h"

#define MAX_DELAY_BUF 2048

class CAgc
{
public:
	CAgc();
	virtual ~CAgc();
	void SetParameters(bool AgcOn, bool UseHang, int Threshold, int ManualGain, int Slope, int Decay, TYPEREAL SampleRate);
	void ProcessData(int Length, TYPECPX* pInData, TYPECPX* pOutData);
	void ProcessData(int Length, TYPECPX* pInData, TYPEMONO16* pOutData);

	int GetDelaySamples() const { return m_DelaySamples; }

 private:
	bool m_AgcOn;				//internal copy of AGC settings parameters
	bool m_UseHang;
	int m_Threshold;
	int m_ManualGain;
	int m_Slope;
	int m_Decay;
	TYPEREAL m_SampleRate;

	TYPEREAL m_SlopeFactor;
	TYPEREAL m_ManualAgcGain;

	TYPEREAL m_DecayAve;
	TYPEREAL m_AttackAve;

	TYPEREAL m_AttackRiseAlpha;
	TYPEREAL m_AttackFallAlpha;
	TYPEREAL m_DecayRiseAlpha;
	TYPEREAL m_DecayFallAlpha;

	TYPEREAL m_FixedGain;
	TYPEREAL m_Knee;
	TYPEREAL m_GainSlope;
	TYPEREAL m_Peak;

	int m_SigDelayPtr;
	int m_MagBufPos;
	int m_DelaySize;
	int m_DelaySamples;
	int m_WindowSamples;
	int m_HangTime;
	int m_HangTimer;

	TYPECPX m_SigDelayBuf[MAX_DELAY_BUF];
	TYPEREAL m_MagBuf[MAX_DELAY_BUF];
};

#endif //  AGCX_H

//////////////////////////////////////////////////////////////////////
// FastFIR.h: interface for the CFastFIR class.
//
// This class implements a FIR Bandpass filter using a FFT convolution algorithm
// The filter is complex and is specified with 3 parameters:
// sample frequency, Hicut and Lowcut frequency
//
// History:
//	2010-09-15  Initial creation MSW
//	2011-03-27  Initial release
//////////////////////////////////////////////////////////////////////
#ifndef FASTFIR_H
#define FASTFIR_H

#include "datatypes.h"
#include "kiwi.h"
#include <fftw3.h>

#define CONV_FIR_SIZE (CONV_FFT_SIZE/2+1)	//must be <= FFT size. Make 1/2 +1 if want
											//output to be in power of 2

class CFastFIR  
{
public:
	CFastFIR();
	virtual ~CFastFIR();

	void SetupParameters( TYPEREAL FLoCut,TYPEREAL FHiCut,TYPEREAL Offset, TYPEREAL SampleRate);
	int ProcessData(int rx_chan, int InLength, TYPECPX* InBuf, TYPECPX* OutBuf);

private:
	inline void CpxMpy(int N, TYPECPX* m, TYPECPX* src, TYPECPX* dest);

	TYPEREAL m_FLoCut;
	TYPEREAL m_FHiCut;
	TYPEREAL m_Offset;
	TYPEREAL m_SampleRate;

	int m_InBufInPos;
	TYPEREAL m_pWindowTbl[CONV_FIR_SIZE];
	TYPECPX m_pFFTOverlapBuf[CONV_FIR_SIZE];
	TYPECPX m_pFilterCoef[CONV_FFT_SIZE];
	TYPECPX m_pFFTBuf[CONV_FFT_SIZE];
	MFFTW_PLAN m_FFT_CoefPlan;
	MFFTW_PLAN m_FFT_FwdPlan;
	MFFTW_PLAN m_FFT_RevPlan;
};

extern CFastFIR m_PassbandFIR[RX_CHANS];

#endif // FASTFIR_H

//////////////////////////////////////////////////////////////////////
// fir.h: interface for the CFir class.
//
//  This class implements a FIR  filter using a double flat coefficient
//array to eliminate testing for buffer wrap around.
//
//Also a decimate by 3 half band filter class CDecimateBy2 is implemented
//
// History:
//	2011-01-29  Initial creation MSW
//	2011-03-27  Initial release
//	2011-08-05  Added decimate by 2 class
//	2011-08-07  Modified FIR filter initialization
//////////////////////////////////////////////////////////////////////
#ifndef FIR_H
#define FIR_H

#include "datatypes.h"
#include "kiwi.h"

#define MAX_NUMCOEF 75

////////////
//class for FIR Filters
////////////
class CFir
{
public:
    CFir();

	void InitConstFir( int NumTaps, const TYPEREAL* pCoef, TYPEREAL Fsamprate);
	void InitConstFir( int NumTaps, const TYPEREAL* pICoef, const TYPEREAL* pQCoef, TYPEREAL Fsamprate);
	int InitLPFilter(int NumTaps, TYPEREAL Scale, TYPEREAL Astop, TYPEREAL Fpass, TYPEREAL Fstop, TYPEREAL Fsamprate);
	int InitHPFilter(int NumTaps, TYPEREAL Scale, TYPEREAL Astop, TYPEREAL Fpass, TYPEREAL Fstop, TYPEREAL Fsamprate);
	void GenerateHBFilter( TYPEREAL FreqOffset);
	void ProcessFilter(int InLength, TYPEREAL* InBuf, TYPEREAL* OutBuf);
	void ProcessFilter(int InLength, TYPEREAL* InBuf, TYPECPX* OutBuf);
	void ProcessFilter(int InLength, TYPECPX* InBuf, TYPECPX* OutBuf);
	void ProcessFilter(int InLength, TYPEREAL* InBuf, TYPEMONO16* OutBuf);
	void ProcessFilter(int InLength, TYPEMONO16* InBuf, TYPEMONO16* OutBuf);

private:
	TYPEREAL Izero(TYPEREAL x);
	TYPEREAL m_SampleRate;
	int m_NumTaps;
	int m_State;
	TYPEREAL m_Coef[MAX_NUMCOEF*2];
	TYPEREAL m_ICoef[MAX_NUMCOEF*2];
	TYPEREAL m_QCoef[MAX_NUMCOEF*2];
	TYPEREAL m_rZBuf[MAX_NUMCOEF];
	TYPECPX m_cZBuf[MAX_NUMCOEF];
};

extern CFir m_AM_FIR[MAX_RX_CHANS];
extern CFir m_de_emp_FIR[MAX_RX_CHANS];

#endif // FIR_H

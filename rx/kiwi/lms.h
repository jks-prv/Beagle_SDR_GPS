#pragma once

#include "datatypes.h"
#include "kiwi.h"

typedef enum { LMS_DENOISE_QRN, LMS_AUTONOTCH_QRM } lms_e;

#define LMSLEN      121
#define LMSLEN_M1   (LMSLEN - 1)
#define MAX_DLEN    (512 - LMSLEN)

#define INC(dlp) dlp = (dlp == (m_dlen + LMSLEN_M1))? 0 : (dlp+1);
#define DEC(dlp) dlp = (dlp == 0)? (m_dlen + LMSLEN_M1) : (dlp-1);

class CLMS
{
public:
    CLMS();
    int Initialize(lms_e lms_type, TYPEREAL delayLineLen, TYPEREAL beta, TYPEREAL decay);
	void ProcessFilter(int ilen, TYPEMONO16* ibuf, TYPEMONO16* obuf);

private:
    lms_e m_lms_type;
    int m_dlen;
    TYPEREAL m_beta;
    TYPEREAL m_decay;
    
	TYPEREAL m_dline[MAX_DLEN + LMSLEN];
	int m_dlp;
	TYPEREAL m_lmscoef[LMSLEN];
};

extern CLMS m_LMS_denoise[RX_CHANS];
extern CLMS m_LMS_autonotch[RX_CHANS];

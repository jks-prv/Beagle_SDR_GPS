#pragma once

#include "datatypes.h"
#include "kiwi.h"
#include "rx.h"

#define LMSLEN      121
#define LMSLEN_M1   (LMSLEN - 1)
#define MAX_DLEN    (512 - LMSLEN)

#define INC(dlp) dlp = (dlp == (m_dlen + LMSLEN_M1))? 0 : (dlp+1);
#define DEC(dlp) dlp = (dlp == 0)? (m_dlen + LMSLEN_M1) : (dlp-1);

class CLMS
{
public:
    CLMS();
    int Initialize(nr_type_e nr_type, TYPEREAL nr_param[NOISE_PARAMS]);
	void ProcessFilter(int ilen, TYPEMONO16* ibuf, TYPEMONO16* obuf);

private:
    nr_type_e m_nr_type;
    int m_dlen;
    TYPEREAL m_beta;
    TYPEREAL m_decay;
    
	TYPEREAL m_dline[MAX_DLEN + LMSLEN];
	int m_dlp;
	TYPEREAL m_lmscoef[LMSLEN];
};

extern CLMS m_LMS[MAX_RX_CHANS][2];

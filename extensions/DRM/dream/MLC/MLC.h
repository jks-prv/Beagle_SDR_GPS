/******************************************************************************\
 * Technische Universitaet Darmstadt, Institut fuer Nachrichtentechnik
 * Copyright (c) 2001
 *
 * Author(s):
 *	Volker Fischer
 *
 * Description:
 *	See MLC.cpp
 *
 ******************************************************************************
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
\******************************************************************************/

#if !defined(MLC_H__3B0BA660_CA63_4344_BB2B_23E7A0D31912__INCLUDED_)
#define MLC_H__3B0BA660_CA63_4344_BB2B_23E7A0D31912__INCLUDED_

#include "../GlobalDefinitions.h"
#include "../util/Modul.h"
#include "../Parameter.h"
#include "../tables/TableMLC.h"
#include "../tables/TableCarMap.h"
#include "ConvEncoder.h"
#include "ViterbiDecoder.h"
#include "Metric.h"
#include "BitInterleaver.h"
#include "QAMMapping.h"
#include "EnergyDispersal.h"


/* Classes ********************************************************************/
class CMLC
{
public:
    CMLC() : iN_mux(0), eChannelType(CT_MSC) {}
    virtual ~CMLC() {}

    void CalculateParam(CParameter& Parameter, int iNewChannelType);

protected:
    int	iLevels;
    /* No input bits for each level. First index: Level, second index:
       Protection level.
       For three levels: [M_0,l  M_1,l  M2,l]
       For six levels: [M_0,lRe  M_0,lIm  M_1,lRe  M_1,lIm  M_2,lRe  ...  ] */
    int	iM[MC_MAX_NUM_LEVELS][2];
    int iN[2];
    int iL[3];
    int iN_mux;
    int iCodeRate[MC_MAX_NUM_LEVELS][2];

    const int* piInterlSequ;

    int	iNumEncBits;

    EChanType	eChannelType;
    ECodScheme	eCodingScheme;
};

class CMLCEncoder : public CTransmitterModul<_BINARY, _COMPLEX>,
            public CMLC
{
public:
    CMLCEncoder() {}
    virtual ~CMLCEncoder() {}

protected:
    CConvEncoder		ConvEncoder[MC_MAX_NUM_LEVELS];
    /* Two different types of interleaver table */
    CBitInterleaver		BitInterleaver[2];
    CQAMMapping			QAMMapping;
    CEngergyDispersal	EnergyDisp;

    /* Internal buffers */
    CVector<_DECISION>	vecEncInBuffer[MC_MAX_NUM_LEVELS];
    CVector<_DECISION>	vecEncOutBuffer[MC_MAX_NUM_LEVELS];

    virtual void InitInternal(CParameter& TransmParam);
    virtual void ProcessDataInternal(CParameter& Parameter);
};

class CMLCDecoder : public CReceiverModul<CEquSig, _BINARY>,
            public CMLC
{
public:
    CMLCDecoder() : iInitNumIterations(MC_NUM_ITERATIONS) {}
    virtual ~CMLCDecoder() {}

    _REAL GetAccMetric() const {
        return 10 * log10(rAccMetric);
    }
    void GetVectorSpace(CVector<_COMPLEX>& veccData);
    void SetNumIterations(int iNewNumIterations)
    {
        iInitNumIterations = iNewNumIterations;
        SetInitFlag();
    }
    int GetInitNumIterations() const {
        return iInitNumIterations;
    }

protected:
    CViterbiDecoder		ViterbiDecoder[MC_MAX_NUM_LEVELS];
    CMLCMetric			MLCMetric;
    /* Two different types of deinterleaver table */
    CBitDeinterleaver	BitDeinterleaver[2];
    CBitInterleaver		BitInterleaver[2];
    CConvEncoder		ConvEncoder[MC_MAX_NUM_LEVELS];
    CEngergyDispersal	EnergyDisp;

    /* Internal buffers */
    CVector<CDistance>	vecMetric;

    CVector<_DECISION>	vecDecOutBits[MC_MAX_NUM_LEVELS];
    CVector<_DECISION>	vecSubsetDef[MC_MAX_NUM_LEVELS];
    int					iNumOutBits;

    /* Accumulated metric */
    _REAL				rAccMetric;

    /* Internal buffer for GetVectorSpace function */
    CVector<_COMPLEX>	vecSigSpacBuf;

    int					iNumIterations;
    int					iInitNumIterations;
    int					iIndexLastBranch;

    virtual void InitInternal(CParameter& Parameters);
    virtual void ProcessDataInternal(CParameter& Parameters);
};


/******************************************************************************\
* Customized channel (de-)coders											   *
\******************************************************************************/
class CMSCMLCEncoder : public CMLCEncoder
{
protected:
    virtual void InitInternal(CParameter& TransmParam)
    {
        /* Set corresponding type */
        eChannelType = CT_MSC;

        /* Call init in encoder */
        CMLCEncoder::InitInternal(TransmParam);
    };
};

class CSDCMLCEncoder : public CMLCEncoder
{
protected:
    virtual void InitInternal(CParameter& TransmParam)
    {
        /* Set corresponding type */
        eChannelType = CT_SDC;

        /* Call init in encoder */
        CMLCEncoder::InitInternal(TransmParam);
    };
};

class CFACMLCEncoder : public CMLCEncoder
{
protected:
    virtual void InitInternal(CParameter& TransmParam)
    {
        /* Set corresponding type */
        eChannelType = CT_FAC;

        /* Call init in encoder */
        CMLCEncoder::InitInternal(TransmParam);
    };
};

class CMSCMLCDecoder : public CMLCDecoder
{
protected:
    virtual void InitInternal(CParameter& Parameters)
    {
        /* Set corresponding type */
        eChannelType = CT_MSC;

        /* Call init in encoder */
        CMLCDecoder::InitInternal(Parameters);
    };
};

class CSDCMLCDecoder : public CMLCDecoder
{
protected:
    virtual void InitInternal(CParameter& Parameters)
    {
        /* Set corresponding type */
        eChannelType = CT_SDC;

        /* Call init in encoder */
        CMLCDecoder::InitInternal(Parameters);
    };
};

class CFACMLCDecoder : public CMLCDecoder
{
protected:
    virtual void InitInternal(CParameter& Parameters)
    {
        /* Set corresponding type */
        eChannelType = CT_FAC;

        /* Call init in encoder */
        CMLCDecoder::InitInternal(Parameters);
    };
};


#endif // !defined(MLC_H__3B0BA660_CA63_4344_BB2B_23E7A0D31912__INCLUDED_)

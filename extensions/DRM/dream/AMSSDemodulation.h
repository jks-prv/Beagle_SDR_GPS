/******************************************************************************\
 * BBC Research & Development
 * Copyright (c) 2005
 *
 * Author(s):
 *	Andrew Murphy
 *
 * Description:
 *	See AMSSDemodulation.cpp
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

#if !defined(_AMSSDEMODULATION_H_)
#define _AMSSDEMODULATION_H_

#include "Parameter.h"
#include "util/Modul.h"
#include "util/Vector.h"
#include "matlib/Matlib.h"

#define AMSS_PLL_LOOP_GAIN						((CReal) 0.00015)
#define AMSS_PLL_LOOP_FILTER_LAMBDA				((CReal) 0.99)

#define AMSS_FILTER_LAMBDA						((CReal) 0.9876)

#define AMSS_IF_FILTER_BW						((CReal) 300.0)

#define AMSS_RESAMPLER_LOOP_GAIN				((CReal) 0.00001)
#define	AMSS_RECURSIVE_FILTER_GAIN				((CReal) 0.05)
#define	AMSS_12kHz_SAMPLES_PER_BIT				256
#define AMSS_BLOCK_SIZE_BITS					47
#define	AMSS_GP_SIZE_BITS						12
#define AMSS_OFFSET_SIZE_BITS					11
#define MAX_BLOCK_FAIL_COUNT_BEFORE_SYNC_LOSS	10
#define	MAX_DATA_ENTITY_GROUP_SEGMENTS			16
#define DATA_ENTITY_GROUP_SEGMENT_SIZE_BITS		32
#define AMSS_MAX_DATA_ENTITY_GROUP_LENGTH		(MAX_DATA_ENTITY_GROUP_SEGMENTS * DATA_ENTITY_GROUP_SEGMENT_SIZE_BITS)

/* Phase lock loop ---------------------------------------------------------- */
class CAMSSPLL
{
public:
    CAMSSPLL() : rCurPhase((CReal) 0.0) {}
    void Init(const int iNewBlockSize);
    void Process(CComplexVector& veccIn, CRealVector& vecrOut);

    void SetRefNormFreq(const CReal rNewNormFreq);
    CReal GetCurPhase() const {
        return rCurPhase;
    }
    CReal GetCurNormFreqOffs() const
    {
        return rNormCurFreqOffset + rNormCurFreqOffsetAdd;
    }

protected:
    CMixer			Mixer;
    int				iBlockSize;
    CRealVector		rvecPhaseTmp;
    CComplexVector	cvecLow;
    CReal			rNormCurFreqOffset;
    CReal			rNormCurFreqOffsetAdd;
    CReal			rCurPhase;

    CRealVector		rvecZPhase;
    CRealVector		rvecA;
    CRealVector		rvecB;

    CReal rPreviousPhaseSample;
    CReal rPhaseOffset;
};

/* Recursive filter  -------------------------------------------------------------------- */
class CRecursiveFilter
{
public:
    CRecursiveFilter() {
        iPeakPos = 0;
        iCurPos = 0;
    }
    ~CRecursiveFilter() {}
    void Init(const int iNewBlockSize);
    void Process(CRealVector& vecrIn /* in/out */);

    int GetPeakPos();
    _REAL GetPeakVal();


protected:
    int				iBlockSize;
    CRealVector		vecrStore;

    CReal			rOut;
    CReal			rPeakVal;
    int				iPeakPos;
    int				iCurPos;

};


/* AMSS phase demodulation module --------------------------------------------------- */
class CAMSSPhaseDemod : public CReceiverModul<_REAL, _REAL>
{
public:
    CAMSSPhaseDemod() :
        rBPNormBW((CReal) 0.0), rNormCurMixFreqOffs((CReal) 0.0),
        rPreviousPhaseSample((CReal) 0.0), rPhaseOffset((CReal) 0.0) {}
    virtual ~CAMSSPhaseDemod() {}

    void SetAcqFreq(const CReal rNewNormCenter);
    bool GetPLLPhase(CReal& rPhaseOut);

protected:
    virtual void InitInternal(CParameter& Parameters);
    virtual void ProcessDataInternal(CParameter& Parameters);

    int			iSymbolBlockSize;

    void SetBPFilter(const CReal rNewBPNormBW, const CReal rNewNormFreqOffset);
    void SetNormCurMixFreqOffs(const CReal rNewNormCurMixFreqOffs);

    CComplexVector				cvecBReal;
    CComplexVector				cvecBImag;
    CRealVector					rvecZReal;
    CRealVector					rvecZImag;

    CRealVector					rvecInpTmp;
    CRealVector					rvecPhase;
    CComplexVector				cvecHilbert;
    int							iHilFiltBlLen;
    CFftPlans					FftPlansHilFilt;

    CReal						rBPNormBW;
    CReal						rNormCurMixFreqOffs;
    CReal						rBPNormCentOffsTot;

    CReal						rPreviousPhaseSample;
    CReal						rPhaseOffset;
    CReal						rPhaseDiff;


    CRealVector		rvecZPhase;
    CRealVector		rvecA;
    CRealVector		rvecB;

    /* Objects */
    CAMSSPLL					AMSSPLL;
    CFreqOffsAcq				FreqOffsAcq;
    //CAGC						AGC;
};


/* AMSS decoding module --------------------------------------------------- */
class CAMSSExtractBits : public CReceiverModul<_REAL, _BINARY>
{
public:
    CAMSSExtractBits()
    {
        iDiffStorePos = 0;
        iDiffInSamplePos = 0;
        iBitSyncSampleCount = 0;
        iBitSyncSliceOffset = 0;
    }
    virtual ~CAMSSExtractBits() { }
protected:
    virtual void InitInternal(CParameter& Parameters);
    virtual void ProcessDataInternal(CParameter& Parameters);

    int					iSymbolBlockSize;

    CRealVector			rvecDiffStore;
    int					iDiffStorePos;
    int					iDiffInSamplePos;

    CRealVector			rvecInpTmp;
    CRealVector			rvecInpTmpAbs;

    int					iBitSyncSampleCount;
    int					iBitSyncSliceOffset;

    /* Objects */
    CRecursiveFilter	RecursiveFilter;
};


/* AMSS decoding module --------------------------------------------------- */
class CAMSSDecode : public CReceiverModul<_BINARY, _BINARY>
{
public:
    CAMSSDecode()
    {
        cDataEntityGroupStatus = new char[MAX_DATA_ENTITY_GROUP_SEGMENTS+1];
        cCurrentBlockBits = new char[AMSS_BLOCK_SIZE_BITS+1];
    }

    virtual ~CAMSSDecode()
    {
        delete[] cDataEntityGroupStatus;
        delete[] cCurrentBlockBits;
    }

    EAMSSBlockLockStat GetLockStatus() const {
        return eAMSSBlockLockStatus;
    }
    int GetPercentageDataEntityGroupComplete() const
    {
        return iPercentageDataEntityGroupComplete;
    }
    char* GetDataEntityGroupStatus() const {
        return cDataEntityGroupStatus;
    }
    int GetCurrentBlock() const {
        return iCurrentBlock;
    }
    char* GetCurrentBlockBits() const {
        return cCurrentBlockBits;
    }

    bool GetBlock1Status() const {
        return blBlock1DataValid;
    }

protected:
    virtual void InitInternal(CParameter& Parameters);
    virtual void ProcessDataInternal(CParameter& Parameters);

    EAMSSBlockLockStat	eAMSSBlockLockStatus;

    int				iPercentageDataEntityGroupComplete;
    char*			cDataEntityGroupStatus;
    int				iCurrentBlock;
    char*			cCurrentBlockBits;

    int				iTotalDataEntityGroupSegments;
    CVector<int>	blDataEntityGroupSegmentsReceived;

    int				DecodeBlock(CVector<_BINARY>& bBits, CParameter& Parameters);
    void			DecodeBlock1(CVector<_BINARY>& bBits, CParameter& Parameters);
    void			DecodeBlock2(CVector<_BINARY>& bBits);

    void			ApplyOffsetWord(CVector<_BINARY>& bBlockBits, CVector<_BINARY>& offset);
    bool		CheckCRC(CVector<_BINARY>& bBits);

    void			ResetStatus(CParameter& ReveiverParam);

    CVector<_BINARY>	bAMSSBits;
    int					iIntakeBufferPos;

    CVector<_BINARY>	bBitsBlock1;
    CVector<_BINARY>	bBitsBlock2;

    CVector<_BINARY>	bOffsetBlock1;
    CVector<_BINARY>	bOffsetBlock2;

    CVector<_BINARY>	bGP;

    CVector<_BINARY>	bBlock1Store;
    CVector<_BINARY>	bBlock2Store;

    bool			blStoredBlock2Valid;
    bool			bVersionFlag;

    bool			blFirstEverBlock1;

    CVector<_BINARY>	bDataEntityGroup;

    int					iBitsSinceLastBlock1Pass;
    int					iBitsSinceLastBlock2Pass;
    int					iBlock1FailCount;
    int					iBlock2FailCount;

    bool			blBlock1DataValid;
};


#endif // !defined(_AMSSDEMODULATION_H_)

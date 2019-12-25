/******************************************************************************\
 * BBC Research & Development
 * Copyright (c) 2005
 *
 * Author(s):
 *	Andrew Murphy
 *
 * Description:
 *	Implementation of an AMSS demodulator
 *
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

#include "AMDemodulation.h"
#include "AMSSDemodulation.h"

#include <fstream>
using namespace std;

/* Implementation *************************************************************/
void CAMSSPhaseDemod::ProcessDataInternal(CParameter&)
{
    int i;

    /* Frequency offset estimation */
    if (FreqOffsAcq.Run(*pvecInputData))
        SetNormCurMixFreqOffs(FreqOffsAcq.GetCurResult());

    /* Band-pass filter and mixer ------------------------------------------- */
    /* Copy CVector data in CMatlibVector */
    for (i = 0; i < iInputBlockSize; i++)
        rvecInpTmp[i] = (*pvecInputData)[i];

    /* Cut out a spectrum part of desired bandwidth */
    cvecHilbert = CComplexVector(
                      FftFilt(cvecBReal, rvecInpTmp, rvecZReal, FftPlansHilFilt),
                      FftFilt(cvecBImag, rvecInpTmp, rvecZImag, FftPlansHilFilt));

    // in AM demod there's a mixer here - this is included in PLL that follows
    /* Track carrier using freq-locked loop */
    AMSSPLL.Process(cvecHilbert, rvecPhase);

    //Apply low-pass filter
    rvecPhase = Filter(rvecB, rvecA, rvecPhase, rvecZPhase);

    for (i=0; i < iInputBlockSize; i++)
    {
        (*pvecOutputData)[i] = rvecPhase[i];
    }


    iOutputBlockSize = iInputBlockSize;
}

void CAMSSPhaseDemod::InitInternal(CParameter& Parameters)
{
    /* Get parameters from info class */
    Parameters.Lock();
    iSymbolBlockSize = Parameters.CellMappingTable.iSymbolBlockSize;
    rBPNormBW = CReal(AMSS_IF_FILTER_BW) / Parameters.GetSigSampleRate();
    Parameters.Unlock();

    /* Define block-sizes for input and output */
    /* The output buffer is a cyclic buffer, we have to specify the total
       buffer size */
    iInputBlockSize = iSymbolBlockSize;

    iMaxOutputBlockSize = iInputBlockSize;
    iOutputBlockSize = iMaxOutputBlockSize;

    /* Init temporary vector for filter input and output */
    rvecInpTmp.Init(iSymbolBlockSize);
    rvecPhase.Init(iSymbolBlockSize);
    cvecHilbert.Init(iSymbolBlockSize);

    /* Init frequency offset acquisition (start new acquisition) */
    FreqOffsAcq.Init(iSymbolBlockSize);
    FreqOffsAcq.Start((CReal) 0.0); /* Search entire bandwidth */

    /* Init loop filter (low-pass filter design) */
    /* andrewm - changed so gain _not_ negative */
    /* IIR filter: H(Z) = ((1 - lam) * z^{-1}) / (1 - lam * z^{-1}) */
    rvecZPhase.Init(2, (CReal) 0.0); /* Memory */
    rvecB.Init(2);
    rvecA.Init(2);
    rvecB[0] = (CReal) 0.0;
    rvecB[1] = ((CReal) 1.0 - AMSS_FILTER_LAMBDA);
    rvecA[0] = (CReal) 1.0;
    rvecA[1] = -AMSS_FILTER_LAMBDA;

    /* Init PLL */
    AMSSPLL.Init(iSymbolBlockSize);

    /* Inits for Hilbert and DC filter -------------------------------------- */
    /* Hilbert filter block length is the same as input block length */
    iHilFiltBlLen = iSymbolBlockSize;


    /* Init state vector for filtering with zeros */
    rvecZReal.Init(iHilFiltBlLen, (CReal) 0.0);
    rvecZImag.Init(iHilFiltBlLen, (CReal) 0.0);

    /* "+ 1" because of the Nyquist frequency (filter in frequency domain) */
    cvecBReal.Init(iHilFiltBlLen + 1);
    cvecBImag.Init(iHilFiltBlLen + 1);

    /* FFT plans are initialized with the long length */
    FftPlansHilFilt.Init(iHilFiltBlLen * 2);

    /* Init band-pass filter */
    SetNormCurMixFreqOffs(rNormCurMixFreqOffs);
}

void CAMSSPhaseDemod::SetNormCurMixFreqOffs(const CReal rNewNormCurMixFreqOffs)
{
    rNormCurMixFreqOffs = rNewNormCurMixFreqOffs;

    /* Generate filter taps */
    SetBPFilter(rBPNormBW, rNormCurMixFreqOffs);

    /* Tell the PLL object the new frequency (we do not care here if it is
       enabled or not) */
    AMSSPLL.SetRefNormFreq(rNormCurMixFreqOffs);
}

void CAMSSPhaseDemod::SetBPFilter(const CReal rNewBPNormBW,
                                  const CReal rNewNormFreqOffset)
{
    /* Set internal parameter */
    rBPNormBW = rNewBPNormBW;

    /* Actual prototype filter design */
    CRealVector vecrFilter(iHilFiltBlLen);
    vecrFilter = FirLP(rBPNormBW, Nuttallwin(iHilFiltBlLen));

    /* Actual band-pass filter offset is the demodulation frequency plus the
       additional offset for the demodulation type */
    rBPNormCentOffsTot = rNewNormFreqOffset;


    /* Set filter coefficients ---------------------------------------------- */
    /* Make sure that the phase in the middle of the filter is always the same
       to avoid clicks when the filter coefficients are changed */
    const CReal rStartPhase = (CReal) iHilFiltBlLen * crPi * rBPNormCentOffsTot;

    /* Copy actual filter coefficients. It is important to initialize the
       vectors with zeros because we also do a zero-padding */
    CRealVector rvecBReal(2 * iHilFiltBlLen, (CReal) 0.0);
    CRealVector rvecBImag(2 * iHilFiltBlLen, (CReal) 0.0);
    for (int i = 0; i < iHilFiltBlLen; i++)
    {
        rvecBReal[i] = vecrFilter[i] *
                       Cos((CReal) 2.0 * crPi * rBPNormCentOffsTot * i - rStartPhase);

        rvecBImag[i] = vecrFilter[i] *
                       Sin((CReal) 2.0 * crPi * rBPNormCentOffsTot * i - rStartPhase);
    }

    /* Transformation in frequency domain for fft filter */
    cvecBReal = rfft(rvecBReal, FftPlansHilFilt);
    cvecBImag = rfft(rvecBImag, FftPlansHilFilt);
}

bool CAMSSPhaseDemod::GetPLLPhase(CReal& rPhaseOut)
{
    bool bReturn;

    /* Lock resources */
    Lock();
    {
        /* Phase is only valid if PLL is enabled. Return status */
        rPhaseOut = AMSSPLL.GetCurPhase();
        bReturn = true;
    }
    Unlock();

    return bReturn;
}


/* Interface functions ------------------------------------------------------ */
void CAMSSPhaseDemod::SetAcqFreq(const CReal rNewNormCenter)
{
    /* Lock resources */
    Lock();
    {
        FreqOffsAcq.Start(rNewNormCenter);
    }
    Unlock();
}

/* Implementation *************************************************************/
void CAMSSExtractBits::ProcessDataInternal(CParameter& Parameters)
{
    int i;

    /* Decimate 4 to 1 to get 12kHz (approx) samples (AMSS is well oversampled and has already been filtered) */
    for (i = 0; i < iInputBlockSize/4; i++)
    {
        rvecInpTmp[i] = (*pvecInputData)[i*4];
    }

    /* Differentiate over bi-phase bit period */
    for (i = 0; i < iInputBlockSize/4; i++)
    {
        iDiffInSamplePos = iDiffStorePos;

        iDiffStorePos++;
        if (iDiffStorePos >= (AMSS_12kHz_SAMPLES_PER_BIT/2))
            iDiffStorePos = 0;

        rvecDiffStore[iDiffInSamplePos] = rvecInpTmp[i];

        rvecInpTmp[i] = rvecInpTmp[i] - rvecDiffStore[iDiffStorePos];
    }

    rvecInpTmpAbs = Abs(rvecInpTmp);

    RecursiveFilter.Process(rvecInpTmpAbs);
    Parameters.Lock();
    Parameters.rResampleOffset = AMSS_RESAMPLER_LOOP_GAIN * ( (signed) (RecursiveFilter.GetPeakPos() - (AMSS_12kHz_SAMPLES_PER_BIT/2)) );
    Parameters.Unlock();

    int iBitCount = 0;

    for (i = 0; i < iInputBlockSize/4; i++)
    {
        if (iBitSyncSampleCount == iBitSyncSliceOffset)
        {
            if (rvecInpTmp[i] < 0)
                (*pvecOutputData)[iBitCount] = true;
            else
                (*pvecOutputData)[iBitCount] = false;

            iBitCount++;
        }

        if (iBitSyncSampleCount >= (AMSS_12kHz_SAMPLES_PER_BIT-1))
        {
            //fprintf(stderr, "pos: %d\n", m_bit_sync_slice_offset);
            iBitSyncSliceOffset = RecursiveFilter.GetPeakPos();
            iBitSyncSampleCount = 0;
        }
        else
            iBitSyncSampleCount++;
    }

    iOutputBlockSize = iBitCount;
}


void CAMSSExtractBits::InitInternal(CParameter& Parameters)
{
    /* Get parameters from info class */
    Parameters.Lock();
    iSymbolBlockSize = Parameters.CellMappingTable.iSymbolBlockSize;
    Parameters.Unlock();

    /* Define block-sizes for input and output */
    /* The output buffer is a cyclic buffer, we have to specify the total
       buffer size */
    iMaxOutputBlockSize = (iSymbolBlockSize/AMSS_12kHz_SAMPLES_PER_BIT) + 2;

    iInputBlockSize = iSymbolBlockSize;
    iOutputBlockSize = 0;

    /* Init recursive filter */
    RecursiveFilter.Init(iSymbolBlockSize/4);

    /* Init temporary vector for filter input and output */
    rvecInpTmp.Init(iSymbolBlockSize/4);	// stores 12kHz samples (not 48)
    rvecInpTmpAbs.Init(iSymbolBlockSize/4);	// stores 12kHz samples (not 48)

    rvecDiffStore.Init(AMSS_12kHz_SAMPLES_PER_BIT/2);

}


void CAMSSDecode::InitInternal(CParameter& Parameters)
{
    iInputBlockSize = 2;

    iMaxOutputBlockSize = AMSS_MAX_DATA_ENTITY_GROUP_LENGTH;		// max length of SDC carried as one data entity group

    bAMSSBits.Init(AMSS_BLOCK_SIZE_BITS);
    iIntakeBufferPos = 0;

    bBitsBlock1.Init(AMSS_BLOCK_SIZE_BITS);
    bBitsBlock2.Init(AMSS_BLOCK_SIZE_BITS);

    eAMSSBlockLockStatus = NO_SYNC;

    bOffsetBlock1.Init(AMSS_OFFSET_SIZE_BITS);
    bOffsetBlock1[0] = 0;
    bOffsetBlock1[1] = 1;
    bOffsetBlock1[2] = 0;
    bOffsetBlock1[3] = 1;
    bOffsetBlock1[4] = 1;
    bOffsetBlock1[5] = 0;
    bOffsetBlock1[6] = 1;
    bOffsetBlock1[7] = 0;
    bOffsetBlock1[8] = 1;
    bOffsetBlock1[9] = 0;
    bOffsetBlock1[10] = 1;

    bOffsetBlock2.Init(AMSS_OFFSET_SIZE_BITS);
    bOffsetBlock2[0] = 1;
    bOffsetBlock2[1] = 0;
    bOffsetBlock2[2] = 1;
    bOffsetBlock2[3] = 1;
    bOffsetBlock2[4] = 0;
    bOffsetBlock2[5] = 1;
    bOffsetBlock2[6] = 0;
    bOffsetBlock2[7] = 1;
    bOffsetBlock2[8] = 0;
    bOffsetBlock2[9] = 1;
    bOffsetBlock2[10] = 1;

    bGP.Init(AMSS_GP_SIZE_BITS);
    bGP[0] = 1;
    bGP[1] = 0;
    bGP[2] = 0;
    bGP[3] = 1;
    bGP[4] = 0;
    bGP[5] = 1;
    bGP[6] = 0;
    bGP[7] = 0;
    bGP[8] = 0;
    bGP[9] = 0;
    bGP[10] = 0;
    bGP[11] =1;


    bBlock1Store.Init(AMSS_BLOCK_SIZE_BITS);
    bBlock2Store.Init(AMSS_BLOCK_SIZE_BITS);

    bDataEntityGroup.Init(AMSS_MAX_DATA_ENTITY_GROUP_LENGTH, 0);

    blDataEntityGroupSegmentsReceived.Init(MAX_DATA_ENTITY_GROUP_SEGMENTS, 0);

    bVersionFlag = false;

    Parameters.Lock();
    ResetStatus(Parameters);
    Parameters.Unlock();
}

void CAMSSDecode::ProcessDataInternal(CParameter& Parameters)
{
    int i = 0;
    int j = 0;
    /* Copy CVector data into bits*/;
    Parameters.Lock();
    for (i = 0; i < iInputBlockSize; i++)
    {
        bAMSSBits[iIntakeBufferPos] = (*pvecInputData)[i];
        iIntakeBufferPos++;

        if (iIntakeBufferPos >= AMSS_BLOCK_SIZE_BITS)
        {
            DecodeBlock(bAMSSBits, Parameters);

            /* Shift everything down by one bit */
            for (j = 1; j < AMSS_BLOCK_SIZE_BITS; j++)
                bAMSSBits[j-1] = bAMSSBits[j];

            /* next bit goes on the end of the array */
            iIntakeBufferPos = AMSS_BLOCK_SIZE_BITS-1;
        }
    }


    if (iPercentageDataEntityGroupComplete >= 100)
    {
        for (j=0; j < iTotalDataEntityGroupSegments * DATA_ENTITY_GROUP_SEGMENT_SIZE_BITS; j++)
            (*pvecOutputData)[j] = bDataEntityGroup[j];

        Parameters.SetNumDecodedBitsSDC(iTotalDataEntityGroupSegments * DATA_ENTITY_GROUP_SEGMENT_SIZE_BITS);
        iOutputBlockSize = iTotalDataEntityGroupSegments * DATA_ENTITY_GROUP_SEGMENT_SIZE_BITS;
    }
    else
        iOutputBlockSize = 0;
    Parameters.Unlock();
}

void CAMSSDecode::DecodeBlock1(CVector<_BINARY>& bBits, CParameter& Parameters)
{
    bool	bLocalVersionFlag;
    int			i;

    uint32_t	iServiceID;
    int			iAMSSCarrierMode;
    int			iLanguage;

    bBits.ResetBitAccess();

    /* Version flag */
    if (bBits.Separate(1) == 0)
        bLocalVersionFlag = false;
    else
        bLocalVersionFlag = true;

    iAMSSCarrierMode = bBits.Separate(3);
    iTotalDataEntityGroupSegments = bBits.Separate(4) + 1;

    iLanguage = bBits.Separate(4);
    iServiceID = bBits.Separate(24);

    if (iServiceID != Parameters.Service[0].iServiceID)
    {
        Parameters.ResetServicesStreams();
    }

    Parameters.iAMSSCarrierMode = iAMSSCarrierMode;
    Parameters.Service[0].iLanguage = iLanguage;
    Parameters.Service[0].iServiceID = iServiceID;

    if ( (bVersionFlag != bLocalVersionFlag) || blFirstEverBlock1)	// discard entire current data entity group
    {
        if (!blFirstEverBlock1)		// give benefit of doubt to any block 2 already received unless outside the total num we expect
            blDataEntityGroupSegmentsReceived.Reset(0);
        else
        {
            for (i=iTotalDataEntityGroupSegments; i < MAX_DATA_ENTITY_GROUP_SEGMENTS; i++)
            {
                blDataEntityGroupSegmentsReceived[i] = 0;
            }
        }

        iPercentageDataEntityGroupComplete = 0;

        for (i=0; i < MAX_DATA_ENTITY_GROUP_SEGMENTS; i++)
        {
            if (i < iTotalDataEntityGroupSegments)
            {
                if (blDataEntityGroupSegmentsReceived[i] == 0)
                    cDataEntityGroupStatus[i] = '#';
            }
            else
                cDataEntityGroupStatus[i] = '_';
        }
        cDataEntityGroupStatus[MAX_DATA_ENTITY_GROUP_SEGMENTS] = '\0';
        bVersionFlag = bLocalVersionFlag;
    }

    if (blFirstEverBlock1)
        blFirstEverBlock1 = false;

    blBlock1DataValid = true;
}

void CAMSSDecode::DecodeBlock2(CVector<_BINARY>& bBits)
{
    int iSegmentNumber;
    int i;

    bBits.ResetBitAccess();
    iSegmentNumber = bBits.Separate(4);

    for (i=0; i < DATA_ENTITY_GROUP_SEGMENT_SIZE_BITS; i++)
    {
        bDataEntityGroup[i + (iSegmentNumber*DATA_ENTITY_GROUP_SEGMENT_SIZE_BITS)] = bBits[4+i];
    }

    blDataEntityGroupSegmentsReceived[iSegmentNumber] = 1;

    if (cDataEntityGroupStatus[iSegmentNumber] == 'C')
        cDataEntityGroupStatus[iSegmentNumber] = 'c';
    else
        cDataEntityGroupStatus[iSegmentNumber] = 'C';

    int iReceivedSegments=0;

    for (i=0; i < MAX_DATA_ENTITY_GROUP_SEGMENTS; i++)
    {
        if (blDataEntityGroupSegmentsReceived[i])
            iReceivedSegments++;
    }

    if (iTotalDataEntityGroupSegments != 0)
        iPercentageDataEntityGroupComplete = (int) (((float) iReceivedSegments/iTotalDataEntityGroupSegments) * 100.0);
    else
        iPercentageDataEntityGroupComplete = 0;
}

int CAMSSDecode::DecodeBlock(CVector<_BINARY>& bBits, CParameter& Parameters)
{
    int i;

    //Take local copies for CRC calcuations
    bBitsBlock1 = bBits;
    bBitsBlock2 = bBits;

    //Apply offset to both
    ApplyOffsetWord(bBitsBlock1, bOffsetBlock1);
    ApplyOffsetWord(bBitsBlock2, bOffsetBlock2);

    bool bBlock1Test = CheckCRC(bBitsBlock1);
    bool bBlock2Test = CheckCRC(bBitsBlock2);

    if (bBlock1Test)
        iCurrentBlock =  1;
    else if (bBlock2Test)
        iCurrentBlock = 2;

    if (bBlock1Test || bBlock2Test)
    {
        //Parameters.Service[0].iServiceID++;
        for (i=0; i < AMSS_BLOCK_SIZE_BITS; i++)
            cCurrentBlockBits[i] = bBits[i] == 1 ? '1' : '0';
    }

    switch (eAMSSBlockLockStatus)
    {
    case NO_SYNC:
        blBlock1DataValid = false;

        if (bBlock1Test)
        {
            //take a local copy for later comparison
            bBlock1Store = bBits;
            eAMSSBlockLockStatus = RE_SYNC;
        }
        if (bBlock2Test)
        {
            // might as well store it anyway just in case it's useful data
            DecodeBlock2(bBits);
            eAMSSBlockLockStatus = RE_SYNC;
        }

        iBitsSinceLastBlock1Pass = 0;
        iBitsSinceLastBlock2Pass = 0;
        iBlock1FailCount = 0;
        iBlock2FailCount = 0;
        break;

    case RE_SYNC:
        iBitsSinceLastBlock1Pass++;
        iBitsSinceLastBlock2Pass++;

        if (bBlock1Test)
        {
            if (iBitsSinceLastBlock1Pass >= (AMSS_BLOCK_SIZE_BITS*2))
            {
                //compare the bits
                if (bBits == bBlock1Store)
                {
                    // definitely a reliable block 1
                    DecodeBlock1(bBits, Parameters);
                    eAMSSBlockLockStatus = DEF_SYNC;
                }
                else
                {
                    //take local copy of the bits
                    bBlock1Store = bBits;

                    // not really sync'ed but still possible
                    eAMSSBlockLockStatus = RE_SYNC;
                }

                iBlock1FailCount = 0;
            }
            else
            {
                //take local copy of the bits;
                bBlock1Store = bBits;

                // might not be really sync'ed but still possible
                eAMSSBlockLockStatus = RE_SYNC;
            }

            iBitsSinceLastBlock1Pass = 0;
        }
        if (bBlock2Test)
        {
            if (iBitsSinceLastBlock2Pass >= (AMSS_BLOCK_SIZE_BITS*2))	// probably a block 2
            {
                DecodeBlock2(bBits);
                eAMSSBlockLockStatus = RE_SYNC;
                iBlock2FailCount = 0;
            }
            else
                eAMSSBlockLockStatus = RE_SYNC;		// stay put in this state

            iBitsSinceLastBlock2Pass = 0;
        }

        if ((iBlock1FailCount > MAX_BLOCK_FAIL_COUNT_BEFORE_SYNC_LOSS) || (iBlock2FailCount > MAX_BLOCK_FAIL_COUNT_BEFORE_SYNC_LOSS) )
        {
            ResetStatus(Parameters);
            eAMSSBlockLockStatus = NO_SYNC;
        }

        break;

    case DEF_SYNC:
        iBitsSinceLastBlock1Pass++;
        iBitsSinceLastBlock2Pass++;

        if (bBlock1Test)
        {
            if (iBitsSinceLastBlock1Pass >= (AMSS_BLOCK_SIZE_BITS*2))	// definitely a block 1
            {
                if (bBlock1Store == bBits)
                {
                    eAMSSBlockLockStatus = DEF_SYNC;
                    DecodeBlock1(bBits, Parameters);
                }
                else
                {
                    // data could have changed, but get a couple just to be sure!
                    bBlock1Store = bBits;
                    eAMSSBlockLockStatus = DEF_SYNC_BUT_DATA_CHANGED;
                }

                iBitsSinceLastBlock1Pass = 0;
                iBlock1FailCount = 0;
            }
            else
                eAMSSBlockLockStatus = POSSIBLE_LOSS_OF_SYNC;
        }
        if (bBlock2Test)
        {
            if (iBitsSinceLastBlock2Pass >= (AMSS_BLOCK_SIZE_BITS*2))	// probably a block 2
            {
                DecodeBlock2(bBits);

                iBitsSinceLastBlock2Pass = 0;
                eAMSSBlockLockStatus = DEF_SYNC;

                iBlock2FailCount = 0;
            }
            else
                eAMSSBlockLockStatus = POSSIBLE_LOSS_OF_SYNC;
        }

        if ((iBlock1FailCount > MAX_BLOCK_FAIL_COUNT_BEFORE_SYNC_LOSS) || (iBlock2FailCount > MAX_BLOCK_FAIL_COUNT_BEFORE_SYNC_LOSS) )
        {
            ResetStatus(Parameters);
            eAMSSBlockLockStatus = NO_SYNC;
        }

        break;

    case DEF_SYNC_BUT_DATA_CHANGED:
        iBitsSinceLastBlock1Pass++;
        iBitsSinceLastBlock2Pass++;

        if (bBlock1Test)
        {
            if (iBitsSinceLastBlock1Pass >= (AMSS_BLOCK_SIZE_BITS*2))	// definitely a block 1
            {
                if (bBlock1Store == bBits)		// data has definitely changed - go to def sync
                {
                    eAMSSBlockLockStatus = DEF_SYNC;
                    DecodeBlock1(bBits, Parameters);

                    //last block 2 was from this new data entity group so decode that.
                    if (blStoredBlock2Valid)
                    {
                        DecodeBlock2(bBlock2Store);
                        blStoredBlock2Valid = false;
                    }
                }
                else
                {
                    // data different again, go to RE_SYNC
                    bBlock1Store = bBits;
                    eAMSSBlockLockStatus = RE_SYNC;
                }

                iBitsSinceLastBlock1Pass = 0;
                iBlock1FailCount = 0;
            }
            else
                eAMSSBlockLockStatus = POSSIBLE_LOSS_OF_SYNC;
        }
        if (bBlock2Test)
        {
            if (iBitsSinceLastBlock2Pass >= (AMSS_BLOCK_SIZE_BITS*2))	// probably a block 2
            {
                DecodeBlock2(bBits);

                //also store the block 2 - may belong to next data entity group
                blStoredBlock2Valid = true;
                bBlock2Store = bBits;

                iBitsSinceLastBlock2Pass = 0;
                eAMSSBlockLockStatus = DEF_SYNC_BUT_DATA_CHANGED;

                iBlock2FailCount = 0;
            }
            else
                eAMSSBlockLockStatus = POSSIBLE_LOSS_OF_SYNC;
        }

        if ((iBlock1FailCount > MAX_BLOCK_FAIL_COUNT_BEFORE_SYNC_LOSS) || (iBlock2FailCount > MAX_BLOCK_FAIL_COUNT_BEFORE_SYNC_LOSS) )
        {
            ResetStatus(Parameters);
            eAMSSBlockLockStatus = NO_SYNC;
        }

        break;

    case POSSIBLE_LOSS_OF_SYNC:
        iBitsSinceLastBlock1Pass++;
        iBitsSinceLastBlock2Pass++;

        if (bBlock1Test)
        {
            // definitely a block 1
            if (iBitsSinceLastBlock1Pass >= (AMSS_BLOCK_SIZE_BITS*2))
            {
                if (bBlock1Store == bBits)		// data has definitely changed - go to def sync
                {
                    eAMSSBlockLockStatus = DEF_SYNC;
                }
                else
                {
                    // data could have changed, but get a couple just to be sure!
                    bBlock1Store = bBits;
                    eAMSSBlockLockStatus = RE_SYNC;
                }

                iBitsSinceLastBlock1Pass = 0;
                iBlock1FailCount = 0;
            }
            else if (iBlock1FailCount > 0) // Found one in an unexpected place and missed it in the expected place
            {
                iBlock1FailCount = 0;
                iBitsSinceLastBlock1Pass = 0;
                eAMSSBlockLockStatus = POSSIBLE_LOSS_OF_SYNC;
            }
            else
                eAMSSBlockLockStatus = POSSIBLE_LOSS_OF_SYNC;
        }

        if (bBlock2Test)
        {
            if (iBitsSinceLastBlock2Pass >= (AMSS_BLOCK_SIZE_BITS*2))	// probably a block 2
            {
                DecodeBlock2(bBits);

                iBitsSinceLastBlock2Pass = 0;
                eAMSSBlockLockStatus = DEF_SYNC;

                iBlock2FailCount = 0;
            }
            else if (iBlock2FailCount > 0) // Found one in an unexpected place and missed it in the expected place
            {
                iBlock2FailCount = 0;
                iBitsSinceLastBlock2Pass = 0;
                eAMSSBlockLockStatus = POSSIBLE_LOSS_OF_SYNC;
            }
            else
                eAMSSBlockLockStatus = POSSIBLE_LOSS_OF_SYNC;
        }

        if ((iBlock1FailCount > MAX_BLOCK_FAIL_COUNT_BEFORE_SYNC_LOSS) || (iBlock2FailCount > MAX_BLOCK_FAIL_COUNT_BEFORE_SYNC_LOSS) )
        {
            ResetStatus(Parameters);
            eAMSSBlockLockStatus = NO_SYNC;
        }

        break;
    }

    if (iBitsSinceLastBlock1Pass >= (AMSS_BLOCK_SIZE_BITS*2))
    {
        iBlock1FailCount++;
        iBitsSinceLastBlock1Pass = 0;
    }

    if (iBitsSinceLastBlock2Pass >= (AMSS_BLOCK_SIZE_BITS*2))
    {
        iBlock2FailCount++;
        iBitsSinceLastBlock2Pass = 0;
    }

    if (bBlock1Test)
        return 1;
    else if (bBlock2Test)
        return 2;
    else
        return 0;
}

bool CAMSSDecode::CheckCRC(CVector<_BINARY>& bBits)
{
    int i=0;
    int j=0;
    for (i=0; i < ( AMSS_BLOCK_SIZE_BITS - (AMSS_GP_SIZE_BITS-1) ); i++)
    {
        if (bBits[i])
            for (j=0; j < AMSS_GP_SIZE_BITS; j++)
                bBits[i+j] ^= bGP[j];
    }

    int iCRCTrueBits=0;
    for (i=AMSS_BLOCK_SIZE_BITS - (AMSS_GP_SIZE_BITS-1); i < AMSS_BLOCK_SIZE_BITS; i++)
    {
        if (bBits[i])
            iCRCTrueBits++;
    }

    if (iCRCTrueBits == 0)
        return true;
    else
        return false;
}



void CAMSSDecode::ApplyOffsetWord(CVector<_BINARY>& bBlockBits, CVector<_BINARY>& bOffset)
{
    for (unsigned short i=0; i < bOffset.Size(); i++)
        bBlockBits[(AMSS_BLOCK_SIZE_BITS-bOffset.Size())+i] ^= bOffset[i];		// XOR with offset work
}

void CRecursiveFilter::Process(CRealVector& vecrIn)
{
    for (int i=0; i < iBlockSize; i++)
    {
        rOut = (AMSS_RECURSIVE_FILTER_GAIN*vecrIn[i]) + ( (1.0F-AMSS_RECURSIVE_FILTER_GAIN)*vecrStore[iCurPos] );
        vecrStore[iCurPos] = rOut;
        //vecrIn[i] = rOut;

        iCurPos++;

        if (iCurPos >= AMSS_12kHz_SAMPLES_PER_BIT)
            iCurPos = 0;
    }
}

void CRecursiveFilter::Init(const int iNewBlockSize)
{
    /* Set internal parameter */
    vecrStore.Init(AMSS_12kHz_SAMPLES_PER_BIT);

    iBlockSize = iNewBlockSize;
}

int CRecursiveFilter::GetPeakPos()
{
    rPeakVal = 0.0;
    iPeakPos = 0;

    for (int i=0; i < AMSS_12kHz_SAMPLES_PER_BIT; i++)
    {
        if (vecrStore[i] > rPeakVal)
        {
            rPeakVal = vecrStore[i];
            iPeakPos = i;
        }
    }

    return iPeakPos;
}

_REAL CRecursiveFilter::GetPeakVal()
{
    rPeakVal = 0.0;

    for (int i=0; i < AMSS_12kHz_SAMPLES_PER_BIT; i++)
    {
        if (vecrStore[i] > rPeakVal)
            rPeakVal = vecrStore[i];
    }

    return rPeakVal;
}

void CAMSSDecode::ResetStatus(CParameter& Parameters)
{
    int i;

    blStoredBlock2Valid = false;
    blFirstEverBlock1 = true;

    iTotalDataEntityGroupSegments = 0;

    iBitsSinceLastBlock1Pass = 0;
    iBitsSinceLastBlock2Pass = 0;
    iBlock1FailCount = 0;
    iBlock2FailCount = 0;

    blBlock1DataValid = false;

    iCurrentBlock = 0;

    for (i=0; i < MAX_DATA_ENTITY_GROUP_SEGMENTS; i++)
        cDataEntityGroupStatus[i] = '_';

    cDataEntityGroupStatus[MAX_DATA_ENTITY_GROUP_SEGMENTS] = '\0';

    for (i=0; i < AMSS_BLOCK_SIZE_BITS; i++)
        cCurrentBlockBits[i] = ' ';

    cCurrentBlockBits[AMSS_BLOCK_SIZE_BITS] = '\0';

    iPercentageDataEntityGroupComplete = 0;

    bDataEntityGroup.Reset(false);
    blDataEntityGroupSegmentsReceived.Reset(0);

    Parameters.ResetServicesStreams();
}


/******************************************************************************\
* AMSS Phase lock loop (PLL)
\******************************************************************************/
void CAMSSPLL::Process(CComplexVector& veccIn, CRealVector& vecrOut)
{
    CReal rPhaseDiff;

    /* Mix it down to zero frequency */
    cvecLow = veccIn;
    Mixer.Process(cvecLow);

    int i;

    for (i=0; i < iBlockSize; i++)
    {
        rvecPhaseTmp[i] = Angle(CComplex(Real(cvecLow[i]), Imag(cvecLow[i])));
    }

    //check for 2PI wrapping
    for (i=0; i < iBlockSize; i++)
    {
        rPhaseDiff = rvecPhaseTmp[i] - rPreviousPhaseSample;

        if (rPhaseDiff >= (crPi))
            rPhaseOffset -=(2.0*crPi);
        else if (rPhaseDiff < (-crPi))
            rPhaseOffset += (2.0*crPi);

        rPreviousPhaseSample = rvecPhaseTmp[i];
        rvecPhaseTmp[i] += rPhaseOffset;

        vecrOut[i] = rvecPhaseTmp[i];
    }

    rvecPhaseTmp = Filter(rvecB, rvecA, rvecPhaseTmp, rvecZPhase);

    /* Calculate current phase for GUI */
    rCurPhase = Mean(rvecPhaseTmp);
    const CReal rOffsEst = rCurPhase;

    rNormCurFreqOffsetAdd = AMSS_PLL_LOOP_GAIN * rOffsEst;

    /* Update mixer */
    Mixer.SetMixFreq(rNormCurFreqOffset + rNormCurFreqOffsetAdd);
}

void CAMSSPLL::Init(const int iNewBlockSize)
{
    /* Set internal parameter */
    iBlockSize = iNewBlockSize;

    /* Init mixer object */
    Mixer.Init(iBlockSize);

    /* Init buffers */
    cvecLow.Init(iBlockSize);
    rvecPhaseTmp.Init(iBlockSize);

    /* Init loop filter (low-pass filter design) */
    /* andrewm - changed so _not_ negative gain! */
    /* IIR filter: H(Z) = ((1 - lam) * z^{-1}) / (1 - lam * z^{-1}) */
    rvecZPhase.Init(2, (CReal) 0.0); /* Memory */
    rvecB.Init(2);
    rvecA.Init(2);
    rvecB[0] = (CReal) 0.0;
    rvecB[1] = ((CReal) 1.0 - AMSS_PLL_LOOP_FILTER_LAMBDA);
    rvecA[0] = (CReal) 1.0;
    rvecA[1] = -AMSS_PLL_LOOP_FILTER_LAMBDA;

    rPreviousPhaseSample = 0;
    rPhaseOffset = 0;
}

void CAMSSPLL::SetRefNormFreq(const CReal rNewNormFreq)
{
    /* Store new reference frequency */
    rNormCurFreqOffset = rNewNormFreq;

    /* Reset offset and phase */
    rNormCurFreqOffsetAdd = (CReal) 0.0;
    rCurPhase = (CReal) 0.0;
}

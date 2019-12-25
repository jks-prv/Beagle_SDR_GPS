/******************************************************************************\
 * Technische Universitaet Darmstadt, Institut fuer Nachrichtentechnik
 * Copyright (c) 2001
 *
 * Author(s):
 *	Volker Fischer
 *
 * Description:
 *	Multi-level-channel (de)coder (MLC)
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

#include "DRM_main.h"
#include "MLC.h"


/* Implementation *************************************************************/
/******************************************************************************\
* MLC-encoder                                                                  *
\******************************************************************************/
void CMLCEncoder::ProcessDataInternal(CParameter&)
{
    int	i, j;
    int iElementCounter;

    /* Energy dispersal ----------------------------------------------------- */
    /* VSPP is treated as a separate part for energy dispersal */
    EnergyDisp.ProcessData(pvecInputData);


    /* Partitioning of input-stream ----------------------------------------- */
    iElementCounter = 0;

    if (iL[2] == 0)
    {
        /* Standard departitioning */
        /* Protection level A */
        for (j = 0; j < iLevels; j++)
        {
            /* Bits */
            for (i = 0; i < iM[j][0]; i++)
            {
                vecEncInBuffer[j][i] =
                    BitToSoft((*pvecInputData)[iElementCounter]);

                iElementCounter++;
            }
        }

        /* Protection level B */
        for (j = 0; j < iLevels; j++)
        {
            /* Bits */
            for (i = 0; i < iM[j][1]; i++)
            {
                vecEncInBuffer[j][iM[j][0] + i] =
                    BitToSoft((*pvecInputData)[iElementCounter]);

                iElementCounter++;
            }
        }
    }
    else
    {
        /* Special partitioning with hierarchical modulation. First set
           hierarchical bits at the beginning, then append the rest */
        /* Hierarchical frame (always "iM[0][1]"). "iM[0][0]" is always "0" in
           this case */
        for (i = 0; i < iM[0][1]; i++)
        {
            vecEncInBuffer[0][i] =
                BitToSoft((*pvecInputData)[iElementCounter]);

            iElementCounter++;
        }


        /* Protection level A (higher protected part) */
        for (j = 1; j < iLevels; j++)
        {
            /* Bits */
            for (i = 0; i < iM[j][0]; i++)
            {
                vecEncInBuffer[j][i] =
                    BitToSoft((*pvecInputData)[iElementCounter]);

                iElementCounter++;
            }
        }

        /* Protection level B  (lower protected part) */
        for (j = 1; j < iLevels; j++)
        {
            /* Bits */
            for (i = 0; i < iM[j][1]; i++)
            {
                vecEncInBuffer[j][iM[j][0] + i] =
                    BitToSoft((*pvecInputData)[iElementCounter]);

                iElementCounter++;
            }
        }
    }


    /* Convolutional encoder ------------------------------------------------ */
    for (j = 0; j < iLevels; j++)
        ConvEncoder[j].Encode(vecEncInBuffer[j], vecEncOutBuffer[j]);


    /* Bit interleaver ------------------------------------------------------ */
    for (j = 0; j < iLevels; j++)
        if (piInterlSequ[j] != -1)
            BitInterleaver[piInterlSequ[j]].Interleave(vecEncOutBuffer[j]);


    /* QAM mapping ---------------------------------------------------------- */
    QAMMapping.Map(vecEncOutBuffer[0],
                   vecEncOutBuffer[1],
                   vecEncOutBuffer[2],
                   vecEncOutBuffer[3],
                   vecEncOutBuffer[4],
                   vecEncOutBuffer[5], pvecOutputData);
}

void CMLCEncoder::InitInternal(CParameter& TransmParam)
{
    int i;
    int	iNumInBits;

    TransmParam.Lock();
    CalculateParam(TransmParam, eChannelType);
    TransmParam.Unlock();

    iNumInBits = iL[0] + iL[1] + iL[2];


    /* Init modules --------------------------------------------------------- */
    /* Energy dispersal */
    EnergyDisp.Init(iNumInBits, iL[2]);

    /* Encoder */
    for (i = 0; i < iLevels; i++)
        ConvEncoder[i].Init(eCodingScheme, eChannelType, iN[0], iN[1],
                            iM[i][0], iM[i][1], iCodeRate[i][0], iCodeRate[i][1], i);

    /* Bit interleaver */
    /* First init all possible interleaver (According table "TableMLC.h" ->
       "Interleaver sequence") */
    if (eCodingScheme == CS_3_HMMIX)
    {
        BitInterleaver[0].Init(iN[0], iN[1], 13);
        BitInterleaver[1].Init(iN[0], iN[1], 21);
    }
    else
    {
        BitInterleaver[0].Init(2 * iN[0], 2 * iN[1], 13);
        BitInterleaver[1].Init(2 * iN[0], 2 * iN[1], 21);
    }

    /* QAM-mapping */
    QAMMapping.Init(iN_mux, eCodingScheme);


    /* Allocate memory for internal bit-buffers ----------------------------- */
    for (i = 0; i < iLevels; i++)
    {
        /* Buffers for each encoder on all different levels */
        /* Add bits from higher protected and lower protected part */
        vecEncInBuffer[i].Init(iM[i][0] + iM[i][1]);

        /* Encoder output buffers for all levels. Must have the same length */
        vecEncOutBuffer[i].Init(iNumEncBits);
    }

    /* Define block-size for input and output */
    iInputBlockSize = iNumInBits;
    iOutputBlockSize = iN_mux;
}


/******************************************************************************\
* MLC-decoder                                                                  *
\******************************************************************************/
void CMLCDecoder::ProcessDataInternal(CParameter&)
{
    int			i, j, k;
    int			iElementCounter;
    bool	bIteration;

    /* Save input signal for signal constellation. We cannot use the copy
       operator of vector because the input vector is not of the same size as
       our intermediate buffer, therefore the "for"-loop */
    for (i = 0; i < iInputBlockSize; i++)
        vecSigSpacBuf[i] = (*pvecInputData)[i].cSig;



#if 0
// TEST
    static FILE* pFile = fopen("test/constellation.dat", "w");
    if (eChannelType == CParameter::CT_MSC) {
        for (i = 0; i < iInputBlockSize; i++)
            fprintf(pFile, "%e %e\n", vecSigSpacBuf[i].real(), vecSigSpacBuf[i].imag());
        fflush(pFile);
    }
// close all;load constellation.dat;constellation=complex(constellation(:,1),constellation(:,2));plot(constellation,'.')
#endif




    /* Iteration loop */
    for (k = 0; k < iNumIterations + 1; k++)
    {
        for (j = 0; j < iLevels; j++)
        {
            drm_next_task("MLCDecoder-level");
            /* Metric ------------------------------------------------------- */
            if (k > 0)
                bIteration = true;
            else
                bIteration = false;

            MLCMetric.CalculateMetric(pvecInputData, vecMetric,
                                      vecSubsetDef[0], vecSubsetDef[1], vecSubsetDef[2],
                                      vecSubsetDef[3], vecSubsetDef[4], vecSubsetDef[5],
                                      j, bIteration);


            /* Bit deinterleaver -------------------------------------------- */
            if (piInterlSequ[j] != -1)
                BitDeinterleaver[piInterlSequ[j]].Deinterleave(vecMetric);


            /* Viterbi decoder ---------------------------------------------- */
            #define MEAS_VITERBI
            #ifdef MEAS_VITERBI
                MEASURE_TIME("v", 3, rAccMetric = ViterbiDecoder[j].Decode(vecMetric, vecDecOutBits[j]));
            #else
                rAccMetric = ViterbiDecoder[j].Decode(vecMetric, vecDecOutBits[j])
            #endif

            /* The last branch of encoding and interleaving must not be used at
               the very last loop */
            /* "iLevels - 1" for iLevels = 1, 2, 3
               "iLevels - 2" for iLevels = 6 */
            if ((k < iNumIterations) ||
                    ((k == iNumIterations) && !(j >= iIndexLastBranch)))
            {
                /* Convolutional encoder ------------------------------------ */
                ConvEncoder[j].Encode(vecDecOutBits[j], vecSubsetDef[j]);


                /* Bit interleaver ------------------------------------------ */
                if (piInterlSequ[j] != -1)
                {
                    BitInterleaver[piInterlSequ[j]].
                    Interleave(vecSubsetDef[j]);
                }
            }
        }
    }


    /* De-partitioning of input-stream -------------------------------------- */
    iElementCounter = 0;

    if (iL[2] == 0)
    {
        drm_next_task("MLCDecoder-departition-std");
        /* Standard departitioning */
        /* Protection level A (higher protected part) */
        for (j = 0; j < iLevels; j++)
        {
            /* Bits */
            for (i = 0; i < iM[j][0]; i++)
            {
                (*pvecOutputData)[iElementCounter] =
                    ExtractBit(vecDecOutBits[j][i]);

                iElementCounter++;
            }
        }

        /* Protection level B (lower protected part) */
        for (j = 0; j < iLevels; j++)
        {
            /* Bits */
            for (i = 0; i < iM[j][1]; i++)
            {
                (*pvecOutputData)[iElementCounter] =
                    ExtractBit(vecDecOutBits[j][iM[j][0] + i]);

                iElementCounter++;
            }
        }
    }
    else
    {
        /* Special departitioning with hierarchical modulation. First set
           hierarchical bits at the beginning, then append the rest */
        /* Hierarchical frame (always "iM[0][1]"). "iM[0][0]" is always "0" in
           this case */
        drm_next_task("MLCDecoder-departition-special");
        for (i = 0; i < iM[0][1]; i++)
        {
            (*pvecOutputData)[iElementCounter] =
                ExtractBit(vecDecOutBits[0][i]);

            iElementCounter++;
        }

        /* Protection level A (higher protected part) */
        for (j = 1; j < iLevels; j++)
        {
            /* Bits */
            for (i = 0; i < iM[j][0]; i++)
            {
                (*pvecOutputData)[iElementCounter] =
                    ExtractBit(vecDecOutBits[j][i]);

                iElementCounter++;
            }
        }

        /* Protection level B (lower protected part) */
        for (j = 1; j < iLevels; j++)
        {
            /* Bits */
            for (i = 0; i < iM[j][1]; i++)
            {
                (*pvecOutputData)[iElementCounter] =
                    ExtractBit(vecDecOutBits[j][iM[j][0] + i]);

                iElementCounter++;
            }
        }
    }


    /* Energy dispersal ----------------------------------------------------- */
    /* VSPP is treated as a separate part for energy dispersal (7.2.2) */
    drm_next_task("MLCDecoder-energy-dispersal");
    EnergyDisp.ProcessData(pvecOutputData);
}

void CMLCDecoder::InitInternal(CParameter& Parameters)
{
    int i;


    /* First, calculate all necessary parameters for decoding process */
    Parameters.Lock();
    CalculateParam(Parameters, eChannelType);
    Parameters.Unlock();

    /* Reasonable number of iterations depends on coding scheme. With a
       4-QAM no iteration is possible */
    if (eCodingScheme == CS_1_SM)
        iNumIterations = 0;
    else
        iNumIterations = iInitNumIterations;

    /* Set this parameter to identify the last level of coder (important for
       very last loop */
    if (eCodingScheme == CS_3_HMMIX)
        iIndexLastBranch = iLevels - 2;
    else
        iIndexLastBranch = iLevels - 1;

    iNumOutBits = iL[0] + iL[1] + iL[2];

    /* Reset accumulated metric for reliability test of transmission */
    rAccMetric = (_REAL) 0.0;


    /* Init modules --------------------------------------------------------- */
    /* Energy dispersal */
    EnergyDisp.Init(iNumOutBits, iL[2]);

    /* Viterbi decoder */
    for (i = 0; i < iLevels; i++)
        ViterbiDecoder[i].Init(eCodingScheme, eChannelType, iN[0], iN[1],
                               iM[i][0], iM[i][1], iCodeRate[i][0], iCodeRate[i][1], i);

    /* Encoder */
    for (i = 0; i < iLevels; i++)
        ConvEncoder[i].Init(eCodingScheme, eChannelType, iN[0], iN[1],
                            iM[i][0], iM[i][1], iCodeRate[i][0], iCodeRate[i][1], i);

    /* Bit interleaver */
    /* First init all possible interleaver (According table "TableMLC.h" ->
       "Interleaver sequence") */
    if (eCodingScheme == CS_3_HMMIX)
    {
        BitDeinterleaver[0].Init(iN[0], iN[1], 13);
        BitDeinterleaver[1].Init(iN[0], iN[1], 21);
        BitInterleaver[0].Init(iN[0], iN[1], 13);
        BitInterleaver[1].Init(iN[0], iN[1], 21);
    }
    else
    {
        BitDeinterleaver[0].Init(2 * iN[0], 2 * iN[1], 13);
        BitDeinterleaver[1].Init(2 * iN[0], 2 * iN[1], 21);
        BitInterleaver[0].Init(2 * iN[0], 2 * iN[1], 13);
        BitInterleaver[1].Init(2 * iN[0], 2 * iN[1], 21);
    }

    /* Metric */
    MLCMetric.Init(iN_mux, eCodingScheme);


    /* Allocate memory for internal bit (metric) -buffers ------------------- */
    vecMetric.Init(iNumEncBits);

    /* Decoder output buffers for all levels. Have different length */
    for (i = 0; i < iLevels; i++)
        vecDecOutBits[i].Init(iM[i][0] + iM[i][1]);

    /* Buffers for subset definition (always number of encoded bits long) */
    for (i = 0; i < MC_MAX_NUM_LEVELS; i++)
        vecSubsetDef[i].Init(iNumEncBits);

    /* Init buffer for signal space */
    vecSigSpacBuf.Init(iN_mux);

    /* Define block-size for input and output */
    iInputBlockSize = iN_mux;
    iOutputBlockSize = iNumOutBits;
}

void CMLCDecoder::GetVectorSpace(CVector<_COMPLEX>& veccData)
{
    /* Init output vectors */
    veccData.Init(iN_mux);

    /* Do copying of data only if vector is of non-zero length which means that
       the module was already initialized */
    if (iN_mux != 0)
    {
        /* Lock resources */
        Lock();

        /* Copy vectors */
        for (int i = 0; i < iN_mux; i++)
            veccData[i] = vecSigSpacBuf[i];

        /* Release resources */
        Unlock();
    }
}


/******************************************************************************\
* MLC base class                                                               *
\******************************************************************************/
void CMLC::CalculateParam(CParameter& Parameter, int iNewChannelType)
{
    int i;
    int iMSCDataLenPartA;

    switch (iNewChannelType)
    {
        /* FAC ********************************************************************/
    case CT_FAC:
        eCodingScheme = CS_1_SM;
        iN_mux = NUM_FAC_CELLS;

        iNumEncBits = NUM_FAC_CELLS * 2;

        iLevels = 1;

        /* Code rates for prot.-Level A and B for each level */
        /* Protection Level A */
        iCodeRate[0][0] = 0;

        /* Protection Level B */
        iCodeRate[0][1] = iCodRateCombFDC4SM;

        /* Define interleaver sequence for all levels */
        piInterlSequ = iInterlSequ4SM;


        /* iN: Number of OFDM-cells of each protection level ---------------- */
        iN[0] = 0;
        iN[1] = iN_mux;


        /* iM: Number of bits each level ------------------------------------ */
        iM[0][0] = 0;
        iM[0][1] = NUM_FAC_BITS_PER_BLOCK;


        /* iL: Number of bits each protection level ------------------------- */
        /* Higher protected part */
        iL[0] = 0;

        /* Lower protected part */
        iL[1] = iM[0][1];

        /* Very strong protected part (VSPP) */
        iL[2] = 0;
        break;


        /* SDC ********************************************************************/
    case CT_SDC:
        eCodingScheme = Parameter.eSDCCodingScheme;
        iN_mux = Parameter.CellMappingTable.iNumSDCCellsPerSFrame;

        iNumEncBits = iN_mux * 2;

        switch (eCodingScheme)
        {
        case CS_1_SM:
            iLevels = 1;

            /* Code rates for prot.-Level A and B for each level */
            /* Protection Level A */
            iCodeRate[0][0] = 0;

            /* Protection Level B */
            iCodeRate[0][1] = iCodRateCombSDC4SM;

            /* Define interleaver sequence for all levels */
            piInterlSequ = iInterlSequ4SM;


            /* iN: Number of OFDM-cells of each protection level ------------ */
            iN[0] = 0;
            iN[1] = iN_mux;


            /* iM: Number of bits each level -------------------------------- */
            iM[0][0] = 0;

            /* M_0,2 = RX_0 * floor((2 * N_SDC - 12) / RY_0) */
            iM[0][1] = iPuncturingPatterns[iCodRateCombSDC4SM][0] *
                       (int) ((_REAL) (2 * iN_mux - 12) /
                              iPuncturingPatterns[iCodRateCombSDC4SM][1]);


            /* iL: Number of bits each protection level --------------------- */
            /* Higher protected part */
            iL[0] = 0;

            /* Lower protected part */
            iL[1] = iM[0][1];

            /* Very strong protected part (VSPP) */
            iL[2] = 0;
            break;

        case CS_2_SM:
            iLevels = 2;

            /* Code rates for prot.-Level A and B for each level */
            for (i = 0; i < 2; i++)
            {
                /* Protection Level A */
                iCodeRate[i][0] = 0;

                /* Protection Level B */
                iCodeRate[i][1] = iCodRateCombSDC16SM[i];
            }

            /* Define interleaver sequence for all levels */
            piInterlSequ = iInterlSequ16SM;


            /* iN: Number of OFDM-cells of each protection level ------------ */
            iN[0] = 0;
            iN[1] = iN_mux;


            /* iM: Number of bits each level -------------------------------- */
            /* M_p,2 = RX_p * floor((N_2 - 6) / RY_p) */
            for (i = 0; i < 2; i++)
            {
                iM[i][0] = 0;

                /* M_p,2 = RX_p * floor((2 * N_SDC - 12) / RY_p) */
                iM[i][1] = iPuncturingPatterns[iCodRateCombSDC16SM[i]][0] *
                           (int) ((_REAL) (2 * iN[1] - 12) /
                                  iPuncturingPatterns[iCodRateCombSDC16SM[i]][1]);
            }


            /* iL: Number of bits each protection level --------------------- */
            /* Higher protected part */
            iL[0] = 0;

            /* Lower protected part */
            iL[1] = iM[0][1] + iM[1][1];

            /* Very strong protected part (VSPP) */
            iL[2] = 0;
            break;

        default:
            break;
        }

        /* Set number of bits for one SDC-block */
        Parameter.SetNumDecodedBitsSDC(iL[1]);
        break;


        /* MSC ********************************************************************/
    case CT_MSC:
        eCodingScheme = Parameter.eMSCCodingScheme;
        iN_mux = Parameter.CellMappingTable.iNumUsefMSCCellsPerFrame;

        /* Data length for part A is the sum of all lengths of the streams */
        iMSCDataLenPartA = Parameter.Stream[0].iLenPartA +
                           Parameter.Stream[1].iLenPartA +
                           Parameter.Stream[2].iLenPartA +
                           Parameter.Stream[3].iLenPartA;

        switch (eCodingScheme)
        {
        case CS_2_SM:
            iLevels = 2;

            /* Code rates for prot.-Level A and B for each level */
            for (i = 0; i < 2; i++)
            {
                /* Protection Level A */
                iCodeRate[i][0] =
                    iCodRateCombMSC16SM[Parameter.MSCPrLe.iPartA][i];

                /* Protection Level B */
                iCodeRate[i][1] =
                    iCodRateCombMSC16SM[Parameter.MSCPrLe.iPartB][i];
            }

            /* Define interleaver sequence for all levels */
            piInterlSequ = iInterlSequ16SM;

            iNumEncBits = iN_mux * 2;


            /* iN: Number of OFDM-cells of each protection level ------------ */
            /* N_1 = ceil(8 * X / (2 * RY_Icm * sum(R_p)) * RY_Icm */
            iN[0] = (int) ceil(8 * (_REAL) iMSCDataLenPartA / (2 *
                               /* RY_Icm */
                               (_REAL) iCodRateCombMSC16SM[Parameter.MSCPrLe.iPartA][2] *
                               (
                                   /* R_0 */
                                   (_REAL) iPuncturingPatterns[iCodRateCombMSC16SM[
                                                                   Parameter.MSCPrLe.iPartA][0]][0] /
                                   iPuncturingPatterns[iCodRateCombMSC16SM[
                                                           Parameter.MSCPrLe.iPartA][0]][1] +
                                   /* R_1 */
                                   (_REAL) iPuncturingPatterns[iCodRateCombMSC16SM[
                                                                   Parameter.MSCPrLe.iPartA][1]][0] /
                                   iPuncturingPatterns[iCodRateCombMSC16SM[
                                                           Parameter.MSCPrLe.iPartA][1]][1]))) *
                    /* RY_Icm */
                    iCodRateCombMSC16SM[Parameter.MSCPrLe.iPartA][2];

            /* Check if result can be possible, if not -> correct. This can
               happen, if a wrong number is in "Param.Stream[x].iLenPartA" */
            if (iN[0] > iN_mux)
                iN[0] = 0;

            iN[1] = iN_mux - iN[0];


            /* iM: Number of bits each level -------------------------------- */
            for (i = 0; i < 2; i++)
            {
                /* M_p,1 = 2 * N_1 * R_p */
                iM[i][0] = (int) (2 * iN[0] *
                                  (_REAL) iPuncturingPatterns[iCodRateCombMSC16SM[
                                                                  Parameter.MSCPrLe.iPartA][i]][0] /
                                  iPuncturingPatterns[iCodRateCombMSC16SM[
                                                          Parameter.MSCPrLe.iPartA][i]][1]);

                /* M_p,2 = RX_p * floor((2 * N_2 - 12) / RY_p) */
                iM[i][1] =
                    iPuncturingPatterns[iCodRateCombMSC16SM[
                                            Parameter.MSCPrLe.iPartB][i]][0] *
                    (int) ((_REAL) (2 * iN[1] - 12) /
                           iPuncturingPatterns[iCodRateCombMSC16SM[
                                                   Parameter.MSCPrLe.iPartB][i]][1]);
            }


            /* iL: Number of bits each protection level --------------------- */
            /* Higher protected part */
            iL[0] = iM[0][0] + iM[1][0];

            /* Lower protected part */
            iL[1] = iM[0][1] + iM[1][1];

            /* Very strong protected part (VSPP) */
            iL[2] = 0;
            break;

        case CS_3_SM:
            iLevels = 3;

            /* Code rates for prot.-Level A and B for each level */
            for (i = 0; i < 3; i++)
            {
                /* Protection Level A */
                iCodeRate[i][0] =
                    iCodRateCombMSC64SM[Parameter.MSCPrLe.iPartA][i];

                /* Protection Level B */
                iCodeRate[i][1] =
                    iCodRateCombMSC64SM[Parameter.MSCPrLe.iPartB][i];
            }

            /* Define interleaver sequence for all levels */
            piInterlSequ = iInterlSequ64SM;

            iNumEncBits = iN_mux * 2;


            /* iN: Number of OFDM-cells of each protection level ------------ */
            /* N_1 = ceil(8 * X / (2 * RY_Icm * sum(R_p)) * RY_Icm */
            iN[0] = (int) ceil(8 * (_REAL) iMSCDataLenPartA / (2 *
                               /* RY_Icm */
                               (_REAL) iCodRateCombMSC64SM[Parameter.MSCPrLe.iPartA][3] *
                               (
                                   /* R_0 */
                                   (_REAL) iPuncturingPatterns[iCodRateCombMSC64SM[
                                                                   Parameter.MSCPrLe.iPartA][0]][0] /
                                   iPuncturingPatterns[iCodRateCombMSC64SM[
                                                           Parameter.MSCPrLe.iPartA][0]][1] +
                                   /* R_1 */
                                   (_REAL) iPuncturingPatterns[iCodRateCombMSC64SM[
                                                                   Parameter.MSCPrLe.iPartA][1]][0] /
                                   iPuncturingPatterns[iCodRateCombMSC64SM[
                                                           Parameter.MSCPrLe.iPartA][1]][1] +
                                   /* R_2 */
                                   (_REAL) iPuncturingPatterns[iCodRateCombMSC64SM[
                                                                   Parameter.MSCPrLe.iPartA][2]][0] /
                                   iPuncturingPatterns[iCodRateCombMSC64SM[
                                                           Parameter.MSCPrLe.iPartA][2]][1]))) *
                    /* RY_Icm */
                    iCodRateCombMSC64SM[Parameter.MSCPrLe.iPartA][3];

            /* Check if result can be possible, if not -> correct. This can
               happen, if a wrong number is in "Param.Stream[x].iLenPartA" */
            if (iN[0] > iN_mux)
                iN[0] = 0;

            iN[1] = iN_mux - iN[0];


            /* iM: Number of bits each level -------------------------------- */
            for (i = 0; i < 3; i++)
            {
                /* M_p,1 = 2 * N_1 * R_p */
                iM[i][0] = (int) (2 * iN[0] *
                                  (_REAL) iPuncturingPatterns[iCodRateCombMSC64SM[
                                                                  Parameter.MSCPrLe.iPartA][i]][0] /
                                  iPuncturingPatterns[iCodRateCombMSC64SM[
                                                          Parameter.MSCPrLe.iPartA][i]][1]);

                /* M_p,2 = RX_p * floor((2 * N_2 - 12) / RY_p) */
                iM[i][1] =
                    iPuncturingPatterns[iCodRateCombMSC64SM[
                                            Parameter.MSCPrLe.iPartB][i]][0] *
                    (int) ((_REAL) (2 * iN[1] - 12) /
                           iPuncturingPatterns[iCodRateCombMSC64SM[
                                                   Parameter.MSCPrLe.iPartB][i]][1]);
            }


            /* iL: Number of bits each protection level --------------------- */
            /* Higher protected part */
            iL[0] = iM[0][0] + iM[1][0] + iM[2][0];

            /* Lower protected part */
            iL[1] = iM[0][1] + iM[1][1] + iM[2][1];

            /* Very strong protected part (VSPP) */
            iL[2] = 0;
            break;

        case CS_3_HMSYM:
            iLevels = 3;

            /* Code rates for prot.-Level A and B for each level */
            /* VSPP (Hierachical) */
            iCodeRate[0][0] = 0;
            iCodeRate[0][1] =
                iCodRateCombMSC64HMsym[Parameter.MSCPrLe.iHierarch][0];

            for (i = 1; i < 3; i++)
            {
                /* Protection Level A */
                iCodeRate[i][0] =
                    iCodRateCombMSC64HMsym[Parameter.MSCPrLe.iPartA][i];

                /* Protection Level B */
                iCodeRate[i][1] =
                    iCodRateCombMSC64HMsym[Parameter.MSCPrLe.iPartB][i];
            }

            /* Define interleaver sequence for all levels */
            piInterlSequ = iInterlSequ64HMsym;

            iNumEncBits = iN_mux * 2;


            /* iN: Number of OFDM-cells of each protection level ------------ */
            /* N_1 = ceil(8 * X / (2 * RY_Icm * sum(R_p)) * RY_Icm */
            iN[0] = (int) ceil(8 * (_REAL) iMSCDataLenPartA / (2 *
                               /* RY_Icm */
                               (_REAL) iCodRateCombMSC64HMsym[Parameter.MSCPrLe.iPartA][3] *
                               (
                                   /* R_1 */
                                   (_REAL) iPuncturingPatterns[iCodRateCombMSC64HMsym[
                                                                   Parameter.MSCPrLe.iPartA][1]][0] /
                                   iPuncturingPatterns[iCodRateCombMSC64HMsym[
                                                           Parameter.MSCPrLe.iPartA][1]][1] +
                                   /* R_2 */
                                   (_REAL) iPuncturingPatterns[iCodRateCombMSC64HMsym[
                                                                   Parameter.MSCPrLe.iPartA][2]][0] /
                                   iPuncturingPatterns[iCodRateCombMSC64HMsym[
                                                           Parameter.MSCPrLe.iPartA][2]][1]))) *
                    /* RY_Icm */
                    iCodRateCombMSC64HMsym[Parameter.MSCPrLe.iPartA][3];

            /* Check if result can be possible, if not -> correct. This can
               happen, if a wrong number is in "Param.Stream[x].iLenPartA" */
            if (iN[0] > iN_mux)
                iN[0] = 0;

            iN[1] = iN_mux - iN[0];


            /* iM: Number of bits each level -------------------------------- */
            /* Level 0, contains the VSPP, treated differently */
            /* M_0,1 */
            iM[0][0] = 0;

            /* M_0,2 = RX_0 * floor((2 * (N_1 + N_2) - 12) / RY_0) */
            iM[0][1] =
                iPuncturingPatterns[iCodRateCombMSC64HMsym[
                                        Parameter.MSCPrLe.iHierarch][0]][0] *
                (int) ((_REAL) (2 * (iN[0] + iN[1]) - 12) /
                       iPuncturingPatterns[iCodRateCombMSC64HMsym[
                                               Parameter.MSCPrLe.iHierarch][0]][1]);

            for (i = 1; i < 3; i++)
            {
                /* M_p,1 = 2 * N_1 * R_p */
                iM[i][0] = (int) (2 * iN[0] *
                                  (_REAL) iPuncturingPatterns[iCodRateCombMSC64HMsym[
                                                                  Parameter.MSCPrLe.iPartA][i]][0] /
                                  iPuncturingPatterns[iCodRateCombMSC64HMsym[
                                                          Parameter.MSCPrLe.iPartA][i]][1]);

                /* M_p,2 = RX_p * floor((2 * N_2 - 12) / RY_p) */
                iM[i][1] =
                    iPuncturingPatterns[iCodRateCombMSC64HMsym[
                                            Parameter.MSCPrLe.iPartB][i]][0] *
                    (int) ((_REAL) (2 * iN[1] - 12) /
                           iPuncturingPatterns[iCodRateCombMSC64HMsym[
                                                   Parameter.MSCPrLe.iPartB][i]][1]);
            }


            /* iL: Number of bits each protection level --------------------- */
            /* Higher protected part */
            iL[0] = iM[1][0] + iM[2][0];

            /* Lower protected part */
            iL[1] = iM[1][1] + iM[2][1];

            /* Very strong protected part (VSPP) */
            iL[2] = iM[0][1];
            break;

        case CS_3_HMMIX:
            iLevels = 6;

            /* Code rates for prot.-Level A and B for each level */
            /* VSPP (Hierachical) */
            iCodeRate[0][0] = 0;
            iCodeRate[0][1] =
                iCodRateCombMSC64HMmix[Parameter.MSCPrLe.iHierarch][0];

            for (i = 1; i < 6; i++)
            {
                /* Protection Level A */
                iCodeRate[i][0] =
                    iCodRateCombMSC64HMmix[Parameter.MSCPrLe.iPartA][i];

                /* Protection Level B */
                iCodeRate[i][1] =
                    iCodRateCombMSC64HMmix[Parameter.MSCPrLe.iPartB][i];
            }

            /* Define interleaver sequence for all levels */
            piInterlSequ = iInterlSequ64HMmix;

            iNumEncBits = iN_mux;


            /* iN: Number of OFDM-cells of each protection level ------------ */
            /* N_1 = ceil(8 * X / (RY_Icm * sum(R_p)) * RY_Icm */
            iN[0] = (int) ceil(8 * (_REAL) iMSCDataLenPartA / (
                                   /* RY_Icm */
                                   (_REAL) iCodRateCombMSC64HMmix[Parameter.MSCPrLe.iPartA][6] *
                                   (
                                       /* R_1 */
                                       (_REAL) iPuncturingPatterns[iCodRateCombMSC64HMmix[
                                                                       Parameter.MSCPrLe.iPartA][1]][0] /
                                       iPuncturingPatterns[iCodRateCombMSC64HMmix[
                                                               Parameter.MSCPrLe.iPartA][1]][1] +
                                       /* R_2 */
                                       (_REAL) iPuncturingPatterns[iCodRateCombMSC64HMmix[
                                                                       Parameter.MSCPrLe.iPartA][2]][0] /
                                       iPuncturingPatterns[iCodRateCombMSC64HMmix[
                                                               Parameter.MSCPrLe.iPartA][2]][1] +
                                       /* R_3 */
                                       (_REAL) iPuncturingPatterns[iCodRateCombMSC64HMmix[
                                                                       Parameter.MSCPrLe.iPartA][3]][0] /
                                       iPuncturingPatterns[iCodRateCombMSC64HMmix[
                                                               Parameter.MSCPrLe.iPartA][3]][1] +
                                       /* R_4 */
                                       (_REAL) iPuncturingPatterns[iCodRateCombMSC64HMmix[
                                                                       Parameter.MSCPrLe.iPartA][4]][0] /
                                       iPuncturingPatterns[iCodRateCombMSC64HMmix[
                                                               Parameter.MSCPrLe.iPartA][4]][1] +
                                       /* R_5 */
                                       (_REAL) iPuncturingPatterns[iCodRateCombMSC64HMmix[
                                                                       Parameter.MSCPrLe.iPartA][5]][0] /
                                       iPuncturingPatterns[iCodRateCombMSC64HMmix[
                                                               Parameter.MSCPrLe.iPartA][5]][1]))) *
                    /* RY_Icm */
                    iCodRateCombMSC64HMmix[Parameter.MSCPrLe.iPartA][6];

            /* Check if result can be possible, if not -> correct. This can
               happen, if a wrong number is in "Param.Stream[x].iLenPartA" */
            if (iN[0] > iN_mux)
                iN[0] = 0;

            iN[1] = iN_mux - iN[0];


            /* iM: Number of bits each level -------------------------------- */
            /* Real-parts of level 0, they contain the VSPP and treated
               differently */
            /* M_0,1Re */
            iM[0][0] = 0;

            /* M_0,2Re = RX_0Re * floor((N_1 + N_2 - 12) / RY_0Re) */
            iM[0][1] =
                iPuncturingPatterns[iCodRateCombMSC64HMmix[
                                        Parameter.MSCPrLe.iHierarch][0]][0] *
                (int) ((_REAL) (iN[0] + iN[1] - 12) /
                       iPuncturingPatterns[iCodRateCombMSC64HMmix[
                                               Parameter.MSCPrLe.iHierarch][0]][1]);

            for (i = 1; i < 6; i++)
            {
                /* M_p,1Re;Im = 2 * N_1 * R_pRe;Im */
                iM[i][0] = (int) (iN[0] *
                                  (_REAL) iPuncturingPatterns[iCodRateCombMSC64HMmix[
                                                                  Parameter.MSCPrLe.iPartA][i]][0] /
                                  iPuncturingPatterns[iCodRateCombMSC64HMmix[
                                                          Parameter.MSCPrLe.iPartA][i]][1]);

                /* M_p,2Re;Im =
                   RX_pRe;Im * floor((2 * N_2 - 12) / RY_pRe;Im) */
                iM[i][1] =
                    iPuncturingPatterns[iCodRateCombMSC64HMmix[
                                            Parameter.MSCPrLe.iPartB][i]][0] *
                    (int) ((_REAL) (iN[1] - 12) /
                           iPuncturingPatterns[iCodRateCombMSC64HMmix[
                                                   Parameter.MSCPrLe.iPartB][i]][1]);
            }


            /* iL: Number of bits each protection level --------------------- */
            /* Higher protected part */
            iL[0] = iM[1][0] + iM[2][0] + iM[3][0] + iM[4][0] + iM[5][0];

            /* Lower protected part */
            iL[1] = iM[1][1] + iM[2][1] + iM[3][1] + iM[4][1] + iM[5][1];

            /* Very strong protected part (VSPP) */
            iL[2] = iM[0][1];
            break;

        default:
            break;
        }

        /* Set number of output bits for next module */
        Parameter.SetNumDecodedBitsMSC(iL[0] + iL[1] + iL[2]);

        /* Set total number of bits for hiearchical frame (needed for MSC
           demultiplexer module) */
        Parameter.SetNumBitsHieraFrTot(iL[2]);
        break;
    }
}

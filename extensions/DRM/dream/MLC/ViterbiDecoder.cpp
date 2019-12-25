/******************************************************************************\
 * Technische Universitaet Darmstadt, Institut fuer Nachrichtentechnik
 * Copyright (c) 2001
 *
 * Author(s):
 *	Volker Fischer, Alexander Kurpiers
 *
 * Description:
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

#include "ViterbiDecoder.h"
#include "printf.h"

/* Implementation *************************************************************/
_REAL CViterbiDecoder::Decode(CVector<CDistance>& vecNewDistance,
                              CVector<_DECISION>& vecOutputBits)
{
    int				i;
    int				iDistCnt;
    int				iCurDecState;
    _VITMETRTYPE*	pCurTrelMetric;
    _VITMETRTYPE*	pOldTrelMetric;

    #ifdef USE_SIMD
        /* -------------------------------------------------------------------------
           Since the metric is 8-bit fixed-point type, we need to scale the input
           metrics to avoid overflows */
        /* Calculate average value of input metrics */
        _REAL rAverage = (_REAL) 0.0;
        for (i = 0; i < iNumOutBitsWithMemory; i++)
        {
            rAverage += vecNewDistance[i].rTow0;
            rAverage += vecNewDistance[i].rTow1;
        }
    
        /* Scale input metrics */
        const _REAL rAmp = rAverage / (2 * iNumOutBitsWithMemory) / 10;
        for (i = 0; i < iNumOutBitsWithMemory; i++)
        {
            vecNewDistance[i].rTow0 /= rAmp;
            vecNewDistance[i].rTow1 /= rAmp;
        }
    #endif

    /* Init pointers for old and new trellis state */
    pCurTrelMetric = vecTrelMetric1;
    pOldTrelMetric = vecTrelMetric2;

    /* Reset all metrics in the trellis. We initialize all states exept of
       the zero state with a high metric, because we KNOW that the state "0"
       is the transmitted state */
    pOldTrelMetric[0] = (_VITMETRTYPE) 0;
    for (i = 1; i < MC_NUM_STATES; i++)
        pOldTrelMetric[i] = MC_METRIC_INIT_VALUE;

    /* Reset counter for puncturing and distance (from metric) */
    iDistCnt = 0;


    /* Main loop over all bits ---------------------------------------------- */
    for (i = 0; i < iNumOutBitsWithMemory; i++)
    {
        /* Calculate all possible metrics ----------------------------------- */
        /* There are only a small set of possible puncturing patterns used for
           DRM: 0001, 0101, 0011, 0111, 1111. These need different numbers of
           input bits (increment of "iDistCnt" is dependent on pattern!). To
           optimize the calculation of the metrics, a "subset" of bits are first
           calculated which are used to get the final result. In this case,
           redundancy can be avoided.
           Note, that not all possible bit-combinations are used in the coder,
           only a subset of numbers: 0, 2, 4, 6, 9, 11, 13, 15 (compare numbers
           in the BUTTERFLY() calls) */

        /* Get first position in input vector (is needed for all cases) */
        const int iPos0 = iDistCnt;
        iDistCnt++;

        if (veciTablePuncPat[i] == PP_TYPE_0001)
        {
            /* Pattern 0001 */
            METRICSET(i)[ 0] = vecNewDistance[iPos0].rTow0;
            METRICSET(i)[ 2] = vecNewDistance[iPos0].rTow0;
            METRICSET(i)[ 4] = vecNewDistance[iPos0].rTow0;
            METRICSET(i)[ 6] = vecNewDistance[iPos0].rTow0;
            METRICSET(i)[ 9] = vecNewDistance[iPos0].rTow1;
            METRICSET(i)[11] = vecNewDistance[iPos0].rTow1;
            METRICSET(i)[13] = vecNewDistance[iPos0].rTow1;
            METRICSET(i)[15] = vecNewDistance[iPos0].rTow1;
        }
        else
        {
            /* The following patterns need one more bit */
            const int iPos1 = iDistCnt;
            iDistCnt++;

            /* Calculate "subsets" of bit-combinations. "rIRxx00" means that
               the fist two bits are used, others are x-ed. "IR" stands for
               "intermediate result" */
            const _REAL rIRxx00 =
                vecNewDistance[iPos1].rTow0 + vecNewDistance[iPos0].rTow0;
            const _REAL rIRxx10 =
                vecNewDistance[iPos1].rTow1 + vecNewDistance[iPos0].rTow0;
            const _REAL rIRxx01 =
                vecNewDistance[iPos1].rTow0 + vecNewDistance[iPos0].rTow1;
            const _REAL rIRxx11 =
                vecNewDistance[iPos1].rTow1 + vecNewDistance[iPos0].rTow1;

            if (veciTablePuncPat[i] == PP_TYPE_0101)
            {
                /* Pattern 0101 */
                METRICSET(i)[ 0] = rIRxx00;
                METRICSET(i)[ 2] = rIRxx00;
                METRICSET(i)[ 4] = rIRxx10;
                METRICSET(i)[ 6] = rIRxx10;
                METRICSET(i)[ 9] = rIRxx01;
                METRICSET(i)[11] = rIRxx01;
                METRICSET(i)[13] = rIRxx11;
                METRICSET(i)[15] = rIRxx11;
            }
            else if (veciTablePuncPat[i] == PP_TYPE_0011)
            {
                /* Pattern 0011 */
                METRICSET(i)[ 0] = rIRxx00;
                METRICSET(i)[ 2] = rIRxx10;
                METRICSET(i)[ 4] = rIRxx00;
                METRICSET(i)[ 6] = rIRxx10;
                METRICSET(i)[ 9] = rIRxx01;
                METRICSET(i)[11] = rIRxx11;
                METRICSET(i)[13] = rIRxx01;
                METRICSET(i)[15] = rIRxx11;
            }
            else
            {
                /* The following patterns need one more bit */
                const int iPos2 = iDistCnt;
                iDistCnt++;

                if (veciTablePuncPat[i] == PP_TYPE_0111)
                {
                    /* Pattern 0111 */
                    METRICSET(i)[ 0] = vecNewDistance[iPos2].rTow0 + rIRxx00;
                    METRICSET(i)[ 2] = vecNewDistance[iPos2].rTow0 + rIRxx10;
                    METRICSET(i)[ 4] = vecNewDistance[iPos2].rTow1 + rIRxx00;
                    METRICSET(i)[ 6] = vecNewDistance[iPos2].rTow1 + rIRxx10;
                    METRICSET(i)[ 9] = vecNewDistance[iPos2].rTow0 + rIRxx01;
                    METRICSET(i)[11] = vecNewDistance[iPos2].rTow0 + rIRxx11;
                    METRICSET(i)[13] = vecNewDistance[iPos2].rTow1 + rIRxx01;
                    METRICSET(i)[15] = vecNewDistance[iPos2].rTow1 + rIRxx11;
                }
                else
                {
                    /* Pattern 1111 */
                    /* This pattern needs all four bits */
                    const int iPos3 = iDistCnt;
                    iDistCnt++;

                    /* Calculate "subsets" of bit-combinations. "rIRxx00" means
                       that the last two bits are used, others are x-ed.
                       "IR" stands for "intermediate result" */
                    const _REAL rIR00xx = vecNewDistance[iPos3].rTow0 +
                                          vecNewDistance[iPos2].rTow0;
                    const _REAL rIR10xx = vecNewDistance[iPos3].rTow1 +
                                          vecNewDistance[iPos2].rTow0;
                    const _REAL rIR01xx = vecNewDistance[iPos3].rTow0 +
                                          vecNewDistance[iPos2].rTow1;
                    const _REAL rIR11xx = vecNewDistance[iPos3].rTow1 +
                                          vecNewDistance[iPos2].rTow1;

                    METRICSET(i)[ 0] = rIR00xx + rIRxx00; /* 0 */
                    METRICSET(i)[ 2] = rIR00xx + rIRxx10; /* 2 */
                    METRICSET(i)[ 4] = rIR01xx + rIRxx00; /* 4 */
                    METRICSET(i)[ 6] = rIR01xx + rIRxx10; /* 6 */
                    METRICSET(i)[ 9] = rIR10xx + rIRxx01; /* 9 */
                    METRICSET(i)[11] = rIR10xx + rIRxx11; /* 11 */
                    METRICSET(i)[13] = rIR11xx + rIRxx01; /* 13 */
                    METRICSET(i)[15] = rIR11xx + rIRxx11; /* 15 */
                }
            }
        }


        /* Update trellis --------------------------------------------------- */
        #ifdef USE_SIMD
            /* Use the butterfly unroll for reordering the metrics for SIMD trellis */
            #define BUTTERFLY(cur, next, prev0, prev1, met0, met1) { \
                /* At this point we convert from float to char! No overflow-check is done here */ \
                chMet1[prev0] = (_VITMETRTYPE) METRICSET(i)[met0]; \
                chMet2[prev0] = (_VITMETRTYPE) METRICSET(i)[met1]; \
            }
        #else
            /* c++ version of trellis update */
            #define BUTTERFLY(cur, next, prev0, prev1, met0, met1) { \
                /* First state in this set ------------------------------------ */ \
                /* Calculate metrics from the two previous states, use the old
                metric from the previous states plus the "transition-metric" */ \
                \
                const _VITMETRTYPE rFiStAccMetricPrev0 = pOldTrelMetric[prev0] + METRICSET(i)[met0]; \
                const _VITMETRTYPE  rFiStAccMetricPrev1 = pOldTrelMetric[prev1] + METRICSET(i)[met1]; \
                \
                /* Take path with smallest metric */ \
                if (rFiStAccMetricPrev0 < rFiStAccMetricPrev1) { \
                    /* Save minimum metric for this state and store decision */ \
                    pCurTrelMetric[cur] = rFiStAccMetricPrev0; \
                    matdecDecisions[i][cur] = 0; \
                } else { \
                    /* Save minimum metric for this state and store decision */ \
                    pCurTrelMetric[cur] = rFiStAccMetricPrev1; \
                    matdecDecisions[i][cur] = 1; \
                } \
                \
                /* Second state in this set ----------------------------------- */ \
                /* The only difference is that we swapped the matric sets */ \
                const _VITMETRTYPE rSecStAccMetricPrev0 = pOldTrelMetric[prev0] + METRICSET(i)[met1]; \
                const _VITMETRTYPE  rSecStAccMetricPrev1 = pOldTrelMetric[prev1] + METRICSET(i)[met0]; \
                \
                /* Take path with smallest metric */ \
                if (rSecStAccMetricPrev0 < rSecStAccMetricPrev1) { \
                    /* Save minimum metric for this state and store decision */ \
                    pCurTrelMetric[next] = rSecStAccMetricPrev0; \
                    matdecDecisions[i][next] = 0; \
                } else { \
                    /* Save minimum metric for this state and store decision */ \
                    pCurTrelMetric[next] = rSecStAccMetricPrev1; \
                    matdecDecisions[i][next] = 1; \
                } \
            }
        #endif

        /* Unroll butterflys to avoid loop overhead. For c++ version, the
            actual calculation of the trellis update is done here, for MMX
            version, only the reordering of the new metrics is done here */
        BUTTERFLY( 0,  1,  0, 32,  0, 15)
        BUTTERFLY( 2,  3,  1, 33,  6,  9)
        BUTTERFLY( 4,  5,  2, 34, 11,  4)
        BUTTERFLY( 6,  7,  3, 35, 13,  2)
        BUTTERFLY( 8,  9,  4, 36, 11,  4)
        BUTTERFLY(10, 11,  5, 37, 13,  2)
        BUTTERFLY(12, 13,  6, 38,  0, 15)
        BUTTERFLY(14, 15,  7, 39,  6,  9)
        BUTTERFLY(16, 17,  8, 40,  4, 11)
        BUTTERFLY(18, 19,  9, 41,  2, 13)
        BUTTERFLY(20, 21, 10, 42, 15,  0)
        BUTTERFLY(22, 23, 11, 43,  9,  6)
        BUTTERFLY(24, 25, 12, 44, 15,  0)
        BUTTERFLY(26, 27, 13, 45,  9,  6)
        BUTTERFLY(28, 29, 14, 46,  4, 11)
        BUTTERFLY(30, 31, 15, 47,  2, 13)
        BUTTERFLY(32, 33, 16, 48,  9,  6)
        BUTTERFLY(34, 35, 17, 49, 15,  0)
        BUTTERFLY(36, 37, 18, 50,  2, 13)
        BUTTERFLY(38, 39, 19, 51,  4, 11)
        BUTTERFLY(40, 41, 20, 52,  2, 13)
        BUTTERFLY(42, 43, 21, 53,  4, 11)
        BUTTERFLY(44, 45, 22, 54,  9,  6)
        BUTTERFLY(46, 47, 23, 55, 15,  0)
        BUTTERFLY(48, 49, 24, 56, 13,  2)
        BUTTERFLY(50, 51, 25, 57, 11,  4)
        BUTTERFLY(52, 53, 26, 58,  6,  9)
        BUTTERFLY(54, 55, 27, 59,  0, 15)
        BUTTERFLY(56, 57, 28, 60,  6,  9)
        BUTTERFLY(58, 59, 29, 61,  0, 15)
        BUTTERFLY(60, 61, 30, 62, 13,  2)
        BUTTERFLY(62, 63, 31, 63, 11,  4)

        #undef BUTTERFLY

        #ifdef USE_SIMD
            /* Do actual trellis update in separate file (assembler implementation) */
            #ifdef USE_MMX
                TrellisUpdateMMX
            #endif
            #ifdef USE_SSE2
                TrellisUpdateSSE2
            #endif
                (&matdecDecisions[i][0], pCurTrelMetric, pOldTrelMetric, chMet1, chMet2);
        #endif

        #ifdef USE_MAX_LOG_MAP
            /* Store accumulated metrics for backward Viterbi */
            for (int j = 0; j < MC_NUM_STATES; j++)
                matrAlpha[i][j] = pOldTrelMetric[j];
        #endif

        /* Swap trellis data pointers (old -> new, new -> old) */
        _VITMETRTYPE* pTMPTrelMetric = pCurTrelMetric;
        pCurTrelMetric = pOldTrelMetric;
        pOldTrelMetric = pTMPTrelMetric;
    }


    #ifdef USE_MAX_LOG_MAP
        /* MAX-LOG MAP implementation ------------------------------------------- */
        /* Reset all metrics in the trellis. We initialize all states exept of
           the zero state with a high metric, because we KNOW that the state "0"
           is the transmitted state */
    
        pOldTrelMetric[0] = (_VITMETRTYPE) 0;
        for (i = 1; i < MC_NUM_STATES; i++)
            pOldTrelMetric[i] = MC_METRIC_INIT_VALUE;
    
        /* Backward direction */
        for (i = iNumOutBitsWithMemory - 1; i >= 0; i--)
        {
            /* Variables storing the likelihood of 0 and 1 bit */
            _VITMETRTYPE rL0 = MC_METRIC_INIT_VALUE;
            _VITMETRTYPE rL1 = MC_METRIC_INIT_VALUE;
    
    
            /* Update trellis --------------------------------------------------- */
            #define BUTTERFLY(cur, prev0, prev1, met0, met1) { \
                /* Calculate metrics from the two previous states, use the old
                metric from the previous states plus the "transition-metric" */
                \
                const _VITMETRTYPE rFiStAccMetricPrev0 = pOldTrelMetric[prev0] + METRICSET(i)[met0]; \
                const _VITMETRTYPE rFiStAccMetricPrev1 = pOldTrelMetric[prev1] + METRICSET(i)[met1]; \
                \
                /* Take path with smallest metric */ \
                if (rFiStAccMetricPrev0 < rFiStAccMetricPrev1) { \
                    /* Save minimum metric for this state */ \
                    pCurTrelMetric[cur] = rFiStAccMetricPrev0; \
                    \
                    /* Likelihood for 0 bit: max(alpha + beta) */ \
                    const _VITMETRTYPE rL0tmp = rFiStAccMetricPrev0 + matrAlpha[i][cur]; \
                    \
                    if (rL0tmp < rL0) \
                        rL0 = rL0tmp; \
                } else { \
                    \
                    /* Save minimum metric for this state */ \
                    pCurTrelMetric[cur] = rFiStAccMetricPrev1; \
                    \
                    /* Likelihood for 1 bit: max(alpha + beta) */ \
                    const _VITMETRTYPE rL1tmp = rFiStAccMetricPrev1 + matrAlpha[i][cur]; \
                    \
                    if (rL1tmp < rL1) \
                        rL1 = rL1tmp; \
                } \
            }
    
            /* Unroll butterflys with backwards direction parameters */
            BUTTERFLY( 0,  0,  1,  0, 15)
            BUTTERFLY( 1,  2,  3,  6,  9)
            BUTTERFLY( 2,  4,  5, 11,  4)
            BUTTERFLY( 3,  6,  7, 13,  2)
            BUTTERFLY( 4,  8,  9, 11,  4)
            BUTTERFLY( 5, 10, 11, 13,  2)
            BUTTERFLY( 6, 12, 13,  0, 15)
            BUTTERFLY( 7, 14, 15,  6,  9)
            BUTTERFLY( 8, 16, 17,  4, 11)
            BUTTERFLY( 9, 18, 19,  2, 13)
            BUTTERFLY(10, 20, 21, 15,  0)
            BUTTERFLY(11, 22, 23,  9,  6)
            BUTTERFLY(12, 24, 25, 15,  0)
            BUTTERFLY(13, 26, 27,  9,  6)
            BUTTERFLY(14, 28, 29,  4, 11)
            BUTTERFLY(15, 30, 31,  2, 13)
            BUTTERFLY(16, 32, 33,  9,  6)
            BUTTERFLY(17, 34, 35, 15,  0)
            BUTTERFLY(18, 36, 37,  2, 13)
            BUTTERFLY(19, 38, 39,  4, 11)
            BUTTERFLY(20, 40, 41,  2, 13)
            BUTTERFLY(21, 42, 43,  4, 11)
            BUTTERFLY(22, 44, 45,  9,  6)
            BUTTERFLY(23, 46, 47, 15,  0)
            BUTTERFLY(24, 48, 49, 13,  2)
            BUTTERFLY(25, 50, 51, 11,  4)
            BUTTERFLY(26, 52, 53,  6,  9)
            BUTTERFLY(27, 54, 55,  0, 15)
            BUTTERFLY(28, 56, 57,  6,  9)
            BUTTERFLY(29, 58, 59,  0, 15)
            BUTTERFLY(30, 60, 61, 13,  2)
            BUTTERFLY(31, 62, 63, 11,  4)
            BUTTERFLY(32,  0,  1, 15,  0)
            BUTTERFLY(33,  2,  3,  9,  6)
            BUTTERFLY(34,  4,  5,  4, 11)
            BUTTERFLY(35,  6,  7,  2, 13)
            BUTTERFLY(36,  8,  9,  4, 11)
            BUTTERFLY(37, 10, 11,  2, 13)
            BUTTERFLY(38, 12, 13, 15,  0)
            BUTTERFLY(39, 14, 15,  9,  6)
            BUTTERFLY(40, 16, 17, 11,  4)
            BUTTERFLY(41, 18, 19, 13,  2)
            BUTTERFLY(42, 20, 21,  0, 15)
            BUTTERFLY(43, 22, 23,  6,  9)
            BUTTERFLY(44, 24, 25,  0, 15)
            BUTTERFLY(45, 26, 27,  6,  9)
            BUTTERFLY(46, 28, 29, 11,  4)
            BUTTERFLY(47, 30, 31, 13,  2)
            BUTTERFLY(48, 32, 33,  6,  9)
            BUTTERFLY(49, 34, 35,  0, 15)
            BUTTERFLY(50, 36, 37, 13,  2)
            BUTTERFLY(51, 38, 39, 11,  4)
            BUTTERFLY(52, 40, 41, 13,  2)
            BUTTERFLY(53, 42, 43, 11,  4)
            BUTTERFLY(54, 44, 45,  6,  9)
            BUTTERFLY(55, 46, 47,  0, 15)
            BUTTERFLY(56, 48, 49,  2, 13)
            BUTTERFLY(57, 50, 51,  4, 11)
            BUTTERFLY(58, 52, 53,  9,  6)
            BUTTERFLY(59, 54, 55, 15,  0)
            BUTTERFLY(60, 56, 57,  9,  6)
            BUTTERFLY(61, 58, 59, 15,  0)
            BUTTERFLY(62, 60, 61,  2, 13)
            BUTTERFLY(63, 62, 63,  4, 11)
    
            #undef BUTTERFLY
    
            /* Calculate final soft out value */
            if (i < iNumOutBits)
                vecOutputBits[i] = rL0 - rL1;
            
            /* Swap trellis data pointers (old -> new, new -> old) */
            _VITMETRTYPE* pTMPTrelMetric = pCurTrelMetric;
            pCurTrelMetric = pOldTrelMetric;
            pOldTrelMetric = pTMPTrelMetric;
        }
    #endif  /* USE_MAX_LOG_MAP */


    #ifndef USE_MAX_LOG_MAP
        /* Chainback the decoded bits from trellis (only for MLSE) -------------- */
        /* The end-state is defined by the DRM standard as all-zeros (shift register
           in the encoder is padded with zeros at the end */
        iCurDecState = 0;
        
        for (i = 0; i < iNumOutBits; i++)
        {
            /* Read out decisions "backwards". Mask only first bit, because in MMX
               implementation, all 8 bits of a "char" are set to the decision */
            const _DECISIONTYPE decCurBit =
                matdecDecisions[iNumOutBitsWithMemory - i - 1][iCurDecState] & 1;
        
            /* Calculate next state from previous decoded bit -> shift old data and add new bit */
            iCurDecState = (iCurDecState >> 1) | (decCurBit << 5);
        
            /* Set decisions "backwards" in actual result vector */
            vecOutputBits[iNumOutBits - i - 1] = (_BINARY) decCurBit;
        }
    #endif

    #ifdef USE_SIMD
        /* No accumulated metric available because of normalizing the metric because of fixed-point implementation */
        return (_REAL) 1.0;
    #else
        /* Return normalized accumulated minimum metric */
        return pOldTrelMetric[0] / iDistCnt;
    #endif
}

void CViterbiDecoder::Init(ECodScheme eNewCodingScheme,
                           EChanType eNewChannelType, int iN1,
                           int iN2, int iNewNumOutBitsPartA,
                           int iNewNumOutBitsPartB, int iPunctPatPartA,
                           int iPunctPatPartB, int iLevel)
{
    /* Number of bits out is the sum of all protection levels */
    iNumOutBits = iNewNumOutBitsPartA + iNewNumOutBitsPartB;

    /* Number of out bits including the state memory */
    iNumOutBitsWithMemory = iNumOutBits + MC_CONSTRAINT_LENGTH - 1;

    /* Init vector, storing table for puncturing pattern and generate pattern */
    veciTablePuncPat.Init(iNumOutBitsWithMemory);

    veciTablePuncPat = GenPuncPatTable(eNewCodingScheme, eNewChannelType, iN1,
                                       iN2, iNewNumOutBitsPartA, iNewNumOutBitsPartB, iPunctPatPartA,
                                       iPunctPatPartB, iLevel);

    /* Init vector for storing the decided bits */
    matdecDecisions.Init(iNumOutBitsWithMemory, MC_NUM_STATES);

    #ifdef USE_MAX_LOG_MAP
        /* Matrix is needed for storing the metrics since we use forward and backward Viterbi algorithm */
        matrMetricSet.Init(iNumOutBitsWithMemory, MC_NUM_OUTPUT_COMBINATIONS);
    
        /* Init matrix for storing the accumulated metrics of forward Viterbi */
        matrAlpha.Init(iNumOutBitsWithMemory, MC_NUM_STATES);
    #endif
}

CViterbiDecoder::CViterbiDecoder()
{
#if 0
    /* Create trellis *********************************************************/
    /* Activate this code to generate the table needed for the butterfly calls
       in the processing routine */

    /* We need to analyze 2^(MC_CONSTRAINT_LENGTH - 1) states in the trellis */
    int	i;
    int	iPrev0IndexForw[MC_NUM_STATES];
    int	iPrev1IndexForw[MC_NUM_STATES];
    int	iMetricPrev0Forw[MC_NUM_STATES];
    int	iMetricPrev1Forw[MC_NUM_STATES];
    int	iPrev0IndexBackw[MC_NUM_STATES];
    int	iPrev1IndexBackw[MC_NUM_STATES];
    int	iMetricPrev0Backw[MC_NUM_STATES];
    int	iMetricPrev1Backw[MC_NUM_STATES];

    for (i = 0; i < MC_NUM_STATES; i++)
    {
        /* Define previous states ------------------------------------------- */
        /* We define in this trellis that we shift the bits from right to
           the left. We use the transition-bits which "fall" out of the
           shift-register (forward case) */
        iPrev0IndexForw[i] = (i >> 1);			/* Old state, Leading "0"
												   (automatically inserted by
												   shifting */
        iPrev1IndexForw[i] = (i >> 1)			 /* Old state */
                             | (1 << (MC_CONSTRAINT_LENGTH - 2)); /* New "1", must be on position
													MC_CONSTRAINT_LENGTH - 1 */

        /* We define in this trellis that we shift the bits from left to
           the right. We use the transition-bits which get in the
           shift-register (backward case) */
        /* Mask operator for length of state register of this code
           [... 0, 0, 1, 1, 1, 1, 1, 1] */
        const int iStRegMask = /* "MC_CONSTRAINT_LENGTH - 1" number of ones */
            1 | (1 << 1) | (1 << 2) | (1 << 3) | (1 << 4) | (1 << 5);

        iPrev0IndexBackw[i] = (i << 1) & iStRegMask; /* Old state, Beginning "0"
												        (automatically inserted
														by shifting. Mask result
														to have length of state
														register */
        iPrev1IndexBackw[i] = ((i << 1) & iStRegMask) | 1;	/* New "1", must on
															   first position */


        /* Assign metrics to the transitions from both paths ---------------- */
        /* We define with the metrics the order: [b_3, b_2, b_1, b_0] */
        iMetricPrev0Forw[i] = 0;
        iMetricPrev1Forw[i] = 0;
        iMetricPrev0Backw[i] = 0;
        iMetricPrev1Backw[i] = 0;
        for (int j = 0; j < MC_NUM_OUTPUT_BITS_PER_STEP; j++)
        {
            /* Calculate respective metrics from convolution of state and
               transition bit */
            /* forwards */
            /* "0" */
            iMetricPrev0Forw[i] |= Convolution(
                                       /* Set all states in the shift-register for encoder. Use
                                          current state with a leading "0" (which is automatically
                                          there) */
                                       i
                                       /* Use generator-polynomial j */
                                       , j)
                                   /* Shift generated bit to the correct position */
                                   << j;

            /* "1" */
            iMetricPrev1Forw[i] |= Convolution(
                                       /* Set all states in the shift-register for encoder. Use
                                          current state with a leading "1". The position of this
                                          bit is "MC_CONSTRAINT_LENGTH" (shifting from position 1:
                                          "MC_CONSTRAINT_LENGTH - 1") */
                                       i | (1 << (MC_CONSTRAINT_LENGTH - 1))
                                       /* Use generator-polynomial j */
                                       , j)
                                   /* Shift generated bit to the correct position */
                                   << j;

            /* backwards */
            /* "0" */
            iMetricPrev0Backw[i] |= Convolution(
                                        /* Set all states in the shift-register for encoder. Use
                                           current state with a "0" at beginning (which is automatically
                                           there) */
                                        (i << 1),
                                        /* Use generator-polynomial j */
                                        j)
                                    /* Shift generated bit to the correct position */
                                    << j;

            /* "1" */
            iMetricPrev1Backw[i] |= Convolution(
                                        /* Set all states in the shift-register for encoder. Use
                                           current state with a "1" at beginning */
                                        (i << 1) | 1,
                                        /* Use generator-polynomial j */
                                        j)
                                    /* Shift generated bit to the correct position */
                                    << j;
        }
    }

    /* Save trellis in file (for substituting in the code) */
    static FILE* pFile = fopen("test/trellis.dat", "w");

    /* forwards */
    fprintf(pFile, "/* forwards */\n");
    for (i = 0; i < MC_NUM_STATES; i += 2)
        fprintf(pFile, "BUTTERFLY(%2d, %2d, %2d, %2d, %2d, %2d)\n", i, i + 1,
                iPrev0IndexForw[i], iPrev1IndexForw[i],
                iMetricPrev0Forw[i], iMetricPrev1Forw[i]);

    /* backwards */
    fprintf(pFile, "\n\n\n/* backwards */\n");
    for (i = 0; i < MC_NUM_STATES; i++)
        fprintf(pFile, "BUTTERFLY(%2d, %2d, %2d, %2d, %2d)\n", i,
                iPrev0IndexBackw[i], iPrev1IndexBackw[i],
                iMetricPrev0Backw[i], iMetricPrev1Backw[i]);

    fprintf(pFile, "\n");
    fflush(pFile);
    kiwi_exit(1);
#endif
}

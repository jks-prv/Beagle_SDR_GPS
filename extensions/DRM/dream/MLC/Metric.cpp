/******************************************************************************\
 * Technische Universitaet Darmstadt, Institut fuer Nachrichtentechnik
 * Copyright (c) 2001
 *
 * Author(s):
 *	Volker Fischer
 *
 * Description:
 *
 * The metric is calculated as follows:
 * Using ML: M = |r - s * h| = |h| * |r / h - s|
 * Using MAP: M = |r - s * h|^2 = |h|^2 * |r / h - s|^2
 *
 * Subset definition:
 * [1 2 3]  -> ([vecSubsetDef1 vecSubsetDef2 vecSubsetDef3])
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

#include "Metric.h"


/* Definitions ****************************************************************/
/* Definitions for binary numbers (BI). On the left is most sigificant bit */
#define BI_00	0 /* Two bits */
#define BI_01	1
#define BI_10	2
#define BI_11	3
#define BI_000	0 /* Three bits */
#define BI_001	1
#define BI_010	2
#define BI_011	3
#define BI_100	4
#define BI_101	5
#define BI_110	6
#define BI_111	7


/* Implementation *************************************************************/
void CMLCMetric::CalculateMetric(CVector<CEquSig>* pcInSymb,
                                 CVector<CDistance>& vecMetric,
                                 CVector<_DECISION>& vecSubsetDef1,
                                 CVector<_DECISION>& vecSubsetDef2,
                                 CVector<_DECISION>& vecSubsetDef3,
                                 CVector<_DECISION>& vecSubsetDef4,
                                 CVector<_DECISION>& vecSubsetDef5,
                                 CVector<_DECISION>& vecSubsetDef6,
                                 int iLevel, bool bIteration)
{
    int i, k;
    int iTabInd0;

    switch (eMapType)
    {
    case CS_1_SM:
        /**********************************************************************/
        /* 4QAM	***************************************************************/
        /**********************************************************************/
        /* Metric according DRM-standard: (i_0  q_0) = (y_0,0  y_0,1) */
        for (i = 0, k = 0; i < iInputBlockSize; i++, k += 2)
        {
            /* Real part ---------------------------------------------------- */
            /* Distance to "0" */
            vecMetric[k].rTow0 = Minimum1((*pcInSymb)[i].cSig.real(),
                                          rTableQAM4[0][0], (*pcInSymb)[i].rChan);

            /* Distance to "1" */
            vecMetric[k].rTow1 = Minimum1((*pcInSymb)[i].cSig.real(),
                                          rTableQAM4[1][0], (*pcInSymb)[i].rChan);


            /* Imaginary part ----------------------------------------------- */
            /* Distance to "0" */
            vecMetric[k + 1].rTow0 = Minimum1((*pcInSymb)[i].cSig.imag(),
                                              rTableQAM4[0][1], (*pcInSymb)[i].rChan);

            /* Distance to "1" */
            vecMetric[k + 1].rTow1 = Minimum1((*pcInSymb)[i].cSig.imag(),
                                              rTableQAM4[1][1], (*pcInSymb)[i].rChan);
        }

        break;

    case CS_2_SM:
        /**********************************************************************/
        /* 16QAM **************************************************************/
        /**********************************************************************/
        /* (i_0  i_1  q_0  q_1) = (y_0,0  y_1,0  y_0,1  y_1,1) */
        switch (iLevel)
        {
        case 0:
            for (i = 0, k = 0; i < iInputBlockSize; i++, k += 2)
            {
                if (bIteration)
                {
                    /* Real part -------------------------------------------- */
#ifdef USE_MAX_LOG_MAP
                    vecMetric[k].rTow0 =
                        Minimum2((*pcInSymb)[i].cSig.real(),
                                 rTableQAM16[BI_00 /* [0  0] */][0],
                                 rTableQAM16[BI_01 /* [0  1] */][0],
                                 (*pcInSymb)[i].rChan, vecSubsetDef2[k]);

                    vecMetric[k].rTow1 =
                        Minimum2((*pcInSymb)[i].cSig.real(),
                                 rTableQAM16[BI_10 /* [1  0] */][0],
                                 rTableQAM16[BI_11 /* [1  1] */][0],
                                 (*pcInSymb)[i].rChan, vecSubsetDef2[k]);
#else
                    /* Lowest bit defined by "vecSubsetDef2" */
                    iTabInd0 = ExtractBit(vecSubsetDef2[k]) & 1;

                    /* Distance to "0" */
                    vecMetric[k].rTow0 =
                        Minimum1((*pcInSymb)[i].cSig.real(),
                                 rTableQAM16[iTabInd0][0], (*pcInSymb)[i].rChan);

                    /* Distance to "1" */
                    vecMetric[k].rTow1 =
                        Minimum1((*pcInSymb)[i].cSig.real(),
                                 rTableQAM16[iTabInd0 | (1 << 1)][0],
                                 (*pcInSymb)[i].rChan);
#endif


                    /* Imaginary part --------------------------------------- */
#ifdef USE_MAX_LOG_MAP
                    vecMetric[k + 1].rTow0 = Minimum2((*pcInSymb)[i].cSig.imag(),
                                                      rTableQAM16[BI_00 /* [0  0] */][1],
                                                      rTableQAM16[BI_01 /* [0  1] */][1],
                                                      (*pcInSymb)[i].rChan, vecSubsetDef2[k + 1]);

                    vecMetric[k + 1].rTow1 = Minimum2((*pcInSymb)[i].cSig.imag(),
                                                      rTableQAM16[BI_10 /* [1  0] */][1],
                                                      rTableQAM16[BI_11 /* [1  1] */][1],
                                                      (*pcInSymb)[i].rChan, vecSubsetDef2[k + 1]);
#else
                    /* Lowest bit defined by "vecSubsetDef2" */
                    iTabInd0 = ExtractBit(vecSubsetDef2[k + 1]) & 1;

                    /* Distance to "0" */
                    vecMetric[k + 1].rTow0 =
                        Minimum1((*pcInSymb)[i].cSig.imag(),
                                 rTableQAM16[iTabInd0][1], (*pcInSymb)[i].rChan);

                    /* Distance to "1" */
                    vecMetric[k + 1].rTow1 =
                        Minimum1((*pcInSymb)[i].cSig.imag(),
                                 rTableQAM16[iTabInd0 | (1 << 1)][1],
                                 (*pcInSymb)[i].rChan);
#endif
                }
                else
                {
                    /* There are two possible points for each bit. Both have to
                       be used. In the first step: {i_1}, {q_1} = 0
                       In the second step: {i_1}, {q_1} = 1 */

                    /* Calculate distances */
                    /* Real part */
                    vecMetric[k].rTow0 =
                        Minimum2((*pcInSymb)[i].cSig.real(),
                                 rTableQAM16[BI_00 /* [0  0] */][0],
                                 rTableQAM16[BI_01 /* [0  1] */][0],
                                 (*pcInSymb)[i].rChan);

                    vecMetric[k].rTow1 =
                        Minimum2((*pcInSymb)[i].cSig.real(),
                                 rTableQAM16[BI_10 /* [1  0] */][0],
                                 rTableQAM16[BI_11 /* [1  1] */][0],
                                 (*pcInSymb)[i].rChan);

                    /* Imaginary part */
                    vecMetric[k + 1].rTow0 =
                        Minimum2((*pcInSymb)[i].cSig.imag(),
                                 rTableQAM16[BI_00 /* [0  0] */][1],
                                 rTableQAM16[BI_01 /* [0  1] */][1],
                                 (*pcInSymb)[i].rChan);

                    vecMetric[k + 1].rTow1 =
                        Minimum2((*pcInSymb)[i].cSig.imag(),
                                 rTableQAM16[BI_10 /* [1  0] */][1],
                                 rTableQAM16[BI_11 /* [1  1] */][1],
                                 (*pcInSymb)[i].rChan);
                }
            }

            break;

        case 1:
            for (i = 0, k = 0; i < iInputBlockSize; i++, k += 2)
            {
                /* Real part ------------------------------------------------ */
#ifdef USE_MAX_LOG_MAP
                vecMetric[k].rTow0 =
                    Minimum2((*pcInSymb)[i].cSig.real(),
                             rTableQAM16[BI_00 /* [0  0] */][0],
                             rTableQAM16[BI_10 /* [1  0] */][0],
                             (*pcInSymb)[i].rChan, vecSubsetDef1[k]);

                vecMetric[k].rTow1 =
                    Minimum2((*pcInSymb)[i].cSig.real(),
                             rTableQAM16[BI_01 /* [0  1] */][0],
                             rTableQAM16[BI_11 /* [1  1] */][0],
                             (*pcInSymb)[i].rChan, vecSubsetDef1[k]);
#else
                /* Higest bit defined by "vecSubsetDef1" */
                iTabInd0 = ((ExtractBit(vecSubsetDef1[k]) & 1) << 1);

                /* Distance to "0" */
                vecMetric[k].rTow0 = Minimum1((*pcInSymb)[i].cSig.real(),
                                              rTableQAM16[iTabInd0][0], (*pcInSymb)[i].rChan);

                /* Distance to "1" */
                vecMetric[k].rTow1 = Minimum1((*pcInSymb)[i].cSig.real(),
                                              rTableQAM16[iTabInd0 | 1][0], (*pcInSymb)[i].rChan);
#endif


                /* Imaginary part ------------------------------------------- */
#ifdef USE_MAX_LOG_MAP
                vecMetric[k + 1].rTow0 =
                    Minimum2((*pcInSymb)[i].cSig.imag(),
                             rTableQAM16[BI_00 /* [0  0] */][1],
                             rTableQAM16[BI_10 /* [1  0] */][1],
                             (*pcInSymb)[i].rChan, vecSubsetDef1[k + 1]);

                vecMetric[k + 1].rTow1 =
                    Minimum2((*pcInSymb)[i].cSig.imag(),
                             rTableQAM16[BI_01 /* [0  1] */][1],
                             rTableQAM16[BI_11 /* [1  1] */][1],
                             (*pcInSymb)[i].rChan, vecSubsetDef1[k + 1]);
#else
                /* Higest bit defined by "vecSubsetDef1" */
                iTabInd0 = ((ExtractBit(vecSubsetDef1[k + 1]) & 1) << 1);

                /* Distance to "0" */
                vecMetric[k + 1].rTow0 = Minimum1((*pcInSymb)[i].cSig.imag(),
                                                  rTableQAM16[iTabInd0][1], (*pcInSymb)[i].rChan);

                /* Distance to "1" */
                vecMetric[k + 1].rTow1 = Minimum1((*pcInSymb)[i].cSig.imag(),
                                                  rTableQAM16[iTabInd0 | 1][1], (*pcInSymb)[i].rChan);
#endif
            }

            break;
        }

        break;

    case CS_3_SM:
        /**********************************************************************/
        /* 64QAM SM ***********************************************************/
        /**********************************************************************/
        /* (i_0  i_1  i_2  q_0  q_1  q_2) =
           (y_0,0  y_1,0  y_2,0  y_0,1  y_1,1  y_2,1) */
        switch (iLevel)
        {
        case 0:
            for (i = 0, k = 0; i < iInputBlockSize; i++, k += 2)
            {
                if (bIteration)
                {
                    /* Real part -------------------------------------------- */
#ifdef USE_MAX_LOG_MAP
                    vecMetric[k].rTow0 =
                        Minimum4((*pcInSymb)[i].cSig.real(),
                                 rTableQAM64SM[BI_000 /* [0 0 0] */][0],
                                 rTableQAM64SM[BI_010 /* [0 1 0] */][0],
                                 rTableQAM64SM[BI_001 /* [0 0 1] */][0],
                                 rTableQAM64SM[BI_011 /* [0 1 1] */][0],
                                 (*pcInSymb)[i].rChan,
                                 vecSubsetDef2[k], vecSubsetDef3[k]);

                    vecMetric[k].rTow1 =
                        Minimum4((*pcInSymb)[i].cSig.real(),
                                 rTableQAM64SM[BI_100 /* [1 0 0] */][0],
                                 rTableQAM64SM[BI_110 /* [1 1 0] */][0],
                                 rTableQAM64SM[BI_101 /* [1 0 1] */][0],
                                 rTableQAM64SM[BI_111 /* [1 1 1] */][0],
                                 (*pcInSymb)[i].rChan,
                                 vecSubsetDef2[k], vecSubsetDef3[k]);
#else
                    /* Lowest bit defined by "vecSubsetDef3" next bit defined
                       by "vecSubsetDef2" */
                    iTabInd0 =
                        (ExtractBit(vecSubsetDef3[k]) & 1) |
                        ((ExtractBit(vecSubsetDef2[k]) & 1) << 1);

                    vecMetric[k].rTow0 =
                        Minimum1((*pcInSymb)[i].cSig.real(),
                                 rTableQAM64SM[iTabInd0][0],	(*pcInSymb)[i].rChan);

                    vecMetric[k].rTow1 =
                        Minimum1((*pcInSymb)[i].cSig.real(),
                                 rTableQAM64SM[iTabInd0 | (1 << 2)][0],
                                 (*pcInSymb)[i].rChan);
#endif


                    /* Imaginary part --------------------------------------- */
#ifdef USE_MAX_LOG_MAP
                    vecMetric[k + 1].rTow0 =
                        Minimum4((*pcInSymb)[i].cSig.imag(),
                                 rTableQAM64SM[BI_000 /* [0 0 0] */][1],
                                 rTableQAM64SM[BI_010 /* [0 1 0] */][1],
                                 rTableQAM64SM[BI_001 /* [0 0 1] */][1],
                                 rTableQAM64SM[BI_011 /* [0 1 1] */][1],
                                 (*pcInSymb)[i].rChan,
                                 vecSubsetDef2[k + 1], vecSubsetDef3[k + 1]);

                    vecMetric[k + 1].rTow1 =
                        Minimum4((*pcInSymb)[i].cSig.imag(),
                                 rTableQAM64SM[BI_100 /* [1 0 0] */][1],
                                 rTableQAM64SM[BI_110 /* [1 1 0] */][1],
                                 rTableQAM64SM[BI_101 /* [1 0 1] */][1],
                                 rTableQAM64SM[BI_111 /* [1 1 1] */][1],
                                 (*pcInSymb)[i].rChan,
                                 vecSubsetDef2[k + 1], vecSubsetDef3[k + 1]);
#else
                    /* Lowest bit defined by "vecSubsetDef3" next bit defined
                       by "vecSubsetDef2" */
                    iTabInd0 =
                        (ExtractBit(vecSubsetDef3[k + 1]) & 1) |
                        ((ExtractBit(vecSubsetDef2[k + 1]) & 1) << 1);

                    /* Calculate distances, imaginary part */
                    vecMetric[k + 1].rTow0 =
                        Minimum1((*pcInSymb)[i].cSig.imag(),
                                 rTableQAM64SM[iTabInd0][1],	(*pcInSymb)[i].rChan);

                    vecMetric[k + 1].rTow1 =
                        Minimum1((*pcInSymb)[i].cSig.imag(),
                                 rTableQAM64SM[iTabInd0 | (1 << 2)][1],
                                 (*pcInSymb)[i].rChan);
#endif
                }
                else
                {
                    /* Real part -------------------------------------------- */
                    vecMetric[k].rTow0 =
                        Minimum4((*pcInSymb)[i].cSig.real(),
                                 rTableQAM64SM[BI_000 /* [0 0 0] */][0],
                                 rTableQAM64SM[BI_001 /* [0 0 1] */][0],
                                 rTableQAM64SM[BI_010 /* [0 1 0] */][0],
                                 rTableQAM64SM[BI_011 /* [0 1 1] */][0],
                                 (*pcInSymb)[i].rChan);

                    vecMetric[k].rTow1 =
                        Minimum4((*pcInSymb)[i].cSig.real(),
                                 rTableQAM64SM[BI_100 /* [1 0 0] */][0],
                                 rTableQAM64SM[BI_101 /* [1 0 1] */][0],
                                 rTableQAM64SM[BI_110 /* [1 1 0] */][0],
                                 rTableQAM64SM[BI_111 /* [1 1 1] */][0],
                                 (*pcInSymb)[i].rChan);


                    /* Imaginary part --------------------------------------- */
                    vecMetric[k + 1].rTow0 =
                        Minimum4((*pcInSymb)[i].cSig.imag(),
                                 rTableQAM64SM[BI_000 /* [0 0 0] */][1],
                                 rTableQAM64SM[BI_001 /* [0 0 1] */][1],
                                 rTableQAM64SM[BI_010 /* [0 1 0] */][1],
                                 rTableQAM64SM[BI_011 /* [0 1 1] */][1],
                                 (*pcInSymb)[i].rChan);

                    vecMetric[k + 1].rTow1 =
                        Minimum4((*pcInSymb)[i].cSig.imag(),
                                 rTableQAM64SM[BI_100 /* [1 0 0] */][1],
                                 rTableQAM64SM[BI_101 /* [1 0 1] */][1],
                                 rTableQAM64SM[BI_110 /* [1 1 0] */][1],
                                 rTableQAM64SM[BI_111 /* [1 1 1] */][1],
                                 (*pcInSymb)[i].rChan);
                }
            }

            break;

        case 1:
            for (i = 0, k = 0; i < iInputBlockSize; i++, k += 2)
            {
                if (bIteration)
                {
                    /* Real part -------------------------------------------- */
#ifdef USE_MAX_LOG_MAP
                    vecMetric[k].rTow0 =
                        Minimum4((*pcInSymb)[i].cSig.real(),
                                 rTableQAM64SM[BI_000 /* [0 0 0] */][0],
                                 rTableQAM64SM[BI_100 /* [1 0 0] */][0],
                                 rTableQAM64SM[BI_001 /* [0 0 1] */][0],
                                 rTableQAM64SM[BI_101 /* [1 0 1] */][0],
                                 (*pcInSymb)[i].rChan,
                                 vecSubsetDef1[k], vecSubsetDef3[k]);

                    vecMetric[k].rTow1 =
                        Minimum4((*pcInSymb)[i].cSig.real(),
                                 rTableQAM64SM[BI_010 /* [0 1 0] */][0],
                                 rTableQAM64SM[BI_110 /* [1 1 0] */][0],
                                 rTableQAM64SM[BI_011 /* [0 1 1] */][0],
                                 rTableQAM64SM[BI_111 /* [1 1 1] */][0],
                                 (*pcInSymb)[i].rChan,
                                 vecSubsetDef1[k], vecSubsetDef3[k]);
#else
                    /* Lowest bit defined by "vecSubsetDef3",highest defined
                       by "vecSubsetDef1" */
                    iTabInd0 =
                        ((ExtractBit(vecSubsetDef1[k]) & 1) << 2) |
                        (ExtractBit(vecSubsetDef3[k]) & 1);

                    vecMetric[k].rTow0 = Minimum1((*pcInSymb)[i].cSig.real(),
                                                  rTableQAM64SM[iTabInd0][0],	(*pcInSymb)[i].rChan);

                    vecMetric[k].rTow1 = Minimum1((*pcInSymb)[i].cSig.real(),
                                                  rTableQAM64SM[iTabInd0 | (1 << 1)][0],
                                                  (*pcInSymb)[i].rChan);
#endif


                    /* Imaginary part --------------------------------------- */
#ifdef USE_MAX_LOG_MAP
                    vecMetric[k + 1].rTow0 =
                        Minimum4((*pcInSymb)[i].cSig.imag(),
                                 rTableQAM64SM[BI_000 /* [0 0 0] */][1],
                                 rTableQAM64SM[BI_100 /* [1 0 0] */][1],
                                 rTableQAM64SM[BI_001 /* [0 0 1] */][1],
                                 rTableQAM64SM[BI_101 /* [1 0 1] */][1],
                                 (*pcInSymb)[i].rChan,
                                 vecSubsetDef1[k + 1], vecSubsetDef3[k + 1]);

                    vecMetric[k + 1].rTow1 =
                        Minimum4((*pcInSymb)[i].cSig.imag(),
                                 rTableQAM64SM[BI_010 /* [0 1 0] */][1],
                                 rTableQAM64SM[BI_110 /* [1 1 0] */][1],
                                 rTableQAM64SM[BI_011 /* [0 1 1] */][1],
                                 rTableQAM64SM[BI_111 /* [1 1 1] */][1],
                                 (*pcInSymb)[i].rChan,
                                 vecSubsetDef1[k + 1], vecSubsetDef3[k + 1]);
#else
                    /* Lowest bit defined by "vecSubsetDef3",highest defined
                       by "vecSubsetDef1" */
                    iTabInd0 =
                        ((ExtractBit(vecSubsetDef1[k + 1]) & 1) << 2) |
                        (ExtractBit(vecSubsetDef3[k + 1]) & 1);

                    vecMetric[k + 1].rTow0 =
                        Minimum1((*pcInSymb)[i].cSig.imag(),
                                 rTableQAM64SM[iTabInd0][1],
                                 (*pcInSymb)[i].rChan);

                    vecMetric[k + 1].rTow1 =
                        Minimum1((*pcInSymb)[i].cSig.imag(),
                                 rTableQAM64SM[iTabInd0 | (1 << 1)][1],
                                 (*pcInSymb)[i].rChan);
#endif
                }
                else
                {
                    /* There are two possible points for each bit. Both have to
                       be used. In the first step: {i_2} = 0, Higest bit
                       defined by "vecSubsetDef1" */

                    /* Real part -------------------------------------------- */
#ifdef USE_MAX_LOG_MAP
                    vecMetric[k].rTow0 =
                        Minimum4((*pcInSymb)[i].cSig.real(),
                                 rTableQAM64SM[BI_000 /* [0 0 0] */][0],
                                 rTableQAM64SM[BI_100 /* [1 0 0] */][0],
                                 rTableQAM64SM[BI_001 /* [0 0 1] */][0],
                                 rTableQAM64SM[BI_101 /* [1 0 1] */][0],
                                 (*pcInSymb)[i].rChan,
                                 vecSubsetDef1[k]);

                    vecMetric[k].rTow1 =
                        Minimum4((*pcInSymb)[i].cSig.real(),
                                 rTableQAM64SM[BI_010 /* [0 1 0] */][0],
                                 rTableQAM64SM[BI_110 /* [1 1 0] */][0],
                                 rTableQAM64SM[BI_011 /* [0 1 1] */][0],
                                 rTableQAM64SM[BI_111 /* [1 1 1] */][0],
                                 (*pcInSymb)[i].rChan,
                                 vecSubsetDef1[k]);
#else
                    iTabInd0 = ((ExtractBit(vecSubsetDef1[k]) & 1) << 2);
                    vecMetric[k].rTow0 =
                        Minimum2((*pcInSymb)[i].cSig.real(),
                                 rTableQAM64SM[iTabInd0][0],
                                 rTableQAM64SM[iTabInd0 | 1][0], (*pcInSymb)[i].rChan);

                    iTabInd0 = ((ExtractBit(vecSubsetDef1[k]) & 1) << 2) |
                               (1 << 1);
                    vecMetric[k].rTow1 =
                        Minimum2((*pcInSymb)[i].cSig.real(),
                                 rTableQAM64SM[iTabInd0][0],
                                 rTableQAM64SM[iTabInd0 | 1][0], (*pcInSymb)[i].rChan);
#endif


                    /* Imaginary part --------------------------------------- */
#ifdef USE_MAX_LOG_MAP
                    vecMetric[k + 1].rTow0 =
                        Minimum4((*pcInSymb)[i].cSig.imag(),
                                 rTableQAM64SM[BI_000 /* [0 0 0] */][1],
                                 rTableQAM64SM[BI_100 /* [1 0 0] */][1],
                                 rTableQAM64SM[BI_001 /* [0 0 1] */][1],
                                 rTableQAM64SM[BI_101 /* [1 0 1] */][1],
                                 (*pcInSymb)[i].rChan,
                                 vecSubsetDef1[k + 1]);

                    vecMetric[k + 1].rTow1 =
                        Minimum4((*pcInSymb)[i].cSig.imag(),
                                 rTableQAM64SM[BI_010 /* [0 1 0] */][1],
                                 rTableQAM64SM[BI_110 /* [1 1 0] */][1],
                                 rTableQAM64SM[BI_011 /* [0 1 1] */][1],
                                 rTableQAM64SM[BI_111 /* [1 1 1] */][1],
                                 (*pcInSymb)[i].rChan,
                                 vecSubsetDef1[k + 1]);
#else
                    iTabInd0 = ((ExtractBit(vecSubsetDef1[k + 1]) & 1) << 2);
                    vecMetric[k + 1].rTow0 =
                        Minimum2((*pcInSymb)[i].cSig.imag(),
                                 rTableQAM64SM[iTabInd0][1],
                                 rTableQAM64SM[iTabInd0 | 1][1], (*pcInSymb)[i].rChan);

                    iTabInd0 = ((ExtractBit(vecSubsetDef1[k + 1]) & 1) << 2) |
                               (1 << 1);
                    vecMetric[k + 1].rTow1 =
                        Minimum2((*pcInSymb)[i].cSig.imag(),
                                 rTableQAM64SM[iTabInd0][1],
                                 rTableQAM64SM[iTabInd0 | 1][1], (*pcInSymb)[i].rChan);
#endif
                }
            }

            break;

        case 2:
            for (i = 0, k = 0; i < iInputBlockSize; i++, k += 2)
            {
                /* Real part ------------------------------------------------ */
#ifdef USE_MAX_LOG_MAP
                vecMetric[k].rTow0 =
                    Minimum4((*pcInSymb)[i].cSig.real(),
                             rTableQAM64SM[BI_000 /* [0 0 0] */][0],
                             rTableQAM64SM[BI_100 /* [1 0 0] */][0],
                             rTableQAM64SM[BI_010 /* [0 1 0] */][0],
                             rTableQAM64SM[BI_110 /* [1 1 0] */][0],
                             (*pcInSymb)[i].rChan,
                             vecSubsetDef1[k], vecSubsetDef2[k]);

                vecMetric[k].rTow1 =
                    Minimum4((*pcInSymb)[i].cSig.real(),
                             rTableQAM64SM[BI_001 /* [0 0 1] */][0],
                             rTableQAM64SM[BI_101 /* [1 0 1] */][0],
                             rTableQAM64SM[BI_011 /* [0 1 1] */][0],
                             rTableQAM64SM[BI_111 /* [1 1 1] */][0],
                             (*pcInSymb)[i].rChan,
                             vecSubsetDef1[k], vecSubsetDef2[k]);
#else
                /* Higest bit defined by "vecSubsetDef1" next bit defined
                   by "vecSubsetDef2" */
                iTabInd0 =
                    ((ExtractBit(vecSubsetDef1[k]) & 1) << 2) |
                    ((ExtractBit(vecSubsetDef2[k]) & 1) << 1);

                vecMetric[k].rTow0 = Minimum1((*pcInSymb)[i].cSig.real(),
                                              rTableQAM64SM[iTabInd0][0], (*pcInSymb)[i].rChan);

                vecMetric[k].rTow1 = Minimum1((*pcInSymb)[i].cSig.real(),
                                              rTableQAM64SM[iTabInd0 | 1][0], (*pcInSymb)[i].rChan);
#endif


                /* Imaginary part ------------------------------------------- */
#ifdef USE_MAX_LOG_MAP
                vecMetric[k + 1].rTow0 =
                    Minimum4((*pcInSymb)[i].cSig.imag(),
                             rTableQAM64SM[BI_000 /* [0 0 0] */][1],
                             rTableQAM64SM[BI_100 /* [1 0 0] */][1],
                             rTableQAM64SM[BI_010 /* [0 1 0] */][1],
                             rTableQAM64SM[BI_110 /* [1 1 0] */][1],
                             (*pcInSymb)[i].rChan,
                             vecSubsetDef1[k + 1], vecSubsetDef2[k + 1]);

                vecMetric[k + 1].rTow1 =
                    Minimum4((*pcInSymb)[i].cSig.imag(),
                             rTableQAM64SM[BI_001 /* [0 0 1] */][1],
                             rTableQAM64SM[BI_101 /* [1 0 1] */][1],
                             rTableQAM64SM[BI_011 /* [0 1 1] */][1],
                             rTableQAM64SM[BI_111 /* [1 1 1] */][1],
                             (*pcInSymb)[i].rChan,
                             vecSubsetDef1[k + 1], vecSubsetDef2[k + 1]);
#else
                /* Higest bit defined by "vecSubsetDef1" next bit defined
                   by "vecSubsetDef2" */
                iTabInd0 =
                    ((ExtractBit(vecSubsetDef1[k + 1]) & 1) << 2) |
                    ((ExtractBit(vecSubsetDef2[k + 1]) & 1) << 1);

                /* Calculate distances, imaginary part */
                vecMetric[k + 1].rTow0 = Minimum1((*pcInSymb)[i].cSig.imag(),
                                                  rTableQAM64SM[iTabInd0][1], (*pcInSymb)[i].rChan);

                vecMetric[k + 1].rTow1 = Minimum1((*pcInSymb)[i].cSig.imag(),
                                                  rTableQAM64SM[iTabInd0 | 1][1], (*pcInSymb)[i].rChan);
#endif
            }

            break;
        }

        break;

    case CS_3_HMSYM:
        /**********************************************************************/
        /* 64QAM HMsym ********************************************************/
        /**********************************************************************/
        /* (i_0  i_1  i_2  q_0  q_1  q_2) =
           (y_0,0  y_1,0  y_2,0  y_0,1  y_1,1  y_2,1) */
        switch (iLevel)
        {
        case 0:
            for (i = 0, k = 0; i < iInputBlockSize; i++, k += 2)
            {
                if (bIteration)
                {
                    /* Real part -------------------------------------------- */
                    /* Lowest bit defined by "vecSubsetDef3" next bit defined
                       by "vecSubsetDef2" */
                    iTabInd0 =
                        (ExtractBit(vecSubsetDef3[k]) & 1) |
                        ((ExtractBit(vecSubsetDef2[k]) & 1) << 1);

                    vecMetric[k].rTow0 = Minimum1((*pcInSymb)[i].cSig.real(),
                                                  rTableQAM64HMsym[iTabInd0][0], (*pcInSymb)[i].rChan);

                    vecMetric[k].rTow1 = Minimum1((*pcInSymb)[i].cSig.real(),
                                                  rTableQAM64HMsym[iTabInd0 | (1 << 2)][0],
                                                  (*pcInSymb)[i].rChan);


                    /* Imaginary part --------------------------------------- */
                    /* Lowest bit defined by "vecSubsetDef3" next bit defined
                       by "vecSubsetDef2" */
                    iTabInd0 =
                        (ExtractBit(vecSubsetDef3[k + 1]) & 1) |
                        ((ExtractBit(vecSubsetDef2[k + 1]) & 1) << 1);

                    /* Calculate distances, imaginary part */
                    vecMetric[k + 1].rTow0 =
                        Minimum1((*pcInSymb)[i].cSig.imag(),
                                 rTableQAM64HMsym[iTabInd0][1], (*pcInSymb)[i].rChan);

                    vecMetric[k + 1].rTow1 =
                        Minimum1((*pcInSymb)[i].cSig.imag(),
                                 rTableQAM64HMsym[iTabInd0 | (1 << 2)][1],
                                 (*pcInSymb)[i].rChan);
                }
                else
                {
                    /* Real part -------------------------------------------- */
                    vecMetric[k].rTow0 =
                        Minimum4((*pcInSymb)[i].cSig.real(),
                                 rTableQAM64HMsym[BI_000 /* [0 0 0] */][0],
                                 rTableQAM64HMsym[BI_001 /* [0 0 1] */][0],
                                 rTableQAM64HMsym[BI_010 /* [0 1 0] */][0],
                                 rTableQAM64HMsym[BI_011 /* [0 1 1] */][0],
                                 (*pcInSymb)[i].rChan);

                    vecMetric[k].rTow1 =
                        Minimum4((*pcInSymb)[i].cSig.real(),
                                 rTableQAM64HMsym[BI_100 /* [1 0 0] */][0],
                                 rTableQAM64HMsym[BI_101 /* [1 0 1] */][0],
                                 rTableQAM64HMsym[BI_110 /* [1 1 0] */][0],
                                 rTableQAM64HMsym[BI_111 /* [1 1 1] */][0],
                                 (*pcInSymb)[i].rChan);


                    /* Imaginary part --------------------------------------- */
                    vecMetric[k + 1].rTow0 =
                        Minimum4((*pcInSymb)[i].cSig.imag(),
                                 rTableQAM64HMsym[BI_000 /* [0 0 0] */][1],
                                 rTableQAM64HMsym[BI_001 /* [0 0 1] */][1],
                                 rTableQAM64HMsym[BI_010 /* [0 1 0] */][1],
                                 rTableQAM64HMsym[BI_011 /* [0 1 1] */][1],
                                 (*pcInSymb)[i].rChan);

                    vecMetric[k + 1].rTow1 =
                        Minimum4((*pcInSymb)[i].cSig.imag(),
                                 rTableQAM64HMsym[BI_100 /* [1 0 0] */][1],
                                 rTableQAM64HMsym[BI_101 /* [1 0 1] */][1],
                                 rTableQAM64HMsym[BI_110 /* [1 1 0] */][1],
                                 rTableQAM64HMsym[BI_111 /* [1 1 1] */][1],
                                 (*pcInSymb)[i].rChan);
                }
            }

            break;

        case 1:
            for (i = 0, k = 0; i < iInputBlockSize; i++, k += 2)
            {
                if (bIteration)
                {
                    /* Real part -------------------------------------------- */
                    /* Lowest bit defined by "vecSubsetDef3",highest defined
                       by "vecSubsetDef1" */
                    iTabInd0 =
                        ((ExtractBit(vecSubsetDef1[k]) & 1) << 2) |
                        (ExtractBit(vecSubsetDef3[k]) & 1);

                    vecMetric[k].rTow0 = Minimum1((*pcInSymb)[i].cSig.real(),
                                                  rTableQAM64HMsym[iTabInd0][0],
                                                  (*pcInSymb)[i].rChan);

                    vecMetric[k].rTow1 = Minimum1((*pcInSymb)[i].cSig.real(),
                                                  rTableQAM64HMsym[iTabInd0 | (1 << 1)][0],
                                                  (*pcInSymb)[i].rChan);


                    /* Imaginary part --------------------------------------- */
                    /* Lowest bit defined by "vecSubsetDef3",highest defined
                       by "vecSubsetDef1" */
                    iTabInd0 =
                        ((ExtractBit(vecSubsetDef1[k + 1]) & 1) << 2) |
                        (ExtractBit(vecSubsetDef3[k + 1]) & 1);

                    vecMetric[k + 1].rTow0 =
                        Minimum1((*pcInSymb)[i].cSig.imag(),
                                 rTableQAM64HMsym[iTabInd0][1], (*pcInSymb)[i].rChan);

                    vecMetric[k + 1].rTow1 =
                        Minimum1((*pcInSymb)[i].cSig.imag(),
                                 rTableQAM64HMsym[iTabInd0 | (1 << 1)][1],
                                 (*pcInSymb)[i].rChan);
                }
                else
                {
                    /* There are two possible points for each bit. Both have to
                       be used. In the first step: {i_2} = 0, Higest bit
                       defined by "vecSubsetDef1" */

                    /* Real part -------------------------------------------- */
                    iTabInd0 = ((ExtractBit(vecSubsetDef1[k]) & 1) << 2);
                    vecMetric[k].rTow0 =
                        Minimum2((*pcInSymb)[i].cSig.real(),
                                 rTableQAM64HMsym[iTabInd0][0],
                                 rTableQAM64HMsym[iTabInd0 | 1][0],
                                 (*pcInSymb)[i].rChan);

                    iTabInd0 = ((ExtractBit(vecSubsetDef1[k]) & 1) << 2) |
                               (1 << 1);
                    vecMetric[k].rTow1 =
                        Minimum2((*pcInSymb)[i].cSig.real(),
                                 rTableQAM64HMsym[iTabInd0][0],
                                 rTableQAM64HMsym[iTabInd0 | 1][0],
                                 (*pcInSymb)[i].rChan);


                    /* Imaginary part --------------------------------------- */
                    iTabInd0 = ((ExtractBit(vecSubsetDef1[k + 1]) & 1) << 2);
                    vecMetric[k + 1].rTow0 =
                        Minimum2((*pcInSymb)[i].cSig.imag(),
                                 rTableQAM64HMsym[iTabInd0][1],
                                 rTableQAM64HMsym[iTabInd0 | 1][1],
                                 (*pcInSymb)[i].rChan);

                    iTabInd0 = ((ExtractBit(vecSubsetDef1[k + 1]) & 1) << 2) |
                               (1 << 1);
                    vecMetric[k + 1].rTow1 =
                        Minimum2((*pcInSymb)[i].cSig.imag(),
                                 rTableQAM64HMsym[iTabInd0][1],
                                 rTableQAM64HMsym[iTabInd0 | 1][1],
                                 (*pcInSymb)[i].rChan);
                }
            }

            break;

        case 2:
            for (i = 0, k = 0; i < iInputBlockSize; i++, k += 2)
            {
                /* Real part ------------------------------------------------ */
                /* Higest bit defined by "vecSubsetDef1" next bit defined
                   by "vecSubsetDef2" */
                iTabInd0 =
                    ((ExtractBit(vecSubsetDef1[k]) & 1) << 2) |
                    ((ExtractBit(vecSubsetDef2[k]) & 1) << 1);

                vecMetric[k].rTow0 = Minimum1((*pcInSymb)[i].cSig.real(),
                                              rTableQAM64HMsym[iTabInd0][0], (*pcInSymb)[i].rChan);

                vecMetric[k].rTow1 = Minimum1((*pcInSymb)[i].cSig.real(),
                                              rTableQAM64HMsym[iTabInd0 | 1][0], (*pcInSymb)[i].rChan);


                /* Imaginary part ------------------------------------------- */
                /* Higest bit defined by "vecSubsetDef1" next bit defined
                   by "vecSubsetDef2" */
                iTabInd0 =
                    ((ExtractBit(vecSubsetDef1[k + 1]) & 1) << 2) |
                    ((ExtractBit(vecSubsetDef2[k + 1]) & 1) << 1);

                /* Calculate distances, imaginary part */
                vecMetric[k + 1].rTow0 = Minimum1((*pcInSymb)[i].cSig.imag(),
                                                  rTableQAM64HMsym[iTabInd0][1], (*pcInSymb)[i].rChan);

                vecMetric[k + 1].rTow1 = Minimum1((*pcInSymb)[i].cSig.imag(),
                                                  rTableQAM64HMsym[iTabInd0 | 1][1], (*pcInSymb)[i].rChan);
            }

            break;
        }

        break;

    case CS_3_HMMIX:
        /**********************************************************************/
        /* 64QAM HMmix ********************************************************/
        /**********************************************************************/
        /* (i_0  i_1  i_2  q_0  q_1  q_2) =
           (y_0,0Re  y_1,0Re  y_2,0Re  y_0,0Im  y_1,0Im  y_2,0Im) */
        switch (iLevel)
        {
        case 0:
            for (i = 0; i < iInputBlockSize; i++)
            {
                if (bIteration)
                {
                    /* Real part -------------------------------------------- */
                    /* Lowest bit defined by "vecSubsetDef5" next bit defined
                       by "vecSubsetDef3" */
                    iTabInd0 =
                        (ExtractBit(vecSubsetDef5[i]) & 1) |
                        ((ExtractBit(vecSubsetDef3[i]) & 1) << 1);

                    vecMetric[i].rTow0 = Minimum1((*pcInSymb)[i].cSig.real(),
                                                  rTableQAM64HMmix[iTabInd0][0], (*pcInSymb)[i].rChan);

                    vecMetric[i].rTow1 = Minimum1((*pcInSymb)[i].cSig.real(),
                                                  rTableQAM64HMmix[iTabInd0 | (1 << 2)][0],
                                                  (*pcInSymb)[i].rChan);
                }
                else
                {
                    /* Real part -------------------------------------------- */
                    vecMetric[i].rTow0 = Minimum4((*pcInSymb)[i].cSig.real(),
                                                  rTableQAM64HMmix[BI_000 /* [0 0 0] */][0],
                                                  rTableQAM64HMmix[BI_001 /* [0 0 1] */][0],
                                                  rTableQAM64HMmix[BI_010 /* [0 1 0] */][0],
                                                  rTableQAM64HMmix[BI_011 /* [0 1 1] */][0],
                                                  (*pcInSymb)[i].rChan);

                    vecMetric[i].rTow1 = Minimum4((*pcInSymb)[i].cSig.real(),
                                                  rTableQAM64HMmix[BI_100 /* [1 0 0] */][0],
                                                  rTableQAM64HMmix[BI_101 /* [1 0 1] */][0],
                                                  rTableQAM64HMmix[BI_110 /* [1 1 0] */][0],
                                                  rTableQAM64HMmix[BI_111 /* [1 1 1] */][0],
                                                  (*pcInSymb)[i].rChan);
                }
            }

            break;

        case 1:
            for (i = 0; i < iInputBlockSize; i++)
            {
                if (bIteration)
                {
                    /* Imaginary part --------------------------------------- */
                    /* Lowest bit defined by "vecSubsetDef6" next bit defined
                       by "vecSubsetDef4" */
                    iTabInd0 =
                        (ExtractBit(vecSubsetDef6[i]) & 1) |
                        ((ExtractBit(vecSubsetDef4[i]) & 1) << 1);

                    /* Calculate distances, imaginary part */
                    vecMetric[i].rTow0 = Minimum1((*pcInSymb)[i].cSig.imag(),
                                                  rTableQAM64HMmix[iTabInd0][1], (*pcInSymb)[i].rChan);

                    vecMetric[i].rTow1 = Minimum1((*pcInSymb)[i].cSig.imag(),
                                                  rTableQAM64HMmix[iTabInd0 | (1 << 2)][1],
                                                  (*pcInSymb)[i].rChan);
                }
                else
                {
                    /* Imaginary part --------------------------------------- */
                    vecMetric[i].rTow0 = Minimum4((*pcInSymb)[i].cSig.imag(),
                                                  rTableQAM64HMmix[BI_000 /* [0 0 0] */][1],
                                                  rTableQAM64HMmix[BI_001 /* [0 0 1] */][1],
                                                  rTableQAM64HMmix[BI_010 /* [0 1 0] */][1],
                                                  rTableQAM64HMmix[BI_011 /* [0 1 1] */][1],
                                                  (*pcInSymb)[i].rChan);

                    vecMetric[i].rTow1 = Minimum4((*pcInSymb)[i].cSig.imag(),
                                                  rTableQAM64HMmix[BI_100 /* [1 0 0] */][1],
                                                  rTableQAM64HMmix[BI_101 /* [1 0 1] */][1],
                                                  rTableQAM64HMmix[BI_110 /* [1 1 0] */][1],
                                                  rTableQAM64HMmix[BI_111 /* [1 1 1] */][1],
                                                  (*pcInSymb)[i].rChan);
                }
            }

            break;

        case 2:
            for (i = 0; i < iInputBlockSize; i++)
            {
                if (bIteration)
                {
                    /* Real part -------------------------------------------- */
                    /* Lowest bit defined by "vecSubsetDef5",highest defined
                       by "vecSubsetDef1" */
                    iTabInd0 =
                        ((ExtractBit(vecSubsetDef1[i]) & 1) << 2) |
                        (ExtractBit(vecSubsetDef5[i]) & 1);

                    vecMetric[i].rTow0 = Minimum1((*pcInSymb)[i].cSig.real(),
                                                  rTableQAM64HMmix[iTabInd0][0], (*pcInSymb)[i].rChan);

                    vecMetric[i].rTow1 = Minimum1((*pcInSymb)[i].cSig.real(),
                                                  rTableQAM64HMmix[iTabInd0 | (1 << 1)][0],
                                                  (*pcInSymb)[i].rChan);
                }
                else
                {
                    /* There are two possible points for each bit. Both have to
                       be used. In the first step: {i_2} = 0, Higest bit
                       defined by "vecSubsetDef1" */

                    /* Real part -------------------------------------------- */
                    iTabInd0 = ((ExtractBit(vecSubsetDef1[i]) & 1) << 2);
                    vecMetric[i].rTow0 = Minimum2((*pcInSymb)[i].cSig.real(),
                                                  rTableQAM64HMmix[iTabInd0][0],
                                                  rTableQAM64HMmix[iTabInd0 | 1][0],
                                                  (*pcInSymb)[i].rChan);

                    iTabInd0 =
                        ((ExtractBit(vecSubsetDef1[i]) & 1) << 2) | (1 << 1);
                    vecMetric[i].rTow1 = Minimum2((*pcInSymb)[i].cSig.real(),
                                                  rTableQAM64HMmix[iTabInd0][0],
                                                  rTableQAM64HMmix[iTabInd0 | 1][0],
                                                  (*pcInSymb)[i].rChan);
                }
            }

            break;

        case 3:
            for (i = 0; i < iInputBlockSize; i++)
            {
                if (bIteration)
                {
                    /* Imaginary part --------------------------------------- */
                    /* Lowest bit defined by "vecSubsetDef6",highest defined
                       by "vecSubsetDef2" */
                    iTabInd0 =
                        ((ExtractBit(vecSubsetDef2[i]) & 1) << 2) |
                        (ExtractBit(vecSubsetDef6[i]) & 1);

                    vecMetric[i].rTow0 = Minimum1((*pcInSymb)[i].cSig.imag(),
                                                  rTableQAM64HMmix[iTabInd0][1], (*pcInSymb)[i].rChan);

                    vecMetric[i].rTow1 = Minimum1((*pcInSymb)[i].cSig.imag(),
                                                  rTableQAM64HMmix[iTabInd0 | (1 << 1)][1],
                                                  (*pcInSymb)[i].rChan);
                }
                else
                {
                    /* There are two possible points for each bit. Both have to
                       be used. In the first step: {i_2} = 0, Higest bit
                       defined by "vecSubsetDef1" */

                    /* Imaginary part ------------------------------------------- */
                    iTabInd0 = ((ExtractBit(vecSubsetDef2[i]) & 1) << 2);
                    vecMetric[i].rTow0 = Minimum2((*pcInSymb)[i].cSig.imag(),
                                                  rTableQAM64HMmix[iTabInd0][1],
                                                  rTableQAM64HMmix[iTabInd0 | 1][1], (*pcInSymb)[i].rChan);

                    iTabInd0 =
                        ((ExtractBit(vecSubsetDef2[i]) & 1) << 2) | (1 << 1);
                    vecMetric[i].rTow1 = Minimum2((*pcInSymb)[i].cSig.imag(),
                                                  rTableQAM64HMmix[iTabInd0][1],
                                                  rTableQAM64HMmix[iTabInd0 | 1][1], (*pcInSymb)[i].rChan);
                }
            }

            break;

        case 4:
            for (i = 0; i < iInputBlockSize; i++)
            {
                /* Real part ------------------------------------------------ */
                /* Higest bit defined by "vecSubsetDef1" next bit defined
                   by "vecSubsetDef2" */
                iTabInd0 =
                    ((ExtractBit(vecSubsetDef1[i]) & 1) << 2) |
                    ((ExtractBit(vecSubsetDef3[i]) & 1) << 1);

                vecMetric[i].rTow0 = Minimum1((*pcInSymb)[i].cSig.real(),
                                              rTableQAM64HMmix[iTabInd0][0], (*pcInSymb)[i].rChan);

                vecMetric[i].rTow1 = Minimum1((*pcInSymb)[i].cSig.real(),
                                              rTableQAM64HMmix[iTabInd0 | 1][0], (*pcInSymb)[i].rChan);
            }

            break;

        case 5:
            for (i = 0; i < iInputBlockSize; i++)
            {
                /* Imaginary part ------------------------------------------- */
                /* Higest bit defined by "vecSubsetDef1" next bit defined
                   by "vecSubsetDef2" */
                iTabInd0 =
                    ((ExtractBit(vecSubsetDef2[i]) & 1) << 2) |
                    ((ExtractBit(vecSubsetDef4[i]) & 1) << 1);

                /* Calculate distances, imaginary part */
                vecMetric[i].rTow0 = Minimum1((*pcInSymb)[i].cSig.imag(),
                                              rTableQAM64HMmix[iTabInd0][1], (*pcInSymb)[i].rChan);

                vecMetric[i].rTow1 = Minimum1((*pcInSymb)[i].cSig.imag(),
                                              rTableQAM64HMmix[iTabInd0 | 1][1], (*pcInSymb)[i].rChan);
            }

            break;
        }

        break;
    }

#ifdef USE_ERASURE_FOR_FASTER_ACQ
    /* Take care of special case with "CS_3_HMMIX" */
    if (eMapType != CS_3_HMMIX)
    {
        for (i = 0; i < iInputBlockSize; i++)
        {
            /* If input symbol is erasure, reset metrics to zero */
            if ((*pcInSymb)[i].rChan == ERASURE_TAG_VALUE)
            {
                vecMetric[2 * i].rTow0 = (_REAL) 0.0;
                vecMetric[2 * i].rTow1 = (_REAL) 0.0;
                vecMetric[2 * i + 1].rTow0 = (_REAL) 0.0;
                vecMetric[2 * i + 1].rTow1 = (_REAL) 0.0;
            }
        }
    }
    else
    {
        for (i = 0; i < iInputBlockSize; i++)
        {
            /* If input symbol is erasure, reset metrics to zero */
            if ((*pcInSymb)[i].rChan == ERASURE_TAG_VALUE)
            {
                vecMetric[i].rTow0 = (_REAL) 0.0;
                vecMetric[i].rTow1 = (_REAL) 0.0;
            }
        }
    }
#endif
}

void CMLCMetric::Init(int iNewInputBlockSize, ECodScheme eNewCodingScheme)
{
    iInputBlockSize = iNewInputBlockSize;
    eMapType = eNewCodingScheme;
}

/* Cleanup definitions afterwards */
#undef BI_00
#undef BI_01
#undef BI_10
#undef BI_11
#undef BI_000
#undef BI_001
#undef BI_010
#undef BI_011
#undef BI_100
#undef BI_101
#undef BI_110
#undef BI_111

/******************************************************************************\
 * Technische Universitaet Darmstadt, Institut fuer Nachrichtentechnik
 * Copyright (c) 2001
 *
 * Author(s):
 *	Volker Fischer
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

#if !defined(VITERBI_DECODER_H__3B0BA660_CA63_4344_BB2B_23E7A0D31912__INCLUDED_)
#define VITERBI_DECODER_H__3B0BA660_CA63_4344_BB2B_23E7A0D31912__INCLUDED_

#include "../GlobalDefinitions.h"
#include "../tables/TableMLC.h"
#include "ConvEncoder.h"
#include "ChannelCode.h"


/* Definitions ****************************************************************/
/* SIMD implementation is always fixed-point (is disabled if MAP decoder is
   activated!) */
#define USE_SIMD
#undef USE_SIMD

/* Use MMX or SSE2 */
#define USE_MMX
#undef USE_MMX


/* No MAP implementation for SIMD! */
#ifdef USE_MAX_LOG_MAP
# undef USE_SIMD
#endif

#ifdef USE_SIMD
# ifndef USE_MMX
#  define USE_SSE2
# endif
#endif

/* Data type for Viterbi metric */
#ifdef USE_SIMD
# define _VITMETRTYPE				unsigned char
# define _DECISIONTYPE				unsigned char
#else
# define _VITMETRTYPE				float
# define _DECISIONTYPE				_BINARY
#endif

/* We initialize each new block of data all branches-metrics with the following
   value exept of the zero-state. This can be done since we actually KNOW that
   the zero state MUST be the transmitted one. The initialization vaule should
   be fairly high. But we have to be careful choosing this parameter. We
   should not take the largest value possible of the data type of the metric
   variable since in the Viterbi-routine we add something to this value and
   in that case we would force an overrun! */
#ifdef USE_SIMD
# define MC_METRIC_INIT_VALUE		((_VITMETRTYPE) 60)
#else
# define MC_METRIC_INIT_VALUE		((_VITMETRTYPE) 1e10)
#endif


/* In case of MAP decoder, all metrics must be stored for the entire input
   std::vector since we need them for the forward and backward direction */
#ifdef USE_MAX_LOG_MAP
# define METRICSET(a)		matrMetricSet[a]
#else
# define METRICSET(a)		vecrMetricSet
#endif


/* Classes ********************************************************************/
class CViterbiDecoder : public CChannelCode
{
public:
    CViterbiDecoder();
    virtual ~CViterbiDecoder() {}

    _REAL	Decode(CVector<CDistance>& vecNewDistance,
                 CVector<_DECISION>& vecOutputBits);
    void	Init(ECodScheme eNewCodingScheme,
              EChanType eNewChannelType, int iN1, int iN2,
              int iNewNumOutBitsPartA, int iNewNumOutBitsPartB,
              int iPunctPatPartA, int iPunctPatPartB, int iLevel);

protected:
    /* Two trellis data std::vectors are needed for current and old state */
    _VITMETRTYPE			vecTrelMetric1[MC_NUM_STATES];
    _VITMETRTYPE			vecTrelMetric2[MC_NUM_STATES];

#ifdef USE_MAX_LOG_MAP
    CMatrix<_REAL>			matrAlpha;
    CMatrix<_REAL>			matrMetricSet;
#else
    _REAL					vecrMetricSet[MC_NUM_OUTPUT_COMBINATIONS];
#endif

    CVector<int>			veciTablePuncPat;

    int						iNumOutBits;
    int						iNumOutBitsWithMemory;

    CMatrix<_DECISIONTYPE>	matdecDecisions;

#ifdef USE_SIMD
    /* Fields for storing the reodered metrics for MMX trellis */
    _VITMETRTYPE			chMet1[MC_NUM_STATES / 2];
    _VITMETRTYPE			chMet2[MC_NUM_STATES / 2];

#ifdef USE_MMX
    void TrellisUpdateMMX(
#endif
#ifdef USE_SSE2
        void TrellisUpdateSSE2(
#endif
            const _DECISIONTYPE* pCurDec,
            const _VITMETRTYPE* pCurTrelMetric, const _VITMETRTYPE* pOldTrelMetric,
            const _VITMETRTYPE* pchMet1, const _VITMETRTYPE* pchMet2);
#endif
    };


#endif // !defined(VITERBI_DECODER_H__3B0BA660_CA63_4344_BB2B_23E7A0D31912__INCLUDED_)

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

#if !defined(MLC_METRIC_H__3B0BA660_CA63_4344_BB2B_23E7A0D31912__INCLUDED_)
#define MLC_METRIC_H__3B0BA660_CA63_4344_BB2B_23E7A0D31912__INCLUDED_

#include "../GlobalDefinitions.h"
#include "../tables/TableQAMMapping.h"
#include "../util/Vector.h"
#include "../Parameter.h"


/* Classes ********************************************************************/
inline _REAL Metric(const _REAL rDist, const _REAL rChan)
{
    /* The calculation of "rChan" was done in the channel estimation module */
#ifdef USE_MAX_LOG_MAP
    /* | r / h - s | ^ 2 * | h | ^ 2 */
    return rDist * rDist * rChan;
#else
    /* | r / h - s | * | h | */
    return rDist * rChan;
#endif
}

class CMLCMetric
{
public:
    CMLCMetric() {}
    virtual ~CMLCMetric() {}

    /* Return the number of used symbols for calculating one branch-metric */
    void CalculateMetric(CVector<CEquSig>* pcInSymb,
                         CVector<CDistance>& vecMetric,
                         CVector<_DECISION>& vecSubsetDef1,
                         CVector<_DECISION>& vecSubsetDef2,
                         CVector<_DECISION>& vecSubsetDef3,
                         CVector<_DECISION>& vecSubsetDef4,
                         CVector<_DECISION>& vecSubsetDef5,
                         CVector<_DECISION>& vecSubsetDef6,
                         int iLevel, bool bIteration);
    void Init(int iNewInputBlockSize, ECodScheme eNewCodingScheme);

protected:
#ifdef USE_MAX_LOG_MAP
    inline _REAL Minimum2(const _REAL rA, const _REAL rX0, const _REAL rX1,
                          const _REAL rChan, const _REAL rLVal0) const
    {
        /* X0: L0 < 0
           X1: L0 > 0 */

        /* First, calculate all distances */
        _REAL rResult1 = Metric(fabs(rA - rX0), rChan);
        _REAL rResult2 = Metric(fabs(rA - rX1), rChan);

        /* Add L-value to metrics which to not correspond to correct hard decision */
        if (rLVal0 > 0.0)
            rResult1 += rLVal0;
        else
            rResult2 -= rLVal0;

        /* Return smallest one */
        if (rResult1 < rResult2)
            return rResult1;
        else
            return rResult2;
    }

    inline _REAL Minimum4(const _REAL rA, const _REAL rX00, const _REAL rX01,
                          const _REAL rX10, _REAL rX11, const _REAL rChan,
                          const _REAL rLVal0) const
    {
        /* X00: L0 < 0
           X01: L0 > 0
           X10: L0 < 0
           X11: L0 > 0 */

        /* First, calculate all distances */
        _REAL rResult1 = Metric(fabs(rA - rX00), rChan);
        _REAL rResult2 = Metric(fabs(rA - rX01), rChan);
        _REAL rResult3 = Metric(fabs(rA - rX10), rChan);
        _REAL rResult4 = Metric(fabs(rA - rX11), rChan);

        /* Add L-value to metrics which to not correspond to correct hard decision */
        if (rLVal0 > 0.0)
        {
            rResult1 += rLVal0;
            rResult3 += rLVal0;
        }
        else
        {
            rResult2 -= rLVal0;
            rResult4 -= rLVal0;
        }

        /* Search for smallest one */
        _REAL rReturn = rResult1;
        if (rResult2 < rReturn)
            rReturn = rResult2;
        if (rResult3 < rReturn)
            rReturn = rResult3;
        if (rResult4 < rReturn)
            rReturn = rResult4;

        return rReturn;
    }

    inline _REAL Minimum4(const _REAL rA, const _REAL rX00, const _REAL rX01,
                          const _REAL rX10, _REAL rX11, const _REAL rChan,
                          const _REAL rLVal0, const _REAL rLVal1) const
    {
        /* X00: L0 < 0, L1 < 0
           X01: L0 > 0, L1 < 0
           X10: L0 < 0, L1 > 0
           X11: L0 > 0, L1 > 0 */

        /* First, calculate all distances */
        _REAL rResult1 = Metric(fabs(rA - rX00), rChan);
        _REAL rResult2 = Metric(fabs(rA - rX01), rChan);
        _REAL rResult3 = Metric(fabs(rA - rX10), rChan);
        _REAL rResult4 = Metric(fabs(rA - rX11), rChan);

        /* Add L-value to metrics which to not correspond to correct hard decision */
        if (rLVal0 > 0.0)
        {
            rResult1 += rLVal0;
            rResult3 += rLVal0;
        }
        else
        {
            rResult2 -= rLVal0;
            rResult4 -= rLVal0;
        }

        if (rLVal1 > 0.0)
        {
            rResult1 += rLVal1;
            rResult2 += rLVal1;
        }
        else
        {
            rResult3 -= rLVal1;
            rResult4 -= rLVal1;
        }

        /* Search for smallest one */
        _REAL rReturn = rResult1;
        if (rResult2 < rReturn)
            rReturn = rResult2;
        if (rResult3 < rReturn)
            rReturn = rResult3;
        if (rResult4 < rReturn)
            rReturn = rResult4;

        return rReturn;
    }
#endif

    inline _REAL Minimum1(const _REAL rA, const _REAL rB,
                          const _REAL rChan) const
    {
        /* The minium in case of only one parameter is trivial */
        return Metric(fabs(rA - rB), rChan);
    }

    inline _REAL Minimum2(const _REAL rA, const _REAL rB1, const _REAL rB2,
                          const _REAL rChan) const
    {
        /* First, calculate all distances */
        const _REAL rResult1 = fabs(rA - rB1);
        const _REAL rResult2 = fabs(rA - rB2);

        /* Return smallest one */
        if (rResult1 < rResult2)
            return Metric(rResult1, rChan);
        else
            return Metric(rResult2, rChan);
    }

    inline _REAL Minimum4(const _REAL rA, const _REAL rB1, const _REAL rB2,
                          const _REAL rB3, _REAL rB4, const _REAL rChan) const
    {
        /* First, calculate all distances */
        const _REAL rResult1 = fabs(rA - rB1);
        const _REAL rResult2 = fabs(rA - rB2);
        const _REAL rResult3 = fabs(rA - rB3);
        const _REAL rResult4 = fabs(rA - rB4);

        /* Search for smallest one */
        _REAL rReturn = rResult1;
        if (rResult2 < rReturn)
            rReturn = rResult2;
        if (rResult3 < rReturn)
            rReturn = rResult3;
        if (rResult4 < rReturn)
            rReturn = rResult4;

        return Metric(rReturn, rChan);
    }


    int						iInputBlockSize;
    ECodScheme	eMapType;
};


#endif // !defined(MLC_METRIC_H__3B0BA660_CA63_4344_BB2B_23E7A0D31912__INCLUDED_)

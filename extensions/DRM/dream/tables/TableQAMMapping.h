/******************************************************************************\
 * Technische Universitaet Darmstadt, Institut fuer Nachrichtentechnik
 * Copyright (c) 2001
 *
 * Author(s):
 *	Volker Fischer
 *
 * Description:
 *	Tables for QAM mapping (Mapping is already normalized)
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

#if !defined(QAM_MAPPING_H__3B0_CA63_4344_BB2B_23E7912__INCLUDED_)
#define QAM_MAPPING_H__3B0_CA63_4344_BB2B_23E7912__INCLUDED_

#include "../matlib/MatlibStdToolbox.h"
#include "../GlobalDefinitions.h"


/* Definitions ****************************************************************/
/* Input bits are collected in bytes separately for imaginary and real part.
   The order is: [i_0, i_1, i_2] and [q_0, q_1, q_2] -> {i, q}
   All entries are normalized according to the DRM-standard */
const _REAL rTableQAM64SM[8][2] = {
    { 1.0801234497f,  1.0801234497f},
    {-0.1543033499f, -0.1543033499f},
    { 0.4629100498f,  0.4629100498f},
    {-0.7715167498f, -0.7715167498f},
    { 0.7715167498f,  0.7715167498f},
    {-0.4629100498f, -0.4629100498f},
    { 0.1543033499f,  0.1543033499f},
    {-1.0801234497f, -1.0801234497f}
};

const _REAL rTableQAM64HMsym[8][2] = {
    { 1.0801234497f,  1.0801234497f},
    { 0.4629100498f,  0.4629100498f},
    { 0.7715167498f,  0.7715167498f},
    { 0.1543033499f,  0.1543033499f},
    {-0.1543033499f, -0.1543033499f},
    {-0.7715167498f, -0.7715167498f},
    {-0.4629100498f, -0.4629100498f},
    {-1.0801234497f, -1.0801234497f}
};

const _REAL rTableQAM64HMmix[8][2] = {
    { 1.0801234497f,  1.0801234497f},
    { 0.4629100498f, -0.1543033499f},
    { 0.7715167498f,  0.4629100498f},
    { 0.1543033499f, -0.7715167498f},
    {-0.1543033499f,  0.7715167498f},
    {-0.7715167498f, -0.4629100498f},
    {-0.4629100498f,  0.1543033499f},
    {-1.0801234497f, -1.0801234497f}
};

const _REAL rTableQAM16[4][2] = {
    { 0.9486832980f,  0.9486832980f},
    {-0.3162277660f, -0.3162277660f},
    { 0.3162277660f,  0.3162277660f},
    {-0.9486832980f, -0.9486832980f}
};

const _REAL rTableQAM4[2][2] = {
    { 0.7071067811f,  0.7071067811f},
    {-0.7071067811f, -0.7071067811f}
};


/* Global functions ***********************************************************/
/*
	----------------------------------------------------------------------------
	Implementation of distance to nearest constellation point (symbol) for all
	QAM types
*/
inline CComplex MinDist4QAM(const CComplex cI)
{
    /* Return std::vector pointing to nearest signal point of this constellation.
       2 possible constellation points for real and imaginary axis */
    return CComplex(
               /* Real axis minimum distance */
               Min(Abs(rTableQAM4[0][0] - Real(cI)), Abs(rTableQAM4[1][0] - Real(cI))),
               /* Imaginary axis minimum distance */
               Min(Abs(rTableQAM4[0][1] - Imag(cI)), Abs(rTableQAM4[1][1] - Imag(cI))));
}

inline CComplex MinDist16QAM(const CComplex cI)
{
    /* Return std::vector pointing to nearest signal point of this constellation.
       4 possible constellation points for real and imaginary axis */
    return CComplex(
               /* Real axis minimum distance */
               Min(Abs(rTableQAM16[0][0] - Real(cI)), Abs(rTableQAM16[1][0] - Real(cI)),
                   Abs(rTableQAM16[2][0] - Real(cI)), Abs(rTableQAM16[3][0] - Real(cI))),
               /* Imaginary axis minimum distance */
               Min(Abs(rTableQAM16[0][1] - Imag(cI)), Abs(rTableQAM16[1][1] - Imag(cI)),
                   Abs(rTableQAM16[2][1] - Imag(cI)), Abs(rTableQAM16[3][1] - Imag(cI))));
}

inline CComplex MinDist64QAM(const CComplex cI)
{
    /* Return std::vector pointing to nearest signal point of this constellation.
       8 possible constellation points for real and imaginary axis */
    return CComplex(
               /* Real axis minimum distance */
               Min(Abs(rTableQAM64SM[0][0] - Real(cI)), Abs(rTableQAM64SM[1][0] - Real(cI)),
                   Abs(rTableQAM64SM[2][0] - Real(cI)), Abs(rTableQAM64SM[3][0] - Real(cI)),
                   Abs(rTableQAM64SM[4][0] - Real(cI)), Abs(rTableQAM64SM[5][0] - Real(cI)),
                   Abs(rTableQAM64SM[6][0] - Real(cI)), Abs(rTableQAM64SM[7][0] - Real(cI))),
               /* Imaginary axis minimum distance */
               Min(Abs(rTableQAM64SM[0][1] - Imag(cI)), Abs(rTableQAM64SM[1][1] - Imag(cI)),
                   Abs(rTableQAM64SM[2][1] - Imag(cI)), Abs(rTableQAM64SM[3][1] - Imag(cI)),
                   Abs(rTableQAM64SM[4][1] - Imag(cI)), Abs(rTableQAM64SM[5][1] - Imag(cI)),
                   Abs(rTableQAM64SM[6][1] - Imag(cI)), Abs(rTableQAM64SM[7][1] - Imag(cI))));
}


/*
	----------------------------------------------------------------------------
	Implementation of hard decision for all QAM types
*/
inline CComplex Dec4QAM(const CComplex cI)
{
    CReal rDecRe, rDecIm;

    /* Real */
    if (Abs(rTableQAM4[0][0] - Real(cI)) < Abs(rTableQAM4[1][0] - Real(cI)))
        rDecRe = rTableQAM4[0][0];
    else
        rDecRe = rTableQAM4[1][0];

    /* Imaginary */
    if (Abs(rTableQAM4[0][1] - Imag(cI)) < Abs(rTableQAM4[1][1] - Imag(cI)))
        rDecIm = rTableQAM4[0][1];
    else
        rDecIm = rTableQAM4[1][1];

    return CComplex(rDecRe, rDecIm);
}

inline CComplex Dec16QAM(const CComplex cI)
{
    CReal rCurDist;

    /* Real */
    CReal rMinDist = Abs(rTableQAM16[0][0] - Real(cI));
    CReal rDecRe = rTableQAM16[0][0];

    rCurDist = Abs(rTableQAM16[1][0] - Real(cI));
    if (rCurDist < rMinDist)
    {
        rMinDist = rCurDist;
        rDecRe = rTableQAM16[1][0];
    }

    rCurDist = Abs(rTableQAM16[2][0] - Real(cI));
    if (rCurDist < rMinDist)
    {
        rMinDist = rCurDist;
        rDecRe = rTableQAM16[2][0];
    }

    rCurDist = Abs(rTableQAM16[3][0] - Real(cI));
    if (rCurDist < rMinDist)
        rDecRe = rTableQAM16[3][0];

    /* Imaginary */
    rMinDist = Abs(rTableQAM16[0][1] - Imag(cI));
    CReal rDecIm = rTableQAM16[0][1];

    rCurDist = Abs(rTableQAM16[1][1] - Imag(cI));
    if (rCurDist < rMinDist)
    {
        rMinDist = rCurDist;
        rDecIm = rTableQAM16[1][1];
    }

    rCurDist = Abs(rTableQAM16[2][1] - Imag(cI));
    if (rCurDist < rMinDist)
    {
        rMinDist = rCurDist;
        rDecIm = rTableQAM16[2][1];
    }

    rCurDist = Abs(rTableQAM16[3][1] - Imag(cI));
    if (rCurDist < rMinDist)
        rDecIm = rTableQAM16[3][1];

    return CComplex(rDecRe, rDecIm);
}

inline CComplex Dec64QAM(const CComplex cI)
{
    CReal rCurDist;

    /* Real */
    CReal rMinDist = Abs(rTableQAM64SM[0][0] - Real(cI));
    CReal rDecRe = rTableQAM64SM[0][0];

    rCurDist = Abs(rTableQAM64SM[1][0] - Real(cI));
    if (rCurDist < rMinDist)
    {
        rMinDist = rCurDist;
        rDecRe = rTableQAM64SM[1][0];
    }

    rCurDist = Abs(rTableQAM64SM[2][0] - Real(cI));
    if (rCurDist < rMinDist)
    {
        rMinDist = rCurDist;
        rDecRe = rTableQAM64SM[2][0];
    }

    rCurDist = Abs(rTableQAM64SM[3][0] - Real(cI));
    if (rCurDist < rMinDist)
    {
        rMinDist = rCurDist;
        rDecRe = rTableQAM64SM[3][0];
    }

    rCurDist = Abs(rTableQAM64SM[4][0] - Real(cI));
    if (rCurDist < rMinDist)
    {
        rMinDist = rCurDist;
        rDecRe = rTableQAM64SM[4][0];
    }

    rCurDist = Abs(rTableQAM64SM[5][0] - Real(cI));
    if (rCurDist < rMinDist)
    {
        rMinDist = rCurDist;
        rDecRe = rTableQAM64SM[5][0];
    }

    rCurDist = Abs(rTableQAM64SM[6][0] - Real(cI));
    if (rCurDist < rMinDist)
    {
        rMinDist = rCurDist;
        rDecRe = rTableQAM64SM[6][0];
    }

    rCurDist = Abs(rTableQAM64SM[7][0] - Real(cI));
    if (rCurDist < rMinDist)
        rDecRe = rTableQAM64SM[7][0];

    /* Imaginary */
    rMinDist = Abs(rTableQAM64SM[0][1] - Imag(cI));
    CReal rDecIm = rTableQAM64SM[0][1];

    rCurDist = Abs(rTableQAM64SM[1][1] - Imag(cI));
    if (rCurDist < rMinDist)
    {
        rMinDist = rCurDist;
        rDecIm = rTableQAM64SM[1][1];
    }

    rCurDist = Abs(rTableQAM64SM[2][1] - Imag(cI));
    if (rCurDist < rMinDist)
    {
        rMinDist = rCurDist;
        rDecIm = rTableQAM64SM[2][1];
    }

    rCurDist = Abs(rTableQAM64SM[3][1] - Imag(cI));
    if (rCurDist < rMinDist)
    {
        rMinDist = rCurDist;
        rDecIm = rTableQAM64SM[3][1];
    }

    rCurDist = Abs(rTableQAM64SM[4][1] - Imag(cI));
    if (rCurDist < rMinDist)
    {
        rMinDist = rCurDist;
        rDecIm = rTableQAM64SM[4][1];
    }

    rCurDist = Abs(rTableQAM64SM[5][1] - Imag(cI));
    if (rCurDist < rMinDist)
    {
        rMinDist = rCurDist;
        rDecIm = rTableQAM64SM[5][1];
    }

    rCurDist = Abs(rTableQAM64SM[6][1] - Imag(cI));
    if (rCurDist < rMinDist)
    {
        rMinDist = rCurDist;
        rDecIm = rTableQAM64SM[6][1];
    }

    rCurDist = Abs(rTableQAM64SM[7][1] - Imag(cI));
    if (rCurDist < rMinDist)
        rDecIm = rTableQAM64SM[7][1];

    return CComplex(rDecRe, rDecIm);
}


#endif // !defined(QAM_MAPPING_H__3B0_CA63_4344_BB2B_23E7912__INCLUDED_)

/******************************************************************************\
 * Copyright (c) 2001
 *
 * Author(s):
 *	Volker Fischer
 *
 * Description:
 *	c++ Mathematic Library (Matlib), signal processing toolbox
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

#ifndef _MATLIB_SIGNAL_PROC_TOOLBOX_H_
#define _MATLIB_SIGNAL_PROC_TOOLBOX_H_

#include "Matlib.h"
#include "MatlibStdToolbox.h"
#include <cstdlib>


/* Helpfunctions **************************************************************/
/* Randomize functions */
CMatlibVector<CReal>	Randn(const int iLen);
inline
CMatlibVector<CReal>	Rand(const int iLen)
							{_VECOP(CReal, iLen, (CReal) ((double) rand() / RAND_MAX));}


/* Window functions */
CMatlibVector<CReal>	Hann(const int iLen);
CMatlibVector<CReal>	Hamming(const int iLen);
CMatlibVector<CReal>	Nuttallwin(const int iLen);
CMatlibVector<CReal>	Bartlett(const int iLen);
CMatlibVector<CReal>	Triang(const int iLen);
CMatlibVector<CReal>	Kaiser(const int iLen, const CReal rBeta);


/* Bessel function */
CReal					Besseli(const CReal rNu, const CReal rZ);


/* Filter data with a recursive (IIR) or nonrecursive (FIR) filter */
CMatlibVector<CReal>	Filter(const CMatlibVector<CReal>& fvB, 
							   const CMatlibVector<CReal>& fvA, 
							   const CMatlibVector<CReal>& fvX, 
							   CMatlibVector<CReal>& fvZ);


/* Levinson durbin recursion */
CMatlibVector<CReal>	Levinson(const CMatlibVector<CReal>& vecrRx, 
								 const CMatlibVector<CReal>& vecrB);
CMatlibVector<CComplex>	Levinson(const CMatlibVector<CComplex>& veccRx, 
								 const CMatlibVector<CComplex>& veccB);


/* Sinc-function */
inline CReal			Sinc(const CReal& rI)
							{return rI == (CReal) 0.0 ? (CReal) 1.0 : sin(crPi * rI) / (crPi * rI);}
inline
CMatlibVector<CReal>	Sinc(const CMatlibVector<CReal>& fvI)
							{_VECOP(CReal, fvI.GetSize(), Sinc(fvI[i]));}


/* My own functions --------------------------------------------------------- */
/* Dominant eigenstd::vector */
CMatlibVector<CReal>	DomEig(const CMatlibMatrix<CReal>& rmI, const CReal rEpsilon = 1e-5);

/* Linear regression */
CReal					LinRegr(const CMatlibVector<CReal>& rvX, const CMatlibVector<CReal>& rvY);

/* Lowpass filter design using windowing method */
CMatlibVector<CReal>	FirLP(const CReal rNormBW,
							  const CMatlibVector<CReal>& rvWin);

/* Complex FIR filter with decimation */
CMatlibVector<CComplex>	FirFiltDec(const CMatlibVector<CComplex>& cvB,
								   const CMatlibVector<CComplex>& cvX,
								   CMatlibVector<CComplex>& cvZ,
								   const int iDecFact);

/* Squared magnitude */
inline CReal			SqMag(const CComplex& cI)
							{return cI.real() * cI.real() + cI.imag() * cI.imag();}
inline CReal			SqMag(const CReal& rI)
							{return rI * rI;}
inline
CMatlibVector<CReal>	SqMag(const CMatlibVector<CComplex>& veccI)
							{_VECOP(CReal, veccI.GetSize(), SqMag(veccI[i]));}
inline
CMatlibVector<CReal>	SqMag(const CMatlibVector<CReal>& vecrI)
							{_VECOP(CReal, vecrI.GetSize(), SqMag(vecrI[i]));}

/* One pole recursion (first order IIR)
   y_n = lambda * y_{n - 1} + (1 - lambda) * x_n */
inline void				IIR1(CReal& rY, const CReal& rX, const CReal rLambda)
							{rY = rLambda * (rY - rX) + rX;}

inline void				IIR1(CComplex& cY, const CComplex& cX, const CReal rLambda)
							{cY = rLambda * (cY - cX) + cX;}

inline void				IIR1(CMatlibVector<CReal>& rY,
							 const CMatlibVector<CReal>& rX,
							 const CReal rLambda)
{
	const int iSize = rY.GetSize();

	for (int i = 0; i < iSize; i++)
		IIR1(rY[i], rX[i], rLambda);
}

/* Two-sided one pole recursion */
inline void				IIR1TwoSided(CReal& rY, const CReal& rX,
									 const CReal rLamUp, const CReal rLamDown)
							{rX > rY ? IIR1(rY, rX, rLamUp) : IIR1(rY, rX, rLamDown);}

/* Get lambda for one-pole recursion from time constant */
inline CReal			IIR1Lam(const CReal& rTau, const CReal& rFs)
							{return exp((CReal) -1.0 / (rTau * rFs));}


#endif	/* _MATLIB_SIGNAL_PROC_TOOLBOX_H_ */

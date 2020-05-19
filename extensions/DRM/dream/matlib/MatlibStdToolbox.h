/******************************************************************************\
 * Copyright (c) 2001
 *
 * Author(s):
 *	Volker Fischer
 *
 * Description:
 *	c++ Mathematic Library (Matlib), standard toolbox
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

#ifndef _MATLIB_STD_TOOLBOX_H_
#define _MATLIB_STD_TOOLBOX_H_

#include "Matlib.h"

/* fftw (Homepage: http://www.fftw.org) */
#include <fftw3.h>

/* Classes ********************************************************************/
class CFftPlans
{
public:
	CFftPlans(const int iFftSize = 0);
	~CFftPlans();

	enum EFFTPlan { FP_RFFTPlForw, FP_RFFTPlBackw, FP_FFTPlForw, FP_FFTPlBackw };

	void Init(const int iFSi);
	void Init(const int iFSi, EFFTPlan eFFTPlan);

	fftwf_plan	RFFTPlForw;
	fftwf_plan	RFFTPlBackw;
	float*		pFftwRealIn;
	float*		pFftwRealOut;
	fftwf_plan	FFTPlForw;
	fftwf_plan	FFTPlBackw;

	fftwf_complex*	pFftwComplexIn;
	fftwf_complex*	pFftwComplexOut;

protected:
	void			Clean();
	bool		InitInternal(const int iFSi);
	bool		bInitialized;
	bool		bFixedSizeInit;
	int				fftw_n;
};


/* Helpfunctions **************************************************************/
inline CReal				Min(const CReal& rA, const CReal& rB)
								{return rA < rB ? rA : rB;}
inline CMatlibVector<CReal>	Min(const CMatlibVector<CReal>& rvA, const CMatlibVector<CReal>& rvB)
								{_VECOP(CReal, rvA.GetSize(), Min(rvA[i], rvB[i]));}
CReal						Min(const CMatlibVector<CReal>& rvI);
void						Min(CReal& rMinVal /* out */, int& iMinInd /* out */,
								const CMatlibVector<CReal>& rvI /* in */);

inline CReal				Min(const CReal& r1, const CReal& r2, const CReal& r3, const CReal& r4)
								{return Min(Min(Min(r1, r2), r3), r4);}
inline CReal				Min(const CReal& r1, const CReal& r2, const CReal& r3, const CReal& r4,
								const CReal& r5, const CReal& r6, const CReal& r7, const CReal& r8)
								{return Min(Min(Min(Min(Min(Min(Min(r1, r2), r3), r4), r5), r6), r7), r8);}


inline CReal				Max(const CReal& rA, const CReal& rB)
								{return rA > rB ? rA : rB;}
inline CMatlibVector<CReal>	Max(const CMatlibVector<CReal>& rvA, const CMatlibVector<CReal>& rvB)
								{_VECOP(CReal, rvA.GetSize(), Max(rvA[i], rvB[i]));}
CReal						Max(const CMatlibVector<CReal>& rvI);
void						Max(CReal& rMaxVal /* out */, int& iMaxInd /* out */,
								const CMatlibVector<CReal>& rvI /* in */);

inline CReal				Max(const CReal& r1, const CReal& r2, const CReal& r3)
								{return Max(Max(r1, r2), r3);}
inline CReal				Max(const CReal& r1, const CReal& r2, const CReal& r3, const CReal& r4,
								const CReal& r5, const CReal& r6, const CReal& r7)
								{return Max(Max(Max(Max(Max(Max(r1, r2), r3), r4), r5), r6), r7);}


inline CMatlibVector<CReal>	Ones(const int iLen)
								{_VECOP(CReal, iLen, (CReal) 1.0);}
inline CMatlibVector<CReal>	Zeros(const int iLen)
								{_VECOP(CReal, iLen, (CReal) 0.0);}


inline CReal				Real(const CComplex& cI) {return cI.real();}
inline CMatlibVector<CReal>	Real(const CMatlibVector<CComplex>& cvI)
								{_VECOP(CReal, cvI.GetSize(), Real(cvI[i]));}

inline CReal				Imag(const CComplex& cI) {return cI.imag();}
inline CMatlibVector<CReal>	Imag(const CMatlibVector<CComplex>& cvI)
								{_VECOP(CReal, cvI.GetSize(), Imag(cvI[i]));}

inline CComplex					Conj(const CComplex& cI) {return conj(cI);}
inline CMatlibVector<CComplex>	Conj(const CMatlibVector<CComplex>& cvI)
									{_VECOP(CComplex, cvI.GetSize(), Conj(cvI[i]));}
inline CMatlibMatrix<CComplex>	Conj(const CMatlibMatrix<CComplex>& cmI)
									{_MATOP(CComplex, cmI.GetRowSize(), cmI.GetColSize(), Conj(cmI[i]));}


/* Absolute and angle (argument) functions */
inline CReal				Abs(const CReal& rI) {return fabs(rI);}
inline CMatlibVector<CReal>	Abs(const CMatlibVector<CReal>& fvI)
								{_VECOP(CReal, fvI.GetSize(), Abs(fvI[i]));}

inline CReal				Abs(const CComplex& cI) {return abs(cI);}
inline CMatlibVector<CReal>	Abs(const CMatlibVector<CComplex>& cvI)
								{_VECOP(CReal, cvI.GetSize(), Abs(cvI[i]));}

inline CReal				Angle(const CComplex& cI) {return arg(cI);}
inline CMatlibVector<CReal>	Angle(const CMatlibVector<CComplex>& cvI)
								{_VECOP(CReal, cvI.GetSize(), Angle(cvI[i]));}


/* Trigonometric functions */
inline CReal				Sin(const CReal& fI) {return sin(fI);}
template<class T> inline
CMatlibVector<T>			Sin(const CMatlibVector<T>& vecI) 
								{_VECOP(T, vecI.GetSize(), sin(vecI[i]));}

inline CReal				Cos(const CReal& fI) {return cos(fI);}
template<class T> inline
CMatlibVector<T>			Cos(const CMatlibVector<T>& vecI)
								{_VECOP(T, vecI.GetSize(), cos(vecI[i]));}

inline CReal				Tan(const CReal& fI) {return tan(fI);}
template<class T> inline
CMatlibVector<T>			Tan(const CMatlibVector<T>& vecI)
								{_VECOP(T, vecI.GetSize(), tan(vecI[i]));}

inline CReal				Sinh(const CReal& fI) {return sinh(fI);}
template<class T> inline
CMatlibVector<T>			Sinh(const CMatlibVector<T>& vecI) 
								{_VECOP(T, vecI.GetSize(), sinh(vecI[i]));}

inline CReal				Cosh(const CReal& fI) {return cosh(fI);}
template<class T> inline
CMatlibVector<T>			Cosh(const CMatlibVector<T>& vecI)
								{_VECOP(T, vecI.GetSize(), cosh(vecI[i]));}

inline CReal				Tanh(const CReal& fI) {return tanh(fI);}
template<class T> inline
CMatlibVector<T>			Tanh(const CMatlibVector<T>& vecI)
								{_VECOP(T, vecI.GetSize(), tanh(vecI[i]));}


/* Square root */
inline CReal				Sqrt(const CReal& fI) {return sqrt(fI);}
template<class T> inline
CMatlibVector<T>			Sqrt(const CMatlibVector<T>& vecI)
								{_VECOP(T, vecI.GetSize(), sqrt(vecI[i]));}


/* Exponential function */
inline CReal				Exp(const CReal& fI) {return exp(fI);}
template<class T> inline
CMatlibVector<T>			Exp(const CMatlibVector<T>& vecI)
								{_VECOP(T, vecI.GetSize(), exp(vecI[i]));}


/* Logarithm */
inline CReal				Log(const CReal& fI) {return log(fI);}
template<class T> inline
CMatlibVector<T>			Log(const CMatlibVector<T>& vecI)
								{_VECOP(T, vecI.GetSize(), log(vecI[i]));}

inline CReal				Log10(const CReal& fI) {return log10(fI);}
template<class T> inline
CMatlibVector<T>			Log10(const CMatlibVector<T>& vecI)
								{_VECOP(T, vecI.GetSize(), log10(vecI[i]));}


/* Mean, variance and standard deviation */
template<class T> inline T	Mean(const CMatlibVector<T>& vecI)
								{return Sum(vecI) / vecI.GetSize();}
template<class T> inline T	Std(CMatlibVector<T>& vecI) 
								{return Sqrt(Var(vecI));}
template<class T> T			Var(const CMatlibVector<T>& vecI);


/* Rounding functions */
inline CReal				Fix(const CReal& fI) {return (int) fI;}
inline CMatlibVector<CReal>	Fix(const CMatlibVector<CReal>& fvI)
								{_VECOP(CReal, fvI.GetSize(), Fix(fvI[i]));}

inline CReal				Floor(const CReal& fI) {return floor(fI);}
inline CMatlibVector<CReal>	Floor(const CMatlibVector<CReal>& fvI)
								{_VECOP(CReal, fvI.GetSize(), Floor(fvI[i]));}

inline CReal				Ceil(const CReal& fI) {return ceil(fI);}
inline CMatlibVector<CReal>	Ceil(const CMatlibVector<CReal>& fvI)
								{_VECOP(CReal, fvI.GetSize(), Ceil(fvI[i]));}

inline CReal				Round(const CReal& fI)
								{return Floor(fI + (CReal) 0.5);}
inline CMatlibVector<CReal>	Round(const CMatlibVector<CReal>& fvI)
								{_VECOP(CReal, fvI.GetSize(), Round(fvI[i]));}

inline CReal				Sign(const CReal& rI)
								{return rI == 0 ? 0 : rI > 0 ? 1 : -1;}

inline int					Mod(const int ix, const int iy)
								{return ix < 0 ? (ix % iy + iy) % iy : ix % iy;}

template<class T> T			Sum(const CMatlibVector<T>& vecI);

CMatlibVector<CReal>		Sort(const CMatlibVector<CReal>& rvI);


/* Matrix inverse */
CMatlibMatrix<CComplex>		Inv(const CMatlibMatrix<CComplex>& matrI);

/* Identity matrix */
CMatlibMatrix<CReal>		Eye(const int iLen);

CMatlibMatrix<CComplex>		Diag(const CMatlibVector<CComplex>& cvI);

CReal						Trace(const CMatlibMatrix<CReal>& rmI);

CMatlibMatrix<CComplex>		Toeplitz(const CMatlibVector<CComplex>& cvI);

/* Matrix transpose */
CMatlibMatrix<CComplex>		Transp(const CMatlibMatrix<CComplex>& cmI);
inline
CMatlibMatrix<CComplex>		TranspH(const CMatlibMatrix<CComplex>& cmI)
								{return Conj(Transp(cmI));} /* With conjugate complex */

/* Fourier transformations (also included: real FFT) */
CMatlibVector<CComplex>		Fft(const CMatlibVector<CComplex>& cvI, CFftPlans& FftPlans);
CMatlibVector<CComplex>		Ifft(const CMatlibVector<CComplex>& cvI, CFftPlans& FftPlans);
CMatlibVector<CComplex>		rfft(const CMatlibVector<CReal>& fvI, CFftPlans& FftPlans);
CMatlibVector<CReal>		rifft(const CMatlibVector<CComplex>& cvI, CFftPlans& FftPlans);

CMatlibVector<CReal>		FftFilt(const CMatlibVector<CComplex>& rvH,
									const CMatlibVector<CReal>& rvI,
									CMatlibVector<CReal>& rvZ,
									CFftPlans& FftPlans);


/* Numerical integration */
typedef CComplex(MATLIB_CALLBACK_QAUD)(CReal rX); /* Callback function definition */
CComplex					Quad(MATLIB_CALLBACK_QAUD f, const CReal a,
								 const CReal b, const CReal errorBound = 1.e-6);


/* Implementation **************************************************************
   (the implementation of template classes must be in the header file!) */
template<class T> inline
T Sum(const CMatlibVector<T>& vecI)
{
	const int iSize = vecI.GetSize();
	T SumRet = 0;
	for (int i = 0; i < iSize; i++)
		SumRet += vecI[i];

	return SumRet;
}

template<class T> inline
T Var(const CMatlibVector<T>& vecI)
{
	const int iSize = vecI.GetSize();

	/* First calculate mean */
	T tMean = Mean(vecI);

	/* Now variance (sum formula) */
	T tRet = 0;
	for (int i = 0; i < iSize; i++)
		tRet += (vecI[i] - tMean) * (vecI[i] - tMean);

	return tRet / (iSize - 1); /* Normalizing */
}


#endif	/* _MATLIB_STD_TOOLBOX_H_ */

/******************************************************************************\
 * Copyright (c) 2001
 *
 * Author(s):
 *	Volker Fischer
 *
 * Description:
 *	c++ Mathematic Library (Matlib)
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

#include "MatlibStdToolbox.h"
#include "../GlobalDefinitions.h"

/* The mutex need to be application wide,
   only the execution routines are thread-safe */
static CMutex* _matlib_mutex = nullptr;
#define MATLIB_MUTEX_LOCK() _matlib_mutex->Lock()
#define MATLIB_MUTEX_UNLOCK() _matlib_mutex->Unlock()

# define PLANNER_FLAGS (FFTW_ESTIMATE | FFTW_DESTROY_INPUT)
/* Warning: for testing purpose only */
//# define PLANNER_FLAGS FFTW_EXHAUSTIVE


/* Implementation *************************************************************/
CReal Min(const CMatlibVector<CReal>& rvI)
{
	const int iSize = rvI.GetSize();
	CReal rMinRet = rvI[0];
	for (int i = 1; i < iSize; i++)
	{
		if (rvI[i] < rMinRet)
			rMinRet = rvI[i];
	}

	return rMinRet;
}

void Min(CReal& rMinVal, int& iMinInd, const CMatlibVector<CReal>& rvI)
{
	const int iSize = rvI.GetSize();
	rMinVal = rvI[0]; /* Init actual minimum value */
	iMinInd = 0; /* Init index of minimum */
	for (int i = 1; i < iSize; i++)
	{
		if (rvI[i] < rMinVal)
		{
			rMinVal = rvI[i];
			iMinInd = i;
		}
	}
}

CReal Max(const CMatlibVector<CReal>& rvI)
{
	CReal rMaxRet;
	int iMaxInd; /* Not used by this function */
	Max(rMaxRet, iMaxInd, rvI);

	return rMaxRet;
}

void Max(CReal& rMaxVal, int& iMaxInd, const CMatlibVector<CReal>& rvI)
{
	const int iSize = rvI.GetSize();
	rMaxVal = rvI[0]; /* Init actual maximum value */
	iMaxInd = 0; /* Init index of maximum */
	for (int i = 1; i < iSize; i++)
	{
		if (rvI[i] > rMaxVal)
		{
			rMaxVal = rvI[i];
			iMaxInd = i;
		}
	}
}

CMatlibVector<CReal> Sort(const CMatlibVector<CReal>& rvI)
{
	const int iSize = rvI.GetSize();
	const int iEnd = iSize - 1;
	CMatlibVector<CReal> fvRet(iSize, VTY_TEMP);

	/* Copy input vector in output vector */
	fvRet = rvI;

	/* Loop through the array one less than its total cell count */
	for (int i = 0; i < iEnd; i++)
	{
		/* Loop through every cell (value) in array */
		for (int j = 0; j < iEnd; j++)
		{
			/* Compare the values and switch if necessary */
			if (fvRet[j] > fvRet[j + 1])
			{
				const CReal rSwap = fvRet[j];
				fvRet[j] = fvRet[j + 1];
				fvRet[j + 1] = rSwap;
			}
		}
	}

	return fvRet;
}

CMatlibMatrix<CReal> Eye(const int iLen)
{
	CMatlibMatrix<CReal> matrRet(iLen, iLen, VTY_TEMP);

	/* Set all values except of the diagonal to zero, diagonal entries = 1 */
	for (int i = 0; i < iLen; i++)
	{
		for (int j = 0; j < iLen; j++)
		{
			if (i == j)
				matrRet[i][j] = (CReal) 1.0;
			else
				matrRet[i][j] = (CReal) 0.0;
		}
	}

	return matrRet;
}

CMatlibMatrix<CComplex> Diag(const CMatlibVector<CComplex>& cvI)
{
	const int iSize = cvI.GetSize();
	CMatlibMatrix<CComplex> matcRet(iSize, iSize, VTY_TEMP);

	/* Set the diagonal to the values of the input vector */
	for (int i = 0; i < iSize; i++)
	{
		for (int j = 0; j < iSize; j++)
		{
			if (i == j)
				matcRet[i][j] = cvI[i];
			else
				matcRet[i][j] = (CReal) 0.0;
		}
	}

	return matcRet;
}

CReal Trace(const CMatlibMatrix<CReal>& rmI)
{
	const int iSize = rmI.GetRowSize(); /* matrix must be square */
	CReal rReturn = (CReal) 0.0;

	for (int i = 0; i < iSize; i++)
		rReturn += rmI[i][i];

	return rReturn;
}

CMatlibMatrix<CComplex> Toeplitz(const CMatlibVector<CComplex>& cvI)
{
	const int				iSize = cvI.GetSize();
	CMatlibMatrix<CComplex>	matcRet(iSize, iSize, VTY_TEMP);

	/* Create Toeplitz matrix */
	for (int i = 0; i < iSize; i++)
	{
		for (int j = 0; j < iSize; j++)
		{
			if (i < j)
				matcRet[i][j] = cvI[j - i];
			else
				matcRet[i][j] = Conj(cvI[i - j]);
		}
	}

	return matcRet;
}

CMatlibMatrix<CComplex> Transp(const CMatlibMatrix<CComplex>& cmI)
{
	const int iRowSize = cmI.GetRowSize();
	const int iColSize = cmI.GetColSize();

	/* Swaped row and column size due to transpose operation */
	CMatlibMatrix<CComplex> matcRet(iColSize, iRowSize, VTY_TEMP);

	/* Transpose matrix */
	for (int i = 0; i < iRowSize; i++)
	{
		for (int j = 0; j < iColSize; j++)
			matcRet[j][i] = cmI[i][j];
	}

	return matcRet;
}

CMatlibMatrix<CComplex> Inv(const CMatlibMatrix<CComplex>& matrI)
{
/*
	Parts of the following code are taken from Ptolemy
	(http://ptolemy.eecs.berkeley.edu/)

	The input matrix must be square, this is NOT checked here!
*/
	_COMPLEX	temp;
	int			row, col, i;

	const int iSize = matrI.GetColSize();
	CMatlibMatrix<CComplex> matrRet(iSize, iSize, VTY_TEMP);

	/* Make a working copy of input matrix */
	CMatlibMatrix<CComplex> work(matrI);

	/* Set result to be the identity matrix */
	matrRet = Eye(iSize);

	for (i = 0; i < iSize; i++) 
	{
		/* Check that the element in (i,i) is not zero */
		if ((Real(work[i][i]) == 0) && (Imag(work[i][i]) == 0))
		{
			/* Swap with a row below this one that has a non-zero element
			   in the same column */
			for (row = i + 1; row < iSize; row++)
			{
				if ((Real(work[i][i]) != 0) || (Imag(work[i][i]) != 0))
					break;
			}

// TEST
if (row == iSize)
{
	fprintf(stderr, "couldn't invert matrix, possibly singular.\n");
	matrRet = Eye(iSize);
	return matrRet;
}

			/* Swap rows */
			for (col = 0; col < iSize; col++)
			{
				temp = work[i][col];
				work[i][col] = work[row][col];
				work[row][col] = temp;
				temp = matrRet[i][col];
				matrRet[i][col] = matrRet[row][col];
				matrRet[row][col] = temp;
			}
		}

		/* Divide every element in the row by element (i,i) */
		temp = work[i][i];
		for (col = 0; col < iSize; col++)
		{
			work[i][col] /= temp;
			matrRet[i][col] /= temp;
		}

		/* Zero out the rest of column i */
		for (row = 0; row < iSize; row++)
		{
			if (row != i)
			{
				temp = work[row][i];
				for (col = iSize - 1; col >= 0; col--)
				{
					work[row][col] -= (temp * work[i][col]);
					matrRet[row][col] -= (temp * matrRet[i][col]);
				}
			}
		}
	}

	return matrRet;
}

/* This function is not listed in the header file. It shall be used only for
   Matlib internal calculations */
CComplex _integral(MATLIB_CALLBACK_QAUD f, const CReal a, const CReal b,
				   const CReal errorBound, CReal& integralBound,
				   bool& integralError, const CReal ru)
{
/*
	The following code (inclusive the actual Quad() function) is based on a
	JavaScript Example written by Lucio Tavernini. The code is hosted at
	http://tavernini.com/integral.shtml.

	Description: Adaptive Simpson's Quadrature

	_integral(f, a, b, errorBound) attempts to integrate f from
	a to b while keeping the asymptotic error estimate below
	errorBound using an adaptive implementation of Simpson's rule.

	Notes: Instead of NaN we use _MAXREAL. Infinite bounds are not allowed! The
	lower bound must always be smaller than the higher bound!
*/

	CReal		left, right, h, h6, bound;
	CComplex	fa, fb, v1, v2, error, value;
	int			m1, jend, mstart, j;

	if (integralError)
		return _MAXREAL; /* NaN */

	/* Integrate over [a,b]. Initialize */
	const int max = 1024;
	CRealVector x(max);
	CComplexVector f1(max);
	CComplexVector f2(max);
	CComplexVector f3(max);
	CComplexVector v(max);

	int step = 1;
	int m = 1;
	bound = errorBound;
	value = 0;
	h = b - a;
	x[0] = a;
	f1[0] = f(a);
	f2[0] = f((CReal) 0.5 * (a + b));
	f3[0] = f(b);
	v[0] = h * (f1[0] + (CReal) 4.0 * f2[0] + f3[0]) / (CReal) 6.0;

	do
	{
		/* Are we going to go forward or backward? */
		if (step == -1)
		{
			/* Forward: j = m,...,max */
			step = 1;
			j = m + 1;
			jend = max;
			m = 0;
			mstart = 0;
		}
		else
		{
			/* Backward: j = m,...,1 */
			step = -1;
			j = m - 1;
			jend = -1;
			m = max - 1;
			mstart = max - 1;
		}
		
		h = (CReal) 0.5 * h;
		h6 = h / 6;
		bound = (CReal) 0.5 * bound;
		
		do
		{
			left = x[j];
			right = x[j] + (CReal) 0.5 * h;

			/* Complete loss of significance? */
			if (left >= right)
			{
				fprintf(stderr, "integral: Error 1\n");
				return value;
			}

			fa = f(x[j] + (CReal) 0.5 * h);
			fb = f(x[j] + (CReal) 1.5 * h);
			v1 = h6 * (f1[j] + (CReal) 4.0 * fa + f2[j]);
			v2 = h6 * (f2[j] + (CReal) 4.0 * fb + f3[j]);
			error = (v[j] - v1 - v2) / (CReal) 15.0;

			if ((Abs(error) <= bound) || (Abs(v1 + v2) < Abs(value) * ru))
				value = ((v1 + v2) + value) - error;
			else
			{
				if (integralError)
					return _MAXREAL; /* NaN */
				
				/* Are we out of memory? */
				if (m == j)
				{
					left = x[j];
					right = x[j] + (CReal) 0.5 * h;

					/* Complete loss of significance? */
					if (left >= right)
					{
						fprintf(stderr, "integral: Error 2\n");
						return value;
					}

					value += _integral(f, left, x[j] + 2 * h, bound,
						integralBound, integralError, ru);
				}
				else 
				{
					/* No, we are not */
					left = x[j];
					right = x[j] + (CReal) 0.125 * h;

					if (left >= right)
					{
						/* The error bound specified is too small! */
						integralError = true;
						return _MAXREAL; /* NaN */
					}

					m1 = m + step;
					x[m] = x[j];
					x[m1] = x[j] + h;
					v[m] = v1;
					v[m1] = v2;
					f1[m] = f1[j];
					f2[m] = fa;
					f3[m] = f2[j];
					f1[m1] = f2[j];
					f2[m1] = fb;
					f3[m1] = f3[j];
					m += 2 * step;
				}
			}
			j += step;
		}
		while (j != jend);
	}
	while (m != mstart);

	if (integralError)
		return _MAXREAL; /* NaN */
	else
		return value;
}

CComplex Quad(MATLIB_CALLBACK_QAUD f, const CReal a, const CReal b,
			  const CReal errorBound)
{
	/* Set globals */
	/* Generate rounding unit */
	CReal value;
	CReal ru = (CReal) 1.0;
	do
	{
		ru = (CReal) 0.5 * ru;
		value = (CReal) 1.0 + ru;
	}
	while (value != (CReal) 1.0);

	ru *= 2;

	CReal integralBound = errorBound;
	bool integralError = false;

	/* Compute */
	return _integral(f, a, b, errorBound, integralBound, integralError, ru);
}

CMatlibVector<CComplex> Fft(const CMatlibVector<CComplex>& cvI,
							CFftPlans& FftPlans)
{
	int				i;
	fftwf_complex*	pFftwComplexIn;
	fftwf_complex*	pFftwComplexOut;

	const int				n(cvI.GetSize());

	CMatlibVector<CComplex>	cvReturn(n, VTY_TEMP);

	/* If input vector has zero length, return */
	if (n == 0)
		return cvReturn;

	/* Init the plan */
	FftPlans.Init(n, CFftPlans::FP_FFTPlForw);

	pFftwComplexIn = FftPlans.pFftwComplexIn;
	pFftwComplexOut = FftPlans.pFftwComplexOut;

	/* fftw (Homepage: http://www.fftw.org/) */
	for (i = 0; i < n; i++)
	{
		pFftwComplexIn[i][0] = cvI[i].real();
		pFftwComplexIn[i][1] = cvI[i].imag();
	}

	/* Actual fftw call */
	fftwf_execute(FftPlans.FFTPlForw);

	for (i = 0; i < n; i++)
		cvReturn[i] = CComplex(pFftwComplexOut[i][0], pFftwComplexOut[i][1]);

	return cvReturn;
}

CMatlibVector<CComplex> Ifft(const CMatlibVector<CComplex>& cvI,
							 CFftPlans& FftPlans)
{
	int				i;
	fftwf_complex*	pFftwComplexIn;
	fftwf_complex*	pFftwComplexOut;

	const int		n(cvI.GetSize());

	CMatlibVector<CComplex>	cvReturn(n, VTY_TEMP);

	/* If input vector has zero length, return */
	if (n == 0)
		return cvReturn;

	/* Init the plan */
	FftPlans.Init(n, CFftPlans::FP_FFTPlBackw);

	pFftwComplexIn = FftPlans.pFftwComplexIn;
	pFftwComplexOut = FftPlans.pFftwComplexOut;

	/* fftw (Homepage: http://www.fftw.org/) */
	for (i = 0; i < n; i++)
	{
		pFftwComplexIn[i][0] = cvI[i].real();
		pFftwComplexIn[i][1] = cvI[i].imag();
	}

	/* Actual fftw call */
	fftwf_execute(FftPlans.FFTPlBackw);
	
	const CReal scale = (CReal) 1.0 / n;
	for (i = 0; i < n; i++)
	{
		cvReturn[i] = CComplex(pFftwComplexOut[i][0] * scale,
			pFftwComplexOut[i][1] * scale);
	}

	return cvReturn;
}

CMatlibVector<CComplex> rfft(const CMatlibVector<CReal>& fvI,
							 CFftPlans& FftPlans)
{
	int			i;
	float* 	pFftwRealIn;
	float* 	pFftwRealOut;

	const int	iSizeI = fvI.GetSize();
	const int	iLongLength(iSizeI);
	const int	iShortLength(iLongLength / 2);
	const int	iUpRoundShortLength((iLongLength + 1) / 2);
	
	CMatlibVector<CComplex>	cvReturn(iShortLength
		/* Include Nyquist frequency in case of even N */ + 1, VTY_TEMP);

	/* If input vector has zero length, return */
	if (iLongLength == 0)
		return cvReturn;

	/* Init the plan */
	FftPlans.Init(iLongLength, CFftPlans::FP_RFFTPlForw);

	pFftwRealIn = FftPlans.pFftwRealIn;
	pFftwRealOut = FftPlans.pFftwRealOut;

	/* fftw (Homepage: http://www.fftw.org/) */
	for (i = 0; i < iSizeI; i++)
		pFftwRealIn[i] = fvI[i];

	/* Actual fftw call */
	fftwf_execute(FftPlans.RFFTPlForw);

	/* Now build complex output vector */
	/* Zero frequency */
	cvReturn[0] = pFftwRealOut[0];
	for (i = 1; i < iUpRoundShortLength; i++)
		cvReturn[i] = CComplex(pFftwRealOut[i], pFftwRealOut[iLongLength - i]);

	/* If N is even, include Nyquist frequency */
	if ((iLongLength & 1) == 0)
		cvReturn[iShortLength] = pFftwRealOut[iShortLength];

	return cvReturn;
}

CMatlibVector<CReal> rifft(const CMatlibVector<CComplex>& cvI,
						   CFftPlans& FftPlans)
{
/*
	This function only works with EVEN N!
*/
	int			i;
	float*		pFftwRealIn;
	float*		pFftwRealOut;

	const int	iShortLength(cvI.GetSize() - 1); /* Nyquist frequency! */
	const int	iLongLength(iShortLength * 2);

	CMatlibVector<CReal> fvReturn(iLongLength, VTY_TEMP);

	/* If input vector is too short, return */
	if (iShortLength <= 0)
		return fvReturn;

	/* Init the plan */
	FftPlans.Init(iLongLength, CFftPlans::FP_RFFTPlBackw);

	pFftwRealIn = FftPlans.pFftwRealIn;
	pFftwRealOut = FftPlans.pFftwRealOut;

	/* Now build half-complex-vector */
	pFftwRealIn[0] = cvI[0].real();
	for (i = 1; i < iShortLength; i++)
	{
		pFftwRealIn[i] = cvI[i].real();
		pFftwRealIn[iLongLength - i] = cvI[i].imag();
	}
	/* Nyquist frequency */
	pFftwRealIn[iShortLength] = cvI[iShortLength].real(); 

	/* Actual fftw call */
	fftwf_execute(FftPlans.RFFTPlBackw);

	/* Scale output vector */
	const CReal scale = (CReal) 1.0 / iLongLength;
	for (i = 0; i < iLongLength; i++) 
		fvReturn[i] = pFftwRealOut[i] * scale;

	return fvReturn;
}

CMatlibVector<CReal> FftFilt(const CMatlibVector<CComplex>& rvH,
							 const CMatlibVector<CReal>& rvI,
							 CMatlibVector<CReal>& rvZ,
							 CFftPlans& FftPlans)
{
/*
	This function only works with EVEN N!
*/
	const int				iL(rvH.GetSize() - 1); /* Nyquist frequency! */
	const int				iL2(2 * iL);
	CMatlibVector<CReal>	rvINew(iL2);
	CMatlibVector<CReal>	rvOutTMP(iL2);

	/* Init the plan */
	FftPlans.Init(iL2);

	/* Update history of input signal */
	rvINew.Merge(rvZ, rvI);

	rvOutTMP = rifft(rfft(rvINew, FftPlans) * rvH, FftPlans);

	/* Save old input signal vector for next block */
	rvZ = rvI;

	/* Cut out correct samples (to get from cyclic convolution to linear
	   convolution) */
	return rvOutTMP(iL + 1, iL2);
}


/* FftPlans implementation -------------------------------------------------- */
CFftPlans::CFftPlans(const int iFftSize) :
	RFFTPlForw(nullptr), RFFTPlBackw(nullptr), pFftwRealIn(nullptr), pFftwRealOut(nullptr),
	FFTPlForw(nullptr), FFTPlBackw(nullptr), pFftwComplexIn(nullptr), pFftwComplexOut(nullptr),
	bInitialized(false), bFixedSizeInit(false), fftw_n(0)
{
	/* Static initialization of CMutex not working on Mac OS X */
    if (!_matlib_mutex) _matlib_mutex = new CMutex();

	/* If iFftSize is non zero then proceed to initialization */
	if (iFftSize)
		Init(iFftSize);
}

CFftPlans::~CFftPlans()
{
	/* This one is obvious, no need to comment! */
	Clean();
}

void CFftPlans::Init(const int iFSi, EFFTPlan eFFTPlan)
{
	/* Init some flags */
	bool bNeedPlanInit = false;
	bool bSizeChanged = false;

	/* If Init(const int iFSi) as been previously called
	   then the fft size can't be changed here
	   (not reelly needed but keep the original behaviour) */
	if (!bFixedSizeInit)
		bSizeChanged = InitInternal(iFSi);

	/* The fft size has changed so we don't need to check for plan init */
	if (!bSizeChanged)
	{
		/* Check if the requested plan need an initialization */
		switch (eFFTPlan)
		{
		case FP_RFFTPlForw:  bNeedPlanInit = !RFFTPlForw;  break;
		case FP_RFFTPlBackw: bNeedPlanInit = !RFFTPlBackw; break;
		case FP_FFTPlForw:   bNeedPlanInit = !FFTPlForw;   break;
		case FP_FFTPlBackw:  bNeedPlanInit = !FFTPlBackw;  break;
		}
	}

	/* The following fftw_ call might be cpu intensive, they must be called only when needed */
	if (bSizeChanged || bNeedPlanInit)
	{
//		fprintf(stderr, "CFftPlans::Init() bSizeChanged=%i bNeedPlanInit=%i iFSi=%i fftw_n=%i\n", bSizeChanged, bNeedPlanInit, iFSi, fftw_n);
		MATLIB_MUTEX_LOCK();
		switch (eFFTPlan)
		{
		case FP_RFFTPlForw:
			RFFTPlForw = fftwf_plan_r2r_1d(fftw_n, pFftwRealIn, pFftwRealOut, FFTW_R2HC, PLANNER_FLAGS);
			break;
		case FP_RFFTPlBackw:
			RFFTPlBackw = fftwf_plan_r2r_1d(fftw_n, pFftwRealIn, pFftwRealOut, FFTW_HC2R, PLANNER_FLAGS);
			break;
		case FP_FFTPlForw:
			FFTPlForw = fftwf_plan_dft_1d(fftw_n, pFftwComplexIn, pFftwComplexOut, FFTW_FORWARD, PLANNER_FLAGS);
			break;
		case FP_FFTPlBackw:
			FFTPlBackw = fftwf_plan_dft_1d(fftw_n, pFftwComplexIn, pFftwComplexOut, FFTW_BACKWARD, PLANNER_FLAGS);
			break;
		}
		MATLIB_MUTEX_UNLOCK();
	}
}

void CFftPlans::Init(const int iFSi)
{
	/* The bFixedSizeInit flag isn't really needed,
	   it only keep the original Init() behaviour */
	bFixedSizeInit = true;
	InitInternal(iFSi);
}

bool CFftPlans::InitInternal(const int iFSi)
{
	if (bInitialized)
	{
		/* No change, so return */
		if (fftw_n == iFSi)
			return false; /* The fft size is the same */

		/* Delete old plans and intermediate buffers */
		Clean();
	}

// TODO intermediate buffers should be created only when needed
    /* Create new intermediate buffers */
	pFftwRealIn = new float[iFSi];
	pFftwRealOut = new float[iFSi];
	pFftwComplexIn = new fftwf_complex[iFSi];
	pFftwComplexOut = new fftwf_complex[iFSi];

	fftw_n = iFSi;
	bInitialized = true;
	return true; /* The fft size has changed */
}

void CFftPlans::Clean()
{
	if (bInitialized)
	{
		/* Delete old plans and intermediate buffers */
		MATLIB_MUTEX_LOCK();
		if (RFFTPlForw)  fftwf_destroy_plan(RFFTPlForw);
		if (RFFTPlBackw) fftwf_destroy_plan(RFFTPlBackw);
		if (FFTPlForw)   fftwf_destroy_plan(FFTPlForw);
		if (FFTPlBackw)  fftwf_destroy_plan(FFTPlBackw);
		MATLIB_MUTEX_UNLOCK();

		delete[] pFftwRealIn;
		delete[] pFftwRealOut;
		delete[] pFftwComplexIn;
		delete[] pFftwComplexOut;

		pFftwRealIn = nullptr;
		pFftwRealOut = nullptr;
		pFftwComplexIn = nullptr;
		pFftwComplexOut = nullptr;

		RFFTPlForw = nullptr;
		RFFTPlBackw = nullptr;
		FFTPlForw = nullptr;
		FFTPlBackw = nullptr;

		fftw_n = 0;

		bInitialized = false;
	}
}


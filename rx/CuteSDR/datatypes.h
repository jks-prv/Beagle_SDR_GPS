//////////////////////////////////////////////////////////////////////
// datatypes.h: Common data type declarations
//
// History:
//	2010-09-15  Initial creation MSW
//	2011-03-27  Initial release
//	2013-07-28  Added single/double precision math macros
//////////////////////////////////////////////////////////////////////

#pragma once

#include "types.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

//comment out to use single precision math
//#define USE_DOUBLE_PRECISION

//define single or double precision reals and complex types

typedef struct {
	f32_t re;
	f32_t im;
} tSComplex;

typedef struct {
	d64_t re;
	d64_t im;
} tDComplex;

typedef struct {
	s4_t re;
	s4_t im;
} tIComplex24;

typedef struct {
	s2_t left;
	s2_t right;
} tStereo16;

typedef struct {
	s4_t left;
	s4_t right;
} tStereo24;

#ifdef USE_DOUBLE_PRECISION
 #define TYPEREAL	d64_t
 #define TYPECPX	tDComplex
#else
 #define TYPEREAL	f32_t
 #define TYPECPX	tSComplex
#endif

#ifdef USE_DOUBLE_PRECISION
 #define MSIN(x) sin(x)
 #define MCOS(x) cos(x)
 #define MPOW(x,y) pow(x,y)
 #define MEXP(x) exp(x)
 #define MFABS(x) fabs(x)
 #define MLOG(x) log(x)
 #define MLOG10(x) log10(x)
 #define MSQRT(x) sqrt(x)
 #define MATAN(x) atan(x)
 #define MFMOD(x,y) fmod(x,y)
 #define MATAN2(x,y) atan2(x,y)
 #define MASIN(x) asin(x)
 #define MROUND(x) round(x)

 #define MFFTW_COMPLEX fftw_complex
 #define MFFTW_MALLOC fftw_malloc
 #define MFFTW_FREE fftw_free
 #define MFFTW_PLAN fftw_plan
 #define MFFTW_PLAN_DFT_1D fftw_plan_dft_1d
 #define MFFTW_DESTROY_PLAN fftw_destroy_plan
 #define MFFTW_EXECUTE fftw_execute
#else
 #define MSIN(x) sinf(x)
 #define MCOS(x) cosf(x)
 #define MPOW(x,y) powf(x,y)
 #define MEXP(x) expf(x)
 #define MFABS(x) fabsf(x)
 #define MLOG(x) logf(x)
 #define MLOG10(x) log10f(x)
 #define MSQRT(x) sqrtf(x)
 #define MATAN(x) atanf(x)
 #define MFMOD(x,y) fmodf(x,y)
 #define MATAN2(x,y) atan2f(x,y)
 #define MASIN(x) asinf(x)
 #define MROUND(x) roundf(x)

 #define MFFTW_COMPLEX fftwf_complex
 #define MFFTW_MALLOC fftwf_malloc
 #define MFFTW_FREE fftwf_free
 #define MFFTW_PLAN fftwf_plan
 #define MFFTW_PLAN_DFT_1D fftwf_plan_dft_1d
 #define MFFTW_DESTROY_PLAN fftwf_destroy_plan
 #define MFFTW_EXECUTE fftwf_execute
#endif

#define TYPECPX24       tIComplex24
#define TYPESTEREO16    tStereo16
#define TYPESTEREO24    tStereo24
#define TYPEMONO16      s2_t

#define K_AMPMAX 32767.0	//maximum sin wave Pk for 16 bit input data

//#define K_2PI (8.0*MATAN(1))	//maybe some compilers are't too smart to optimize out
#define K_2PI (2.0 * 3.14159265358979323846)
#define K_PI (3.14159265358979323846)
#define K_PI4 (K_PI/4.0)
#define K_PI2 (K_PI/2.0)
#define K_3PI4 (3.0*K_PI4)

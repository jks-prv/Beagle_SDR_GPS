//////////////////////////////////////////////////////////////////////
// datatypes.h: Common data type declarations
//
// History:
//	2010-09-15  Initial creation MSW
//	2011-03-27  Initial release
//	2013-07-28  Added single/double precision math macros
//////////////////////////////////////////////////////////////////////
#ifndef DATATYPES_H
#define DATATYPES_H

#include "cuteSDR.h"

#include <types.h>
#include <iostream>
#include <string.h>
#include <stdio.h>
#include <math.h>

//comment out to use single precision math
//#define USE_DOUBLE_PRECISION

//define single or double precision reals and complex types
typedef float tSReal;
typedef double tDReal;

struct tSComplex
{
	tSReal re;
	tSReal im;
};

struct tDComplex
{
	tDReal re;
	tDReal im;
};

struct tStereo16
{
	s2_t re;
	s2_t im;
};

struct tStereo24
{
	s4_t re;
	s4_t im;
};

#ifdef USE_DOUBLE_PRECISION
 #define TYPEREAL	tDReal
 #define TYPECPX	tDComplex
#else
 #define TYPEREAL	tSReal
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
#endif

#define TYPESTEREO16 tStereo16
#define TYPESTEREO24 tStereo24
#define TYPEMONO16 s2_t

typedef unsigned char u8_t;
typedef unsigned int u32_t;
typedef signed int qint32;

#define K_AMPMAX 32767.0	//maximum sin wave Pk for 16 bit input data

//#define K_2PI (8.0*MATAN(1))	//maybe some compilers are't too smart to optimize out
#define K_2PI (2.0 * 3.14159265358979323846)
#define K_PI (3.14159265358979323846)
#define K_PI4 (K_PI/4.0)
#define K_PI2 (K_PI/2.0)
#define K_3PI4 (3.0*K_PI4)


#endif // DATATYPES_H

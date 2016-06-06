//////////////////////////////////////////////////////////////////////
// datatypes.h: Common data type declarations
//
// History:
//	2010-09-15  Initial creation MSW
//	2011-03-27  Initial release
//////////////////////////////////////////////////////////////////////
#ifndef DATATYPES_H
#define DATATYPES_H

#include "cuteSDR.h"

#include <types.h>
#include <iostream>
#include <string.h>
#include <stdio.h>
#include <math.h>

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


//#define TYPEREAL tDReal
#define TYPEREAL tSReal
//#define TYPECPX	tDComplex
#define TYPECPX	tSComplex

#define TYPECPXF tSComplex
#define TYPESTEREO16 tStereo16
#define TYPESTEREO24 tStereo24
#define TYPEMONO16 s2_t

typedef unsigned char u8_t;
typedef unsigned int u32_t;
typedef signed int qint32;

#define K_AMPMAX 32767.0	//maximum sin wave Pk for 16 bit input data

//#define K_2PI (8.0*atan(1))	//maybe some compilers are't too smart to optimize out
#define K_2PI (2.0 * 3.14159265358979323846)
#define K_PI (3.14159265358979323846)
#define K_PI4 (K_PI/4.0)
#define K_PI2 (K_PI/2.0)
#define K_3PI4 (3.0*K_PI4)


#endif // DATATYPES_H

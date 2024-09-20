#ifndef _TYPES_H_
#define _TYPES_H_

#include "bits.h"

#include <stdlib.h>

typedef unsigned long long	u64_t;
typedef unsigned int        u4_t;
typedef unsigned char       u1_t;
typedef unsigned short      u2_t;

typedef signed long long	s64_t;
typedef signed int			s4_t;
typedef signed short        s2_t;
typedef signed char         s1_t;

typedef float               f32_t;
typedef double              d64_t;

typedef unsigned int        bf_t;

typedef void (*func_t)();
typedef void (*funcPI_t)(int);
typedef void (*funcPI2_t)(int, int);
typedef void (*funcP_t)(void *);
typedef int (*funcPR_t)(void *);

#define TO_VOID_PARAM(p)    ((void *) (long) (p))
#define FROM_VOID_PARAM(p)  ((long) (p))

#define U1(v) ((u1_t) (v))
#define S1(v) ((s1_t) (v))
#define U2(v) ((u2_t) (v))
#define S2(v) ((s2_t) (v))
#define U4(v) ((u4_t) (v))
#define S4(v) ((s4_t) (v))
#define U8(v) ((u64_t) (v))
#define S8(v) ((s64_t) (v))

#define S16x4_S64(a,b,c,d)	S8( (U8(a)<<48) | (U8(b)<<32) | (U8(c)<<16) | U8(d) )
#define S14_16(w)			S2( U2(w) | ( ((U2(w)) & 0x2000)? 0xc000:0 ) )
#define S14_32(w)			S4( S2( U2(w) | ((U2(w) & 0x2000)? 0xffffc000:0 ) ) )
#define S24_8_16(h8,l16)	S4( (U1(h8)<<16) | U2(l16) | ((U1(h8) & 0x80)? 0xff000000:0) )
#define S24_16_8(h16,l8)	S4( (U2(h16)<<8) | U1(l8) | ((U2(h16) & 0x8000)? 0xff000000:0) )
#define S18_2_16(h2,l16)    S4( (U4(h2)<<16) | U4(l16) | ((U2(h2) & 0x0002)? 0xfffc0000:0) )
#define S32_16_16(h16,l16)  S4( (U4(h16)<<16) | U4(l16) )

#define B2I(bytes)			(((bytes)+3)/4)
#define I2B(ints)			((ints)*4)
#define B2S(bytes)			(((bytes)+1)/2)
#define S2B(shorts)			((shorts)*2)

#define	B3(i)				(((i) >> 24) & 0xff)
#define	B2(i)				(((i) >> 16) & 0xff)
#define	B1(i)				(((i) >>  8) & 0xff)
#define	B0(i)				(((i) >>  0) & 0xff)

#define SET_LE_U32(a, u32)  a[0] = B0(u32); a[1] = B1(u32); a[2] = B2(u32); a[3] = B3(u32);

#define SET_BE_U16(a, u16)  a[0] = B1(u16); a[1] = B0(u16);
#define GET_BE_U16(a)       ((a[0] << 8) | a[1])

#define	FLIP32(i)			((B0(i) << 24) | (B1(i) << 16) | (B2(i) << 8) | (B3(i) << 0))
#define	FLIP16(i)			((B0(i) << 8) | (B1(i) << 0))

#define	B7(i)				(((i) >> 56) & 0xff)
#define	B6(i)				(((i) >> 48) & 0xff)
#define	B5(i)				(((i) >> 40) & 0xff)
#define	B4(i)				(((i) >> 32) & 0xff)

#define	FLIP64(i)			((B0(i) << 56) | (B1(i) << 48) | (B2(i) << 40) | (B3(i) << 32) | (B4(i) << 24) | (B5(i) << 16) | (B6(i) << 8) | (B7(i) << 0))

#ifndef __cplusplus
 typedef	unsigned char	bool;
#endif

#ifndef TRUE
 #define TRUE 1
#endif

#ifndef true
 #define true 1
#endif

#ifndef FALSE
 #define FALSE 0
#endif

#ifndef false
 #define false 0
#endif

#define	NOT_FOUND	-1

#define	ARRAY_EL_LEN(x) ((int) sizeof ((x) [0]))
#define	ARRAY_LEN(x)	((int) (sizeof (x) / ARRAY_EL_LEN(x)))
#define ARRAY_END(x)    (&(x)[ARRAY_LEN(x)])

#define	K		1024
#define	M		(K*K)
#define	B		(M*K)

#define	MHz		1000000
#define	kHz		1000

#define MAX(a,b) ((a)>(b)?(a):(b))
#define MIN(a,b) ((a)<(b)?(a):(b))

static __inline__ u4_t round_up(u4_t val, u4_t size) { 
	return (val + size - 1) / size * size;
}

#define I_DIV_CEIL(v,n) (((v)+(n))/(n))

#define M_2_KM(m) ((m) / 1e3)
#define KM_2_M(km) ((km) * 1e3)

#define DEG_2_RAD(deg) ((deg) * K_PI / 180.0)
#define RAD_2_DEG(rad) ((rad) * 180.0 / K_PI)

#define CLAMP(a,min,max) ( ((a) < (min))? (min) : ( ((a) > (max))? (max) : (a) ) )
#define PN_CLAMP(a,n) ( ((a) < -(n))? -(n) : ( ((a) > (n))? (n) : (a) ) )
#define SI_CLAMP(a,n) ( ((a) > ((n)-1))? ((n)-1) : ( ((a) < -(n))? -(n) : (a) ) )

#define	STRINGIFY(x) #x
#define	STRINGIFY_DEFINE(x) STRINGIFY(x)	// indirection needed for a -Dx=y define
#define	CAT_STRING(x,y) x y			// just a reminder of how this is done: "foo" "bar"
#define	CAT_DEFINE_VAR(x,y) x ## y	// just a reminder of how this is done: foo ## bar

// documentation assistance
#define SPACE_FOR_NULL 1
#define SPACE_FOR_NL   1


// Some packaged code (e.g. FT8, HFDL) needs to exist as .c files and not .cpp
// So compiled namespace linkage to cross-call is needed.

#ifdef __cplusplus
    // separately compiled .c files (e.g. HFDL) need c, not c++, function names to link properly
    #define C_LINKAGE(x) extern "C" { x; }
    #define DEF_0 =0
    #define DEF_NULL =NULL
    #define DEF_FALSE =FALSE
    #define DEF_TRUE =TRUE
#else
    #define C_LINKAGE(x) x;
    #define DEF_0
    #define DEF_NULL
    #define DEF_FALSE
    #define DEF_TRUE
#endif

#endif

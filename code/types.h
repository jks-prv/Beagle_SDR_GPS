#ifndef _TYPES_H_
#define _TYPES_H_

#include <stdlib.h>

typedef unsigned long long	u64_t;
typedef unsigned int        u4_t;
typedef unsigned char       u1_t;
typedef unsigned short      u2_t;

typedef signed long long	s64_t;
typedef signed int			s4_t;
typedef signed short        s2_t;
typedef signed char         s1_t;

#define U1(v) ((u1_t) (v))
#define S1(v) ((s1_t) (v))
#define U2(v) ((u2_t) (v))
#define S2(v) ((s2_t) (v))
#define U4(v) ((u4_t) (v))
#define S4(v) ((s4_t) (v))
#define U8(v) ((u64_t) (v))
#define S8(v) ((s64_t) (v))

#define S16x4_S64(a,b,c,d)	S8( (U8(a)<<48) | (U8(b)<<32) | (U8(c)<<16) | U8(d) )
#define S14_16(w)			S2( (u2_t)(w) | ( (((u2_t)(w)) & 0x2000)? 0xc000:0 ) )
#define S14_32(w)			S4( S2( (u2_t)(w) | ((U2(w) & 0x2000)? 0xffffc000:0 ) ) )
#define S24_8_16(h8,l16)	S4( (U1(h8)<<16) | U2(l16) | ((U1(h8) & 0x80)? 0xff000000:0) )
#define S24_16_8(h16,l8)	S4( (U2(h16)<<8) | U1(l8) | ((U2(h16) & 0x8000)? 0xff000000:0) )

#define B2I(bytes)			(((bytes)+3)/4)
#define I2B(ints)			((ints)*4)
#define B2S(bytes)			(((bytes)+1)/2)
#define S2B(shorts)			((shorts)*2)

#ifndef __cplusplus
 typedef	unsigned char	bool;
#endif

#ifndef TRUE
 #define TRUE 1
#endif

#ifndef FALSE
 #define FALSE 0
#endif

#define	ENUM_BAD	-1

#define	ARRAY_LEN(x)	((int) (sizeof (x) / sizeof ((x) [0])))

#define	K		1024
#define	M		(K*K)
#define	B		(M*K)

#define	MHz		1000000
#define	KHz		1000

#define _2PI	(2.0 * 3.14159265358979323846)

#define MAX(a,b) ((a)>(b)?(a):(b))
#define max(a,b) MAX(a,b)
#define MIN(a,b) ((a)<(b)?(a):(b))
#define min(a,b) MIN(a,b)

#endif

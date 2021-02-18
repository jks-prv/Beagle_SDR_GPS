#include "../types.h"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdlib.h>
#include <unistd.h>

// More experiments with fixed-point multiplication for use with hardware AGC

// Q1.15 = s.mmm MMM = { -1 1/2 1/4 1/8 ... 1/2^15 }
// Q2.14 = sm.mm MMM = { -2 1 1/2 1/4 ... 1/2^14 }
double q1_15_f = pow(2,-(16-1));    // 16-1 because Q1_15 format is s.mmm MMM
double q2_14_f = pow(2,-(15-1));    // 15-1 because Q2_14 format is sm.mm MMM

void p_Q1_15(s2_t q1)
{
    // Q1.15 = s.mmm MMM = { -1 1/2 1/4 1/8 ... 1/2^15 }
    printf("%04x %+.3f\n", U2(q1), q1 * q1_15_f);
}

void p_Q2_14(s2_t q2)
{
	s2_t q1 = q2 << 1;
    printf("%04x %+.3f | %04x %+.3f\n", U2(q2), q2 * q2_14_f, U2(q1), q1 * q1_15_f);
}

// s.mmm MMM * s.mmm MMM = sc.mm MMM MMMM => (s >> 16) | ((mM & 0x3fff8000) >> 15)
#define CONV_MUL_Q2_30_Q1_15(v32) \
	( ( (((v32) & b_sign) >> 16) | (((v32) & 0x3fff8000) >> 15) ) )

void fpmult_Q1_15(s2_t a, const char *as, s2_t b, const char *bs, s2_t p, const char *ps)
{
	u4_t b_sign  = 0x80000000UL;
    s4_t p32 = S4(a * b);
    s4_t ca32 = S4(a);
    s4_t cb32 = S4(b);
    s2_t p16 = CONV_MUL_Q2_30_Q1_15(p32);
    u2_t pc = U2(p);
    u2_t pc16 = U2(p16);
    bool ok = (abs(pc - pc16) <= 1);
    u4_t ptop = U4(p32) >> 28;
    #define OZ(b) (ptop & b)? 1:0
    
    printf("%04x %+.3f * %04x %+.3f = %08x %08x %08x %d%d%d%d %04x %04x %2s %12s %12s %12s\n",
        U2(a), a * q1_15_f, U2(b), b * q1_15_f, U4(ca32), U4(cb32), U4(p32),
        OZ(8), OZ(4), OZ(2), OZ(1), U2(p16), U2(p), ok? "ok":"N", as, bs, ps);
}

// sm.mm MMM * sm.mm MMM = scmm .MMM MMMM => (s >> 16) | ((mM & 0x3fff8000) >> 15)
#define CONV_MUL_Q4_28_Q2_14(v32) \
	( ( (((v32) & b_sign) >> 16) | (((v32) & 0x3fff8000) >> 15) ) )

// Q1.23(24) * Q2.14(16) = Q2.38(40)
// s.mmm MMMMM * sm.mm MMM = sm.mm MMMMMMMMM
void fpmult_Q2_14(s4_t a, const char *as, s2_t b, const char *bs, s64_t p, const char *ps)
{
	u4_t b_sign  = 0x80000000UL;
    s4_t p32 = S4(a * b);
    s4_t ca32 = S4(a);
    s4_t cb32 = S4(b);
    s2_t p16 = CONV_MUL_Q4_28_Q2_14(p32);
    u2_t pc = U2(p);
    u2_t pc16 = U2(p16);
    bool ok = (abs(pc - pc16) <= 1);
    u4_t ptop = U4(p32) >> 28;
    #define OZ(b) (ptop & b)? 1:0
    
    printf("%04x * %04x = %08x %08x %08x %d%d%d%d %04x %04x %2s %12s %12s %12s\n",
        U2(a), U2(b), U4(ca32), U4(cb32), U4(p32), OZ(8), OZ(4), OZ(2), OZ(1), U2(p16), U2(p), ok? "ok":"N", as, bs, ps);
}

int main()
{
    // Q1.15 = s.mmm MMM = { -1 1/2 1/4 1/8 ... 1/2^15 }
    // NB: careful forming neg numbers, must sign-extend! e.g. -1/8 = these set: -1 1/2 1/4 1/8 i.e. neg of 1/8
    #define ONE 0x7fff
    #define HALF 0x4000
    #define QUARTER 0x2000
    #define EIGHTH 0x1000
    #define SIXTEENTH 0x0800
    #define ZERO 0x0000
    #define M_SIXTEENTH 0xf800
    #define M_EIGHTH 0xf000
    #define M_QUARTER 0xe000
    #define M_HALF 0xc000
    #define M_ONE 0x8000
    
    #define FQ1_15(a,b,p) fpmult_Q1_15(a, #a, b, #b, p, #p)

#if 1
    FQ1_15(ONE, ONE, ONE);
    FQ1_15(ONE, HALF, HALF);
    FQ1_15(HALF, HALF, QUARTER);

    FQ1_15(ONE, M_ONE, M_ONE);
    FQ1_15(M_ONE, HALF, M_HALF);
    FQ1_15(M_ONE, M_HALF, HALF);
    
    FQ1_15(M_HALF, M_HALF, QUARTER);
    FQ1_15(M_QUARTER, ONE, M_QUARTER);
    FQ1_15(M_HALF, HALF, M_QUARTER);
    FQ1_15(QUARTER, QUARTER, SIXTEENTH);
    FQ1_15(M_QUARTER, QUARTER, M_SIXTEENTH);
#endif

    // Q2.14 = sm.mm MMM = { -2 1 1/2 1/4 ... 1/2^14 }
    #define Q2_14_ONE 0x4000
    #define Q2_14_HALF 0x2000
    #define Q2_14_QUARTER 0x1000
    #define Q2_14_EIGHTH 0x0800
    #define Q2_14_SIXTEENTH 0x0400
    #define Q2_14_ZERO 0x0000
    #define Q2_14_M_SIXTEENTH 0xfc00
    #define Q2_14_M_EIGHTH 0xf800
    #define Q2_14_M_QUARTER 0xf000
    #define Q2_14_M_HALF 0xe000
    #define Q2_14_M_ONE 0xc000

#if 1
    p_Q1_15(HALF);
    p_Q1_15(M_HALF);

    p_Q2_14(Q2_14_HALF);
    p_Q2_14(Q2_14_M_HALF);
#endif

    #define FQ2_14(a,b,p) fpmult_Q2_14(a, #a, b, #b, p, #p)

#if 0
    FQ2_14(ONE, Q2_14_ONE, ONE);
    FQ2_14(ONE, Q2_14_HALF, HALF);
    FQ2_14(HALF, Q2_14_HALF, QUARTER);

    FQ2_14(ONE, Q2_14_M_ONE, M_ONE);
    FQ2_14(M_ONE, Q2_14_HALF, M_HALF);
    FQ2_14(M_ONE, Q2_14_M_HALF, HALF);
    
    FQ2_14(M_HALF, Q2_14_M_HALF, QUARTER);
    FQ2_14(M_QUARTER, Q2_14_ONE, M_QUARTER);
    FQ2_14(M_HALF, Q2_14_HALF, M_QUARTER);
    FQ2_14(QUARTER, Q2_14_QUARTER, SIXTEENTH);
    FQ2_14(M_QUARTER, Q2_14_QUARTER, M_SIXTEENTH);
#endif

	return 0;
}

#include "types.h"
#include "datatypes.h"

// Checks our understanding of signed fixed-point multiplication
// used in the Verilog for the IQ mixers, GEN attenuator, etc.
//
// Notation used here:
// 's' = sign, 'c' = carry, 'm' = mantissa (fractional part of fixed-point representation)
//	lowercase letter is a single bit, e.g. 's' for sign bit
//	uppercase letter is 4 bits, e.g. 'M' for 4-bits of mantissa
//		so smMMMM is an 18-bit field, 1-bit sign, 17-bits of mantissa
//	"17'b1" is Verilog bit field notation (e.g. 17-bits of ones)
//	"{35, [33 -:17]}" is Verilog bit field composition notation ("-:17" means 17-bits "down from here")
//
// Uses the 18-bit signed fixed-point representation: smMMMM interpreted as:
//		0x1ffff = +max = 0.999... (+1.0 -epsilon)  = { s:0, m:17'b1 }
//		0x00001 = +min = +epsilon = { s:0, m:17'h00001 }
//		0x00000 = zero = 0 = { s:0, m:17'b0 }
//		0x3ffff = -min = -epsilon = { s:1, m:17'b1 }
//		0x20000 = -max = -1.0 = { s:1, m:17'b0 }
// i.e. the decimal point is to the left of the first mantissa bit: {s zero.mmm...}
//
// 36-bits is (1ULL<<36)-1 = 0xfffffffff (9 hex digits) = 36'b1
//
// Was INCORRECTLY interpreting as {csmm MMMmmmx XXXX} and doing {34 -:18} to get result
//		i.e. (s36>>17) & 0x3ffff gives:
// +1 * +1 = 0x1ffff * 0x1ffff = 0x3 fffc 0001 = 0x1fffe (+1 minus lsb)
// -1 * -1 = 0x20000 * 0x20000 = 0x4 0000 0000 = 0x20000 (-1, wrong: overflow s0c1, should be +1)
// +1 * -1 = 0x1ffff * 0x20000 = 0xc 0002 0000 = 0x20001 (-1 plus lsb))
//
// Now CORRECTLY interpreting as {scmm MMMmmmx XXXX} and doing {35, [33 -:17]} to get result
//		i.e. ((s36 & b35) >> 18) | ((s36>>17) & 0x1ffff) gives:
// +1 * +1 = 0x1ffff * 0x1ffff = 0x3 fffc 0001 = 0x1fffe (+1 minus lsb)
// -1 * -1 = 0x20000 * 0x20000 = 0x4 0000 0000 = 0x00000 (0, wrong: overflow s0c1, should be +1)
// +1 * -1 = 0x1ffff * 0x20000 = 0xc 0002 0000 = 0x20001 (-1 plus lsb))
//
// Overflow (s=0 c=1) only ever occurs for -1 * -1.
//
// "Q" fixed-point notation concept
// QIa.Fa op QIb.Fb = QIa+Ib.Fa+Fb e.g. Q1.7 + Q2.6 = Q3.13
// But note in corner case of Q1.n * Q1.n this rule gives Q2.n*2 when only Q1 is required
// to represent +/-1 * +/-1 (i.e. it's not an optimal representation)
// see: www.superkits.net/whitepapers/Fixed%20Point%20Representation%20&%20Fractional%20Math.pdf
//   s.m MMMM       = Q1.17(18) = {-1, 1/2^1, ... 1/2^17}
// sm.mm MMMM MMMM  = Q2.34(36) = {-2, 1, 1/2^1, ... 1/2^34}
// smm.m MMMM MMMM  = Q3.33(36) = {-4, 2, 1, 1/2^1, ... 1/2^33}
//
// Simulated a range of sine values that the Xilinx DDS IP is likely to produce.
// Tested against lots of random values in addition to the corner cases.
//
// Also evaluated the effect of rounding. Found that always adding the next lower bit gave
// some improvement. This is what we've seen in other example code. Note that the addition
// applies to the mantissa AND sign bit as a whole.

#define	D		8		// sin table depth in bits
#define	W		15		// sin table width in bits

#define QUIET 1

// gives e.g. diffs = 1764475
//#define MAX_DIFF 1
//#define ROUND 0
//#define NEG_RND -1

// gives e.g. diffs = 1762242, some improvement
#define MAX_DIFF 1
#define ROUND 1
#define NEG_RND 1

// gives e.g. diffs = 2416375, worse and MAX_DIFF had to be increased to 2
//#define MAX_DIFF 2
//#define ROUND 1
//#define NEG_RND -1

#define EXTS4(v, w) ((s4_t) ((u4_t) (v) | ((u4_t) (v) & (1<<((w)-1))? (~((1<<w)-1)) : 0)))
#define EXTS64(v, w) ((s64_t) ((u64_t) (v) | ((u64_t) (v) & (1<<((w)-1))? (~((1ULL<<w)-1)) : 0)))

// CORRECT s36 interpretation: {scmm MMMmmmx XXXX}
#define CONV_MUL_S36_S18_NEW(s36) \
	( ( ( (((s36) & b_sign) >> 18) | (((s36) >> 17) & 0x1ffff) ) + (((s36) & b_round)? 1:0) ) & 0x3ffff )

// **INCORRECT** s36 interpretation: {csmm MMMmmmx XXXX} (or even {ssmm...})
#define CONV_MUL_S36_S18_OLD(s36) \
	( ( ((s36) >> 17) + ( ((s36) & b_round)? ( ((s36) & b_sign)? NEG_RND:1):0 ) ) & 0x3ffff )

// Why did this work previously at all? Because in all cases (except -1*-1) s=c, so taking {cmm MMMmmm}
// as the result is okay as it gives {smm MMMmmm}.

// choice of conversion is made here:
#define CONV_MUL_S36_S18(s36) CONV_MUL_S36_S18_NEW(s36)
//#define CONV_MUL_S36_S18(s36) CONV_MUL_S36_S18_OLD(s36)


int main()
{
	int i, addr, quad_last;
	u4_t sin_table[1<<D];
	int depth = 1<<D;
	int qdepth = depth/4;
	int W_mask = (1<<W)-1;
	double pacc=0, pinc = K_2PI / depth;
	int run=0, diffs=0;
	
	double s18_f = pow(2,-(18-1));      // 18-1 because 18-bit format is s.m MMMM
	double s35_f = pow(2,-(35-1));      // 35-1 because 36-bit format is sc.mm MMMM MMMM
	
	double max_pos_f = pow(2,31)-1;
	//printf("D=%d/0x%04x D=%db W=%db/0x%x 0x%08x 0x%08x %.12e %d %f %f\n", depth, depth, D, W, W_mask,
	//	(s4_t) (-1.0*max_pos_f), (s4_t) ((-1.0*max_pos_f)-1), max_pos_f, 0x7fffffff,
	//	(double) 0x1ffff * s18_f, (double) EXTS4(0x20000,18) * s18_f);
	double ck_cos = cos(0);
	if ((s4_t) (ck_cos * max_pos_f) != 0x7fffffff) { printf("fail\n"); exit(0); }

	for (i=0; i < depth; i++) {
		pacc = pinc*i;
		double osc = sin(pacc);
		s4_t i_osc = osc * max_pos_f;
		sin_table[i] = (i_osc >> (32-W)) & W_mask;
	}

	u4_t m18 = 0x3ffff;
	u4_t m18_half_range = 0x2ffff;
	u64_t m36 = 0xfffffffffULL;
	u64_t b_sign = 0x800000000ULL;
	u64_t b_carry = 0x400000000ULL;
	u64_t b_round = ROUND? 0x10000 : 0;		// i.e. 1 << (17-1)

    // 18b = { -1 1/2 1/4 1/8 m m MMM }
    // NB: careful forming neg numbers, must sign-extend in 18b part! e.g. -1/8 = these set: -1 1/2 1/4 1/8 i.e. neg of 1/8
	s4_t plus_one = 0x1ffff;
	s4_t plus_one_half = 0x10000;
	s4_t plus_one_quarter = 0x08000;
	s4_t minus_one = EXTS4(0x20000, 18);
	s4_t minus_one_half = EXTS4(0x30000, 18);
	s4_t minus_one_quarter = EXTS4(0x38000, 18);
	
#define CHECK(quiet, title, mulA_s4, mulB_s4, ck_expr) { \
	s64_t prod_s64 = (s64_t) (mulA_s4) * (s64_t) (mulB_s4); \
	bool c = prod_s64 & b_carry; \
	bool s = prod_s64 & b_sign; \
	s4_t r_m18 = CONV_MUL_S36_S18(prod_s64); \
	s4_t r_old_m18 = CONV_MUL_S36_S18_OLD(prod_s64); \
	s4_t r = EXTS4(r_m18, 18); \
	s4_t ck_s4 = (ck_expr); \
	s4_t d = ck_s4 - r; \
	diffs += d? 1:0; \
	bool overflow = (s != c); \
	bool bad = ((d < -MAX_DIFF || d > MAX_DIFF) && !overflow); \
	s4_t t = (prod_s64 >> 18); \
\
	if (0) if (!quiet || bad || overflow) printf("\"%12s\" #%d: %08x %05x %+.3f * %08x %05x %+.3f = %016llx %09llx(36) %+.3lf [%09llx(36) %+.3lf] t: %08x %08x(18) %+.3f ck: %08x %+.3f %08x %+.3f\n", \
		title, run, mulA_s4, mulA_s4 & m18, mulA_s4 * s18_f, mulB_s4, mulB_s4 & m18, mulB_s4 * s18_f, \
		prod_s64, prod_s64 & m36, prod_s64 * s35_f, (-prod_s64) & m36, (-prod_s64) * s35_f, \
		t, t & m18, t*s18_f, \
		ck_s4, ck_s4 * s18_f, -ck_s4, -ck_s4 * s18_f); \
\
	if (1) if (!quiet || bad || overflow) printf("\"%12s\" #%d: %08x %05x %+f * %08x %05x %+f = %016llx %009llx %+lf s%d c%d " \
		"%05x %+f %05x %+f %d%s new: %05x old: %05x %s\n", \
		title, run, mulA_s4, mulA_s4 & m18, mulA_s4 * s18_f, mulB_s4, mulB_s4 & m18, mulB_s4 * s18_f, \
		prod_s64, prod_s64 & m36, prod_s64 * s35_f, s? 1:0, c? 1:0, \
		r_m18, r * s18_f, ck_s4 & m18, ck_s4 * s18_f, d, overflow? "(overflow)":"", r_m18, r_old_m18, \
		bad? "**************************************":""); \
	if (bad) { printf("BAD ***********************************\n"); exit(0); } \
	run++; \
}

#define CHECK_OVERFLOW(quiet, title, mulA_s4, mulB_s4) { \
	s64_t prod_s64 = (s64_t) (mulA_s4) * (s64_t) (mulB_s4); \
	bool c = prod_s64 & b_carry; \
	bool s = prod_s64 & b_sign; \
	s4_t r_m18 = CONV_MUL_S36_S18(prod_s64); \
	bool overflow = (s != c); \
	if (!quiet || overflow) printf("\"%s\" #%d: %05x %+f * %05x %+f = %009llx s%d c%d " \
		"%05x %s\n", \
		title, run, mulA_s4 & m18, mulA_s4 * s18_f, mulB_s4 & m18, mulB_s4 * s18_f, \
		prod_s64 & m36, s? 1:0, c? 1:0, \
		r_m18, overflow? "(overflow)":""); \
	run++; \
}

#define CHECK_SIN(quiet, title, iter, mulB_s4, mulB_f) { \
	if (!quiet) printf("\n%s ===========================\n", title); \
	quad_last = 0; \
	for (addr = 0; addr < iter; addr++) { \
		u4_t quad = (addr >> (D-2)) &3; \
		u4_t sin_18 = sin_table[addr] << (18-W); \
		s4_t sin_s4 = EXTS4(sin_18, 18); \
		s64_t sin_s64 = EXTS64(sin_18, 18); \
		u4_t sin_m18 = sin_s64 & m18; \
		\
		s64_t prod_s64 = sin_s64 * (s64_t) (mulB_s4); \
		bool s = prod_s64 & b_sign; \
		bool c = prod_s64 & b_carry; \
		s4_t r_m18 = CONV_MUL_S36_S18(prod_s64); \
		s4_t r = EXTS4(r_m18, 18); \
		double mulA_f = (double) sin_s4 * s18_f; \
		double prod_f = mulA_f * mulB_f; \
		s4_t ck = (s4_t) (prod_f * pow(2, (18-1))); \
		s4_t d = ck - r; \
		diffs += d? 1:0; \
		bool overflow = (s != c); \
		bool bad = ((d < -MAX_DIFF || d > MAX_DIFF) && !overflow); \
		if (bad) printf("A=%lf B=%lf X=%lf 0x%08x\n", mulA_f, mulB_f, prod_f, ck); \
		\
		if (!quiet && quad != quad_last) { \
			printf("%s ---------------------------------------------------------------\n", title); \
			quad_last = quad; \
		} \
		if (!quiet || bad || overflow) printf("\"%s\" #%d: %4d/%04x %d sin=%05x %+f %+f " \
			"  %05x * %05x = %009llx s%d c%d " \
			"%05x %05x %d%s %s\n", \
			title, run, addr, addr, quad, sin_18, sin_s4 * s18_f, mulB_s4 * s18_f, \
			sin_18, mulB_s4, prod_s64 & m36, s? 1:0, c? 1:0, \
			r_m18, ck & m18, d, overflow? "(overflow)":"", \
			bad? "**************************************":""); \
		if (bad) { printf("BAD ***********************************\n"); exit(0); } \
		run++; \
	} \
}
	
	CHECK(!QUIET, "+1 * +1", plus_one, plus_one, plus_one);
	CHECK(!QUIET, "+1 * -1", plus_one, minus_one, minus_one);
	CHECK(!QUIET, "-1 * -1", minus_one, minus_one, plus_one);
	CHECK(!QUIET, "+1/2 * +1/2", plus_one_half, plus_one_half, plus_one_quarter);
	CHECK(!QUIET, "-1/2 * +1/2", minus_one_half, plus_one_half, minus_one_quarter);
	CHECK(!QUIET, "+1/2 * +1/2", plus_one_half, plus_one_half, plus_one_quarter);
	
	// check mixing of DDS sin output by max gain RF/GEN input: +1 & -1
	#if 1
	CHECK_SIN(QUIET, "SIN * 1", depth, plus_one, +1.0);
	CHECK_SIN(QUIET, "SIN * -1", depth, minus_one, -1.0);
	CHECK_SIN(QUIET, "SIN * 1/2", depth, plus_one_half, +0.5);
	CHECK_SIN(QUIET, "SIN * -1/2", depth, minus_one_half, -0.5);
	#endif
	
	#if 1
	long long ii;
	for (ii=0; ii < 1*B; ii++) {
	//for (i=0; i < 1000000; i++) {
		u4_t mulA_u4 = random() & m18;
		s4_t mulA = EXTS4(mulA_u4, 18);
		u4_t mulB_u4 = random() & m18;
		s4_t mulB = EXTS4(mulB_u4, 18);
		double mulA_f = (double) mulA * s18_f;
		double mulB_f = (double) mulB * s18_f;
		double prod_f = mulA_f * mulB_f;
		s4_t prod_s4 = (s4_t) (prod_f * pow(2, (18-1)));
		CHECK(QUIET, "random * random", mulA, mulB, prod_s4);
	}
	#endif

	u4_t seqA, seqB, last_seq;

	#if 0
	last_seq = m18;
	for (seqA = 0; seqA <= m18; seqA++) {
		if ((seqA & 0x3f000) != (last_seq & 0x3f000)) {
			printf("seqA %05x\n", seqA);
			last_seq = seqA;
		}
		for (seqB = 0; seqB <= m18; seqB++) {
			s4_t mulA = EXTS4(seqA, 18);
			s4_t mulB = EXTS4(seqB, 18);
			CHECK_OVERFLOW(QUIET, "check seq overflow", mulA, mulB);
		}
	}
	#endif

	#if 0
	last_seq = m18;
	for (seqA = 0; seqA <= m18; seqA++) {
		if ((seqA & 0x3f000) != (last_seq & 0x3f000)) {
			printf("seqA %05x\n", seqA);
			last_seq = seqA;
		}
		for (seqB = 0; seqB <= m18; seqB++) {
			s4_t mulA = EXTS4(seqA, 18);
			s4_t mulB = EXTS4(seqB, 18);
			double mulA_f = (double) mulA * s18_f;
			double mulB_f = (double) mulB * s18_f;
			double prod_f = mulA_f * mulB_f;
			s4_t prod_s4 = (s4_t) (prod_f * pow(2, (18-1)));
			CHECK(QUIET, "check seq", mulA, mulB, prod_s4);
		}
	}
	#endif

	#if 1
	for (i=0; i < 1000000; i++) {
		u4_t mul_u4 = random() & m18;
		//u4_t mul_u4 = minus_one;
		//u4_t mul_u4 = random() & m18_half_range;
		s4_t mul = EXTS4(mul_u4, 18);
		if (mul >= plus_one_half) mul -= plus_one_half;
		//if (mul <= minus_one_half) mul += plus_one_half;
		double mul_f = (double) mul * s18_f;
		//printf("mul %x %x %+f\n", mul_u4, mul, mul_f);
		CHECK_SIN(QUIET, "SIN * random", depth, mul, mul_f);
	}
	#endif
	
	printf("ROUND %d, diffs = %d\n", ROUND, diffs);
}

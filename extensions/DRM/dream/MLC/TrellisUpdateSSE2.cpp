/******************************************************************************\
 * Technische Universitaet Darmstadt, Institut fuer Nachrichtentechnik
 * Copyright (c) 2003
 *
 * Author(s):
 *	Volker Fischer, Phil Karn, Morgan Lius
 *
 * Description:
 *
	SSE2 fixed-point implementation of trellis update

	This code is based on a Viterbi sample code from
	Phil Karn, KA9Q (Dec 2001)
	simd-viterbi-2.0.3.zip -> viterbi27.c, mmxbfly27.s, ssebfly27.s
	homepage: http://www.ka9q.net

	Some comments to this MMX code:
	- To compare two 8-bit sized unsigned char, we need to apply a
	  special strategy:
	  psubusb mm5, mm1 // mm5 - mm1
	  pcmpeqb mm5, mm3 // mm3 = 0
	  We subtract unsigned with saturation and afterwards compare for
	  equal to zero. If value in mm1 is larger than the value in mm5, we
	  always get 0 as the result
	- Defining __asm Blocks as C Macros in windows: Put the __asm keyword in
	  front of each assembly instruction
	- If we want to use c-pointers to arrays (like "pOldTrelMetric"), we
	  first have to copy it to a register (like edx), otherwise we get
	  errors
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

#include "ViterbiDecoder.h"


/* Implementation *************************************************************/
#ifdef USE_SSE2
void CViterbiDecoder::TrellisUpdateSSE2(const _DECISIONTYPE* pCurDec,
                                        const _VITMETRTYPE* pCurTrelMetric, const _VITMETRTYPE* pOldTrelMetric,
                                        const _VITMETRTYPE* pchMet1, const _VITMETRTYPE* pchMet2)
{
#ifdef _WIN32
    /**************************************************************************\
    * Windows                                                                  *
    \**************************************************************************/
    __asm
    {
        /* Each invocation of BFLY() will do 8 butterflies in parallel */
#		define BFLY(GROUP) \
		{ \
			/* Compute branch metrics */ \
			__asm mov edx, pOldTrelMetric /* Incoming path metric */ \
			__asm movdqu xmm4, [edx + (16 * GROUP)] /* high bit = 0 */ \
			__asm movdqu xmm5, [edx + ((16 * GROUP) + 32)] /* high bit = 1 */ \
			__asm mov edx, pchMet1 \
			__asm movdqu xmm0, [edx + (16 * GROUP)] \
			__asm mov edx, pchMet2 \
			__asm movdqu xmm3, [edx + (16 * GROUP)] \
			\
			__asm movdqu xmm1, xmm4 /* first set (mm1, mm2) */ \
			__asm paddusb xmm1, xmm0 /* first set: decision for bit = 0 (mm1) */ \
			__asm movdqu xmm2, xmm5 \
			__asm paddusb xmm2, xmm3 /* first set: decision for bit = 1 (mm2) */ \
			__asm movdqu xmm6, xmm4 /* second set (mm6, mm7) */ \
			__asm paddusb xmm6, xmm3 /* second set: decision for bit = 0 (mm6) */ \
			__asm movdqu xmm7, xmm5 \
			__asm paddusb xmm7, xmm0 /* second set: decision for bit = 1 (mm7) */ \
			\
			/* live registers 1 2 6 7. Compare mm1 and mm2; mm6 and mm7 */ \
			__asm movdqu xmm5, xmm2 \
			__asm movdqu xmm4, xmm7 \
			__asm psubusb xmm5, xmm1 /* mm5 = mm2 - mm1 */ \
			__asm psubusb xmm4, xmm6 /* mm4 = mm7 - mm6 */ \
			__asm pxor xmm3, xmm3 /* zero mm3 register, needed for comparison */ \
			__asm pcmpeqb xmm5, xmm3 /* mm5 = first set of decisions */ \
			__asm pcmpeqb xmm4, xmm3 /* mm4 = second set of decisions */ \
			\
			/* live registers 1 2 4 5 6 7. Select survivors. Avoid jumps
        -> mask results with AND and ANDN. then OR */ \
        __asm movdqu xmm3, xmm5 \
        __asm movdqu xmm0, xmm4 \
        __asm pand xmm2, xmm5 \
        __asm pand xmm7, xmm4 \
        __asm pandn xmm3, xmm1 \
        __asm pandn xmm0, xmm6 \
        __asm por xmm2, xmm3 /* mm2: first set survivors (decisions in mm5) */ \
        __asm por xmm7, xmm0 /* mm7: second set survivors (decisions in mm4) */ \
        \
        /* live registers 2 4 5 7 */ \
        /* interleave & store decisions in mm4, mm5 */ \
        /* interleave & store new branch metrics in mm2, mm7 */ \
        __asm movdqu xmm3, xmm5 \
        __asm movdqu xmm0, xmm2 \
        __asm punpcklbw xmm3, xmm4 /* interleave first 8 decisions */ \
        __asm punpckhbw xmm5, xmm4 /* interleave second 8 decisions */ \
        __asm punpcklbw xmm0, xmm7 /* interleave first 8 new metrics */ \
        __asm punpckhbw xmm2, xmm7 /* interleave second 8 new metrics */ \
        __asm mov edx, pCurDec \
        __asm movdqu [edx + (32 * GROUP)], xmm3 \
        __asm movdqu [edx + (32 * GROUP + 16)], xmm5 \
        __asm mov edx, pCurTrelMetric \
        __asm movdqu [edx + (32 * GROUP)], xmm0 /* new metrics */ \
        __asm movdqu [edx + (32 * GROUP + 16)], xmm2 \
    }

    BFLY(0)
    BFLY(1)


    /* -----------------------------------------------------------------
       Normalize by finding smallest metric and subtracting it
       from all metrics */

#if 1 // if 0, always normalize
    /* See if we have to normalize */
    mov eax, [edx] /* Extract first output metric */
    and eax, 255
    cmp eax, 150 /* Is it greater than 150? */
    mov eax, 0
    jle done /* No, no need to normalize */
#endif

    /* Search for minimum, byte-wise for whole register */
    movdqu xmm0, [edx]
    movdqu xmm1, [edx + 16]
    pminub xmm0, xmm1
    movdqu xmm1, [edx + 32]
    pminub xmm0, xmm1
    movdqu xmm1, [edx + 48]
    pminub xmm0, xmm1

    /* mm0 contains 8 smallest metrics
       crunch down to single lowest metric */
    movdqu xmm1, xmm0
    psrldq xmm0, 8 /* The count to psrldq is in bytes not bits! */
    pminub xmm0, xmm1
    movdqu xmm1, xmm0
    psrlq xmm0, 32 /* Compare lowest 4 bytes with highest 4 bytes */
    pminub xmm0, xmm1 /* -> results are in lowest 4 bytes */
    movdqu xmm1, xmm0
    psrlq xmm0, 16 /* Compare lowest 2 bytes with mext 2 bytes */
    pminub xmm0, xmm1 /* -> results are in lowest 2 bytes */
    movdqu xmm1, xmm0
    psrlq xmm0, 8 /* Compare lowest byte with second lowest byte */
    pminub xmm0, xmm1 /* -> resulting minium metric is in lowest byte */

    /* Expand value in lowest byte to all 16 bytes (watch this part better) */
    punpcklbw xmm0,xmm0 /* lowest 2 bytes have same value */
    pshuflw xmm0, xmm0, 0 /*  lowest 8 bytes have same value */
    punpcklqdq xmm0,xmm0 /* all 16 bytes have same value */


    /* mm0 now contains lowest metric in all 8 bytes
       subtract it from every output metric. Trashes mm7 */
#		define PSUBUSBM(MEM, REG) \
		{ \
			__asm movdqu xmm7, MEM \
			__asm psubusb xmm7, REG \
			__asm movdqu MEM, xmm7 \
		}

    PSUBUSBM([edx], mm0)
    PSUBUSBM([edx + 16], mm0)
    PSUBUSBM([edx + 32], mm0)
    PSUBUSBM([edx + 48], mm0)


done:
    /* Needed, when we have used mmx registers and want to use floating
       point operations afterwards */
    emms

#undef BFLY
#undef MINIMUM
#undef PSUBUSBM
}
#else
    /**************************************************************************\
    * Linux                                                                    *
    \**************************************************************************/
    /* Each invocation of BFLY() will do 16 butterflies in parallel */
#	define BFLY(GROUP) \
		/* Compute branch metrics */ \
		"mov %1,%%edx; " /* Incoming path metric (input) */ \
		"movdqu (16 * "GROUP")(%%edx),%%xmm4; " /* high bit = 0 */ \
		"movdqu ((16 * "GROUP") + 32)(%%edx),%%xmm5; "	/* high bit = 1  */ \
		"mov %2,%%eax; "  \
		"mov %3,%%ebx ;"   \
		"movdqu (16 * "GROUP")(%%eax),%%xmm0; "  \
		"movdqu (16 * "GROUP")(%%ebx),%%xmm3; "  \
		\
		"movdqu %%xmm4,%%xmm1; " /* first set (mm1, mm2) */ \
		"paddusb %%xmm0,%%xmm1; " /* first set: decision for bit = 0 (mm1) */ \
		"movdqu %%xmm5,%%xmm2; " \
		"paddusb %%xmm3,%%xmm2; " /* first set: decision for bit = 1 (mm2) */ \
		"movdqu %%xmm4,%%xmm6;" /* second set (mm6, mm7) */ \
		"paddusb %%xmm3,%%xmm6; " /* second set: decision for bit = 0 (mm6) */ \
		"movdqu %%xmm5,%%xmm7; " \
		"paddusb %%xmm0,%%xmm7; " /* second set: decision for bit = 1 (mm7) */ \
		\
		/* live registers 1 2 6 7. Compare mm1 and mm2; mm6 and mm7 */ \
		"movdqu %%xmm2,%%xmm5; " \
		"movdqu %%xmm7,%%xmm4; " \
		"psubusb %%xmm1,%%xmm5; " /* mm5 = mm2 - mm1 */ \
		"psubusb %%xmm6,%%xmm4; " /* mm4 = mm7 - mm6 */ \
		"pxor %%xmm3,%%xmm3; " /* zero mm3 register, needed for comparison */ \
		"pcmpeqb %%xmm3,%%xmm5; " /* mm5 = first set of decisions */ \
		"pcmpeqb %%xmm3,%%xmm4; " /* mm4 = second set of decisions */ \
		\
		/* live registers 1 2 4 5 6 7. Select survivors. Avoid jumps */ \
		/*   -> mask results with AND and ANDN. then OR */ \
		"movdqu %%xmm5,%%xmm3; " \
		"movdqu %%xmm4,%%xmm0; " \
		"pand %%xmm5,%%xmm2; " \
		"pand %%xmm4,%%xmm7; " \
		"pandn %%xmm1,%%xmm3; " \
		"pandn %%xmm6,%%xmm0; " \
		"por %%xmm3,%%xmm2; " /* mm2: first set survivors (decisions in mm5) */ \
		"por %%xmm0,%%xmm7; " /* mm7: second set survivors (decisions in mm4) */ \
		\
		/* live registers 2 4 5 7 */ \
		/* interleave & store decisions in mm4, mm5 */ \
		/* interleave & store new branch metrics in mm2, mm7 */ \
		"movdqu %%xmm5,%%xmm3; " \
		"movdqu %%xmm2,%%xmm0; " \
		"punpcklbw %%xmm4,%%xmm3; " /* interleave first 8 decisions */ \
		"punpckhbw %%xmm4,%%xmm5; " /* interleave second 8 decisions */ \
		"punpcklbw %%xmm7,%%xmm0; " /* interleave first 8 new metrics */ \
		"punpckhbw %%xmm7,%%xmm2; " /* interleave second 8 new metrics */ \
		"mov %4,%%edx; " \
		"movdqu %%xmm3,(32 * "GROUP")(%%edx); "  \
		"movdqu %%xmm5,((32 * "GROUP") + 16)(%%edx); "   \
		"mov %0,%%edx; " \
		"movdqu %%xmm0,(32 * "GROUP")(%%edx); " /* new metrics */ \
		"movdqu %%xmm2,((32 * "GROUP") + 16)(%%edx); " \
 

    asm
    (
        BFLY("0")
        BFLY("1")


        /* -----------------------------------------------------------------
           Normalize by finding smallest metric and subtracting it
           from all metrics */
#if 1 // if 0, always normalize
        /* See if we have to normalize */

        "mov (%%edx),%%eax ;" /* Extract first output metric */
        "and $255,%%eax ;"
        "cmp $150,%%eax ;" /* Is it greater than 150? */
        "mov $0,%%eax ;"
        "jle done ;" /* No, no need to normalize. Where is the label done? */
#endif


        /* Search for minimum, byte-wise for whole register */
        "movdqu (%%edx),%%xmm0 ;"
        "movdqu 16(%%edx),%%xmm1 ;"
        "pminub %%xmm1,%%xmm0 ;"
        "movdqu 32(%%edx),%%xmm1 ;"
        "pminub %%xmm1,%%xmm0 ;"
        "movdqu 48(%%edx),%%xmm1 ;" /* Offset is in bytes  */
        "pminub %%xmm1,%%xmm0 ;"

        /* xmm0 contains 16 smallest metrics
           crunch down to single lowest metric */
        "movdqu %%xmm0,%%xmm1 ;"
        "psrldq $8,%%xmm0 ;" /* The count to psrldq is in bytes not bits! */
        "pminub %%xmm1,%%xmm0 ;"
        "movdqu %%xmm0,%%xmm1 ;"
        "psrlq $32,%%xmm0 ;" /* Compare lowest 4 bytes with highest 4 bytes */
        "pminub %%xmm1,%%xmm0 ;" /* -> results are in lowest 4 bytes */
        "movdqu %%xmm0,%%xmm1 ;"
        "psrlq $16,%%xmm0 ;" /* Compare lowest 2 bytes with mext 2 bytes */
        "pminub %%xmm1,%%xmm0 ;" /* -> results are in lowest 2 bytes */
        "movdqu %%xmm0,%%xmm1 ;"
        "psrlq $8,%%xmm0 ;" /* Compare lowest byte with second lowest byte */
        "pminub %%xmm1,%%xmm0 ;" /* -> resulting minium metric is in lowest byte */

        /* Expand value in lowest byte to all 16 bytes (watch this part better) */
        "punpcklbw %%xmm0,%%xmm0 ;"	/* lowest 2 bytes have same value */
        "pshuflw $0,%%xmm0,%%xmm0 ;" /* lowest 8 bytes have same value */
        "punpcklqdq %%xmm0,%%xmm0 ;" /* all 16 bytes have same value */


        /* xmm0 now contains lowest metric in all 16 bytes
           subtract it from every output metric. Trashes mm7 */
#		define PSUBUSBM(REG, MEM) \
			"movdqu "MEM",%%xmm7 ;" \
			"psubusb "REG",%%xmm7 ;" \
			"movdqu %%xmm7,"MEM" ;" \
 

        PSUBUSBM("%%xmm0","(%%edx)")
        PSUBUSBM("%%xmm0","16(%%edx)")
        PSUBUSBM("%%xmm0","32(%%edx)")
        PSUBUSBM("%%xmm0","48(%%edx)")


        "done: emms ;"	/* Needed, when we have used mmx registers and want to use floating
			 point operations afterwards */

                :
                :"m"(pCurTrelMetric),"m"(pOldTrelMetric),"m"(pchMet1),"m"(pchMet2),"m"(pCurDec)
            );

#undef BFLY
#undef PSUBUSBM
#endif
}
#endif

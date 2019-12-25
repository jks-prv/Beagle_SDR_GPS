/******************************************************************************\
 * Technische Universitaet Darmstadt, Institut fuer Nachrichtentechnik
 * Copyright (c) 2003
 *
 * Author(s):
 *	Volker Fischer, Phil Karn, Morgan Lius
 *
 * Description:
 *
	MMX fixed-point implementation of trellis update

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
#ifdef USE_MMX
void CViterbiDecoder::TrellisUpdateMMX(const _DECISIONTYPE* pCurDec,
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
			__asm movq mm4, [edx + (8 * GROUP)] /* high bit = 0 */ \
			__asm movq mm5, [edx + ((8 * GROUP) + 32)] /* high bit = 1 */ \
			__asm mov edx, pchMet1 \
			__asm movq mm0, [edx + (8 * GROUP)] \
			__asm mov edx, pchMet2 \
			__asm movq mm3, [edx + (8 * GROUP)] \
			\
			__asm movq mm1, mm4 /* first set (mm1, mm2) */ \
			__asm paddusb mm1, mm0 /* first set: decision for bit = 0 (mm1) */ \
			__asm movq mm2, mm5 \
			__asm paddusb mm2, mm3 /* first set: decision for bit = 1 (mm2) */ \
			__asm movq mm6, mm4 /* second set (mm6, mm7) */ \
			__asm paddusb mm6, mm3 /* second set: decision for bit = 0 (mm6) */ \
			__asm movq mm7, mm5 \
			__asm paddusb mm7, mm0 /* second set: decision for bit = 1 (mm7) */ \
			\
			/* live registers 1 2 6 7. Compare mm1 and mm2; mm6 and mm7 */ \
			__asm movq mm5, mm2 \
			__asm movq mm4, mm7 \
			__asm psubusb mm5, mm1 /* mm5 = mm2 - mm1 */ \
			__asm psubusb mm4, mm6 /* mm4 = mm7 - mm6 */ \
			__asm pxor mm3, mm3 /* zero mm3 register, needed for comparison */ \
			__asm pcmpeqb mm5, mm3 /* mm5 = first set of decisions */ \
			__asm pcmpeqb mm4, mm3 /* mm4 = second set of decisions */ \
			\
			/* live registers 1 2 4 5 6 7. Select survivors. Avoid jumps
        -> mask results with AND and ANDN. then OR */ \
        __asm movq mm3, mm5 \
        __asm movq mm0, mm4 \
        __asm pand mm2, mm5 \
        __asm pand mm7, mm4 \
        __asm pandn mm3, mm1 \
        __asm pandn mm0, mm6 \
        __asm por mm2, mm3 /* mm2: first set survivors (decisions in mm5) */ \
        __asm por mm7, mm0 /* mm7: second set survivors (decisions in mm4) */ \
        \
        /* live registers 2 4 5 7 */ \
        /* interleave & store decisions in mm4, mm5 */ \
        /* interleave & store new branch metrics in mm2, mm7 */ \
        __asm movq mm3, mm5 \
        __asm movq mm0, mm2 \
        __asm punpcklbw mm3, mm4 /* interleave first 8 decisions */ \
        __asm punpckhbw mm5, mm4 /* interleave second 8 decisions */ \
        __asm punpcklbw mm0, mm7 /* interleave first 8 new metrics */ \
        __asm punpckhbw mm2, mm7 /* interleave second 8 new metrics */ \
        __asm mov edx, pCurDec \
        __asm movq [edx + (16 * GROUP)], mm3 \
        __asm movq [edx + (16 * GROUP + 8)], mm5 \
        __asm mov edx, pCurTrelMetric \
        __asm movq [edx + (16 * GROUP)], mm0 /* new metrics */ \
        __asm movq [edx + (16 * GROUP + 8)], mm2 \
    }

    /* Invoke macro 4 times for a total of 32 butterflies */
    BFLY(0)
    BFLY(1)
    BFLY(2)
    BFLY(3)


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

    /* Search for the minimum metric. Result ist stored in mm0 */
#		define PMINUB_MM0_MM1 \
		{ \
			__asm movq mm2, mm0 \
			__asm psubusb mm2, mm1 /* mm2 = mm0 - mm1 */ \
			__asm pxor mm3, mm3 /* zero mm3 register, needed for comparison */ \
			__asm pcmpeqb mm2, mm3 /* decisions */ \
			\
			__asm pand mm0, mm2 \
			__asm pandn mm2, mm1 \
			__asm por mm0, mm2 \
		}

    /* Search for minimum, byte-wise for whole register */
    movq mm0, [edx]
    movq mm1, [edx + 8]
    PMINUB_MM0_MM1
    movq mm1, [edx + 16]
    PMINUB_MM0_MM1
    movq mm1, [edx + 24]
    PMINUB_MM0_MM1
    movq mm1, [edx + 32]
    PMINUB_MM0_MM1
    movq mm1, [edx + 40]
    PMINUB_MM0_MM1
    movq mm1, [edx + 48]
    PMINUB_MM0_MM1
    movq mm1, [edx + 56]
    PMINUB_MM0_MM1

    /* mm0 contains 8 smallest metrics
       crunch down to single lowest metric */
    movq mm1, mm0
    psrlq mm0, 32 /* Compare lowest 4 bytes with highest 4 bytes */
    PMINUB_MM0_MM1 /* -> results are in lowest 4 bytes */
    movq mm1, mm0
    psrlq mm0, 16 /* Compare lowest 2 bytes with mext 2 bytes */
    PMINUB_MM0_MM1 /* -> results are in lowest 2 bytes */
    movq mm1, mm0
    psrlq mm0, 8 /* Compare lowest byte with second lowest byte */
    PMINUB_MM0_MM1 /* -> resulting minium metric is in lowest byte */

    /* Expand value in lowest byte to all 8 bytes */
    punpcklbw mm0,mm0 /* First 2 bytes have same value */
    punpcklbw mm0,mm0 /* First 4 bytes have same value */
    punpcklbw mm0,mm0 /* All bytes are the same */


    /* mm0 now contains lowest metric in all 8 bytes
       subtract it from every output metric. Trashes mm7 */
#		define PSUBUSBM(MEM, REG) \
		{ \
			__asm movq mm7, MEM \
			__asm psubusb mm7, REG \
			__asm movq MEM, mm7 \
		}

    PSUBUSBM([edx], mm0)
    PSUBUSBM([edx + 8], mm0)
    PSUBUSBM([edx + 16], mm0)
    PSUBUSBM([edx + 24], mm0)
    PSUBUSBM([edx + 32], mm0)
    PSUBUSBM([edx + 40], mm0)
    PSUBUSBM([edx + 48], mm0)
    PSUBUSBM([edx + 56], mm0)


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
    /* Each invocation of BFLY() will do 8 butterflies in parallel */
#	define BFLY(GROUP) \
		/* Compute branch metrics */ \
		"mov %1,%%edx; " /* Incoming path metric (input) */ \
		"movq (8 * "GROUP")(%%edx),%%mm4; " /* high bit = 0 */ \
		"movq ((8 * "GROUP")+32)(%%edx),%%mm5; "	/* high bit = 1  */ \
		"mov %2,%%eax; "  \
		"mov %3,%%ebx ;"   \
		"movq (8 * "GROUP")(%%eax),%%mm0; "  \
		"movq (8 * "GROUP")(%%ebx),%%mm3; "  \
		\
		"movq %%mm4,%%mm1; " /* first set (mm1, mm2) */ \
		"paddusb %%mm0,%%mm1; " /* first set: decision for bit = 0 (mm1) */ \
		"movq %%mm5,%%mm2; " \
		"paddusb %%mm3,%%mm2; " /* first set: decision for bit = 1 (mm2) */ \
		"movq %%mm4,%%mm6;" /* second set (mm6, mm7) */ \
		"paddusb %%mm3,%%mm6; " /* second set: decision for bit = 0 (mm6) */ \
		"movq %%mm5,%%mm7; " \
		"paddusb %%mm0,%%mm7; " /* second set: decision for bit = 1 (mm7) */ \
		\
		/* live registers 1 2 6 7. Compare mm1 and mm2; mm6 and mm7 */ \
		"movq %%mm2,%%mm5; " \
		"movq %%mm7,%%mm4; " \
		"psubusb %%mm1,%%mm5; " /* mm5 = mm2 - mm1 */ \
		"psubusb %%mm6,%%mm4; " /* mm4 = mm7 - mm6 */ \
		"pxor %%mm3,%%mm3; " /* zero mm3 register, needed for comparison */ \
		"pcmpeqb %%mm3,%%mm5; " /* mm5 = first set of decisions */ \
		"pcmpeqb %%mm3,%%mm4; " /* mm4 = second set of decisions */ \
		\
		/* live registers 1 2 4 5 6 7. Select survivors. Avoid jumps */ \
		/*   -> mask results with AND and ANDN. then OR */ \
		"movq %%mm5,%%mm3; " \
		"movq %%mm4,%%mm0; " \
		"pand %%mm5,%%mm2; " \
		"pand %%mm4,%%mm7; " \
		"pandn %%mm1,%%mm3; " \
		"pandn %%mm6,%%mm0; " \
		"por %%mm3,%%mm2; " /* mm2: first set survivors (decisions in mm5) */ \
		"por %%mm0,%%mm7; " /* mm7: second set survivors (decisions in mm4) */ \
		\
		/* live registers 2 4 5 7 */ \
		/* interleave & store decisions in mm4, mm5 */ \
		/* interleave & store new branch metrics in mm2, mm7 */ \
		"movq %%mm5,%%mm3; " \
		"movq %%mm2,%%mm0; " \
		"punpcklbw %%mm4,%%mm3; " /* interleave first 8 decisions */ \
		"punpckhbw %%mm4,%%mm5; " /* interleave second 8 decisions */ \
		"punpcklbw %%mm7,%%mm0; " /* interleave first 8 new metrics */ \
		"punpckhbw %%mm7,%%mm2; " /* interleave second 8 new metrics */ \
		"mov %4,%%edx; " \
		"movq %%mm3,(16 * "GROUP")(%%edx); "  \
		"movq %%mm5,((16 * "GROUP") + 8)(%%edx); "   \
		"mov %0,%%edx; " \
		"movq %%mm0,(16 * "GROUP")(%%edx); " /* new metrics */ \
		"movq %%mm2,((16 * "GROUP") + 8)(%%edx); " \
 

    asm
    (
        /* Invoke macro 4 times for a total of 32 butterflies */
        BFLY("0")
        BFLY("1")
        BFLY("2")
        BFLY("3")


        /* -----------------------------------------------------------------
           Normalize by finding smallest metric and subtracting it
           from all metrics */

#if 1 // if 0, always normalize
        /* See if we have to normalize */

        "mov (%%edx),%%eax; " /* Extract first output metric */
        "and $255,%%eax; "
        "cmp $150,%%eax; " /* Is it greater than 150? */
        "mov $0,%%eax ;"
        "jle done; " /* No, no need to normalize.  */

#endif

        /* Search for the minimum metric. Result ist stored in mm0 */
#		define PMINUBMM0MM1 \
			"movq %%mm0,%%mm2; " \
			"psubusb %%mm1,%%mm2; " /* mm2 = mm0 - mm1 */ \
			"pxor %%mm3,%%mm3; " /* zero mm3 register, needed for comparison */ \
			"pcmpeqb %%mm3,%%mm2; " /* decisions */ \
			"pand %%mm2,%%mm0; " \
			"pandn %%mm1,%%mm2; " \
			"por %%mm2,%%mm0; "


        /* Search for minimum, byte-wise for whole register */
        "movq (%%edx),%%mm0; "
        "movq 8(%%edx),%%mm1; "
        PMINUBMM0MM1
        "movq 16(%%edx),%%mm1; "
        PMINUBMM0MM1
        "movq 24(%%edx),%%mm1;"
        PMINUBMM0MM1
        "movq 32(%%edx),%%mm1; "
        PMINUBMM0MM1
        "movq 40(%%edx),%%mm1; "
        PMINUBMM0MM1
        "movq 48(%%edx),%%mm1; "
        PMINUBMM0MM1
        "movq 56(%%edx),%%mm1; "
        PMINUBMM0MM1

        /* mm0 contains 8 smallest metrics
           crunch down to single lowest metric */

        "movq %%mm0,%%mm1; "
        "psrlq $32,%%mm0; " /* Compare lowest 4 bytes with highest 4 bytes */
        PMINUBMM0MM1 /* -> results are in lowest 4 bytes */
        "movq %%mm0,%%mm1; "
        "psrlq $16,%%mm0; " /* Compare lowest 2 bytes with mext 2 bytes */
        PMINUBMM0MM1 /* -> results are in lowest 2 bytes */
        "movq %%mm0,%%mm1; "
        "psrlq $8,%%mm0; " /* Compare lowest byte with second lowest byte */
        PMINUBMM0MM1 /* -> resulting minium metric is in lowest byte */

        /* Expand value in lowest byte to all 8 bytes */
        "punpcklbw %%mm0,%%mm0; " /* First 2 bytes have same value */
        "punpcklbw %%mm0,%%mm0; " /* First 4 bytes have same value */
        "punpcklbw %%mm0,%%mm0; " /* All bytes are the same */


        /* mm0 now contains lowest metric in all 8 bytes
           subtract it from every output metric. Trashes mm7 */
#		define PSUBUSBM(REG, MEM) \
			"movq "MEM",%%mm7; " \
			"psubusb "REG",%%mm7; " \
			"movq %%mm7,"MEM"; " \
 

        PSUBUSBM("%%mm0","(%%edx)")
        PSUBUSBM("%%mm0","8(%%edx)")
        PSUBUSBM("%%mm0","16(%%edx)")
        PSUBUSBM("%%mm0","24(%%edx)")
        PSUBUSBM("%%mm0","32(%%edx)")
        PSUBUSBM("%%mm0","40(%%edx)")
        PSUBUSBM("%%mm0","48(%%edx)")
        PSUBUSBM("%%mm0","56(%%edx)")


        "done: emms; "	/* Needed, when we have used mmx registers and want to use floating
			 point operations afterwards */

                :
                :"m"(pCurTrelMetric),"m"(pOldTrelMetric),"m"(pchMet1),"m"(pchMet2),"m"(pCurDec)
            );

#undef BFLY
#undef MINIMUM
#undef PSUBUSBM
#endif
}
#endif

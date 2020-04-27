/******************************************************************************\
 * Technische Universitaet Darmstadt, Institut fuer Nachrichtentechnik
 * Copyright (c) 2001
 *
 * Author(s):
 *	Volker Fischer
 *
 * Description:
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

#include "BlockInterleaver.h"


/* Implementation *************************************************************/
/******************************************************************************\
* Block-interleaver base class												   *
\******************************************************************************/
void CBlockInterleaver::MakeTable(CVector<int>& veciIntTable, int iFrameSize, 
								  int it_0)
{
	int i;
	int	iHighestOne;
	int is, iq;

	/* The following equations are taken directly from the DRM-standard paper 
	   (7.3.3 and 7.6) */
	iHighestOne = iFrameSize;
	/* s = 2 ^ ceil(log2(iFrameSize)), means: find the highest "1" of 
	   iFrameSize. The result of the equation is only one "1" at position 
	   "highest position of "1" in iFrameSize plus one". Therefore: 
	   "1 << (16 + 1)".
	   This implementation: shift bits in iHighestOne to the left until a 
	   "1" reaches position 16. Simultaneously the bit in "is" is 
	   right-shifted */
	is = 1 << (16 + 1);
	while (!(iHighestOne & (1 << 16)))
	{
		iHighestOne <<= 1;
		is >>= 1;
	}

	iq = is / 4 - 1;

	veciIntTable[0] = 0;

	for (i = 1; i < iFrameSize; i++)
	{
		veciIntTable[i] = (it_0 * veciIntTable[i - 1] + iq) % is;
		while (veciIntTable[i] >= iFrameSize)
			veciIntTable[i] = (it_0 * veciIntTable[i] + iq) % is;
	}
}

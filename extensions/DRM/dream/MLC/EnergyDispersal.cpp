/******************************************************************************\
 * Technische Universitaet Darmstadt, Institut fuer Nachrichtentechnik
 * Copyright (c) 2001
 *
 * Author(s):
 *	Volker Fischer
 *
 * Description:
 *
 * Note:
 * Input data stream is divided into a regluar stream and the VSPP stream.
 * Both stream are treated independently. The respective positions of the
 * two streams are requested in the init-routine.
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

#include "EnergyDispersal.h"


/* Implementation *************************************************************/
void CEngergyDispersal::ProcessData(CVector<_BINARY>* pbiData)
{
    int			i;
    uint32_t	iTempShiftRegister;
    _BINARY		biPRBSbit;

    /* Init shift register and set all registers to "1" with bit-wise
       not-operation */
    iShiftRegisterSPP = ~uint32_t(0);
    iShiftRegisterVSPP = ~uint32_t(0);

    /* Main routine */
    for (i = 0; i < iNumInBits; i++)
    {
        if (i < iEndIndVSPP)
        {
            /* Calculate new PRBS bit */
            iTempShiftRegister = iShiftRegisterVSPP;

            /* P(X) = X^9 + X^5 + 1,
               in this implementation we have to shift n-1! */
            biPRBSbit = _BINARY(((iTempShiftRegister >> 4) & 1) ^
                                ((iTempShiftRegister >> 8) & 1));

            /* Shift bits in shift register and add new bit */
            iShiftRegisterVSPP <<= 1;
            iShiftRegisterVSPP |= (biPRBSbit & 1);
        }
        else
        {
            /* Calculate new PRBS bit */
            iTempShiftRegister = iShiftRegisterSPP;

            /* P(X) = X^9 + X^5 + 1,
               in this implementation we have to shift n-1! */
            biPRBSbit = _BINARY(((iTempShiftRegister >> 4) & 1) ^
                                ((iTempShiftRegister >> 8) & 1));

            /* Shift bits in shift register and add new bit */
            iShiftRegisterSPP <<= 1;
            iShiftRegisterSPP |= (biPRBSbit & 1);
        }

        /* Apply PRBS to the data-stream */
        (*pbiData)[i] ^= biPRBSbit;
    }
}

void CEngergyDispersal::Init(int iNewNumInBits, int iNewLengthVSPP)
{
    /* Set the internal parameters */
    iNumInBits = iNewNumInBits;
    iEndIndVSPP = iNewLengthVSPP;
}

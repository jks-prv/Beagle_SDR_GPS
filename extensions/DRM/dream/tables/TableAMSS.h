/******************************************************************************\
 * BBC Research & Development
 * Copyright (c) 2005
 *
 * Author(s):
 *	Andrew Murphy
 *
 * Description:
 *	Tables for AMSS
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

#if !defined(__TABLE_AMSS_H__3B0_CA63_4344_BGDEB2B_23E7912__INCLUDED_)
#define __TABLE_AMSS_H__3B0_CA63_4344_BGDEB2B_23E7912__INCLUDED_

#include <string>
#include "../GlobalDefinitions.h"


/* Definitions ****************************************************************/
#define LEN_TABLE_AMSS_CARRIER_MODE		8

const std::string strTableAMSSCarrierMode[LEN_TABLE_AMSS_CARRIER_MODE] =
{
    "No Carrier Control",	// 0 0 0
    "reserved",				// 0 0 1
    "AMC Mode 1 (-3dB)",	// 0 1 0
    "AMC Mode 2 (-6dB)",	// 0 1 1
    "DAM Mode 1 (+3dB)",	// 1 0 0
    "DAM Mode 2 (+6dB)",	// 1 0 1
    "reserved",				// 1 1 0
    "reserved"				// 1 1 1
};


#endif // !defined(__TABLE_AMSS_H__3B0_CA63_4344_BGDEB2B_23E7912__INCLUDED_)

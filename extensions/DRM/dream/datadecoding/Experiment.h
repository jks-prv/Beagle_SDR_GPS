
/******************************************************************************\
 * British Broadcasting Corporation
 * Copyright (c) 2010
 *
 * Author(s):
 *	Julian Cable
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

#ifndef __EXPERIMENT_H
#define __EXPERIMENT_H

#include "../GlobalDefinitions.h"
#include "../util/Vector.h"
# include "journaline/dabdatagroupdecoder.h"

/* Definitions ****************************************************************/


/* Classes ********************************************************************/

class CExperiment
{
public:
	CExperiment();
	virtual ~CExperiment();

	void AddDataUnit(CVector<_BINARY>& vecbiNewData);

protected:
	DAB_DATAGROUP_DECODER_t	dgdec;
	static void dg_cb(const DAB_DATAGROUP_DECODER_msc_datagroup_header_t*,
		const unsigned long len, const unsigned char* buf, void* data);
};

#endif

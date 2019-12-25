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

#include "Experiment.h"
#include <iostream>
using namespace std;

CExperiment::CExperiment() : dgdec(nullptr)
{
	/* Create decoder instance. Pass the pointer to this object. This is needed
	   for the call-back functions! */
	dgdec = DAB_DATAGROUP_DECODER_createDec(dg_cb, this);
}

CExperiment::~CExperiment()
{
	/* Delete decoder instances */
	if (dgdec != nullptr)
		DAB_DATAGROUP_DECODER_deleteDec(dgdec);
}

void CExperiment::AddDataUnit(CVector<_BINARY>& vecbiNewData)
{
	const int iSizeBytes = vecbiNewData.Size() / SIZEOF__BYTE;

	if (iSizeBytes > 0)
	{
		/* Bits to byte conversion */
		CVector<_BYTE> vecbyData(iSizeBytes);
		vecbiNewData.ResetBitAccess();

		for (int i = 0; i < iSizeBytes; i++)
			vecbyData[i] = (_BYTE) vecbiNewData.Separate(SIZEOF__BYTE);

		/* Add new data unit to Journaline decoder library */
		DAB_DATAGROUP_DECODER_putData(dgdec, iSizeBytes, &vecbyData[0]);
	}
}

void CExperiment::dg_cb(const DAB_DATAGROUP_DECODER_msc_datagroup_header_t*,
		const unsigned long len, const unsigned char* buf, void* data)
{
	cerr << "experimental dab data group length " << len << " bytes" << endl;
	CExperiment* This = reinterpret_cast<CExperiment*>(data);
	cerr << (char*)buf;
	// TODO
	(void)This;
}

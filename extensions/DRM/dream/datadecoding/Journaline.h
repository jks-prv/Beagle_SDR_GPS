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

#if !defined(JOURNALINE_H__3B0UBVE987346456363LIHGEW982__INCLUDED_)
#define JOURNALINE_H__3B0UBVE987346456363LIHGEW982__INCLUDED_

#include "../GlobalDefinitions.h"
#include "../util/Vector.h"
# include "journaline/NML.h"
# include "journaline/newssvcdec.h"
# include "journaline/dabdatagroupdecoder.h"

/* Definitions ****************************************************************/
/* Definitions for links which objects are not yet received or items which
   do not have links (no menu) */
#define JOURNALINE_IS_NO_LINK			-2
#define JOURNALINE_LINK_NOT_ACTIVE		-1


/* Classes ********************************************************************/
struct CNewsItem
{
	std::string	sText;
	int		iLink;
};

class CNews
{
public:
	CNews() : sTitle(""), vecItem(0) {}

	std::string				sTitle;
	CVector<CNewsItem>	vecItem;
};


class CJournaline
{
public:
	CJournaline();
	virtual ~CJournaline();

	void GetNews(int iObjID, CNews& News);
	void AddDataUnit(CVector<_BINARY>& vecbiNewData);
	void Reset() {ResetOpenJournalineDecoder();}
	void AddFile(const std::string filename);

protected:
	DAB_DATAGROUP_DECODER_t	dgdec;
	NEWS_SVC_DEC_decoder_t	newsdec;

	void ResetOpenJournalineDecoder();

	/* Callback functions for journaline decoder internal tasks */
	static void obj_avail_cb(unsigned long, NEWS_SVC_DEC_obj_availability_t*,
		void*) {}
	static void dg_cb(const DAB_DATAGROUP_DECODER_msc_datagroup_header_t*,
		const unsigned long len, const unsigned char* buf, void* data)
		{NEWS_SVC_DEC_putData(((CJournaline*) data)->newsdec, len, buf);}
};

#endif // !defined(JOURNALINE_H__3B0UBVE987346456363LIHGEW982__INCLUDED_)

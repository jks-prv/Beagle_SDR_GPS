/******************************************************************************\
 * Technische Universitaet Darmstadt, Institut fuer Nachrichtentechnik
 * Copyright (c) 2007
 *
 * Author(s):
 *	Volker Fischer, Oliver Haffenden, Julian Cable
 *
 * Description:
 *	see RCITagItems.cpp
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

#ifndef RCI_TAG_ITEMS_H_INCLUDED
#define RCI_TAG_ITEMS_H_INCLUDED

#include "MDITagItems.h"

class CTagItemGeneratorCfre : public CTagItemGenerator /* cfre tag */
{
public:
	void GenTag(int iNewFreqkHz);
protected:
	std::string GetTagName(void);
	std::string GetProfiles(void) { return ""; }
};

class CTagItemGeneratorCdmo : public CTagItemGenerator /* cdmo tag */
{
public:
	void GenTag(const ERecMode eMode);
protected:
	std::string GetTagName(void);
	std::string GetProfiles(void) { return ""; }
};

#endif

/******************************************************************************\
 * Technische Universitaet Darmstadt, Institut fuer Nachrichtentechnik
 * Copyright (c) 2004
 *
 * Author(s):
 *	Volker Fischer, Oliver Haffenden
 *
 * Description:
 *	see MDIInBuffer.cpp
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

#ifndef MDI_IN_BUFFER_H_INCLUDED
#define MDI_IN_BUFFER_H_INCLUDED

#include "../GlobalDefinitions.h"
#include <vector>
#include <queue>

class CMDIInBuffer
{
public:
	CMDIInBuffer() : buffer() ,guard(),blocker()
	{}

	void Put(const std::vector<_BYTE>& data);
	void Get(std::vector<_BYTE>& data);

protected:
    std::queue< std::vector<_BYTE> > buffer;
	CMutex guard;
	CWaitCondition blocker;
};

#endif

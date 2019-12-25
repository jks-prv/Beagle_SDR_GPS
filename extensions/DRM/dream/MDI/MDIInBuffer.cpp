/******************************************************************************\
 * Technische Universitaet Darmstadt, Institut fuer Nachrichtentechnik
 * Copyright (c) 2004
 *
 * Author(s):
 *	Volker Fischer, Oliver Haffenden
 *
 * Description:
 *	simple blocking buffer
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

#include "MDIInBuffer.h"
#include <iostream>
using namespace std;

/* write the received packet to the buffer, if the previous one was not read yet
 * it will be lost, but we need new data in preference to old and we should be
 * able to keep up, so that's OK
 */
void
CMDIInBuffer::Put(const vector<_BYTE>& data)
{
	guard.Lock();
	buffer.push(data);
	blocker.WakeOne();
	guard.Unlock();
}

/* get the buffer contents, but if it takes more than a second, return an empty buffer
 * clear the buffer after reading it so we don't read the same buffer twice
 * Its possible signals could get lost :(
 */
void
CMDIInBuffer::Get(vector<_BYTE>& data)
{
	guard.Lock();
	if(buffer.empty())
	{
		if(blocker.Wait(&guard, 1000))
		{
			if(buffer.empty())
				data.clear();
			else
			{
				data = buffer.front();
				buffer.pop();
			}
		}
		else
		{
			data.clear();
		}
	}
	else
	{
		data = buffer.front();
		buffer.pop();
	}
	guard.Unlock();
}

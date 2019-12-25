/******************************************************************************\
 * Technische Universitaet Darmstadt, Institut fuer Nachrichtentechnik
 * Copyright (c) 2004
 *
 * Author(s):
 *	Volker Fischer, Julian Cable, Oliver Haffenden
 *
 * Description:
 *
 * This is a dummy implementation of the CPacketSocket interface to enable the CMDI
 * class to work even if no socket support is available. Packets are silently discarded
 * and there are never any incoming packets.
 *
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

#include "PacketSocketNull.h"

// Set the sink which will receive the packets
void CPacketSocketNull::SetPacketSink(CPacketSink*)
{
}

// Stop sending packets to the sink
void CPacketSocketNull::ResetPacketSink(void)
{
}

// Send packet to the socket
void CPacketSocketNull::SendPacket(const vector<_BYTE>&, uint32_t, uint16_t)
{
}

void CPacketSocketNull::poll()
{
}

bool CPacketSocketNull::SetOrigin(const string&)
{
	return false;
}

bool CPacketSocketNull::SetDestination(const string&)
{
	return false;
}

bool CPacketSocketNull::GetDestination(string&)
{
	return false;
}

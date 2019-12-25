/******************************************************************************\
 * British Broadcasting Corporation
 * Copyright (c) 2007
 *
 * Author(s):
 *	Juian Cable
 *
 * Description:
 *	Implementation of a CPacketSource that reads from a file
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

#ifndef _PACKETSOURCE_FILE_H
#define _PACKETSOURCE_FILE_H

#include "../GlobalDefinitions.h"
#include "../util/Vector.h"
#include "../util/Buffer.h"
#include "PacketInOut.h"

class CPacer;

class CPacketSourceFile : public CPacketSource
{
public:
	CPacketSourceFile();
	~CPacketSourceFile();
	// Set the sink which will receive the packets
	void SetPacketSink(CPacketSink *pSink);
	// Stop sending packets to the sink
	void ResetPacketSink(void);
	bool SetOrigin(const std::string& str);
	bool GetOrigin(std::string& str) { (void)str; return false; }
	void poll();

private:

    int readRawAF(std::vector<_BYTE>& vecbydata, int& interval);
    void readRawPFT(std::vector<_BYTE>& vecbydata, int& interval);
    void readFF(std::vector<_BYTE>& vecbydata, int& interval);

    void readPcap(std::vector<_BYTE>& vecbydata, int& interval);
    void readTagPacketHeader(std::string& tag, uint32_t& len);

    CPacketSink		*pPacketSink;
    uint64_t		last_packet_time;
    CPacer*		pacer;
    void*		pF;
    int 		wanted_dest_port;
    enum {pcap,ff,af,pf}    eFileType;
};

#endif

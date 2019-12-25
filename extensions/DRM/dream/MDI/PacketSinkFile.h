/******************************************************************************\
 * British Broadcasting Corporation
 * Copyright (c) 2007
 *
 * Author(s):
 *	Oliver Haffenden
 *
 * Description:
 *	
 *  This defines a concrete subclass of CPacketSink that writes to a file
 *  For the moment this will be a raw file but FF could be added as a decorator
 *  The writing can be stopped and started - if it is not currently writing,
 *  any packets it receives will be silently discarded
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

#ifndef PACKET_SINK_FILE_INCLUDED
#define PACKET_SINK_FILE_INCLUDED

#include "PacketInOut.h"

class CPacketSinkFile : public CPacketSink
{
public:
	CPacketSinkFile();
	virtual ~CPacketSinkFile() {}

	virtual void SendPacket(const std::vector<_BYTE>& vecbydata, uint32_t addr=0, uint16_t port=0);

	virtual bool SetDestination(const std::string& strFName);
	virtual bool GetDestination(std::string& strFName) { strFName = strFileName; return true; }
	void StartRecording();
	void StopRecording();

protected:
	virtual void open()=0;
	virtual void close()=0;
	virtual void write(const std::vector<_BYTE>& vecbydata)=0;

	FILE *pFile;
	bool bIsRecording;
	bool bChangeReceived;
	std::string strFileName;
};

class CPacketSinkRawFile : public CPacketSinkFile
{
public:
	CPacketSinkRawFile();
	virtual ~CPacketSinkRawFile();
protected:
	virtual void open();
	virtual void close();
	virtual void write(const std::vector<_BYTE>& vecbydata);
};

class CPacketSinkFileFraming : public CPacketSinkFile
{
public:
	CPacketSinkFileFraming();
	virtual ~CPacketSinkFileFraming();
protected:
	virtual void open();
	virtual void close();
	virtual void write(const std::vector<_BYTE>& vecbydata);
};

class CPacketSinkPcapFile : public CPacketSinkFile
{
public:
	CPacketSinkPcapFile();
	virtual ~CPacketSinkPcapFile();
protected:
	virtual void open();
	virtual void close();
	virtual void write(const std::vector<_BYTE>& vecbydata);
};

#endif

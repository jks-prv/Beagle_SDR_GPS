/******************************************************************************\
 * British Broadcasting Corporation
 * Copyright (c) 2006
 *
 * Author(s):
 *	Oliver Haffenden
 *
 * Description:
 *
 *	This class represents a particular consumer of RSI information and supplier of
 *  RCI commands. There could be several of these. The profile is a property of the
 *  particular subscriber and different subscribers could have different profiles.
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

#include "PacketSocket.h"
#include "RSISubscriber.h"
#include "../DRMReceiver.h"
#include "TagPacketGenerator.h"


CRSISubscriber::CRSISubscriber(CPacketSink *pSink) : pPacketSink(pSink),
	cProfile(0), bNeedPft(false), fragment_size(0), pDRMReceiver(0),
	bUseAFCRC(true), sequence_counter(0)
{
	TagPacketDecoderRSCIControl.SetSubscriber(this);
}

void CRSISubscriber::SetReceiver(CDRMReceiver *pReceiver)
{
	pDRMReceiver = pReceiver;
	TagPacketDecoderRSCIControl.SetReceiver(pReceiver);
}

void CRSISubscriber::SetProfile(const char c)
{
	cProfile = c;
}

void CRSISubscriber::SetPFTFragmentSize(const int iFrag)
{
    if(iFrag>0)
    {
        fragment_size = iFrag;
        bNeedPft = true;
    }
    else
        bNeedPft = false;
}

void CRSISubscriber::TransmitPacket(CTagPacketGenerator& Generator)
{
    if (pPacketSink != nullptr)
	{
	 	Generator.SetProfile(cProfile);
		vector<_BYTE> packet = AFPacketGenerator.GenAFPacket(bUseAFCRC, Generator);
		if(bNeedPft)
		{
			vector< vector<_BYTE> > packets;
			CPft::MakePFTPackets(packet, packets, sequence_counter, fragment_size);
			sequence_counter++;
			for(size_t i=0; i<packets.size(); i++)
				pPacketSink->SendPacket(packets[i]);
		}
		else
			pPacketSink->SendPacket(packet);
	}
}


/* implementation of function from CPacketSink interface - process incoming RCI commands */
void CRSISubscriber::SendPacket(const vector<_BYTE>& vecbydata, uint32_t, uint16_t)
{
	CVectorEx<_BINARY> vecbidata;
	vecbidata.Init(vecbydata.size()*SIZEOF__BYTE);
	vecbidata.ResetBitAccess();
	for(size_t i=0; i<vecbydata.size(); i++)
		vecbidata.Enqueue(vecbydata[i], SIZEOF__BYTE);
	CTagPacketDecoder::Error err = TagPacketDecoderRSCIControl.DecodeAFPacket(vecbidata);
	if(err != CTagPacketDecoder::E_OK)
		cerr << "bad RSCI Control Packet Received" << endl;
}


/* TODO wrap a sendto in a class and store it in pPacketSink */
CRSISubscriberSocket::CRSISubscriberSocket(CPacketSink *pSink):CRSISubscriber(pSink),pSocket(nullptr)
{
	pSocket = new CPacketSocketNative;
	pPacketSink = pSocket;
}

CRSISubscriberSocket::~CRSISubscriberSocket()
{
	delete pSocket;
}

bool CRSISubscriberSocket::SetDestination(const string& dest)
{
	if(pSocket==nullptr)
	{
		return false;
	}
	string d = dest;
	if(d[0] == 'P' || d[0] == 'p')
	{
		SetPFTFragmentSize(800);
		d.erase(0, 1);
	}
	bool bOk = pSocket->SetDestination(d);
	if(bOk)
		pSocket->SetPacketSink(this);
	return bOk;
}

bool CRSISubscriberSocket::GetDestination(string& str)
{
	/* want the canonical version so incoming can match */
	if(pSocket)
		return pSocket->GetDestination(str);
	return false;
}

bool CRSISubscriberSocket::SetOrigin(const string& str)
{
	if(pSocket==nullptr)
	{
		return false;
	}
	// Delegate to socket
	bool bOK = pSocket->SetOrigin(str);
	if (bOK)
	{
		// Connect socket to the MDI decoder
		pSocket->SetPacketSink(this);
		return bOK;
	}
	return false;
}

bool CRSISubscriberSocket::GetOrigin(string& str)
{
	if(pSocket==nullptr)
	{
		return false;
	}
	// Delegate to socket
	return pSocket->GetOrigin(str);
}

/* poll for incoming packets */
void CRSISubscriberSocket::poll()
{
	if(pSocket!=nullptr)
		pSocket->poll();
}

CRSISubscriberFile::CRSISubscriberFile(): CRSISubscriber(nullptr), pPacketSinkFile(nullptr)
{
	/* override the subscriber back to nullptr to prevent Cpro doing anything */
	TagPacketDecoderRSCIControl.SetSubscriber(nullptr);
}

bool CRSISubscriberFile::SetDestination(const string& strFName)
{
    string dest = strFName;
	if(pPacketSink)
	{
		delete pPacketSink;
		pPacketSink = nullptr;
		pPacketSinkFile = nullptr;
	}
	string ext;
	size_t p = strFName.rfind('.');
	if (p != string::npos)
		ext = strFName.substr(p + 1);
	if (ext == "pcap")
		pPacketSinkFile = new CPacketSinkPcapFile;
	else if(ext == "ff")
	{
		pPacketSinkFile = new CPacketSinkFileFraming;
		dest.erase(p);
	}
	else
		pPacketSinkFile = new CPacketSinkRawFile;
	if(pPacketSinkFile && pPacketSinkFile->SetDestination(dest))
	{
		pPacketSinkFile->StartRecording();
		pPacketSink = pPacketSinkFile;
		return true;
	}
	return false;
}

bool CRSISubscriberFile::GetDestination(string& strFName)
{
	if(pPacketSinkFile)
		return pPacketSinkFile->GetDestination(strFName);
	return false;
}

void CRSISubscriberFile::StartRecording()
{
	if(pPacketSinkFile)
		pPacketSinkFile->StartRecording();
}

void CRSISubscriberFile::StopRecording()
{
	if(pPacketSinkFile)
		pPacketSinkFile->StopRecording();
}

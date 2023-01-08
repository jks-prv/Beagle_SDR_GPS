/******************************************************************************\
 * Technische Universitaet Darmstadt, Institut fuer Nachrichtentechnik
 * Copyright (c) 2001
 *
 * Author(s):
 *	Volker Fischer, Julian Cable
 *
 * Description:
 *	Data module (using multimedia information carried in DRM stream)
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

#include "DRM.h"
#include "DataDecoder.h"
#include "epgutil.h"
#include "Journaline.h"
#include "Experiment.h"
#include <iostream>

CDataDecoder::CDataDecoder ():iServPacketID (0), DoNotProcessData (true),
	Journaline(*new CJournaline()),
	Experiment(*new CExperiment()),
	iOldJournalineServiceID (0)
{
		for(size_t i=0; i<MAX_NUM_PACK_PER_STREAM; i++)
			eAppType[i] = AT_NOT_SUP;
}

CDataDecoder::~CDataDecoder ()
{
	delete &Journaline;
}

void
CDataDecoder::ProcessDataInternal(CParameter & Parameters)
{
	int i, j;
	int iPacketID;
	int iNewContInd;
	int iNewPacketDataSize;
	int iOldPacketDataSize;
	int iNumSkipBytes;
	_BINARY biFirstFlag;
	_BINARY biLastFlag;
	_BINARY biPadPackInd;
	CCRC CRCObject;

	/* Check if something went wrong in the initialization routine */
	if (DoNotProcessData)
		return;

	/* CRC check for all packets -------------------------------------------- */
	/* Reset bit extraction access */
	(*pvecInputData).ResetBitAccess();

	for (j = 0; j < iNumDataPackets; j++)
	{
		/* Check the CRC of this packet */
		CRCObject.Reset(16);

		/* "- 2": 16 bits for CRC at the end */
		for (i = 0; i < iTotalPacketSize - 2; i++)
		{
			_BYTE b =pvecInputData->Separate(SIZEOF__BYTE);
			CRCObject.AddByte(b);
		}

		/* Store result in vector and show CRC in multimedia window */
		uint16_t crc = pvecInputData->Separate(16);
		int iShortID = Parameters.GetCurSelDataService();
		if (CRCObject.CheckCRC(crc))
		{
			veciCRCOk[j] = 1;	/* CRC ok */
			Parameters.DataComponentStatus[iShortID].SetStatus(RX_OK);
		}
		else
		{
			veciCRCOk[j] = 0;	/* CRC wrong */
			Parameters.DataComponentStatus[iShortID].SetStatus(CRC_ERROR);
		}
	}

	/* Extract packet data -------------------------------------------------- */
	/* Reset bit extraction access */
	(*pvecInputData).ResetBitAccess();

	for (j = 0; j < iNumDataPackets; j++)
	{
		/* Check if CRC was ok */
		if (veciCRCOk[j] == 1)
		{
			/* Read header data --------------------------------------------- */
			/* First flag */
			biFirstFlag = (_BINARY) (*pvecInputData).Separate(1);

			/* Last flag */
			biLastFlag = (_BINARY) (*pvecInputData).Separate(1);

			/* Packet ID */
			iPacketID = (int) (*pvecInputData).Separate(2);

			/* Padded packet indicator (PPI) */
			biPadPackInd = (_BINARY) (*pvecInputData).Separate(1);

			/* Continuity index (CI) */
			iNewContInd = (int) (*pvecInputData).Separate(3);

			/* Act on parameters given in header */
			/* Continuity index: this 3-bit field shall increment by one
			   modulo-8 for each packet with this packet Id */
			if ((iContInd[iPacketID] + 1) % 8 != iNewContInd)
				DataUnit[iPacketID].bOK = false;

			/* Store continuity index */
			iContInd[iPacketID] = iNewContInd;

			/* Reset flag for data unit ok when receiving the first packet of
			   a new data unit */
			if (biFirstFlag == 1)
			{
				DataUnit[iPacketID].Reset();
				DataUnit[iPacketID].bOK = true;
			}

			/* If all packets are received correctly, data unit is ready */
			if (biLastFlag == 1)
			{
				if (DataUnit[iPacketID].bOK)
					DataUnit[iPacketID].bReady = true;
			}

			/* Data field --------------------------------------------------- */
			/* Get size of new data block */
			if (biPadPackInd == 1)
			{
				/* Padding is present: the first byte gives the number of
				   useful data bytes in the data field. */
				iNewPacketDataSize =
					(int) (*pvecInputData).Separate(SIZEOF__BYTE) *
					SIZEOF__BYTE;

				if (iNewPacketDataSize > iMaxPacketDataSize)
				{
					/* Error, reset flags */
					DataUnit[iPacketID].bOK = false;
					DataUnit[iPacketID].bReady = false;

					/* Set values to read complete packet size */
                    iNewPacketDataSize = iMaxPacketDataSize; // TODO check the semantics here iNewPacketDataSize;
					iNumSkipBytes = 2;	/* Only CRC has to be skipped */
				}
				else
				{
					/* Number of unused bytes ("- 2" because we also have the
					   one byte which stored the size, the other byte is the
					   header) */
					iNumSkipBytes = iTotalPacketSize - 2 -
						iNewPacketDataSize / SIZEOF__BYTE;
				}

				/* Packets with no useful data are permitted if no packet
				   data is available to fill the logical frame. The PPI
				   shall be set to 1 and the first byte of the data field
				   shall be set to 0 to indicate no useful data. The first
				   and last flags shall be set to 1. The continuity index
				   shall be incremented for these empty packets */
				if ((biFirstFlag == 1) &&
					(biLastFlag == 1) && (iNewPacketDataSize == 0))
				{
					/* Packet with no useful data, reset flag */
					DataUnit[iPacketID].bReady = false;
				}
			}
			else
			{
				iNewPacketDataSize = iMaxPacketDataSize;

				/* All bytes are useful bytes, only CRC has to be skipped */
				iNumSkipBytes = 2;
			}

			/* Add new data to data unit vector (bit-wise copying) */
			iOldPacketDataSize = DataUnit[iPacketID].vecbiData.Size();

			DataUnit[iPacketID].vecbiData.Enlarge(iNewPacketDataSize);

			/* Read useful bits */
			for (i = 0; i < iNewPacketDataSize; i++)
				DataUnit[iPacketID].vecbiData[iOldPacketDataSize + i] =
					(_BINARY) (*pvecInputData).Separate(1);

			/* Read bytes which are not used */
			for (i = 0; i < iNumSkipBytes; i++)
				(*pvecInputData).Separate(SIZEOF__BYTE);

			/* Use data unit ------------------------------------------------ */
			if (DataUnit[iPacketID].bReady)
			{
				/* Decode all IDs regardless whether activated or not
				   (iPacketID == or != iServPacketID) */
				/* Only DAB multimedia is supported */
				//cout << "new data unit for packet id " << iPacketID << " apptype " << eAppType[iPacketID] << endl;

				if(eAppType[iPacketID] == AT_NOT_SUP)
				{
					int iCurSelDataServ = Parameters.GetCurSelDataService();
					// TODO int iCurDataStreamID = Parameters.Service[iCurSelDataServ].DataParam.iStreamID;
				    //printf("AT_NOT_SUP iPacketID=%d iCurSelDataServ=%d ", iPacketID, iCurSelDataServ);
					for (int i = 0; i <=3; i++)
					{
						if(Parameters.Service[iCurSelDataServ].DataParam.iPacketID == iPacketID)
						{
							eAppType[iPacketID] = GetAppType(Parameters.Service[iCurSelDataServ].DataParam);
							//printf("#%d MATCH eAppType[iPacketID]=%d\n", i, eAppType[iPacketID]);
						}
                        //else printf("#%d NO-MATCH iPacketID=%d\n", i, Parameters.Service[iCurSelDataServ].DataParam.iPacketID);
					}
				}

				switch (eAppType[iPacketID])
				{
				case AT_MOTSLIDESHOW:	/* MOTSlideshow */
					/* Packet unit decoding */
				    //printf("AT_MOTSLIDESHOW iPacketID=%d\n", iPacketID);
		            DRM_msg_encoded(DRM_MSG_SERVICE, "drm_slideshow_status", (char *) "1");
					MOTObject[iPacketID].
						AddDataUnit(DataUnit[iPacketID].vecbiData);
					break;
				case AT_EPG:	/* EPG */
					/* Packet unit decoding */
				    //printf("AT_EPG iPacketID=%d iEPGPacketID=%d iEPGService=%d\n", iPacketID, iEPGPacketID, iEPGService);
				    //printf("AT_EPG MOT path <%s>\n", Parameters.GetDataDirectory("MOT").c_str());
					//cout << iEPGPacketID << " " << iPacketID << endl; cout.flush();
					if(iEPGPacketID == -1)
					{
						cerr << "data unit received but EPG packetId not set" << endl;
						iEPGPacketID = iPacketID;
					}
					MOTObject[iEPGPacketID].AddDataUnit(DataUnit[iPacketID].vecbiData);
					break;

				case AT_BROADCASTWEBSITE:	/* MOT Broadcast Web Site */
					/* Packet unit decoding */
				    //printf("AT_BROADCASTWEBSITE iPacketID=%d\n", iPacketID);
					MOTObject[iPacketID].AddDataUnit(DataUnit[iPacketID].vecbiData);
					break;

				case AT_JOURNALINE:
				    //printf("AT_JOURNALINE iPacketID=%d\n", iPacketID);
					Journaline.AddDataUnit(DataUnit[iPacketID].vecbiData);
					break;

				case AT_EXPERIMENTAL:
				    //printf("AT_EXPERIMENTAL iPacketID=%d\n", iPacketID);
					Experiment.AddDataUnit(DataUnit[iPacketID].vecbiData);
					break;

				default:		/* do nothing */
				    //printf("AT_default eAppType=%d iPacketID=%d\n", eAppType[iPacketID], iPacketID);
					;
				}

				/* Packet was used, reset it now for new filling with new data
				   (this will also reset the flag
				   "DataUnit[iPacketID].bReady") */
				DataUnit[iPacketID].Reset();
			}
		}
		else
		{
			/* Skip incorrect packet */
			for (i = 0; i < iTotalPacketSize; i++)
				(*pvecInputData).Separate(SIZEOF__BYTE);
		}
	}
	if(iEPGService >= 0)	/* if EPG decoding is active */
		DecodeEPG(Parameters);
}

void
CDataDecoder::DecodeEPG(const CParameter & Parameters)
{
	/* Application Decoding - must run all the time and in background */
    //printf("DecodeEPG iEPGPacketID=%d MOTObject=%d\n", iEPGPacketID, MOTObject[iEPGPacketID].NewObjectAvailable());
	if ((DoNotProcessData == false)
		&& (iEPGPacketID >= 0)
		&& MOTObject[iEPGPacketID].NewObjectAvailable())
	{
		//cerr << "EPG object" << endl;
		CMOTObject NewObj;
		MOTObject[iEPGPacketID].GetNextObject(NewObj);
		string fileName;
		bool advanced = false;
		//printf("EPG obj iContentType=%d\n", NewObj.iContentType);
		if (NewObj.iContentType == 7)
		{
			for (size_t i = 0; i < NewObj.vecbProfileSubset.size(); i++)
				if (NewObj.vecbProfileSubset[i] == 2)
				{
					advanced = true;
					break;
				}
			int iScopeId = NewObj.iScopeId;
			if (iScopeId == 0)
				iScopeId = Parameters.Service[iEPGService].iServiceID;
			fileName = epgFilename(NewObj.ScopeStart, iScopeId,
								   NewObj.iContentSubType, advanced);

#if !defined(HAVE_LIBZ) && !defined(HAVE_LIBFREEIMAGE)
			const string& s = NewObj.strName;
			if (s.size() >= 3)
				if (s.substr(s.size() - 3, 3) == ".gz")
					fileName += ".gz";
#endif
		}
		else
		{
			fileName = NewObj.strName;
		}

		//string path = Parameters.GetDataDirectory("EPG") + fileName;
		//mkdirs(path);
        string path = "/tmp/kiwi.data/drm.ch"+ to_string(DRM_rx_chan()) +"_"+ fileName;
		//cerr << "writing EPG file " << path << endl;
		printf("DRM write EPG file <%s>\n", path.c_str());
		FILE *f = fopen(path.c_str(), "wb");
		if (f)
		{
			fwrite(&NewObj.Body.vecData.front(), 1,
				   NewObj.Body.vecData.size(), f);
			fclose(f);
		}
	}
}

void
CDataDecoder::InitInternal(CParameter & Parameters)
{
	int iTotalNumInputBits;
	int iTotalNumInputBytes;
	int iCurDataStreamID;
	int iCurSelDataServ;

	/* Init error flag */
	DoNotProcessData = false;

	/* Get current selected data service */
	iCurSelDataServ = Parameters.GetCurSelDataService();

	/* Get current data stream ID */
	iCurDataStreamID =
		Parameters.Service[iCurSelDataServ].DataParam.iStreamID;

	/* Get number of total input bits (and bytes) for this module */
	iTotalNumInputBits = Parameters.iNumDataDecoderBits;
	iTotalNumInputBytes = iTotalNumInputBits / SIZEOF__BYTE;

	/* Get the packet ID of the selected service */
	iServPacketID =
		Parameters.Service[iCurSelDataServ].DataParam.iPacketID;

	/* Init application type (will be overwritten by correct type later */
	eAppType[iServPacketID] = AT_NOT_SUP;

	/* Check, if service is activated. Also, only packet services can be
	   decoded */
	if ((iCurDataStreamID != STREAM_ID_NOT_USED) &&
		(Parameters.Service[iCurSelDataServ].DataParam.
		 ePacketModInd == CDataParam::PM_PACKET_MODE))
	{
		/* Calculate total packet size. DRM standard: packet length: this
		   field indicates the length in bytes of the data field of each
		   packet specified as an unsigned binary number (the total packet
		   length is three bytes longer as it includes the header and CRC
		   fields) */
		iTotalPacketSize =
			Parameters.Service[iCurSelDataServ].DataParam.iPacketLen + 3;

		/* Check total packet size, could be wrong due to wrong SDC */
		if ((iTotalPacketSize <= 0) ||
			(iTotalPacketSize > iTotalNumInputBytes))
		{
			/* Set error flag */
			DoNotProcessData = true;
		}
		else
		{
			/* Maximum number of bits for the data part in a packet
			   ("- 3" because two bits for CRC and one for the header) */
			iMaxPacketDataSize = (iTotalPacketSize - 3) * SIZEOF__BYTE;

			/* Number of data packets in one data block */
			iNumDataPackets = iTotalNumInputBytes / iTotalPacketSize;

			eAppType[iServPacketID] = GetAppType(Parameters.Service[iCurSelDataServ].DataParam);

			/* Check, if service ID of Journaline application has
			   changed, that indicates that a new transmission is
			   received -> reset decoder in this case. Otherwise
			   use old buffer. That ensures that the decoder keeps
			   old data in buffer when synchronization was lost for
			   a short time */
			const uint32_t iNewServID = Parameters.Service[iCurSelDataServ].iServiceID;

			if((eAppType[iServPacketID]==AT_JOURNALINE) && (iOldJournalineServiceID != iNewServID))
			{

				// Problem: if two different services point to the same stream, they have different
				// IDs and the Journaline is reset! TODO: fix this problem...

				/* Reset Journaline decoder and store the new service
				   ID number */
				//printf("Journaline iOldJournalineServiceID=%X iNewServID=%X\n", iOldJournalineServiceID, iNewServID);
				Journaline.Reset();
				iOldJournalineServiceID = iNewServID;
			}

			/* Init vector for storing the CRC results for each packet */
			veciCRCOk.Init(iNumDataPackets);

			/* Reset data units for all possible data IDs */
			for (int i = 0; i < MAX_NUM_PACK_PER_STREAM; i++)
			{
				DataUnit[i].Reset();

				/* Reset continuity index (CI) */
				iContInd[i] = 0;
			}
		}
	}
	else
		DoNotProcessData = true;

	/* Set input block size */
	iInputBlockSize = iTotalNumInputBits;

	iEPGService = -1;			/* no service */
	iEPGPacketID = -1;

	/* look for EPG */
	for (int i = 0; i <= 3; i++)
	{
		if ((Parameters.Service[i].DataParam.eAppDomain ==
			 CDataParam::AD_DAB_SPEC_APP)
			&& (Parameters.Service[i].DataParam.iUserAppIdent == 7))
		{
			iEPGService = i;
			iEPGPacketID = Parameters.Service[i].DataParam.iPacketID;
			eAppType[iEPGPacketID] = AT_EPG;
			//cerr << "EPG packet id " << iEPGPacketID << endl;
		}
	}
}

bool
	CDataDecoder::GetMOTObject(CMOTObject & NewObj,
							   const EAppType eAppTypeReq)
{
	bool bReturn = false;

	/* Lock resources */
	Lock();

	/* Check if data service is current MOT application */
	if ((DoNotProcessData == false)
		&& (eAppType[iServPacketID] == eAppTypeReq)
		&& MOTObject[iServPacketID].NewObjectAvailable())
	{
		MOTObject[iServPacketID].GetNextObject(NewObj);
		bReturn = true;
	}
	/* Release resources */
	Unlock();

	return bReturn;
}

bool
	CDataDecoder::GetMOTDirectory(CMOTDirectory & MOTDirectoryOut,
								  const EAppType eAppTypeReq)
{
	bool bReturn = false;

	/* Lock resources */
	Lock();

	/* Check if data service is current MOT application */
	if ((DoNotProcessData == false)
		&& (eAppType[iServPacketID] == eAppTypeReq))
	{
		MOTObject[iServPacketID].GetDirectory(MOTDirectoryOut);
		bReturn = true;
	}
	/* Release resources */
	Unlock();

	return bReturn;
}

void
CDataDecoder::GetNews(const int iObjID, CNews & News)
{
	/* Lock resources */
	Lock();

	/* Check if data service is Journaline application */
	if ((DoNotProcessData == false)
		&& (eAppType[iServPacketID] == AT_JOURNALINE))
	{
		Journaline.GetNews(iObjID, News);
	}

	/* Release resources */
	Unlock();
}

CDataDecoder::EAppType CDataDecoder::GetAppType(const CDataParam& DataParam)
{
	EAppType eAppType = AT_NOT_SUP;

		/* Only DAB application supported */
	if (DataParam.eAppDomain == CDataParam::AD_DAB_SPEC_APP)
	{
		/* Get application identifier of current selected service, only
		   used with DAB */
		const int iDABUserAppIdent = DataParam.iUserAppIdent;

		switch (iDABUserAppIdent)
		{
		case DAB_AT_MOTSLIDESHOW:		/* MOTSlideshow */
			eAppType = AT_MOTSLIDESHOW;
			break;

		case DAB_AT_BROADCASTWEBSITE:		/* MOT Broadcast Web Site */
			eAppType = AT_BROADCASTWEBSITE;
			break;

		case DAB_AT_TPEG:
			eAppType = AT_TPEG;
			break;

		case DAB_AT_DGPS:
			eAppType = AT_DGPS;
			break;

		case DAB_AT_TMC:
			eAppType = AT_TMC;
			break;

		case DAB_AT_EPG:
			eAppType  = AT_EPG;
			break;

		case DAB_AT_JAVA:
			eAppType = AT_JAVA;
			break;

		case DAB_AT_JOURNALINE:	/* Journaline */
			eAppType  = AT_JOURNALINE;
			break;

		case AT_DMB: case AT_VOICE: case AT_MIDDLEWARE: case AT_IPDC:
		case DAB_AT_DREAM_EXPERIMENTAL:	/* Just save the objects as files */
			eAppType  = AT_EXPERIMENTAL;
			break;
		}
	}
	return eAppType;
}

/******************************************************************************\
 * Technische Universitaet Darmstadt, Institut fuer Nachrichtentechnik
 * Copyright (c) 2001
 *
 * Author(s):
 *	Volker Fischer
 *
 * Description:
 *	See DataDecoder.cpp
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

#if !defined(DATADECODER_H__3B0BA660_CA3452363E7A0D31912__INCLUDED_)
#define DATADECODER_H__3B0BA660_CA3452363E7A0D31912__INCLUDED_

#include "../GlobalDefinitions.h"
#include "../Parameter.h"
#include "../util/Modul.h"
#include "../util/CRC.h"
#include "../util/Vector.h"
#include "DABMOT.h"
#include "MOTSlideShow.h"

class CExperiment;
class CJournaline;
class CNews;

/* Definitions ****************************************************************/
/* Maximum number of packets per stream */
#define MAX_NUM_PACK_PER_STREAM					4

/* Define for application types */
#define DAB_AT_DREAM_EXPERIMENTAL 1
#define DAB_AT_MOTSLIDESHOW 2
#define DAB_AT_BROADCASTWEBSITE 3
#define DAB_AT_TPEG 4
#define DAB_AT_DGPS 5
#define DAB_AT_TMC 	6
#define DAB_AT_EPG 	7
#define DAB_AT_JAVA 	8
#define DAB_AT_DMB 	9
#define DAB_AT_IPDC 	0xa
#define DAB_AT_VOICE 	0xb
#define DAB_AT_MIDDLEWARE 	0xc
#define DAB_AT_JOURNALINE 0x44A

class CDataDecoder:public CReceiverModul < _BINARY, _BINARY >
{
  public:
    CDataDecoder ();
    virtual ~CDataDecoder ();

    enum EAppType
    { AT_NOT_SUP, AT_MOTSLIDESHOW, AT_JOURNALINE,
	AT_BROADCASTWEBSITE, AT_TPEG, AT_DGPS, AT_TMC, AT_EPG,
	    AT_JAVA, AT_EXPERIMENTAL, AT_DMB, AT_VOICE, AT_MIDDLEWARE, AT_IPDC
    };

    bool GetMOTObject (CMOTObject & NewPic, const EAppType eAppTypeReq);
    bool GetMOTDirectory (CMOTDirectory & MOTDirectoryOut, const EAppType eAppTypeReq);
	CMOTDABDec *getApplication(int iPacketID) { return (iPacketID>=0 && iPacketID<3)?&MOTObject[iPacketID]:nullptr; }
    void GetNews (const int iObjID, CNews & News);
    EAppType GetAppType ()
    {
		return eAppType[iServPacketID];
    }

    CJournaline& Journaline;
    int iEPGService;

  protected:
    class CDataUnit
    {
      public:
	    CVector < _BINARY > vecbiData;
        bool bOK;
        bool bReady;

        void Reset ()
        {
            vecbiData.Init (0);
            bOK = false;
            bReady = false;
        }
    };

    int iTotalPacketSize;
    int iNumDataPackets;
    int iMaxPacketDataSize;
    int iServPacketID;
    CVector < int >veciCRCOk;

    bool DoNotProcessData;

    int iContInd[MAX_NUM_PACK_PER_STREAM];
    CDataUnit DataUnit[MAX_NUM_PACK_PER_STREAM];
    CMOTDABDec MOTObject[MAX_NUM_PACK_PER_STREAM];
    CExperiment& Experiment;
    uint32_t iOldJournalineServiceID;

    EAppType eAppType[MAX_NUM_PACK_PER_STREAM];

    virtual void InitInternal (CParameter & Parameters);
    virtual void ProcessDataInternal (CParameter & Parameters);

    int iEPGPacketID;
    void DecodeEPG(const CParameter& Parameters);
	EAppType GetAppType(const CDataParam&);
};


#endif // !defined(DATADECODER_H__3B0BA660_CA3452363E7A0D31912__INCLUDED_)

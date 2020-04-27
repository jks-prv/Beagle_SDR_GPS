/******************************************************************************\
 * British Broadcasting Corporation
 * Copyright (c) 2007
 *
 * Author(s):
 *	Volker Fischer, Oliver Haffenden, Julian Cable, Andrew Murphy
 *
 * Description:
 *	see MDIRSCI.cpp
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


#if !defined(MDI_CONCRETE_H_INCLUDED)
#define MDI_CONCRETE_H_INCLUDED

#include "../GlobalDefinitions.h"

#include "MDIInBuffer.h"

#include "../util/Modul.h"
#include "../util/CRC.h"
#include "Pft.h"

#include "MDIDefinitions.h"
#include "MDITagItems.h"
#include "RCITagItems.h"
#include "TagPacketDecoderRSCIControl.h"
#include "TagPacketGenerator.h"
#include "RSISubscriber.h"
#include <vector>

/* Classes ********************************************************************/
class CUpstreamDI : public CReceiverModul<_BINARY, _BINARY> , public CPacketSink
{
public:
	CUpstreamDI();
	virtual ~CUpstreamDI();

	/* CRSIMDIInInterface */
	bool SetOrigin(const std::string& strAddr);
	bool GetInEnabled() {return source != nullptr;}

	/* CRCIOutInterface */
	bool SetDestination(const std::string& strArgument);
	bool GetOutEnabled() {return bMDIOutEnabled;}
	void SetAFPktCRC(const bool bNAFPktCRC) {bUseAFCRC=bNAFPktCRC;}
	void SetFrequency(int iNewFreqkHz);
	void SetReceiverMode(ERecMode eNewMode);

	/* CPacketSink */
	virtual void SendPacket(const std::vector<_BYTE>& vecbydata, uint32_t addr=0, uint16_t port=0);

	bool GetDestination(std::string& strArgument);

	/* CReceiverModul */
	void InitInternal(CParameter& Parameters);
	void ProcessDataInternal(CParameter& Parameters);

protected:

	std::string						strOrigin;
	std::string						strDestination;
	CMDIInBuffer	  			queue;
	CPacketSource*				source;
	CRSISubscriberSocket		sink;
	CPft						Pft;

	bool						bUseAFCRC;

	CSingleBuffer<_BINARY>		MDIInBuffer;
	bool						bMDIOutEnabled;
	bool						bNeedPft;

	/* Tag Item Generators */

	CTagItemGeneratorProTyRSCI TagItemGeneratorProTyRSCI; /* *ptr tag */
	CTagItemGeneratorCfre TagItemGeneratorCfre;
	CTagItemGeneratorCdmo TagItemGeneratorCdmo;

	/* TAG Packet generator */
	CTagPacketGenerator TagPacketGenerator;
	CAFPacketGenerator AFPacketGenerator;

};

class CDownstreamDI: public CPacketSink
{
public:
	CDownstreamDI();
	virtual ~CDownstreamDI();

	void GenDIPacket();
	void poll();

	void SendLockedFrame(CParameter& Parameter,
						CSingleBuffer<_BINARY>& FACData,
						CSingleBuffer<_BINARY>& SDCData,
						std::vector<CSingleBuffer<_BINARY> >& vecMSCData
	);
	void SendUnlockedFrame(CParameter& Parameter); /* called once per frame even if the Rx isn't synchronised */
	void SendAMFrame(CParameter& Parameter, CSingleBuffer<_BINARY>& CodedAudioData);

	void SetAFPktCRC(const bool bNAFPktCRC);

	bool AddSubscriber(const std::string& dest, const char profile, const std::string& origin="");

	bool SetOrigin(const std::string& strAddr);
	void SetRSIRecording(CParameter& Parameter, bool bOn, char cPro, const std::string& type="");
	void NewFrequency(CParameter& Parameter); /* needs to be called in case a new RSCI file needs to be started */

	virtual bool GetOutEnabled() {return bMDIOutEnabled;}
	virtual bool GetInEnabled() {return bMDIInEnabled;}
	void GetNextPacket(CSingleBuffer<_BINARY>&	buf);
	void SetReceiver(CDRMReceiver *pReceiver);

	/* CPacketSink */
	virtual void SendPacket(const std::vector<_BYTE>& vecbydata, uint32_t addr=0, uint16_t port=0);
	bool SetDestination(const std::string& strArgument);
	bool GetDestination(std::string& strArgument);

	std::string GetRSIfilename(CParameter& Parameter, const char cProfile);

protected:

	void ResetTags();

	uint32_t					iLogFraCnt;
	CDRMReceiver*				pDrmReceiver;

	bool					bMDIOutEnabled;
	bool					bMDIInEnabled;
	bool					bNeedPft;

	bool					bIsRecording;
	int							iFrequency;
	std::string						strRecordType;


	/* Generators for all of the MDI and RSCI tags */

	CTagItemGeneratorProTyMDI TagItemGeneratorProTyMDI; /* *ptr tag */
	CTagItemGeneratorProTyRSCI TagItemGeneratorProTyRSCI; /* *ptr tag */
	CTagItemGeneratorLoFrCnt TagItemGeneratorLoFrCnt ; /* dlfc tag */
	CTagItemGeneratorFAC TagItemGeneratorFAC; /* fac_ tag */
	CTagItemGeneratorSDC TagItemGeneratorSDC; /* sdc_ tag */
	CTagItemGeneratorSDC TagItemGeneratorSDCEmpty; /* empty sdc_ tag for use in non-SDC frames */
	CTagItemGeneratorSDCChanInf TagItemGeneratorSDCChanInf; /* sdci tag */
	CTagItemGeneratorRobMod TagItemGeneratorRobMod; /* robm tag */
	CTagItemGeneratorRINF TagItemGeneratorRINF; /* info tag */
	CTagItemGeneratorRWMF TagItemGeneratorRWMF; /* RWMF tag */
	CTagItemGeneratorRWMM TagItemGeneratorRWMM; /* RWMM tag */
	CTagItemGeneratorRMER TagItemGeneratorRMER; /* RMER tag */
	CTagItemGeneratorRDOP TagItemGeneratorRDOP; /* RDOP tag */
	CTagItemGeneratorRDEL TagItemGeneratorRDEL; /* RDEL tag */
	CTagItemGeneratorRAFS TagItemGeneratorRAFS; /* RAFS tag */
	CTagItemGeneratorRINT TagItemGeneratorRINT; /* RINT tag */
	CTagItemGeneratorRNIP TagItemGeneratorRNIP; /* RNIP tag */
	CTagItemGeneratorSignalStrength TagItemGeneratorSignalStrength; /* rdbv tag */
	CTagItemGeneratorReceiverStatus TagItemGeneratorReceiverStatus; /* rsta tag */

	CTagItemGeneratorProfile TagItemGeneratorProfile; /* rpro */
	CTagItemGeneratorRxDemodMode TagItemGeneratorRxDemodMode; /* rdmo */
	CTagItemGeneratorRxFrequency TagItemGeneratorRxFrequency; /* rfre */
	CTagItemGeneratorRxActivated TagItemGeneratorRxActivated; /* ract */
	CTagItemGeneratorRxBandwidth TagItemGeneratorRxBandwidth; /* rbw_ */
	CTagItemGeneratorRxService TagItemGeneratorRxService; /* rser */

	CTagItemGeneratorGPS TagItemGeneratorGPS; /* rgps */
	CTagItemGeneratorPowerSpectralDensity TagItemGeneratorPowerSpectralDensity; /* rpsd */
    CTagItemGeneratorPowerImpulseResponse TagItemGeneratorPowerImpulseResponse; /* rpir */
	CTagItemGeneratorPilots TagItemGeneratorPilots; /* rpil */

	CVector<CTagItemGeneratorStr>	vecTagItemGeneratorStr; /* strx tag */
	CTagItemGeneratorAMAudio TagItemGeneratorAMAudio; /* rama tag */

	/* Mandatory tags but not implemented yet */
	CVector<CTagItemGeneratorRBP>	vecTagItemGeneratorRBP;

	/* TAG Packet generator */
	CTagPacketGeneratorWithProfiles TagPacketGenerator;

	std::vector< CRSISubscriber *>		RSISubscribers;
	CRSISubscriberFile*				pRSISubscriberFile;
	CPacketSource*					source;
	CPacketSink*					sink;
	CSingleBuffer<_BINARY>			MDIInBuffer;

};

#endif // !defined(MDI_H__3B0346264660_CA63_3452345DGERH31912__INCLUDED_)

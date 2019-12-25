/******************************************************************************\
 * Technische Universitaet Darmstadt, Institut fuer Nachrichtentechnik
 * Copyright (c) 2001-2007
 *
 * Author(s):
 *	Volker Fischer, Andrew Murphy, Andrea Russo
 *
 * Description:
 *	See Parameter.cpp
 *
 * 10/01/2007 Andrew Murphy, BBC Research & Development, 2005
 *	- Additions to include additional RSCI related fields
 *
 * 11/21/2005 Andrew Murphy, BBC Research & Development, 2005
 *	- Additions to include AMSS demodulation (Added class
 *    CAltFreqOtherServicesSign)
 *
 * 11/28/2005 Andrea Russo
 *	- Added classes for store alternative frequencies schedules and regions
 *
 *******************************************************************************
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

#if !defined(PARAMETER_H__3B0BA660_CA63_4344_BB2B_23E7A0D31912__INCLUDED_)
#define PARAMETER_H__3B0BA660_CA63_4344_BB2B_23E7A0D31912__INCLUDED_

#include "GlobalDefinitions.h"
#include "OFDMcellmapping/CellMappingTable.h"
#include "matlib/Matlib.h"
#include <time.h>
#include "ServiceInformation.h"
#include <map>
#include <iostream>
#ifdef HAVE_LIBGPS
# include <gps.h>
#else
    struct gps_fix_t {
	double time;
	double latitude;
	double longitude;
	double altitude;
	double speed;
	double track;
    };
    struct gps_data_t {
	uint32_t set;
#define TIME_SET        0x00000002u
#define LATLON_SET      0x00000008u
#define ALTITUDE_SET    0x00000010u
#define SPEED_SET       0x00000020u
#define TRACK_SET       0x00000040u
#define STATUS_SET      0x00000100u
#define SATELLITE_SET   0x00010000u
	gps_fix_t fix;
	int    status; 
	int satellites_used;
    };
#endif

class CDRMReceiver;

enum EInChanSel {CS_LEFT_CHAN, CS_RIGHT_CHAN, CS_MIX_CHAN, CS_SUB_CHAN, CS_IQ_POS,
                 CS_IQ_NEG, CS_IQ_POS_ZERO, CS_IQ_NEG_ZERO, CS_IQ_POS_SPLIT, CS_IQ_NEG_SPLIT
                };
enum EOutChanSel {CS_BOTH_BOTH, CS_LEFT_LEFT, CS_RIGHT_RIGHT,
                  CS_LEFT_MIX, CS_RIGHT_MIX
                 };

/* Maximum frequency for audio spectrum */
#define MAX_SPEC_AUDIO_FREQUENCY	20000 /* Hz */


/* CS: Coding Scheme */
enum ECodScheme { CS_1_SM, CS_2_SM, CS_3_SM, CS_3_HMSYM, CS_3_HMMIX };

/* CT: Channel Type */
enum EChanType { CT_MSC, CT_SDC, CT_FAC };

enum ETypeTiSyncTrac {TSENERGY, TSFIRSTPEAK};

enum ETypeIntFreq { FLINEAR, FDFTFILTER, FWIENER };
enum ETypeIntTime { TLINEAR, TWIENER };
enum ETypeSNREst { SNR_FAC, SNR_PIL };
enum ETypeRxStatus { NOT_PRESENT, CRC_ERROR, DATA_ERROR, RX_OK };
/* RM: Receiver mode (analog or digital demodulation) */

enum ERecMode { RM_DRM, RM_AM, RM_FM, RM_NONE };

/* Acquisition state of receiver */
enum EAcqStat {AS_NO_SIGNAL, AS_WITH_SIGNAL};

/* Receiver state */
enum ERecState {RS_TRACKING, RS_ACQUISITION};

enum EAmAgcType {AT_NO_AGC, AT_SLOW, AT_MEDIUM, AT_FAST};

/* AM enumerations */
enum EDemodType {DT_AM, DT_LSB, DT_USB, DT_CW, DT_FM};
enum ENoiRedType {NR_OFF, NR_LOW, NR_MEDIUM, NR_HIGH, NR_SPEEX};

/* AMSS status */
enum EAMSSBlockLockStat { NO_SYNC, RE_SYNC, DEF_SYNC, DEF_SYNC_BUT_DATA_CHANGED, POSSIBLE_LOSS_OF_SYNC };

/* Classes ********************************************************************/

#include "SDC/audioparam.h"

class CDataParam
{
public:

    /* PM: Packet Mode */
    enum EPackMod { PM_SYNCHRON_STR_MODE, PM_PACKET_MODE };

    /* DU: Data Unit */
    enum EDatUnit { DU_SINGLE_PACKETS, DU_DATA_UNITS };

    /* AD: Application Domain */
    enum EApplDomain { AD_DRM_SPEC_APP, AD_DAB_SPEC_APP, AD_OTHER_SPEC_APP };

    int iStreamID;			/* Stream Id of the stream which carries the data service */

    EPackMod ePacketModInd;	/* Packet mode indicator */

    /* In case of packet mode ------------------------------------------- */
    EDatUnit eDataUnitInd;	/* Data unit indicator */
    int iPacketID;			/* Packet Id (2 bits) */
    int iPacketLen;			/* Packet length */

    // "DAB specified application" not yet implemented!!!
    EApplDomain eAppDomain;	/* Application domain */
    int iUserAppIdent;		/* User application identifier, only DAB */

    CDataParam():
            iStreamID(STREAM_ID_NOT_USED),
            ePacketModInd(PM_PACKET_MODE),
            eDataUnitInd(DU_DATA_UNITS),
            iPacketID(0),
            iPacketLen(0),
            eAppDomain(AD_DAB_SPEC_APP),
            iUserAppIdent(0)
    {
    }
    CDataParam(const CDataParam& DataParam):
            iStreamID(DataParam.iStreamID),
            ePacketModInd(DataParam.ePacketModInd),
            eDataUnitInd(DataParam.eDataUnitInd),
            iPacketID(DataParam.iPacketID),
            iPacketLen(DataParam.iPacketLen),
            eAppDomain(DataParam.eAppDomain),
            iUserAppIdent(DataParam.iUserAppIdent)
    {
    }
    CDataParam& operator=(const CDataParam& DataParam)
    {
        iStreamID = DataParam.iStreamID;
        ePacketModInd = DataParam.ePacketModInd;
        eDataUnitInd = DataParam.eDataUnitInd;
        iPacketID = DataParam.iPacketID;
        iPacketLen = DataParam.iPacketLen;
        eAppDomain = DataParam.eAppDomain;
        iUserAppIdent = DataParam.iUserAppIdent;
        return *this;
    }

    /* This function is needed to detect changes in the data service */
    bool operator!=(const CDataParam DataParam)
    {
        if (iStreamID != DataParam.iStreamID)
            return true;
        if (ePacketModInd != DataParam.ePacketModInd)
            return true;
        if (DataParam.ePacketModInd == PM_PACKET_MODE)
        {
            if (eDataUnitInd != DataParam.eDataUnitInd)
                return true;
            if (iPacketID != DataParam.iPacketID)
                return true;
            if (iPacketLen != DataParam.iPacketLen)
                return true;
            if (eAppDomain != DataParam.eAppDomain)
                return true;
            if (DataParam.eAppDomain == AD_DAB_SPEC_APP)
                if (iUserAppIdent != DataParam.iUserAppIdent)
                    return true;
        }
        return false;
    }
};

class CService
{
public:

    /* CA: CA system */
    enum ECACond { CA_USED, CA_NOT_USED };

    /* SF: Service Flag */
    enum ETyOServ { SF_AUDIO, SF_DATA };

    CService():
            iServiceID(SERV_ID_NOT_USED), eCAIndication(CA_NOT_USED),
            iLanguage(0), eAudDataFlag(SF_AUDIO), iServiceDescr(0),
            strCountryCode(), strLanguageCode(), strLabel(),
            AudioParam(), DataParam()
    {
    }
    CService(const CService& s):
            iServiceID(s.iServiceID), eCAIndication(s.eCAIndication),
            iLanguage(s.iLanguage), eAudDataFlag(s.eAudDataFlag),
            iServiceDescr(s.iServiceDescr), strCountryCode(s.strCountryCode),
            strLanguageCode(s.strLanguageCode), strLabel(s.strLabel),
            AudioParam(s.AudioParam), DataParam(s.DataParam)
    {
    }
    CService& operator=(const CService& s)
    {
        iServiceID = s.iServiceID;
        eCAIndication = s.eCAIndication;
        iLanguage = s.iLanguage;
        eAudDataFlag = s.eAudDataFlag;
        iServiceDescr = s.iServiceDescr;
        strCountryCode = s.strCountryCode;
        strLanguageCode = s.strLanguageCode;
        strLabel = s.strLabel;
        AudioParam = s.AudioParam;
        DataParam = s.DataParam;
        return *this;
    }

    bool IsActive() const
    {
        return iServiceID != SERV_ID_NOT_USED;
    }

    uint32_t iServiceID;
    ECACond eCAIndication;
    int iLanguage;
    ETyOServ eAudDataFlag;
    int iServiceDescr;
    std::string strCountryCode;
    std::string strLanguageCode;

    /* Label of the service */
    std::string strLabel;

    /* Audio parameters */
    CAudioParam AudioParam;

    /* Data parameters */
    CDataParam DataParam;
};

class CStream
{
public:

    CStream():iLenPartA(0), iLenPartB(0)
    {
    }
    CStream(const CStream& s):iLenPartA(s.iLenPartA), iLenPartB(s.iLenPartB)
    {
    }
    CStream& operator=(const CStream& Stream)
    {
        iLenPartA=Stream.iLenPartA;
        iLenPartB=Stream.iLenPartB;
        return *this;
    }

    bool operator==(const CStream Stream)
    {
        if (iLenPartA != Stream.iLenPartA)
            return false;
        if (iLenPartB != Stream.iLenPartB)
            return false;
        return true;
    }

    int iLenPartA;			/* Data length for part A */
    int iLenPartB;			/* Data length for part B */
};

class CMSCProtLev
{
public:

    CMSCProtLev():iPartA(0),iPartB(0),iHierarch(0) {}
    CMSCProtLev(const CMSCProtLev& p):iPartA(p.iPartA),iPartB(p.iPartB),iHierarch(p.iHierarch) {}
    CMSCProtLev& operator=(const CMSCProtLev& NewMSCProtLev)
    {
        iPartA = NewMSCProtLev.iPartA;
        iPartB = NewMSCProtLev.iPartB;
        iHierarch = NewMSCProtLev.iHierarch;
        return *this;
    }

    int iPartA;				/* MSC protection level for part A */
    int iPartB;				/* MSC protection level for part B */
    int iHierarch;			/* MSC protection level for hierachical frame */
};

/* Alternative Frequency Signalling ************************************** */
/* Alternative frequency signalling Schedules informations class */
class CAltFreqSched
{
public:
    CAltFreqSched():iDayCode(0),iStartTime(0),iDuration(0)
    {
    }
    CAltFreqSched(const CAltFreqSched& nAFS):
            iDayCode(nAFS.iDayCode), iStartTime(nAFS.iStartTime),
            iDuration(nAFS.iDuration)
    {
    }

    CAltFreqSched& operator=(const CAltFreqSched& nAFS)
    {
        iDayCode = nAFS.iDayCode;
        iStartTime = nAFS.iStartTime;
        iDuration = nAFS.iDuration;

        return *this;
    }

    bool operator==(const CAltFreqSched& nAFS)
    {
        if (iDayCode != nAFS.iDayCode)
            return false;
        if (iStartTime != nAFS.iStartTime)
            return false;
        if (iDuration != nAFS.iDuration)
            return false;

        return true;
    }

    bool IsActive(const time_t ltime);

    int iDayCode;
    int iStartTime;
    int iDuration;
};

/* Alternative frequency signalling Regions informations class */
class CAltFreqRegion
{
public:
    CAltFreqRegion():veciCIRAFZones(),
            iLatitude(0), iLongitude(0),
            iLatitudeEx(0), iLongitudeEx(0)
    {
    }
    CAltFreqRegion(const CAltFreqRegion& nAFR):
            veciCIRAFZones(nAFR.veciCIRAFZones),
            iLatitude(nAFR.iLatitude),
            iLongitude(nAFR.iLongitude),
            iLatitudeEx(nAFR.iLatitudeEx), iLongitudeEx(nAFR.iLongitudeEx)
    {
    }

    CAltFreqRegion& operator=(const CAltFreqRegion& nAFR)
    {
        iLatitude = nAFR.iLatitude;
        iLongitude = nAFR.iLongitude;
        iLatitudeEx = nAFR.iLatitudeEx;
        iLongitudeEx = nAFR.iLongitudeEx;

        veciCIRAFZones = nAFR.veciCIRAFZones;

        return *this;
    }

    bool operator==(const CAltFreqRegion& nAFR)
    {
        if (iLatitude != nAFR.iLatitude)
            return false;
        if (iLongitude != nAFR.iLongitude)
            return false;
        if (iLatitudeEx != nAFR.iLatitudeEx)
            return false;
        if (iLongitudeEx != nAFR.iLongitudeEx)
            return false;

        /* Vector sizes */
        if (veciCIRAFZones.size() != nAFR.veciCIRAFZones.size())
            return false;

        /* Vector contents */
        for (size_t i = 0; i < veciCIRAFZones.size(); i++)
            if (veciCIRAFZones[i] != nAFR.veciCIRAFZones[i])
                return false;

        return true;
    }

    std::vector<int> veciCIRAFZones;
    int iLatitude;
    int iLongitude;
    int iLatitudeEx;
    int iLongitudeEx;
};

class CServiceDefinition
{
public:
    CServiceDefinition():veciFrequencies(), iRegionID(0), iScheduleID(0),iSystemID(0)
    {
    }

    CServiceDefinition(const CServiceDefinition& nAF):
            veciFrequencies(nAF.veciFrequencies),
            iRegionID(nAF.iRegionID), iScheduleID(nAF.iScheduleID),
            iSystemID(nAF.iSystemID)
    {
    }

    CServiceDefinition& operator=(const CServiceDefinition& nAF)
    {
        veciFrequencies = nAF.veciFrequencies;
        iRegionID = nAF.iRegionID;
        iScheduleID = nAF.iScheduleID;
        iSystemID = nAF.iSystemID;
        return *this;
    }

    bool operator==(const CServiceDefinition& sd) const
    {
        size_t i;

        /* Vector sizes */
        if (veciFrequencies.size() != sd.veciFrequencies.size())
            return false;

        /* Vector contents */
        for (i = 0; i < veciFrequencies.size(); i++)
            if (veciFrequencies[i] != sd.veciFrequencies[i])
                return false;

        if (iRegionID != sd.iRegionID)
            return false;

        if (iScheduleID != sd.iScheduleID)
            return false;

        if (iSystemID != sd.iSystemID)
            return false;

        return true;
    }
    bool operator!=(const CServiceDefinition& sd) const {
        return !(*this==sd);
    }

    std::string Frequency(size_t) const;
    std::string FrequencyUnits() const;
    std::string System() const;

    std::vector<int> veciFrequencies;
    int iRegionID;
    int iScheduleID;
    int iSystemID;
};

class CMultiplexDefinition: public CServiceDefinition
{
public:
    CMultiplexDefinition():CServiceDefinition(), veciServRestrict(4), bIsSyncMultplx(false)
    {
    }

    CMultiplexDefinition(const CMultiplexDefinition& nAF):CServiceDefinition(nAF),
            veciServRestrict(nAF.veciServRestrict),
            bIsSyncMultplx(nAF.bIsSyncMultplx)
    {
    }

    CMultiplexDefinition& operator=(const CMultiplexDefinition& nAF)
    {
        CServiceDefinition(*this) = nAF;
        veciServRestrict = nAF.veciServRestrict;
        bIsSyncMultplx = nAF.bIsSyncMultplx;
        return *this;
    }

    bool operator==(const CMultiplexDefinition& md) const
    {
        if (CServiceDefinition(*this) != md)
            return false;

        /* Vector sizes */
        if (veciServRestrict.size() != md.veciServRestrict.size())
            return false;

        /* Vector contents */
        for (size_t i = 0; i < veciServRestrict.size(); i++)
            if (veciServRestrict[i] != md.veciServRestrict[i])
                return false;

        if (bIsSyncMultplx != md.bIsSyncMultplx)
            return false;

        return true;
    }

    std::vector<int> veciServRestrict;
    bool bIsSyncMultplx;
};

class COtherService: public CServiceDefinition
{
public:
    COtherService(): CServiceDefinition(), bSameService(true),
            iShortID(0), iServiceID(SERV_ID_NOT_USED)
    {
    }

    COtherService(const COtherService& nAF):
            CServiceDefinition(nAF), bSameService(nAF.bSameService),
            iShortID(nAF.iShortID), iServiceID(nAF.iServiceID)
    {
    }

    COtherService& operator=(const COtherService& nAF)
    {
        CServiceDefinition(*this) = nAF;

        bSameService = nAF.bSameService;
        iShortID = nAF.iShortID;
        iServiceID = nAF.iServiceID;

        return *this;
    }

    bool operator==(const COtherService& nAF)
    {
        if (CServiceDefinition(*this) != nAF)
            return false;

        if (bSameService != nAF.bSameService)
            return false;

        if (iShortID != nAF.iShortID)
            return false;

        if (iServiceID != nAF.iServiceID)
            return false;

        return true;
    }

    std::string ServiceID() const;

    bool bSameService;
    int iShortID;
    uint32_t iServiceID;
};

/* Alternative frequency signalling class */
class CAltFreqSign
{
public:

    CAltFreqSign():vecRegions(16),vecSchedules(16),vecMultiplexes(),vecOtherServices(),
            bRegionVersionFlag(false),bScheduleVersionFlag(false),
            bMultiplexVersionFlag(false),bOtherServicesVersionFlag(false)
    {
    }

    CAltFreqSign(const CAltFreqSign& a):vecRegions(a.vecRegions),
            vecSchedules(a.vecSchedules), vecMultiplexes(a.vecMultiplexes),
            bRegionVersionFlag(a.bRegionVersionFlag),
            bScheduleVersionFlag(a.bScheduleVersionFlag),
            bMultiplexVersionFlag(a.bMultiplexVersionFlag),
            bOtherServicesVersionFlag(a.bOtherServicesVersionFlag)
    {
    }

    CAltFreqSign& operator=(const CAltFreqSign& a)
    {
        vecRegions = a.vecRegions;
        vecSchedules = a.vecSchedules;
        vecMultiplexes = a.vecMultiplexes;
        bRegionVersionFlag = a.bRegionVersionFlag;
        bScheduleVersionFlag = a.bScheduleVersionFlag;
        bMultiplexVersionFlag = a.bMultiplexVersionFlag;
        bOtherServicesVersionFlag = a.bOtherServicesVersionFlag;
        return *this;
    }

    void ResetRegions(bool b)
    {
        vecRegions.clear();
        vecRegions.resize(16);
        bRegionVersionFlag=b;
    }

    void ResetSchedules(bool b)
    {
        vecSchedules.clear();
        vecSchedules.resize(16);
        bScheduleVersionFlag=b;
    }

    void ResetMultiplexes(bool b)
    {
        vecMultiplexes.clear();
        bMultiplexVersionFlag=b;
    }

    void ResetOtherServices(bool b)
    {
        vecOtherServices.clear();
        bOtherServicesVersionFlag=b;
    }

    void Reset()
    {
        ResetRegions(false);
        ResetSchedules(false);
        ResetMultiplexes(false);
        ResetOtherServices(false);
    }

    std::vector < std::vector<CAltFreqRegion> > vecRegions; // outer std::vector indexed by regionID
    std::vector < std::vector<CAltFreqSched> > vecSchedules; // outer std::vector indexed by scheduleID
    std::vector < CMultiplexDefinition > vecMultiplexes;
    std::vector < COtherService > vecOtherServices;
    bool bRegionVersionFlag;
    bool bScheduleVersionFlag;
    bool bMultiplexVersionFlag;
    bool bOtherServicesVersionFlag;
};

/* Class to store information about the last service selected ------------- */

class CLastService
{
public:
    CLastService():iService(0), iServiceID(SERV_ID_NOT_USED)
    {
    }
    CLastService(const CLastService& l):iService(l.iService), iServiceID(l.iServiceID)
    {
    }
    CLastService& operator=(const CLastService& l)
    {
        iService = l.iService;
        iServiceID = l.iServiceID;
        return *this;
    }

    void Reset()
    {
        iService = 0;
        iServiceID = SERV_ID_NOT_USED;
    }

    void Save(const int iCurSel, const int iCurServiceID)
    {
        if (iCurServiceID != SERV_ID_NOT_USED)
        {
            iService = iCurSel;
            iServiceID = iCurServiceID;
        }
    }

    /* store only fac parameters */
    int iService;
    uint32_t iServiceID;
};

/* Classes to keep track of status flags for RSCI rsta tag and log file */
class CRxStatus
{
public:
    CRxStatus():status(NOT_PRESENT),iNum(0),iNumOK(0) {}
    CRxStatus(const CRxStatus& s):status(s.status),iNum(s.iNum),iNumOK(s.iNumOK) {}
    CRxStatus& operator=(const CRxStatus& s)
    {
        status = s.status;
        iNum = s.iNum;
        iNumOK = s.iNumOK;
        return *this;
    }
    void SetStatus(const ETypeRxStatus);
    ETypeRxStatus GetStatus() {
        return status;
    }
    int GetCount() {
        return iNum;
    }
    int GetOKCount() {
        return iNumOK;
    }
    void ResetCounts() {
        iNum=0;
        iNumOK = 0;
    }
private:
    ETypeRxStatus status;
    int iNum, iNumOK;
};

class CReceiveStatus
{
public:
    CReceiveStatus():FSync(),TSync(),InterfaceI(),InterfaceO(),
            FAC(),SDC(),SLAudio(),LLAudio()
    {
    }
    CReceiveStatus(const CReceiveStatus& s):FSync(s.FSync), TSync(s.TSync),
            InterfaceI(s.InterfaceI), InterfaceO(s.InterfaceO), FAC(s.FAC), SDC(s.SDC),
            SLAudio(s.SLAudio),LLAudio(s.LLAudio)
    {
    }
    CReceiveStatus& operator=(const CReceiveStatus& s)
    {
        FSync = s.FSync;
        TSync = s.TSync;
        InterfaceI = s.InterfaceI;
        InterfaceO = s.InterfaceO;
        FAC = s.FAC;
        SDC = s.SDC;
        SLAudio = s.SLAudio;
        LLAudio = s.LLAudio;
        return *this;
    }

    CRxStatus FSync;
    CRxStatus TSync;
    CRxStatus InterfaceI;
    CRxStatus InterfaceO;
    CRxStatus FAC;
    CRxStatus SDC;
    CRxStatus SLAudio;
    CRxStatus LLAudio;
};


/* Simulation raw-data management. We have to implement a shift register
   with varying size. We do that by adding a variable for storing the
   current write position. */
class CRawSimData
{
    /* We have to implement a shift register with varying size. We do that
       by adding a variable for storing the current write position. We use
       always the first value of the array for reading and do a shift of the
       other data by adding a arbitrary value (0) at the end of the whole
       shift register */
public:
    /* Here, the maximal size of the shift register is set */
    CRawSimData():ciMaxDelBlocks(50), iCurWritePos(0)
    {
        veciShRegSt.Init(ciMaxDelBlocks);
    }

    void Add(uint32_t iNewSRS);
    uint32_t Get();

    void Reset()
    {
        iCurWritePos = 0;
    }

protected:
    /* Max number of delayed blocks */
    int ciMaxDelBlocks;
    CShiftRegister < uint32_t > veciShRegSt;
    int iCurWritePos;
};

class CFrontEndParameters
{
public:
    enum ESMeterCorrectionType {S_METER_CORRECTION_TYPE_CAL_FACTOR_ONLY, S_METER_CORRECTION_TYPE_AGC_ONLY, S_METER_CORRECTION_TYPE_AGC_RSSI};

    // Constructor
    CFrontEndParameters():
            eSMeterCorrectionType(S_METER_CORRECTION_TYPE_CAL_FACTOR_ONLY), rSMeterBandwidth(10000.0),
            rDefaultMeasurementBandwidth(10000.0), bAutoMeasurementBandwidth(true), rCalFactorAM(0.0),
            rCalFactorDRM(0.0), rIFCentreFreq(12000.0)
    {}
    CFrontEndParameters(const CFrontEndParameters& p):
            eSMeterCorrectionType(p.eSMeterCorrectionType), rSMeterBandwidth(p.rSMeterBandwidth),
            rDefaultMeasurementBandwidth(p.rDefaultMeasurementBandwidth),
            bAutoMeasurementBandwidth(p.bAutoMeasurementBandwidth),
            rCalFactorAM(p.rCalFactorAM), rCalFactorDRM(p.rCalFactorDRM),
            rIFCentreFreq(p.rIFCentreFreq)
    {}
    CFrontEndParameters& operator=(const CFrontEndParameters& p)
    {
        eSMeterCorrectionType = p.eSMeterCorrectionType;
        rSMeterBandwidth = p.rSMeterBandwidth;
        rDefaultMeasurementBandwidth = p.rDefaultMeasurementBandwidth;
        bAutoMeasurementBandwidth = p.bAutoMeasurementBandwidth;
        rCalFactorAM = p.rCalFactorAM;
        rCalFactorDRM = p.rCalFactorDRM;
        rIFCentreFreq = p.rIFCentreFreq;
        return *this;
    }

    ESMeterCorrectionType eSMeterCorrectionType;
    _REAL rSMeterBandwidth; // The bandwidth the S-meter uses to do the measurement

    _REAL rDefaultMeasurementBandwidth; // Bandwidth to do measurement if not synchronised
    bool bAutoMeasurementBandwidth; // true: use the current FAC bandwidth if locked, false: use default bandwidth always
    _REAL rCalFactorAM;
    _REAL rCalFactorDRM;
    _REAL rIFCentreFreq;

};


class CMinMaxMean
{
public:
    CMinMaxMean();

    void addSample(_REAL);
    _REAL getCurrent();
    _REAL getMean();
    void getMinMax(_REAL&, _REAL&);
protected:
    _REAL rSum, rCur, rMin, rMax;
    int iNum;
};

class CParameter
{
public:
    CParameter();
    CParameter(const CParameter&);
    virtual ~CParameter();
    CParameter& operator=(const CParameter&);

    /* Enumerations --------------------------------------------------------- */
    /* AS: AFS in SDC is valid or not */
    enum EAFSVali { AS_VALID, AS_NOT_VALID };

    /* SI: Symbol Interleaver */
    enum ESymIntMod { SI_LONG, SI_SHORT };

    /* CT: Current Time */
    enum ECurTime { CT_OFF, CT_LOCAL, CT_UTC, CT_UTC_OFFSET };

    /* ST: Simulation Type */
    enum ESimType
    { ST_NONE, ST_BITERROR, ST_MSECHANEST, ST_BER_IDEALCHAN,
      ST_SYNC_PARAM, ST_SINR
    };

    /* Misc. Functions ------------------------------------------------------ */
    void SetReceiver(CDRMReceiver *pDRMReceiver);
    void GenerateRandomSerialNumber();
    void GenerateReceiverID();
    void ResetServicesStreams();
    void GetActiveServices(set<int>& actServ);
    void GetActiveStreams(set<int>& actStr);
    void InitCellMapTable(const ERobMode eNewWaveMode,
                          const ESpecOcc eNewSpecOcc);

    void SetNumDecodedBitsMSC(const int iNewNumDecodedBitsMSC);
    void SetNumDecodedBitsSDC(const int iNewNumDecodedBitsSDC);
    void SetNumBitsHieraFrTot(const int iNewNumBitsHieraFrTot);
    void SetNumAudioDecoderBits(const int iNewNumAudioDecoderBits);
    void SetNumDataDecoderBits(const int iNewNumDataDecoderBits);

    bool SetWaveMode(const ERobMode eNewWaveMode);
    ERobMode GetWaveMode() const {
        return eRobustnessMode;
    }

    void SetFrequency(int iNewFrequency) {
        iFrequency = iNewFrequency;
    }
    int GetFrequency() {
        return iFrequency;
    }

    void SetServiceParameters(int iShortID, const CService& newService);

    void SetCurSelAudioService(const int iNewService);
    int GetCurSelAudioService() const {
        return iCurSelAudioService;
    }
    void SetCurSelDataService(const int iNewService);
    int GetCurSelDataService() const {
        return iCurSelDataService;
    }

    void ResetCurSelAudDatServ()
    {
        iCurSelAudioService = 0;
        iCurSelDataService = 0;
    }

    /*
       Sample rate related getters/setters
    */
    int GetAudSampleRate() const
    {
        return iAudSampleRate;
    }
    int GetSigSampleRate() const
    {
        return iSigSampleRate;
    }
    int GetSigUpscaleRatio() const
    {
        return iSigUpscaleRatio;
    }
    int GetSoundCardSigSampleRate() const
    {
        return iSigSampleRate / iSigUpscaleRatio;
    }
    void SetSoundCardSigSampleRate(int sr)
    {
        iSigSampleRate = sr * iSigUpscaleRatio;
    }
    void SetNewAudSampleRate(int sr)
    {
        /* Perform range check */
        if      (sr < 8000)   sr = 8000;
        else if (sr > 192000) sr = 192000;
        /* Audio sample rate must be a multiple of 25,
           set to the nearest multiple of 25 */
        // TODO AM Demod still have issue with some sample rate
        // The buffering system is not enough flexible
        sr = (sr + 12) / 25 * 25; // <- ok for DRM mode
        iNewAudSampleRate = sr;
    }
    void SetNewSigSampleRate(int sr)
    {
        /* Set to the nearest supported sample rate */
        if      (sr < 36000)  sr = 24000;
        else if (sr < 72000)  sr = 48000;
        else if (sr < 144000) sr = 96000;
        else                  sr = 192000;
        iNewSigSampleRate = sr;
    }
    void SetNewSigUpscaleRatio(int ratio)
    {
        iNewSigUpscaleRatio = ratio < 2 ? 1 : 2;
    }
    /* New sample rate are fetched at init and restart */
    void FetchNewSampleRate()
    {
        if (iNewSigUpscaleRatio != 0)
        {
            iSigUpscaleRatio = iNewSigUpscaleRatio;
            iNewSigUpscaleRatio = 0;
        }
        if (iNewAudSampleRate != 0)
        {
            iAudSampleRate = iNewAudSampleRate;
            iNewAudSampleRate = 0;
        }
        if (iNewSigSampleRate != 0)
        {
            iSigSampleRate = iNewSigSampleRate * iSigUpscaleRatio;
            iNewSigSampleRate = 0;
        }
    }

    _REAL GetDCFrequency() const
    {
        return iSigSampleRate * (rFreqOffsetAcqui + rFreqOffsetTrack);
    }

    _REAL GetBitRateKbps(const int iShortID, const bool bAudData) const;
    _REAL PartABLenRatio(const int iShortID) const;

    /* Parameters controlled by FAC ----------------------------------------- */
    void SetInterleaverDepth(const ESymIntMod eNewDepth);
    ESymIntMod GetInterleaverDepth() const
    {
        return eSymbolInterlMode;
    }

    void SetMSCCodingScheme(const ECodScheme eNewScheme);
    void SetSDCCodingScheme(const ECodScheme eNewScheme);

    void SetSpectrumOccup(ESpecOcc eNewSpecOcc);
    ESpecOcc GetSpectrumOccup() const
    {
        return eSpectOccup;
    }

    void SetNumOfServices(const size_t iNNumAuSe, const size_t iNNumDaSe);
    size_t GetTotNumServices() const
    {
        return iNumAudioService + iNumDataService;
    }

    void SetAudDataFlag(const int iShortID, const CService::ETyOServ iNewADaFl);
    void SetServiceID(const int iShortID, const uint32_t iNewServiceID);

    CDRMReceiver* pDRMRec;

    /* Symbol interleaver mode (long or short interleaving) */
    ESymIntMod eSymbolInterlMode;

    ECodScheme eMSCCodingScheme;	/* MSC coding scheme */
    ECodScheme eSDCCodingScheme;	/* SDC coding scheme */

    size_t iNumAudioService;
    size_t iNumDataService;

    /* AMSS */
    int iAMSSCarrierMode;

    /* Serial number and received ID */
    std::string sReceiverID;
    std::string sSerialNumber;
protected:
    std::string sDataFilesDirectory;
public:

    std::string GetDataDirectory(const char* pcChildDirectory = nullptr) const;
    void SetDataDirectory(std::string sNewDataFilesDirectory);

    /* Parameters controlled by SDC ----------------------------------------- */
    void SetAudioParam(const int iShortID, const CAudioParam& NewAudParam);
    CAudioParam GetAudioParam(const int iShortID) const;
    CDataParam GetDataParam(const int iShortID) const;
    void SetDataParam(const int iShortID, const CDataParam& NewDataParam);

    void SetMSCProtLev(const CMSCProtLev NewMSCPrLe, const bool bWithHierarch);
    void SetStreamLen(const int iStreamID, const int iNewLenPartA, const int iNewLenPartB);
    void GetStreamLen(const int iStreamID, int& iLenPartA, int& iLenPartB) const;
    int GetStreamLen(const int iStreamID) const;
    ECurTime eTransmitCurrentTime;

    /* Protection levels for MSC */
    CMSCProtLev MSCPrLe;

    std::vector<CStream> Stream;
    std::vector<CService> Service;
    std::vector<CRxStatus> AudioComponentStatus;
    std::vector<CRxStatus> DataComponentStatus;

    /* information about services gathered from SDC, EPG and web schedules */
    map<uint32_t,CServiceInformation> ServiceInformation;

    /* These values are used to set input and output block sizes of some modules */
    int iNumBitsHierarchFrameTotal;
    int iNumDecodedBitsMSC;
    int iNumSDCBitsPerSFrame;	/* Number of SDC bits per super-frame */
    int iNumAudioDecoderBits;	/* Number of input bits for audio module */
    int iNumDataDecoderBits;	/* Number of input bits for data decoder module */

    /* Date */
    int iYear;
    int iMonth;
    int iDay;

    /* UTC (hours and minutes) */
    int iUTCHour;
    int iUTCMin;
    int iUTCOff;
    int iUTCSense;
    bool bValidUTCOffsetAndSense;

    /* Identifies the current frame. This parameter is set by FAC */
    int iFrameIDTransm;
    int iFrameIDReceiv;

    /* Synchronization ------------------------------------------------------ */
    _REAL rFreqOffsetAcqui;
    _REAL rFreqOffsetTrack;

    _REAL rResampleOffset;

    int iTimingOffsTrack;

    ERecMode GetReceiverMode() {
        return eReceiverMode;
    }
    ERecMode eReceiverMode;
    EAcqStat GetAcquiState() {
        return eAcquiState;
    }
    EAcqStat eAcquiState;
    int iNumAudioFrames;

    CVector <_BINARY> vecbiAudioFrameStatus;
    bool bMeasurePSD, bMeasurePSDAlways;
    _REAL rPIRStart;
    _REAL rPIREnd;

    /* std::vector to hold the PSD values for the rpsd tag. */
    CVector <_REAL> vecrPSD;

    // std::vector to hold impulse response values for (proposed) rpir tag
    CVector <_REAL> vecrPIR;

    CMatrix <_COMPLEX> matcReceivedPilotValues;

    /* Simulation ----------------------------------------------------------- */
    CRawSimData RawSimDa;
    ESimType eSimType;

    int iDRMChannelNum;
    int iSpecChDoppler;
    _REAL rBitErrRate;
    _REAL rSyncTestParam;		/* For any other simulations, used
								   with "ST_SYNC_PARAM" type */
    _REAL rSINR;
    int iNumBitErrors;
    int iChanEstDelay;

    int iNumTaps;
    std::vector<int> iPathDelay;
    _REAL rGainCorr;
    int iOffUsfExtr;

    void SetSNR(const _REAL);
    _REAL GetSNR();
    void SetNominalSNRdB(const _REAL rSNRdBNominal);
    _REAL GetNominalSNRdB();
    void SetSystemSNRdB(const _REAL rSNRdBSystem)
    {
        rSysSimSNRdB = rSNRdBSystem;
    }
    _REAL GetSystemSNRdB() const
    {
        return rSysSimSNRdB;
    }
    _REAL GetSysSNRdBPilPos() const;

    CReceiveStatus ReceiveStatus;
    CFrontEndParameters FrontEndParameters;
    CAltFreqSign AltFreqSign;

    void Lock()
    {
        Mutex.Lock();
    }
    void Unlock()
    {
        Mutex.Unlock();
    }
    /* Channel Estimation */
    _REAL rMER;
    _REAL rWMERMSC;
    _REAL rWMERFAC;
    _REAL rSigmaEstimate;
    _REAL rMinDelay;
    _REAL rMaxDelay;

    bool bMeasureDelay;
    CRealVector vecrRdel;
    CRealVector vecrRdelThresholds;
    CRealVector vecrRdelIntervals;
    bool bMeasureDoppler;
    _REAL rRdop;
    /* interference (constellation-based measurement rnic)*/
    bool bMeasureInterference;
    _REAL rIntFreq, rINR, rICR;

    /* peak of PSD - for PSD-based interference measurement rnip */
    _REAL rMaxPSDwrtSig;
    _REAL rMaxPSDFreq;

    /* the signal level as measured at IF by dream */
    void SetIFSignalLevel(_REAL);
    _REAL GetIFSignalLevel();
    _REAL rSigStrengthCorrection;

    /* General -------------------------------------------------------------- */
    _REAL GetNominalBandwidth();
    _REAL GetSysToNomBWCorrFact();

    CCellMappingTable CellMappingTable;

    CMinMaxMean SNRstat, SigStrstat;

    std::string audioencoder, audiodecoder;
    gps_data_t gps_data;
    bool use_gpsd;
    bool restart_gpsd;
    std::string gps_host; std::string gps_port;

protected:

    int iAudSampleRate;
    int iSigSampleRate;
    int iSigUpscaleRatio;
    int iNewAudSampleRate;
    int iNewSigSampleRate;
    int iNewSigUpscaleRatio;

    _REAL rSysSimSNRdB;

    int iFrequency;
    bool bValidSignalStrength;
    _REAL rSigStr;
    _REAL rIFSigStr;

    /* Current selected audio service for processing */
    int iCurSelAudioService;
    int iCurSelDataService;

    ERobMode eRobustnessMode;	/* E.g.: Mode A, B, C or D */
    ESpecOcc eSpectOccup;

    /* For resync to last service------------------------------------------- */
    CLastService LastAudioService;
    CLastService LastDataService;

    CMutex Mutex;
public:
    bool lenient_RSCI;
};

#endif // !defined(PARAMETER_H__3B0BA660_CA63_4344_BB2B_23E7A0D31912__INCLUDED_)

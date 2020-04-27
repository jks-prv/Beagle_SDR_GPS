/******************************************************************************\
 * Technische Universitaet Darmstadt, Institut fuer Nachrichtentechnik
 * Copyright (c) 2007
 *
 * Author(s):
 *	Volker Fischer, Andrew Murphy
 *
 * Description:
 *	DRM Parameters
 *
 ******************************************************************************
 *
 * This program is free software(), you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation(), either version 2 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY(), without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program(), if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
\******************************************************************************/

#include "Parameter.h"
#include "DRMReceiver.h"
#include "Version.h"
#include <limits>
#include <sstream>
#include <iomanip>
//#include "util/LogPrint.h"

#ifdef _WIN32
# define PATH_SEPARATOR "\\"
# define PATH_SEPARATORS "/\\"
#else
# define PATH_SEPARATOR "/"
# define PATH_SEPARATORS "/"
#endif
#define DEFAULT_DATA_FILES_DIRECTORY "data" PATH_SEPARATOR

/* Implementation *************************************************************/
CParameter::CParameter():
    pDRMRec(nullptr),
    eSymbolInterlMode(),
    eMSCCodingScheme(),
    eSDCCodingScheme(),
    iNumAudioService(0),
    iNumDataService(0),
    iAMSSCarrierMode(0),
    sReceiverID("                "),
    sSerialNumber(),
    sDataFilesDirectory(DEFAULT_DATA_FILES_DIRECTORY),
    eTransmitCurrentTime(CT_OFF),
    MSCPrLe(),
    Stream(MAX_NUM_STREAMS), Service(MAX_NUM_SERVICES),
	AudioComponentStatus(MAX_NUM_SERVICES),DataComponentStatus(MAX_NUM_SERVICES),
    iNumBitsHierarchFrameTotal(0),
    iNumDecodedBitsMSC(0),
    iNumSDCBitsPerSFrame(0),
    iNumAudioDecoderBits(0),
    iNumDataDecoderBits(0),
    iYear(0),
    iMonth(0),
    iDay(0),
    iUTCHour(0),
    iUTCMin(0),
    iFrameIDTransm(0),
    iFrameIDReceiv(0),
    rFreqOffsetAcqui(0.0),
    rFreqOffsetTrack(0.0),
    rResampleOffset(0.0),
    iTimingOffsTrack(0),
    eReceiverMode(RM_NONE),
    eAcquiState(AS_NO_SIGNAL),
    iNumAudioFrames(0),
    vecbiAudioFrameStatus(0),
    bMeasurePSD(false), bMeasurePSDAlways(false),
    vecrPSD(0),
    matcReceivedPilotValues(),
    RawSimDa(),
    eSimType(ST_NONE),
    iDRMChannelNum(0),
    iSpecChDoppler(0),
    rBitErrRate(0.0),
    rSyncTestParam(0.0),
    rSINR(0.0),
    iNumBitErrors(0),
    iChanEstDelay(0),
    iNumTaps(0),
    iPathDelay(MAX_NUM_TAPS_DRM_CHAN),
    rGainCorr(0.0),
    iOffUsfExtr(0),
    ReceiveStatus(),
    FrontEndParameters(),
    AltFreqSign(),
    rMER(0.0),
    rWMERMSC(0.0),
    rWMERFAC(0.0),
    rSigmaEstimate(0.0),
    rMinDelay(0.0),
    rMaxDelay(0.0),
    bMeasureDelay(),
    vecrRdel(0),
    vecrRdelThresholds(0),
    vecrRdelIntervals(0),
    bMeasureDoppler(0),
    rRdop(0.0),
    bMeasureInterference(false),
    rIntFreq(0.0),
    rINR(0.0),
    rICR(0.0),
    rMaxPSDwrtSig(0.0),
    rMaxPSDFreq(0.0),
    rSigStrengthCorrection(0.0),
    CellMappingTable(),
    audioencoder(""),audiodecoder(""),
    use_gpsd(0), restart_gpsd(false),
    gps_host("localhost"), gps_port("2497"),
    iAudSampleRate(DEFAULT_SOUNDCRD_SAMPLE_RATE),
    iSigSampleRate(DEFAULT_SOUNDCRD_SAMPLE_RATE),
    iSigUpscaleRatio(1),
    iNewAudSampleRate(0),
    iNewSigSampleRate(0),
    iNewSigUpscaleRatio(0),
    rSysSimSNRdB(0.0),
    iFrequency(0),
    bValidSignalStrength(false),
    rSigStr(0.0),
    rIFSigStr(0.0),
    iCurSelAudioService(0),
    iCurSelDataService(0),
    eRobustnessMode(RM_ROBUSTNESS_MODE_B),
    eSpectOccup(SO_3),
    LastAudioService(),
    LastDataService(),
    Mutex(), lenient_RSCI(false)
{
    GenerateRandomSerialNumber();
    CellMappingTable.MakeTable(eRobustnessMode, eSpectOccup, iSigSampleRate);
    gps_data.set=0;
    gps_data.status=0;
#ifdef HAVE_LIBGPS
    gps_data.gps_fd = -1;
#endif
}

CParameter::~CParameter()
{
}

CParameter::CParameter(const CParameter& p):
    pDRMRec(p.pDRMRec),
    eSymbolInterlMode(p.eSymbolInterlMode),
    eMSCCodingScheme(p.eMSCCodingScheme),
    eSDCCodingScheme(p.eSDCCodingScheme),
    iNumAudioService(p.iNumAudioService),
    iNumDataService(p.iNumDataService),
    iAMSSCarrierMode(p.iAMSSCarrierMode),
    sReceiverID(p.sReceiverID),
    sSerialNumber(p.sSerialNumber),
    sDataFilesDirectory(p.sDataFilesDirectory),
    eTransmitCurrentTime(p.eTransmitCurrentTime),
    MSCPrLe(p.MSCPrLe),
    Stream(p.Stream), Service(p.Service),
	AudioComponentStatus(p.AudioComponentStatus),DataComponentStatus(p.DataComponentStatus),
	iNumBitsHierarchFrameTotal(p.iNumBitsHierarchFrameTotal),
    iNumDecodedBitsMSC(p.iNumDecodedBitsMSC),
    iNumSDCBitsPerSFrame(p.iNumSDCBitsPerSFrame),
    iNumAudioDecoderBits(p.iNumAudioDecoderBits),
    iNumDataDecoderBits(p.iNumDataDecoderBits),
    iYear(p.iYear),
    iMonth(p.iMonth),
    iDay(p.iDay),
    iUTCHour(p.iUTCHour),
    iUTCMin(p.iUTCMin),
    iUTCOff(p.iUTCOff),
    iUTCSense(p.iUTCSense),
    bValidUTCOffsetAndSense(p.bValidUTCOffsetAndSense),
    iFrameIDTransm(p.iFrameIDTransm),
    iFrameIDReceiv(p.iFrameIDReceiv),
    rFreqOffsetAcqui(p.rFreqOffsetAcqui),
    rFreqOffsetTrack(p.rFreqOffsetTrack),
    rResampleOffset(p.rResampleOffset),
    iTimingOffsTrack(p.iTimingOffsTrack),
    eReceiverMode(p.eReceiverMode),
    eAcquiState(p.eAcquiState),
    iNumAudioFrames(p.iNumAudioFrames),
    vecbiAudioFrameStatus(p.vecbiAudioFrameStatus),
    bMeasurePSD(p.bMeasurePSD), bMeasurePSDAlways(p.bMeasurePSDAlways),
    vecrPSD(p.vecrPSD),
//matcReceivedPilotValues(p.matcReceivedPilotValues),
    matcReceivedPilotValues(), // OPH says copy constructor for CMatrix not safe yet
    RawSimDa(p.RawSimDa),
    eSimType(p.eSimType),
    iDRMChannelNum(p.iDRMChannelNum),
    iSpecChDoppler(p.iSpecChDoppler),
    rBitErrRate(p.rBitErrRate),
    rSyncTestParam	(p.rSyncTestParam),
    rSINR(p.rSINR),
    iNumBitErrors(p.iNumBitErrors),
    iChanEstDelay(p.iChanEstDelay),
    iNumTaps(p.iNumTaps),
    iPathDelay(p.iPathDelay),
    rGainCorr(p.rGainCorr),
    iOffUsfExtr(p.iOffUsfExtr),
    ReceiveStatus(p.ReceiveStatus),
    FrontEndParameters(p.FrontEndParameters),
    AltFreqSign(p.AltFreqSign),
    rMER(p.rMER),
    rWMERMSC(p.rWMERMSC),
    rWMERFAC(p.rWMERFAC),
    rSigmaEstimate(p.rSigmaEstimate),
    rMinDelay(p.rMinDelay),
    rMaxDelay(p.rMaxDelay),
    bMeasureDelay(p.bMeasureDelay),
    vecrRdel(p.vecrRdel),
    vecrRdelThresholds(p.vecrRdelThresholds),
    vecrRdelIntervals(p.vecrRdelIntervals),
    bMeasureDoppler(p.bMeasureDoppler),
    rRdop(p.rRdop),
    bMeasureInterference(p.bMeasureInterference),
    rIntFreq(p.rIntFreq),
    rINR(p.rINR),
    rICR(p.rICR),
    rMaxPSDwrtSig(p.rMaxPSDwrtSig),
    rMaxPSDFreq(p.rMaxPSDFreq),
    rSigStrengthCorrection(p.rSigStrengthCorrection),
    CellMappingTable(), // jfbc CCellMappingTable uses a CMatrix :(
    audioencoder(p.audioencoder),audiodecoder(p.audiodecoder),
    use_gpsd(p.use_gpsd),restart_gpsd(p.restart_gpsd),
    gps_host(p.gps_host),gps_port(p.gps_port),
    iAudSampleRate(p.iAudSampleRate),
    iSigSampleRate(p.iSigSampleRate),
    iSigUpscaleRatio(p.iSigUpscaleRatio),
    iNewAudSampleRate(p.iNewAudSampleRate),
    iNewSigSampleRate(p.iNewSigSampleRate),
    iNewSigUpscaleRatio(p.iNewSigUpscaleRatio),
    rSysSimSNRdB(p.rSysSimSNRdB),
    iFrequency(p.iFrequency),
    bValidSignalStrength(p.bValidSignalStrength),
    rSigStr(p.rSigStr),
    rIFSigStr(p.rIFSigStr),
    iCurSelAudioService(p.iCurSelAudioService),
    iCurSelDataService(p.iCurSelDataService),
    eRobustnessMode(p.eRobustnessMode),
    eSpectOccup(p.eSpectOccup),
    LastAudioService(p.LastAudioService),
    LastDataService(p.LastDataService)
//, Mutex() // jfbc: I don't think this state should be copied
  ,lenient_RSCI(p.lenient_RSCI)
{
    CellMappingTable.MakeTable(eRobustnessMode, eSpectOccup, iSigSampleRate);
    matcReceivedPilotValues = p.matcReceivedPilotValues; // TODO
    gps_data = p.gps_data;
}

CParameter& CParameter::operator=(const CParameter& p)
{
    gps_data = p.gps_data;
    pDRMRec = p.pDRMRec;
    eSymbolInterlMode = p.eSymbolInterlMode;
    eMSCCodingScheme = p.eMSCCodingScheme;
    eSDCCodingScheme = p.eSDCCodingScheme;
    iNumAudioService = p.iNumAudioService;
    iNumDataService = p.iNumDataService;
    iAMSSCarrierMode = p.iAMSSCarrierMode;
    sReceiverID = p.sReceiverID;
    sSerialNumber = p.sSerialNumber;
    sDataFilesDirectory = p.sDataFilesDirectory;
    eTransmitCurrentTime = p.eTransmitCurrentTime;
    MSCPrLe = p.MSCPrLe;
    Stream = p.Stream;
    Service = p.Service;
	AudioComponentStatus = p.AudioComponentStatus;
	DataComponentStatus = p.DataComponentStatus;
    iNumBitsHierarchFrameTotal = p.iNumBitsHierarchFrameTotal;
    iNumDecodedBitsMSC = p.iNumDecodedBitsMSC;
    iNumSDCBitsPerSFrame = p.iNumSDCBitsPerSFrame;
    iNumAudioDecoderBits = p.iNumAudioDecoderBits;
    iNumDataDecoderBits = p.iNumDataDecoderBits;
    iYear = p.iYear;
    iMonth = p.iMonth;
    iDay = p.iDay;
    iUTCHour = p.iUTCHour;
    iUTCMin = p.iUTCMin;
    iFrameIDTransm = p.iFrameIDTransm;
    iFrameIDReceiv = p.iFrameIDReceiv;
    rFreqOffsetAcqui = p.rFreqOffsetAcqui;
    rFreqOffsetTrack = p.rFreqOffsetTrack;
    rResampleOffset = p.rResampleOffset;
    iTimingOffsTrack = p.iTimingOffsTrack;
    eReceiverMode = p.eReceiverMode;
    eAcquiState = p.eAcquiState;
    iNumAudioFrames = p.iNumAudioFrames;
    vecbiAudioFrameStatus = p.vecbiAudioFrameStatus;
    bMeasurePSD = p.bMeasurePSD;
    bMeasurePSDAlways = p.bMeasurePSDAlways;
    vecrPSD = p.vecrPSD;
    matcReceivedPilotValues = p.matcReceivedPilotValues;
    RawSimDa = p.RawSimDa;
    eSimType = p.eSimType;
    iDRMChannelNum = p.iDRMChannelNum;
    iSpecChDoppler = p.iSpecChDoppler;
    rBitErrRate = p.rBitErrRate;
    rSyncTestParam	 = p.rSyncTestParam;
    rSINR = p.rSINR;
    iNumBitErrors = p.iNumBitErrors;
    iChanEstDelay = p.iChanEstDelay;
    iNumTaps = p.iNumTaps;
    iPathDelay = p.iPathDelay;
    rGainCorr = p.rGainCorr;
    iOffUsfExtr = p.iOffUsfExtr;
    ReceiveStatus = p.ReceiveStatus;
    FrontEndParameters = p.FrontEndParameters;
    AltFreqSign = p.AltFreqSign;
    rMER = p.rMER;
    rWMERMSC = p.rWMERMSC;
    rWMERFAC = p.rWMERFAC;
    rSigmaEstimate = p.rSigmaEstimate;
    rMinDelay = p.rMinDelay;
    rMaxDelay = p.rMaxDelay;
    bMeasureDelay = p.bMeasureDelay;
    vecrRdel = p.vecrRdel;
    vecrRdelThresholds = p.vecrRdelThresholds;
    vecrRdelIntervals = p.vecrRdelIntervals;
    bMeasureDoppler = p.bMeasureDoppler;
    rRdop = p.rRdop;
    bMeasureInterference = p.bMeasureInterference;
    rIntFreq = p.rIntFreq;
    rINR = p.rINR;
    rICR = p.rICR;
    rMaxPSDwrtSig = p.rMaxPSDwrtSig;
    rMaxPSDFreq = p.rMaxPSDFreq;
    rSigStrengthCorrection = p.rSigStrengthCorrection;
    CellMappingTable.MakeTable(eRobustnessMode, eSpectOccup, iSigSampleRate); // don't copy CMatrix
    audiodecoder =  p.audiodecoder;
    audioencoder =  p.audioencoder;
    use_gpsd = p.use_gpsd;
    gps_host = p.gps_host;
    gps_port = p.gps_port;
    restart_gpsd = p.restart_gpsd;
    iAudSampleRate = p.iAudSampleRate;
    iSigSampleRate = p.iSigSampleRate;
    iSigUpscaleRatio = p.iSigUpscaleRatio;
    iNewAudSampleRate = p.iNewAudSampleRate;
    iNewSigSampleRate = p.iNewSigSampleRate;
    iNewSigUpscaleRatio = p.iNewSigUpscaleRatio;
    rSysSimSNRdB = p.rSysSimSNRdB;
    iFrequency = p.iFrequency;
    bValidSignalStrength = p.bValidSignalStrength;
    rSigStr = p.rSigStr;
    rIFSigStr = p.rIFSigStr;
    iCurSelAudioService = p.iCurSelAudioService;
    iCurSelDataService = p.iCurSelDataService;
    eRobustnessMode = p.eRobustnessMode;
    eSpectOccup = p.eSpectOccup;
    LastAudioService = p.LastAudioService;
    LastDataService = p.LastDataService;
    lenient_RSCI = p.lenient_RSCI;
    return *this;
}

void CParameter::SetReceiver(CDRMReceiver* pDRMReceiver)
{
	pDRMRec = pDRMReceiver;
    if (pDRMRec)
        eReceiverMode = pDRMRec->GetReceiverMode();
}

void CParameter::ResetServicesStreams()
{
    int i;
    if (GetReceiverMode() == RM_DRM)
    {

        /* Store informations about last service selected
         * so the current service can be selected automatically after a resync */

        if (iCurSelAudioService > 0)
            LastAudioService.Save(iCurSelAudioService, Service[iCurSelAudioService].iServiceID);

        if (iCurSelDataService > 0)
            LastDataService.Save(iCurSelDataService, Service[iCurSelDataService].iServiceID);

        /* Reset everything to possible start values */
        for (i = 0; i < MAX_NUM_SERVICES; i++)
        {
            Service[i].AudioParam.strTextMessage = "";
            Service[i].AudioParam.iStreamID = STREAM_ID_NOT_USED;
            Service[i].AudioParam.eAudioCoding = CAudioParam::AC_NONE;
            Service[i].AudioParam.eSBRFlag = CAudioParam::SBR_NOT_USED;
            Service[i].AudioParam.eAudioSamplRate = CAudioParam::AS_24KHZ;
            Service[i].AudioParam.bTextflag = false;
            Service[i].AudioParam.bEnhanceFlag = false;
            Service[i].AudioParam.eAudioMode = CAudioParam::AM_MONO;
            Service[i].AudioParam.eSurround = CAudioParam::MS_NONE;
            Service[i].AudioParam.xHE_AAC_config.clear();

            Service[i].DataParam.iStreamID = STREAM_ID_NOT_USED;
            Service[i].DataParam.ePacketModInd = CDataParam::PM_PACKET_MODE;
            Service[i].DataParam.eDataUnitInd = CDataParam::DU_SINGLE_PACKETS;
            Service[i].DataParam.iPacketID = 0;
            Service[i].DataParam.iPacketLen = 0;
            Service[i].DataParam.eAppDomain = CDataParam::AD_DRM_SPEC_APP;
            Service[i].DataParam.iUserAppIdent = 0;

            Service[i].iServiceID = SERV_ID_NOT_USED;
            Service[i].eCAIndication = CService::CA_NOT_USED;
            Service[i].iLanguage = 0;
            Service[i].strCountryCode = "";
            Service[i].strLanguageCode = "";
            Service[i].eAudDataFlag = CService::SF_AUDIO;
            Service[i].iServiceDescr = 0;
            Service[i].strLabel = "";
			AudioComponentStatus[i].SetStatus(NOT_PRESENT);
			DataComponentStatus[i].SetStatus(NOT_PRESENT);
        }

        for (i = 0; i < MAX_NUM_STREAMS; i++)
        {
            Stream[i].iLenPartA = 0;
            Stream[i].iLenPartB = 0;
        }
    }
    else
    {

        // Set up encoded AM audio parameters
        Service[0].AudioParam.strTextMessage = "";
        Service[0].AudioParam.iStreamID = 0;
        Service[0].AudioParam.eAudioCoding = CAudioParam::AC_AAC;
        Service[0].AudioParam.eSBRFlag = CAudioParam::SBR_NOT_USED;
        Service[0].AudioParam.eAudioSamplRate = CAudioParam::AS_24KHZ;
        Service[0].AudioParam.bTextflag = false;
        Service[0].AudioParam.bEnhanceFlag = false;
        Service[0].AudioParam.eAudioMode = CAudioParam::AM_MONO; // ? FM could be stereo
        Service[0].AudioParam.eSurround = CAudioParam::MS_NONE;
        Service[0].AudioParam.xHE_AAC_config.clear();

        Service[0].iServiceID = SERV_ID_NOT_USED;
        Service[0].eCAIndication = CService::CA_NOT_USED;
        Service[0].iLanguage = 0;
        Service[0].strCountryCode = "";
        Service[0].strLanguageCode = "";
        Service[0].eAudDataFlag = CService::SF_AUDIO;
        Service[0].iServiceDescr = 0;
        Service[0].strLabel = "";

        Stream[0].iLenPartA = 0;
        Stream[0].iLenPartB = 1044;
    }

    /* Reset alternative frequencies */
    AltFreqSign.Reset();

    /* Date, time */
    iDay = 0;
    iMonth = 0;
    iYear = 0;
    iUTCHour = 0;
    iUTCMin = 0;
    iUTCOff = 0;
    iUTCSense = 0;
    bValidUTCOffsetAndSense = false;
}

void CParameter::GetActiveServices(set<int>& actServ)
{
    /* Init return vector */
    actServ.clear();

    /* Get active services */
    for (int i = 0; i < MAX_NUM_SERVICES; i++)
    {
        if (Service[i].IsActive())
            /* A service is active, add ID to set */
            actServ.insert(i);
    }
}

/* using a set ensures each stream appears only once */
void CParameter::GetActiveStreams(set<int>& actStr)
{
    actStr.clear();

    /* Determine which streams are active */
    for (int i = 0; i < MAX_NUM_SERVICES; i++)
    {
        if (Service[i].IsActive())
        {
            /* Audio stream */
            if (Service[i].AudioParam.iStreamID != STREAM_ID_NOT_USED)
                actStr.insert(Service[i].AudioParam.iStreamID);

            /* Data stream */
            if (Service[i].DataParam.iStreamID != STREAM_ID_NOT_USED)
                actStr.insert(Service[i].DataParam.iStreamID);
        }
    }
}

_REAL CParameter::GetBitRateKbps(const int iShortID, const bool bAudData) const
{
    /* Init lengths to zero in case the stream is not yet assigned */
    int iLen = 0;

    /* First, check if audio or data service and get lengths */
    if (Service[iShortID].eAudDataFlag == CService::SF_AUDIO)
    {
        /* Check if we want to get the data stream connected to an audio
           stream */
        if (bAudData)
        {
            iLen = GetStreamLen( Service[iShortID].DataParam.iStreamID);
        }
        else
        {
            iLen = GetStreamLen( Service[iShortID].AudioParam.iStreamID);
        }
    }
    else
    {
        iLen = GetStreamLen( Service[iShortID].DataParam.iStreamID);
    }

    /* We have 3 frames with time duration of 1.2 seconds. Bit rate should be
       returned in kbps (/ 1000) */
    return (_REAL) iLen * SIZEOF__BYTE * 3 / (_REAL) 1.2 / 1000;
}

_REAL CParameter::PartABLenRatio(const int iShortID) const
{
    int iLenA = 0;
    int iLenB = 0;

    /* Get the length of protection part A and B */
    if (Service[iShortID].eAudDataFlag == CService::SF_AUDIO)
    {
        /* Audio service */
        if (Service[iShortID].AudioParam.iStreamID != STREAM_ID_NOT_USED)
        {
            iLenA = Stream[Service[iShortID].AudioParam.iStreamID].iLenPartA;
            iLenB = Stream[Service[iShortID].AudioParam.iStreamID].iLenPartB;
        }
    }
    else
    {
        /* Data service */
        if (Service[iShortID].DataParam.iStreamID != STREAM_ID_NOT_USED)
        {
            iLenA = Stream[Service[iShortID].DataParam.iStreamID].iLenPartA;
            iLenB = Stream[Service[iShortID].DataParam.iStreamID].iLenPartB;
        }
    }

    const int iTotLen = iLenA + iLenB;

    if (iTotLen != 0)
        return (_REAL) iLenA / iTotLen;
    else
        return (_REAL) 0.0;
}

void CParameter::InitCellMapTable(const ERobMode eNewWaveMode,
                                  const ESpecOcc eNewSpecOcc)
{
    /* Set new values and make table */
    eRobustnessMode = eNewWaveMode;
    eSpectOccup = eNewSpecOcc;
    CellMappingTable.MakeTable(eRobustnessMode, eSpectOccup, iSigSampleRate);
}

bool CParameter::SetWaveMode(const ERobMode eNewWaveMode)
{
    /* First check if spectrum occupancy and robustness mode pair is defined */
    if ((
                (eNewWaveMode == RM_ROBUSTNESS_MODE_C) ||
                (eNewWaveMode == RM_ROBUSTNESS_MODE_D)
            ) && !(
                (eSpectOccup == SO_3) ||
                (eSpectOccup == SO_5)
            ))
    {
        /* Set spectrum occupance to a valid parameter */
        eSpectOccup = SO_3;
    }

    /* Apply changes only if new paramter differs from old one */
    if (eRobustnessMode != eNewWaveMode)
    {
        /* Set new value */
        eRobustnessMode = eNewWaveMode;

        /* This parameter change provokes update of table */
        CellMappingTable.MakeTable(eRobustnessMode, eSpectOccup, iSigSampleRate);

        /* Set init flags */
        if (pDRMRec) pDRMRec->InitsForWaveMode();

        /* Signal that parameter has changed */
        return true;
    }
    else
        return false;
}

void CParameter::SetSpectrumOccup(ESpecOcc eNewSpecOcc)
{
    /* First check if spectrum occupancy and robustness mode pair is defined */
    if ((
                (eRobustnessMode == RM_ROBUSTNESS_MODE_C) ||
                (eRobustnessMode == RM_ROBUSTNESS_MODE_D)
            ) && !(
                (eNewSpecOcc == SO_3) ||
                (eNewSpecOcc == SO_5)
            ))
    {
        /* Set spectrum occupance to a valid parameter */
        eNewSpecOcc = SO_3;
    }

    /* Apply changes only if new paramter differs from old one */
    if (eSpectOccup != eNewSpecOcc)
    {
        /* Set new value */
        eSpectOccup = eNewSpecOcc;

        /* This parameter change provokes update of table */
        CellMappingTable.MakeTable(eRobustnessMode, eSpectOccup, iSigSampleRate);

        /* Set init flags */
        if (pDRMRec) pDRMRec->InitsForSpectrumOccup();
    }
}

void CParameter::SetStreamLen(const int iStreamID, const int iNewLenPartA,
                              const int iNewLenPartB)
{
    /* Apply changes only if parameters have changed */
    if ((Stream[iStreamID].iLenPartA != iNewLenPartA) ||
            (Stream[iStreamID].iLenPartB != iNewLenPartB))
    {
        /* Use new parameters */
        Stream[iStreamID].iLenPartA = iNewLenPartA;
        Stream[iStreamID].iLenPartB = iNewLenPartB;

        /* Set init flags */
        if (pDRMRec) pDRMRec->InitsForMSC();
    }
}

int CParameter::GetStreamLen(const int iStreamID) const
{
    if (iStreamID != STREAM_ID_NOT_USED)
        return Stream[iStreamID].iLenPartA + Stream[iStreamID].iLenPartB;
    else
        return 0;
}

void CParameter::SetNumDecodedBitsMSC(const int iNewNumDecodedBitsMSC)
{
    /* Apply changes only if parameters have changed */
    if (iNewNumDecodedBitsMSC != iNumDecodedBitsMSC)
    {
        iNumDecodedBitsMSC = iNewNumDecodedBitsMSC;

        /* Set init flags */
        if (pDRMRec) pDRMRec->InitsForMSCDemux();
    }
}

void CParameter::SetNumDecodedBitsSDC(const int iNewNumDecodedBitsSDC)
{
    /* Apply changes only if parameters have changed */
    if (iNewNumDecodedBitsSDC != iNumSDCBitsPerSFrame)
    {
        iNumSDCBitsPerSFrame = iNewNumDecodedBitsSDC;

        /* Set init flags */
        if (pDRMRec) pDRMRec->InitsForNoDecBitsSDC();
    }
}

void CParameter::SetNumBitsHieraFrTot(const int iNewNumBitsHieraFrTot)
{
    /* Apply changes only if parameters have changed */
    if (iNewNumBitsHieraFrTot != iNumBitsHierarchFrameTotal)
    {
        iNumBitsHierarchFrameTotal = iNewNumBitsHieraFrTot;

        /* Set init flags */
        if (pDRMRec) pDRMRec->InitsForMSCDemux();
    }
}

void CParameter::SetNumAudioDecoderBits(const int iNewNumAudioDecoderBits)
{
    /* Apply changes only if parameters have changed */
    if (iNewNumAudioDecoderBits != iNumAudioDecoderBits)
    {
        iNumAudioDecoderBits = iNewNumAudioDecoderBits;
    }
}

void CParameter::SetNumDataDecoderBits(const int iNewNumDataDecoderBits)
{
    /* Apply changes only if parameters have changed */
    if (iNewNumDataDecoderBits != iNumDataDecoderBits)
    {
        iNumDataDecoderBits = iNewNumDataDecoderBits;
    }
}

void CParameter::SetMSCProtLev(const CMSCProtLev NewMSCPrLe,
                               const bool bWithHierarch)
{
    bool bParamersHaveChanged = false;

    if ((NewMSCPrLe.iPartA != MSCPrLe.iPartA) ||
            (NewMSCPrLe.iPartB != MSCPrLe.iPartB))
    {
        MSCPrLe.iPartA = NewMSCPrLe.iPartA;
        MSCPrLe.iPartB = NewMSCPrLe.iPartB;

        bParamersHaveChanged = true;
    }

    /* Apply changes only if parameters have changed */
    if (bWithHierarch)
    {
        if (NewMSCPrLe.iHierarch != MSCPrLe.iHierarch)
        {
            MSCPrLe.iHierarch = NewMSCPrLe.iHierarch;

            bParamersHaveChanged = true;
        }
    }

    /* In case parameters have changed, set init flags */
    if (bParamersHaveChanged)
        if (pDRMRec) pDRMRec->InitsForMSC();
}

void CParameter::SetServiceParameters(int iShortID, const CService& newService)
{
    Service[iShortID] = newService;
}

void CParameter::SetAudioParam(const int iShortID, const CAudioParam& NewAudParam)
{
    /* Apply changes only if parameters have changed */
    if (Service[iShortID].AudioParam != NewAudParam)
    {
        Service[iShortID].AudioParam = NewAudParam;

        /* Set init flags */
        if (pDRMRec) pDRMRec->InitsForAudParam();
    }
}

CAudioParam CParameter::GetAudioParam(const int iShortID) const
{
    return Service[iShortID].AudioParam;
}

void CParameter::SetDataParam(const int iShortID, const CDataParam& NewDataParam)
{
    /* Apply changes only if parameters have changed */
    if (Service[iShortID].DataParam != NewDataParam)
    {
        Service[iShortID].DataParam = NewDataParam;
        /* Set init flags */
        if (pDRMRec) pDRMRec->InitsForDataParam();
    }
}

CDataParam CParameter::GetDataParam(const int iShortID) const
{
    return Service[iShortID].DataParam;
}

void CParameter::SetInterleaverDepth(const ESymIntMod eNewDepth)
{
    if (eSymbolInterlMode != eNewDepth)
    {
        eSymbolInterlMode = eNewDepth;

        /* Set init flags */
        if (pDRMRec) pDRMRec->InitsForInterlDepth();
    }
}

void CParameter::SetMSCCodingScheme(const ECodScheme eNewScheme)
{
    if (eMSCCodingScheme != eNewScheme)
    {
        eMSCCodingScheme = eNewScheme;

        /* Set init flags */
        if (pDRMRec) pDRMRec->InitsForMSCCodSche();
    }
}

void CParameter::SetSDCCodingScheme(const ECodScheme eNewScheme)
{
    if (eSDCCodingScheme != eNewScheme)
    {
        eSDCCodingScheme = eNewScheme;

        /* Set init flags */
        if (pDRMRec) pDRMRec->InitsForSDCCodSche();
    }
}

void CParameter::SetCurSelAudioService(const int iNewService)
{
    /* Change the current selected audio service ID only if the new ID does
       contain an audio service. If not, keep the old ID. In that case it is
       possible to select a "data-only" service and still listen to the audio of
       the last selected service */
    if ((iCurSelAudioService != iNewService) &&
            (Service[iNewService].AudioParam.iStreamID != STREAM_ID_NOT_USED))
    {
        iCurSelAudioService = iNewService;

        LastAudioService.Reset();

        /* Set init flags */
        if (pDRMRec) pDRMRec->InitsForAudParam();
    }
}

void CParameter::SetCurSelDataService(const int iNewService)
{
    /* Change the current selected data service ID only if the new ID does
       contain a data service. If not, keep the old ID. In that case it is
       possible to select a "data-only" service and click back to an audio
       service to be able to decode data service and listen to audio at the
       same time */
    if ((iCurSelDataService != iNewService) &&
            (Service[iNewService].DataParam.iStreamID != STREAM_ID_NOT_USED))
    {
        iCurSelDataService = iNewService;
        LastDataService.Reset();

        /* Set init flags */
        if (pDRMRec) pDRMRec->InitsForDataParam();
    }
}

void CParameter::SetNumOfServices(const size_t iNNumAuSe, const size_t iNNumDaSe)
{
    /* Check whether number of activated services is not greater than the
       number of services signalled by the FAC because it can happen that
       a false CRC check (it is only a 8 bit CRC) of the FAC block
       initializes a wrong service */
    set<int> actServ;

    GetActiveServices(actServ);
    if (actServ.size() > iNNumAuSe + iNNumDaSe)
    {
        /* Reset services and streams and set flag for init modules */
        ResetServicesStreams();
        if (pDRMRec) pDRMRec->InitsForMSCDemux();
    }

    if ((iNumAudioService != iNNumAuSe) || (iNumDataService != iNNumDaSe))
    {
        iNumAudioService = iNNumAuSe;
        iNumDataService = iNNumDaSe;

        /* Set init flags */
        if (pDRMRec) pDRMRec->InitsForMSCDemux();
    }
}

void CParameter::SetAudDataFlag(const int iShortID, const CService::ETyOServ iNewADaFl)
{
    if (Service[iShortID].eAudDataFlag != iNewADaFl)
    {
        Service[iShortID].eAudDataFlag = iNewADaFl;

        /* Set init flags */
        if (pDRMRec) pDRMRec->InitsForMSC();
    }
}

void CParameter::SetServiceID(const int iShortID, const uint32_t iNewServiceID)
{
    if (Service[iShortID].iServiceID != iNewServiceID)
    {
        /* JFBC - what is this for? */
        if ((iShortID == 0) && (Service[0].iServiceID > 0))
            ResetServicesStreams();

        Service[iShortID].iServiceID = iNewServiceID;

        /* Set init flags */
        if (pDRMRec) pDRMRec->InitsForMSC();


        /* If the receiver has lost the sync automatically restore
        	last current service selected */

        if ((iShortID > 0) && (iNewServiceID > 0))
        {
            if (LastAudioService.iServiceID == iNewServiceID)
            {
                /* Restore last audio service selected */
                iCurSelAudioService = LastAudioService.iService;

                LastAudioService.Reset();

                /* Set init flags */
                if (pDRMRec) pDRMRec->InitsForAudParam();
            }

            if (LastDataService.iServiceID == iNewServiceID)
            {
                /* Restore last data service selected */
                iCurSelDataService = LastDataService.iService;

                LastDataService.Reset();

                /* Set init flags */
                if (pDRMRec) pDRMRec->InitsForDataParam();
            }
        }
    }
}

/* Append child directory to data files directory,
   always terminated by '/' or '\' */
string CParameter::GetDataDirectory(const char* pcChildDirectory) const
{
    string sDirectory(sDataFilesDirectory);
    size_t p = sDirectory.find_last_of(PATH_SEPARATORS);
    if (sDirectory != "" && (p == string::npos || p != (sDirectory.size()-1)))
        sDirectory += PATH_SEPARATOR;
    if (pcChildDirectory != nullptr)
    {
        sDirectory += pcChildDirectory;
        size_t p = sDirectory.find_last_of(PATH_SEPARATORS);
        if (sDirectory != "" && (p == string::npos || p != (sDirectory.size()-1)))
            sDirectory += PATH_SEPARATOR;
    }
    if (sDirectory == "")
        sDirectory = DEFAULT_DATA_FILES_DIRECTORY;
    return sDirectory;
}

/* Set new data files directory, terminated by '/' or '\' if not */
void CParameter::SetDataDirectory(string sNewDataFilesDirectory)
{
    if (sNewDataFilesDirectory == "")
        sNewDataFilesDirectory = DEFAULT_DATA_FILES_DIRECTORY;
    else
    {
        size_t p = sNewDataFilesDirectory.find_last_of(PATH_SEPARATORS);
        if (p == string::npos || p != (sNewDataFilesDirectory.size()-1))
            sNewDataFilesDirectory += PATH_SEPARATOR;
    }
    sDataFilesDirectory = sNewDataFilesDirectory;
}


/* Implementaions for simulation -------------------------------------------- */
void CRawSimData::Add(uint32_t iNewSRS)
{
    /* Attention, function does not take care of overruns, data will be
       lost if added to a filled shift register! */
    if (iCurWritePos < ciMaxDelBlocks)
        veciShRegSt[iCurWritePos++] = iNewSRS;
}

uint32_t CRawSimData::Get()
{
    /* We always use the first value of the array for reading and do a
       shift of the other data by adding a arbitrary value (0) at the
       end of the whole shift register */
    uint32_t iRet = veciShRegSt[0];
    veciShRegSt.AddEnd(0);
    iCurWritePos--;

    return iRet;
}

_REAL CParameter::GetSysSNRdBPilPos() const
{
    /*
    	Get system SNR in dB for the pilot positions. Since the average power of
    	the pilots is higher than the data cells, the SNR is also higher at these
    	positions compared to the total SNR of the DRM signal.
    */
    return (_REAL) 10.0 * log10(pow((_REAL) 10.0, rSysSimSNRdB / 10) /
                                CellMappingTable.rAvPowPerSymbol *
				CellMappingTable.rAvScatPilPow *
				(_REAL) CellMappingTable.iNumCarrier);
}

void
CParameter::SetSNR(const _REAL iNewSNR)
{
    SNRstat.addSample(iNewSNR);
}

_REAL
CParameter::GetSNR()
{
    return SNRstat.getCurrent();
}

_REAL CParameter::GetNominalSNRdB()
{
    /* Convert SNR from system bandwidth to nominal bandwidth */
    return (_REAL) 10.0 * log10(pow((_REAL) 10.0, rSysSimSNRdB / 10) *
                                GetSysToNomBWCorrFact());
}

void CParameter::SetNominalSNRdB(const _REAL rSNRdBNominal)
{
    /* Convert SNR from nominal bandwidth to system bandwidth */
    rSysSimSNRdB = (_REAL) 10.0 * log10(pow((_REAL) 10.0, rSNRdBNominal / 10) /
                                        GetSysToNomBWCorrFact());
}

_REAL CParameter::GetNominalBandwidth()
{
    _REAL rNomBW;

    /* Nominal bandwidth as defined in the DRM standard */
    switch (eSpectOccup)
    {
    case SO_0:
        rNomBW = (_REAL) 4500.0; /* Hz */
        break;

    case SO_1:
        rNomBW = (_REAL) 5000.0; /* Hz */
        break;

    case SO_2:
        rNomBW = (_REAL) 9000.0; /* Hz */
        break;

    case SO_3:
        rNomBW = (_REAL) 10000.0; /* Hz */
        break;

    case SO_4:
        rNomBW = (_REAL) 18000.0; /* Hz */
        break;

    case SO_5:
        rNomBW = (_REAL) 20000.0; /* Hz */
        break;

    default:
        rNomBW = (_REAL) 10000.0; /* Hz */
        break;
    }

    return rNomBW;
}

_REAL CParameter::GetSysToNomBWCorrFact()
{
    _REAL rNomBW = GetNominalBandwidth();

    /* Calculate system bandwidth (N / T_u) */
    const _REAL rSysBW = (_REAL) CellMappingTable.iNumCarrier / CellMappingTable.iFFTSizeN * iSigSampleRate;

    return rSysBW / rNomBW;
}


void CParameter::SetIFSignalLevel(_REAL rNewSigStr)
{
    rIFSigStr = rNewSigStr;
}

_REAL CParameter::GetIFSignalLevel()
{
    return rIFSigStr;
}

void CRxStatus::SetStatus(const ETypeRxStatus OK)
{
    status = OK;
    iNum++;
    if (OK==RX_OK)
        iNumOK++;
}

void CParameter::GenerateReceiverID()
{
    stringstream ssInfoVer;
    ssInfoVer << setw(2) << setfill('0') << setw(2) << setfill('0') << dream_version_major << setw(2) << setfill('0') << dream_version_minor;

    sReceiverID = string(dream_manufacturer)+string(dream_implementation)+ssInfoVer.str();

    while (sSerialNumber.length() < 6)
        sSerialNumber += "_";

    if (sSerialNumber.length() > 6)
        sSerialNumber.erase(6, sSerialNumber.length()-6);

    sReceiverID += sSerialNumber;
}

void CParameter::GenerateRandomSerialNumber()
{
    //seed random number generator
    srand((unsigned int)time(0));

    char randomChars[36];

    for (size_t q=0; q < 36; q++)
    {
        if (q < 26)
            randomChars[q] = char(q)+97;
        else
            randomChars[q] = (char(q)-26)+48;
    }

    char serialNumTemp[7];

    for (size_t i=0; i < 6; i++)
#ifdef _WIN32
        serialNumTemp[i] = randomChars[(int) 35.0*rand()/RAND_MAX]; /* integer overflow on linux, RAND_MAX=0x7FFFFFFF */
#else
        serialNumTemp[i] = randomChars[35ll * (long long)rand() / RAND_MAX];
#endif

    serialNumTemp[6] = '\0';

    sSerialNumber = serialNumTemp;
}

CMinMaxMean::CMinMaxMean():rSum(0.0),rCur(0.0),
    rMin(numeric_limits<_REAL>::max()),rMax(numeric_limits<_REAL>::min()),iNum(0)
{
}

void
CMinMaxMean::addSample(_REAL r)
{
    rCur = r;
    rSum += r;
    iNum++;
    if (r>rMax)
        rMax = r;
    if (r<rMin)
        rMin = r;
}

_REAL
CMinMaxMean::getCurrent()
{
    return rCur;
}

_REAL
CMinMaxMean::getMean()
{
    _REAL rMean = 0.0;
    if (iNum>0)
        rMean = rSum / iNum;
    rSum = 0.0;
    iNum = 0;
    return rMean;
}

void
CMinMaxMean::getMinMax(_REAL& rMinOut, _REAL& rMaxOut)
{
    if (rMin <= rMax)
    {
        rMinOut = rMin;
        rMaxOut = rMax;
    }
    else
    {
        rMinOut = 0.0;
        rMaxOut = 0.0;
    }
    rMin = numeric_limits<_REAL>::max();
    rMax = numeric_limits<_REAL>::min();
}

string CServiceDefinition::Frequency(size_t n) const
{
    if (n>=veciFrequencies.size())
        return ""; // not in the list

    stringstream ss;
    int iFrequency = veciFrequencies[n];

    switch (iSystemID)
    {
    case 0:
    case 1:
    case 2:
        /* AM or DRM */
        ss << iFrequency;
        break;

    case 3:
    case 4:
    case 5:
        /* 'FM1 frequency' - 87.5 to 107.9 MHz (100 kHz steps) */
        ss << 87.5 + 0.1 * float(iFrequency);
        break;

    case 6:
    case 7:
    case 8:
        /* 'FM2 frequency'- 76.0 to 90.0 MHz (100 kHz steps) */
        ss << 76.0 + 0.1 * float(iFrequency);
        break;

    case 9:
    case 10:
    case 11:
        if (iFrequency<=11) {
            int chan = iFrequency / 4;
            char subchan = 'A' + iFrequency % 4;
            ss << "Band I channel " << (chan+2) << subchan;
        } else if (64<= iFrequency && iFrequency <=95) {
            int chan = iFrequency / 4;
            char subchan = 'A' + iFrequency % 4;
            ss << "Band III channel " << (chan-11) << subchan;
        } else if (96<= iFrequency && iFrequency <=101) {
            int chan = iFrequency / 6;
            char subchan = 'A' + iFrequency % 6;
            ss << "Band III+ channel " << (chan-3) << subchan;
        } else if (128<= iFrequency && iFrequency <=143) {
            char chan = iFrequency - 128;
            double m = 1452.96+1.712*double(chan);
            ss << "European L-Band channel L" << ('A'+chan) << ", " << m << " MHz";
        } else if (160<= iFrequency && iFrequency <=182) {
            int chan = iFrequency - 159;
            double m = 1451.072+1.744*double(chan);
            ss << "Canadian L-Band channel " << chan << ", " << m << " MHz";
        } else {
            ss << "unknown channel " << iFrequency;
        }
        break;
    default:
        break;
    }
    return ss.str();
}

string CServiceDefinition::FrequencyUnits() const
{
    switch (iSystemID)
    {
    case 0:
    case 1:
    case 2:
        return "kHz";
        break;

    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
    case 8:
        return "MHz";
        break;

    default:
        return "";
        break;
    }
}

string CServiceDefinition::System() const
{
    switch (iSystemID)
    {
    case 0:
        return "DRM";
        break;

    case 1:
    case 2:
        return "AM";
        break;

    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
    case 8:
        return "FM";
        break;
    case 9:
    case 10:
    case 11:
        return "DAB";
        break;

    default:
        return "";
        break;
    }
}

string COtherService::ServiceID() const
{
    stringstream ss;
    /*
    switch (iSystemID)
    {
    case 0:
    case 1:
    	ss << "ID:" << hex << setw(6) << iServiceID;
    	break;

    case 3:
    case 6:
    	ss << "ECC+PI:" << hex << setw(6) << iServiceID;
    	break;
    case 4:
    case 7:
    	ss << "PI:" << hex << setw(4) << iServiceID;
    	break;
    case 9:
    	ss << "ECC+aud:" << hex << setw(6) << iServiceID;
    	break;
    case 10:
    	ss << "AUDIO:" << hex << setw(4) << iServiceID;
    	break;
    case 11:
    	ss << "DATA:" << hex << setw(8) << iServiceID;
    	break;
    	break;

    default:
    	break;
    }
    */
    return ss.str();
}

/* See ETSI ES 201 980 v2.1.1 Annex O */
bool
CAltFreqSched::IsActive(const time_t ltime)
{
    int iScheduleStart;
    int iScheduleEnd;
    int iWeekDay;

    /* Empty schedule is always active */
    if (iDuration == 0)
        return true;

    /* Calculate time in UTC */
    struct tm *gmtCur = gmtime(&ltime);

    /* Check day
       tm_wday: day of week (0 - 6; Sunday = 0)
       I must normalize so Monday = 0   */

    if (gmtCur->tm_wday == 0)
        iWeekDay = 6;
    else
        iWeekDay = gmtCur->tm_wday - 1;

    /* iTimeWeek minutes since last Monday 00:00 in UTC */
    /* the value is in the range 0 <= iTimeWeek < 60 * 24 * 7)   */

    const int iTimeWeek =
        (iWeekDay * 24 * 60) + (gmtCur->tm_hour * 60) + gmtCur->tm_min;

    /* Day Code: this field indicates which days the frequency schedule
     * (the following Start Time and Duration) applies to.
     * The msb indicates Monday, the lsb Sunday. Between one and seven bits may be set to 1.
     */
    for (int i = 0; i < 7; i++)
    {
        /* Check if day is active */
        if ((1 << (6 - i)) & iDayCode)
        {
            /* Tuesday -> 1 * 24 * 60 = 1440 */
            iScheduleStart = (i * 24 * 60) + iStartTime;
            iScheduleEnd = iScheduleStart + iDuration;

            /* the normal check (are we inside start and end?) */
            if ((iTimeWeek >= iScheduleStart) && (iTimeWeek <= iScheduleEnd))
                return true;

            /* the wrap-around check */
            const int iMinutesPerWeek = 7 * 24 * 60;

            if (iScheduleEnd > iMinutesPerWeek)
            {
                /* our duration wraps into next Monday (or even later) */
                if (iTimeWeek < (iScheduleEnd - iMinutesPerWeek))
                    return true;
            }
        }
    }
    return false;
}

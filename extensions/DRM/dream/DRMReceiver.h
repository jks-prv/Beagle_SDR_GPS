/******************************************************************************\
 * Technische Universitaet Darmstadt, Institut fuer Nachrichtentechnik
 * Copyright (c) 2001
 *
 * Author(s):
 *	Volker Fischer, Andrew Murphy, Andrea Russo, Oliver Haffenden
 *
 * Description:
 *	See DrmReceiver.cpp
 *
 * 11/21/2005 Andrew Murphy, BBC Research & Development, 2005
 *	- Additions to include AMSS demodulation
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

#ifndef DRMRECEIVER_H
#define DRMRECEIVER_H

#include "GlobalDefinitions.h"
#include <iostream>
#include "MDI/MDIRSCI.h" /* OPH: need this near the top so winsock2 is included before winsock */
#include "MDI/MDIDecode.h"
#include "Parameter.h"
#include "util/Buffer.h"
#include "util/Utilities.h"
#include "DataIO.h"
#include "OFDM.h"
#include "DRMSignalIO.h"
#include "MSCMultiplexer.h"
#include "InputResample.h"
#include "datadecoding/DataDecoder.h"
#include "sourcedecoders/AudioSourceEncoder.h"
#include "sourcedecoders/AudioSourceDecoder.h"
#include "MLC/MLC.h"
#include "interleaver/SymbolInterleaver.h"
#include "OFDMcellmapping/OFDMCellMapping.h"
#include "chanest/ChannelEstimation.h"
#include "sync/FreqSyncAcq.h"
#include "sync/TimeSync.h"
#include "sync/SyncUsingPil.h"
#include "AMDemodulation.h"
#include "AMSSDemodulation.h"
#include "sound/soundinterface.h"
#include "PlotManager.h"
#include "DrmTransceiver.h"

/* Definitions ****************************************************************/
/* Number of FAC frames until the acquisition is activated in case a signal
   was successfully decoded */
#ifdef KIWISDR
    #define	NUM_FAC_FRA_U_ACQ_WITH			(10*3)
#else
    #define	NUM_FAC_FRA_U_ACQ_WITH			10
#endif

/* Number of OFDM symbols until the acquisition is activated in case no signal
   could be decoded after previous acquisition try */
#ifdef KIWISDR
    #define	NUM_OFDMSYM_U_ACQ_WITHOUT		(150*3)
#else
    #define	NUM_OFDMSYM_U_ACQ_WITHOUT		150
#endif

/* Number of FAC blocks for delayed tracking mode switch (caused by time needed
   for initalizing the channel estimation */
#define NUM_FAC_DEL_TRACK_SWITCH		2

/* Length of the history for synchronization parameters (used for the plot) */
#define LEN_HIST_PLOT_SYNC_PARMS		2250


/* Classes ********************************************************************/
class CRig;
class CSettings;

class CSplitFAC : public CSplitModul<_BINARY>
{
    void SetInputBlockSize(CParameter&)
    {
        this->iInputBlockSize = NUM_FAC_BITS_PER_BLOCK;
    }
};

class CSplitSDC : public CSplitModul<_BINARY>
{
    void SetInputBlockSize(CParameter& p)
    {
        this->iInputBlockSize = p.iNumSDCBitsPerSFrame;
    }
};

class CSplitMSC : public CSplitModul<_BINARY>
{
public:
    void SetStream(int iID) {
        iStreamID = iID;
    }

protected:
    void SetInputBlockSize(CParameter& p)
    {
        this->iInputBlockSize = p.GetStreamLen(iStreamID) * SIZEOF__BYTE;
    }

    int iStreamID;
};

class CSplitAudio : public CSplitModul<_SAMPLE>
{
protected:
    void SetInputBlockSize(CParameter& p)
    {
        this->iInputBlockSize = (int) ((_REAL) p.GetAudSampleRate() * (_REAL) 0.4 /* 400 ms */) * 2 /* stereo */;
    }
};

class CConvertAudio : public CReceiverModul<_REAL,_SAMPLE>
{
protected:
    virtual void InitInternal(CParameter&);
    virtual void ProcessDataInternal(CParameter&);
};

class CDRMReceiver : public CDRMTransceiver
{
public:

    CDRMReceiver(CSettings* pSettings=nullptr);
    virtual ~CDRMReceiver();

    virtual void			LoadSettings() override; // can write to settings to set defaults
    virtual void			SaveSettings() override;

    std::string GetInputDevice()
    {
        return ReceiveData.GetSoundInterface();
    }

    std::string GetOutputDevice()
    {
        return WriteData.GetSoundInterface();
    }
    virtual void            EnumerateInputs(std::vector<string>& names, std::vector<string>& descriptions) override;
    virtual void            EnumerateOutputs(std::vector<string>& names, std::vector<string>& descriptions) override;
    virtual void            SetInputDevice(std::string) override;
    virtual void			SetOutputDevice(std::string) override;

    virtual CSettings*GetSettings()  override{
        return pSettings;
    }

    virtual void SetSettings(CSettings* pNewSettings) override {
        pSettings = pNewSettings;
    }

    virtual CParameter*	GetParameters() override {
        return &Parameters;
    }

    virtual bool IsReceiver() const override { return true; }
    virtual bool IsTransmitter() const override  { return false; }

    EAcqStat				GetAcquiState() {
        return Parameters.GetAcquiState();
    }
    ERecMode				GetReceiverMode() {
        return eReceiverMode;
    }
    bool GetDownstreamRSCIOutEnabled()
    {
        return downstreamRSCI.GetOutEnabled();
    }

    void					SetReceiverMode(ERecMode eNewMode);
    void					SetInitResOff(_REAL rNRO)
    {
        rInitResampleOffset = rNRO;
    }
    void					SetAMDemodType(EDemodType);
    void					SetAMFilterBW(int iBw);
    void					SetAMDemodAcq(_REAL rNewNorCen);
    void	 				SetFrequency(int);
    int		 				GetFrequency() {
        return Parameters.GetFrequency();
    }
    void					SetIQRecording(bool);
    void					SetRSIRecording(bool, const char);

    /* Channel Estimation */
    void SetFreqInt(ETypeIntFreq eNewTy)
    {
        ChannelEstimation.SetFreqInt(eNewTy);
    }

    ETypeIntFreq GetFrequencyInterpolationAlgorithm()
    {
        return ChannelEstimation.GetFrequencyInterpolationAlgorithm();
    }

    void SetTimeInt(ETypeIntTime eNewTy)
    {
        ChannelEstimation.SetTimeInt(eNewTy);
    }

    ETypeIntTime GetTimeInterpolationAlgorithm() const
    {
        return ChannelEstimation.GetTimeInterpolationAlgorithm();
    }

    void SetIntCons(const bool bNewIntCons)
    {
        ChannelEstimation.SetIntCons(bNewIntCons);
    }

    bool GetIntCons()
    {
        return ChannelEstimation.GetIntCons();
    }

    void SetSNREst(ETypeSNREst eNewTy)
    {
        ChannelEstimation.SetSNREst(eNewTy);
    }

    ETypeSNREst GetSNREst()
    {
        return ChannelEstimation.GetSNREst();
    }

    void SetTiSyncTracType(ETypeTiSyncTrac eNewTy)
    {
        ChannelEstimation.GetTimeSyncTrack()->SetTiSyncTracType(eNewTy);
    }

    ETypeTiSyncTrac GetTiSyncTracType()
    {
        return ChannelEstimation.GetTimeSyncTrack()->GetTiSyncTracType();
    }

    /* Get pointer to internal modules */
    CUtilizeFACData*		GetFAC() {
        return &UtilizeFACData;
    }
    CUtilizeSDCData*		GetSDC() {
        return &UtilizeSDCData;
    }
    CTimeSync*				GetTimeSync() {
        return &TimeSync;
    }
    CFACMLCDecoder*			GetFACMLC() {
        return &FACMLCDecoder;
    }
    CSDCMLCDecoder*			GetSDCMLC() {
        return &SDCMLCDecoder;
    }
    CMSCMLCDecoder*			GetMSCMLC() {
        return &MSCMLCDecoder;
    }
    CReceiveData*			GetReceiveData() {
        return &ReceiveData;
    }
    COFDMDemodulation*		GetOFDMDemod() {
        return &OFDMDemodulation;
    }
    CSyncUsingPil*			GetSyncUsPil() {
        return &SyncUsingPil;
    }
    CWriteData*				GetWriteData() {
        return &WriteData;
    }
    CDataDecoder*			GetDataDecoder() {
        return &DataDecoder;
    }
    CAMDemodulation*		GetAMDemod() {
        return &AMDemodulation;
    }
    CAMSSPhaseDemod*		GetAMSSPhaseDemod() {
        return &AMSSPhaseDemod;
    }
    CAMSSDecode*			GetAMSSDecode() {
        return &AMSSDecode;
    }
    CFreqSyncAcq*			GetFreqSyncAcq() {
        return &FreqSyncAcq;
    }
    CAudioSourceDecoder*	GetAudSorceDec() {
        return &AudioSourceDecoder;
    }
    CUpstreamDI*			GetRSIIn() {
        return pUpstreamRSCI;
    }
    CDownstreamDI*			GetRSIOut() {
        return &downstreamRSCI;
    }
    CChannelEstimation*		GetChannelEstimation() {
        return &ChannelEstimation;
    }

    CPlotManager*			GetPlotManager() {
        return &PlotManager;
    }

    void					InitsForWaveMode();
    void					InitsForSpectrumOccup();
    void					InitsForNoDecBitsSDC();
    void					InitsForAudParam();
    void					InitsForDataParam();
    void					InitsForInterlDepth();
    void					InitsForMSCCodSche();
    void					InitsForSDCCodSche();
    void					InitsForMSC();
    void					InitsForMSCDemux();
    void                    process();
    void                    updatePosition();
    void					InitReceiverMode();
    void					SetInStartMode();
    void                    CloseSoundInterfaces();

protected:

    void					SetInTrackingMode();
    void					SetInTrackingModeDelayed();
    void					InitsForAllModules();
    void					DemodulateDRM(bool&);
    void					DecodeDRM(bool&, bool&);
    void					UtilizeDRM(bool&);
    void					DemodulateAM(bool&);
    void					DecodeAM(bool&);
    void					UtilizeAM(bool&);
    void					DemodulateFM(bool&);
    void					DecodeFM(bool&);
    void					UtilizeFM(bool&);
    void					DetectAcquiFAC();
    void					DetectAcquiSymbol();
    void					saveSDCtoFile();

    /* Modules */
    CReceiveData			ReceiveData;
    CWriteData				WriteData;
    CInputResample			InputResample;
    CFreqSyncAcq			FreqSyncAcq;
    CTimeSync				TimeSync;
    COFDMDemodulation		OFDMDemodulation;
    CSyncUsingPil			SyncUsingPil;
    CChannelEstimation		ChannelEstimation;
    COFDMCellDemapping		OFDMCellDemapping;
    CFACMLCDecoder			FACMLCDecoder;
    CUtilizeFACData			UtilizeFACData;
    CSDCMLCDecoder			SDCMLCDecoder;
    CUtilizeSDCData			UtilizeSDCData;
    CSymbDeinterleaver		SymbDeinterleaver;
    CMSCMLCDecoder			MSCMLCDecoder;
    CMSCDemultiplexer		MSCDemultiplexer;
    CAudioSourceDecoder		AudioSourceDecoder;
    CDataDecoder			DataDecoder;
    CSplit					Split;
    CSplit					SplitForIQRecord;
    CWriteIQFile			WriteIQFile;
    CSplitAudio				SplitAudio;
    CAudioSourceEncoderRx	AudioSourceEncoder; // For encoding the audio for RSI
    CSplitFAC				SplitFAC;
    CSplitSDC				SplitSDC;
    CSplitMSC				SplitMSC[MAX_NUM_STREAMS];
    CConvertAudio			ConvertAudio;
    CAMDemodulation			AMDemodulation;
    CAMSSPhaseDemod			AMSSPhaseDemod;
    CAMSSExtractBits		AMSSExtractBits;
    CAMSSDecode				AMSSDecode;

    CUpstreamDI*			pUpstreamRSCI;
    CDecodeRSIMDI			DecodeRSIMDI;
    CDownstreamDI			downstreamRSCI;

    /* Buffers */
    CSingleBuffer<_REAL>			AMDataBuf;
    CSingleBuffer<_REAL>			AMSSDataBuf;
    CSingleBuffer<_REAL>			AMSSPhaseBuf;
    CCyclicBuffer<_REAL>			AMSSResPhaseBuf;
    CCyclicBuffer<_BINARY>			AMSSBitsBuf;

    CCyclicBuffer<_REAL>			DemodDataBuf;
    CSingleBuffer<_REAL>			IQRecordDataBuf;

    CCyclicBuffer<_REAL>			RecDataBuf;
    CCyclicBuffer<_REAL>			InpResBuf;
    CCyclicBuffer<_COMPLEX>			FreqSyncAcqBuf;
    CSingleBuffer<_COMPLEX>			TimeSyncBuf;
    CSingleBuffer<_COMPLEX>			OFDMDemodBuf;
    CSingleBuffer<_COMPLEX>			SyncUsingPilBuf;
    CSingleBuffer<CEquSig>			ChanEstBuf;
    CCyclicBuffer<CEquSig>			MSCCarDemapBuf;
    CCyclicBuffer<CEquSig>			FACCarDemapBuf;
    CCyclicBuffer<CEquSig>			SDCCarDemapBuf;
    CSingleBuffer<CEquSig>			DeintlBuf;
    CSingleBuffer<_BINARY>			FACDecBuf;
    CSingleBuffer<_BINARY>			FACUseBuf;
    CSingleBuffer<_BINARY>			FACSendBuf;
    CSingleBuffer<_BINARY>			SDCDecBuf;
    CSingleBuffer<_BINARY>			SDCUseBuf;
    CSingleBuffer<_BINARY>			SDCSendBuf;
    CSingleBuffer<_BINARY>			MSCMLCDecBuf;
    CSingleBuffer<_BINARY>			RSIPacketBuf;
    std::vector<CSingleBuffer<_BINARY> >	MSCDecBuf;
    std::vector<CSingleBuffer<_BINARY> >	MSCUseBuf;
    std::vector<CSingleBuffer<_BINARY> >	MSCSendBuf;
    CSingleBuffer<_BINARY>			EncAMAudioBuf;
    CCyclicBuffer<_SAMPLE>			AudSoDecBuf;
    CCyclicBuffer<_SAMPLE>			AMAudioBuf;
    CCyclicBuffer<_SAMPLE>			AMSoEncBuf; // For encoding

    int						iAcquRestartCnt;
    int						iAcquDetecCnt;
    int						iGoodSignCnt;
    int						iDelayedTrackModeCnt;
    ERecState				eReceiverState;
    ERecMode				eReceiverMode;
    ERecMode				eNewReceiverMode;

    int						iAudioStreamID;
    int						iDataStreamID;

    _REAL					rInitResampleOffset;

    CVectorEx<_BINARY>		vecbiMostRecentSDC;

    /* number of frames without FAC data before generating free-running RSCI */
    static const int		MAX_UNLOCKED_COUNT;

    /* Counter for unlocked frames, to keep generating RSCI even when unlocked */
    int						iUnlockedCount;
    int						iBwAM;
    int						iBwLSB;
    int						iBwUSB;
    int						iBwCW;
    int						iBwFM;
    time_t					time_keeper;

    CPlotManager			PlotManager;
    std::string					rsiOrigin;
    int						iPrevSigSampleRate; /* sample rate before sound file */
    CParameter&             Parameters;
    CSettings*              pSettings;
};


#endif // !defined(DRMRECEIVER_H__3B0BA660_CA63_4344_BB2B_23E7A0D31912__INCLUDED_)

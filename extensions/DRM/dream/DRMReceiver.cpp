/******************************************************************************\
 * Technische Universitaet Darmstadt, Institut fuer Nachrichtentechnik
 * Copyright (c) 2001-2005
 *
 * Author(s):
 *	Volker Fischer, Andrew Murphy, Julian Cable
 *
 * Description:
 *	DRM-receiver
 * The hand over of data is done via an intermediate-buffer. The calling
 * convention is always "input-buffer, output-buffer". Additionally, the
 * DRM-parameters are fed to the function.
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

// NB v1.470: Because of the C_LINKAGE() change

#include "DRM_main.h"
#include "DRMReceiver.h"

#include "util/Settings.h"
#include "util/Utilities.h"
#include "util/FileTyper.h"

#include "mode.h"
#include "sound/sound.h"
#include "sound/soundnull.h"
#include "sound/AudioFileIn.h"

#if 0
#include <fcd.h>
#endif
const int
CDRMReceiver::MAX_UNLOCKED_COUNT = 2;

/* Implementation *************************************************************/
CDRMReceiver::CDRMReceiver(CSettings* nPsettings) : CDRMTransceiver(),
    ReceiveData(), WriteData(),
    FreqSyncAcq(),
    ChannelEstimation(),
    UtilizeFACData(), UtilizeSDCData(), MSCDemultiplexer(),
    AudioSourceDecoder(),
    pUpstreamRSCI(new CUpstreamDI()), DecodeRSIMDI(), downstreamRSCI(),
    RSIPacketBuf(),
    MSCDecBuf(MAX_NUM_STREAMS), MSCUseBuf(MAX_NUM_STREAMS),
    MSCSendBuf(MAX_NUM_STREAMS), iAcquRestartCnt(0),
    iAcquDetecCnt(0), iGoodSignCnt(0), eReceiverMode(RM_DRM),
    eNewReceiverMode(RM_DRM), iAudioStreamID(STREAM_ID_NOT_USED),
    iDataStreamID(STREAM_ID_NOT_USED),
    rInitResampleOffset((_REAL) 0.0),
    iBwAM(10000), iBwLSB(5000), iBwUSB(5000), iBwCW(150), iBwFM(6000),
    time_keeper(0),
    PlotManager(), iPrevSigSampleRate(0),Parameters(*(new CParameter())), pSettings(nPsettings)
{
    Parameters.SetReceiver(this);
    downstreamRSCI.SetReceiver(this);
    PlotManager.SetReceiver(this);
#ifdef HAVE_LIBGPS
    Parameters.gps_data.gps_fd = -1;
#endif

}

CDRMReceiver::~CDRMReceiver()
{
    delete pUpstreamRSCI;
}

void
CDRMReceiver::SetReceiverMode(ERecMode eNewMode)
{
    if (eReceiverMode!=eNewMode || eNewReceiverMode != RM_NONE)
        eNewReceiverMode = eNewMode;
}

void
CDRMReceiver::SetAMDemodType(EDemodType eNew)
{
    AMDemodulation.SetDemodType(eNew);
    switch (eNew)
    {
    case DT_AM:
        AMDemodulation.SetFilterBW(iBwAM);
        break;

    case DT_LSB:
        AMDemodulation.SetFilterBW(iBwLSB);
        break;

    case DT_USB:
        AMDemodulation.SetFilterBW(iBwUSB);
        break;

    case DT_CW:
        AMDemodulation.SetFilterBW(iBwCW);
        break;

    case DT_FM:
        AMDemodulation.SetFilterBW(iBwFM);
        break;
    }
}

void
CDRMReceiver::SetAMFilterBW(int value)
{
    /* Store filter bandwidth for this demodulation type */
    switch (AMDemodulation.GetDemodType())
    {
    case DT_AM:
        iBwAM = value;
        break;

    case DT_LSB:
        iBwLSB = value;
        break;

    case DT_USB:
        iBwUSB = value;
        break;

    case DT_CW:
        iBwCW = value;
        break;

    case DT_FM:
        iBwFM = value;
        break;
    }
    AMDemodulation.SetFilterBW(value);
}

void
CDRMReceiver::SetInputDevice(string s)
{
    //ReceiveData.SetTrigger();
	ReceiveData.Stop();
	ReceiveData.ClearInputData();
	/* Get a fresh CUpstreamDI interface */
	if (pUpstreamRSCI->GetInEnabled())
	{
		delete pUpstreamRSCI;
		pUpstreamRSCI = new CUpstreamDI();
	}
    string device = s;
	FileTyper::type t = FileTyper::resolve(device);
	if (t == FileTyper::unrecognised) {
		vector<string> names;
		vector<string> descriptions;
		ReceiveData.Enumerate(names, descriptions);
		if (names.size() > 0) {
			if (device == "") {
				device = names[0];
				t = FileTyper::pcm;
			}
			else {
                for (unsigned i = 0; i<names.size(); i++) {
					if (device == names[i]) {
						device = names[i];
						t = FileTyper::pcm;
						break;
					}
				}
			}
		}
	}
    switch(t) {
    case FileTyper::pcm:
    case FileTyper::pipe:
        /* SetSyncInput to false, can be modified by pUpstreamRSCI */
        InputResample.SetSyncInput(false);
        SyncUsingPil.SetSyncInput(false);
        TimeSync.SetSyncInput(false);
        ReceiveData.SetSoundInterface(device); // audio input
        break;
    case FileTyper::unrecognised: // includes rsi network
    case FileTyper::pcap:
    case FileTyper::file_framing:
    case FileTyper::raw_af:
    case FileTyper::raw_pft:
        pUpstreamRSCI->SetOrigin(device);
    }
}

void
CDRMReceiver::SetOutputDevice(string device)
{
    WriteData.SetSoundInterface(device);
    WriteData.Init(Parameters);
}

void CDRMReceiver::EnumerateInputs(vector<string>& names, vector<string>& descriptions)
{
    ReceiveData.Enumerate(names, descriptions);
}

void CDRMReceiver::EnumerateOutputs(vector<string>& names, vector<string>& descriptions)
{
    WriteData.Enumerate(names, descriptions);
}

void
CDRMReceiver::DemodulateDRM(bool& bEnoughData)
{
    /* Resample input DRM-stream -------------------------------- */
    if (InputResample.ProcessData(Parameters, DemodDataBuf, InpResBuf))
    {
        bEnoughData = true;
    }
    drm_next_task("InputResample");

    /* Frequency synchronization acquisition -------------------- */
    if (FreqSyncAcq.ProcessData(Parameters, InpResBuf, FreqSyncAcqBuf))
    {
        bEnoughData = true;
    }
    drm_next_task("FreqSyncAcq");

    /* Time synchronization ------------------------------------- */
    if (TimeSync.ProcessData(Parameters, FreqSyncAcqBuf, TimeSyncBuf))
    {
        bEnoughData = true;
        /* Use count of OFDM-symbols for detecting
         * aquisition state for acquisition detection
         * only if no signal was decoded before */
        if (Parameters.eAcquiState == AS_NO_SIGNAL)
        {
            /* Increment symbol counter and check if bound is reached */
            iAcquDetecCnt++;

            if (iAcquDetecCnt > NUM_OFDMSYM_U_ACQ_WITHOUT)
                SetInStartMode();
        }
    }
    drm_next_task("TimeSync");

    /* OFDM-demodulation ---------------------------------------- */
    if (OFDMDemodulation.
            ProcessData(Parameters, TimeSyncBuf, OFDMDemodBuf))
    {
        bEnoughData = true;
    }
    drm_next_task("OFDMDemodulation");

    /* Synchronization in the frequency domain (using pilots) --- */
    if (SyncUsingPil.
            ProcessData(Parameters, OFDMDemodBuf, SyncUsingPilBuf))
    {
        bEnoughData = true;
    }
    drm_next_task("SyncUsingPil");

    /* Channel estimation and equalisation ---------------------- */
    if (ChannelEstimation.
            ProcessData(Parameters, SyncUsingPilBuf, ChanEstBuf))
    {
        bEnoughData = true;

        /* If this module has finished, all synchronization units
           have also finished their OFDM symbol based estimates.
           Update synchronization parameters histories */
        PlotManager.UpdateParamHistories(eReceiverState);
    }
    drm_next_task("ChannelEstimation");

    /* Demapping of the MSC, FAC, SDC and pilots off the carriers */
    if (OFDMCellDemapping.ProcessData(Parameters, ChanEstBuf,
                                      MSCCarDemapBuf,
                                      FACCarDemapBuf, SDCCarDemapBuf))
    {
        bEnoughData = true;
    }
    drm_next_task("OFDMCellDemapping");
}

void
CDRMReceiver::DecodeDRM(bool& bEnoughData, bool& bFrameToSend)
{
    /* FAC ------------------------------------------------------ */
    if (FACMLCDecoder.ProcessData(Parameters, FACCarDemapBuf, FACDecBuf))
    {
        bEnoughData = true;
        bFrameToSend = true;
    }
    drm_next_task("FACMLCDecoder");

    /* SDC ------------------------------------------------------ */
    if (SDCMLCDecoder.ProcessData(Parameters, SDCCarDemapBuf, SDCDecBuf))
    {
        bEnoughData = true;
    }
    drm_next_task("SDCMLCDecoder");

    /* MSC ------------------------------------------------------ */

    /* Symbol de-interleaver */
    if (SymbDeinterleaver.ProcessData(Parameters, MSCCarDemapBuf, DeintlBuf))
    {
        bEnoughData = true;
    }
    drm_next_task("SymbDeinterleaver");

    /* MLC decoder */
    if (MSCMLCDecoder.ProcessData(Parameters, DeintlBuf, MSCMLCDecBuf))
    {
        bEnoughData = true;
    }
    drm_next_task("MSCMLCDecoder");

    /* MSC demultiplexer (will leave FAC & SDC alone! */
    if (MSCDemultiplexer.ProcessData(Parameters, MSCMLCDecBuf, MSCDecBuf))
    {
        bEnoughData = true;
    }
    drm_next_task("MSCDemultiplexer");
}

void
CDRMReceiver::UtilizeDRM(bool& bEnoughData)
{
    if (UtilizeFACData.WriteData(Parameters, FACUseBuf))
    {
        bEnoughData = true;

        /* Use information of FAC CRC for detecting the acquisition
           requirement */
        DetectAcquiFAC();
    }
#if 0
        saveSDCtoFile();
#endif
    drm_next_task("UtilizeFACData");

    if (UtilizeSDCData.WriteData(Parameters, SDCUseBuf))
    {
        bEnoughData = true;
    }
    drm_next_task("UtilizeSDCData");

    /* Data decoding */
    if (iDataStreamID != STREAM_ID_NOT_USED)
    {
        //cerr << "data decode processing" << endl;
        if (DataDecoder.WriteData(Parameters, MSCUseBuf[iDataStreamID]))
            bEnoughData = true;
    }
    drm_next_task("DataDecoder");

    /* Source decoding (audio) */
    if (iAudioStreamID != STREAM_ID_NOT_USED)
    {
        //cerr << "audio processing" << endl;
        if (AudioSourceDecoder.ProcessData(Parameters,
                                           MSCUseBuf[iAudioStreamID],
                                           AudSoDecBuf))
        {
            bEnoughData = true;

            /* Store the number of correctly decoded audio blocks for
             *                            the history */
            PlotManager.SetCurrentCDAud(AudioSourceDecoder.GetNumCorDecAudio());
        }
        drm_next_task("audio");
    }
    
    #if 0
    if( (iDataStreamID == STREAM_ID_NOT_USED)
       &&
         (iAudioStreamID == STREAM_ID_NOT_USED)
    ) // try and decode stream 0 as audio anyway
    {
        //cerr << "stream 0" << endl;
        if (AudioSourceDecoder.ProcessData(Parameters,
                                           MSCUseBuf[0],
                                           AudSoDecBuf))
        {
            bEnoughData = true;

            /* Store the number of correctly decoded audio blocks for
             *                            the history */
            PlotManager.SetCurrentCDAud(AudioSourceDecoder.GetNumCorDecAudio());
        }
    }
    #endif
}

void
CDRMReceiver::DemodulateAM(bool& bEnoughData)
{
    /* The incoming samples are split 2 ways.
       One set is passed to the existing AM demodulator.
       The other set is passed to the new AMSS demodulator.
       The AMSS and AM demodulators work completely independently
     */
    if (Split.ProcessData(Parameters, DemodDataBuf, AMDataBuf, AMSSDataBuf))
    {
        bEnoughData = true;
    }

    /* AM demodulation ------------------------------------------ */
    if (AMDemodulation.ProcessData(Parameters, AMDataBuf, AMAudioBuf))
    {
        bEnoughData = true;
    }

    /* AMSS phase demodulation */
    if (AMSSPhaseDemod.ProcessData(Parameters, AMSSDataBuf, AMSSPhaseBuf))
    {
        bEnoughData = true;
    }
}

void
CDRMReceiver::DecodeAM(bool& bEnoughData)
{
    /* AMSS resampling */
    if (InputResample.ProcessData(Parameters, AMSSPhaseBuf, AMSSResPhaseBuf))
    {
        bEnoughData = true;
    }

    /* AMSS bit extraction */
    if (AMSSExtractBits.
            ProcessData(Parameters, AMSSResPhaseBuf, AMSSBitsBuf))
    {
        bEnoughData = true;
    }

    /* AMSS data decoding */
    if (AMSSDecode.ProcessData(Parameters, AMSSBitsBuf, SDCDecBuf))
    {
        bEnoughData = true;
    }
}

void
CDRMReceiver::UtilizeAM(bool& bEnoughData)
{
    if (UtilizeSDCData.WriteData(Parameters, SDCDecBuf))
    {
        bEnoughData = true;
    }
}

void CDRMReceiver::DemodulateFM(bool& bEnoughData)
{
    if (ConvertAudio.ProcessData(Parameters, DemodDataBuf, AMAudioBuf))
    {
        bEnoughData = true;
        iAudioStreamID = 0; // TODO
    }
}

void CDRMReceiver::DecodeFM(bool& bEnoughData)
{
    (void)bEnoughData;
}

void CDRMReceiver::UtilizeFM(bool& bEnoughData)
{
    (void)bEnoughData;
}

void
CDRMReceiver::DetectAcquiFAC()
{
    /* If upstreamRSCI in is enabled, do not check for acquisition state because we want
       to stay in tracking mode all the time */
    if (pUpstreamRSCI->GetInEnabled())
        return;

    /* Acquisition switch */
    if (!UtilizeFACData.GetCRCOk())
    {
        /* Reset "good signal" count */
        iGoodSignCnt = 0;

        iAcquRestartCnt++;

        /* Check situation when receiver must be set back in start mode */
        if ((Parameters.eAcquiState == AS_WITH_SIGNAL)
                && (iAcquRestartCnt > NUM_FAC_FRA_U_ACQ_WITH))
        {
            SetInStartMode();
        }
    }
    else
    {
        /* Set the receiver state to "with signal" not until two successive FAC
           frames are "ok", because there is only a 8-bit CRC which is not good
           for many bit errors. But it is very unlikely that we have two
           successive FAC blocks "ok" if no good signal is received */
        if (iGoodSignCnt > 0)
        {
            Parameters.eAcquiState = AS_WITH_SIGNAL;

            /* Take care of delayed tracking mode switch */
            if (iDelayedTrackModeCnt > 0)
                iDelayedTrackModeCnt--;
            else
                SetInTrackingModeDelayed();
        }
        else
        {
            /* If one CRC was correct, reset acquisition since
               we assume, that this was a correct detected signal */
            iAcquRestartCnt = 0;
            iAcquDetecCnt = 0;

            /* Set in tracking mode */
            SetInTrackingMode();

            iGoodSignCnt++;
        }
    }
}

void
CDRMReceiver::InitReceiverMode()
{
    switch (eNewReceiverMode)
    {
    case RM_AM:
    case RM_FM:
#if 0
        if (pAMParam == nullptr)
        {
            /* its the first time we have been in AM mode */
            if (pDRMParam == nullptr)
            {
                /* DRM Mode was never invoked so we get to claim the default parameter instance */
                pAMParam = pParameters;
            }
            else
            {
                /* change from DRM to AM Mode - we better have our own copy
                 * but make sure we inherit the initial settings of the default
                 */
                pAMParam = new CParameter(*pDRMParam);
            }
        }
        else
        {
            /* we have been in AM mode before, and have our own parameters but
             * we might need some state from the DRM mode params
             */
            switch (eReceiverMode)
            {
            case RM_AM:
                /* AM to AM switch */
                break;
            case RM_FM:
                /* AM to FM switch - re-acquisition requested - no special action */
                break;
            case RM_DRM:
                /* DRM to AM switch - grab some common stuff */
                pAMParam->rSigStrengthCorrection = pDRMParam->rSigStrengthCorrection;
                pAMParam->bMeasurePSD = pDRMParam->bMeasurePSD;
                pAMParam->bMeasurePSDAlways = pDRMParam->bMeasurePSDAlways;
                pAMParam->bMeasureInterference = pDRMParam->bMeasureInterference;
                pAMParam->FrontEndParameters = pDRMParam->FrontEndParameters;
                pAMParam->gps_data = pDRMParam->gps_data;
                pAMParam->use_gpsd = pDRMParam->use_gpsd;
                pAMParam->sSerialNumber = pDRMParam->sSerialNumber;
                pAMParam->sReceiverID  = pDRMParam->sReceiverID;
                pAMParam->SetDataDirectory(pDRMParam->GetDataDirectory());
                break;
            case RM_NONE:
                /* Start from cold in AM mode - no special action */
                break;
            }
        }
        pAMParam->eReceiverMode = eNewReceiverMode;
        pParameters = pAMParam;

        if (pParameters == nullptr)
            throw CGenErr("Something went terribly wrong in the Receiver");
#endif
        Parameters.eReceiverMode = eNewReceiverMode;

        /* Tell the SDC decoder that it's AMSS to decode (no AFS index) */
        UtilizeSDCData.GetSDCReceive()->SetSDCType(CSDCReceive::SDC_AMSS);

        /* Set the receive status - this affects the RSI output */
        Parameters.ReceiveStatus.TSync.SetStatus(NOT_PRESENT);
        Parameters.ReceiveStatus.FSync.SetStatus(NOT_PRESENT);
        Parameters.ReceiveStatus.FAC.SetStatus(NOT_PRESENT);
        Parameters.ReceiveStatus.SDC.SetStatus(NOT_PRESENT);
        Parameters.ReceiveStatus.SLAudio.SetStatus(NOT_PRESENT);
        Parameters.ReceiveStatus.LLAudio.SetStatus(NOT_PRESENT);
        break;
    case RM_DRM:
#if 0
        if (pDRMParam == nullptr)
        {
            /* its the first time we have been in DRM mode */
            if (pAMParam == nullptr)
            {
                /* AM Mode was never invoked so we get to claim the default parameter instance */
                pDRMParam = pParameters;
            }
            else
            {
                /* change from AM to DRM Mode - we better have our own copy
                 * but make sure we inherit the initial settings of the default
                 */
                pDRMParam = new CParameter(*pAMParam);
            }
        }
        else
        {
            /* we have been in DRM mode before, and have our own parameters but
             * we might need some state from the AM mode params
             */
            switch (eReceiverMode)
            {
            case RM_AM:
            case RM_FM:
                /* AM to DRM switch - grab some common stuff */
                pDRMParam->rSigStrengthCorrection = pAMParam->rSigStrengthCorrection;
                pDRMParam->bMeasurePSD = pAMParam->bMeasurePSD;
                pDRMParam->bMeasurePSDAlways = pAMParam->bMeasurePSDAlways;
                pDRMParam->bMeasureInterference = pAMParam->bMeasureInterference;
                pDRMParam->FrontEndParameters = pAMParam->FrontEndParameters;
                pDRMParam->gps_data = pAMParam->gps_data;
                pDRMParam->use_gpsd = pAMParam->use_gpsd;
                pDRMParam->sSerialNumber = pAMParam->sSerialNumber;
                pDRMParam->sReceiverID  = pAMParam->sReceiverID;
                pDRMParam->SetDataDirectory(pAMParam->GetDataDirectory());
                break;
            case RM_DRM:
                /* DRM to DRM switch - re-acquisition requested - no special action */
                break;
            case RM_NONE:
                /* Start from cold in DRM mode - no special action */
                break;
            }
        }
#endif
//        pDRMParam->eReceiverMode = RM_DRM;
//        pParameters = pDRMParam;

//        if (pParameters == nullptr)
//            throw CGenErr("Something went terribly wrong in the Receiver");
        Parameters.eReceiverMode = eNewReceiverMode;

        UtilizeSDCData.GetSDCReceive()->SetSDCType(CSDCReceive::SDC_DRM);
        break;
    case RM_NONE:
        return;
    }

    eReceiverMode = eNewReceiverMode;
    /* Reset new mode flag */
    eNewReceiverMode = RM_NONE;

    /* Init all modules */
    SetInStartMode();

    if (pUpstreamRSCI->GetOutEnabled())
    {
        pUpstreamRSCI->SetReceiverMode(eReceiverMode);
    }
}

void
CDRMReceiver::CloseSoundInterfaces()
{
    ReceiveData.Stop();
    WriteData.Stop();
}

void
CDRMReceiver::SetAMDemodAcq(_REAL rNewNorCen)
{
    /* Set the frequency where the AM demodulation should look for the
       aquisition. Receiver must be in AM demodulation mode */
    if (eReceiverMode == RM_AM)
    {
        AMDemodulation.SetAcqFreq(rNewNorCen);
        AMSSPhaseDemod.SetAcqFreq(rNewNorCen);
    }
}

void
CDRMReceiver::SetInStartMode()
{
    iUnlockedCount = MAX_UNLOCKED_COUNT;

    Parameters.Lock();
    /* Load start parameters for all modules */

    /* Define with which parameters the receiver should try to decode the
       signal. If we are correct with our assumptions, the receiver does not
       need to reinitialize */
    Parameters.InitCellMapTable(RM_ROBUSTNESS_MODE_B, SO_3);

    /* Set initial MLC parameters */
    Parameters.SetInterleaverDepth(CParameter::SI_LONG);
    Parameters.SetMSCCodingScheme(CS_3_SM);
    Parameters.SetSDCCodingScheme(CS_2_SM);

    /* Select the service we want to decode. Always zero, because we do not
       know how many services are transmitted in the signal we want to
       decode */

    /* TODO: if service 0 is not used but another service is the audio service
     * we have a problem. We should check as soon as we have information about
     * services if service 0 is really the audio service
     */

    /* Set the following parameters to zero states (initial states) --------- */
    Parameters.ResetServicesStreams();
    Parameters.ResetCurSelAudDatServ();

    /* Protection levels */
    Parameters.MSCPrLe.iPartA = 0;
    Parameters.MSCPrLe.iPartB = 1;
    Parameters.MSCPrLe.iHierarch = 0;

    /* Number of audio and data services */
    Parameters.iNumAudioService = 0;
    Parameters.iNumDataService = 0;

    /* We start with FAC ID = 0 (arbitrary) */
    Parameters.iFrameIDReceiv = 0;

    /* Set synchronization parameters */
    Parameters.rResampleOffset = rInitResampleOffset;	/* Initial resample offset */
    Parameters.rFreqOffsetAcqui = (_REAL) 0.0;
    Parameters.rFreqOffsetTrack = (_REAL) 0.0;
    Parameters.iTimingOffsTrack = 0;

    Parameters.Unlock();

    /* Initialization of the modules */
    InitsForAllModules();

    /* Activate acquisition */
    FreqSyncAcq.StartAcquisition();
    TimeSync.StartAcquisition();
    ChannelEstimation.GetTimeSyncTrack()->StopTracking();
    ChannelEstimation.StartSaRaOffAcq();
    ChannelEstimation.GetTimeWiener()->StopTracking();

    SyncUsingPil.StartAcquisition();
    SyncUsingPil.StopTrackPil();

    Parameters.Lock();
    /* Set flag that no signal is currently received */
    Parameters.eAcquiState = AS_NO_SIGNAL;

    /* Set flag for receiver state */
    eReceiverState = RS_ACQUISITION;

    /* Reset counters for acquisition decision, "good signal" and delayed
       tracking mode counter */
    iAcquRestartCnt = 0;
    iAcquDetecCnt = 0;
    iGoodSignCnt = 0;
    iDelayedTrackModeCnt = NUM_FAC_DEL_TRACK_SWITCH;

    /* Reset GUI lights */
    Parameters.ReceiveStatus.InterfaceI.SetStatus(NOT_PRESENT);
    Parameters.ReceiveStatus.InterfaceO.SetStatus(NOT_PRESENT);
    Parameters.ReceiveStatus.TSync.SetStatus(NOT_PRESENT);
    Parameters.ReceiveStatus.FSync.SetStatus(NOT_PRESENT);
    Parameters.ReceiveStatus.FAC.SetStatus(NOT_PRESENT);
    Parameters.ReceiveStatus.SDC.SetStatus(NOT_PRESENT);
    Parameters.ReceiveStatus.SLAudio.SetStatus(NOT_PRESENT);
    Parameters.ReceiveStatus.LLAudio.SetStatus(NOT_PRESENT);

    /* Clear audio decoder string */
    Parameters.audiodecoder = "";

    Parameters.Unlock();

    /* In case upstreamRSCI is enabled, go directly to tracking mode, do not activate the
       synchronization units */
    if (pUpstreamRSCI->GetInEnabled())
    {
        /* We want to have as low CPU usage as possible, therefore set the
           synchronization units in a state where they do only a minimum
           work */
        FreqSyncAcq.StopAcquisition();
        TimeSync.StopTimingAcqu();
        InputResample.SetSyncInput(true);
        SyncUsingPil.SetSyncInput(true);

        /* This is important so that always the same amount of module input
           data is queried, otherwise it could be that amount of input data is
           set to zero and the receiver gets into an infinite loop */
        TimeSync.SetSyncInput(true);

        /* Always tracking mode for upstreamRSCI */
        Parameters.Lock();
        Parameters.eAcquiState = AS_WITH_SIGNAL;
        Parameters.Unlock();

        SetInTrackingMode();
    }
}

void
CDRMReceiver::SetInTrackingMode()
{
    /* We do this with the flag "eReceiverState" to ensure that the following
       routines are only called once when the tracking is actually started */
    if (eReceiverState == RS_ACQUISITION)
    {
        /* In case the acquisition estimation is still in progress, stop it now
           to avoid a false estimation which could destroy synchronization */
        TimeSync.StopRMDetAcqu();

        /* Acquisition is done, deactivate it now and start tracking */
        ChannelEstimation.GetTimeWiener()->StartTracking();

        /* Reset acquisition for frame synchronization */
        SyncUsingPil.StopAcquisition();
        SyncUsingPil.StartTrackPil();

        /* Set receiver flag to tracking */
        eReceiverState = RS_TRACKING;
    }
}

void
CDRMReceiver::SetInTrackingModeDelayed()
{
    /* The timing tracking must be enabled delayed because it must wait until
       the channel estimation has initialized its estimation */
    TimeSync.StopTimingAcqu();
    ChannelEstimation.GetTimeSyncTrack()->StartTracking();
}

void
CDRMReceiver::process()
{
    #ifdef USE_MEASURE_TIME
        real_printf("| "); fflush(stdout);
    #endif

    bool bFrameToSend = false;
    bool bEnoughData = true;

    /* Input - from upstream RSCI or input and demodulation from sound card / file */
    if (pUpstreamRSCI->GetInEnabled())
    {
        RSIPacketBuf.Clear();
        pUpstreamRSCI->ReadData(Parameters, RSIPacketBuf);
        if (RSIPacketBuf.GetFillLevel() > 0)
        {
            time_keeper = time(nullptr);
            DecodeRSIMDI.ProcessData(Parameters, RSIPacketBuf, FACDecBuf, SDCDecBuf, MSCDecBuf);
            PlotManager.UpdateParamHistoriesRSIIn();
            bFrameToSend = true;
        }
        else
        {
            time_t now = time(nullptr);
            if ((now - time_keeper) > 2)
            {
                Parameters.ReceiveStatus.InterfaceI.SetStatus(NOT_PRESENT);
                Parameters.ReceiveStatus.InterfaceO.SetStatus(NOT_PRESENT);
                Parameters.ReceiveStatus.TSync.SetStatus(NOT_PRESENT);
                Parameters.ReceiveStatus.FSync.SetStatus(NOT_PRESENT);
                Parameters.ReceiveStatus.FAC.SetStatus(NOT_PRESENT);
                Parameters.ReceiveStatus.SDC.SetStatus(NOT_PRESENT);
                Parameters.ReceiveStatus.SLAudio.SetStatus(NOT_PRESENT);
            }
        }
    }
    else
    {

        if (WriteIQFile.IsRecording())
        {
            /* Receive data in RecDataBuf */
            ReceiveData.ReadData(Parameters, RecDataBuf);

            /* Split samples, one output to the demodulation, another for IQ recording */
            if (SplitForIQRecord.ProcessData(Parameters, RecDataBuf, DemodDataBuf, IQRecordDataBuf))
            {
                bEnoughData = true;
            }
            drm_next_task("WriteIQFile");
        }
        else
        {
            /* No I/Q recording then receive data directly in DemodDataBuf */
            MEASURE_TIME("r", 4, ReceiveData.ReadData(Parameters, DemodDataBuf));
            drm_next_task("read");
        }

        switch (eReceiverMode)
        {
        case RM_DRM:
            MEASURE_TIME("d", 1, DemodulateDRM(bEnoughData); DecodeDRM(bEnoughData, bFrameToSend));
            break;
        case RM_AM:
            DemodulateAM(bEnoughData);
            DecodeAM(bEnoughData);
            break;
        case RM_FM:
            DemodulateFM(bEnoughData);
            DecodeFM(bEnoughData);
            break;
        case RM_NONE:
            break;
        }
    }

    /* Split the data for downstream RSCI and local processing. TODO make this conditional */
    switch (eReceiverMode)
    {
    case RM_DRM:
        SplitFAC.ProcessData(Parameters, FACDecBuf, FACUseBuf, FACSendBuf);

        /* if we have an SDC block, make a copy and keep it until the next frame is to be sent */
        if (SDCDecBuf.GetFillLevel() == Parameters.iNumSDCBitsPerSFrame)
        {
            SplitSDC.ProcessData(Parameters, SDCDecBuf, SDCUseBuf, SDCSendBuf);
        }

        for (int i = 0; i < int(MSCDecBuf.size()); i++)
        {
            SplitMSC[i].ProcessData(Parameters, MSCDecBuf[i], MSCUseBuf[i], MSCSendBuf[i]);
        }
        break;
    case RM_AM:
        SplitAudio.ProcessData(Parameters, AMAudioBuf, AudSoDecBuf, AMSoEncBuf);
        break;
    case RM_FM:
        SplitAudio.ProcessData(Parameters, AMAudioBuf, AudSoDecBuf, AMSoEncBuf);
        break;
    case RM_NONE:
        break;
    }

    /* Decoding */
    while (bEnoughData) // TODO break if stop requested
    {
        /* Init flag */
        bEnoughData = false;

        // Write output I/Q file
        if (WriteIQFile.IsRecording())
        {
            if (WriteIQFile.WriteData(Parameters, IQRecordDataBuf))
            {
                bEnoughData = true;
            }
        }

        switch (eReceiverMode)
        {
        case RM_DRM:
            MEASURE_TIME("u", 2, UtilizeDRM(bEnoughData));
            break;
        case RM_AM:
            UtilizeAM(bEnoughData);
            break;
        case RM_FM:
            UtilizeFM(bEnoughData);
            break;
        case RM_NONE:
            break;
        }
    }

    /* Output to downstream RSCI */
    if (downstreamRSCI.GetOutEnabled())
    {
        switch (eReceiverMode)
        {
        case RM_DRM:
            if (Parameters.eAcquiState == AS_NO_SIGNAL)
            {
                /* we will get one of these between each FAC block, and occasionally we */
                /* might get two, so don't start generating free-wheeling RSCI until we've. */
                /* had three in a row */
                if (FreqSyncAcq.GetUnlockedFrameBoundary())
                {
                    if (iUnlockedCount < MAX_UNLOCKED_COUNT)
                        iUnlockedCount++;
                    else
                        downstreamRSCI.SendUnlockedFrame(Parameters);
                }
            }
            else if (bFrameToSend)
            {
                downstreamRSCI.SendLockedFrame(Parameters, FACSendBuf, SDCSendBuf, MSCSendBuf);
                iUnlockedCount = 0;
                bFrameToSend = false;
            }
            break;
        case RM_AM:
        case RM_FM:
            /* Encode audio for RSI output */
            if (AudioSourceEncoder.ProcessData(Parameters, AMSoEncBuf, MSCSendBuf[0]))
                bFrameToSend = true;

            if (bFrameToSend)
                downstreamRSCI.SendAMFrame(Parameters, MSCSendBuf[0]);
            break;
        case RM_NONE:
            break;
        }
    }

    /* Check for RSCI commands */
    if (downstreamRSCI.GetInEnabled())
    {
        downstreamRSCI.poll();
    }

    /* Play and/or save the audio */
    if (iAudioStreamID != STREAM_ID_NOT_USED || (eReceiverMode == RM_AM) || (eReceiverMode == RM_FM))
    {
        bool rv;
        MEASURE_TIME("w", 5, rv = WriteData.WriteData(Parameters, AudSoDecBuf));
        if (rv)
        {
            bEnoughData = true;
        }
    }
}

void CDRMReceiver::updatePosition()
{
#ifdef HAVE_LIBGPS
//TODO locking
    gps_data_t* gps_data = &Parameters.gps_data;
    int result=0;
    if(Parameters.restart_gpsd)
    {
        stringstream s;
        s <<  Parameters.gps_port;
        if(gps_data->gps_fd != -1) (void)gps_close(gps_data);
        result = gps_open(Parameters.gps_host.c_str(), s.str().c_str(), gps_data);
        if(!result) (void)gps_stream(gps_data, WATCH_ENABLE, nullptr);
        if(result) gps_data->gps_fd = -1;
        Parameters.restart_gpsd = false;
    }
    if(gps_data->gps_fd != -1)
    {
        if(Parameters.use_gpsd)
# if GPSD_API_MAJOR_VERSION < 7
            result = gps_read(gps_data);
# else
            result = gps_read(gps_data, NULL, 0);
# endif
        else
        {
            (void)gps_close(gps_data);
            gps_data->gps_fd = -1;
        }
    }
    (void)result;
#endif
}

void
CDRMReceiver::InitsForAllModules()
{
    if (downstreamRSCI.GetOutEnabled())
    {
        Parameters.bMeasureDelay = true;
        Parameters.bMeasureDoppler = true;
        Parameters.bMeasureInterference = true;
        Parameters.bMeasurePSD = true;
    }
    else
    {
        Parameters.bMeasureDelay = false;
        Parameters.bMeasureDoppler = false;
        Parameters.bMeasureInterference = false;
        if(Parameters.bMeasurePSDAlways)
            Parameters.bMeasurePSD = true;
        else
            Parameters.bMeasurePSD = false;
    }

    /* Set init flags */
    SplitFAC.SetInitFlag();
    SplitSDC.SetInitFlag();
    for (size_t i = 0; i < MSCDecBuf.size(); i++)
    {
        SplitMSC[i].SetStream(i);
        SplitMSC[i].SetInitFlag();
        MSCDecBuf[i].Clear();
        MSCUseBuf[i].Clear();
        MSCSendBuf[i].Clear();
    }
    ConvertAudio.SetInitFlag();

    //ReceiveData.SetTrigger();
    ReceiveData.SetInitFlag();
    InputResample.SetInitFlag();
    FreqSyncAcq.SetInitFlag();
    TimeSync.SetInitFlag();
    OFDMDemodulation.SetInitFlag();
    SyncUsingPil.SetInitFlag();
    ChannelEstimation.SetInitFlag();
    OFDMCellDemapping.SetInitFlag();
    FACMLCDecoder.SetInitFlag();
    UtilizeFACData.SetInitFlag();
    SDCMLCDecoder.SetInitFlag();
    UtilizeSDCData.SetInitFlag();
    SymbDeinterleaver.SetInitFlag();
    MSCMLCDecoder.SetInitFlag();
    DecodeRSIMDI.SetInitFlag();
    MSCDemultiplexer.SetInitFlag();
    AudioSourceDecoder.SetInitFlag();
    DataDecoder.SetInitFlag();
    WriteData.SetInitFlag();

    Split.SetInitFlag();
    SplitAudio.SetInitFlag();
    AudioSourceEncoder.SetInitFlag();
    AMDemodulation.SetInitFlag();

    SplitForIQRecord.SetInitFlag();
    WriteIQFile.SetInitFlag();
    /* AMSS */
    AMSSPhaseDemod.SetInitFlag();
    AMSSExtractBits.SetInitFlag();
    AMSSDecode.SetInitFlag();

    pUpstreamRSCI->SetInitFlag();
    //downstreamRSCI.SetInitFlag();

    /* Clear all buffers (this is especially important for the "AudSoDecBuf"
       buffer since AM mode and DRM mode use the same buffer. When init is
       called or modes are switched, the buffer could have some data left which
       lead to an overrun) */
    RecDataBuf.Clear();
    AMDataBuf.Clear();

    DemodDataBuf.Clear();
    IQRecordDataBuf.Clear();

    AMSSDataBuf.Clear();
    AMSSPhaseBuf.Clear();
    AMSSResPhaseBuf.Clear();
    AMSSBitsBuf.Clear();

    InpResBuf.Clear();
    FreqSyncAcqBuf.Clear();
    TimeSyncBuf.Clear();
    OFDMDemodBuf.Clear();
    SyncUsingPilBuf.Clear();
    ChanEstBuf.Clear();
    MSCCarDemapBuf.Clear();
    FACCarDemapBuf.Clear();
    SDCCarDemapBuf.Clear();
    DeintlBuf.Clear();
    FACDecBuf.Clear();
    SDCDecBuf.Clear();
    MSCMLCDecBuf.Clear();
    RSIPacketBuf.Clear();
    AudSoDecBuf.Clear();
    AMAudioBuf.Clear();
    AMSoEncBuf.Clear();
}

/* -----------------------------------------------------------------------------
   Initialization routines for the modules. We have to look into the modules
   and decide on which parameters the modules depend on */
void
CDRMReceiver::InitsForWaveMode()
{
    /* Reset averaging of the parameter histories (needed, e.g., because the
       number of OFDM symbols per DRM frame might have changed) */
    PlotManager.Init();

    /* After a new robustness mode was detected, give the time synchronization
       a bit more time for its job */
    iAcquDetecCnt = 0;

    /* Set init flags */
    ReceiveData.SetInitFlag();
    InputResample.SetInitFlag();
    FreqSyncAcq.SetInitFlag();
    Split.SetInitFlag();
    AMDemodulation.SetInitFlag();
    AudioSourceEncoder.SetInitFlag();

    SplitForIQRecord.SetInitFlag();
    WriteIQFile.SetInitFlag();

    AMSSPhaseDemod.SetInitFlag();
    AMSSExtractBits.SetInitFlag();
    AMSSDecode.SetInitFlag();

    TimeSync.SetInitFlag();
    OFDMDemodulation.SetInitFlag();
    SyncUsingPil.SetInitFlag();
    ChannelEstimation.SetInitFlag();
    OFDMCellDemapping.SetInitFlag();
    SymbDeinterleaver.SetInitFlag();	// Because of "iNumUsefMSCCellsPerFrame"
    MSCMLCDecoder.SetInitFlag();	// Because of "iNumUsefMSCCellsPerFrame"
    SDCMLCDecoder.SetInitFlag();	// Because of "iNumSDCCellsPerSFrame"
}

void
CDRMReceiver::InitsForSpectrumOccup()
{
    /* Set init flags */
    FreqSyncAcq.SetInitFlag();	// Because of bandpass filter
    OFDMDemodulation.SetInitFlag();
    SyncUsingPil.SetInitFlag();
    ChannelEstimation.SetInitFlag();
    OFDMCellDemapping.SetInitFlag();
    SymbDeinterleaver.SetInitFlag();	// Because of "iNumUsefMSCCellsPerFrame"
    MSCMLCDecoder.SetInitFlag();	// Because of "iNumUsefMSCCellsPerFrame"
    SDCMLCDecoder.SetInitFlag();	// Because of "iNumSDCCellsPerSFrame"
}

/* SDC ---------------------------------------------------------------------- */
void
CDRMReceiver::InitsForSDCCodSche()
{
    /* Set init flags */
    SDCMLCDecoder.SetInitFlag();

#ifdef USE_DD_WIENER_FILT_TIME
    ChannelEstimation.SetInitFlag();
#endif
}

void
CDRMReceiver::InitsForNoDecBitsSDC()
{
    /* Set init flag */
    SplitSDC.SetInitFlag();
    UtilizeSDCData.SetInitFlag();
}

/* MSC ---------------------------------------------------------------------- */
void
CDRMReceiver::InitsForInterlDepth()
{
    /* Can be absolutely handled seperately */
    SymbDeinterleaver.SetInitFlag();
}

void
CDRMReceiver::InitsForMSCCodSche()
{
    /* Set init flags */
    MSCMLCDecoder.SetInitFlag();
    MSCDemultiplexer.SetInitFlag();	// Not sure if really needed, look at code! TODO

#ifdef USE_DD_WIENER_FILT_TIME
    ChannelEstimation.SetInitFlag();
#endif
}

void
CDRMReceiver::InitsForMSC()
{
    /* Set init flags */
    MSCMLCDecoder.SetInitFlag();

    InitsForMSCDemux();
}

void
CDRMReceiver::InitsForMSCDemux()
{
    /* Set init flags */
    DecodeRSIMDI.SetInitFlag();
    MSCDemultiplexer.SetInitFlag();
    for (size_t i = 0; i < MSCDecBuf.size(); i++)
    {
        SplitMSC[i].SetStream(i);
        SplitMSC[i].SetInitFlag();
    }
    InitsForAudParam();
    InitsForDataParam();

    /* Reset value used for the history because if an audio service was selected
       but then only a data service is selected, the value would remain with the
       last state */
    PlotManager.SetCurrentCDAud(0);
}

void
CDRMReceiver::InitsForAudParam()
{
    for (size_t i = 0; i < MSCDecBuf.size(); i++)
    {
        MSCDecBuf[i].Clear();
        MSCUseBuf[i].Clear();
        MSCSendBuf[i].Clear();
    }

    /* Set init flags */
    DecodeRSIMDI.SetInitFlag();
    MSCDemultiplexer.SetInitFlag();
    int a = Parameters.GetCurSelAudioService();
    iAudioStreamID = Parameters.GetAudioParam(a).iStreamID;
    Parameters.SetNumAudioDecoderBits(Parameters.GetStreamLen(iAudioStreamID) * SIZEOF__BYTE);
    AudioSourceDecoder.SetInitFlag();
}

void
CDRMReceiver::InitsForDataParam()
{
    /* Set init flags */
    DecodeRSIMDI.SetInitFlag();
    MSCDemultiplexer.SetInitFlag();
    int d = Parameters.GetCurSelDataService();
    iDataStreamID = Parameters.GetDataParam(d).iStreamID;
    Parameters.SetNumDataDecoderBits(Parameters.
                                          GetStreamLen(iDataStreamID) *
                                          SIZEOF__BYTE);
    DataDecoder.SetInitFlag();
}

void CDRMReceiver::SetFrequency(int iNewFreqkHz)
{
    Parameters.Lock();
    Parameters.SetFrequency(iNewFreqkHz);
    /* clear out AMSS data and re-initialise AMSS acquisition */
    if (Parameters.eReceiverMode == RM_AM)
        Parameters.ResetServicesStreams();
    Parameters.Unlock();

    if (pUpstreamRSCI->GetOutEnabled())
    {
        pUpstreamRSCI->SetFrequency(iNewFreqkHz);
    }

#if 0
	{
		FCD_MODE_ENUM fme;
		unsigned int uFreq, rFreq;
		int lnbOffset = 6;
		double d = (double) (iNewFreqkHz-lnbOffset);

		//d *= 1.0 + n/1000000.0;
		uFreq = (unsigned int) d;

		fme = fcdAppSetFreq(uFreq, &rFreq);

		if ((fme != FCD_MODE_APP) || (uFreq != rFreq))
		{
			stringstream ss;
			ss << "Error in" << __FUNCTION__ << "set:" << uFreq << "read:" << rFreq;
			qDebug(ss.str().c_str()); 
		}
	}
#endif
    if (downstreamRSCI.GetOutEnabled())
        downstreamRSCI.NewFrequency(Parameters);

    /* tell the IQ file writer that freq has changed in case it needs to start a new file */
    WriteIQFile.NewFrequency(Parameters);
}

void
CDRMReceiver::SetIQRecording(bool bON)
{
    if (bON)
        WriteIQFile.StartRecording(Parameters);
    else
        WriteIQFile.StopRecording();
}

void
CDRMReceiver::SetRSIRecording(bool bOn, const char cProfile)
{
    downstreamRSCI.SetRSIRecording(Parameters, bOn, cProfile);
}

/* TEST store information about alternative frequency transmitted in SDC */
void
CDRMReceiver::saveSDCtoFile()
{
    static FILE *pFile = nullptr;

    if (pFile == nullptr)
        pFile = fopen("test/altfreq.dat", "w");

    Parameters.Lock();
    size_t inum = Parameters.AltFreqSign.vecMultiplexes.size();
    for (size_t z = 0; z < inum; z++)
    {
        fprintf(pFile, "sync:%d sr:", Parameters.AltFreqSign.vecMultiplexes[z].bIsSyncMultplx);

        for (int k = 0; k < 4; k++)
            fprintf(pFile, "%d", Parameters.AltFreqSign.vecMultiplexes[z].  veciServRestrict[k]);
        fprintf(pFile, " fr:");

        for (size_t kk = 0; kk < Parameters.AltFreqSign.vecMultiplexes[z].veciFrequencies.size(); kk++)
            fprintf(pFile, "%d ", Parameters.AltFreqSign.vecMultiplexes[z].  veciFrequencies[kk]);

        fprintf(pFile, " rID:%d sID:%d   /   ",
                Parameters.AltFreqSign.vecMultiplexes[z].iRegionID,
                Parameters.AltFreqSign.vecMultiplexes[z].iScheduleID);
    }
    Parameters.Unlock();
    fprintf(pFile, "\n");
    fflush(pFile);
}

void
CDRMReceiver::LoadSettings()
{
    if (pSettings == nullptr) return;
    CSettings& s = *pSettings;

    /* Serial Number */
    string sValue = s.Get("Receiver", "serialnumber");
    if (sValue != "")
    {
        // Pad to a minimum of 6 characters
        while (sValue.length() < 6)
            sValue += "_";
        Parameters.sSerialNumber = sValue;
    }

    Parameters.GenerateReceiverID();

    /* Data files directory */
    string sDataFilesDirectory = s.Get(
        "Receiver", "datafilesdirectory", Parameters.GetDataDirectory());
    Parameters.SetDataDirectory(sDataFilesDirectory);
    s.Put("Receiver", "datafilesdirectory", Parameters.GetDataDirectory());

    /* Receiver ------------------------------------------------------------- */

    /* Sound card audio sample rate, some settings below depends on this one */
    Parameters.SetNewAudSampleRate(s.Get("Receiver", "samplerateaud", int(DEFAULT_SOUNDCRD_SAMPLE_RATE)));

    /* Sound card signal sample rate, some settings below depends on this one */
    Parameters.SetNewSigSampleRate(s.Get("Receiver", "sampleratesig", int(DEFAULT_SOUNDCRD_SAMPLE_RATE)));

    /* Signal upscale ratio */
    Parameters.SetNewSigUpscaleRatio(s.Get("Receiver", "sigupratio", int(1)));

    /* Fetch new sample rate if any */
    Parameters.FetchNewSampleRate();

    /* if 0 then only measure PSD when RSCI in use otherwise always measure it */
    Parameters.bMeasurePSDAlways = s.Get("Receiver", "measurepsdalways", 0);

    /* Upstream RSCI if any */
    string str = s.Get("command", "rsiin");
    if (str == "") {

        /* Input from file if any */
        str = s.Get("command", string("fileio"));
        if (str == "") {
            /* Sound In device */
            str = s.Get("Receiver", "snddevin", string());
            if(str == "") {
                vector<string> vn,vd;
                EnumerateInputs(vn, vd);
                if(vn.size()>0) {
                    str = vn[0];
                }
            }
        }
    }
    SetInputDevice(str);

    /* Channel Estimation: Frequency Interpolation */
    SetFreqInt((ETypeIntFreq)s.Get("Receiver", "freqint", int(FWIENER)));

    /* Channel Estimation: Time Interpolation */
    SetTimeInt((ETypeIntTime)s.Get("Receiver", "timeint", int(TWIENER)));

    /* Time Sync Tracking */
    SetTiSyncTracType((ETypeTiSyncTrac)s.Get("Receiver", "timesync", int(TSENERGY)));

    /* Flip spectrum flag */
    ReceiveData.SetFlippedSpectrum(s.Get("Receiver", "flipspectrum", false));

    /* Input channel selection */
    ReceiveData.SetInChanSel((EInChanSel)s.Get("Receiver", "inchansel", int(CS_MIX_CHAN)));

    /* Output channel selection */
    WriteData.SetOutChanSel((EOutChanSel)s.Get("Receiver", "outchansel", int(CS_BOTH_BOTH)));

    /* AM Parameters */

    /* AGC */
    AMDemodulation.SetAGCType((EAmAgcType)s.Get("AM Demodulation", "agc", 0));

    /* noise reduction */
    AMDemodulation.SetNoiRedType((ENoiRedType)s.Get("AM Demodulation", "noisered", 0));

#ifdef HAVE_SPEEX
    /* noise reduction level */
    AMDemodulation.SetNoiRedLevel(s.Get("AM Demodulation", "noiseredlvl", -12));
#endif

    /* pll enabled/disabled */
    AMDemodulation.EnablePLL(s.Get("AM Demodulation", "enablepll", 0));

    /* auto frequency acquisition */
    AMDemodulation.EnableAutoFreqAcq(s.Get("AM Demodulation", "autofreqacq", 0));

    /* demodulation type and bandwidth */
    EDemodType eDemodType = (EDemodType)s.Get("AM Demodulation", "demodulation", DT_AM);
    iBwAM = s.Get("AM Demodulation", "filterbwam", 10000);
    iBwUSB = s.Get("AM Demodulation", "filterbwusb", 5000);
    iBwLSB = s.Get("AM Demodulation", "filterbwlsb", 5000);
    iBwCW = s.Get("AM Demodulation", "filterbwcw", 150);
    iBwFM = s.Get("AM Demodulation", "filterbwfm", 6000);

    /* Load user's saved filter bandwidth and demodulation type */
    SetAMDemodType(eDemodType);

    /* Sound Out device */
    str = s.Get("Receiver", "snddevout", string());
    if(str == "") {
        vector<string> vn,vd;
        EnumerateOutputs(vn, vd);
        if(vn.size()>0) {
            str = vn[0];
        }
    }
    string wav_str = s.Get("command", "writewav");
    //if (str != "" && wav_str == "") {
    if (str != "") {
        SetOutputDevice(str);
    } else {
    }

    str = s.Get("command", "rciout");
    if (str != "")
        pUpstreamRSCI->SetDestination(str);

    /* downstream RSCI */
	string rsiout = s.Get("command", "rsiout", string(""));
	string rciin = s.Get("command", "rciin", string(""));
	if(rsiout != "" || rciin != "")
	{
		istringstream cc(rciin);
		vector<string> rci;
		while(cc >> str)
		{
			rci.push_back(str);
		}
		istringstream ss(rsiout);
		size_t n=0;
		while(ss >> str)
		{
			char profile = str[0];
			string dest = str.substr(2);
			if(rci.size()>n)
			{
				downstreamRSCI.AddSubscriber(dest, profile, rci[n]);
				n++;
			}
			else
			{
				downstreamRSCI.AddSubscriber(dest, profile);
			}
		}
		for(;n<rci.size(); n++)
			downstreamRSCI.AddSubscriber("", ' ', rci[n]);
    }
    /* RSCI File Recording */
    str = s.Get("command", "rsirecordprofile");
    string s2 = s.Get("command", "rsirecordtype");
    if (str != "" || s2 != "")
        downstreamRSCI.SetRSIRecording(Parameters, true, str[0], s2);

    /* IQ File Recording */
    if (s.Get("command", "recordiq", false))
        WriteIQFile.StartRecording(Parameters);

    /* Mute audio flag */
    WriteData.MuteAudio(s.Get("Receiver", "muteaudio", false));

    /* Output to File */
    str = s.Get("command", "writewav");
    if (str != "")
    {
        WriteData.Init(Parameters); /* Needed for iAudSampleRate initialization */
        WriteData.StartWriteWaveFile(str);
    }

    /* Reverberation flag */
    AudioSourceDecoder.SetReverbEffect(s.Get("Receiver", "reverb", true));

    /* Bandpass filter flag */
    FreqSyncAcq.SetRecFilter(s.Get("Receiver", "filter", false));

    /* Set parameters for frequency acquisition search window */
    const _REAL rFreqAcSeWinCenter = s.Get("command", "fracwincent", -1);
    const _REAL rFreqAcSeWinSize = s.Get("command", "fracwinsize", -1);
    FreqSyncAcq.SetSearchWindow(rFreqAcSeWinCenter, rFreqAcSeWinSize);

    /* Modified metrics flag */
    ChannelEstimation.SetIntCons(s.Get("Receiver", "modmetric", false));

    /* Number of iterations for MLC setting */
    MSCMLCDecoder.SetNumIterations(s.Get("Receiver", "mlciter", 1));

    /* Receiver mode (DRM, AM, FM) */
    SetReceiverMode(ERecMode(s.Get("Receiver", "mode", int(0))));

    /* Tuned Frequency */
    Parameters.SetFrequency(s.Get("Receiver", "frequency", 0));

    /* Front-end - combine into Hamlib? */
    CFrontEndParameters& FrontEndParameters = Parameters.FrontEndParameters;

    FrontEndParameters.eSMeterCorrectionType =
        CFrontEndParameters::ESMeterCorrectionType(s.Get("FrontEnd", "smetercorrectiontype", 0));

    FrontEndParameters.rSMeterBandwidth = s.Get("FrontEnd", "smeterbandwidth", (_REAL) 0.0);

    FrontEndParameters.rDefaultMeasurementBandwidth = s.Get("FrontEnd", "defaultmeasurementbandwidth", 0);

    FrontEndParameters.bAutoMeasurementBandwidth = s.Get("FrontEnd", "automeasurementbandwidth", true);

    FrontEndParameters.rCalFactorDRM = s.Get("FrontEnd", "calfactordrm", (_REAL) 0.0);

    FrontEndParameters.rCalFactorAM = s.Get("FrontEnd", "calfactoram", (_REAL) 0.0);

    FrontEndParameters.rIFCentreFreq = s.Get("FrontEnd", "ifcentrefrequency", (_REAL(DEFAULT_SOUNDCRD_SAMPLE_RATE) / 4));

    /* Latitude string (used to be just for log file) */
    double latitude, longitude;
    latitude = s.Get("GPS", "latitude", s.Get("Logfile", "latitude", (_REAL) 1000.0));
    /* Longitude string */
    longitude = s.Get("GPS", "longitude", s.Get("Logfile", "longitude", (_REAL) 1000.0));
    Parameters.Lock();
    if(-90.0 <= latitude && latitude <= 90.0 && -180.0 <= longitude  && longitude <= 180.0)
    {
        Parameters.gps_data.set = LATLON_SET;
        Parameters.gps_data.fix.latitude = latitude;
        Parameters.gps_data.fix.longitude = longitude;
    }
    else {
        Parameters.gps_data.set = 0;
    }
    bool use_gpsd = s.Get("GPS", "usegpsd", false);
    Parameters.use_gpsd=use_gpsd;
    string host = s.Get("GPS", "host", string("localhost"));
    Parameters.gps_host = host;
    Parameters.gps_port = s.Get("GPS", "port", string("2947"));
    if(use_gpsd)
        Parameters.restart_gpsd=true;

    bool permissive = s.Get("command", "permissive", false);
    Parameters.lenient_RSCI = permissive;

    Parameters.Unlock();
}

void
CDRMReceiver::SaveSettings()
{
    if (pSettings == nullptr) return;
    CSettings& s = *pSettings;

    s.Put("Receiver", "mode", int(eReceiverMode));

    /* Receiver ------------------------------------------------------------- */

    /* Fetch new sample rate if any */
    Parameters.FetchNewSampleRate();

    /* Sound card audio sample rate */
    s.Put("Receiver", "samplerateaud", Parameters.GetAudSampleRate());

    /* Sound card signal sample rate */
    s.Put("Receiver", "sampleratesig", Parameters.GetSoundCardSigSampleRate());

    /* Signal upscale ratio */
    s.Put("Receiver", "sigupratio", Parameters.GetSigUpscaleRatio());

    /* if 0 then only measure PSD when RSCI in use otherwise always measure it */
    s.Put("Receiver", "measurepsdalways", Parameters.bMeasurePSDAlways);

    /* Channel Estimation: Frequency Interpolation */
    s.Put("Receiver", "freqint", GetFrequencyInterpolationAlgorithm());

    /* Channel Estimation: Time Interpolation */
    s.Put("Receiver", "timeint", GetTimeInterpolationAlgorithm());

    /* Time Sync Tracking */
    s.Put("Receiver", "timesync", GetTiSyncTracType());

    /* Flip spectrum flag */
    s.Put("Receiver", "flipspectrum", ReceiveData.GetFlippedSpectrum());

    /* Input channel selection */
    s.Put("Receiver", "inchansel", ReceiveData.GetInChanSel());

    /* Output channel selection */
    s.Put("Receiver", "outchansel",  WriteData.GetOutChanSel());

    /* Mute audio flag */
    s.Put("Receiver", "muteaudio", WriteData.GetMuteAudio());

    /* Reverberation */
    s.Put("Receiver", "reverb", AudioSourceDecoder.GetReverbEffect());

    /* Bandpass filter flag */
    s.Put("Receiver", "filter", FreqSyncAcq.GetRecFilter());

    /* Modified metrics flag */
    s.Put("Receiver", "modmetric", ChannelEstimation.GetIntCons());

    /* Sound In device - don't save files, only devices */
    string indev = ReceiveData.GetSoundInterface();
    if(indev.find(".") == string::npos) {
        s.Put("Receiver", "snddevin", indev);
    }

    /* Sound Out device */
    s.Put("Receiver", "snddevout", WriteData.GetSoundInterface());

    /* Number of iterations for MLC setting */
    s.Put("Receiver", "mlciter", MSCMLCDecoder.GetInitNumIterations());

    /* Tuned Frequency */
    s.Put("Receiver", "frequency", Parameters.GetFrequency());

    /* AM Parameters */

    /* AGC */
    s.Put("AM Demodulation", "agc", AMDemodulation.GetAGCType());

    /* noise reduction */
    s.Put("AM Demodulation", "noisered", AMDemodulation.GetNoiRedType());

#ifdef HAVE_SPEEX
    /* noise reduction level */
    s.Put("AM Demodulation", "noiseredlvl", AMDemodulation.GetNoiRedLevel());
#endif

    /* pll enabled/disabled */
    s.Put("AM Demodulation", "enablepll", AMDemodulation.PLLEnabled());

    /* auto frequency acquisition */
    s.Put("AM Demodulation", "autofreqacq", AMDemodulation.AutoFreqAcqEnabled());

    /* demodulation */
    s.Put("AM Demodulation", "demodulation", AMDemodulation.GetDemodType());

    s.Put("AM Demodulation", "filterbwam", iBwAM);
    s.Put("AM Demodulation", "filterbwusb", iBwUSB);
    s.Put("AM Demodulation", "filterbwlsb", iBwLSB);
    s.Put("AM Demodulation", "filterbwcw", iBwCW);
    s.Put("AM Demodulation", "filterbwfm", iBwFM);

    /* Front-end - combine into Hamlib? */
    s.Put("FrontEnd", "smetercorrectiontype", int(Parameters.FrontEndParameters.eSMeterCorrectionType));

    s.Put("FrontEnd", "smeterbandwidth", int(Parameters.FrontEndParameters.rSMeterBandwidth));

    s.Put("FrontEnd", "defaultmeasurementbandwidth", int(Parameters.FrontEndParameters.rDefaultMeasurementBandwidth));

    s.Put("FrontEnd", "automeasurementbandwidth", Parameters.FrontEndParameters.bAutoMeasurementBandwidth);

    s.Put("FrontEnd", "calfactordrm", int(Parameters.FrontEndParameters.rCalFactorDRM));

    s.Put("FrontEnd", "calfactoram", int(Parameters.FrontEndParameters.rCalFactorAM));

    s.Put("FrontEnd", "ifcentrefrequency", int(Parameters.FrontEndParameters.rIFCentreFreq));

    /* Serial Number */
    s.Put("Receiver", "serialnumber", Parameters.sSerialNumber);

    s.Put("Receiver", "datafilesdirectory", Parameters.GetDataDirectory());

    /* GPS */
    if(Parameters.gps_data.set & LATLON_SET) {
	s.Put("GPS", "latitude", (_REAL) Parameters.gps_data.fix.latitude);
	s.Put("GPS", "longitude", (_REAL) Parameters.gps_data.fix.longitude);
    }
    s.Put("GPS", "usegpsd", Parameters.use_gpsd);
    s.Put("GPS", "host", Parameters.gps_host);
    s.Put("GPS", "port", Parameters.gps_port);
}

void CConvertAudio::InitInternal(CParameter& Parameters)
{
    iInputBlockSize = Parameters.CellMappingTable.iSymbolBlockSize;
    iOutputBlockSize = 2*iInputBlockSize;
    iMaxOutputBlockSize = 2 * int((_REAL) Parameters.GetAudSampleRate() * (_REAL) 0.4 /* 400 ms */);
}

void CConvertAudio::ProcessDataInternal(CParameter& Parameters)
{
    (void)Parameters;
    for (int i = 0; i < this->iInputBlockSize; i++)
    {
        (*this->pvecOutputData)[2*i] = _SAMPLE((*this->pvecInputData)[i]);
        (*this->pvecOutputData)[2*i+1] = _SAMPLE((*this->pvecInputData)[i]);
    }
}

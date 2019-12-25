/******************************************************************************\
 * Technische Universitaet Darmstadt, Institut fuer Nachrichtentechnik
 * Copyright (c) 2001
 *
 * Author(s):
 *	Volker Fischer
 *
 * Description:
 *	DRM-simulation
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

#include "DrmSimulation.h"


/* Implementation *************************************************************/

CDRMSimulation::CDRMSimulation() : iSimTime(0), iSimNumErrors(0),
        rStartSNR(0.0), rEndSNR(0.0), rStepSNR(0.0),
        Parameters(),
        DataBuf(), MLCEncBuf(), IntlBuf(), GenFACDataBuf(), FACMapBuf(), GenSDCDataBuf(),
        SDCMapBuf(), CarMapBuf(), OFDMModBuf(), OFDMDemodBufChan2(), ChanEstInBufSim(),
        ChanEstOutBufChan(),
        RecDataBuf(), ChanResInBuf(), InpResBuf(), FreqSyncAcqBuf(), TimeSyncBuf(),
        OFDMDemodBuf(), SyncUsingPilBuf(), ChanEstBuf(),
        MSCCarDemapBuf(), FACCarDemapBuf(), SDCCarDemapBuf(), DeintlBuf(),
        FACDecBuf(), SDCDecBuf(), MSCMLCDecBuf(), GenSimData(),
        MSCMLCEncoder(), SymbInterleaver(), GenerateFACData(), FACMLCEncoder(),
        GenerateSDCData(), SDCMLCEncoder(), OFDMCellMapping(), OFDMModulation(),
        DRMChannel(), InputResample(), FreqSyncAcq(), TimeSync(), OFDMDemodulation(),
        SyncUsingPil(), ChannelEstimation(), OFDMCellDemapping(), FACMLCDecoder(), UtilizeFACData(),
        SDCMLCDecoder(), UtilizeSDCData(), SymbDeinterleaver(), MSCMLCDecoder(), EvaSimData(),
        OFDMDemodSimulation(), IdealChanEst(), DataConvChanResam()
{
    /* Set all parameters to meaningful value for startup state. If we want to
       make a simulation we just have to specify the important values */
    /* Init streams */
    Parameters.ResetServicesStreams();


    /* Service parameters (only use service 0) ------------------------------- */
    /* Data service */
    Parameters.SetNumOfServices(0,1);

    Parameters.SetAudDataFlag(0,  CService::SF_DATA);

    CDataParam DataParam;
    DataParam.iStreamID = 0;
    DataParam.ePacketModInd = CDataParam::PM_SYNCHRON_STR_MODE;
    Parameters.SetDataParam(0, DataParam);

    //Parameters.SetCurSelDataService(1); /* Service ID must be set for activation */
    Parameters.SetCurSelDataService(0); /* Service ID must be set for activation */

    /* Stream */
    Parameters.SetStreamLen(0, 0, 0); // EEP, if "= 0"


    /* Date, time */
    Parameters.iDay = 0;
    Parameters.iMonth = 0;
    Parameters.iYear = 0;
    Parameters.iUTCHour = 0;
    Parameters.iUTCMin = 0;

    /* Frame IDs */
    Parameters.iFrameIDTransm = 0;
    Parameters.iFrameIDReceiv = 0;

    /* Initialize synchronization parameters */
    Parameters.rResampleOffset = (_REAL) 0.0;
    Parameters.rFreqOffsetAcqui = (_REAL) VIRTUAL_INTERMED_FREQ / Parameters.GetSigSampleRate();
    Parameters.rFreqOffsetTrack = (_REAL) 0.0;
    Parameters.iTimingOffsTrack = 0;

    Parameters.InitCellMapTable(RM_ROBUSTNESS_MODE_B, SO_3);

    Parameters.MSCPrLe.iPartA = 1;
    Parameters.MSCPrLe.iPartB = 1;
    Parameters.MSCPrLe.iHierarch = 0;

    Parameters.eSymbolInterlMode = CParameter::SI_SHORT;
    Parameters.eMSCCodingScheme = CS_3_SM;
    Parameters.eSDCCodingScheme = CS_2_SM;

    /* DRM channel parameters */
    Parameters.iDRMChannelNum = 1;
    Parameters.SetNominalSNRdB(25);
}

void CDRMSimulation::Run()
{
    /*
     The hand over of data is done via an intermediate-buffer. The calling
     convention is always "input-buffer, output-buffer". Additional, the
     DRM-parameters are fed to the function
    */

    /* Initialization of the modules */
    Init();

    /* Set run flag */
    // TODO Parameters.eRunState = CParameter::RUNNING;

    while (true) // TODO Parameters.eRunState == CParameter::RUNNING)
    {
        /**********************************************************************\
        * Transmitter														   *
        \**********************************************************************/
        /* MSC -------------------------------------------------------------- */
        /* Read the source signal */
        GenSimData.ReadData(Parameters, DataBuf);

        /* MLC-encoder */
        MSCMLCEncoder.ProcessData(Parameters, DataBuf, MLCEncBuf);

        /* Convolutional interleaver */
        SymbInterleaver.ProcessData(Parameters, MLCEncBuf, IntlBuf);


        /* FAC -------------------------------------------------------------- */
        GenerateFACData.ReadData(Parameters, GenFACDataBuf);
        FACMLCEncoder.ProcessData(Parameters, GenFACDataBuf, FACMapBuf);


        /* SDC -------------------------------------------------------------- */
        GenerateSDCData.ReadData(Parameters, GenSDCDataBuf);
        SDCMLCEncoder.ProcessData(Parameters, GenSDCDataBuf, SDCMapBuf);


        /* Mapping of the MSC, FAC, SDC and pilots on the carriers */
        OFDMCellMapping.ProcessData(Parameters, IntlBuf, FACMapBuf,
                                    SDCMapBuf, CarMapBuf);

        /* OFDM-modulation */
        OFDMModulation.ProcessData(Parameters, CarMapBuf, OFDMModBuf);



        /**********************************************************************\
        * Channel    														   *
        \**********************************************************************/
        DRMChannel.TransferData(Parameters, OFDMModBuf, RecDataBuf);



        /**********************************************************************\
        * Receiver    														   *
        \**********************************************************************/
        switch (Parameters.eSimType)
        {
        case CParameter::ST_MSECHANEST:
        case CParameter::ST_BER_IDEALCHAN:
        case CParameter::ST_SINR:
            /* MSE of channel estimation, ideal channel estimation -------------- */
            /* Special OFDM demodulation for channel estimation tests (with guard-
               interval removal) */
            OFDMDemodSimulation.ProcessDataOut(Parameters, RecDataBuf,
                                               ChanEstInBufSim, OFDMDemodBufChan2);

            /* Channel estimation and equalization */
            ChannelEstimation.ProcessData(Parameters, ChanEstInBufSim, ChanEstBuf);

            /* Ideal channel estimation (with MSE calculation) */
            IdealChanEst.ProcessDataIn(Parameters, ChanEstBuf, OFDMDemodBufChan2,
                                       ChanEstBuf);
            break;


        default: /* Other types like ST_BITERROR or ST_SYNC_PARAM */
            /* Bit error rate (we can use all synchronization units here!) ------ */
            /* This module converts the "CChanSimDataMod" data type of "DRMChannel"
               to the "_REAL" data type, because a regular module can only have ONE
               type of input buffers */
            DataConvChanResam.ProcessData(Parameters, RecDataBuf, ChanResInBuf);

            /* Resample input DRM-stream */
            InputResample.ProcessData(Parameters, ChanResInBuf, InpResBuf);

            /* Frequency synchronization acquisition */
            FreqSyncAcq.ProcessData(Parameters, InpResBuf, FreqSyncAcqBuf);

            /* Time synchronization */
            TimeSync.ProcessData(Parameters, FreqSyncAcqBuf, TimeSyncBuf);

            /* OFDM-demodulation */
            OFDMDemodulation.ProcessData(Parameters, TimeSyncBuf, OFDMDemodBuf);

            /* Synchronization in the frequency domain (using pilots) */
            SyncUsingPil.ProcessData(Parameters, OFDMDemodBuf, SyncUsingPilBuf);

            /* Channel estimation and equalization */
            ChannelEstimation.ProcessData(Parameters, SyncUsingPilBuf, ChanEstBuf);
            break;
        }

        /* Demapping of the MSC, FAC, SDC and pilots from the carriers */
        OFDMCellDemapping.ProcessData(Parameters, ChanEstBuf,
                                      MSCCarDemapBuf, FACCarDemapBuf, SDCCarDemapBuf);

        /* FAC */
        FACMLCDecoder.ProcessData(Parameters, FACCarDemapBuf, FACDecBuf);
        UtilizeFACData.WriteData(Parameters, FACDecBuf);


        /* SDC */
        SDCMLCDecoder.ProcessData(Parameters, SDCCarDemapBuf, SDCDecBuf);
        UtilizeSDCData.WriteData(Parameters, SDCDecBuf);


        /* MSC */
        /* Symbol de-interleaver */
        SymbDeinterleaver.ProcessData(Parameters, MSCCarDemapBuf, DeintlBuf);

        /* MLC-decoder */
        MSCMLCDecoder.ProcessData(Parameters, DeintlBuf, MSCMLCDecBuf);

        /* Evaluate simulation data */
        EvaSimData.WriteData(Parameters, MSCMLCDecBuf);
    }
}

void CDRMSimulation::Init()
{
    /* Defines number of cells, important! */
    OFDMCellMapping.Init(Parameters, CarMapBuf);

    /* Defines number of SDC bits per super-frame */
    SDCMLCEncoder.Init(Parameters, SDCMapBuf);

    MSCMLCEncoder.Init(Parameters, MLCEncBuf);
    SymbInterleaver.Init(Parameters, IntlBuf);
    GenerateFACData.Init(Parameters, GenFACDataBuf);
    FACMLCEncoder.Init(Parameters, FACMapBuf);
    GenerateSDCData.Init(Parameters, GenSDCDataBuf);
    OFDMModulation.Init(Parameters, OFDMModBuf);
    GenSimData.Init(Parameters, DataBuf);


    /* Receiver modules */
    /* The order of modules are important! */
    InputResample.Init(Parameters, InpResBuf);
    FreqSyncAcq.Init(Parameters, FreqSyncAcqBuf);
    TimeSync.Init(Parameters, TimeSyncBuf);
    SyncUsingPil.Init(Parameters, SyncUsingPilBuf);

    /* Channel estimation init must be called before OFDMDemodSimulation
       module, because the delay is set here which the other modules use! */
    ChannelEstimation.Init(Parameters, ChanEstBuf);

    OFDMCellDemapping.Init(Parameters, MSCCarDemapBuf, FACCarDemapBuf,
                           SDCCarDemapBuf);
    FACMLCDecoder.Init(Parameters, FACDecBuf);
    UtilizeFACData.Init(Parameters);
    SDCMLCDecoder.Init(Parameters, SDCDecBuf);
    UtilizeSDCData.Init(Parameters);
    SymbDeinterleaver.Init(Parameters, DeintlBuf);
    MSCMLCDecoder.Init(Parameters, MSCMLCDecBuf);


    /* Special module for simulation */
    EvaSimData.Init(Parameters);

    /* Init channel. The channel must be initialized before the modules
       "OFDMDemodSimulation" and "IdealChanEst" because they need iNumTaps and
       tap delays in global struct */
    DRMChannel.Init(Parameters, RecDataBuf);

    /* Mode dependent initializations */
    switch (Parameters.eSimType)
    {
    case CParameter::ST_MSECHANEST:
    case CParameter::ST_BER_IDEALCHAN:
    case CParameter::ST_SINR:
        /* Init OFDM demod before IdealChanEst, because the timing offset of
           useful part extraction is set here */
        OFDMDemodSimulation.Init(Parameters, ChanEstInBufSim, OFDMDemodBufChan2);

        /* Problem: "ChanEstBuf" is used for input and output buffer. That only
           works with single buffers. This solution works for this case but is
           not a very nice solution FIXME */
        IdealChanEst.Init(Parameters, ChanEstBuf);
        break;

    default: /* Other types like ST_BITERROR or ST_SYNC_PARAM */
        DataConvChanResam.Init(Parameters, ChanResInBuf);

        OFDMDemodulation.Init(Parameters, OFDMDemodBuf);
        break;
    }


    /* Clear all buffers ---------------------------------------------------- */
    /* The buffers must be cleared in case there is some data left in the
       buffers from the last simulation (with, e.g., different SNR) */
    DataBuf.Clear();
    MLCEncBuf.Clear();
    IntlBuf.Clear();
    GenFACDataBuf.Clear();
    FACMapBuf.Clear();
    GenSDCDataBuf.Clear();
    SDCMapBuf.Clear();
    CarMapBuf.Clear();
    OFDMModBuf.Clear();
    OFDMDemodBufChan2.Clear();
    ChanEstInBufSim.Clear();
    ChanEstOutBufChan.Clear();
    RecDataBuf.Clear();
    ChanResInBuf.Clear();
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


    /* We only want to simulate tracking performance ------------------------ */
    TimeSync.StopTimingAcqu();
    TimeSync.StopRMDetAcqu(); /* Robustness mode detection */
    ChannelEstimation.GetTimeSyncTrack()->StartTracking();

    /* We stop tracking of time wiener interpolation since during acquisition,
       no automatic update of the statistic estimates is done. We need that
       because we set the correct parameters once in the init routine */
    ChannelEstimation.GetTimeWiener()->StopTracking();

    /* Disable FAC evaluation to make sure that no mistakenly correct CRC
       sets false parameters which can cause run-time errors */
    UtilizeFACData.SetSyncInput(true);

    /* We have to first start aquisition and then stop it right after it to set
       internal parameters */
    SyncUsingPil.StartAcquisition();
    SyncUsingPil.StopAcquisition();

    SyncUsingPil.StartTrackPil();
}


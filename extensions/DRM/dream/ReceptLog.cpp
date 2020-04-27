/******************************************************************************\
 * Technische Universitaet Darmstadt, Institut fuer Nachrichtentechnik
 * Copyright (c) 2007
 *
 * Author(s):
 *	Volker Fischer, Andrew Murphy, Julian Cable
 *
 * Description:
 *	see ReceptLog.h
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

#include "Version.h"
#include "ReceptLog.h"
#include <iomanip>
#include <iostream>
#include "matlib/MatlibStdToolbox.h"

/* implementation --------------------------------------------- */

CReceptLog::CReceptLog(CParameter & p):Parameters(p), File(), bLogActivated(false),
    bRxlEnabled(false), bPositionEnabled(false),
    iSecDelLogStart(0)
{
    iFrequency = Parameters.GetFrequency();
    latitude = Parameters.gps_data.fix.latitude;
    longitude = Parameters.gps_data.fix.longitude;
}

void
CReceptLog::Start(const string & filename)
{
    File.open(filename.c_str(), ios::app);
    if (File.is_open())
    {
        bLogActivated = true;
        writeHeader();
    }
    init();
}

void
CReceptLog::Stop()
{
    if (!bLogActivated)
        return;
    writeTrailer();
    File.close();
    bLogActivated = false;
}

void
CReceptLog::Update()
{
    if (!bLogActivated)
        return;
    writeParameters();
}

bool CReceptLog::restartNeeded()
{
    int iCurrentFrequency = Parameters.GetFrequency();
    double currentLatitude = Parameters.gps_data.fix.latitude;
    double currentLongitude = Parameters.gps_data.fix.longitude;
    if((iCurrentFrequency != iFrequency)
            || (bPositionEnabled && int(currentLatitude) != int(latitude))
            || (bPositionEnabled && int(currentLongitude) != int(longitude))
      )
    {
        iFrequency = iCurrentFrequency;
        latitude = currentLatitude;
        longitude = currentLongitude;
        return true;
    }
    return false;
}

/* Get robustness mode string */
char
CReceptLog::GetRobModeStr()
{
    char chRobMode = 'X';
    switch (Parameters.GetWaveMode())
    {
    case RM_ROBUSTNESS_MODE_A:
        chRobMode = 'A';
        break;

    case RM_ROBUSTNESS_MODE_B:
        chRobMode = 'B';
        break;

    case RM_ROBUSTNESS_MODE_C:
        chRobMode = 'C';
        break;

    case RM_ROBUSTNESS_MODE_D:
        chRobMode = 'D';
        break;

    case RM_ROBUSTNESS_MODE_E:
        chRobMode = 'E';
        break;

    case RM_NO_MODE_DETECTED:
        chRobMode = 'X';
        break;
    }
    return chRobMode;
}

void
CShortLog::init()
{
    Parameters.Lock();
    Parameters.ReceiveStatus.FAC.ResetCounts();
    Parameters.ReceiveStatus.SLAudio.ResetCounts();
    Parameters.Unlock();
    /* initialise the minute count */
    iCount = 0;
}

void
CShortLog::writeHeader()
{
    string latitude, longitude, label, RobMode;
    ESpecOcc SpecOcc=SO_5;
    _REAL bitrate = 0.0;

    Parameters.Lock();
    iFrequency = Parameters.GetFrequency();

    if (Parameters.gps_data.set & LATLON_SET)
    {
        asDM(latitude, Parameters.gps_data.fix.latitude, 'S', 'N');
        asDM(longitude, Parameters.gps_data.fix.longitude, 'W', 'E');
    }
    int iCurSelServ = Parameters.GetCurSelAudioService();

    if (Parameters.Service[iCurSelServ].IsActive())
    {
        /* Service label (UTF-8 encoded string -> convert ? TODO locale) */
        label = Parameters.Service[iCurSelServ].strLabel;
        bitrate = Parameters.GetBitRateKbps(iCurSelServ, false);
        RobMode = GetRobModeStr();
        SpecOcc = Parameters.GetSpectrumOccup();
    }

    Parameters.Unlock();

    if (!File.is_open())
        return; /* allow updates when file closed */

    /* Beginning of new table (similar to DW DRM log file) */
    File << endl << ">>>>" << endl << "Dream" << endl << "Software Version ";
    if (dream_version_patch == 0)
        File << dream_version_major << "." << dream_version_minor << dream_version_build << endl;
    else
        File << dream_version_major << "." << dream_version_minor << "." << dream_version_patch << dream_version_build << endl;

    time_t now;
    (void) time(&now);
    File << "Starttime (UTC)  " << strdate(now) << " " << strtime(now) << endl;

    File << "Frequency        ";
    if (iFrequency != 0)
        File << iFrequency << " kHz";
    File << endl;

    if (latitude != "")
    {
        File << "Latitude         " << latitude << endl;
        File << "Longitude        " << longitude << endl;
    }

    /* Write additional text */

    /* First get current selected audio service */

    /* Check whether service parameters were not transmitted yet */
    if (RobMode != "")
    {
        /* Service label (UTF-8 encoded string -> convert ? TODO locale) */
        File << fixed << setprecision(2);
        File << "Label            " << label << endl;
        File << "Bitrate          " << setw(4) << bitrate << " kbps" << endl;
        File << "Mode             " << RobMode << endl;
        File << "Bandwidth        ";
        switch (SpecOcc)
        {
        case SO_0:
            File << "4,5 kHz";
            break;

        case SO_1:
            File << "5 kHz";
            break;

        case SO_2:
            File << "9 kHz";
            break;

        case SO_3:
            File << "10 kHz";
            break;

        case SO_4:
            File << "18 kHz";
            break;

        case SO_5:
            File << "20 kHz";
            break;

        default:
            File << "10 kHz";
        }
        File << endl;
    }

    File << endl << "MINUTE  SNR     SYNC    AUDIO     TYPE";
    if (bRxlEnabled)
        File << "      RXL";
    File << endl;

    iCount = 0; // start count each time a new header is put
}

void
CShortLog::writeParameters()
{
    Parameters.Lock();

    int iAverageSNR = (int) Round(Parameters.SNRstat.getMean());
    int iNumCRCOkFAC = Parameters.ReceiveStatus.FAC.GetOKCount();
    int iNumCRCOkMSC = Parameters.ReceiveStatus.SLAudio.GetOKCount();

    Parameters.ReceiveStatus.FAC.ResetCounts();
    Parameters.ReceiveStatus.SLAudio.ResetCounts();

    int iTmpNumAAC=0, iRXL=0;

    /* If no sync, do not print number of AAC frames.
     * If the number of correct FAC CRCs is lower than 10%,
     * we assume that receiver is not synchronized */
    if (iNumCRCOkFAC >= 15)
        iTmpNumAAC = Parameters.iNumAudioFrames;

    if (bRxlEnabled)
        iRXL = (int)Round(Parameters.SigStrstat.getMean()+S9_DBUV);

    Parameters.Unlock();

    int count = iCount++;

    if (!File.is_open())
        return; /* allow updates when file closed */

    try
    {
        File << "  " << fixed << setw(4) << setfill('0') << count
             << setfill(' ') << setw(5) << iAverageSNR
             << setw(9) << iNumCRCOkFAC
             << setw(6) << iNumCRCOkMSC << "/" << setw(2) << setfill('0') << iTmpNumAAC
             << setfill(' ') << "      0";
        if (bRxlEnabled)
        {
            File << setw(10) << setprecision(2) << iRXL;
        }
        File << endl;
        File.flush();
    }
    catch (...)
    {
        /* To prevent errors if user views the file during reception */
    }
}

void
CShortLog::writeTrailer()
{

    _REAL rMaxSNR, rMinSNR;
    _REAL rMaxSigStr=0.0, rMinSigStr=0.0;

    Parameters.Lock();
    Parameters.SNRstat.getMinMax(rMinSNR, rMaxSNR);
    if (bRxlEnabled)
    {
        Parameters.SigStrstat.getMinMax(rMinSigStr, rMaxSigStr);
        rMinSigStr+=S9_DBUV;
        rMaxSigStr+=S9_DBUV;
    }
    Parameters.Unlock();

    if (!File.is_open())
        return; /* allow updates when file closed */

    File << fixed << setprecision(1);
    File << endl << "SNR min: " << setw(4) << rMinSNR << ", max: " << setw(4) << rMaxSNR << endl;

    if (bRxlEnabled)
    {
        File << "RXL min: " << setw(4) << rMinSigStr << ", max: " << setw(4) << rMaxSigStr << endl;
    }

    /* Short log file ending */
    File << "CRC: " << endl << "<<<<" << endl << endl;
}

void
CLongLog::writeHeader()
{
    if (!File.is_open())
        return; /* allow updates when file closed */

    File <<
         "FREQ/MODE/QAM PL:ABH,       DATE,       TIME,    SNR, SYNC, FAC, MSC, AUDIO, AUDIOOK, DOPPLER, DELAY";
    if (bRxlEnabled)
        File << ",     RXL";
    if (bPositionEnabled)
        File << ",  LATITUDE, LONGITUDE";
#ifdef _DEBUG_
    /* In case of debug mode, use more parameters */
    File << ",    DC-FREQ, SAMRATEOFFS";
#endif
    File << endl;
}

void
CLongLog::init()
{
    Parameters.Lock();
    Parameters.ReceiveStatus.LLAudio.ResetCounts();
    Parameters.Unlock();
}

void
CLongLog::writeParameters()
{

    Parameters.Lock();
    iFrequency = Parameters.GetFrequency();

    /* Get parameters for delay and Doppler. In case the receiver is
       not synchronized, set parameters to zero */
    _REAL rDelay = (_REAL) 0.0;
    _REAL rDoppler = (_REAL) 0.0;
    if (Parameters.GetAcquiState() == AS_WITH_SIGNAL)
    {
        rDelay = Parameters.rMinDelay;
        rDoppler = Parameters.rSigmaEstimate;
    }

    /* Only show mode if FAC CRC was ok */
    int iCurProtLevPartA = 0;
    int iCurProtLevPartB = 0;
    int iCurProtLevPartH = 0;
    int iCurMSCSc = 0;

    if (Parameters.ReceiveStatus.FAC.GetStatus() == RX_OK)
    {
        /* Copy protection levels */
        iCurProtLevPartA = Parameters.MSCPrLe.iPartA;
        iCurProtLevPartB = Parameters.MSCPrLe.iPartB;
        iCurProtLevPartH = Parameters.MSCPrLe.iHierarch;
        switch (Parameters.eMSCCodingScheme)
        {
        case CS_3_SM:
            iCurMSCSc = 0;
            break;

        case CS_3_HMMIX:
            iCurMSCSc = 1;
            break;

        case CS_3_HMSYM:
            iCurMSCSc = 2;
            break;

        case CS_2_SM:
            iCurMSCSc = 3;
            break;

        case CS_1_SM:			/* TODO */
            break;
        }
    }

    char cRobMode = GetRobModeStr();

    _REAL rSNR = Parameters.SNRstat.getCurrent();
    int iFrameSyncStatus = (Parameters.ReceiveStatus.FSync.GetStatus()==RX_OK)?1:0;
    int iFACStatus = (Parameters.ReceiveStatus.FAC.GetStatus()==RX_OK)?1:0;
    int iAudioStatus = (Parameters.ReceiveStatus.LLAudio.GetStatus()==RX_OK)?1:0;
    int iNumCRCMSC = Parameters.ReceiveStatus.LLAudio.GetCount();
    int iNumCRCOkMSC = Parameters.ReceiveStatus.LLAudio.GetOKCount();

    Parameters.ReceiveStatus.LLAudio.ResetCounts();

    double latitude=0.0, longitude=0.0;
    if (bPositionEnabled && (Parameters.gps_data.set & LATLON_SET))
    {
        latitude = Parameters.gps_data.fix.latitude;
        longitude = Parameters.gps_data.fix.longitude;
    }

    Parameters.Unlock();

    if (!File.is_open())
        return; /* allow updates when file closed */

    try
    {
        time_t now;
        (void) time(&now);
        File << fixed << setprecision(2)
             << setw(5) << iFrequency << '/' << setw(1) << cRobMode
             << iCurMSCSc << iCurProtLevPartA << iCurProtLevPartB << iCurProtLevPartH << "         ,"
             << " " << strdate(now) << ", "
             << strtime(now) << ".0" << ","
             << setw(7) << rSNR << ","
             << setw(5) << iFrameSyncStatus << ","
             << setw(4) << iFACStatus << ","
             << setw(4) << iAudioStatus << ","
             << setw(6) << iNumCRCMSC << ","
             << setw(8) << iNumCRCOkMSC << ","
             << "  " << setw(6) << rDoppler << ','
             << setw(6) << rDelay;

        if (bRxlEnabled)
            File << ',' << setprecision(2) << setw(8) << Parameters.SigStrstat.getCurrent()+S9_DBUV;

        if (bPositionEnabled)
            File << ',' << setprecision(4) << setw(10) << latitude << ',' << setw(10) << longitude;

#ifdef _DEBUG_
        /* Some more parameters in debug mode */
        Parameters.Lock();
        File << Parameters.GetDCFrequency() << ',' << Parameters.GetSampFreqEst();
        Parameters.Unlock();
#endif
        File << endl;
        File.flush();
    }

    catch (...)
    {
        /* To prevent errors if user views the file during reception */
    }

}

void
CLongLog::writeTrailer()
{
    if (!File.is_open())
        return; /* allow updates when file closed */

    File << endl << endl;
}

string CReceptLog::strdate(time_t t)
{
    struct tm * today;
    stringstream s;

    today = gmtime(&t);		/* Always UTC */

    s << setfill('0')
      << setw(4) << today->tm_year + 1900 << "-"
      << setw(2) << today->tm_mon + 1 << "-" << setw(2) << today->tm_mday;
    return s.str();
}

string CReceptLog::strtime(time_t t)
{
    struct tm * today;
    stringstream s;

    today = gmtime(&t);		/* Always UTC */

    s << setfill('0')
      << setw(2) << today->tm_hour << ":" << setw(2) << today-> tm_min << ":" << setw(2) << today->tm_sec;
    return s.str();
}

void
CReceptLog::asDM(string& pos, double d, char n, char p) const
{
    stringstream s;
    char np;
    int ideg;
    double dmin;
    if (d<0.0)
    {
        np = n;
        ideg = 0 - int(d);
        dmin = 0.0 - d;
    }
    else
    {
        np = p;
        ideg = int(d);
        dmin = d;
    }
    int degrees = (unsigned int) dmin;
    uint16_t minutes = (unsigned int) (((floor((dmin - degrees) * 1000000) / 1000000) + 0.00005) * 60.0);
    s << ideg << '\xb0' << setw(2) << setfill('0') << minutes << "'" << np;
    pos = s.str();
}

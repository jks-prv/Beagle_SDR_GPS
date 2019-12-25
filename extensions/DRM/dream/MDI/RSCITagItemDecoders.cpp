/******************************************************************************\
 * Technische Universitaet Darmstadt, Institut fuer Nachrichtentechnik
 * Copyright (c) 2004
 *
 * Author(s):
 *	Volker Fischer, Julian Cable, Oliver Haffenden
 *
 * Description:
 *	Implements Digital Radio Mondiale (DRM) Multiplex Distribution Interface
 *	(MDI), Receiver Status and Control Interface (RSCI)
 *  and Distribution and Communications Protocol (DCP) as described in
 *	ETSI TS 102 820,  ETSI TS 102 349 and ETSI TS 102 821 respectively.
 *
 *  This module derives, from the CTagItemDecoder base class, tag item decoders specialised to decode each of the tag
 *  items defined in the control part of RSCI.
 *  Decoded commands are generally sent straight to the CDRMReceiver object which
 *	they hold a pointer to.
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


#include "RSCITagItemDecoders.h"
#include "../DRMReceiver.h"
#include <time.h>
#include <stdlib.h>

/* RX_STAT Items */

_REAL CTagItemDecoderRSI::decodeDb(CVector<_BINARY>& vecbiTag)
{
    int8_t n = (int8_t)vecbiTag.Separate(8);
    uint8_t m = (uint8_t)vecbiTag.Separate(8);
    return _REAL(n)+_REAL(m)/256.0;
}

void CTagItemDecoderRdbv::DecodeTag(CVector<_BINARY>& vecbiTag, const int iLen)
{
    if (iLen != 16)
        return;
    _REAL rSigStr = decodeDb(vecbiTag);
    pParameter->SigStrstat.addSample(rSigStr);
    /* this is the only signal strength we have so update the IF level too.
     * TODO scaling factor ? */
    pParameter->SetIFSignalLevel(rSigStr);
}

void CTagItemDecoderRsta::DecodeTag(CVector<_BINARY>& vecbiTag, const int iLen)
{
    if (iLen != 32)
        return;
    uint8_t sync = uint8_t(vecbiTag.Separate(8));
    uint8_t fac = uint8_t(vecbiTag.Separate(8));
    uint8_t sdc = uint8_t(vecbiTag.Separate(8));
    uint8_t audio = uint8_t(vecbiTag.Separate(8));
    pParameter->ReceiveStatus.TSync.SetStatus((sync==0)?RX_OK:CRC_ERROR);
    pParameter->ReceiveStatus.FAC.SetStatus((fac==0)?RX_OK:CRC_ERROR);
    pParameter->ReceiveStatus.SDC.SetStatus((sdc==0)?RX_OK:CRC_ERROR);

    unsigned iShortID = unsigned(pParameter->GetCurSelAudioService());
    pParameter->AudioComponentStatus[iShortID].SetStatus((audio==0)?RX_OK:CRC_ERROR);
}

void CTagItemDecoderRwmf::DecodeTag(CVector<_BINARY>& vecbiTag, const int iLen)
{
    if (iLen != 16)
        return;
    pParameter->rWMERFAC = decodeDb(vecbiTag);
}

void CTagItemDecoderRwmm::DecodeTag(CVector<_BINARY>& vecbiTag, const int iLen)
{
    if (iLen != 16)
        return;
    pParameter->rWMERMSC = decodeDb(vecbiTag);
}

void CTagItemDecoderRmer::DecodeTag(CVector<_BINARY>& vecbiTag, const int iLen)
{
    if (iLen != 16)
        return;
    pParameter->rMER = decodeDb(vecbiTag);
}

void CTagItemDecoderRdop::DecodeTag(CVector<_BINARY>& vecbiTag, const int iLen)
{
    if (iLen != 16)
        return;
    pParameter->rRdop = decodeDb(vecbiTag);
}

void CTagItemDecoderRdel::DecodeTag(CVector<_BINARY>& vecbiTag, const int iLen)
{
    int iNumEntries = iLen/(3*SIZEOF__BYTE);
    pParameter->vecrRdelIntervals.Init(iNumEntries);
    pParameter->vecrRdelThresholds.Init(iNumEntries);

    for (int i=0; i<iNumEntries; i++)
    {
        pParameter->vecrRdelThresholds[i] = vecbiTag.Separate(SIZEOF__BYTE);
        pParameter->vecrRdelIntervals[i] = decodeDb(vecbiTag);
    }
}

void CTagItemDecoderRpsd::DecodeTag(CVector<_BINARY>& vecbiTag, const int iLen)
{
    if (iLen != 680 && iLen !=1112)
        return;

    int iVectorLen = iLen/SIZEOF__BYTE;

    pParameter->vecrPSD.Init(iVectorLen);

    for (int i = 0; i < iVectorLen; i++)
    {
        pParameter->vecrPSD[i] = -(_REAL(vecbiTag.Separate(SIZEOF__BYTE))/_REAL(2.0));
    }

}

void CTagItemDecoderRpir::DecodeTag(CVector<_BINARY>& vecbiTag, const int iLen)
{
    const _REAL rOffset = _REAL(-60.0);

    if (iLen == 0)
    {
        pParameter->vecrPIR.Init(0);
        return;
    }

    int iVectorLen = iLen/SIZEOF__BYTE - 4; // 4 bytes for the scale start and end

    pParameter->rPIRStart = _REAL(int16_t(vecbiTag.Separate(2 * SIZEOF__BYTE))) / _REAL(256.0);
    pParameter->rPIREnd = _REAL(int16_t(vecbiTag.Separate(2 * SIZEOF__BYTE))) / _REAL(256.0);

    pParameter->vecrPIR.Init(iVectorLen);

    for (int i = 0; i < iVectorLen; i++)
    {
        pParameter->vecrPIR[i] = -(_REAL(vecbiTag.Separate(SIZEOF__BYTE))/_REAL(2.0)) - rOffset;
    }
}

void CTagItemDecoderRgps::DecodeTag(CVector<_BINARY>& vecbiTag, const int iLen)
{
    if (iLen != 26 * SIZEOF__BYTE)
        return;

    gps_data_t& gps_data = pParameter->gps_data;

    gps_data.set=0;
    uint16_t source = (uint16_t)vecbiTag.Separate(SIZEOF__BYTE);
    switch(source)
    {
    case 0:
        gps_data.set=0;
        break;
    case 1:
        gps_data.set=STATUS_SET;
        gps_data.status=1;
        break;
    case 2:
        gps_data.set=STATUS_SET;
        gps_data.status=2;
        break;
    case 3:
        gps_data.set=STATUS_SET;
        gps_data.status=0;
        break;
    case 0xff:
        gps_data.set=0;
        gps_data.status=0;
        break;
    default:
        cerr << "error decoding rgps" << endl;
    }

    uint8_t nSats = (uint8_t)vecbiTag.Separate(SIZEOF__BYTE);
    if(nSats != 0xff)
    {
        gps_data.set |= SATELLITE_SET;
        gps_data.satellites_used = nSats;
    }

    uint16_t val;
    val = uint16_t(vecbiTag.Separate(2 * SIZEOF__BYTE));
    int16_t iLatitudeDegrees = *(int16_t*)&val;
    uint8_t uiLatitudeMinutes = (uint8_t)vecbiTag.Separate(SIZEOF__BYTE);
    uint16_t uiLatitudeMinuteFractions = (uint16_t)vecbiTag.Separate(2 * SIZEOF__BYTE);
    val = uint16_t(vecbiTag.Separate(2 * SIZEOF__BYTE));
    int16_t iLongitudeDegrees = *(int16_t*)&val;
    uint8_t uiLongitudeMinutes = (uint8_t)vecbiTag.Separate(SIZEOF__BYTE);
    uint16_t uiLongitudeMinuteFractions = (uint16_t)vecbiTag.Separate(2 * SIZEOF__BYTE);
    if(uiLatitudeMinutes != 0xff)
    {
        gps_data.fix.latitude = double(iLatitudeDegrees)
                   + (double(uiLatitudeMinutes) + double(uiLatitudeMinuteFractions)/65536.0)/60.0;
        gps_data.fix.longitude = double(iLongitudeDegrees)
                    + (double(uiLongitudeMinutes) + double(uiLongitudeMinuteFractions)/65536.0)/60.0;
        gps_data.set |= LATLON_SET;
    }

    val = uint16_t(vecbiTag.Separate(2 * SIZEOF__BYTE));
    uint8_t uiAltitudeMetreFractions = (uint8_t)vecbiTag.Separate(SIZEOF__BYTE);
    if(val != 0xffff)
    {
        uint16_t iAltitudeMetres = *(int16_t*)&val;
        gps_data.fix.altitude = double(iAltitudeMetres+uiAltitudeMetreFractions)/256.0;
        gps_data.set |= ALTITUDE_SET;
    }

    struct tm tm;
    tm.tm_hour = uint8_t(vecbiTag.Separate(SIZEOF__BYTE));
    tm.tm_min = uint8_t(vecbiTag.Separate(SIZEOF__BYTE));
    tm.tm_sec = uint8_t(vecbiTag.Separate(SIZEOF__BYTE));
    uint16_t year = uint16_t(vecbiTag.Separate(2*SIZEOF__BYTE));
    tm.tm_year = year - 1900;
    tm.tm_mon = uint8_t(vecbiTag.Separate(SIZEOF__BYTE))-1;
    tm.tm_mday = uint8_t(vecbiTag.Separate(SIZEOF__BYTE));

    if(tm.tm_hour != 0xff)
    {
        string se;
        char *e = getenv("TZ");
        if(e)
            se = e;
#ifdef _WIN32
        _putenv("TZ=UTC");
        _tzset();
        time_t t = mktime(&tm);
        stringstream ss("TZ=");
        ss << se;
        _putenv(ss.str().c_str());
#else
        putenv(const_cast<char*>("TZ=UTC"));
        tzset();
        time_t t = mktime(&tm);
        if(e)
            setenv("TZ", se.c_str(), 1);
        else
            unsetenv("TZ");
#endif
        gps_data.fix.time = t;
        gps_data.set |= TIME_SET;
    }

    uint16_t speed = (uint16_t)vecbiTag.Separate(2*SIZEOF__BYTE);
    if(speed != 0xffff)
    {
        gps_data.fix.speed = double(speed)/10.0;
        gps_data.set |= SPEED_SET;
    }

    uint16_t track = (uint16_t)vecbiTag.Separate(2*SIZEOF__BYTE);
    if(track != 0xffff)
    {
        gps_data.fix.track = track;
        gps_data.set |= TRACK_SET;
    }
}

/* RX_CTRL Items */

void CTagItemDecoderCact::DecodeTag(CVector<_BINARY>& vecbiTag, const int iLen)
{
    if (iLen != 8)
        return;

    const int iNewState = vecbiTag.Separate(8) - '0';

    if (pDRMReceiver == nullptr)
        return;

    // TODO pDRMReceiver->SetState(iNewState);
    (void)iNewState;

}

void CTagItemDecoderCfre::DecodeTag(CVector<_BINARY>& vecbiTag, const int iLen)
{
    if (iLen != 32)
        return;

    if (pDRMReceiver == nullptr)
        return;

    const int iNewFrequency = vecbiTag.Separate(32);
    pDRMReceiver->SetFrequency(iNewFrequency/1000);

}

void CTagItemDecoderCdmo::DecodeTag(CVector<_BINARY>& vecbiTag, const int iLen)
{
    if (iLen != 32)
        return;

    string s = "";
    for (int i = 0; i < iLen / SIZEOF__BYTE; i++)
        s += (_BYTE) vecbiTag.Separate(SIZEOF__BYTE);

    if (pDRMReceiver == nullptr)
        return;

    if(s == "drm_")
        pDRMReceiver->SetReceiverMode(RM_DRM);
    if(s == "am__")
        pDRMReceiver->SetReceiverMode(RM_AM);
    if(s == "fm__")
        pDRMReceiver->SetReceiverMode(RM_FM);
}

void CTagItemDecoderCrec::DecodeTag(CVector<_BINARY>& vecbiTag, const int iLen)
{
    if (iLen != 32)
        return;

    string s = "";
    for (int i = 0; i < 2; i++)
        s += (_BYTE) vecbiTag.Separate(SIZEOF__BYTE);
    char c3 = (char) vecbiTag.Separate(SIZEOF__BYTE);
    char c4 = (char) vecbiTag.Separate(SIZEOF__BYTE);

    if (pDRMReceiver == nullptr)
        return;

    if(s == "st")
        pDRMReceiver->SetRSIRecording(c4=='1', c3);
    if(s == "iq")
        pDRMReceiver->SetIQRecording(c4=='1');
}

void CTagItemDecoderCpro::DecodeTag(CVector<_BINARY>& vecbiTag, const int iLen)
{
    if (iLen != 8)
        return;

    char c = char(vecbiTag.Separate(SIZEOF__BYTE));
    if (pRSISubscriber != nullptr)
        pRSISubscriber->SetProfile(c);
}
/* TODO: other control tag items */

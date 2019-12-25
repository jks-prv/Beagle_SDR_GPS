/******************************************************************************\
 *
 * Copyright (c) 2013
 *
 * Author(s):
 *  David Flamand
 *
 * Description:
 *  Module for interacting with console, no dependency to ncurses for
 *  increased portability with various handheld devices and dev boards,
 *  compatible with ANSI terminal.
 *  For best results stderr should be redirected to file or /dev/null
 *
 * Currently defined Keys:
 *  SPACEBAR  switch between signal info and service info
 *  F         display both signal info and service info
 *  N         toggle no display
 *  1 2 3 4   audio service selection
 *  CTRL-C Q  quit
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

#include "DRM_main.h"

#include <unistd.h>
#include <cstdio>
//#include <QString>
#include "ConsoleIO.h"
using namespace std;

#if defined(__APPLE__)
 #include <stdio.h>
 #include <sys/time.h>
#endif

#define MINIMUM_IF_LEVEL -200.0

#define NA "---"


/* Implementation *************************************************************/

CDRMReceiver* CConsoleIO::pDRMReceiver;
unsigned long long CConsoleIO::time;


void
CConsoleIO::Enter(CDRMReceiver* pDRMReceiver)
{
	CConsoleIO::pDRMReceiver = pDRMReceiver;
}

void
CConsoleIO::Leave()
{
}

ERunState
CConsoleIO::Update(drm_t *drm)
{
	CParameter& Parameters = *pDRMReceiver->GetParameters();

    #if defined(__APPLE__)
	    struct timeval tv;
        if (gettimeofday(&tv, NULL) == -1) {
            printf("gettimeofday error\n");
            return RUNNING;
        }
	    unsigned long long curtime = (unsigned long long)tv.tv_sec*1000 + (unsigned long long)tv.tv_usec/1000;
    #else
	    struct timespec ts;
        if (clock_gettime(CLOCK_MONOTONIC, &ts) == -1)
            return RUNNING;
        unsigned long long curtime = (unsigned long long)ts.tv_sec*1000 + (unsigned long long)ts.tv_nsec/1000000;
    #endif
    
	if ((curtime - time) < GUI_CONTROL_UPDATE_TIME) {
	//if ((curtime - time) < 250) {
        //printf("%llu ", curtime - time); fflush(stdout);
		return RUNNING;
	}
    //printf("."); fflush(stdout);
	time = curtime;
	
	#if 1
        CVector<_COMPLEX> facIQ, sdcIQ, mscIQ;
        pDRMReceiver->GetFACMLC()->GetVectorSpace(facIQ);
        pDRMReceiver->GetSDCMLC()->GetVectorSpace(sdcIQ);
        pDRMReceiver->GetMSCMLC()->GetVectorSpace(mscIQ);
        #define N_IQDATA (64*2 + 256*2 + 2048*2)
        u1_t u1[N_IQDATA];
        int i;
        for (i=0; i < 64; i++) {
            u1[i*2]   = (facIQ[i].real() + 1.5) * 255.0 / 3.0;
            u1[i*2+1] = (facIQ[i].imag() + 1.5) * 255.0 / 3.0;
        }
        for (; i < 64+256; i++) {
            u1[i*2]   = (sdcIQ[i].real() + 1.5) * 255.0 / 3.0;
            u1[i*2+1] = (sdcIQ[i].imag() + 1.5) * 255.0 / 3.0;
        }
        for (; i < 64+256+2048; i++) {
            u1[i*2]   = (mscIQ[i].real() + 1.5) * 255.0 / 3.0;
            u1[i*2+1] = (mscIQ[i].imag() + 1.5) * 255.0 / 3.0;
        }
        DRM_data(drm, (u1_t) DRM_DAT_IQ, u1, N_IQDATA);
        //printf("%d|%d|%d ", facIQ.Size(), sdcIQ.Size(), mscIQ.Size()); fflush(stdout);
    #endif

	char *sb;

    Parameters.Lock();

    int msc = ETypeRxStatus2int(Parameters.ReceiveStatus.SLAudio.GetStatus());
    int sdc = ETypeRxStatus2int(Parameters.ReceiveStatus.SDC.GetStatus());
    int fac = ETypeRxStatus2int(Parameters.ReceiveStatus.FAC.GetStatus());
    int time = ETypeRxStatus2int(Parameters.ReceiveStatus.TSync.GetStatus());
    int frame = ETypeRxStatus2int(Parameters.ReceiveStatus.FSync.GetStatus());
    ETypeRxStatus soundCardStatusI = Parameters.ReceiveStatus.InterfaceI.GetStatus(); /* Input */
    ETypeRxStatus soundCardStatusO = Parameters.ReceiveStatus.InterfaceO.GetStatus(); /* Output */
    int inter = ETypeRxStatus2int(soundCardStatusO == NOT_PRESENT ||
		(soundCardStatusI != NOT_PRESENT && soundCardStatusI != RX_OK) ? soundCardStatusI : soundCardStatusO);
	//cprintf(HOME "        IO:%c  Time:%c  Frame:%c  FAC:%c  SDC:%c  MSC:%c" NL, inter, time, frame, fac, sdc, msc);
	
	_REAL rIFLevel = Parameters.GetIFSignalLevel();
	//if (rIFLevel > MINIMUM_IF_LEVEL)
	//	cprintf("                   IF Level: %.1f dB" NL, rIFLevel);
	//else
	//	cprintf("                   IF Level: " NA NL);

	asprintf(&sb, "{\"io\":%d,\"time\":%d,\"frame\":%d,\"FAC\":%d,\"SDC\":%d,\"MSC\":%d,\"if\":%.1f",
	    inter, time, frame, fac, sdc, msc, rIFLevel);
	
    int signal = pDRMReceiver->GetAcquiState() == AS_WITH_SIGNAL;
    if (signal)
    {
        _REAL rSNR = Parameters.GetSNR();
        //cprintf("                        SNR: %.1f dB" NL, rSNR);
        
        sb = kstr_asprintf(sb,
            ",\"snr\":%.1f,\"mod\":%d,\"occ\":%d,\"int\":%d,\"sdc\":%d,\"msc\":%d,\"plb\":%d,\"pla\":%d",
            rSNR, Parameters.GetWaveMode(), Parameters.GetSpectrumOccup(), Parameters.eSymbolInterlMode,
            Parameters.eSDCCodingScheme, Parameters.eMSCCodingScheme,
            Parameters.MSCPrLe.iPartB, Parameters.MSCPrLe.iPartA
        );
    }

    //cprintf(NL "Service:" NL);
    sb = kstr_cat(sb, ",\"svc\":[");
    char* strTextMessage = nullptr;
    int iCurAudService = Parameters.GetCurSelAudioService();
    
    if (iCurAudService != drm->audio_service) {
        //printf("iCurAudService=%d => audio_service=%d\n", iCurAudService, drm->audio_service);
        Parameters.SetCurSelAudioService(drm->audio_service);
        iCurAudService = Parameters.GetCurSelAudioService();
    }
    
    for (int i = 0; i < MAX_NUM_SERVICES; i++)
    {
        sb = kstr_asprintf(sb, "%s{", i? ",":"");
        CService service = Parameters.Service[i];

        if (service.IsActive())
        {
            CAudioParam::EAudCod ac = service.AudioParam.eAudioCoding;
            const char *strLabel = kiwi_str_clean((char *) service.strLabel.c_str());
            int ID = service.iServiceID;
            bool audio = service.eAudDataFlag == CService::SF_AUDIO;
            //cprintf("%c%i | %X | %s ", i==iCurAudService ? '>' : ' ', i+1, ID, strLabel);
            _REAL rPartABLenRat = Parameters.PartABLenRatio(i);
            //if (rPartABLenRat != (_REAL) 0.0)
            //    cprintf("| UEP (%.1f%%) |", rPartABLenRat * 100);
            //else
            //    cprintf("| EEP |");
            _REAL rBitRate = Parameters.GetBitRateKbps(i, !audio);
            bool br_audio = audio;
            //if (rBitRate > 0.0)
            //    cprintf(" %s %.2f kbps", audio ? "Audio" : "Data", rBitRate);
            if (audio && service.DataParam.iStreamID != STREAM_ID_NOT_USED)
            {
                _REAL rBitRate = Parameters.GetBitRateKbps(i, true);
                br_audio = false;
                //cprintf(" + Data %.2f kbps", rBitRate);
            }
            if (i == iCurAudService)
                strTextMessage = strdup(service.AudioParam.strTextMessage.c_str());
                //strTextMessage = (char *) service.AudioParam.strTextMessage.c_str();

            sb = kstr_asprintf(sb,
                "\"cur\":%d,\"ac\":%d,\"id\":\"%X\",\"lbl\":\"%s\",\"ep\":%.1f,\"ad\":%d,\"br\":%.2f",
                (i == iCurAudService)? 1:0, ac, ID, strLabel, rPartABLenRat * 100, br_audio? 1:0, rBitRate
            );
        }
        sb = kstr_cat(sb, "}");
    }
    sb = kstr_cat(sb, "]");

    if (strTextMessage != nullptr)
    {
        //string msg(strTextMessage);
        //REPLACE_STR(msg, "\r\n", "\r");  /* Windows CR-LF */
        //REPLACE_CHAR(msg, '\n', "\r");   /* New Line */
        //REPLACE_CHAR(msg, '\f', "\r");   /* Form Feed */
        //REPLACE_CHAR(msg, '\v', "\r");   /* Vertical Tab */
        //REPLACE_CHAR(msg, '\t', "    "); /* Horizontal Tab */
        //REPLACE_CHAR(msg, '\r', NL);     /* Carriage Return */
        //cprintf(NL "%s", msg.c_str());

        //QString TextMessage(QString::fromUtf8(audioService.AudioParam.strTextMessage.c_str()));
        char *s = strTextMessage;
        //char *s = strdup(msg.c_str());
        //printf("%d <<<%s>>>\n", strlen(s), kiwi_str_clean(s));
        sb = kstr_asprintf(sb, ",\"msg\":\"%s\"", kiwi_str_clean(s));
        free(s);
    }

#if 0
		int signal = pDRMReceiver->GetAcquiState() == AS_WITH_SIGNAL;
		if (signal)
		{
			_REAL rWMERMSC = Parameters.rWMERMSC;
			_REAL rMER = Parameters.rMER;
			cprintf("         MSC WMER / MSC MER: %.1f dB / %.1f dB" NL, rWMERMSC, rMER);

			_REAL rDCFreq = Parameters.GetDCFrequency();
			cprintf(" DC Frequency of DRM Signal: %.2f Hz" NL, rDCFreq);

		    _REAL rCurSamROffs = Parameters.rResampleOffset;
			int iCurSamROffsPPM = (int)(rCurSamROffs / Parameters.GetSigSampleRate() * 1e6);
			cprintf("    Sample Frequency Offset: %.2f Hz (%i ppm)" NL, rCurSamROffs, iCurSamROffsPPM);

			_REAL rSigma = Parameters.rSigmaEstimate;
			_REAL rMinDelay = Parameters.rMinDelay;
			if (rSigma >= 0.0)
			cprintf("            Doppler / Delay: %.2f Hz / %.2f ms" NL, rSigma, rMinDelay);
			else
			cprintf("            Doppler / Delay: " NA NL);

			const char *strRob;
			switch (Parameters.GetWaveMode()) {
			case RM_ROBUSTNESS_MODE_A: strRob = "A"; break;
			case RM_ROBUSTNESS_MODE_B: strRob = "B"; break;
			case RM_ROBUSTNESS_MODE_C: strRob = "C"; break;
			case RM_ROBUSTNESS_MODE_D: strRob = "D"; break;
			default:                   strRob = "?"; }
			const char *strOcc;
			switch (Parameters.GetSpectrumOccup()) {
			case SO_0: strOcc = "4.5 kHz"; break;
			case SO_1: strOcc = "5 kHz";   break;
			case SO_2: strOcc = "9 kHz";   break;
	 		case SO_3: strOcc = "10 kHz";  break;
			case SO_4: strOcc = "18 kHz";  break;
			case SO_5: strOcc = "20 kHz";  break;
			default:   strOcc = "?";       }
			cprintf("       DRM Mode / Bandwidth: %s / %s" NL, strRob, strOcc);

			const char *strInter;
		    switch (Parameters.eSymbolInterlMode) {
		    case CParameter::SI_LONG:  strInter = "2 s";    break;
		    case CParameter::SI_SHORT: strInter = "400 ms"; break;
		    default:                   strInter = "?";      }
			cprintf("          Interleaver Depth: %s" NL, strInter);

			const char *strSDC;
			switch (Parameters.eSDCCodingScheme) {
			case CS_1_SM: strSDC = "4-QAM";  break;
			case CS_2_SM: strSDC = "16-QAM"; break;
			default:      strSDC = "?";      }
			const char *strMSC;
			switch (Parameters.eMSCCodingScheme) {
			case CS_2_SM:    strMSC = "SM 16-QAM";    break;
			case CS_3_SM:    strMSC = "SM 64-QAM";    break;
			case CS_3_HMSYM: strMSC = "HMsym 64-QAM"; break;
			case CS_3_HMMIX: strMSC = "HMmix 64-QAM"; break;
			default:         strMSC = "?";            }
			cprintf("             SDC / MSC Mode: %s / %s" NL, strSDC, strMSC);

			int iPartB = Parameters.MSCPrLe.iPartB;
			int iPartA = Parameters.MSCPrLe.iPartA;
			cprintf("        Prot. Level (B / A): %i / %i" NL, iPartB, iPartA);

			int iNumAudio = Parameters.iNumAudioService;
			int iNumData = Parameters.iNumDataService;
			cprintf("         Number of Services: Audio: %i / Data: %i" NL, (int)iNumAudio, (int)iNumData);

			int iYear = Parameters.iYear;
			int iMonth = Parameters.iMonth;
			int iDay = Parameters.iDay;
			int iUTCHour = Parameters.iUTCHour;
			int iUTCMin = Parameters.iUTCMin;
			if (iYear==0 && iMonth==0 && iDay==0 && iUTCHour==0 && iUTCMin==0)
			cprintf("       Received time - date: Service not available" CL);
			else
			cprintf("       Received time - date: %04i-%02i-%02i %02i:%02i:00" CL, iYear, iMonth, iDay, iUTCHour, iUTCMin);

		}
		else {
			cprintf("                        SNR: " NA NL
					"         MSC WMER / MSC MER: " NA NL
					" DC Frequency of DRM Signal: " NA NL
					"    Sample Frequency Offset: " NA NL
					"            Doppler / Delay: " NA NL
					"       DRM Mode / Bandwidth: " NA NL
					"          Interleaver Depth: " NA NL
					"             SDC / MSC Mode: " NA NL
					"        Prot. Level (B / A): " NA NL
					"         Number of Services: " NA NL
					"       Received time - date: " NA CL);
		}

#endif

    Parameters.Unlock();

    sb = kstr_cat(sb, "}");
    DRM_msg(drm, sb);
    kstr_free(sb);

	return RUNNING;
}

int
CConsoleIO::ETypeRxStatus2int(ETypeRxStatus eTypeRxStatus)
{
	switch (eTypeRxStatus) {
	case CRC_ERROR:  return 1;
	case DATA_ERROR: return 2;
	case RX_OK:      return 0;
	default:         return 3; }
}


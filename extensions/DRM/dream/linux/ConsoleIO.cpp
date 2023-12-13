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

// Copyright (c) 2017-2023 John Seamons, ZL4VO/KF6VO

#include "mem.h"

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

void
CConsoleIO::Enter(CDRMReceiver* pDRMReceiver)
{
	CConsoleIO::pDRMReceiver = pDRMReceiver;
	decoder = pDRMReceiver->GetDataDecoder();
    //real_printf("--> CConsoleIO::Enter set null\n");
    text_msg_utf8_enc = nullptr;
}

void
CConsoleIO::Leave()
{
    //real_printf("--> CConsoleIO::Leave free %p\n", text_msg_utf8_enc);
    free(text_msg_utf8_enc);
    text_msg_utf8_enc = nullptr;
}

void
CConsoleIO::Restart()
{
    //real_printf("--> CConsoleIO::Restart free %p\n", text_msg_utf8_enc);
    free(text_msg_utf8_enc);
    text_msg_utf8_enc = nullptr;
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
    
	if ((curtime - time_iq) < GUI_IQ_UPDATE_TIME) {
		return RUNNING;
	}
    //printf("."); fflush(stdout);
	time_iq = curtime;
	
	if (drm->send_iq) {
        pDRMReceiver->GetFACMLC()->GetVectorSpace(facIQ);
        pDRMReceiver->GetSDCMLC()->GetVectorSpace(sdcIQ);
        pDRMReceiver->GetMSCMLC()->GetVectorSpace(mscIQ);
        
        // NB: limited due to N_DATABUF in DRM.h
        int n_fac = MIN(64, facIQ.Size()), n_sdc = MIN(255, sdcIQ.Size()), n_msc = MIN(2048, mscIQ.Size());

        if (n_fac && n_sdc && n_msc) {
            int i, o;
            int len = 4 + (n_fac + n_sdc + n_msc) * 2;
            u1_t u1[len];
            u1[0] = n_fac; u1[1] = n_sdc; u1[2] = n_msc & 0xff; u1[3] = n_msc >> 8;
            o = 4;
            
            for (i = 0; i < n_fac; i++) {
                u1[o++] = (facIQ[i].real() + 1.5) * 255.0 / 3.0;
                u1[o++] = (facIQ[i].imag() + 1.5) * 255.0 / 3.0;
            }
            for (i = 0; i < n_sdc; i++) {
                u1[o++] = (sdcIQ[i].real() + 1.5) * 255.0 / 3.0;
                u1[o++] = (sdcIQ[i].imag() + 1.5) * 255.0 / 3.0;
            }
            for (i = 0; i < n_msc; i++) {
                u1[o++] = (mscIQ[i].real() + 1.5) * 255.0 / 3.0;
                u1[o++] = (mscIQ[i].imag() + 1.5) * 255.0 / 3.0;
            }
            //printf("%d|%d|%d(%d,%d) ", n_fac, n_sdc, n_msc, u1[3], u1[2]); fflush(stdout);
            DRM_data((u1_t) DRM_DAT_IQ, u1, len);
        }
    }


	if ((curtime - time) < GUI_CONTROL_UPDATE_TIME) {
	//if ((curtime - time) < 250) {
        //printf("%llu ", curtime - time); fflush(stdout);
		return RUNNING;
	}
    //printf("."); fflush(stdout);
	time = curtime;

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
	
	_REAL rIFLevel = Parameters.GetIFSignalLevel();

	asprintf(&sb, "{\"io\":%d,\"time\":%d,\"frame\":%d,\"FAC\":%d,\"SDC\":%d,\"MSC\":%d,\"if\":%.1f",
	    inter, time, frame, fac, sdc, msc, rIFLevel);
	
    int signal = (pDRMReceiver->GetAcquiState() == AS_WITH_SIGNAL);
    if (signal) {
        _REAL rSNR = Parameters.GetSNR();
        sb = kstr_asprintf(sb,
            ",\"snr\":%.1f,\"mod\":%d,\"occ\":%d,\"ilv\":%d,\"sdc\":%d,\"msc\":%d,\"plb\":%d,\"pla\":%d,\"nas\":%d,\"nds\":%d,"
            "\"epg\":%d",
            rSNR, Parameters.GetWaveMode(), Parameters.GetSpectrumOccup(), Parameters.eSymbolInterlMode,
            Parameters.eSDCCodingScheme, Parameters.eMSCCodingScheme,
            Parameters.MSCPrLe.iPartB, Parameters.MSCPrLe.iPartA,
            Parameters.iNumAudioService, Parameters.iNumDataService,
            decoder->iEPGService
        );
    }

    sb = kstr_cat(sb, ",\"svc\":[");
    int iCurAudService = Parameters.GetCurSelAudioService();
    int iCurDataService = Parameters.GetCurSelDataService();
    uint32_t CurDataServiceID = 0;
    
    if (iCurAudService != drm->audio_service) {
        Parameters.SetCurSelAudioService(drm->audio_service);
        Parameters.SetCurSelDataService(drm->audio_service);
        iCurAudService = Parameters.GetCurSelAudioService();
        iCurDataService = Parameters.GetCurSelDataService();
    }
    
    int datStat = ETypeRxStatus2int(Parameters.DataComponentStatus[iCurDataService].GetStatus());

    for (int i = 0; i < MAX_NUM_SERVICES; i++)
    {
        sb = kstr_asprintf(sb, "%s{", i? ",":"");
        CService service = Parameters.Service[i];

        if (service.IsActive())
        {
            CAudioParam::EAudCod ac = service.AudioParam.eAudioCoding;
            char *strLabel = kiwi_str_encode((char *) service.strLabel.c_str());
            //real_printf("--> strLabel new %p %d<%s>\n", strLabel, strlen(strLabel), strLabel);
            uint32_t ID = service.iServiceID;
            if (i == iCurDataService) CurDataServiceID = ID;
            bool audio = (service.eAudDataFlag == CService::SF_AUDIO);
            _REAL rPartABLenRat = Parameters.PartABLenRatio(i);
            _REAL rBitRate = Parameters.GetBitRateKbps(i, !audio);
            bool br_audio = audio;
            if (audio && service.DataParam.iStreamID != STREAM_ID_NOT_USED)
            {
                _REAL rBitRate = Parameters.GetBitRateKbps(i, true);
                br_audio = false;
            }
            if (i == iCurAudService && service.AudioParam.bTextflag) {
                //real_printf("--> bTextflag free %p\n", text_msg_utf8_enc);
                free(text_msg_utf8_enc);
                text_msg_utf8_enc = kiwi_str_encode((char *) service.AudioParam.strTextMessage.c_str());
                //real_printf("--> bTextflag new %p %d<%s>\n", text_msg_utf8_enc, strlen(text_msg_utf8_enc), text_msg_utf8_enc);
            }

            sb = kstr_asprintf(sb,
                "\"cur\":%d,\"dat\":%d,\"ac\":%d,\"id\":\"%X\",\"lbl\":\"%s\",\"ep\":%.1f,\"ad\":%d,\"br\":%.2f",
                (i == iCurAudService)? 1:0,
                (i == iCurDataService)? datStat : -1,
                ac, ID, strLabel, rPartABLenRat * 100, br_audio? 1:0, rBitRate
            );
            //real_printf("--> strLabel free %p\n", strLabel);
            free(strLabel);
        }
        sb = kstr_cat(sb, "}");
    }
    sb = kstr_cat(sb, "]");

    if (signal && text_msg_utf8_enc)
        sb = kstr_asprintf(sb, ",\"msg\":\"%s\"", text_msg_utf8_enc);

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
    DRM_msg_encoded(DRM_MSG_STATUS, "drm_status_cb", sb);
    kstr_free(sb);
    

    // process Journaline stream
    bool reset = false;
    if (drm->journaline_objSet) {
        total = ready = 0;
        if (drm->journaline_objID < 0) {
            decoder->Journaline.Reset();
            reset = true;
            drm->journaline_objID = 0;
        }
        drm->journaline_objSet = false;
    }
    CNews News;
    decoder->GetNews(drm->journaline_objID, News);
    int new_total = News.vecItem.Size();
    int new_ready = 0;
    bool dirty = false;

    for (int i = 0; i < new_total; i++) {
        switch (News.vecItem[i].iLink) {
        case JOURNALINE_IS_NO_LINK:         // Only text, no link
        case JOURNALINE_LINK_NOT_ACTIVE:
            break;
        default:        // has active link
            new_ready++;
        }
    }
    if (new_total != total) { total = new_total; dirty = true; }
    if (new_ready != ready) { ready = new_ready; dirty = true; }

    if (dirty || reset) {
        char *c2_s = NULL;
        //printf("new_total =%d|%d new_ready=%d|%d\n", new_total, total, new_ready, ready);
        char *c_s = (char *) News.sTitle.c_str();
        if (!c_s || *c_s == '\0') {
            c_s = (char *) "";
            if (new_total == 0)
                drm->journaline_objID = 0;
        } else {
            c2_s = kiwi_str_escape_HTML(c_s);   // also removes non-printing chars
            if (c2_s) c_s = c2_s;
        }
        sb = kstr_asprintf(NULL, "{\"l\":%d,\"t\":\"%s\",\"a\":[", drm->journaline_objID, c_s);
        kiwi_ifree(c2_s, "drm console");
        
        bool comma = false;
        for (int i = 0; i < new_total; i++) {
            int iLink = News.vecItem[i].iLink;
            c_s = (char *) News.vecItem[i].sText.c_str();
            if (!c_s || *c_s == '\0') {
                c_s = (char *) "";
            }
            
            #if 0
                printf("JL%d %x %p sl=%d\n", i, iLink, c_s, strlen(c_s));
                for (int j = 0; j < strlen(c_s); j++) {
                    char c = c_s[j];
                    if (c < ' ' && c != '\n')
                        printf("JL %02x@%d\n", c, j);
                }
            #endif
            
            kiwi_str_replace(c_s, "\n", " ");
            c2_s = kiwi_str_escape_HTML(c_s);   // also removes non-printing chars
            if (c2_s) c_s = c2_s;
            sb = kstr_asprintf(sb, "%s{\"i\":%d,\"l\":%d,\"s\":\"%s\"}",
                comma? ",":"", i, iLink, c_s);
            kiwi_ifree(c2_s, "drm console");
            comma = true;

            #if 0
                switch (iLink) {
                case JOURNALINE_IS_NO_LINK: /* Only text, no link */
                    printf("%d JL-nl %s\n", i, c_s);
                    break;
                case JOURNALINE_LINK_NOT_ACTIVE:
                    printf("%d JL-w   %s\n", i, c_s);
                    break;
                default:    // has active link
                    printf("%d JL-%x %s\n", i, iLink, c_s);
                    break;
                }
            #endif
        }
        sb = kstr_cat(sb, "]}");
        //printf("%s\n", kstr_sp(sb));
        DRM_msg_encoded(DRM_MSG_JOURNALINE, "drm_journaline_cb", sb);
        kstr_free(sb);
    }


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


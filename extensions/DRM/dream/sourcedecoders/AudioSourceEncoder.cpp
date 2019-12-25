/******************************************************************************\
 * Technische Universitaet Darmstadt, Institut fuer Nachrichtentechnik
 * Copyright (c) 2001
 *
 * Author(s):
 *	Volker Fischer, Ollie Haffenden
 *
 * Description:
 *	Audio source encoder/decoder
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

#include "AudioSourceEncoder.h"
#include <iostream>
#include <unistd.h>


/* Implementation *************************************************************/

CAudioSourceEncoderImplementation::CAudioSourceEncoderImplementation()
    : bUsingTextMessage(false), codec(nullptr)
{
    /* Initialize Audio Codec List */
    CAudioCodec::InitCodecList();

    /* In case codec might be dereferenced before initialised this will
       get us a null codec at least, it is safer than other codec */
    codec = CAudioCodec::GetEncoder(AC_NULL);

    /* Needed by TransmDlg.cpp to report available codec */
    bCanEncodeAAC  = CAudioCodec::GetEncoder(CAudioParam::AC_AAC,  true) != nullptr;
    bCanEncodeOPUS = CAudioCodec::GetEncoder(CAudioParam::AC_OPUS, true) != nullptr;
}

CAudioSourceEncoderImplementation::~CAudioSourceEncoderImplementation()
{
//printf("==== CAudioSourceEncoderImplementation::~CAudioSourceEncoderImplementation ().. pid=%d\n", getpid());
    /* Unreference Audio Codec List */
    CAudioCodec::UnrefCodecList();
}

void
CAudioSourceEncoderImplementation::ProcessDataInternal(CParameter& Parameters,
                                                       CVectorEx < _SAMPLE >
                                                       *pvecInputData,
                                                       CVectorEx < _BINARY >
                                                       *pvecOutputData,
                                                       int &iInputBlockSize,
                                                       int &iOutputBlockSize)
{
    int iCurSelServ = 0;

    /* Reset data to zero. This is important since usually not all data is used
       and this data has to be set to zero as defined in the DRM standard */
    for (int i = 0; i < iOutputBlockSize; i++)
        (*pvecOutputData)[i] = 0;

    if (bIsDataService == false)
    {
        /* Check if audio param have changed */
        Parameters.Lock();

        /* Get audio param for audio service */
        CAudioParam& AudioParam(Parameters.Service[iCurSelServ].AudioParam);

        if (AudioParam.bParamChanged)
        {
            AudioParam.bParamChanged = false;
            codec->EncUpdate(AudioParam);
        }
        Parameters.Unlock();

        /* Resample data to encoder bit-rate */
        /* Change type of data (short -> real) */
        /* The input data is always stereo, if the encoder
           if set to mono we take the left channel only */
        for (int j = 0; j < iInputBlockSize / 2; j++)
        {
            for (int i = 0; i < iNumChannels; i++)
                vecTempResBufIn[i][j] = (*pvecInputData)[j * 2 + i];
        }

        /* Resample data */
        for (int i = 0; i < iNumChannels; i++)
            ResampleObj[i].Resample(vecTempResBufIn[i], vecTempResBufOut[i]);

        /* Split data in individual audio blocks */
        for (int j = 0; j < iNumAudioFrames; j++)
        {
            int bytesEncoded;
            CVector < unsigned char >vecsTmpData(lMaxBytesEncOut);

            /* Convert _REAL type to _SAMPLE type, copy in smaller buffer */
            for (unsigned long k = 0; k < lNumSampEncIn; k++)
            {
                for (int i = 0; i < iNumChannels; i++)
                    vecsEncInData[k * iNumChannels + i] =
                            Real2Sample(vecTempResBufOut[i][j * lNumSampEncIn + k]);
            }

            /* Actual encoding */
            bytesEncoded = codec->Encode(vecsEncInData, lNumSampEncIn * iNumChannels, vecsTmpData, lMaxBytesEncOut);
            if (bytesEncoded > 0)
            {
                /* Extract CRC */
                aac_crc_bits[j] = vecsTmpData[0];

                /* Extract actual data */
                for (int i = 0; i < bytesEncoded - 1 /* "-1" for CRC */ ; i++)
                    audio_frame[j][i] = vecsTmpData[i + 1];

                /* Store block lengths for boarders in AAC super-frame-header */
                veciFrameLength[j] = bytesEncoded - 1;
            }
            else
            {
                /* Encoder is in initialization phase, reset CRC and length */
                aac_crc_bits[j] = 0;
                veciFrameLength[j] = 0;
            }
        }

        /* Write data to output vector */
        /* First init buffer with zeros */
        for (int i = 0; i < iOutputBlockSize; i++)
            (*pvecOutputData)[i] = 0;

        /* Reset bit extraction access */
        (*pvecOutputData).ResetBitAccess();

        /* Audio super-frame-header */
        int iAudioFrameLength = 0;
        for (int j = 0; j < iNumBorders; j++)
        {
            /* Accumulate audio frame length */
            iAudioFrameLength += veciFrameLength[j];

            /* Frame border in bytes (12 bits) */
            (*pvecOutputData).Enqueue(iAudioFrameLength, 12);
        }

        /* Byte-alignment (4 bits) in case of odd number of borders */
        if (iNumBorders & 1)
            (*pvecOutputData).Enqueue(0, 4);

        /* Higher protected part */
        int iCurNumBytes = 0;
        for (int j = 0; j < iNumAudioFrames; j++)
        {
            /* Data */
            for (int i = 0; i < iNumHigherProtectedBytes; i++)
            {
                /* Check if enough data is available, set data to 0 if not */
                if (i < veciFrameLength[j])
                    (*pvecOutputData).Enqueue(audio_frame[j][i], 8);
                else
                    (*pvecOutputData).Enqueue(0, 8);

                iCurNumBytes++;
            }

            /* CRCs */
            (*pvecOutputData).Enqueue(aac_crc_bits[j], 8);
        }

        /* Lower protected part */
        for (int j = 0; j < iNumAudioFrames; j++)
        {
            for (int i = iNumHigherProtectedBytes; i < veciFrameLength[j]; i++)
            {
                /* If encoder produced too many bits, we have to drop them */
                if (iCurNumBytes < iAudioPayloadLen)
                    (*pvecOutputData).Enqueue(audio_frame[j][i], 8);

                iCurNumBytes++;
            }
        }

#ifdef _DEBUG_
        /* Save number of bits actually used by audio encoder */
        static FILE *pFile = fopen("test/audbits.dat", "w");
        fprintf(pFile, "%d %d\n", iAudioPayloadLen, iCurNumBytes);
        fflush(pFile);
#endif
    }

    /* Data service and text message application ---------------------------- */
    if (bIsDataService)
    {
        // TODO: make a separate modul for data encoding
        /* Write data packets in stream */
        CVector < _BINARY > vecbiData;
        const int iNumPack = iOutputBlockSize / iTotPacketSize;
        int iPos = 0;

        for (int j = 0; j < iNumPack; j++)
        {
            /* Get new packet */
            DataEncoder.GeneratePacket(vecbiData);

            /* Put it on stream */
            for (int i = 0; i < iTotPacketSize; i++)
            {
                (*pvecOutputData)[iPos] = vecbiData[i];
                iPos++;
            }
        }
    }
    else
    {
        /* Text message application. Last four bytes in stream are written */
        if (bUsingTextMessage)
        {
            /* Always four bytes for text message "piece" */
            CVector < _BINARY >
                    vecbiTextMessBuf(SIZEOF__BYTE *
                                     NUM_BYTES_TEXT_MESS_IN_AUD_STR);

            /* Get a "piece" */
            TextMessage.Encode(vecbiTextMessBuf);

            /* Calculate start point for text message */
            const int iByteStartTextMess =
                    iTotNumBitsForUsage -
                    SIZEOF__BYTE * NUM_BYTES_TEXT_MESS_IN_AUD_STR;

            /* Add text message bytes to output stream */
            for (int i = iByteStartTextMess; i < iTotNumBitsForUsage; i++)
                (*pvecOutputData)[i] =
                    vecbiTextMessBuf[i - iByteStartTextMess];
        }
    }
}

void
CAudioSourceEncoderImplementation::InitInternalTx(CParameter & Parameters,
                                                  int &iInputBlockSize,
                                                  int &iOutputBlockSize)
{
    int iCurStreamID;

    int iCurSelServ = 0;		// TEST

    Parameters.Lock();

    /* Close previous encoder instance if any */
    CloseEncoder();

    /* Calculate number of input samples in mono. Audio block are always
       400 ms long */
    const int iNumInSamplesMono = (int) ((_REAL) Parameters.GetAudSampleRate() *
                                         (_REAL) 0.4 /* 400 ms */ );

    /* Set the total available number of bits, byte aligned */
    iTotNumBitsForUsage =
            (Parameters.iNumDecodedBitsMSC / SIZEOF__BYTE) * SIZEOF__BYTE;

    /* Total number of bytes which can be used for data and audio */
    const int iTotNumBytesForUsage = iTotNumBitsForUsage / SIZEOF__BYTE;

    if (Parameters.iNumDataService == 1)
    {
        /* Data service ----------------------------------------------------- */
        bIsDataService = true;
        iTotPacketSize = DataEncoder.Init(Parameters);

        /* Get stream ID for data service */
        iCurStreamID = Parameters.Service[iCurSelServ].DataParam.iStreamID;
    }
    else
    {
        /* Audio service ---------------------------------------------------- */
        bIsDataService = false;

        /* Get stream ID and codec type for audio service */
        iCurStreamID = Parameters.Service[iCurSelServ].AudioParam.iStreamID;
        CAudioParam::EAudCod eAudioCoding = Parameters.Service[iCurSelServ].AudioParam.eAudioCoding;

        /* Get encoder instance */
        codec = CAudioCodec::GetEncoder(eAudioCoding);

        /* Total frame size is input block size minus the bytes for the text
           message (if text message is used) */
        int iTotAudFraSizeBits = iTotNumBitsForUsage;
        if (bUsingTextMessage)
            iTotAudFraSizeBits -=
                    SIZEOF__BYTE * NUM_BYTES_TEXT_MESS_IN_AUD_STR;

        int iNumHeaderBytes;

        switch (eAudioCoding)
        {
        case CAudioParam::AC_AAC:
        {
            int iTimeEachAudBloMS = 40;

            iNumChannels = (Parameters.Service[iCurSelServ].AudioParam.eAudioMode==CAudioParam::AM_MONO)?1:2;

            switch (Parameters.Service[iCurSelServ].AudioParam.eAudioSamplRate)
            {
            case CAudioParam::AS_12KHZ:
                iTimeEachAudBloMS = 80;	/* ms */
                iNumAudioFrames = 5;
                break;

            case CAudioParam::AS_24KHZ:
                iTimeEachAudBloMS = 40;	/* ms */
                iNumAudioFrames = 10;
                break;
            
            default:
                break;
            }

            /* Number of borders, the border of the last frame is not included because
               the audio frame size of the last frame is the frame remaining bytes */
            iNumBorders = iNumAudioFrames - 1;

            /* Calculate the number of header bytes */
            iNumHeaderBytes = (iNumBorders * 12 + 7) / 8;

            /* The audio_payload_length is derived from the length of the audio
               super frame (data_length_of_part_A + data_length_of_part_B)
               subtracting the audio super frame overhead (bytes used for the audio
               super frame header() and for the aac_crc_bits) (5.3.1.1, Table 5) */
            iAudioPayloadLen = iTotAudFraSizeBits / SIZEOF__BYTE - iNumHeaderBytes - iNumAudioFrames /* for CRCs */ ;

            const int iActEncOutBytes = iAudioPayloadLen / iNumAudioFrames;

            /* Open encoder instance */
            codec->EncOpen(Parameters.Service[iCurSelServ].AudioParam, lNumSampEncIn, lMaxBytesEncOut);
            lNumSampEncIn /= unsigned(iNumChannels);

            /* Calculate bitrate, bit per second */
            const int iBitRate = iActEncOutBytes * SIZEOF__BYTE * AUD_DEC_TRANSFROM_LENGTH / int(lNumSampEncIn) / iTimeEachAudBloMS * 1000;
            codec->EncSetBitrate(iBitRate);
        }
            break;

        case CAudioParam::AC_OPUS:
        {
            /* Set various encoder parameters */
            iNumChannels = 2;
            iNumAudioFrames = 20;
            lEncSamprate = 48000; // used later to get ratio
            Parameters.Service[iCurSelServ].AudioParam.eAudioSamplRate = CAudioParam::AS_48KHZ;	/* Set parameter in global struct */

            /* Number of borders, opus decoder need to know the exact frame size,
               thus the number of borders equal the number of audio frames */
            iNumBorders = iNumAudioFrames;

            /* Calculate the number of header bytes */
            iNumHeaderBytes = (iNumBorders * 12 + 7) / 8;

            /* The audio_payload_length is derived from the length of the audio
               super frame (data_length_of_part_A + data_length_of_part_B)
               subtracting the audio super frame overhead (bytes used for the audio
               super frame header()) */
            iAudioPayloadLen = iTotAudFraSizeBits / SIZEOF__BYTE - iNumHeaderBytes;

            const int iActEncOutBytes = (int) (iAudioPayloadLen / iNumAudioFrames);

            /* Open encoder instance */
            codec->EncOpen(Parameters.Service[iCurSelServ].AudioParam, lNumSampEncIn, lMaxBytesEncOut);
            lNumSampEncIn /= iNumChannels;

            /* Calculate bitrate, bit per frame */
            const int iBitRate = iActEncOutBytes * SIZEOF__BYTE;
            codec->EncSetBitrate(iBitRate);

            /* Set flags to reset and init codec params on first ProcessDataInternal call */
            Parameters.Service[iCurSelServ].AudioParam.bOPUSRequestReset = true;
            Parameters.Service[iCurSelServ].AudioParam.bParamChanged = true;
        }
            break;

        default:
            /* Unsupported encoder, parameters must be safe */
            lEncSamprate = 12000;
            iNumChannels = 1;
            iNumAudioFrames = 1;
            iNumBorders = 0;
            iNumHeaderBytes = 0;
        }

        /* Set parameter in global struct */
        Parameters.Service[iCurSelServ].AudioParam.eAudioMode =
                iNumChannels >= 2 ? CAudioParam::AM_STEREO : CAudioParam::AM_MONO;

        /* Init storage for actual data, CRCs and frame lengths */
        audio_frame.Init(iNumAudioFrames, lMaxBytesEncOut);
        vecsEncInData.Init(lNumSampEncIn * iNumChannels);
        aac_crc_bits.Init(iNumAudioFrames);
        veciFrameLength.Init(iNumAudioFrames);

        _REAL rRatio = _REAL(lEncSamprate) / _REAL(Parameters.GetAudSampleRate()) * _REAL(lNumSampEncIn) / _REAL(AUD_DEC_TRANSFROM_LENGTH);

        for (int i = 0; i < iNumChannels; i++)
        {
            /* Additional buffers needed for resampling since we need conversation
               between _SAMPLE and _REAL */
            vecTempResBufIn[i].Init(iNumInSamplesMono);
            vecTempResBufOut[i].Init(lNumSampEncIn * iNumAudioFrames, 0.0);
            /* Init resample objects */
            ResampleObj[i].Init(iNumInSamplesMono, rRatio);
        }

        /* Calculate number of bytes for higher protected blocks */
        iNumHigherProtectedBytes =
                (Parameters.Stream[iCurStreamID].iLenPartA
                 - iNumHeaderBytes -
                 iNumAudioFrames /* CRC bytes */ ) / iNumAudioFrames;

        if (iNumHigherProtectedBytes < 0)
            iNumHigherProtectedBytes = 0;
    }

    /* Adjust part B length for SDC stream. Notice, that the
       "Parameters.iNumDecodedBitsMSC" parameter depends on these settings.
       Thus, length part A and B have to be set before, preferably in the
       DRMTransmitter initialization */
    if ((Parameters.Stream[iCurStreamID].iLenPartA == 0) ||
            (iTotNumBytesForUsage < Parameters.Stream[iCurStreamID].iLenPartA))
    {
        /* Equal error protection was chosen or protection part A was chosen too
           high, set to equal error protection! */
        Parameters.Stream[iCurStreamID].iLenPartA = 0;
        Parameters.Stream[iCurStreamID].iLenPartB = iTotNumBytesForUsage;
    }
    else
        Parameters.Stream[iCurStreamID].iLenPartB = iTotNumBytesForUsage -
                Parameters.Stream[iCurStreamID].iLenPartA;

    /* Define input and output block size */
    iOutputBlockSize = Parameters.iNumDecodedBitsMSC;
    iInputBlockSize = iNumInSamplesMono * 2 /* stereo */ ;

    Parameters.Unlock();
}

void
CAudioSourceEncoderImplementation::InitInternalRx(CParameter& Parameters,
                                                  int &iInputBlockSize,
                                                  int &iOutputBlockSize)
{

    int iCurSelServ = 0;		// TEST

    Parameters.Lock();

    /* Close previous encoder instance if any */
    CloseEncoder();

    /* Calculate number of input samples in mono. Audio block are always 400 ms long */
    const int iNumInSamplesMono = (int) ((_REAL) Parameters.GetAudSampleRate() *
                                         (_REAL) 0.4 /* 400 ms */ );

    /* Set the total available number of bits, byte aligned */
    iTotNumBitsForUsage =
            (Parameters.Stream[0].iLenPartA + Parameters.Stream[0].iLenPartB) * SIZEOF__BYTE;

    /* Total number of bytes which can be used for data and audio */
    //const int iTotNumBytesForUsage = iTotNumBitsForUsage / SIZEOF__BYTE;

    /* Audio service ---------------------------------------------------- */
    bIsDataService = false;

    /* Total frame size is input block size minus the bytes for the text
       message (if text message is used) */
    int iTotAudFraSizeBits = iTotNumBitsForUsage;
    if (bUsingTextMessage)
        iTotAudFraSizeBits -= SIZEOF__BYTE * NUM_BYTES_TEXT_MESS_IN_AUD_STR;

    /* Get encoder instance */
    codec = CAudioCodec::GetEncoder(Parameters.Service[0].AudioParam.eAudioCoding);

    switch (Parameters.Service[0].AudioParam.eAudioCoding) {

    case CAudioParam::AC_AAC:
    {
        int iTimeEachAudBloMS = 40;

        switch (Parameters.Service[iCurSelServ].AudioParam.eAudioSamplRate)
        {
        case CAudioParam::AS_12KHZ:
            iTimeEachAudBloMS = 80;	/* ms */
            iNumAudioFrames = 5;
            break;

        case CAudioParam::AS_24KHZ:
            iTimeEachAudBloMS = 40;	/* ms */
            iNumAudioFrames = 10;
            break;
            
        default:
            break;
        }

        /* Number of borders, the border of the last frame is not included because
           the audio frame size of the last frame is the frame remaining bytes */
        iNumBorders = iNumAudioFrames - 1;

        /* Calculate the number of header bytes */
        int iNumHeaderBytes = (iNumBorders * 12 + 7) / 8;

        /* The audio_payload_length is derived from the length of the audio
           super frame (data_length_of_part_A + data_length_of_part_B)
           subtracting the audio super frame overhead (bytes used for the audio
           super frame header() and for the aac_crc_bits) (5.3.1.1, Table 5) */
        iAudioPayloadLen = iTotAudFraSizeBits / SIZEOF__BYTE - iNumHeaderBytes - iNumAudioFrames /* for CRCs */ ;

        const int iActEncOutBytes = (int) (iAudioPayloadLen / iNumAudioFrames);

        /* Open encoder instance */
        codec->EncOpen(Parameters.Service[iCurSelServ].AudioParam, lNumSampEncIn, lMaxBytesEncOut);
        lNumSampEncIn /= iNumChannels;

        /* Calculate bitrate, bit per second */
        const int iBitRate = iActEncOutBytes * SIZEOF__BYTE * AUD_DEC_TRANSFROM_LENGTH / lNumSampEncIn / iTimeEachAudBloMS * 1000;
        codec->EncSetBitrate(iBitRate);
    }
        break;

    case CAudioParam::AC_OPUS:
    {
        /* Set various encoder parameters */
        lEncSamprate = 48000;
        iNumChannels = 2;
        iNumAudioFrames = 20;
        Parameters.Service[0].AudioParam.eAudioSamplRate = CAudioParam::AS_48KHZ;	/* Set parameter in global struct */

        /* Number of borders, opus decoder need to know the exact frame size,
           thus the number of borders equal the number of audio frames */
        iNumBorders = iNumAudioFrames;

        /* Calculate the number of header bytes */
        int iNumHeaderBytes = (iNumBorders * 12 + 7) / 8;

        /* The audio_payload_length is derived from the length of the audio
           super frame (data_length_of_part_A + data_length_of_part_B)
           subtracting the audio super frame overhead (bytes used for the audio
           super frame header()) */
        iAudioPayloadLen = iTotAudFraSizeBits / SIZEOF__BYTE - iNumHeaderBytes;

        const int iActEncOutBytes = (int) (iAudioPayloadLen / iNumAudioFrames);

        /* Open encoder instance */
        codec->EncOpen(Parameters.Service[iCurSelServ].AudioParam, lNumSampEncIn, lMaxBytesEncOut);
        lNumSampEncIn /= iNumChannels;

        /* Calculate bitrate, bit per frame */
        const int iBitRate = iActEncOutBytes * SIZEOF__BYTE;
        codec->EncSetBitrate(iBitRate);

        /* Set flags to reset and init codec params on first ProcessDataInternal call */
        Parameters.Service[0].AudioParam.bOPUSRequestReset = true;
        Parameters.Service[0].AudioParam.bParamChanged = true;
    }
        break;

    default:
        /* Unsupported encoder, parameters must be safe */
        lEncSamprate = 12000;
        iNumChannels = 1;
        iNumAudioFrames = 1;
        iNumBorders = 0;
    }

    /* Set parameter in global struct */
    Parameters.Service[0].AudioParam.eAudioMode =
            iNumChannels == 2 ? CAudioParam::AM_STEREO : CAudioParam::AM_MONO;

    /* Init storage for actual data, CRCs and frame lengths */
    audio_frame.Init(iNumAudioFrames, lMaxBytesEncOut);
    vecsEncInData.Init(lNumSampEncIn * iNumChannels);
    aac_crc_bits.Init(iNumAudioFrames);
    veciFrameLength.Init(iNumAudioFrames);

    for (int i = 0; i < iNumChannels; i++)
    {
        /* Additional buffers needed for resampling since we need conversation
           between _SAMPLE and _REAL */
        vecTempResBufIn[i].Init(iNumInSamplesMono);
        vecTempResBufOut[i].Init(lNumSampEncIn * iNumAudioFrames, (_REAL) 0.0);

        /* Init resample objects */
        ResampleObj[i].Init(iNumInSamplesMono,
                            (_REAL) lEncSamprate / (_REAL) Parameters.GetAudSampleRate() *
                            (_REAL) lNumSampEncIn / (_REAL) AUD_DEC_TRANSFROM_LENGTH);
    }

    /* Calculate number of bytes for higher protected blocks */
    iNumHigherProtectedBytes = 0;

    /* Define input and output block size */
    iOutputBlockSize = iTotNumBitsForUsage;
    iInputBlockSize = iNumInSamplesMono * 2 /* stereo */ ;

    Parameters.Unlock();
}

void
CAudioSourceEncoderImplementation::CloseEncoder()
{
    if (codec != nullptr)
        codec->EncClose();
}

void
CAudioSourceEncoderImplementation::SetTextMessage(const string & strText)
{
    /* Set text message in text message object */
    TextMessage.SetMessage(strText);

    /* Set text message flag */
    bUsingTextMessage = true;
}

void
CAudioSourceEncoderImplementation::ClearTextMessage()
{
    /* Clear all text segments */
    TextMessage.ClearAllText();

    /* Clear text message flag */
    bUsingTextMessage = false;
}


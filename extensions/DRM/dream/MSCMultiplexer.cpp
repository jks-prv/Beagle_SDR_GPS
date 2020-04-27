/******************************************************************************\
 * Technische Universitaet Darmstadt, Institut fuer Nachrichtentechnik
 * Copyright (c) 2001
 *
 * Author(s):
 *	Volker Fischer
 *
 * Description:
 *	MSC audio/data demultiplexer
 *
 *
 * - (6.2.3.1) Multiplex frames (DRM standard):
 * The multiplex frames are built by placing the logical frames from each
 * non-hierarchical stream together. The logical frames consist, in general, of
 * two parts each with a separate protection level. The multiplex frame is
 * constructed by taking the data from the higher protected part of the logical
 * frame from the lowest numbered stream (stream 0 when hierarchical modulation
 * is not used, or stream 1 when hierarchical modulation is used) and placing
 * it at the start of the multiplex frame. Next the data from the higher
 * protected part of the logical frame from the next lowest numbered stream is
 * appended and so on until all streams have been transferred. The data from
 * the lower protected part of the logical frame from the lowest numbered
 * stream (stream 0 when hierarchical modulation is not used, or stream 1 when
 * hierarchical modulation is used) is then appended, followed by the data from
 * the lower protected part of the logical frame from the next lowest numbered
 * stream, and so on until all streams have been transferred. The higher
 * protected part is designated part A and the lower protected part is
 * designated part B in the multiplex description. The multiplex frame will be
 * larger than or equal to the sum of the logical frames from which it is
 * formed. The remainder, if any, of the multiplex frame shall be filled with
 * 0s. These bits shall be ignored by the receiver.
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

#include "MSCMultiplexer.h"


/* Implementation *************************************************************/
void CMSCDemultiplexer::ProcessDataInternal(CParameter&)
{
    for (size_t i=0; i<MAX_NUM_STREAMS; i++)
        ExtractData(*pvecInputData, *vecpvecOutputData[i], StreamPos[i]);
}

void CMSCDemultiplexer::InitInternal(CParameter& Parameters)
{
    Parameters.Lock();
    for (size_t i=0; i<MAX_NUM_STREAMS; i++)
    {
        StreamPos[i] = GetStreamPos(Parameters, i);
        veciOutputBlockSize[i] = StreamPos[i].iLenHigh + StreamPos[i].iLenLow;
    }
    /* Set input block size */
    iInputBlockSize = Parameters.iNumDecodedBitsMSC;
    Parameters.Unlock();
}

void CMSCDemultiplexer::ExtractData(CVectorEx<_BINARY>& vecIn,
                                    CVectorEx<_BINARY>& vecOut,
                                    SStreamPos& StrPos)
{
    int i;

    /* Higher protected part */
    for (i = 0; i < StrPos.iLenHigh; i++)
        vecOut[i] = vecIn[i + StrPos.iOffsetHigh];

    /* Lower protected part */
    for (i = 0; i < StrPos.iLenLow; i++)
        vecOut[i + StrPos.iLenHigh] = vecIn[i + StrPos.iOffsetLow];
}

CMSCDemultiplexer::SStreamPos CMSCDemultiplexer::GetStreamPos(CParameter& Param,
        const int iStreamID)
{
    CMSCDemultiplexer::SStreamPos	StPos;

    /* Init positions with zeros (needed if an error occurs) */
    StPos.iOffsetLow = 0;
    StPos.iOffsetHigh = 0;
    StPos.iLenLow = 0;
    StPos.iLenHigh = 0;

    if (iStreamID != STREAM_ID_NOT_USED)
    {
        /* Length of higher and lower protected part of audio stream (number
           of bits) */
        StPos.iLenHigh = Param.Stream[iStreamID].iLenPartA * SIZEOF__BYTE;
        StPos.iLenLow = Param.Stream[iStreamID].iLenPartB *	SIZEOF__BYTE;


        /* Byte-offset of higher and lower protected part of audio stream --- */
        /* Get active streams */
        set<int> actStreams;
        Param.GetActiveStreams(actStreams);

        /* Get start offset for lower protected parts in stream. Since lower
           protected part comes after the higher protected part, the offset
           must be shifted initially by all higher protected part lengths
           (iLenPartA of all streams are added) 6.2.3.1 */
        StPos.iOffsetLow = 0;
        set<int>::iterator i;
        for (i = actStreams.begin(); i!=actStreams.end(); i++)
        {
            StPos.iOffsetLow += Param.Stream[*i].iLenPartA * SIZEOF__BYTE;
        }

        /* Real start position of the streams */
        StPos.iOffsetHigh = 0;
        for (i = actStreams.begin(); i!=actStreams.end(); i++)
        {
            if (*i < iStreamID)
            {
                StPos.iOffsetHigh += Param.Stream[*i].iLenPartA * SIZEOF__BYTE;
                StPos.iOffsetLow += Param.Stream[*i].iLenPartB * SIZEOF__BYTE;
            }
        }

        /* Special case if hierarchical modulation is used */
        if (((Param.eMSCCodingScheme == CS_3_HMSYM) ||
                (Param.eMSCCodingScheme == CS_3_HMMIX)))
        {
            if (iStreamID == 0)
            {
                /* Hierarchical channel is selected. Data is at the beginning
                   of incoming data block */
                StPos.iOffsetLow = 0;
            }
            else
            {
                /* Shift all offsets by the length of the hierarchical frame. We
                   cannot use the information about the length in
                   "Stream[0].iLenPartB", because the real length of the frame
                   is longer or equal to the length in "Stream[0].iLenPartB" */
                StPos.iOffsetHigh += Param.iNumBitsHierarchFrameTotal;
                StPos.iOffsetLow += Param.iNumBitsHierarchFrameTotal -
                                    /* We have to subtract this because we added it in the
                                       for loop above which we do not need here */
                                    Param.Stream[0].iLenPartB * SIZEOF__BYTE;
            }
        }

        /* Possibility check ------------------------------------------------ */
        /* Test, if parameters have possible values */
        if ((StPos.iOffsetHigh + StPos.iLenHigh > Param.iNumDecodedBitsMSC) ||
                (StPos.iOffsetLow + StPos.iLenLow > Param.iNumDecodedBitsMSC))
        {
            /* Something is wrong, set everything to zero */
            StPos.iOffsetLow = 0;
            StPos.iOffsetHigh = 0;
            StPos.iLenLow = 0;
            StPos.iLenHigh = 0;
        }
    }

    return StPos;
}

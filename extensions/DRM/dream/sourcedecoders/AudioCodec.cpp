/******************************************************************************\
 *
 * Copyright (c) 2013
 *
 * Author(s):
 *  David Flamand
 *
 * Description:
 *  Audio codec base class
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

#include "AudioCodec.h"
#include "null_codec.h"

#ifdef HAVE_OPUS
# include "opus_codec.h"
#endif

#ifdef HAVE_LIBFDK_AAC
# include "fdk_aac_codec.h"
#endif

#include <unistd.h>

CAudioCodec::CAudioCodec():pFile(nullptr)
{
}

CAudioCodec::~CAudioCodec() {
}

vector<CAudioCodec*>
CAudioCodec::CodecList;

int
CAudioCodec::RefCount = 0;

void
CAudioCodec::InitCodecList()
{
	if (CodecList.size() == 0)
	{
		/* Null codec, MUST be the first */
		CodecList.push_back(new NullCodec);

		/* AAC */
#ifdef HAVE_LIBFDK_AAC
        CodecList.push_back(new FdkAacCodec);
#endif

		/* Opus */
#ifdef HAVE_OPUS
		CodecList.push_back(new OpusCodec);
#endif
	}
	RefCount ++;
}

void
CAudioCodec::UnrefCodecList()
{
	RefCount --;
	if (!RefCount)
	{
		while (CodecList.size() != 0)
		{
			delete CodecList.back();
			CodecList.pop_back();
		}
	}
}

CAudioCodec*
CAudioCodec::GetDecoder(CAudioParam::EAudCod eAudioCoding, bool bCanReturnNullPtr)
{
    const int size = int(CodecList.size());

	for (int i = 1; i < size; i++)
        if (CodecList[unsigned(i)]->CanDecode(eAudioCoding)) {
            return CodecList[unsigned(i)];
        }
	/* Fallback to null codec */
    return bCanReturnNullPtr ? nullptr : CodecList[0]; // ie the null codec
}

CAudioCodec*
CAudioCodec::GetEncoder(CAudioParam::EAudCod eAudioCoding, bool bCanReturnNullPtr)
{
	const int size = CodecList.size();

	for (int i = 1; i < size; i++)
		if (CodecList[i]->CanEncode(eAudioCoding))
			return CodecList[i];
	/* Fallback to null codec */
    return bCanReturnNullPtr ? nullptr : CodecList[0]; // ie the null codec
}

void
CAudioCodec::Init(const CAudioParam&, int)
{
}

void
CAudioCodec::openFile(const CParameter& Parameters)
{
    if(pFile != nullptr) {
        fclose(pFile);
        pFile = nullptr;
    }
    string fn = fileName(Parameters);
    pFile = fopen(fn.c_str(), "wb");
}

void
CAudioCodec::writeFile(const vector<uint8_t>& audio_frame)
{
    if (pFile!=nullptr)
    {
        size_t iNewFrL = size_t(audio_frame.size()) + 1;
        fwrite(&iNewFrL, size_t(4), size_t(1), pFile);	// frame length
        fwrite(&audio_frame[0], 1, iNewFrL, pFile);	// data
        fflush(pFile);
    }
}

void
CAudioCodec::closeFile() {
    if(pFile != nullptr) {
        fclose(pFile);
        pFile = nullptr;
    }
}

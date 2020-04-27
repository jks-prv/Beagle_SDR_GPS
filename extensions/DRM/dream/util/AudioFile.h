/******************************************************************************\
 * Technische Universitaet Darmstadt, Institut fuer Nachrichtentechnik
 * Copyright (c) 2004
 *
 * Author(s):
 *	Volker Fischer
 *
 * Description:
 *	Class for writing wav-files
 *	
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

#if !defined(AUDIOFILE_H__FD6B234594328533_80UWFB06C2AC__INCLUDED_)
#define AUDIOFILE_H__FD6B234594328533_80UWFB06C2AC__INCLUDED_


/* Classes ********************************************************************/
class CWaveFile
{
public:
	CWaveFile() : pFile(nullptr), iBytesWritten(0) {}
	virtual ~CWaveFile() {if (pFile != nullptr) Close();}

	void Open(const std::string strFileName, const int iSampleRate)
	{
		if (pFile != nullptr)
			Close();

		uint32_t sr = uint32_t(iSampleRate);
		const CWaveHdr WaveHeader =
		{
			/* Use always stereo and PCM */
			{'R', 'I', 'F', 'F'}, 0, {'W', 'A', 'V', 'E'},
			{'f', 'm', 't', ' '}, 16, 1, 2, sr,
			sr * 4 /* same as block align */,
			4 /* block align */, 16,
			{'d', 'a', 't', 'a'}, 0
		};

		pFile = fopen(strFileName.c_str(), "wb");
		if (pFile != nullptr)
		{
			iBytesWritten = sizeof(CWaveHdr);
			(void)fwrite((const void*) &WaveHeader, size_t(sizeof(CWaveHdr)), size_t(1), pFile);
		}
	}

	void AddStereoSample(const _SAMPLE sLeft, const _SAMPLE sRight)
	{
		if (pFile != nullptr)
		{
			iBytesWritten += 2 * sizeof(_SAMPLE);
			(void)fwrite((const void*) &sLeft, size_t(2), size_t(1), pFile);
			(void)fwrite((const void*) &sRight, size_t(2), size_t(1), pFile);
		}
	}

	void UpdateHeader()
	{
		if (pFile != nullptr)
		{
			const uint32_t iFileLength = iBytesWritten - 8;
			fseek(pFile, 4 /* offset */, SEEK_SET /* origin */);
			(void)fwrite((const void*) &iFileLength, size_t(4), size_t(1), pFile);

			const uint32_t iDataLength = iBytesWritten - sizeof(CWaveHdr);
			fseek(pFile, 40 /* offset */, SEEK_SET /* origin */);
			(void)fwrite((const void*) &iDataLength, size_t(4), size_t(1), pFile);

			fseek(pFile, 0 /* offset */, SEEK_END /* origin */);
		}
	}


	void Close()
	{
		if (pFile != nullptr)
		{
	        UpdateHeader();
			fclose(pFile);
			pFile = nullptr;
		}
	}

protected:
	struct CWaveHdr
	{
		/* Wave header struct */
		char cMainChunk[4]; /* "RIFF" */
		uint32_t length; /* Length of file */
		char cChunkType[4]; /* "WAVE" */
		char cSubChunk[4]; /* "fmt " */
		uint32_t cLength; /* Length of cSubChunk (always 16 bytes) */
		uint16_t iFormatTag; /* waveform code: PCM */
		uint16_t iChannels; /* Number of channels */
		uint32_t iSamplesPerSec; /* Sample-rate */
		uint32_t iAvgBytesPerSec;
		uint16_t iBlockAlign; /* Bytes per sample */
		uint16_t iBitsPerSample;
		char cDataChunk[4]; /* "data" */
		uint32_t iDataLength; /* Length of data */
	};

	FILE*		pFile;
	uint32_t	iBytesWritten;
};


#endif // AUDIOFILE_H__FD6B234594328533_80UWFB06C2AC__INCLUDED_

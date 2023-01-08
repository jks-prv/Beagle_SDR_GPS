/******************************************************************************\
 * Technische Universitaet Darmstadt, Institut fuer Nachrichtentechnik
 * Copyright (c) 2001
 *
 * Author(s):
 *	Volker Fischer, Andrea Russo, Doyle Richard, Julian Cable
 *
 * Description:
 *	DAB MOT interface implementation
 *
 * 12/22/2003 Doyle Richard
 *	- Header extension decoding
 * 10/13/2005 Andrea Russo
 *	- Broadcast WebSite application
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

#include "DRM.h"
#include "DABMOT.h"
#include "../util/Utilities.h"
#include <algorithm>
#include <assert.h>
#include <cctype>
#include <cstring>
#include <fstream>
#include <iostream>
#ifdef HAVE_LIBZ
#include <zlib.h>
#else
#ifdef HAVE_LIBFREEIMAGE
# include <FreeImage.h>
#endif
#endif

/* Implementation *************************************************************/
ostream & operator<<(ostream & out, CDateAndTime & d)
{
	d.dump(out);
	return out;
}

ostream & operator<<(ostream & out, CMOTObject & o)
{
	o.dump(out);
	return out;
}

ostream & operator<<(ostream & out, CMOTDirectory & o)
{
	o.dump(out);
	return out;
}

/******************************************************************************\
* Encoder                                                                      *
\******************************************************************************/
void
CMOTDABEnc::SetMOTObject(CMOTObject & NewMOTObject)
{
	size_t i;
	CMOTObjectRaw MOTObjectRaw;

	/* Get some necessary parameters of object */
	const int iPicSizeBits = NewMOTObject.vecbRawData.Size();
	const int iPicSizeBytes = iPicSizeBits / SIZEOF__BYTE;
	const string strFileName = NewMOTObject.strName;

	/* File name size is restricted (in this implementation) to 128 (2^7) bytes.
	   If file name is longer, cut it. TODO: better solution: set Ext flag in
	   "ContentName" header extension to allow larger file names */
	size_t iFileNameSize = strFileName.size();
	if (iFileNameSize > 128)
		iFileNameSize = 128;

	/* Copy actual raw data of object */
	MOTObjectRaw.Body.Init(iPicSizeBits);
	MOTObjectRaw.Body = NewMOTObject.vecbRawData;

	/* Get content type and content sub type of object. We use the format string
	   to get these informations about the object */
	int iContentType = 0;		/* Set default value (general data) */
	int iContentSubType = 0;	/* Set default value (gif) */

	/* Get ending string which declares the type of the file. Make lowercase */

	string strFormat;
	strFormat.resize(NewMOTObject.strFormat.size());
	transform(NewMOTObject.strFormat.begin(), NewMOTObject.strFormat.end(),
			  strFormat.begin(), (int (*)(int)) tolower);

	/* gif: 0, image: 2 */
	if (strFormat.compare("gif") == 0)
	{
		iContentType = 2;
		iContentSubType = 0;
	}

	/* jfif: 1, image: 2. Possible endings: jpg, jpeg, jfif */
	if ((strFormat.compare("jpg") == 0) ||
		(strFormat.compare("jpeg") == 0) || (strFormat.compare("jfif") == 0))
	{
		iContentType = 2;
		iContentSubType = 1;
	}

	/* bmp: 2, image: 2 */
	if (strFormat.compare("bmp") == 0)
	{
		iContentType = 2;
		iContentSubType = 2;
	}

	/* png: 3, image: 2 */
	if (strFormat.compare("png") == 0)
	{
		iContentType = 2;
		iContentSubType = 3;
	}

	/* Header --------------------------------------------------------------- */
	/* Header size (including header extension) */
	const int iHeaderSize = 7 /* Header core  */  +
		5 /* TriggerTime */  +
		3 + iFileNameSize /* ContentName (header + actual name) */  +
		2 /* VersionNumber */ ;

	/* Allocate memory and reset bit access */
	MOTObjectRaw.Header.Init(iHeaderSize * SIZEOF__BYTE);
	MOTObjectRaw.Header.ResetBitAccess();

	/* BodySize: This 28-bit field, coded as an unsigned binary number,
	   indicates the total size of the body in bytes */
	MOTObjectRaw.Header.Enqueue((uint32_t) iPicSizeBytes, 28);

	/* HeaderSize: This 13-bit field, coded as an unsigned binary number,
	   indicates the total size of the header in bytes */
	MOTObjectRaw.Header.Enqueue((uint32_t) iHeaderSize, 13);

	/* ContentType: This 6-bit field indicates the main category of the body's
	   content */
	MOTObjectRaw.Header.Enqueue((uint32_t) iContentType, 6);

	/* ContentSubType: This 9-bit field indicates the exact type of the body's
	   content depending on the value of the field ContentType */
	MOTObjectRaw.Header.Enqueue((uint32_t) iContentSubType, 9);

	/* Header extension ----------------------------------------------------- */
	/* MOT Slideshow application: Only the MOT parameter ContentName is
	   mandatory and must be used for each slide object that will be handled by
	   the MOT decoder and the memory management of the Slide Show terminal */

	/* TriggerTime: This parameter specifies the time for when the presentation
	   takes place. The TriggerTime activates the object according to its
	   ContentType. The value of the parameter field is coded in the UTC
	   format */

	/* PLI (Parameter Length Indicator): This 2-bit field describes the total
	   length of the associated parameter. In this case:
	   1 0 total parameter length = 5 bytes; length of DataField is 4 bytes */
	MOTObjectRaw.Header.Enqueue((uint32_t) 2, 2);

	/* ParamId (Parameter Identifier): This 6-bit field identifies the
	   parameter. 1 0 1 (dec: 5) -> TriggerTime */
	MOTObjectRaw.Header.Enqueue((uint32_t) 5, 6);

	/* Validity flag = 0: "Now", MJD and UTC shall be ignored and be set to 0.
	   Set MJD and UTC to zero. UTC flag is also zero -> short form */
	MOTObjectRaw.Header.Enqueue((uint32_t) 0, 32);

	/* VersionNumber: If several versions of an object are transferred, this
	   parameter indicates its VersionNumber. The parameter value is coded as an
	   unsigned binary number, starting at 0 and being incremented by 1 modulo
	   256 each time the version changes. If the VersionNumber differs, the
	   content of the body was modified */
	/* PLI
	   0 1 total parameter length = 2 bytes, length of DataField is 1 byte */
	MOTObjectRaw.Header.Enqueue((uint32_t) 1, 2);

	/* ParamId (Parameter Identifier): This 6-bit field identifies the
	   parameter. 1 1 0 (dec: 6) -> VersionNumber */
	MOTObjectRaw.Header.Enqueue((uint32_t) 6, 6);

	/* Version number data field */
	MOTObjectRaw.Header.Enqueue((uint32_t) 0, 8);

	/* ContentName: The DataField of this parameter starts with a one byte
	   field, comprising a 4-bit character set indicator (see table 3) and a
	   4-bit Rfa field. The following character field contains a unique name or
	   identifier for the object. The total number of characters is determined
	   by the DataFieldLength indicator minus one byte */

	/* PLI
	   1 1 total parameter length depends on the DataFieldLength indicator */
	MOTObjectRaw.Header.Enqueue((uint32_t) 3, 2);

	/* ParamId (Parameter Identifier): This 6-bit field identifies the
	   parameter. 1 1 0 0 (dec: 12) -> ContentName */
	MOTObjectRaw.Header.Enqueue((uint32_t) 12, 6);

	/* Ext (ExtensionFlag): This 1-bit field specifies the length of the
	   DataFieldLength Indicator.
	   0: the total parameter length is derived from the next 7 bits */
	MOTObjectRaw.Header.Enqueue((uint32_t) 0, 1);

	/* DataFieldLength Indicator: This field specifies as an unsigned binary
	   number the length of the parameter's DataField in bytes. The length of
	   this field is either 7 or 15 bits, depending on the setting of the
	   ExtensionFlag */
	MOTObjectRaw.Header.Enqueue((uint32_t) (1 /* header */  +
											iFileNameSize	/* actual data */
								), 7);

	/* Character set indicator (0 0 0 0 complete EBU Latin based repertoire) */
	MOTObjectRaw.Header.Enqueue((uint32_t) 0, 4);

	/* Rfa 4 bits */
	MOTObjectRaw.Header.Enqueue((uint32_t) 0, 4);

	/* Character field */
	for (i = 0; i < iFileNameSize; i++)
		MOTObjectRaw.Header.Enqueue((uint32_t) strFileName[i], 8);

	/* Generate segments ---------------------------------------------------- */
	/* Header (header should not be partitioned! TODO) */
	const int iPartiSizeHeader = 100;	/* Bytes */// TEST

	PartitionUnits(MOTObjectRaw.Header, MOTObjSegments.vvbiHeader,
				   iPartiSizeHeader);

	/* Body */
	const int iPartiSizeBody = 100;	/* Bytes */// TEST

	PartitionUnits(MOTObjectRaw.Body, MOTObjSegments.vvbiBody,
				   iPartiSizeBody);
}

void
CMOTDABEnc::PartitionUnits(CVector < _BINARY > &vecbiSource,
						   CVector < CVector < _BINARY > >&vecbiDest,
						   const int iPartiSize)
{
	int i, j;
	int iActSegSize;

	/* Divide the generated units in partitions */
	const int iSourceSize = vecbiSource.Size() / SIZEOF__BYTE;
	const int iNumSeg = (int) ceil((_REAL) iSourceSize / iPartiSize);	/* Bytes */
	const int iSizeLastSeg = iSourceSize -
		(int) floor((_REAL) iSourceSize / iPartiSize) * iPartiSize;

	/* Init memory for destination vector, reset bit access of source */
	vecbiDest.Init(iNumSeg);
	vecbiSource.ResetBitAccess();

	for (i = 0; i < iNumSeg; i++)
	{
		/* All segments except the last one must have the size
		   "iPartSizeHeader". If "iSizeLastSeg" is =, the source data size is
		   a multiple of the partitions size. In this case, all units have
		   the same size (-> "|| (iSizeLastSeg == 0)") */
		if ((i < iNumSeg - 1) || (iSizeLastSeg == 0))
			iActSegSize = iPartiSize;
		else
			iActSegSize = iSizeLastSeg;

		/* Add segment data ------------------------------------------------- */
		/* Header */
		/* Allocate memory for body data and segment header bits (16) */
		vecbiDest[i].Init(iActSegSize * SIZEOF__BYTE + 16);
		vecbiDest[i].ResetBitAccess();

		/* Segment header */
		/* RepetitionCount: This 3-bit field indicates, as an unsigned
		   binary number, the remaining transmission repetitions for the
		   current object.
		   In our current implementation, no repetitions used. TODO */
		vecbiDest[i].Enqueue((uint32_t) 0, 3);

		/* SegmentSize: This 13-bit field, coded as an unsigned binary
		   number, indicates the size of the segment data field in bytes */
		vecbiDest[i].Enqueue((uint32_t) iActSegSize, 13);

		/* Body */
		for (j = 0; j < iActSegSize * SIZEOF__BYTE; j++)
			vecbiDest[i].Enqueue(vecbiSource.Separate(1), 1);
	}
}

void
CMOTDABEnc::GenMOTObj(CVector < _BINARY > &vecbiData,
					  CVector < _BINARY > &vecbiSeg, const bool bHeader,
					  const int iSegNum, const int iTranspID,
					  const bool bLastSeg)
{
	int i;
	CCRC CRCObject;

	/* Standard settings for this implementation */
	const bool bCRCUsed = true;	/* CRC */
	const bool bSegFieldUsed = true;	/* segment field */
	const bool bUsAccFieldUsed = true;	/* user access field */
	const bool bTransIDFieldUsed = true;	/* transport ID field */

/* Total length of object in bits */
	int iTotLenMOTObj = 16 /* group header */ ;
	if (bSegFieldUsed)
		iTotLenMOTObj += 16;
	if (bUsAccFieldUsed)
	{
		iTotLenMOTObj += 8;
		if (bTransIDFieldUsed)
			iTotLenMOTObj += 16;
	}
	iTotLenMOTObj += vecbiSeg.Size();
	if (bCRCUsed)
		iTotLenMOTObj += 16;

	/* Init data vector */
	vecbiData.Init(iTotLenMOTObj);
	vecbiData.ResetBitAccess();

	/* MSC data group header ------------------------------------------------ */
	/* Extension flag: this 1-bit flag shall indicate whether the extension
	   field is present, or not. Not used right now -> 0 */
	vecbiData.Enqueue((uint32_t) 0, 1);

	/* CRC flag: this 1-bit flag shall indicate whether there is a CRC at the
	   end of the MSC data group */
	if (bCRCUsed)
		vecbiData.Enqueue((uint32_t) 1, 1);
	else
		vecbiData.Enqueue((uint32_t) 0, 1);

	/* Segment flag: this 1-bit flag shall indicate whether the segment field is
	   present, or not */
	if (bSegFieldUsed)
		vecbiData.Enqueue((uint32_t) 1, 1);
	else
		vecbiData.Enqueue((uint32_t) 0, 1);

	/* User access flag: this 1-bit flag shall indicate whether the user access
	   field is present, or not. We always use this field -> 1 */
	if (bUsAccFieldUsed)
		vecbiData.Enqueue((uint32_t) 1, 1);
	else
		vecbiData.Enqueue((uint32_t) 0, 1);

	/* Data group type: this 4-bit field shall define the type of data carried
	   in the data group data field. Data group types:
	   3: MOT header information
	   4: MOT data */
	if (bHeader)
		vecbiData.Enqueue((uint32_t) 3, 4);
	else
		vecbiData.Enqueue((uint32_t) 4, 4);

	/* Continuity index: the binary value of this 4-bit field shall be
	   incremented each time a MSC data group of a particular type, with a
	   content different from that of the immediately preceding data group of
	   the same type, is transmitted */
	if (bHeader)
	{
		vecbiData.Enqueue((uint32_t) iContIndexHeader, 4);

		/* Increment modulo 16 */
		iContIndexHeader++;
		if (iContIndexHeader == 16)
			iContIndexHeader = 0;
	}
	else
	{
		vecbiData.Enqueue((uint32_t) iContIndexBody, 4);

		/* Increment modulo 16 */
		iContIndexBody++;
		if (iContIndexBody == 16)
			iContIndexBody = 0;
	}

	/* Repetition index: the binary value of this 4-bit field shall signal the
	   remaining number of repetitions of a MSC data group with the same data
	   content, occurring in successive MSC data groups of the same type.
	   No repetition used in this implementation right now -> 0, TODO */
	vecbiData.Enqueue((uint32_t) 0, 4);

	/* Extension field: this 16-bit field shall be used to carry the Data Group
	   Conditional Access (DGCA) when general data or MOT data uses conditional
	   access (Data group types 0010 and 0101, respectively). The DGCA contains
	   the Initialization Modifier (IM) and additional Conditional Access (CA)
	   information. For other Data group types, the Extension field is reserved
	   for future additions to the Data group header.
	   Extension field is not used in this implementation! */

	/* Session header ------------------------------------------------------- */
	/* Segment field */
	if (bSegFieldUsed)
	{
		/* Last: this 1-bit flag shall indicate whether the segment number field
		   is the last or whether there are more to be transmitted */
		if (bLastSeg)
			vecbiData.Enqueue((uint32_t) 1, 1);
		else
			vecbiData.Enqueue((uint32_t) 0, 1);

		/* Segment number: this 15-bit field, coded as an unsigned binary number
		   (in the range 0 to 32767), shall indicate the segment number.
		   NOTE: The first segment is numbered 0 and the segment number is
		   incremented by one at each new segment */
		vecbiData.Enqueue((uint32_t) iSegNum, 15);
	}

	/* User access field */
	if (bUsAccFieldUsed)
	{
		/* Rfa (Reserved for future addition): this 3-bit field shall be
		   reserved for future additions */
		vecbiData.Enqueue((uint32_t) 0, 3);

		/* Transport Id flag: this 1-bit flag shall indicate whether the
		   Transport Id field is present, or not */
		if (bTransIDFieldUsed)
			vecbiData.Enqueue((uint32_t) 1, 1);
		else
			vecbiData.Enqueue((uint32_t) 0, 1);

		/* Length indicator: this 4-bit field, coded as an unsigned binary
		   number (in the range 0 to 15), shall indicate the length n in bytes
		   of the Transport Id and End user address fields.
		   We do not use end user address field, only transport ID -> 2 */
		if (bTransIDFieldUsed)
			vecbiData.Enqueue((uint32_t) 2, 4);
		else
			vecbiData.Enqueue((uint32_t) 0, 4);

		/* Transport Id (Identifier): this 16-bit field shall uniquely identify
		   one data object (file and header information) from a stream of such
		   objects, It may be used to indicate the object to which the
		   information carried in the data group belongs or relates */
		if (bTransIDFieldUsed)
			vecbiData.Enqueue((uint32_t) iTranspID, 16);
	}

	/* MSC data group data field -------------------------------------------- */
	vecbiSeg.ResetBitAccess();

	for (i = 0; i < vecbiSeg.Size(); i++)
		vecbiData.Enqueue(vecbiSeg.Separate(1), 1);

	/* MSC data group CRC --------------------------------------------------- */
	/* The data group CRC shall be a 16-bit CRC word calculated on the data
	   group header, the session header and the data group data field. The
	   generation shall be based on the ITU-T Recommendation X.25.
	   At the beginning of each CRC word calculation, all shift register stage
	   contents shall be initialized to "1". The CRC word shall be complemented
	   (1's complement) prior to transmission */
	if (bCRCUsed)
	{
		/* Reset bit access */
		vecbiData.ResetBitAccess();

		/* Calculate the CRC and put it at the end of the segment */
		CRCObject.Reset(16);

		/* "byLengthBody" was defined in the header */
		for (i = 0; i < iTotLenMOTObj / SIZEOF__BYTE - 2 /* CRC */ ; i++)
			CRCObject.AddByte((_BYTE) vecbiData.Separate(SIZEOF__BYTE));

		/* Now, pointer in "enqueue"-function is back at the same place,
		   add CRC */
		vecbiData.Enqueue(CRCObject.GetCRC(), 16);
	}
}

bool
CMOTDABEnc::GetDataGroup(CVector < _BINARY > &vecbiNewData)
{
	bool bLastSegment;

	/* Init return value. Is overwritten in case the object is ready */
	bool bObjectDone = false;

	if (bCurSegHeader)
	{
		/* Check if this is last segment */
		if (iSegmCntHeader == MOTObjSegments.vvbiHeader.Size() - 1)
			bLastSegment = true;
		else
			bLastSegment = false;

		/* Generate MOT object for header */
		GenMOTObj(vecbiNewData, MOTObjSegments.vvbiHeader[iSegmCntHeader],
				  true, iSegmCntHeader, iTransportID, bLastSegment);

		iSegmCntHeader++;
		if (iSegmCntHeader == MOTObjSegments.vvbiHeader.Size())
		{
			/* Reset counter for body */
			iSegmCntBody = 0;

			/* Header is ready, transmit body now */
			bCurSegHeader = false;
		}
	}
	else
	{
		/* Check that body size is not zero */
		if (iSegmCntBody < MOTObjSegments.vvbiBody.Size())
		{
			/* Check if this is last segment */
			if (iSegmCntBody == MOTObjSegments.vvbiBody.Size() - 1)
				bLastSegment = true;
			else
				bLastSegment = false;

			/* Generate MOT object for Body */
			GenMOTObj(vecbiNewData,
					  MOTObjSegments.vvbiBody[iSegmCntBody], false,
					  iSegmCntBody, iTransportID, bLastSegment);

			iSegmCntBody++;
		}

		if (iSegmCntBody == MOTObjSegments.vvbiBody.Size())
		{
			/* Reset counter for header */
			iSegmCntHeader = 0;

			/* Body is ready, transmit header from next object */
			bCurSegHeader = true;
			iTransportID++;

			/* Signal that old object is done */
			bObjectDone = true;
		}
	}

	/* Return status of object transmission */
	return bObjectDone;
}

_REAL
CMOTDABEnc::GetProgPerc() const
{
/*
	Get percentage of processed data of current object.
*/
	const int
		iTotNumSeg =
		MOTObjSegments.vvbiHeader.Size() + MOTObjSegments.vvbiBody.Size();

	return ((_REAL) iSegmCntBody + (_REAL) iSegmCntHeader) / iTotNumSeg;
}

void
CMOTDABEnc::Reset()
{
	/* Reset continuity indices */
	iContIndexHeader = 0;
	iContIndexBody = 0;
	iTransportID = 0;

	/* Init counter for segments */
	iSegmCntHeader = 0;
	iSegmCntBody = 0;

	/* Init flag which shows what is currently generated, header or body */
	bCurSegHeader = true;		/* Start with header */

	/* Add a "null object" so that at least one valid object can be processed */
	CMOTObject NullObject;
	SetMOTObject(NullObject);
}

/******************************************************************************\
* Decoder                                                                      *
\******************************************************************************/
CMOTDABDec::CMOTDABDec():MOTmode (unknown), MOTHeaders(),
MOTDirectoryEntity(), MOTDirComprEntity(),
MOTDirectory(), MOTCarousel(), qiNewObjects()
{
	assert(qiNewObjects.empty());
}

bool
CMOTDABDec::NewObjectAvailable()
{
	return !qiNewObjects.empty();
}

void
CMOTDABDec::GetNextObject(CMOTObject & NewMOTObject)
{
	TTransportID firstNew;
	firstNew = qiNewObjects.front();
	qiNewObjects.pop();
	NewMOTObject = MOTCarousel[firstNew];
}

void
CMOTDABDec::DeliverIfReady(TTransportID TransportID)
{
	CMOTObject & o = MOTCarousel[TransportID];
	if ((!o.bComplete) && o.bHasHeader && o.Body.Ready())
	{
		o.bComplete = true;
		if (o.Body.IsZipped())
		{
			if (o.Body.uncompress() == false)
				/* Can't unzip so change the filename */
				o.strName = string(o.strName.c_str()) + ".gz";
		}
		//cerr << o << endl;;
		//ostringstream ss; ss << o << endl;
		ofstream file;
        string fn = "/tmp/kiwi.data/drm.ch"+ to_string(DRM_rx_chan()) +"_"+ o.strName;
        printf("DRM write file <%s>\n", fn.c_str());
		file.open(fn.c_str());
		file.write((char*)&o.Body.vecData[0], o.Body.vecData.Size());
		file.close();
		DRM_msg_encoded(DRM_MSG_SLIDESHOW, "drm_slideshow_cb", (char *) o.strName.c_str());
	}
}

void
CMOTDABDec::AddDataUnit(CVector < _BINARY > &vecbiNewData)
{
	int iLenGroupDataField;
	CCRC CRCObject;
	bool bCRCOk;
	int iSegmentNum;
	_BINARY biLastFlag;
	_BINARY biTransportIDFlag = 0;
	int iLenIndicat;
	int iSegmentSize;
	TTransportID TransportID = -1;

	/* Get length of data unit */
	iLenGroupDataField = vecbiNewData.Size();

	/* CRC check ------------------------------------------------------------ */
	/* We do the CRC check at the beginning no matter if it is used or not
	   since we have to reset bit access for that */
	/* Reset bit extraction access */
	vecbiNewData.ResetBitAccess();

	/* Check the CRC of this packet */
	CRCObject.Reset(16);

	/* "- 2": 16 bits for CRC at the end */
	for (size_t i = 0; i < size_t(iLenGroupDataField / SIZEOF__BYTE) - 2; i++)
		CRCObject.AddByte((_BYTE) vecbiNewData.Separate(SIZEOF__BYTE));

	bCRCOk = CRCObject.CheckCRC(vecbiNewData.Separate(16));

	/* MSC data group header ------------------------------------------------ */
	/* Reset bit extraction access */
	vecbiNewData.ResetBitAccess();

	/* Extension flag */
	const _BINARY biExtensionFlag = (_BINARY) vecbiNewData.Separate(1);

	/* CRC flag */
	const _BINARY biCRCFlag = (_BINARY) vecbiNewData.Separate(1);

	/* Segment flag */
	const _BINARY biSegmentFlag = (_BINARY) vecbiNewData.Separate(1);

	/* User access flag */
	const _BINARY biUserAccFlag = (_BINARY) vecbiNewData.Separate(1);

	/* Data group type */
	const int iDataGroupType = (int) vecbiNewData.Separate(4);

	/* Continuity index (not yet used) */
	vecbiNewData.Separate(4);

	/* Repetition index (not yet used) */
	vecbiNewData.Separate(4);

	/* Extension field (not used) */
	if (biExtensionFlag == 1)
		vecbiNewData.Separate(16);

	/* Session header ------------------------------------------------------- */
	/* Segment field */
	if (biSegmentFlag == 1)
	{
		/* Last */
		biLastFlag = (_BINARY) vecbiNewData.Separate(1);

		/* Segment number */
		iSegmentNum = (int) vecbiNewData.Separate(15);
	}
	else
	{
		biLastFlag = 0;
		iSegmentNum = 0;
	}

	/* User access field */
	if (biUserAccFlag == 1)
	{
		/* Rfa (Reserved for future addition) */
		vecbiNewData.Separate(3);

		/* Transport Id flag */
		biTransportIDFlag = (_BINARY) vecbiNewData.Separate(1);

		/* Length indicator */
		iLenIndicat = (int) vecbiNewData.Separate(4);

		/* Transport Id */
		if (biTransportIDFlag == 1)
			TransportID = (int) vecbiNewData.Separate(16);

		/* End user address field (not used) */
		int iLenEndUserAddress;

		if (biTransportIDFlag == 1)
			iLenEndUserAddress = (iLenIndicat - 2) * SIZEOF__BYTE;
		else
			iLenEndUserAddress = iLenIndicat * SIZEOF__BYTE;

		vecbiNewData.Separate(iLenEndUserAddress);
	}
	//cerr << "MOT: new data unit, tid " << TransportID << " CRC " << bCRCOk << " DG" << iDataGroupType << endl;

	/* MSC data group data field -------------------------------------------- */
	/* If CRC is not used enter if-block, if CRC flag is used, it must be ok to
	   enter the if-block */
	if ((biCRCFlag == 0) || ((biCRCFlag == 1) && (bCRCOk)))
	{
		/* Segmentation header ---------------------------------------------- */
		/* Repetition count (not used) */
		int repetition_count = vecbiNewData.Separate(3);

		(void)repetition_count; /* silence warnings */

		/* Segment size */
		iSegmentSize = (int) vecbiNewData.Separate(13);

		/* Get MOT data ----------------------------------------------------- */
		/* Segment number and user access data is needed */
		if ((biSegmentFlag == 1) && (biUserAccFlag == 1) &&
			(biTransportIDFlag == 1))
		{
			/* don't make any assumptions about the order or interleaving of
			   Data Groups from the same or different objects.
			 */

			/* Distinguish between header and body */
			if (iDataGroupType == 3)
			{
				/* Header information, i.e. the header core and the header
				   extension, are transferred in Data Group type 3 */

				if (MOTmode != headerMode)
				{
					/* mode change, throw away any directory */
					MOTDirectory.Reset();
					//cout << "Reset " << MOTDirectory << endl;
				}
				MOTmode = headerMode;

				/* in theory, there can be only one header at a time, but
				   lets be a bit more tolerant
				 */
				if (MOTHeaders.count(TransportID) == 0)
				{
					CBitReassembler o;
					MOTHeaders[TransportID] = o;
				}
				CBitReassembler & o = MOTHeaders[TransportID];
				o.AddSegment(vecbiNewData, iSegmentSize, iSegmentNum,
							 biLastFlag);
				/* if the header just got complete, see if the body is complete */
				if (o.Ready())
				{
					MOTCarousel[TransportID].AddHeader(o.vecData);
                    //printf("HDR iDataGroupType=3.DeliverIfReady TransportID=%x\n", TransportID);
					DeliverIfReady(TransportID);
				}
			}
			else if (iDataGroupType == 4)
			{
				/* Body data segments are transferred in Data Group type 4 */

				/* don't worry about modes, just store everything in the carousel
				   defer decisions until the object and either header or directory
				   are complete
				 */
				map < TTransportID, CMOTObject >::iterator o =
					MOTCarousel.find(TransportID);

				if (o == MOTCarousel.end())
				{
					/* we never saw this before */
					MOTCarousel[TransportID].TransportID = TransportID;
					o = MOTCarousel.find(TransportID);
				}
				/* is this an old object we have completed? */
				if (o->second.bComplete == false)
				{
					o->second.Body.AddSegment(vecbiNewData, iSegmentSize,
											  iSegmentNum, biLastFlag);
				}
				else
				{
					/* discard the segment */
					vecbiNewData.Separate(iSegmentSize * SIZEOF__BYTE);
				}

				/* if the body just got complete we can see if its ready to deliver */
                //printf("DATA iDataGroupType=4.DeliverIfReady TransportID=%x\n", TransportID);
				DeliverIfReady(TransportID);
			}
			else if (iDataGroupType == 6)	/* MOT directory */
			{
				if (MOTmode != directoryMode)
				{
					/* mode change, throw away any headers */
					MOTHeaders.clear();
					MOTDirectory.TransportID = -1;	/* forced reset */
					MOTmode = directoryMode;
					//cout << "Reset " << MOTDirectory << endl;
				}

				/* The carousel is changing */
				if (MOTDirectory.TransportID != TransportID)
				{
					/* we never got all the previous directory */
					cout << " we never got all the previous directory " << TransportID << ", " << MOTDirectory.  TransportID << endl;
					MOTDirectory.Reset();
					MOTDirectory.TransportID = TransportID;
					MOTDirectoryEntity.Reset();
					MOTDirComprEntity.Reset();
				}

				if ((MOTDirectory.TransportID != TransportID) ||
					((MOTDirectory.TransportID == TransportID)
						&& (!MOTDirectoryEntity.Ready())))
				{

					/* Handle the new segment */

					/* rely on the Add routine to check duplicates, set ready, etc. */
					MOTDirectoryEntity.AddSegment(vecbiNewData,
												  iSegmentSize,
												  iSegmentNum, biLastFlag);
					/* have we got the full directory ? */
					if (MOTDirectoryEntity.Ready())
					{
						//cout << "Ready " << MOTDirectory << endl;
						ProcessDirectory(MOTDirectoryEntity);
						MOTDirectory.TransportID = TransportID;
					}			/* END IF HAVE ALL OF THE NEW DIRECTORY */
				}
				else
				{
					vecbiNewData.Separate(iSegmentSize * SIZEOF__BYTE);
				}
			}					/* of DG type 6 */
#if 0							//Commented until we can test it with a real compressed directory
			else if (iDataGroupType == 7)	/* MOT directory compressed */
			{
				if (MOTmode != directoryMode)
				{
					/* mode change, throw away any headers */
					MOTHeaders.clear();
					MOTDirectory.TransportID = -1;	/* forced reset */
					MOTmode = directoryMode;
				}

				/* The carousel is changing */
				if (MOTDirectory.TransportID != TransportID)
				{
					/* we never got all the previous directory */
					MOTDirectory.Reset();
					MOTDirectory.TransportID = TransportID;
					MOTDirectoryEntity.Reset();
					MOTDirComprEntity.Reset();
				}

				if ((MOTDirectory.TransportID != TransportID) ||
					((MOTDirectory.TransportID == TransportID)
						&& (!MOTDirComprEntity.Ready())))
				{
					/* Handle the new segment */

					/* rely on the Add routine to check duplicates, set ready, etc. */
					MOTDirComprEntity.AddSegment(vecbiNewData,
												 iSegmentSize,
												 iSegmentNum, biLastFlag);
					/* have we got the full directory ? */
					if (MOTDirComprEntity.Ready())
					{
						/* uncompress data and extract directory */

						/* Compression Flag - must be 1 */

						const bool bCompressionFlag =
							MOTDirComprEntity.vecData.
							Separate(1) ? true : false;

						/* rfu */
						MOTDirComprEntity.vecData.Separate(1);

						/* EntitySize */
						MOTDirComprEntity.vecData.Separate(30);

						/* CompressionID */
						MOTDirComprEntity.vecData.Separate(8);

						/* rfu */
						MOTDirComprEntity.vecData.Separate(2);

						/* Uncompressed DataLength */
						MOTDirComprEntity.vecData.Separate(30);

						CBitReassembler MOTDirectoryData;

						MOTDirectoryData.vecData =
							MOTDirComprEntity.vecData.
							Separate(MOTDirComprEntity.vecData.Size() -
									 (9 * SIZEOF__BYTE));

						if (bCompressionFlag
							&& MOTDirectoryData.IsZipped()
							&& MOTDirectoryData.uncompress())
						{
							ProcessDirectory(MOTDirectoryData);

							MOTDirectory.TransportID = TransportID;
						}
						else
						{
							/* reset */
							MOTDirComprEntity.Reset();
						}

					}			/* END IF HAVE ALL OF THE NEW DIRECTORY */
				}
				else
				{
					vecbiNewData.Separate(iSegmentSize * SIZEOF__BYTE);
				}
			}					/* of DG type 7 */
#endif
			else
			{
				vecbiNewData.Separate(iSegmentSize * SIZEOF__BYTE);
			}
		}
	}
}

void
CMOTDABDec::ProcessDirectory(CBitReassembler & MOTDir)
{
	MOTDirectory.AddHeader(MOTDir.vecData);

	/* first reset the "I'm in the carousel" flag for all objects
	   and set the "I was in the carousel for the relevant objects
	   then set the "I'm in the carousel" flag for all objects in the
	   new directory.
	   Then delete the objects from the carousel that need deleting
	   leave objects alone that were never in a carousel, they might
	   become valid later. */

	/* save bodies of old carousel */
	map < TTransportID, CByteReassembler > Body;
	for (map < TTransportID,
		 CMOTObject >::iterator m =
		 MOTCarousel.begin(); m != MOTCarousel.end(); m++)
	{
		Body[m->first] = m->second.Body;
	}
	MOTCarousel.erase(MOTCarousel.begin(), MOTCarousel.end());

	//cout << "decode directory " << MOTDirectory.  iNumberOfObjects << " === " << MOTDirectory.vecObjects.size() << " {";

	MOTDirectory.vecObjects.clear();

	for (size_t i = 0; i < size_t(MOTDirectory.iNumberOfObjects); i++)
	{
		/* add each header to the carousel */
		TTransportID tid = (TTransportID) MOTDir.vecData.Separate(16);
		MOTCarousel[tid].AddHeader(MOTDir.vecData);
		/* keep any bodies or body fragments previously received */
		map < TTransportID, CByteReassembler >::iterator b = Body.find(tid);
		if (b != Body.end())
			MOTCarousel[tid].Body = b->second;
		/* mark objects which are in the new directory */
		MOTDirectory.vecObjects.push_back(tid);
		//cout << tid << " ";
        //printf("ProcessDirectory.DeliverIfReady tid=%x\n", tid);
		DeliverIfReady(tid);
	}
	//cout << "}" << endl;

}

void
CDateAndTime::extract_absolute(CVector < _BINARY > &vecbiData)
{
	vecbiData.Separate(1);		/* rfa */
	CModJulDate ModJulDate(vecbiData.Separate(17));
	day = ModJulDate.GetDay();
	month = ModJulDate.GetMonth();
	year = ModJulDate.GetYear();
	vecbiData.Separate(1);		/* rfa */
	lto_flag = (int) vecbiData.Separate(1);
	utc_flag = (int) vecbiData.Separate(1);
	hours = (int) vecbiData.Separate(5);
	minutes = (int) vecbiData.Separate(6);
	if (utc_flag != 0)
	{
		seconds = (int) vecbiData.Separate(6);
		vecbiData.Separate(10);	/* rfa */
	}
	else
	{
		seconds = 0;
	}
	if (lto_flag != 0)
	{
		vecbiData.Separate(2);	/* rfa */
		int sign = (int) vecbiData.Separate(1);
		half_hours = (int) vecbiData.Separate(5);
		if (sign == 1)
			half_hours = 0 - half_hours;
	}
	else
	{
		half_hours = 0;
	}
}

void
CDateAndTime::extract_relative(CVector < _BINARY > &vecbiData)
{
	int granularity = (int) vecbiData.Separate(2);
	int interval = (int) vecbiData.Separate(6);
	time_t t = time(nullptr);
	switch (granularity)
	{
	case 0:
		t += 2 * 60 * interval;
		break;
	case 1:
		t += 30 * 60 * interval;
		break;
	case 2:
		t += 2 * 60 * 60 * interval;
		break;
	case 3:
		t += 24 * 60 * 60 * interval;
		break;
	}
	struct tm *tmp = gmtime(&t);
	year = tmp->tm_year;
	month = tmp->tm_mon;
	day = tmp->tm_mday;
	hours = tmp->tm_hour;
	minutes = tmp->tm_min;
	seconds = tmp->tm_sec;
}

void
CDateAndTime::dump(ostream & out)
{
	out << year << '/' << uint16_t(month) << '/' << uint16_t(day);
	out << " " << hours << ':' << minutes << ':' << seconds;
	out << " flags: " << utc_flag << ':' << lto_flag << ':' << half_hours;
}

void
CMOTObjectBase::decodeExtHeader(_BYTE & bParamId, int &iHeaderFieldLen,
								int &iDataFieldLen,
								CVector < _BINARY > &vecbiHeader) const
{
	int iPLI = (int) vecbiHeader.Separate(2);
	bParamId = (unsigned char) vecbiHeader.Separate(6);

	iHeaderFieldLen = 1;

	switch (iPLI)
	{
	case 0:
		/* Total parameter length = 1 byte; no DataField
		   available */
		iDataFieldLen = 0;
		break;

	case 1:
		/* Total parameter length = 2 bytes, length of DataField
		   is 1 byte */
		iDataFieldLen = 1;
		break;

	case 2:
		/* Total parameter length = 5 bytes; length of DataField
		   is 4 bytes */
		iDataFieldLen = 4;
		break;

	case 3:
		/* Total parameter length depends on the DataFieldLength
		   indicator (the maximum parameter length is
		   32770 bytes) */

		/* Ext (ExtensionFlag): This 1-bit field specifies the
		   length of the DataFieldLength Indicator and is coded
		   as follows:
		   - 0: the total parameter length is derived from the
		   next 7 bits;
		   - 1: the total parameter length is derived from the
		   next 15 bits */
		_BINARY biExt = (_BINARY) vecbiHeader.Separate(1);

		/* Get data field length */
		if (biExt == 0)
		{
			iDataFieldLen = (int) vecbiHeader.Separate(7);
			iHeaderFieldLen++;
		}
		else
		{
			iDataFieldLen = (int) vecbiHeader.Separate(15);
			iHeaderFieldLen += 2;
		}
	}
}

void
CReassembler::cachelast(CVector < _BYTE > &vecDataIn, size_t iSegSize)
{
	vecLastSegment.Init(iSegSize);
	for (size_t i = 0; i < iSegSize; i++)
		vecLastSegment[i] = vecDataIn.Separate(8);
}

void
CReassembler::copyin(CVector < _BYTE > &vecDataIn, size_t iSegNum,
					 size_t bytes)
{
	size_t offset = iSegNum * iSegmentSize;
	size_t iNewSize = offset + bytes;
	if (size_t(vecData.Size()) < iNewSize)
		vecData.Enlarge(iNewSize - vecData.Size());
	for (size_t i = 0; i < bytes; i++)
		vecData[offset + i] = vecDataIn.Separate(8);
}

void
CReassembler::AddSegment(CVector < _BYTE > &vecDataIn,
						 int iSegSize, int iSegNum, bool bLast)
{
	if (bLast)
	{
		if (iLastSegmentNum == -1)
		{
			iLastSegmentNum = iSegNum;
			iLastSegmentSize = iSegSize;
			/* three cases:
			   1: single segment - easy! (actually degenerate with case 3)
			   2: multi-segment and the last segment came first.
			   3: normal - some segment, not the last, came first,
			   we know the segment size
			 */
			if (iSegNum == 0)
			{					/* case 1 */
				copyin(vecDataIn, 0, iSegSize);
			}
			else if (iSegmentSize == 0)
			{					/* case 2 */
				cachelast(vecDataIn, iSegSize);
			}
			else
			{					/* case 3 */
				copyin(vecDataIn, iSegNum, iSegSize);
			}
		}						/* otherwise do nothing as we already have the last segment */
	}
	else
	{
		iSegmentSize = iSegSize;
		if (Tracker.HaveSegment(iSegNum) == false)
		{
			copyin(vecDataIn, iSegNum, iSegSize);
		}
	}
	Tracker.AddSegment(iSegNum);	/* tracking the last segment makes the Ready work! */

	if ((iLastSegmentSize != -1)	/* we have the last segment */
		&& (bReady == false)	/* we haven't already completed reassembly */
		&& Tracker.Ready()		/* there are no gaps */
		)
	{
		if (vecLastSegment.Size() > 0)
		{
			/* we have everything, but the last segment came first */
			copylast();
		}
		bReady = true;
	}
	//qDebug("AddSegment %d last %d ready %d\n", iSegNum, bLast?1:0, bReady?1:0);
}

void
CReassembler::copylast()
{
	size_t offset = iLastSegmentNum * iSegmentSize;
	vecData.Enlarge(vecLastSegment.Size());
	for (size_t i = 0; i < size_t(vecLastSegment.Size()); i++)
		vecData[offset + i] = vecLastSegment[i];
	vecLastSegment.Init(0);
}

void
CBitReassembler::cachelast(CVector < _BYTE > &vecDataIn, size_t iSegSize)
{
	vecLastSegment.Init(8 * iSegSize);
	vecLastSegment.ResetBitAccess();
	for (size_t i = 0; i < 8 * iSegSize; i++)
		vecLastSegment[i] = vecDataIn.Separate(1);
}

void
CBitReassembler::copyin(CVector < _BYTE > &vecDataIn, size_t iSegNum,
						size_t bytes)
{
	size_t offset = iSegNum * 8 * iSegmentSize;
	size_t bits = 8 * bytes;
	size_t iNewSize = offset + bits;
	if (size_t(vecData.Size()) < iNewSize)
		vecData.Enlarge(iNewSize - vecData.Size());
	for (size_t i = 0; i < bits; i++)
		vecData[offset + i] = vecDataIn.Separate(1);
}

void
CBitReassembler::copylast()
{
	size_t offset = iLastSegmentNum * 8 * iSegmentSize;
	vecData.Enlarge(vecLastSegment.Size());
	for (size_t i = 0; i < size_t(vecLastSegment.Size()); i++)
		vecData[offset + i] = vecLastSegment[i];
	vecLastSegment.Init(0);
	vecLastSegment.ResetBitAccess();
}

string
CMOTObjectBase::extractString(CVector < _BINARY > &vecbiData, int iLen) const
{
	string strVar;
	for (size_t i = 0; i < size_t(iLen); i++)
	{
		strVar += (char) vecbiData.Separate(SIZEOF__BYTE);
	}
	return strVar;
}

void
CMOTDirectory::Reset ()
{
    CMOTObjectBase::Reset ();
    bSortedHeaderInformation = false;
    DirectoryIndex.clear ();
    bCompressionFlag = false;
    iCarouselPeriod = -1;
    iNumberOfObjects = 0;
    iSegmentSize = 0;
}


void
CMOTDirectory::AddHeader(CVector < _BINARY > &vecbiHeader)
{
	int iDirectorySize;

	/* compression flag - must be zero */
	bCompressionFlag = vecbiHeader.Separate(1) ? true : false;

	/* rfu */
	vecbiHeader.Separate(1);

	/* Directory size: not used */
	iDirectorySize = (int) vecbiHeader.Separate(30); (void)iDirectorySize;

	/* Number of objects */
	iNumberOfObjects = (int) vecbiHeader.Separate(16);

	/* Carousel period */
	iCarouselPeriod = (int) vecbiHeader.Separate(24);

	/*  rfu */
	vecbiHeader.Separate(1);

	/* Rfa: not used */
	vecbiHeader.Separate(2);

	/* Segment size: not used */
	iSegmentSize = (int) vecbiHeader.Separate(13);
	/* Directory extension length */
	int iDirectoryExtensionLength = (int) vecbiHeader.Separate(16);

	/* Use all header extension data blocks */
	int iSizeRec = iDirectoryExtensionLength;

	/* Use all header extension data blocks */
	while (iSizeRec > 0)
	{
		_BYTE bParamId;
		int iHdrFieldLen, iDataFieldLen, iTmp;

		decodeExtHeader(bParamId, iHdrFieldLen, iDataFieldLen, vecbiHeader);

		iSizeRec -= (iHdrFieldLen + iDataFieldLen);

		switch (bParamId)
		{
		case 0:				/* SortedHeaderInformation */
			bSortedHeaderInformation = true;
			break;
		case 1:				/* DefaultPermitOutdatedVersions */
			iTmp = (int) vecbiHeader.Separate(iDataFieldLen * SIZEOF__BYTE);
			if (iTmp == 0)
				bPermitOutdatedVersions = false;
			else
				bPermitOutdatedVersions = true;
			break;
		case 9:				/* DefaultExpiration */
			if (iDataFieldLen == 1)
			{
				/* relative */
				ExpireTime.extract_relative(vecbiHeader);
			}
			else
			{
				/* absolute */
				ExpireTime.extract_absolute(vecbiHeader);
			}
			break;
		case 34:				/* DirectoryIndex */
		{
			_BYTE iProfile = (_BYTE) vecbiHeader.Separate(SIZEOF__BYTE);
			DirectoryIndex[iProfile] =
				extractString(vecbiHeader, iDataFieldLen - 1);
		}
			break;
		default:
			if (iDataFieldLen > 0)
				vecbiHeader.Separate(iDataFieldLen * SIZEOF__BYTE);
			break;
		}
	}
}

bool
CReassembler::IsZipped() const
{
/*
	Check if the header file is a gzip header

	see GZIP file format specification
	http://www.ietf.org/rfc/rfc1952.txt
*/
	if(vecData.Size()<3)
		return false;
	/* Check for gzip header [31, 139, 8] */
	if ((vecData[0] == 31) && (vecData[1] == 139) && (vecData[2] == 8))
		return true;
	else
		return false;
}

unsigned int
CReassembler::gzGetOriginalSize() const
{
/*
	Get the original size from a gzip file
	last 4 bytes contains the original file size (ISIZE)

	see GZIP file format specification
	http://www.ietf.org/rfc/rfc1952.txt
*/
	CVector < _BYTE > byHeader(4);

	const int iLastByte = vecData.Size() - 1;

	byHeader[0] = vecData[iLastByte - 3];
	byHeader[1] = vecData[iLastByte - 2];
	byHeader[2] = vecData[iLastByte - 1];
	byHeader[3] = vecData[iLastByte];

	return byHeader[0] + (byHeader[1] << 8) + (byHeader[2] << 16) +
		(byHeader[3] << 24);
}

bool
CReassembler::uncompress()
{
#ifdef HAVE_LIBZ
	CVector < _BYTE > vecbRawDataOut;
	/* Extract the original file size */
	unsigned long dest_len = gzGetOriginalSize();

	if (dest_len < MAX_DEC_NUM_BYTES_ZIP_DATA)
	{
		vecbRawDataOut.Init(dest_len);

		int zerr;
		z_stream stream;
		memset(&stream, 0, sizeof(stream));
		if ((zerr = inflateInit2(&stream, -MAX_WBITS)) != Z_OK)
			return false;

		/* skip past minimal gzip header */
		stream.next_in = &vecData[10];
		stream.avail_in = vecData.Size() - 10;

		stream.next_out = &vecbRawDataOut[0];
		stream.avail_out = vecbRawDataOut.Size();

		zerr = inflate(&stream, Z_NO_FLUSH);
		dest_len = vecbRawDataOut.Size() - stream.avail_out;

		if (zerr != Z_OK && zerr != Z_STREAM_END)
			return false;

		inflateEnd(&stream);

		if (zerr != Z_OK && zerr != Z_STREAM_END)
			return false;

		vecData.Init(dest_len);
		vecData = vecbRawDataOut;
		return true;
	}
	else
	{
		vecData.Init(0);
		return false;
	}
#else
#	ifdef HAVE_LIBFREEIMAGE
	CVector < _BYTE > vecbRawDataOut;
	CVector < _BYTE > &vecbRawDataIn = vecData;
	/* Extract the original file size */
	const unsigned long dest_len = gzGetOriginalSize();

	if (dest_len < MAX_DEC_NUM_BYTES_ZIP_DATA)
	{
		vecbRawDataOut.Init(dest_len);

		/* Actual decompression call */
		const int zerr = FreeImage_ZLibGUnzip(&vecbRawDataOut[0],
											  dest_len, &vecbRawDataIn[0],
											  vecbRawDataIn.Size());

		if (zerr > 0)
		{
			/* Copy data */
			vecData.Init(zerr);
			vecData = vecbRawDataOut;
			return true;
		}
		else
		{
			vecData.Init(0);
			return false;
		}
	}
	else
	{
		vecData.Init(0);
		return false;
	}
#	else
	return false;
#	endif
#endif
}

static const char *txt[] = { "txt", "txt", "html", 0 };
static const char *img[] = { "gif", "jpeg", "bmp", "png", 0 };
static const char *audio[] = { "mpg", "mp2", "mp3", "mpg", "mp2", "mp3",
	"pcm", "aiff", "atrac", "atrac2", "mp4", 0
};
static const char *video[] = { "mpg", "mp2", "mp4", "h263", 0 };

void
CMOTObject::AddHeader(CVector < _BINARY > &vecbiHeader)
{

	/* HeaderSize and BodySize */
	iBodySize = (int) vecbiHeader.Separate(28);
	size_t iHeaderSize = (size_t) vecbiHeader.Separate(13);

	/* Content type and content sub-type */
	iContentType = (int) vecbiHeader.Separate(6);
	iContentSubType = (int) vecbiHeader.Separate(9);
	/* 7 bytes for header core */
	int iSizeRec = iHeaderSize - 7;

	/* Use all header extension data blocks */
	while (iSizeRec > 0)
	{
		_BYTE bParamId;
		int iHdrFieldLen, iDataFieldLen, iTmp;

		decodeExtHeader(bParamId, iHdrFieldLen, iDataFieldLen, vecbiHeader);

		iSizeRec -= (iHdrFieldLen + iDataFieldLen);

		switch (bParamId)
		{
		case 1:				/* PermitOutdatedVersions */
			iTmp = (int) vecbiHeader.Separate(iDataFieldLen * SIZEOF__BYTE);
			if (iTmp == 0)
				bPermitOutdatedVersions = false;
			else
				bPermitOutdatedVersions = true;
			break;
		case 5:				/* TriggerTime - OBSOLETE */
			/* not decoded - skip */
			(void) vecbiHeader.Separate(iDataFieldLen * SIZEOF__BYTE);
			break;
		case 6:				/* Version - OBSOLETE */
			iVersion = (int) vecbiHeader.Separate(SIZEOF__BYTE);
			break;
		case 7:				/* RetransmissionDistance */
			/* not decoded - skip */
			(void) vecbiHeader.Separate(iDataFieldLen * SIZEOF__BYTE);
			break;
		case 9:				/* Expiration */
			if (iDataFieldLen == 1)
			{
				/* relative */
				ExpireTime.extract_relative(vecbiHeader);
			}
			else
			{
				/* absolute */
				ExpireTime.extract_absolute(vecbiHeader);
			}
			break;
		case 10:				/* Priority */
			/* not decoded - skip */
			(void) vecbiHeader.Separate(iDataFieldLen * SIZEOF__BYTE);
			break;
		case 11:				/* Label */
			/* not decoded - skip */
			(void) vecbiHeader.Separate(iDataFieldLen * SIZEOF__BYTE);
			break;
		case 12:				/* Content Name */
			/* TODO - convert characters to UNICODE */
			iCharacterSetForName = vecbiHeader.Separate(4);
			/* rfa  */
			vecbiHeader.Separate(4);
			strName = extractString(vecbiHeader, iDataFieldLen - 1);
			break;
		case 13:				/* UniqueBodyVersion */
			iUniqueBodyVersion = (int) vecbiHeader.Separate(32);
			break;
		case 15:				/* Content Description */
			iCharacterSetForDescription = vecbiHeader.Separate(4);

			/* rfa */
			vecbiHeader.Separate(4);

			strContentDescription =
				extractString(vecbiHeader, iDataFieldLen - 1);
			break;
		case 16:				/* Mime Type */
			strMimeType = extractString(vecbiHeader, iDataFieldLen);
			break;
		case 17:				/* Compression Type */
			iCompressionType =
				(int) vecbiHeader.Separate(iDataFieldLen * SIZEOF__BYTE);
			break;
		case 0x21:				/* ProfileSubset */
		{
			vecbProfileSubset.clear();
			for (size_t i = 0; i < size_t(iDataFieldLen); i++)
				vecbProfileSubset.push_back((_BYTE) vecbiHeader.
											Separate(SIZEOF__BYTE));
		}
			break;
		case 0x23:				/* CAInfo */
			/* not decoded - skip */
			(void) vecbiHeader.Separate(iDataFieldLen * SIZEOF__BYTE);
			break;
		case 0x24:				/* CAReplacementObject */
			/* not decoded - skip */
			(void) vecbiHeader.Separate(iDataFieldLen * SIZEOF__BYTE);
			break;
		case 0x25:				/* ScopeStart */
			ScopeStart.extract_absolute(vecbiHeader);
			break;
		case 0x26:				/* ScopeEnd */
			ScopeEnd.extract_absolute(vecbiHeader);
			break;
		case 0x27:				/* ScopeId */
			iScopeId = vecbiHeader.Separate(iDataFieldLen * SIZEOF__BYTE);
			break;
		default:
			/* not decoded - skip */
			if (iDataFieldLen > 0)
				(void) vecbiHeader.Separate(iDataFieldLen * SIZEOF__BYTE);
			break;
		}
	}
	/* set strFormat */
	switch (iContentType)
	{
	case 1:
		strFormat = txt[iContentSubType];
		break;
	case 2:
		strFormat = img[iContentSubType];
		break;
	case 3:
		strFormat = audio[iContentSubType];
		break;
	case 4:
		strFormat = video[iContentSubType];
		break;
	}
    //printf("AddHeader iContentType=%d iContentSubType=%d fn=%s fmt=%s\n",
        //iContentType, iContentSubType, strName.c_str(), strFormat.c_str());
	bHasHeader = true;
}

void
CMOTObject::dump(ostream & out)
{
	out << "MOT(" << TransportID << "):";
	out << " Name: " << strName;
	out << ':' << iCharacterSetForName;
	out << " Description: " << strContentDescription;
	out << ':' << iCharacterSetForDescription;
	out << " mime: " << strMimeType;
	out << " content: " << iContentType << '/' << iContentSubType;
	out << " UniqueBodyVersion: " << iUniqueBodyVersion;
	out << " Expires: " << ExpireTime;
	out << " PermitOutdatedVersions: " << bPermitOutdatedVersions;
	out << " Scope: " << hex << iScopeId << dec << " from " << ScopeStart <<
		" to " << ScopeEnd;
	out << " CompressionType: " << iCompressionType;
	out << " Version: " << iVersion;
	out << " Priority: " << iPriority;
	out << " RetransmissionDistance: " << iRetransmissionDistance;
	out << " format: " << strFormat;
	out << " length: " << iBodySize;
}

void
CMOTDirectory::dump(ostream & out)
{
	out << "MOTDIR(" << TransportID << "):";
	out << " Expires: " << ExpireTime;
	out << " PermitOutdatedVersions: " << bPermitOutdatedVersions;
	out << " CarouselPeriod: " << iCarouselPeriod;
	out << " SegmentSize: " << iSegmentSize;
	out << " CompressionFlag: " << bCompressionFlag;
	out << " SortedHeaderInformation: " << bSortedHeaderInformation;
	out << " indices { ";
	for (map < _BYTE, string >::iterator di = DirectoryIndex.begin();
		 di != DirectoryIndex.end(); di++)
    {
		out << hex << int(di->first) << dec << " => " << di->second;
		out.flush();
    }
	out << " }";
	out << " there are " << iNumberOfObjects << " objects {";
	for (size_t i = 0; i < vecObjects.size(); i++)
		out << " " << vecObjects[i];
	out << " }";
}

/******************************************************************************\
 * Technische Universitaet Darmstadt, Institut fuer Nachrichtentechnik
 * Copyright (c) 2001
 *
 * Author(s):
 *	Volker Fischer, Andrea Russo, Julian Cable
 *
 * Description:
 *	See DABMOT.cpp
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

#if !defined(DABMOT_H__3B0UBVE98732KJVEW363E7A0D31912__INCLUDED_)
#define DABMOT_H__3B0UBVE98732KJVEW363E7A0D31912__INCLUDED_

#include "../GlobalDefinitions.h"
#include "../util/Vector.h"
#include "../util/CRC.h"
#include <time.h>
#include <map>
#include <queue>


/* Definitions ****************************************************************/
/* Invalid data segment number. Shall be a negative value since the test for
   invalid data segment number is always "if (iDataSegNum < 0)" */
#define INVALID_DATA_SEG_NUM			-1

/* Maximum number of bytes for zip'ed files. We need to specify this number to
   avoid segmentation faults due to erroneous zip header giving a much too high
   number of bytes */
#define MAX_DEC_NUM_BYTES_ZIP_DATA		1000000	/* 1 MB */

/* Registrered BWS profiles (ETSI TS 101 498-1) */
#define RESERVED_PROFILE	0x00
#define BASIC_PROFILE		0x01
#define TOP_NEWS_PROFILE	0x02
#define UNRESTRICTED_PC_PROFILE 0xFF
/* Classes ********************************************************************/


class CMOTObjectRaw
{
  public:
    CMOTObjectRaw ()
    {
	Reset ();
    }

    CMOTObjectRaw (const CMOTObjectRaw & nOR)
    {
	Header = nOR.Header;
	Body = nOR.Body;
    }

    CMOTObjectRaw & operator= (const CMOTObjectRaw & nOR)
    {
	Header = nOR.Header;
	Body = nOR.Body;
	return *this;
    }

    void Reset ()
    {
	Header.Init (0);
	Body.Init (0);
    }

    CVector < _BINARY > Header;
    CVector < _BINARY > Body;
};

class CDateAndTime
{
  public:

    CDateAndTime():utc_flag(0), lto_flag(0), half_hours(0),
        year(0), month(0), day(0), hours(0), minutes(0), seconds(0)
    {}

    void extract_relative (CVector < _BINARY > &vecbiData);
    void extract_absolute (CVector < _BINARY > &vecbiData);
    void Reset ()
    {
        utc_flag = 0;
        lto_flag = 0;
        half_hours = 0;
        year = 0;
        month = 0;
        day = 0;
        hours = 0;
        minutes = 0;
        seconds = 0;
    }

    void dump(std::ostream& out);

    int utc_flag, lto_flag, half_hours;
    uint16_t year;
    uint8_t month, day;
    int hours, minutes, seconds;
};

typedef int TTransportID;

class CSegmentTracker
{
  public:

    CSegmentTracker ()
    {
	Reset ();
    }

    void Reset ()
    {
	vecbHaveSegment.clear ();
    }

    size_t size ()
    {
	return vecbHaveSegment.size ();
    }

    bool Ready ()
    {
	if (vecbHaveSegment.size () == 0)
	    return false;
	for (size_t i = 0; i < vecbHaveSegment.size (); i++)
	  {
	      if (vecbHaveSegment[i] == false)
		{
		    return false;
		}
	  }
	return true;
    }

    void AddSegment (int iSegNum)
    {
	if ((iSegNum + 1) > int (vecbHaveSegment.size ()))
	    vecbHaveSegment.resize (iSegNum + 1, false);
	vecbHaveSegment[iSegNum] = true;
    }

    bool HaveSegment (int iSegNum)
    {
	if (iSegNum < int (vecbHaveSegment.size ()))
	    return vecbHaveSegment[iSegNum];
	return false;
    }

  protected:
    std::vector < bool > vecbHaveSegment;
};

class CReassembler
{
  public:

	CReassembler(): vecData(), vecLastSegment(),
		iLastSegmentNum(-1), iLastSegmentSize(-1), iSegmentSize(0),
		Tracker(), bReady(false)
    {
    }

    CReassembler (const CReassembler & r):iLastSegmentNum (r.iLastSegmentNum),
	iLastSegmentSize (r.iLastSegmentSize),
	iSegmentSize (r.iSegmentSize), Tracker (r.Tracker), bReady (r.bReady)
    {
	vecData.Init (r.vecData.Size ());
	vecData = r.vecData;
	vecLastSegment.Init (r.vecLastSegment.Size ());
	vecLastSegment = r.vecLastSegment;
    }

    virtual ~ CReassembler ()
    {
    }

    inline CReassembler & operator= (const CReassembler & r)
    {
	iLastSegmentNum = r.iLastSegmentNum;
	iLastSegmentSize = r.iLastSegmentSize;
	iSegmentSize = r.iSegmentSize;
	Tracker = r.Tracker;
	vecData.Init (r.vecData.Size ());
	vecData = r.vecData;
	vecLastSegment.Init (r.vecLastSegment.Size ());
	vecLastSegment = r.vecLastSegment;
	bReady = r.bReady;

	return *this;
    }

    void Reset ()
    {
	vecData.Init (0);
	vecData.ResetBitAccess ();
	vecLastSegment.Init (0);
	vecLastSegment.ResetBitAccess ();
	iLastSegmentNum = -1;
	iLastSegmentSize = -1;
	iSegmentSize = 0;
	Tracker.Reset ();
	bReady = false;
    }

    bool Ready ()
    {
	return bReady;
    }

    void AddSegment (CVector < _BYTE > &vecDataIn,
		     int iSegSize, int iSegNum, bool bLast = false);

    bool IsZipped () const;
    bool uncompress();

    CVector < _BYTE > vecData;

  protected:

    virtual void copyin (CVector < _BYTE > &vecDataIn, size_t iSegNum,
			 size_t bytes);
    virtual void cachelast (CVector < _BYTE > &vecDataIn, size_t iSegSize);
    virtual void copylast ();

    unsigned int gzGetOriginalSize () const;

    CVector < _BYTE > vecLastSegment;
    int iLastSegmentNum;
    int iLastSegmentSize;
    size_t iSegmentSize;
    CSegmentTracker Tracker;
    bool bReady;
};

class CBitReassembler:public CReassembler
{
  public:
    CBitReassembler ():CReassembler ()
    {
    }
    CBitReassembler (const CBitReassembler & r):CReassembler (r)
    {
    }

  protected:
    virtual void copyin (CVector < _BYTE > &vecDataIn, size_t iSegNum,
			 size_t bytes);
    virtual void cachelast (CVector < _BYTE > &vecDataIn, size_t iSegSize);
    virtual void copylast ();
};

typedef CReassembler CByteReassembler;

class CMOTObjectBase
{
  public:

    CMOTObjectBase ():TransportID(-1),ExpireTime(),bPermitOutdatedVersions(false)
    {
		Reset ();
    }

    virtual ~CMOTObjectBase () { }

    virtual void Reset ()
    {
		TransportID = -1;
		ExpireTime.Reset ();
		bPermitOutdatedVersions = false;
    }

    void decodeExtHeader (_BYTE & bParamId,
			  int &iHeaderFieldLen, int &iDataFieldLen,
			  CVector < _BINARY > &vecbiHeader) const;

    std::string extractString (CVector < _BINARY > &vecbiData, int iLen) const;

    TTransportID TransportID;
    CDateAndTime ExpireTime;
    bool bPermitOutdatedVersions;

};

class CMOTDirectory:public CMOTObjectBase
{
  public:

    CMOTDirectory ():CMOTObjectBase(), iCarouselPeriod(0),
    iNumberOfObjects(0), iSegmentSize(0),
    bCompressionFlag(false), bSortedHeaderInformation(false),
    DirectoryIndex(), vecObjects()
    {
    }

    CMOTDirectory (const CMOTDirectory & nD):CMOTObjectBase (nD),
		iCarouselPeriod (nD.iCarouselPeriod),
		iNumberOfObjects (nD.iNumberOfObjects),
		iSegmentSize (nD.iSegmentSize),
		bCompressionFlag (nD.bCompressionFlag),
		bSortedHeaderInformation (nD.bSortedHeaderInformation),
		DirectoryIndex (nD.DirectoryIndex)
    {
    }

    virtual ~CMOTDirectory ()
    {
    }

    inline CMOTDirectory & operator= (const CMOTDirectory & nD)
    {
		TransportID = nD.TransportID;
		ExpireTime = nD.ExpireTime;
		bPermitOutdatedVersions = nD.bPermitOutdatedVersions;
		bSortedHeaderInformation = nD.bSortedHeaderInformation;
		bCompressionFlag = nD.bCompressionFlag;
		iCarouselPeriod = nD.iCarouselPeriod;
		DirectoryIndex = nD.DirectoryIndex;
		iNumberOfObjects = nD.iNumberOfObjects;
		iSegmentSize = nD.iSegmentSize;

		return *this;
    }

    virtual void Reset ();

    virtual void AddHeader (CVector < _BINARY > &vecbiHeader);

    void dump(std::ostream&);

    int iCarouselPeriod, iNumberOfObjects, iSegmentSize;
    bool bCompressionFlag, bSortedHeaderInformation;
    std::map < _BYTE, std::string > DirectoryIndex;
    std::vector < TTransportID > vecObjects;
};

/* we define this to reduce the need for copy constructors and operator=
   since CReassembler has a good set, the defaults will do for this, but not
   for classes with CVector members
*/

class CMOTObject:public CMOTObjectBase
{
  public:

    CMOTObject ():CMOTObjectBase(), vecbRawData(),
    bComplete(false), bHasHeader(false), Body(),
    strName(""), iBodySize(0), iCharacterSetForName(0), iCharacterSetForDescription(0),
    strFormat(""), strMimeType(""), iCompressionType(0), strContentDescription(""),
    iVersion(0), iUniqueBodyVersion(0), iContentType(0), iContentSubType(0),
    iPriority(0), iRetransmissionDistance(0), vecbProfileSubset(),
    ScopeStart(), ScopeEnd(), iScopeId(0), bReady(false)
    {
    }

    CMOTObject (const CMOTObject & nO):CMOTObjectBase (nO),
	bComplete (nO.bComplete),
	bHasHeader (nO.bHasHeader),
	strName (nO.strName),
	iBodySize(nO.iBodySize),
	iCharacterSetForName (nO.iCharacterSetForName),
	iCharacterSetForDescription (nO.iCharacterSetForDescription),
	strFormat (nO.strFormat),
	strMimeType (nO.strMimeType),
	iCompressionType (nO.iCompressionType),
	strContentDescription (nO.strContentDescription),
	iVersion (nO.iVersion),
	iUniqueBodyVersion (nO.iUniqueBodyVersion),
	iContentType (nO.iContentType),
	iContentSubType (nO.iContentSubType),
	iPriority (nO.iPriority),
	iRetransmissionDistance (nO.iRetransmissionDistance),
	vecbProfileSubset (nO.vecbProfileSubset),
	ScopeStart (nO.ScopeStart),
	ScopeEnd (nO.ScopeEnd), iScopeId (nO.iScopeId)
    {
        Body = nO.Body;
        vecbRawData.Init (nO.vecbRawData.Size ());
        vecbRawData = nO.vecbRawData;
    }

    virtual ~ CMOTObject ()
    {
    }

    inline CMOTObject & operator= (const CMOTObject & nO)
    {
        TransportID = nO.TransportID;
        ExpireTime = nO.ExpireTime;
        bPermitOutdatedVersions = nO.bPermitOutdatedVersions;
        bComplete = nO.bComplete;
        bHasHeader = nO.bHasHeader;
        strFormat = nO.strFormat;
        strName = nO.strName;
        iBodySize = nO.iBodySize;
        iCharacterSetForName = nO.iCharacterSetForName;
        iCharacterSetForDescription = nO.iCharacterSetForDescription;
        strMimeType = nO.strMimeType;
        iCompressionType = nO.iCompressionType;
        strContentDescription = nO.strContentDescription;
        iVersion = nO.iVersion;
        iUniqueBodyVersion = nO.iUniqueBodyVersion;
        iContentType = nO.iContentType;
        iContentSubType = nO.iContentSubType;
        iPriority = nO.iPriority;
        iRetransmissionDistance = nO.iRetransmissionDistance;
        vecbProfileSubset = nO.vecbProfileSubset;
        ScopeStart = nO.ScopeStart;
        ScopeEnd = nO.ScopeEnd;
        iScopeId = nO.iScopeId;
        Body = nO.Body;

        vecbRawData.Init (nO.vecbRawData.Size ());
        vecbRawData = nO.vecbRawData;

        return *this;
    }


    void Reset ()
    {
        vecbRawData.Init (0);
        bComplete = false;
        bHasHeader = false;
        Body.Reset ();
        strFormat = "";
        strName = "";
        iBodySize = 0;
        iCharacterSetForName = 0;
        iCharacterSetForDescription = 0;
        strMimeType = "";
        iCompressionType = 0;
        strContentDescription = "";
        iVersion = 0;
        iUniqueBodyVersion = 0;
        iContentType = 0;
        iContentSubType = 0;
        iPriority = 0;
        iRetransmissionDistance = 0;
        vecbProfileSubset.clear ();
        ScopeStart.Reset ();
        ScopeEnd.Reset ();
        iScopeId = 0;
    }

    void dump(std::ostream&);

    void AddHeader (CVector < _BINARY > &header);

    /* for encoding */
    CVector < _BYTE > vecbRawData;

    bool bComplete, bHasHeader;
    CByteReassembler Body;
    std::string strName;
    int iBodySize;
    int iCharacterSetForName;
    int iCharacterSetForDescription;
    std::string strFormat;
    std::string strMimeType;
    int iCompressionType;
    std::string strContentDescription;
    int iVersion, iUniqueBodyVersion;
    int iContentType, iContentSubType;
    int iPriority;
    int iRetransmissionDistance;
    std::vector < _BYTE > vecbProfileSubset;

    /* the following for EPG Objects */
    CDateAndTime ScopeStart, ScopeEnd;
    int iScopeId;

  protected:
    bool bReady;
};


/* Encoder ------------------------------------------------------------------ */
class CMOTDABEnc
{
  public:
    CMOTDABEnc ():MOTObject(), MOTObjSegments(),
    iSegmCntHeader(0), iSegmCntBody(0), bCurSegHeader(false),
    iContIndexHeader(0), iContIndexBody(0), iTransportID(0)
    {
    }

    virtual ~CMOTDABEnc ()
    {
    }

    void Reset ();
    bool GetDataGroup (CVector < _BINARY > &vecbiNewData);
    void SetMOTObject (CMOTObject & NewMOTObject);
    _REAL GetProgPerc () const;

  protected:
    class CMOTObjSegm
    {
      public:
	CVector < CVector < _BINARY > >vvbiHeader;
	CVector < CVector < _BINARY > >vvbiBody;
    };

    void GenMOTSegments (CMOTObjSegm & MOTObjSegm);
    void PartitionUnits (CVector < _BINARY > &vecbiSource,
			 CVector < CVector < _BINARY > >&vecbiDest,
			 const int iPartiSize);

    void GenMOTObj (CVector < _BINARY > &vecbiData,
		    CVector < _BINARY > &vecbiSeg, const bool bHeader,
		    const int iSegNum, const int iTranspID,
		    const bool bLastSeg);

    CMOTObject MOTObject;
    CMOTObjSegm MOTObjSegments;

    int iSegmCntHeader;
    int iSegmCntBody;
    bool bCurSegHeader;

    int iContIndexHeader;
    int iContIndexBody;

    int iTransportID;
};

/* Decoder ------------------------------------------------------------------ */

class CMOTDABDec
{
  public:

    CMOTDABDec ();

    virtual ~ CMOTDABDec ()
    {
    }

    /* client methods */
    void GetNextObject (CMOTObject & NewMOTObject);

    void GetObject (CMOTObject & NewMOTObject, TTransportID TransportID)
    {
        NewMOTObject = MOTCarousel[TransportID];
    }

    void GetDirectory (CMOTDirectory & MOTDirectoryOut)
    {
        MOTDirectoryOut = MOTDirectory;
    }

    bool NewObjectAvailable ();

    /* push from lower level */
    void AddDataUnit (CVector < _BINARY > &vecbiNewData);

  protected:

    void ProcessDirectory (CBitReassembler &MOTDir);

    void DeliverIfReady (TTransportID TransportID);

    /* These fields are the in-progress carousel objects */
    enum
    { unknown, headerMode, directoryMode } MOTmode;
    /* strictly, there should be only one of these! */
    std::map < TTransportID, CBitReassembler > MOTHeaders;
    CBitReassembler MOTDirectoryEntity;
    CBitReassembler MOTDirComprEntity;

    /* These fields are the cached complete carousel */
    CMOTDirectory MOTDirectory;
    std::map < TTransportID, CMOTObject > MOTCarousel;
    std::queue < TTransportID > qiNewObjects;
};

#endif // !defined(DABMOT_H__3B0UBVE98732KJVEW363E7A0D31912__INCLUDED_)


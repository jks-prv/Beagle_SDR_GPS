/******************************************************************************\
 * Technische Universitaet Darmstadt, Institut fuer Nachrichtentechnik
 * Copyright (c) 2001-2005
 *
 * Author(s):
 *	Volker Fischer, Andrew Murphy, David Flamand
 *
 * Description:
 *	See SDC.cpp
 *
 * 11/30/2012 David Flamand, added transmit data entity type 8
 *
 * 11/21/2005 Andrew Murphy, BBC Research & Development, 2005
 *	- AMSS data entity groups (no AFS index), added eSDCType, data type 11
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

#ifndef SDC_H
#define SDC_H

#include "../GlobalDefinitions.h"
#include "../Parameter.h"
#include "../util/CRC.h"
#include "../util/Vector.h"
#include "../util/Utilities.h"

/* Definitions ****************************************************************/
/* Number of bits of header of SDC block */
#define NUM_BITS_HEADER_SDC			12


/* Classes ********************************************************************/
class CSDCTransmit
{
public:
    CSDCTransmit() {}
    virtual ~CSDCTransmit();

    void SDCParam(CVector<_BINARY>* pbiData, CParameter& Parameter);

protected:
    void CommitEnter(CVector<_BINARY>* pbiData, CParameter& Parameter);
    void CommitFlush();
    void CommitLeave();

    bool CanTransmitCurrentTime(CParameter& Parameter);

    void DataEntityType0(CVector<_BINARY>& vecbiData, CParameter& Parameter);
    void DataEntityType1(CVector<_BINARY>& vecbiData, int ServiceID,
                         CParameter& Parameter);
// ...
    void DataEntityType5(CVector<_BINARY>& vecbiData, int ServiceID,
                         CParameter& Parameter);
// ...
    void DataEntityType8(CVector<_BINARY>& vecbiData, int ServiceID,
                         CParameter& Parameter);
    void DataEntityType9(CVector<_BINARY>& vecbiData, int ServiceID,
                         CParameter& Parameter);

    CCRC CRCObject;
    CVector<_BINARY>* pbiData;
    CVector<_BINARY> vecbiData;
    int iNumUsedBits;
    int iMaxNumBitsDataBlocks;
    int iLengthDataFieldBytes;
    int iUsefulBitsSDC;
    int iLastMinuteTransmitted;
};

class CSDCReceive
{
public:
    enum ERetStatus {SR_OK, SR_BAD_CRC, SR_BAD_DATA};
    enum ESDCType {SDC_DRM, SDC_AMSS};
    CSDCReceive() : eSDCType(SDC_DRM) {}
    virtual ~CSDCReceive();

    ERetStatus SDCParam(CVector<_BINARY>* pbiData, CParameter& Parameter);
    void SetSDCType(ESDCType sdcType) {
        eSDCType = sdcType;
    }

protected:
    bool DataEntityType0(CVector<_BINARY>* pbiData, const int iLengthOfBody,
                             CParameter& Parameter, const bool bVersion);
    bool DataEntityType1(CVector<_BINARY>* pbiData, const int iLengthOfBody,
                             CParameter& Parameter);
// ...
    bool DataEntityType3(CVector<_BINARY>* pbiData, const int iLengthOfBody,
                             CParameter& Parameter, const bool bVersion);
    bool DataEntityType4(CVector<_BINARY>* pbiData, const int iLengthOfBody,
                             CParameter& Parameter, const bool bVersion);
    bool DataEntityType5(CVector<_BINARY>* pbiData, const int iLengthOfBody,
                             CParameter& Parameter, const bool bVersion);
// ...
    bool DataEntityType7(CVector<_BINARY>* pbiData, const int iLengthOfBody,
                             CParameter& Parameter, const bool bVersion);
    bool DataEntityType8(CVector<_BINARY>* pbiData, const int iLengthOfBody,
                             CParameter& Parameter);
    bool DataEntityType9(CVector<_BINARY>* pbiData, const int iLengthOfBody,
                             CParameter& Parameter, const bool bVersion);
// ...
    bool DataEntityType11(CVector<_BINARY>* pbiData, const int iLengthOfBody,
                              CParameter& Parameter, const bool bVersion);
    bool DataEntityType12(CVector<_BINARY>* pbiData, const int iLengthOfBody,
                              CParameter& Parameter);

    CCRC		CRCObject;
    ESDCType	eSDCType;
};


#endif // SDC_H

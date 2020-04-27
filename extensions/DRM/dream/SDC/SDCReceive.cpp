/******************************************************************************\
 * Technische Universitaet Darmstadt, Institut fuer Nachrichtentechnik
 * Copyright (c) 2001-2005
 *
 * Author(s):
 *	Volker Fischer, Andrew Murphy
 *
 * Description:
 *	SDC data stream decoding (receiver)
 *
 *
 * 11/21/2005 Andrew Murphy, BBC Research & Development, 2005
 *	- AMSS data entity groups (no AFS index), added eSDCType, data type 11
 *
 * 11/28/2005 Andrea Russo
 *	- Added code for store alternative frequencies informations about Regions
 *      and Schedules.
 *
 ******************************************************************************
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

#include "SDC.h"

CSDCReceive::~CSDCReceive(){}

/* Implementation *************************************************************/
CSDCReceive::ERetStatus CSDCReceive::SDCParam(CVector<_BINARY>* pbiData,
        CParameter& Parameter)
{
    /* Calculate length of data field in bytes
       (consistant to table 61 in (6.4.1)) */
    int iLengthDataFieldBytes;
    int iUsefulBitsSDC;
    int iNumBytesForCRCCheck;
    int iBitsConsumed;

    Parameter.Lock();
    if (eSDCType == SDC_DRM)
    {
        iLengthDataFieldBytes =
            (int) ((_REAL) (Parameter.iNumSDCBitsPerSFrame - 20) / 8);

        /* 20 bits from AFS index and CRC */
        iUsefulBitsSDC= 20 + iLengthDataFieldBytes * 8;
    }
    else	// SDC_AMSS
    {
        iLengthDataFieldBytes =
            (int) ((_REAL) (Parameter.iNumSDCBitsPerSFrame - 16) / 8);

        /* 16 bits from CRC */
        iUsefulBitsSDC = 16 + iLengthDataFieldBytes * 8;
    }
    Parameter.Unlock();

    /* CRC ------------------------------------------------------------------ */
    /* Check the CRC of this data block */
    CRCObject.Reset(16);

    (*pbiData).ResetBitAccess();

    /* Special treatment of SDC data stream: The CRC (Cyclic Redundancy
    Check) field shall contain a 16-bit CRC calculated over the AFS
    index coded in an 8-bit field (4 msbs are 0) and the data field.
    4 MSBs from AFS-index. Insert four "0" in the data-stream */
    if (eSDCType == SDC_DRM) /* Skip for AMSS */
    {
        const _BYTE byFirstByte = (_BYTE) (*pbiData).Separate(4);
        CRCObject.AddByte(byFirstByte);
    }

    if (eSDCType == SDC_DRM)
    {
        /* "- 4": Four bits already used, "/ SIZEOF__BYTE": We add bytes, not
           bits, "- 2": 16 bits for CRC at the end */
        iNumBytesForCRCCheck = (iUsefulBitsSDC - 4) / SIZEOF__BYTE - 2;
    }
    else
    {
        /* Consider 2 bytes for CRC ("- 2") */
        iNumBytesForCRCCheck = iUsefulBitsSDC / SIZEOF__BYTE - 2;
    }

    for (int i = 0; i < iNumBytesForCRCCheck; i++)
        CRCObject.AddByte((_BYTE) (*pbiData).Separate(SIZEOF__BYTE));

    bool permissive = Parameter.lenient_RSCI;

    if (permissive || CRCObject.CheckCRC((*pbiData).Separate(16)))
    {
        /* CRC-check successful, extract data from SDC-stream --------------- */
        int			iLengthOfBody;
        bool	bError = false;

        /* Reset separation function */
        (*pbiData).ResetBitAccess();

        /* AFS index */
        /* Reconfiguration index (not used by this application) */
        if (eSDCType == SDC_DRM) /* Skip for AMSS */
            (*pbiData).Separate(4);

        /* Init bit count and total number of bits for body */
        if (eSDCType == SDC_DRM)
            iBitsConsumed = 4; /* 4 bits for AFS index */
        else
            iBitsConsumed = 0; /* 0 for AMSS, no AFS index */

        const int iTotNumBitsWithoutCRC = iUsefulBitsSDC - 16;

        /* Length of the body, excluding the initial 4 bits ("- 4"),
           measured in bytes ("/ 8").
           With this condition also the error code of the "Separate" function
           is checked! (implicitly)
           Check for: -end tag, -error, -no more data available */
        while (((iLengthOfBody = (*pbiData).Separate(7)) != 0) &&
                (bError == false) && (iBitsConsumed < iTotNumBitsWithoutCRC))
        {
            /* Version flag */
            bool bVersionFlag;
            if ((*pbiData).Separate(1) == 0)
                bVersionFlag = false;
            else
                bVersionFlag = true;

            /* Data entity type */
            /* First calculate number of bits for this entity ("+ 4" because of:
               "The body of the data entities shall be at least 4 bits long. The
               length of the body, excluding the initial 4 bits, shall be
               signalled by the header") */
            const int iNumBitsEntity = iLengthOfBody * 8 + 4;

            /* Call the routine for the signalled type */
            int t = (*pbiData).Separate(4);
            switch (t)
            {
            case 0: /* Type 0 */
                bError = DataEntityType0(pbiData, iLengthOfBody, Parameter, bVersionFlag);
                break;

            case 1: /* Type 1 */
                bError = DataEntityType1(pbiData, iLengthOfBody, Parameter);
                break;

            case 3: /* Type 3 */
                bError = DataEntityType3(pbiData, iLengthOfBody, Parameter, bVersionFlag);
                break;

            case 4: /* Type 4 */
                bError = DataEntityType4(pbiData, iLengthOfBody, Parameter, bVersionFlag);
                break;

            case 5: /* Type 5 */
                bError = DataEntityType5(pbiData, iLengthOfBody, Parameter, bVersionFlag);
                break;

            case 7: /* Type 7 */
                bError = DataEntityType7(pbiData, iLengthOfBody, Parameter, bVersionFlag);
                break;

            case 8: /* Type 8 */
                bError = DataEntityType8(pbiData, iLengthOfBody, Parameter);
                break;

            case 9: /* Type 9 */
                bError = DataEntityType9(pbiData, iLengthOfBody, Parameter, bVersionFlag);
                break;

            case 11: /* Type 11 */
                bError = DataEntityType11(pbiData, iLengthOfBody, Parameter, bVersionFlag);
                break;

            case 12: /* Type 12 */
                bError = DataEntityType12(pbiData, iLengthOfBody, Parameter);
                break;

            default:
                /* This type is not supported, delete all bits of this entity
                   from the queue */
                (*pbiData).Separate(iNumBitsEntity);
            }
            /* Count number of bits consumed (7 for length, 1 for version flag,
               4 for type = 12 plus actual entitiy body data) */
            iBitsConsumed += 12 + iNumBitsEntity;
        }

        /* If error was detected, return proper error code */
        if (bError)
            return SR_BAD_DATA;
        else
            return SR_OK; /* everything was ok */
    }
    else
    {
        /* Data is corrupted, do not use it. Return error code */
        return SR_BAD_CRC;
    }
}


/******************************************************************************\
* Data entity Type 0 (Multiplex description data entity)					   *
\******************************************************************************/
bool CSDCReceive::DataEntityType0(CVector<_BINARY>* pbiData,
                                      const int iLengthOfBody,
                                      CParameter& Parameter,
                                      const bool)
{
    CMSCProtLev				MSCPrLe;
    int						iLenPartA;
    int						iLenPartB;

    /* The receiver may determine the number of streams present in the multiplex
       by dividing the length field of the header by three (6.4.3.1) */
    const int iNumStreams = iLengthOfBody / 3;

    /* Check number of streams for overflow */
    if (iNumStreams > MAX_NUM_STREAMS)
        return true;

    /* Get protection levels */
    /* Protection level for part A */
    MSCPrLe.iPartA = (*pbiData).Separate(2);

    /* Protection level for part B */
    MSCPrLe.iPartB = (*pbiData).Separate(2);

    /* Reset hierarchical flag (hierarchical present or not) */
    bool bWithHierarch = false;

    Parameter.Lock();
    /* Get stream parameters */
    for (int i = 0; i < iNumStreams; i++)
    {
        /* In case of hirachical modulation stream 0 describes the protection
           level and length of hierarchical data */
        if ((i == 0) &&
                ((Parameter.eMSCCodingScheme == CS_3_HMSYM) ||
                 (Parameter.eMSCCodingScheme == CS_3_HMMIX)))
        {
            /* Protection level for hierarchical */
            MSCPrLe.iHierarch = (*pbiData).Separate(2);
            bWithHierarch = true;

            /* rfu: these 10 bits shall be reserved for future use by the stream
               description field and shall be set to zero until they are
               defined */
            if ((*pbiData).Separate(10) != 0)
                return true;

            /* Data length for hierarchical */
            iLenPartB = (*pbiData).Separate(12);

            /* Set new parameters in global struct. Length of part A is zero
               with hierarchical modulation */
            Parameter.SetStreamLen(i, 0, iLenPartB);
        }
        else
        {
            /* Data length for part A */
            iLenPartA = (*pbiData).Separate(12);

            /* Data length for part B */
            iLenPartB = (*pbiData).Separate(12);

            /* Set new parameters in global struct */
            Parameter.SetStreamLen(i, iLenPartA, iLenPartB);
        }
    }

    /* Set new parameters in global struct */
    Parameter.SetMSCProtLev(MSCPrLe, bWithHierarch);
    Parameter.Unlock();

    return false;
}


/******************************************************************************\
* Data entity Type 1 (Label data entity)									   *
* Uses the unique flavour of the version flag so it is not needed here		   *
\******************************************************************************/
bool CSDCReceive::DataEntityType1(CVector<_BINARY>* pbiData,
                                      const int iLengthOfBody,
                                      CParameter& Parameter)
{
    /* Short ID (the short ID is the index of the service-array) */
    const int iTempShortID = (*pbiData).Separate(2);

    /* rfu: these 2 bits are reserved for future use and shall be set to zero
       until they are defined */
    if ((*pbiData).Separate(2) != 0)
        return true;


    /* Get label string ----------------------------------------------------- */
    string strLabel(iLengthOfBody, 0);
    /* Check the following restriction to the length of label: "label: this is a
       variable length field of up to 64 bytes defining the label" */
    if (iLengthOfBody <= 64)
    {

        /* Get all characters from SDC-stream */
        for (int i = 0; i < iLengthOfBody; i++)
        {
            /* Get character */
            strLabel[i] = char((*pbiData).Separate(8));
        }

        /* store label string in the current service structure */
        Parameter.Lock();
        Parameter.Service[iTempShortID].strLabel = strLabel;
        /* and keep it in the persistent service information store.
         * But only if the FAC has already seen the sid. */
        uint32_t sid = Parameter.Service[iTempShortID].iServiceID;
        if (sid != SERV_ID_NOT_USED)
        {
            (void)Parameter.ServiceInformation[sid].label.insert(strLabel);
            Parameter.ServiceInformation[sid].id = sid;
        }
        Parameter.Unlock();

        return false;
    }
    else
        return true; /* error */
}


/******************************************************************************\
* Data entity Type 3 (Alternative frequency signalling)                        *
\******************************************************************************/
bool CSDCReceive::DataEntityType3(CVector<_BINARY>* pbiData,
                                      const int iLengthOfBody,
                                      CParameter& Parameter,
                                      const bool bVersion)
{
    int			i;
    bool	bEnhanceFlag = false;
    bool	bServRestrFlag = false;
    int			iServRestr = 0x0f;
    CMultiplexDefinition AltFreq;

    /* Init number of frequency count */
    int iNumFreqTmp = iLengthOfBody;

    /* Init region ID and schedule ID with "not used" parameters */
    AltFreq.iRegionID = 0;
    AltFreq.iScheduleID = 0;

    /* Synchronous Multiplex flag: this flag indicates whether the multiplex is
       broadcast synchronously */
    switch ((*pbiData).Separate(1))
    {
    case 0: /* 0 */
        /* Multiplex is not synchronous (different content and/or channel
           parameters and/or multiplex parameters and/or signal timing in target
           area) */
        AltFreq.bIsSyncMultplx = false;
        break;

    case 1: /* 1 */
        /* Multiplex is synchronous (identical content and channel parameters
           and multiplex parameters and signal timing in target area) */
        AltFreq.bIsSyncMultplx = true;
        break;
    }

    /* Layer flag: this flag indicates whether the frequencies given apply to
       the base layer of the DRM multiplex or to the enhancement layer */
    switch ((*pbiData).Separate(1))
    {
    case 0: /* 0 */
        /* Base layer */
        bEnhanceFlag = false;
        break;

    case 1: /* 1 */
        /* Enhancement layer */
        bEnhanceFlag = true;
        break;
    }

    /* Service Restriction flag: this flag indicates whether all or just some of
       the services of the tuned multiplex are available in the DRM multiplex on
       the frequencies */
    switch ((*pbiData).Separate(1))
    {
    case 0: /* 0 */
        /* All services in the tuned multiplex are available on the frequencies
           given */
        bServRestrFlag = false;
        break;

    case 1: /* 1 */
        /* A restricted set of services are available on the frequencies
           given */
        bServRestrFlag = true;
        break;
    }

    /* Region/Schedule flag: this field indicates whether the list of
       frequencies is restricted by region and/or schedule or not */
    bool bRegionSchedFlag = false;
    switch ((*pbiData).Separate(1))
    {
    case 0: /* 0 */
        /* No restriction */
        bRegionSchedFlag = false;
        break;

    case 1: /* 1 */
        /* Region and/or schedule applies to this list of frequencies */
        bRegionSchedFlag = true;
        break;
    }

    /* Service Restriction field: this 8 bit field is only present if the
       Service Restriction flag is set to 1 */
    if (bServRestrFlag)
    {
        /* Short Id flags 4 bits. This field indicates, which services
           (identified by their Short Id) of the tuned DRM multiplex are carried
           in the DRM multiplex on the alternative frequencies by setting the
           corresponding bit to 1 */
        iServRestr = (*pbiData).Separate(4);

        /* rfa 4 bits. This field (if present) is reserved for future additions
           and shall be set to zero until it is defined */
        if ((*pbiData).Separate(4) != 0)
            return true;

        /* Remove one byte from frequency count */
        iNumFreqTmp--;
    }
    else
    {
        iServRestr = 0x0f; /* all services are included */
    }

    /* Region/Schedule field: this 8 bit field is only present if the
       Region/Schedule flag is set to 1 */
    if (bRegionSchedFlag)
    {
        /* Region Id 4 bits. This field indicates whether the region is
           unspecified (value 0) or whether the alternative frequencies are
           valid just in certain geographic areas, in which case it carries
           the Region Id (value 1 to 15). The region may be described by one or
           more "Alternative frequency signalling: Region definition data entity
           - type 7" with this Region Id */
        AltFreq.iRegionID = (*pbiData).Separate(4);

        /* Schedule Id 4 bits. This field indicates whether the schedule is
           unspecified (value 0) or whether the alternative frequencies are
           valid just at certain times, in which case it carries the Schedule Id
           (value 1 to 15). The schedule is described by one or more
           "Alternative frequency signalling: Schedule definition data entity
           - type 4" with this Schedule Id */
        AltFreq.iScheduleID = (*pbiData).Separate(4);

        /* Remove one byte from frequency count */
        iNumFreqTmp--;
    }

    /* Check for error (length of body must be so long to include Service
       Restriction field and Region/Schedule field, also check that
       remaining number of bytes is devidable by 2 since we read 16 bits) */
    if ((iNumFreqTmp < 0) || (iNumFreqTmp % 2 != 0))
        return true;

    /* n frequencies: this field carries n 16 bit fields. n is in the
       range 0 to 16. The number of frequencies, n, is determined from the
       length field of the header and the value of the Service Restriction flag
       and the Region/Schedule flag */
    const int iNumFreq = iNumFreqTmp / 2; /* 16 bits are read */

    AltFreq.veciFrequencies.resize(iNumFreq);

    for (i = 0; i < iNumFreq; i++)
    {
        /* rfu 1 bit. This field is reserved for future use of the frequency
           value field and shall be set to zero until defined */
        if ((*pbiData).Separate(1) != 0)
            return true;

        /* Frequency value 15 bits. This field is coded as an unsigned integer
           and gives the frequency in kHz */
        AltFreq.veciFrequencies[i] = (*pbiData).Separate(15);
    }

    /* Now, set data in global struct */
    Parameter.Lock();

    /* Check the version flag */
    if (bVersion != Parameter.AltFreqSign.bMultiplexVersionFlag)
    {
        /* If version flag has changed, delete all data for this entity type and save flag */
        Parameter.AltFreqSign.ResetMultiplexes(bVersion);
    }
    /* Enhancement layer is not supported */
    if (bEnhanceFlag == false)
    {

        /* Set some parameters */

        /* Set Service Restriction
         * The first bit (msb) refers to Short Id 3,
         * while the last bit (lsb) refers to Short Id 0 of the tuned DRM multiplex
         * We made iServRestr valid whether the flag was there or not.
         */
        for (i = 0; i < MAX_NUM_SERVICES; i++)
        {
            /* Mask last bit (lsb) */
            AltFreq.veciServRestrict[i] = iServRestr & 1;

            /* Shift by one bit to get information for next service */
            iServRestr >>= 1;
        }

        /* Now apply temporary object to global struct (first check if new object is not already there) */
        int iCurNumAltFreq = Parameter.AltFreqSign.vecMultiplexes.size();

        bool bAltFreqIsAlreadyThere = false;
        for (i = 0; i < iCurNumAltFreq; i++)
        {
            if (Parameter.AltFreqSign.vecMultiplexes[i] == AltFreq)
                bAltFreqIsAlreadyThere = true;
        }

        if (bAltFreqIsAlreadyThere == false)
            Parameter.AltFreqSign.vecMultiplexes.push_back(AltFreq);
    }
    Parameter.Unlock();

    return false;
}


/******************************************************************************\
* Data entity Type 4 (Alternative frequency signalling: Schedule definition)   *
\******************************************************************************/
bool CSDCReceive::DataEntityType4(CVector<_BINARY>* pbiData,
                                      const int iLengthOfBody,
                                      CParameter& Parameter,
                                      const bool bVersion)
{
    /* Check length -> must be 4 bytes */
    if (iLengthOfBody != 4)
        return true;

    int iScheduleID = 0;
    CAltFreqSched Sched;

    /* Schedule Id: this field indicates the Schedule Id for the defined
       schedule. Up to 15 different schedules with an individual Schedule Id
       (values 1 to 15) can be defined; the value 0 shall not be used, since it
       indicates "unspecified schedule" in data entity type 3 and 11 */
    iScheduleID = (*pbiData).Separate(4);

    /* Day Code: this field indicates which days the frequency schedule (the
       following Start Time and Duration) applies to. The msb indicates Monday,
       the lsb Sunday. Between one and seven bits may be set to 1 */
    Sched.iDayCode = (*pbiData).Separate(7);

    /* Start Time: this field indicates the time from when the frequency is
       valid. The time is expressed in minutes since midnight UTC. Valid values
       range from 0 to 1439 (representing 00:00 to 23:59) */
    Sched.iStartTime = (*pbiData).Separate(11);

    /* Duration: this field indicates how long the frequency is valid starting
       from the indicated Start Time. The time is expressed in minutes. Valid
       values range from 1 to 16383 */
    Sched.iDuration = (*pbiData).Separate(14);

    /* Error checking */
    if ((iScheduleID == 0) || (Sched.iDayCode == 0) || (Sched.iDayCode > 127) ||
            (Sched.iStartTime > 1439) || (Sched.iDuration > 16383) || (Sched.iDuration == 0))
    {
        return true;
    }


    /* Now apply temporary object to global struct */
    Parameter.Lock();
    /* Check the version flag */
    if (bVersion != Parameter.AltFreqSign.bScheduleVersionFlag)
    {
        /* If version flag has changed, delete all data for this entity type and save flag */
        Parameter.AltFreqSign.ResetSchedules(bVersion);
    }

    vector<CAltFreqSched>& vecSchedules =
        Parameter.AltFreqSign.vecSchedules[iScheduleID];

    /*(first check if new object is not already there) */

    bool bAltFreqSchedIsAlreadyThere = false;
    for (size_t i = 0; i < vecSchedules.size(); i++)
    {
        if (vecSchedules[i] == Sched)
            bAltFreqSchedIsAlreadyThere = true;
    }

    if (bAltFreqSchedIsAlreadyThere == false)
        vecSchedules.push_back(Sched);

    Parameter.Unlock();

    return false;
}

/******************************************************************************\
* Data entity Type 5 (Application information data entity)					   *
\******************************************************************************/
bool CSDCReceive::DataEntityType5(CVector<_BINARY>* pbiData,
                                      const int iLengthOfBody,
                                      CParameter& Parameter,
                                      const bool)
{
    /* Short ID (the short ID is the index of the service-array) */
    const int iTempShortID = (*pbiData).Separate(2);

    /* Load data parameters class with current parameters */
    Parameter.Lock();
    CDataParam DataParam = Parameter.GetDataParam(iTempShortID);
    Parameter.Unlock();

    /* Stream Id */
    DataParam.iStreamID = (*pbiData).Separate(2);

    /* Packet mode indicator */
    switch ((*pbiData).Separate(1))
    {
    case 0: /* 0 */
        DataParam.ePacketModInd = CDataParam::PM_SYNCHRON_STR_MODE;

        /* Descriptor (not used) */
        (*pbiData).Separate(7);
        break;

    case 1: /* 1 */
        DataParam.ePacketModInd = CDataParam::PM_PACKET_MODE;

        /* Descriptor */
        /* Data unit indicator */
        switch ((*pbiData).Separate(1))
        {
        case 0: /* 0 */
            DataParam.eDataUnitInd = CDataParam::DU_SINGLE_PACKETS;
            break;

        case 1: /* 1 */
            DataParam.eDataUnitInd = CDataParam::DU_DATA_UNITS;
            break;
        }

        /* Packet Id */
        DataParam.iPacketID = (*pbiData).Separate(2);

        /* Application domain */
        switch ((*pbiData).Separate(4))
        {
        case 0: /* 0000 */
            DataParam.eAppDomain = CDataParam::AD_DRM_SPEC_APP;
            break;

        case 1: /* 0001 */
            DataParam.eAppDomain = CDataParam::AD_DAB_SPEC_APP;
            break;

        default: /* 2 - 15 reserved */
            DataParam.eAppDomain = CDataParam::AD_OTHER_SPEC_APP;
            break;
        }

        /* Packet length */
        DataParam.iPacketLen = (*pbiData).Separate(8);
        break;
    }

    /* Application data */
    if (DataParam.ePacketModInd == CDataParam::PM_SYNCHRON_STR_MODE)
    {
        /* Not used */
        (*pbiData).Separate(iLengthOfBody * 8 - 8);
    }
    else if (DataParam.eAppDomain == CDataParam::AD_DAB_SPEC_APP)
    {
        /* rfu */
        (*pbiData).Separate(5);

        /* User application identifier */
        DataParam.iUserAppIdent = (*pbiData).Separate(11);

        /* Data fields as required by DAB application specification, not used */
        (*pbiData).Separate(iLengthOfBody * 8 - 32);
    }
    else
    {
        /* AppIdent */
        DataParam.iUserAppIdent = (*pbiData).Separate(iLengthOfBody * 8 - 16);
    }

    /* Set new parameters in global struct */
    Parameter.Lock();
    Parameter.SetDataParam(iTempShortID, DataParam);
    Parameter.Unlock();

    return false;
}

/******************************************************************************\
* Data entity Type 7 (Alternative frequency signalling: Region definition)     *
\******************************************************************************/
bool CSDCReceive::DataEntityType7(CVector<_BINARY>* pbiData,
                                      const int iLengthOfBody,
                                      CParameter& Parameter,
                                      const bool bVersion)
{
    size_t i;
    CAltFreqRegion Region;

    /* Region Id: this field indicates the identifier for this region
       definition. Up to 15 different geographic regions with an individual
       Region Id (values 1 to 15) can be defined; the value 0 shall not be used,
       since it indicates "unspecified geographic area" in data entity
       type 3 and 11 */
    const int iRegionID = (*pbiData).Separate(4);

    /* Latitude: this field specifies the southerly point of the area in
       degrees, as a 2's complement number between -90 (south pole) and
       +90 (north pole) */


    Region.iLatitude = Complement2toInt(8, pbiData);

    /* Longitude: this field specifies the westerly point of the area in
       degrees, as a 2's complement number between -180 (west) and
       +179 (east) */

    Region.iLongitude = Complement2toInt(9, pbiData);

    /* Latitude Extent: this field specifies the size of the area to the north,
       in 1 degree steps; the value of Latitude plus the value of Latitude
       Extent shall be equal or less than 90 */
    Region.iLatitudeEx = (*pbiData).Separate(7);

    /* Longitude Extent: this field specifies the size of the area to the east,
       in 1 degree steps; the value of Longitude plus the value of Longitude
       Extent may exceed the value +179 (i.e. wrap into the region of negative
       longitude values) */
    Region.iLongitudeEx = (*pbiData).Separate(8);

    /* n CIRAF Zones: this field carries n CIRAF zones (n in the range 0 to 16).
       The number of CIRAF zones, n, is determined from the length field of the
       header - 4 */
    for (i = 0; i < size_t(iLengthOfBody - 4); i++)
    {
        /* Each CIRAF zone is coded as an 8 bit unsigned binary number in the
           range 1 to 85 */
        const int iCIRAFZone = (*pbiData).Separate(8);

        if ((iCIRAFZone == 0) || (iCIRAFZone > 85))
            return true; /* Error */
        else
            Region.veciCIRAFZones.push_back(iCIRAFZone);

        /*
        	TODO: To check whether a certain longitude value is inside the specified
        	longitude range, the following formula in pseudo program code shall be used
        	(with my_longitude in the range -180 to +179):
        	inside_area = ( (my_longitude >= longitude) AND
        		(my_longitude <= (longitude + longitude_extent) ) OR
        		( ((longitude + longitude_extent) >= +180) AND
        		(my_longitude <= (longitude + longitude_extent - 360)) )
        */
    }

    /* Error checking */
    if ((iRegionID == 0)
            || (Region.iLatitude + Region.iLatitudeEx > 90)
            || (Region.iLongitude < -180) || (Region.iLongitude > 179)
            || (Region.iLatitude < -90) || (Region.iLatitude > 90))
    {
        return true; /* Error */
    }


    /* Now apply temporary object to global struct */
    Parameter.Lock();
    /* Check the version flag */
    if (bVersion != Parameter.AltFreqSign.bRegionVersionFlag)
    {
        /* If version flag has changed, delete all data for this entity type and save flag */
        Parameter.AltFreqSign.ResetRegions(bVersion);
    }

    vector<CAltFreqRegion>& vecRegions = Parameter.AltFreqSign.vecRegions[iRegionID];

    /*(first check if new object is not already there) */

    bool bAltFreqRegionIsAlreadyThere = false;
    for (i = 0; i < vecRegions.size(); i++)
    {
        if (vecRegions[i] == Region)
            bAltFreqRegionIsAlreadyThere = true;
    }

    if (bAltFreqRegionIsAlreadyThere == false)
        vecRegions.push_back(Region);

    Parameter.Unlock();

    return false;
}


/******************************************************************************\
* Data entity Type 8 (Time and date information data entity)				   *
\******************************************************************************/
bool CSDCReceive::DataEntityType8(CVector<_BINARY>* pbiData,
                                      const int iLengthOfBody,
                                      CParameter& Parameter)
{
    /* Check length -> must be 3 or 4 bytes */
    if (iLengthOfBody < 3)
        return false;

    if (iLengthOfBody > 4)
        return false;

    if (iLengthOfBody == 3)
    {
        /* Decode date */
        CModJulDate ModJulDate((*pbiData).Separate(17));
    
        Parameter.Lock();
        Parameter.iDay = ModJulDate.GetDay();
        Parameter.iMonth = ModJulDate.GetMonth();
        Parameter.iYear = ModJulDate.GetYear();
    
        /* UTC (hours and minutes) */
        Parameter.iUTCHour = (*pbiData).Separate(5);
        Parameter.iUTCMin = (*pbiData).Separate(6);
    
        Parameter.bValidUTCOffsetAndSense = false;
    }

    if (iLengthOfBody == 4)
    {
        /* Decode date */
        CModJulDate ModJulDate((*pbiData).Separate(17));
    
        Parameter.Lock();
        Parameter.iDay = ModJulDate.GetDay();
        Parameter.iMonth = ModJulDate.GetMonth();
        Parameter.iYear = ModJulDate.GetYear();
    
        /* UTC (hours and minutes) */
        Parameter.iUTCHour = (*pbiData).Separate(5);
        Parameter.iUTCMin = (*pbiData).Separate(6);
    
        /* rfu */
        const int rfu = (*pbiData).Separate(2);
    
        /* UTC Sense */
        Parameter.iUTCSense = (*pbiData).Separate(1); 
    
        /* UTC Offset (local time offset) */
        Parameter.iUTCOff = ((*pbiData).Separate(5));
    
        Parameter.bValidUTCOffsetAndSense = true;
    
        if (rfu)
        {
            /* rfu is set, reset all */
            Parameter.iDay = 0;
            Parameter.iMonth = 0;
            Parameter.iYear = 0;
            Parameter.iUTCSense = 0; 
            Parameter.iUTCOff = 0;
            Parameter.bValidUTCOffsetAndSense = false;
        }
    }
	
    Parameter.Unlock();

    return false;
}


/******************************************************************************\
* Data entity Type 9 (Audio information data entity)						   *
\******************************************************************************/
/*
6.4.3.10 Audio information data entity - type 9
Each audio service needs a detailed description of the parameters needed for audio decoding. This data entity uses the reconfiguration mechanism for the version flag.
• Short Id                2 bits
• Stream Id               2 bits
• audio coding            2 bits
• SBR flag                1 bit
• audio mode              2 bits
• audio sampling rate     3 bits
• text flag               1 bit
• enhancement flag        1 bit
• coder field             5 bits
• rfa                     1 bit
• codec specific config   8n bits

 */
bool CSDCReceive::DataEntityType9(CVector<_BINARY>* pbiData,
                                      const int iLengthOfBody,
                                      CParameter& Parameter,
                                      const bool)
{

    /* Check length -> must be at least 2 bytes */
    if (iLengthOfBody < 2)
        return true;

    /* Short ID (the short ID is the index of the service-array) */
    const int iTempShortID = int((*pbiData).Separate(2));

    /* Load audio parameters class with current parameters */
    Parameter.Lock();
    /* Stream Id */
    CAudioParam AudParam = Parameter.GetAudioParam(iTempShortID);
    AudParam.iStreamID = int((*pbiData).Separate(2));
    bool bError = AudParam.setFromType9Bits(*pbiData, unsigned(iLengthOfBody));
    Parameter.Unlock();

    /* Set new parameters in global struct */
    if (bError == false)
    {
        Parameter.Lock();
        Parameter.SetAudioParam(iTempShortID, AudParam);
        Parameter.Unlock();
        return false;
    }
    else
        return true;
}

/******************************************************************************\
* Data entity Type 11 (Alternative frequency signalling - other services)      *
\******************************************************************************/
bool CSDCReceive::DataEntityType11(CVector<_BINARY>* pbiData,
                                       const int iLengthOfBody,
                                       CParameter& Parameter,
                                       const bool bVersion)
{
    size_t i;
    bool		bRegionSchedFlag = false;
    int				iFrequencyEntryLength;
    COtherService	OtherService;

    /* Init number of frequency count */
    int iNumFreqTmp = iLengthOfBody;

    /* Short ID/Announcement flag: specifies
     * whether this data entity is an AFS or an Announcement
     * type entity. We only support AFS
     */
    const int iShortIDAnnounceFlag = pbiData->Separate(1);

    /* Short Id / announcement field */
    OtherService.iShortID = (*pbiData).Separate(2);

    /* Region/Schedule flag: this field indicates whether the list of
       frequencies is restricted by region and/or schedule or not */
    switch ((*pbiData).Separate(1))
    {
    case 0: /* 0 */
        /* No restriction */
        bRegionSchedFlag = false;
        break;

    case 1: /* 1 */
        /* Region and/or schedule applies to this list of frequencies */
        bRegionSchedFlag = true;
        break;
    }

    /* Same service ID flag: this field indicates whether the specified
       other service carries the same audio programme or not */
    switch ((*pbiData).Separate(1))
    {
    case 0: /* 0 */
        /* No restriction */
        OtherService.bSameService = false;
        break;

    case 1: /* 1 */
        OtherService.bSameService = true;
        break;
    }

    /* RFA - 2 bits */
    (*pbiData).Separate(2);

    /* Other system ID */
    OtherService.iSystemID = (*pbiData).Separate(5);

    /* Remove one byte from frequency count */
    iNumFreqTmp--;

    /* Optional Region/schedule ID */
    if (bRegionSchedFlag)
    {
        OtherService.iRegionID = (*pbiData).Separate(4);
        OtherService.iScheduleID = (*pbiData).Separate(4);

        /* Remove one byte from frequency count */
        iNumFreqTmp--;
    }

    switch (OtherService.iSystemID)
    {
    case 0:
    case 1:
    case 3:
    case 6:
    case 9:
        /* DRM, FM-RDS (European & North American grid, PI+ECC),
           FM-RDS (Asia grid, PI+ECC), DAB (ECC + audio service ID) */
        OtherService.iServiceID = (*pbiData).Separate(24);

        /* Remove three bytes from frequency count */
        iNumFreqTmp -= 3;
        break;

    case 4:
    case 7:
    case 10:
        /* FM RDS (European & North American grid, PI only),
           FM-RDS (Asia grid, PI only), DAB (audio service ID only) */
        OtherService.iServiceID = (*pbiData).Separate(16);

        /* Remove two bytes from frequency count */
        iNumFreqTmp -= 2;
        break;

    case 11:
        /* DAB (data service ID) */
        OtherService.iServiceID = (*pbiData).Separate(32);

        /* Remove four bytes from frequency count */
        iNumFreqTmp -= 4;
        break;

    default:
        OtherService.iServiceID = SERV_ID_NOT_USED;
    }

    /* n frequencies: this field carries n, variable length bit fields. n is in
       the range 0 to 16. The number of frequencies, n, is determined from the
       length field of the header and the value of the Service Restriction flag
       and the Region/Schedule flag */
    switch (OtherService.iSystemID)
    {
    case 0:
    case 1:
    case 2:
        /* 16 bit frequency value for DRM/AM frequency */
        iFrequencyEntryLength = 2;
        break;

    default:
        /* 8 bit frequency value for all other broadcast systems currently
           defined */
        iFrequencyEntryLength = 1;
    }

    /* Check for error (length of body must be so long to include Service
       Restriction field and Region/Schedule field, also check that
       remaining number of bytes is devisible by iFrequencyEntryLength since
       we read iFrequencyEntryLength * 8 bits) */
    if ( (iNumFreqTmp < 0) || ((iNumFreqTmp % iFrequencyEntryLength) != 0) )
        return true;

    /* 16 bits are read */
    const size_t iNumFreq = iNumFreqTmp / iFrequencyEntryLength;

    OtherService.veciFrequencies.resize(iNumFreq);

    for (i = 0; i < iNumFreq; i++)
    {
        /* Frequency value iFrequencyEntryLength*8 bits. This field is coded as
           an unsigned integer and gives the frequency in kHz */
        int iFrequency = (*pbiData).Separate(iFrequencyEntryLength*8);

        if (iFrequencyEntryLength == 2) // mask off top bit, undefined
            iFrequency &= (unsigned short) 0x7fff;
        OtherService.veciFrequencies[i] = iFrequency;
    }

    if (iShortIDAnnounceFlag != 0)
        return false; // no error, but we don't support announcements

    /* Now, set data in global struct */
    Parameter.Lock();
    /* Check the version flag */
    if (bVersion != Parameter.AltFreqSign.bOtherServicesVersionFlag)
    {
        /* If version flag has changed, delete all data for this entity type and save flag */
        Parameter.AltFreqSign.ResetOtherServices(bVersion);
    }

    /* (first check if new object is not already there) */
    const size_t iCurNumAltFreqOtherServices = Parameter.AltFreqSign.vecOtherServices.size();

    bool bAltFreqIsAlreadyThere = false;
    for (i = 0; i < iCurNumAltFreqOtherServices; i++)
    {
        if (Parameter.AltFreqSign.vecOtherServices[i] == OtherService)
            bAltFreqIsAlreadyThere = true;
    }
    if (bAltFreqIsAlreadyThere == false)
        Parameter.AltFreqSign.vecOtherServices.push_back(OtherService);
    Parameter.Unlock();

    return false;
}


/******************************************************************************\
* Data entity Type 12 (Language and country data entity)                       *
\******************************************************************************/
bool CSDCReceive::DataEntityType12(CVector<_BINARY>* pbiData,
                                       const int iLengthOfBody,
                                       CParameter& Parameter)
{
    int i;

    /* Check length -> must be 5 bytes */
    if (iLengthOfBody != 5)
        return true;

    /* Short Id: this field indicates the short Id for the service concerned */
    const int iShortID = (*pbiData).Separate(2);

    /* rfu: these 2 bits are reserved for future use and shall be set to zero
       until they are defined */
    if ((*pbiData).Separate(2) != 0)
        return true;

    Parameter.Lock();
    /* Language code: this 24-bit field identifies the language of the target
       audience of the service according to ISO 639-2 using three lower case
       characters as specified by ISO 8859-1. If the language is not specified,
       the field shall contain three "-" characters */
    Parameter.Service[iShortID].strLanguageCode = "";
    for (i = 0; i < 3; i++)
    {
        /* Get character */
        const char cNewChar = char((*pbiData).Separate(8));

        /* Append new character */
        Parameter.Service[iShortID].strLanguageCode.append(&cNewChar, 1);
    }

    /* Country code: this 16-bit field identifies the country of origin of the
       service (the site of the studio) according to ISO 3166 using two lower
       case characters as specified by ISO 8859-1. If the country code is not
       specified, the field shall contain two "-" characters */
    Parameter.Service[iShortID].strCountryCode = "";
    for (i = 0; i < 2; i++)
    {
        /* Get character */
        const char cNewChar = char((*pbiData).Separate(8));

        /* Append new character */
        Parameter.Service[iShortID].strCountryCode.append(&cNewChar, 1);
    }
    Parameter.Unlock();

    return false;
}

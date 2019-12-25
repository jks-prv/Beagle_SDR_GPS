/******************************************************************************\
 * Technische Universitaet Darmstadt, Institut fuer Nachrichtentechnik
 * Copyright (c) 2007
 *
 * Author(s):
 *	Volker Fischer, Oliver Haffenden, Andrew Murphy
 *
 * Description:
 *	see MDITagItems.cpp
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

#ifndef MDI_TAG_ITEMS_H_INCLUDED
#define MDI_TAG_ITEMS_H_INCLUDED

#include "../GlobalDefinitions.h"
#include "MDIDefinitions.h"
#include "../Parameter.h"
#include "../util/Buffer.h"

/* Base class for all of the tag item generators. Handles some of the functions common to all tag items */
class CTagItemGenerator
{
public:
	void PutTagItemData(CVector<_BINARY> &vecbiDestination); // Call this to write the binary data (header + payload) to the std::vector
    int GetTotalLength() { return vecbiTagData.Size();} // returns the length in bits
	void Reset(); // Resets bit std::vector to zero length (i.e. no header)
	void GenEmptyTag(); // Generates valid tag item with zero payload length
	virtual ~CTagItemGenerator() {}
	virtual bool IsInProfile(char cProfile);

protected:
	virtual std::string GetTagName() = 0; // Return the tag name
	virtual std::string GetProfiles() = 0;

	// Prepare std::vector and make the header
	void PrepareTag(int iLenDataBits);

	void Enqueue(uint32_t iInformation, int iNumOfBits);

private:
	CVector<_BINARY> vecbiTagData; // Stores the generated data
};

/* Base class for tag items for applications with different profiles */
class CTagItemGeneratorWithProfiles : public CTagItemGenerator
{
public:
	CTagItemGeneratorWithProfiles();
	virtual bool IsInProfile(char cProfile);
protected:
	virtual std::string GetTagName() =0;
//private:
	virtual std::string GetProfiles()=0; // Return a std::string containing the set of profiles for this tag
};

class CTagItemGeneratorProTyMDI : public CTagItemGeneratorWithProfiles /* *ptr tag for MDI */
{
public:
	void GenTag();
protected:
	virtual std::string GetTagName();
	virtual std::string GetProfiles(); // Return a std::string containing the set of profiles for this tag
};

class CTagItemGeneratorProTyRSCI : public CTagItemGeneratorWithProfiles /* *ptr tag for RSCI */
{
public:
	void GenTag();
protected:
	virtual std::string GetTagName();
	virtual std::string GetProfiles(); // Return a std::string containing the set of profiles for this tag
};

class CTagItemGeneratorLoFrCnt : public CTagItemGeneratorWithProfiles /* dlfc tag */
{
public:
	CTagItemGeneratorLoFrCnt();
	void GenTag();
protected:
	virtual std::string GetTagName();
	virtual std::string GetProfiles(); // Return a std::string containing the set of profiles for this tag
	int iLogFraCnt;
};

class CTagItemGeneratorFAC : public CTagItemGeneratorWithProfiles /* fac_ tag */
{
public:
	void GenTag(CParameter& Parameter, CSingleBuffer<_BINARY>& FACData);
protected:
	virtual std::string GetTagName();
	virtual std::string GetProfiles(); // Return a std::string containing the set of profiles for this tag
};

class CTagItemGeneratorSDC : public CTagItemGeneratorWithProfiles /* sdc_ tag */
{
public:
	void GenTag(CParameter& Parameter, CSingleBuffer<_BINARY>& FACData);
protected:
	virtual std::string GetTagName();
	virtual std::string GetProfiles(); // Return a std::string containing the set of profiles for this tag
};

class CTagItemGeneratorSDCChanInf : public CTagItemGeneratorWithProfiles /* sdci tag */
{
public:
	void GenTag(CParameter& Parameter);
protected:
	virtual std::string GetTagName();
	virtual std::string GetProfiles(); // Return a std::string containing the set of profiles for this tag
};

class CTagItemGeneratorRobMod : public CTagItemGeneratorWithProfiles /* robm tag */
{
public:
	void GenTag(ERobMode eCurRobMode);
protected:
	virtual std::string GetTagName();
	virtual std::string GetProfiles(); // Return a std::string containing the set of profiles for this tag
};

class CTagItemGeneratorRINF : public CTagItemGeneratorWithProfiles /* info tag */
{
public:
	void GenTag(std::string strUTF8Text);
protected:
	virtual std::string GetTagName();
	virtual std::string GetProfiles(); // Return a std::string containing the set of profiles for this tag
};

class CTagItemGeneratorStr : public CTagItemGeneratorWithProfiles /* strx tag */
{
public:
	CTagItemGeneratorStr();
	void SetStreamNumber(int iStrNum);
	void GenTag(CParameter& Parameter, CSingleBuffer<_BINARY>& FACData);
protected:
	virtual std::string GetTagName();
	virtual std::string GetProfiles(); // Return a std::string containing the set of profiles for this tag
	int iStreamNumber;
};

/* all MER tags have the same format, only the name is different */
class CTagItemGeneratorMERFormat : public CTagItemGeneratorWithProfiles
{
public:
	void GenTag(bool bIsValid, _REAL rMER);
protected:
	virtual std::string GetTagName() {return "";}
	virtual std::string GetProfiles() {return "";} // Return a std::string containing the set of profiles for this tag
};

class CTagItemGeneratorRWMF : public CTagItemGeneratorMERFormat /* RWMF tag */
{
protected:
	virtual std::string GetTagName();
	virtual std::string GetProfiles(); // Return a std::string containing the set of profiles for this tag
};

class CTagItemGeneratorRWMM : public CTagItemGeneratorMERFormat /* RWMM tag */
{
protected:
	virtual std::string GetTagName();
	virtual std::string GetProfiles(); // Return a std::string containing the set of profiles for this tag
};

class CTagItemGeneratorRMER : public CTagItemGeneratorMERFormat /* RMER tag */
{
protected:
	virtual std::string GetTagName();
	virtual std::string GetProfiles(); // Return a std::string containing the set of profiles for this tag
};

class CTagItemGeneratorRDOP : public CTagItemGeneratorMERFormat /* RDOP tag */
{
protected:
	virtual std::string GetTagName();
	virtual std::string GetProfiles(); // Return a std::string containing the set of profiles for this tag
};

class CTagItemGeneratorRDEL : public CTagItemGeneratorWithProfiles /* RDEL tag */
{
public:
	void GenTag(bool bIsValid, const CRealVector &vecrThresholds, const CRealVector &vecrIntervals);
protected:
	virtual std::string GetTagName();
	virtual std::string GetProfiles(); // Return a std::string containing the set of profiles for this tag
};

class CTagItemGeneratorRAFS : public CTagItemGeneratorWithProfiles /* RAFS tag */
{
public:
	void GenTag(CParameter& Parameter);
protected:
	virtual std::string GetTagName();
	virtual std::string GetProfiles(); // Return a std::string containing the set of profiles for this tag
};

class CTagItemGeneratorRINT : public CTagItemGeneratorWithProfiles /* rnic tag */
{
public:
	void GenTag(bool bIsValid, CReal rIntFreq, CReal rINR, CReal rICR);
protected:
	virtual std::string GetTagName();
	virtual std::string GetProfiles(); // Return a std::string containing the set of profiles for this tag
};

class CTagItemGeneratorRNIP : public CTagItemGeneratorWithProfiles /* rnic tag */
{
public:
	void GenTag(bool bIsValid, CReal rIntFreq, CReal rISR);
protected:
	virtual std::string GetTagName();
	virtual std::string GetProfiles(); // Return a std::string containing the set of profiles for this tag
};

class CTagItemGeneratorSignalStrength : public CTagItemGeneratorWithProfiles /* rdbv tag */
{
public:
	void GenTag(bool bIsValid, _REAL rSigStrength);
protected:
	virtual std::string GetTagName();
	virtual std::string GetProfiles(); // Return a std::string containing the set of profiles for this tag
};

class CTagItemGeneratorReceiverStatus : public CTagItemGeneratorWithProfiles /* rsta tag */
{
public:
	void GenTag(CParameter& Parameter);
protected:
	virtual std::string GetTagName();
	virtual std::string GetProfiles(); // Return a std::string containing the set of profiles for this tag
};

class CTagItemGeneratorProfile : public CTagItemGeneratorWithProfiles /* rpro */
{
public:
	void GenTag(char cProfile);
protected:
	virtual std::string GetTagName();
	virtual std::string GetProfiles(); // Return a std::string containing the set of profiles for this tag
};

class CTagItemGeneratorRxDemodMode : public CTagItemGeneratorWithProfiles /* rdmo */
{
public:
	void GenTag(ERecMode eMode); /* ERecMode defined in DRMReceiver.h but can't include it! */
protected:
	virtual std::string GetTagName();
	virtual std::string GetProfiles(); // Return a std::string containing the set of profiles for this tag
};

class CTagItemGeneratorRxFrequency : public CTagItemGeneratorWithProfiles /* rfre */
{
public:
	void GenTag(bool bIsValid, int iFrequency);
protected:
	virtual std::string GetTagName();
	virtual std::string GetProfiles(); // Return a std::string containing the set of profiles for this tag
};

class CTagItemGeneratorRxActivated : public CTagItemGeneratorWithProfiles /* ract */
{
public:
	void GenTag(bool bActivated);
protected:
	virtual std::string GetTagName();
	virtual std::string GetProfiles(); // Return a std::string containing the set of profiles for this tag
};

class CTagItemGeneratorRxBandwidth : public CTagItemGeneratorWithProfiles /* rbw_ */
{
public:
	void GenTag(bool bIsValid, _REAL rBandwidth);
protected:
	virtual std::string GetTagName();
	virtual std::string GetProfiles(); // Return a std::string containing the set of profiles for this tag
};

class CTagItemGeneratorRxService : public CTagItemGeneratorWithProfiles /* rser */
{
public:
	void GenTag(bool bIsValid, int iService);
protected:
	virtual std::string GetTagName();
	virtual std::string GetProfiles(); // Return a std::string containing the set of profiles for this tag
};

class CTagItemGeneratorRBP : public CTagItemGeneratorWithProfiles /*rbp0 etc  */
{
public:
	CTagItemGeneratorRBP();
	void SetStreamNumber(int iStrNum);
	void GenTag(); // Not yet implemented
protected:
	virtual std::string GetTagName();
	virtual std::string GetProfiles(); // Return a std::string containing the set of profiles for this tag
	int iStreamNumber;
};

//andrewm - 7/11/2006
class CTagItemGeneratorGPS : public CTagItemGeneratorWithProfiles /* rgps */
{
public:
	void GenTag(bool bIsValid, gps_data_t& GPSData);
protected:
	virtual std::string GetTagName();
	virtual std::string GetProfiles(); // Return a std::string containing the set of profiles for this tag
};

// oliver 17/1/2007
class CTagItemGeneratorPowerSpectralDensity : public CTagItemGeneratorWithProfiles /* rpsd */
{
public:
	void GenTag(CParameter& Parameter);
protected:
	virtual std::string GetTagName();
	virtual std::string GetProfiles(); // Return a std::string containing the set of profiles for this tag
};

class CTagItemGeneratorPowerImpulseResponse : public CTagItemGeneratorWithProfiles /* rpir */
{
public:
	void GenTag(CParameter& Parameter);
protected:
	virtual std::string GetTagName();
	virtual std::string GetProfiles(); // Return a std::string containing the set of profiles for this tag
};

class CTagItemGeneratorPilots : public CTagItemGeneratorWithProfiles /* rpil */
{
public:
	void GenTag(CParameter& Parameter);
protected:
	virtual std::string GetTagName();
	virtual std::string GetProfiles(); // Return a std::string containing the set of profiles for this tag
};

class CTagItemGeneratorAMAudio : public CTagItemGeneratorWithProfiles /* rama */
{
public:
		void GenTag(CParameter& Parameter, CSingleBuffer<_BINARY>& AudioData
);
protected:
		virtual std::string GetTagName(void);
		virtual std::string GetProfiles(void); // Return a std::string containing the set of profiles for this tag
};

#endif

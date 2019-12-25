/******************************************************************************\
 * Technische Universitaet Darmstadt, Institut fuer Nachrichtentechnik
 * Copyright (c) 2001
 *
 * Author(s):
 *	Volker Fischer
 *
 * Description:
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

#if !defined(MOTSLIDESHOW_H__3B0UBVE98732KJVEW363LIHGEW982__INCLUDED_)
#define MOTSLIDESHOW_H__3B0UBVE98732KJVEW363LIHGEW982__INCLUDED_

#include "../GlobalDefinitions.h"
#include "../util/Vector.h"
#include "DABMOT.h"


/* Classes ********************************************************************/
/* Encoder ------------------------------------------------------------------ */
class CMOTSlideShowEncoder
{
  public:
    CMOTSlideShowEncoder (): vecPicFileNames(0), bRemovePath(false)
    {
    }
    virtual ~ CMOTSlideShowEncoder ()
    {
    }

    void Init ();

    void GetDataUnit (CVector < _BINARY > &vecbiNewData);

    void AddFileName(const std::string & strFileName, const std::string & strFormat);
    void ClearAllFileNames() {vecPicFileNames.Init(0);}
    void SetPathRemoval(bool bNewRemovePath) {bRemovePath=bNewRemovePath;}
    bool GetTransStat(std::string & strCurPict, _REAL & rCurPerc) const;

  protected:
    struct SPicDescr
    {
	std::string strName, strFormat;
    };
    void AddNextPicture ();
    std::string TransformFilename(const std::string strFileName);

    CMOTDABEnc MOTDAB;

    CVector < SPicDescr > vecPicFileNames;
    int iPictureCnt;

    std::string strCurObjName;

    bool bRemovePath;
};

#endif // !defined(MOTSLIDESHOW_H__3B0UBVE98732KJVEW363LIHGEW982__INCLUDED_)

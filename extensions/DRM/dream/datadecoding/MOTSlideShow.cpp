 /******************************************************************************\
 * Technische Universitaet Darmstadt, Institut fuer Nachrichtentechnik
 * Copyright (c) 2001
 *
 * Author(s):
 *	Volker Fischer
 *
 * Description:
 *	MOT applications (MOT Slideshow and Broadcast Web Site)
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

#include "MOTSlideShow.h"
using namespace std;

/* Implementation *************************************************************/
/******************************************************************************\
* Encoder                                                                      *
\******************************************************************************/
void
CMOTSlideShowEncoder::GetDataUnit (CVector < _BINARY > &vecbiNewData)
{
    /* Get new data group from MOT encoder. If the last MOT object was
       completely transmitted, this functions returns true. In this case, put
       a new picture to the MOT encoder object */
    if (MOTDAB.GetDataGroup (vecbiNewData))
	AddNextPicture ();
}

bool
CMOTSlideShowEncoder::GetTransStat (string & strCurPict, _REAL & rCurPerc) const
{
/*
	Name and current percentage of transmitted data of current picture.
*/
    strCurPict = strCurObjName;
    rCurPerc = MOTDAB.GetProgPerc ();

    if (vecPicFileNames.Size () != 0) {
        return true;
    }
    else {
        return false;
    }
}

string
CMOTSlideShowEncoder::TransformFilename(string strFileName)
{
    if (bRemovePath)
    {
        /* Keep only the filename */
#ifdef _WIN32
        const size_t pos = strFileName.rfind('\\');
#else
        const size_t pos = strFileName.rfind('/');
#endif
        if (pos != string::npos)
            strFileName.erase(0, pos + 1);
    }
    else
    {
#ifdef _WIN32
        /* Remove the C:\ if any */
        if (strFileName.length() >= 2)
        {
            if (strFileName[1] == ':' &&
                ((strFileName[0]>='A' && strFileName[0]<='Z') ||
                 (strFileName[0]>='a' && strFileName[0]<='z')))
            {  
                strFileName.erase(0, 2);
                /* Remove the first '\' if any */
                if (strFileName.length() >= 1)
                {
                    if (strFileName[0] == '\\')
                        strFileName.erase(0, 1);
                }
            }
        }
        /* Replace all '\' by '/' */
        const int len = strFileName.length();
        for (int i = 0; i < len; i++)
            if (strFileName[i] == '\\')
                strFileName[i] = '/';
#else
        /* Remove the first '/' if any */
        if (strFileName.length() >= 1)
        {
            if (strFileName[0] == '/')
                strFileName.erase(0, 1);
        }
#endif
    }
    return strFileName;
}

void
CMOTSlideShowEncoder::Init ()
{
    /* Reset picture counter for browsing in the vector of file names. Start
       with first picture */
    iPictureCnt = 0;
    strCurObjName = "";

    MOTDAB.Reset ();

    AddNextPicture ();
}

void
CMOTSlideShowEncoder::AddNextPicture ()
{
    /* Make sure at least one picture is in container */
    if (vecPicFileNames.Size () > 0)
      {
	  /* Get current file name */
	  strCurObjName = vecPicFileNames[iPictureCnt].strName;

	  /* Try to open file binary */
	  FILE *pFiBody = fopen (strCurObjName.c_str (), "rb");

	  if (pFiBody != nullptr)
	    {
		CMOTObject MOTPicture;
		_BYTE byIn;

		/* Set file name and format string */
		MOTPicture.strName = TransformFilename(vecPicFileNames[iPictureCnt].strName);
		MOTPicture.strFormat = vecPicFileNames[iPictureCnt].strFormat;

		/* Fill body data with content of selected file */
		MOTPicture.vecbRawData.Init (0);

		while (fread ((void *) &byIn, size_t (1), size_t (1), pFiBody)
		       != size_t (0))
		  {
		      /* Add one byte = SIZEOF__BYTE bits */
		      MOTPicture.vecbRawData.Enlarge (SIZEOF__BYTE);
		      MOTPicture.vecbRawData.Enqueue ((uint32_t) byIn,
						      SIZEOF__BYTE);
		  }

		/* Close the file afterwards */
		fclose (pFiBody);

		MOTDAB.SetMOTObject (MOTPicture);
	    }

	  /* Set file counter to next picture, test for wrap around */
	  iPictureCnt++;
	  if (iPictureCnt == vecPicFileNames.Size ())
	      iPictureCnt = 0;
      }
}

void
CMOTSlideShowEncoder::AddFileName (const string & strFileName,
				   const string & strFormat)
{
    /* Only ContentSubType "JFIF" (JPEG) and ContentSubType "PNG" are allowed
       for SlideShow application (not tested here!) */
    /* Add file name to the list */
    const int iOldNumObj = vecPicFileNames.Size ();
    vecPicFileNames.Enlarge (1);
    vecPicFileNames[iOldNumObj].strName = strFileName;
    vecPicFileNames[iOldNumObj].strFormat = strFormat;
}

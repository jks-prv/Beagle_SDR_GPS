/******************************************************************************\
 *
 * Copyright (c) 2013
 *
 * Author(s):
 *  David Flamand
 *
 * Description:
 *  see ConsoleIO.cpp
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

#include <signal.h>
#include "../DRMReceiver.h"

#include "Journaline.h"

class CConsoleIO
{
    public:
        void Enter(CDRMReceiver* pDRMReceiver);
        void Leave();
        void Restart();
        ERunState Update(drm_t *drm);

    protected:
        int ETypeRxStatus2int(ETypeRxStatus eTypeRxStatus);
        CDRMReceiver* pDRMReceiver;
        unsigned long long time_iq, time;
        CVector<_COMPLEX> facIQ, sdcIQ, mscIQ;
        char *text_msg_utf8_enc;
        CDataDecoder* decoder;
    
    private:
        // Journaline
        int total;
        int ready;
};

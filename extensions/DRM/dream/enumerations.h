/******************************************************************************\
 * Technische Universitaet Darmstadt, Institut fuer Nachrichtentechnik
 * Copyright (c) 2001-2001-2014
 *
 * Author(s):
 *  Volker Fischer, Andrew Murphy, Andrea Russo
 *
 * Description:
 * enumerated types - in separate file to reduce global dependencies
 *
 *******************************************************************************
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
#ifndef ENUMERATIONS_H
#define ENUMERATIONS_H

enum ESpecOcc {SO_0=0, SO_1=1, SO_2=2, SO_3=3, SO_4=4, SO_5=5}; /* SO: Spectrum Occupancy */
enum ERobMode {RM_ROBUSTNESS_MODE_A=0, RM_ROBUSTNESS_MODE_B=1,
               RM_ROBUSTNESS_MODE_C=2, RM_ROBUSTNESS_MODE_D=3, RM_ROBUSTNESS_MODE_E=4,
               RM_NO_MODE_DETECTED=-1
              }; /* RM: Robustness Mode */

/* SI: Symbol Interleaver */
enum ESymIntMod { SI_LONG, SI_SHORT, SI_MODE_E };

/* CS: Coding Scheme */
enum ECodScheme { CS_1_SM, CS_2_SM, CS_3_SM, CS_3_HMSYM, CS_3_HMMIX };

/* CT: Channel Type */
enum EChanType { CT_MSC, CT_SDC, CT_FAC };

enum ETypeIntFreq
{ FLINEAR, FDFTFILTER, FWIENER };
enum ETypeIntTime
{ TLINEAR, TWIENER };
enum ETypeSNREst
{ SNR_FAC, SNR_PIL };
enum ETypeRxStatus
{ NOT_PRESENT, CRC_ERROR, DATA_ERROR, RX_OK };
/* RM: Receiver mode (analog or digital demodulation) */

enum ERecMode
{ RM_DRM, RM_AM, RM_FM, RM_NONE };

/* Acquisition state of receiver */
enum EAcqStat {AS_NO_SIGNAL, AS_WITH_SIGNAL};

/* Receiver state */
enum ERecState {RS_TRACKING, RS_ACQUISITION};

#endif // ENUMERATIONS_H

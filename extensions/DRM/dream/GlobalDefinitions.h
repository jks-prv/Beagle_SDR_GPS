/******************************************************************************\
 * Technische Universitaet Darmstadt, Institut fuer Nachrichtentechnik
 * Copyright (c) 2001-2006
 *
 * Author(s):
 *	Volker Fischer
 *
 * Description:
 *	Global definitions
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

#ifndef GLOBALDEFINITIONS_H
#define GLOBALDEFINITIONS_H

#include <complex>
#include <string>
#include <cstdio>
#include <limits>
#include <cmath>
#include "tables/TableDRMGlobal.h"
#ifndef M_PI
# define M_PI (3.14159265358979323846)
#endif

/* Definitions ****************************************************************/
/* When you define the following flag, a directory called
   "test" MUST EXIST in the windows directory (or linux directory if you use
   Linux)! */
#define _DEBUG_
#undef _DEBUG_

#ifdef _WIN32 /* For Windows set flags here, otherwise it is set by configure */

# undef NOMINMAX
# define NOMINMAX 1

# define DLL_EXPORT 1
//# undef DLL_EXPORT 1

#endif /* _WIN32 */

/* Choose algorithms -------------------------------------------------------- */
/* There are two algorithms available for frequency offset estimation for
   tracking mode: Using frequency pilots or the guard-interval correlation. In
   case of guard-interval correlation (which will be chosen if this macro is
   defined), the Hilbert filter in TimeSync must be used all the time -> more
   CPU usage. Also, the frequency tracking range is smaller */
#undef USE_FRQOFFS_TRACK_GUARDCORR

/* The sample rate offset estimation can be done using the frequency pilots or
   the movement of the estimated impulse response. Defining this macro will
   enable the frequency pilot based estimation. Simulations showed that this
   method is more vulnerable to bad channel situations */
#undef USE_SAMOFFS_TRACK_FRE_PIL

/* Using max-log MAP decoder. A lot more memory and CPU is needed for this
   method. This is just for showing the potential of an improved decoding
   method and should not be activated for the "regular" version of Dream */
#undef USE_MAX_LOG_MAP

/* This method tries to speed up the audio output after a re-synchronization
   when long symbol interleaver is used. We work with erasure symbols to mark
   data which is not yet received. We hope that the channel decoder can still
   decode audio even if not all data is yet received to fill the interleaver
   history */
#define USE_ERASURE_FOR_FASTER_ACQ

/* If the following macro is defined, the Wiener filter for channel estimation
   in time direction will be a Decision-Directed channel estimation ->
   additional to the actual pilot cells, hard decisions about the data cells
   are used as new pilots, too */
#undef USE_DD_WIENER_FILT_TIME


/* Define the application specific data-types ------------------------------- */
#ifdef KIWISDR
    typedef	float						_REAL;
#else
    typedef	double						_REAL;
#endif

typedef	std::complex<_REAL>				_COMPLEX;
typedef short							_SAMPLE;
typedef unsigned char					_BYTE;

// bool seems not to work with linux TODO: Fix Me!
typedef unsigned char/*bool*/			_BINARY;

#if HAVE_STDINT_H
# include <stdint.h>
#elif HAVE_INTTYPES_H
# include <inttypes.h>
#elif defined(_WIN32)
# ifndef HAVE_INT8_T
#  define HAVE_INT8_T 1
typedef signed char int8_t;
# endif
# ifndef HAVE_INT16_T
#  define HAVE_INT16_T 1
typedef signed __int16 int16_t;
# endif
# ifndef HAVE_INT32_T
#  define HAVE_INT32_T 1
typedef signed __int32 int32_t;
# endif
typedef unsigned char uint8_t;
# ifndef HAVE_U_INT16_T
#  define HAVE_U_INT16_T 1
typedef unsigned __int16 uint16_t;
# endif
# ifndef HAVE_U_INT32_T
#  define HAVE_U_INT32_T 1
typedef unsigned __int32 uint32_t;
# endif
typedef signed __int64 int64_t;
typedef unsigned __int64 uint64_t;
#else
typedef signed char int8_t;
typedef unsigned char uint8_t;
#ifndef _INT16_T
    typedef signed int int16_t;
#endif
#ifndef _UINT16_T
typedef unsigned int uint16_t;
#endif
#ifndef _INT32_T
typedef signed long int32_t;
#endif
#ifndef _UINT32_T
typedef unsigned long uint32_t;
#endif
typedef signed long long int64_t;
typedef unsigned long long uint64_t;
#endif

/* Define type-specific information */
#define SIZEOF__BYTE					8
#define _MAXSHORT						std::numeric_limits<int16_t>::max()
#define _MAXREAL						((_REAL) 3.4e38) /* Max for float */

#ifdef USE_ERASURE_FOR_FASTER_ACQ
/* Use max-value for showing that this is an erasure */
# define ERASURE_TAG_VALUE				_MAXREAL
#endif


/* MAP ---------------------------------------------------------------------- */
#ifdef USE_MAX_LOG_MAP
    typedef _REAL						_DECISION;
    #define ML_SOFT_INF_MAX_VALUE		((_DECISION) 1e10)
    inline _BINARY ExtractBit(_DECISION dD) {
        return dD > 0 ? 1 : 0;
    }
    inline _DECISION BitToSoft(_BINARY biB) {
        return biB == 0 ? -1.0 : 1.0;
    }
#else
    typedef _BINARY						_DECISION;
    #define ExtractBit(a)				(a)
    #define BitToSoft(a)				(a)
#endif


/* Definitions for window message system ------------------------------------ */
typedef unsigned int					_MESSAGE_IDENT;
#define MS_FAC_CRC						1	/* MS: Message */
#define MS_SDC_CRC						2
#define MS_MSC_CRC						3
#define MS_FRAME_SYNC					4
#define MS_TIME_SYNC					5
#define MS_IOINTERFACE					6
#define MS_RESET_ALL					7
#define MS_MOT_OBJ_STAT					8

#define GUI_IQ_UPDATE_TIME			    500     /* Milliseconds */
#define GUI_CONTROL_UPDATE_TIME			1000    /* Milliseconds */


/* Global enumerations ------------------------------------------------------ */
enum ESpecOcc {SO_0, SO_1, SO_2, SO_3, SO_4, SO_5}; /* SO: Spectrum Occupancy */
enum ERobMode {RM_ROBUSTNESS_MODE_A, RM_ROBUSTNESS_MODE_B,
               RM_ROBUSTNESS_MODE_C, RM_ROBUSTNESS_MODE_D,
               RM_ROBUSTNESS_MODE_E,
               RM_NO_MODE_DETECTED
              }; /* RM: Robustness Mode */


/* Constants ---------------------------------------------------------------- */
const _REAL crPi = M_PI;


#define S9_DBUV 34.0 /* S9 in dBuV for converting HamLib S-meter readings to RSCI format */

/* Define a number for the case: log10(0), which would lead to #inf */
#define RET_VAL_LOG_0					(-200.0)




/* Classes ********************************************************************/
/* For metric */
class CDistance
{
public:
    /* Distance towards 0 or towards 1 */
    _REAL rTow0;
    _REAL rTow1;
};

/* Viterbi needs information of equalized received signal and channel */
class CEquSig
{
public:
    CEquSig() : cSig(_COMPLEX((_REAL) 0.0, (_REAL) 0.0)), rChan((_REAL) 0.0) {}
    CEquSig(const _COMPLEX cNS, const _REAL rNC) : cSig(cNS), rChan(rNC) {}

    _COMPLEX	cSig; /* Actual signal */
    _REAL		rChan; /* Channel power at this cell */
};


/* No GUI and no threads, we do not need mutex in this case */

class CMutex
{
public:
    void Lock() {}
    void Unlock() {}
};

class CWaitCondition
{
public:
    void WakeOne() {}
    bool Wait(CMutex*, unsigned long) {return true;}
};

class CGenErr
{
public:
    CGenErr(std::string strNE) : strError(strNE) {}
    std::string strError;
};


// FIXME something nicer than using "MAX_NUM_TAPS_DRM_CHAN"
/* For simulation, data from channel simulation */
#define MAX_NUM_TAPS_DRM_CHAN			4
template<class T> class CChanSimData
{
public:
    T					tIn; /* Channel input data */
    T					tOut; /* Output of the channel (with noise) */
    T					tRef; /* Channel reference signal (without noise) */
    _COMPLEX			veccTap[MAX_NUM_TAPS_DRM_CHAN]; /* Tap gains */
    _COMPLEX			veccTapBackw[MAX_NUM_TAPS_DRM_CHAN];
};
typedef CChanSimData<_REAL>		CChanSimDataMod; /* OFDM modulated signals */
typedef CChanSimData<_COMPLEX>	CChanSimDataDemod; /* Demodulated signals */

/* Global functions ***********************************************************/
/* Converting _REAL to _SAMPLE */
inline _SAMPLE Real2Sample(const _REAL rInput)
{
    /* Lower bound */
    if (rInput < -_MAXSHORT)
        return -_MAXSHORT;

    /* Upper bound */
    if (rInput > _MAXSHORT)
        return _MAXSHORT;

    return (_SAMPLE) rInput;
}


#endif // !defined(DEF_H__3B0BA660_CA63_4344_BB2B_23E7A0D31912__INCLUDED_)

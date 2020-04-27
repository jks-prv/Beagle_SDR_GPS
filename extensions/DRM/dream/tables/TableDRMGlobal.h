/******************************************************************************\
 * Technische Universitaet Darmstadt, Institut fuer Nachrichtentechnik
 * Copyright (c) 2001
 *
 * Author(s):
 *	Volker Fischer
 *
 * Description:
 *	DRM global definitions
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

#if !defined(TABLE_DRM_GLOB_H__3B0_CA63_4344_BB2B_23E7912__INCLUDED_)
#define TABLE_DRM_GLOB_H__3B0_CA63_4344_BB2B_23E7912__INCLUDED_


/* Definitions ****************************************************************/
/* We define a "virtual" intermedia frequency for the DC carrier in the range
   of the FFT-size. This IF is independent of the "real" IF defined by the
   frequency estimation acquisition unit. Here, we are constrained to certain
   numbers to get continuous signals like they are defined in the DRM-standard,
   i.e. the frequency pilots which have to be continuous. Our IF must be a
   multiple of 1500 Hz and must also be chosen so that the largest mode (20 kHz)
   must fit into the range of the FFT-size. Therefore 6000 Hz was chosen */
#define VIRTUAL_INTERMED_FREQ			6000	// Hz

/* Default sample rate MUST be set to a safe value of 48000,
   for testing purpose it must be a multiple of 750 */
#define DEFAULT_SOUNDCRD_SAMPLE_RATE	48000	// Hz

/* DRM parameters */
#define NUM_FRAMES_IN_SUPERFRAME		3

#define RMA_FFT_SIZE_N					1152	// RMB: Robustness Mode A
#define RMA_NUM_SYM_PER_FRAME			15
#define RMA_ENUM_TG_TU					1
#define RMA_DENOM_TG_TU					9

#define RMB_FFT_SIZE_N					1024	// RMA: Robustness Mode B
#define RMB_NUM_SYM_PER_FRAME			15
#define RMB_ENUM_TG_TU					1
#define RMB_DENOM_TG_TU					4

#define RMC_FFT_SIZE_N					704		// RMC: Robustness Mode C
#define RMC_NUM_SYM_PER_FRAME			20
#define RMC_ENUM_TG_TU					4
#define RMC_DENOM_TG_TU					11

#define RMD_FFT_SIZE_N					448		// RMD: Robustness Mode D
#define RMD_NUM_SYM_PER_FRAME			24
#define RMD_ENUM_TG_TU					11
#define RMD_DENOM_TG_TU					14

#define MAX_NUM_STREAMS					4
#define MAX_NUM_SERVICES				4

#define NUM_ROBUSTNESS_MODES			4

#define ADJ_FOR_SRATE(value, srate)		(value * srate / 48000) /* Reminder: 48000 is the right value, do not edit! (don't replace it by DEFAULT_SOUNDCRD_SAMPLE_RATE) */


/* Service ID has 24 bits, define a number which cannot be an ID and fits into
   the 32 bits of the length of the variable (e.g.: 1 << 25) */
#define SERV_ID_NOT_USED				(1 << 25)

/* Define a stream ID which is not valid to show that this service is not
   attached to a stream */
#define STREAM_ID_NOT_USED				(MAX_NUM_STREAMS + 1)


/* Audio stream definitions ------------------------------------------------- */
/* The text message (when present) shall occupy the last four bytes of the
   lower protected part of each logical frame carrying an audio stream
   (6.5.1) */
#define NUM_BYTES_TEXT_MESS_IN_AUD_STR	4

/* Transform length: the transform length is 960 to ensure that one
   audio frame corresponds to 80 ms or 40 ms in time. This is required
   to harmonize CELP and AAC frame lengths and thus to allow the
   combination of an integer number of audio frames to build an audio
   super frame of 400 ms duration */
#define AUD_DEC_TRANSFROM_LENGTH		960

/* Number of DRM frames per minute */
#define NUM_DRM_FRAMES_PER_MIN			150


#endif // !defined(TABLE_DRM_GLOB_H__3B0_CA63_4344_BB2B_23E7912__INCLUDED_)

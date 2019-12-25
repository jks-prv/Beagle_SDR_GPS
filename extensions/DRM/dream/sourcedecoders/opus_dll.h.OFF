#ifndef __OPUS_DLL_H
#define __OPUS_DLL_H

#ifdef _WIN32
# include <windows.h>
#else
# include <dlfcn.h>
#endif

/* (C) COPYRIGHT 1994-2002 Xiph.Org Foundation */
/* Modified by Jean-Marc Valin */
/*
   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

   - Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.

   - Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER
   OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
   LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
/* opus_types.h based on ogg_types.h from libogg */

/**
   @file opus_types.h
   @brief Opus reference implementation types
*/
#ifndef OPUS_TYPES_H
#define OPUS_TYPES_H

/* Use the real stdint.h if it's there (taken from Paul Hsieh's pstdint.h) */
#if (defined(__STDC__) && __STDC__ && __STDC_VERSION__ >= 199901L) || (defined(__GNUC__) && (defined(_STDINT_H) || defined(_STDINT_H_)) || defined (HAVE_STDINT_H))
#include <stdint.h>

   typedef int16_t opus_int16;
   typedef uint16_t opus_uint16;
   typedef int32_t opus_int32;
   typedef uint32_t opus_uint32;
#elif defined(_WIN32)

#  if defined(__CYGWIN__)
#    include <_G_config.h>
     typedef _G_int32_t opus_int32;
     typedef _G_uint32_t opus_uint32;
     typedef _G_int16 opus_int16;
     typedef _G_uint16 opus_uint16;
#  elif defined(__MINGW32__)
     typedef short opus_int16;
     typedef unsigned short opus_uint16;
     typedef int opus_int32;
     typedef unsigned int opus_uint32;
#  elif defined(__MWERKS__)
     typedef int opus_int32;
     typedef unsigned int opus_uint32;
     typedef short opus_int16;
     typedef unsigned short opus_uint16;
#  else
     /* MSVC/Borland */
     typedef __int32 opus_int32;
     typedef unsigned __int32 opus_uint32;
     typedef __int16 opus_int16;
     typedef unsigned __int16 opus_uint16;
#  endif

#elif defined(__MACOS__)

#  include <sys/types.h>
   typedef SInt16 opus_int16;
   typedef UInt16 opus_uint16;
   typedef SInt32 opus_int32;
   typedef UInt32 opus_uint32;

#elif (defined(__APPLE__) && defined(__MACH__)) /* MacOS X Framework build */

#  include <sys/types.h>
   typedef int16_t opus_int16;
   typedef u_int16_t opus_uint16;
   typedef int32_t opus_int32;
   typedef u_int32_t opus_uint32;

#elif defined(__BEOS__)

   /* Be */
#  include <inttypes.h>
   typedef int16 opus_int16;
   typedef u_int16 opus_uint16;
   typedef int32_t opus_int32;
   typedef u_int32_t opus_uint32;

#elif defined (__EMX__)

   /* OS/2 GCC */
   typedef short opus_int16;
   typedef unsigned short opus_uint16;
   typedef int opus_int32;
   typedef unsigned int opus_uint32;

#elif defined (DJGPP)

   /* DJGPP */
   typedef short opus_int16;
   typedef unsigned short opus_uint16;
   typedef int opus_int32;
   typedef unsigned int opus_uint32;

#elif defined(R5900)

   /* PS2 EE */
   typedef int opus_int32;
   typedef unsigned opus_uint32;
   typedef short opus_int16;
   typedef unsigned short opus_uint16;

#elif defined(__SYMBIAN32__)

   /* Symbian GCC */
   typedef signed short opus_int16;
   typedef unsigned short opus_uint16;
   typedef signed int opus_int32;
   typedef unsigned int opus_uint32;

#elif defined(CONFIG_TI_C54X) || defined (CONFIG_TI_C55X)

   typedef short opus_int16;
   typedef unsigned short opus_uint16;
   typedef long opus_int32;
   typedef unsigned long opus_uint32;

#elif defined(CONFIG_TI_C6X)

   typedef short opus_int16;
   typedef unsigned short opus_uint16;
   typedef int opus_int32;
   typedef unsigned int opus_uint32;

#else

   /* Give up, take a reasonable guess */
   typedef short opus_int16;
   typedef unsigned short opus_uint16;
   typedef int opus_int32;
   typedef unsigned int opus_uint32;

#endif

#define opus_int         int                     /* used for counters etc; at least 16 bits */
#define opus_int64       long long
#define opus_int8        signed char

#define opus_uint        unsigned int            /* used for counters etc; at least 16 bits */
#define opus_uint64      unsigned long long
#define opus_uint8       unsigned char

#endif  /* OPUS_TYPES_H */
/* Copyright (c) 2010-2011 Xiph.Org Foundation, Skype Limited
   Written by Jean-Marc Valin and Koen Vos */
/*
   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

   - Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.

   - Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER
   OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
   LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/**
 * @file opus_defines.h
 * @brief Opus reference implementation constants
 */

#ifndef OPUS_DEFINES_H
#define OPUS_DEFINES_H

/*#include "opus_types.h"*/

#ifdef __cplusplus
extern "C" {
#endif

/** @defgroup opus_errorcodes Error codes
 * @{
 */
/** No error @hideinitializer*/
#define OPUS_OK                0
/** One or more invalid/out of range arguments @hideinitializer*/
#define OPUS_BAD_ARG          -1
/** The mode struct passed is invalid @hideinitializer*/
#define OPUS_BUFFER_TOO_SMALL -2
/** An internal error was detected @hideinitializer*/
#define OPUS_INTERNAL_ERROR   -3
/** The compressed data passed is corrupted @hideinitializer*/
#define OPUS_INVALID_PACKET   -4
/** Invalid/unsupported request number @hideinitializer*/
#define OPUS_UNIMPLEMENTED    -5
/** An encoder or decoder structure is invalid or already freed @hideinitializer*/
#define OPUS_INVALID_STATE    -6
/** Memory allocation has failed @hideinitializer*/
#define OPUS_ALLOC_FAIL       -7
/**@}*/

/** @cond OPUS_INTERNAL_DOC */
/**Export control for opus functions */

#if defined(__GNUC__) && defined(OPUS_BUILD)
# define OPUS_EXPORT __attribute__ ((visibility ("default")))
#elif defined(WIN32) && !defined(__MINGW32__)
# ifdef OPUS_BUILD
#   define OPUS_EXPORT __declspec(dllexport)
# else
#   define OPUS_EXPORT
# endif
#else
# define OPUS_EXPORT
#endif

# if !defined(OPUS_GNUC_PREREQ)
#  if defined(__GNUC__)&&defined(__GNUC_MINOR__)
#   define OPUS_GNUC_PREREQ(_maj,_min) \
 ((__GNUC__<<16)+__GNUC_MINOR__>=((_maj)<<16)+(_min))
#  else
#   define OPUS_GNUC_PREREQ(_maj,_min) 0
#  endif
# endif

#if (!defined(__STDC_VERSION__) || (__STDC_VERSION__ < 199901L) )
# if OPUS_GNUC_PREREQ(3,0)
#  define OPUS_RESTRICT __restrict__
# elif (defined(_MSC_VER) && _MSC_VER >= 1400)
#  define OPUS_RESTRICT __restrict
# else
#  define OPUS_RESTRICT
# endif
#else
# define OPUS_RESTRICT restrict
#endif

/**Warning attributes for opus functions
  * NONNULL is not used in OPUS_BUILD to avoid the compiler optimizing out
  * some paranoid null checks. */
#if defined(__GNUC__) && OPUS_GNUC_PREREQ(3, 4)
# define OPUS_WARN_UNUSED_RESULT __attribute__ ((__warn_unused_result__))
#else
# define OPUS_WARN_UNUSED_RESULT
#endif
#if !defined(OPUS_BUILD) && defined(__GNUC__) && OPUS_GNUC_PREREQ(3, 4)
# define OPUS_ARG_NONNULL(_x)  __attribute__ ((__nonnull__(_x)))
#else
# define OPUS_ARG_NONNULL(_x)
#endif

/** These are the actual Encoder CTL ID numbers.
  * They should not be used directly by applications.
  * In general, SETs should be even and GETs should be odd.*/
#define OPUS_SET_APPLICATION_REQUEST         4000
#define OPUS_GET_APPLICATION_REQUEST         4001
#define OPUS_SET_BITRATE_REQUEST             4002
#define OPUS_GET_BITRATE_REQUEST             4003
#define OPUS_SET_MAX_BANDWIDTH_REQUEST       4004
#define OPUS_GET_MAX_BANDWIDTH_REQUEST       4005
#define OPUS_SET_VBR_REQUEST                 4006
#define OPUS_GET_VBR_REQUEST                 4007
#define OPUS_SET_BANDWIDTH_REQUEST           4008
#define OPUS_GET_BANDWIDTH_REQUEST           4009
#define OPUS_SET_COMPLEXITY_REQUEST          4010
#define OPUS_GET_COMPLEXITY_REQUEST          4011
#define OPUS_SET_INBAND_FEC_REQUEST          4012
#define OPUS_GET_INBAND_FEC_REQUEST          4013
#define OPUS_SET_PACKET_LOSS_PERC_REQUEST    4014
#define OPUS_GET_PACKET_LOSS_PERC_REQUEST    4015
#define OPUS_SET_DTX_REQUEST                 4016
#define OPUS_GET_DTX_REQUEST                 4017
#define OPUS_SET_VBR_CONSTRAINT_REQUEST      4020
#define OPUS_GET_VBR_CONSTRAINT_REQUEST      4021
#define OPUS_SET_FORCE_CHANNELS_REQUEST      4022
#define OPUS_GET_FORCE_CHANNELS_REQUEST      4023
#define OPUS_SET_SIGNAL_REQUEST              4024
#define OPUS_GET_SIGNAL_REQUEST              4025
#define OPUS_GET_LOOKAHEAD_REQUEST           4027
/* #define OPUS_RESET_STATE 4028 */
#define OPUS_GET_SAMPLE_RATE_REQUEST         4029
#define OPUS_GET_FINAL_RANGE_REQUEST         4031
#define OPUS_GET_PITCH_REQUEST               4033
#define OPUS_SET_GAIN_REQUEST                4034
#define OPUS_GET_GAIN_REQUEST                4045
#define OPUS_SET_LSB_DEPTH_REQUEST           4036
#define OPUS_GET_LSB_DEPTH_REQUEST           4037

/* Macros to trigger compilation errors when the wrong types are provided to a CTL */
#define __opus_check_int(x) (((void)((x) == (opus_int32)0)), (opus_int32)(x))
#define __opus_check_int_ptr(ptr) ((ptr) + ((ptr) - (opus_int32*)(ptr)))
#define __opus_check_uint_ptr(ptr) ((ptr) + ((ptr) - (opus_uint32*)(ptr)))
/** @endcond */

/** @defgroup opus_ctlvalues Pre-defined values for CTL interface
  * @see opus_genericctls, opus_encoderctls
  * @{
  */
/* Values for the various encoder CTLs */
#define OPUS_AUTO                           -1000 /**<Auto/default setting @hideinitializer*/
#define OPUS_BITRATE_MAX                       -1 /**<Maximum bitrate @hideinitializer*/

/** Best for most VoIP/videoconference applications where listening quality and intelligibility matter most
 * @hideinitializer */
#define OPUS_APPLICATION_VOIP                2048
/** Best for broadcast/high-fidelity application where the decoded audio should be as close as possible to the input
 * @hideinitializer */
#define OPUS_APPLICATION_AUDIO               2049
/** Only use when lowest-achievable latency is what matters most. Voice-optimized modes cannot be used.
 * @hideinitializer */
#define OPUS_APPLICATION_RESTRICTED_LOWDELAY 2051

#define OPUS_SIGNAL_VOICE                    3001 /**< Signal being encoded is voice */
#define OPUS_SIGNAL_MUSIC                    3002 /**< Signal being encoded is music */
#define OPUS_BANDWIDTH_NARROWBAND            1101 /**< 4 kHz bandpass @hideinitializer*/
#define OPUS_BANDWIDTH_MEDIUMBAND            1102 /**< 6 kHz bandpass @hideinitializer*/
#define OPUS_BANDWIDTH_WIDEBAND              1103 /**< 8 kHz bandpass @hideinitializer*/
#define OPUS_BANDWIDTH_SUPERWIDEBAND         1104 /**<12 kHz bandpass @hideinitializer*/
#define OPUS_BANDWIDTH_FULLBAND              1105 /**<20 kHz bandpass @hideinitializer*/

/**@}*/


/** @defgroup opus_encoderctls Encoder related CTLs
  *
  * These are convenience macros for use with the \c opus_encode_ctl
  * interface. They are used to generate the appropriate series of
  * arguments for that call, passing the correct type, size and so
  * on as expected for each particular request.
  *
  * Some usage examples:
  *
  * @code
  * int ret;
  * ret = opus_encoder_ctl(enc_ctx, OPUS_SET_BANDWIDTH(OPUS_AUTO));
  * if (ret != OPUS_OK) return ret;
  *
  * opus_int32 rate;
  * opus_encoder_ctl(enc_ctx, OPUS_GET_BANDWIDTH(&rate));
  *
  * opus_encoder_ctl(enc_ctx, OPUS_RESET_STATE);
  * @endcode
  *
  * @see opus_genericctls, opus_encoder
  * @{
  */

/** Configures the encoder's computational complexity.
  * The supported range is 0-10 inclusive with 10 representing the highest complexity.
  * @see OPUS_GET_COMPLEXITY
  * @param[in] x <tt>opus_int32</tt>: Allowed values: 0-10, inclusive.
  *
  * @hideinitializer */
#define OPUS_SET_COMPLEXITY(x) OPUS_SET_COMPLEXITY_REQUEST, __opus_check_int(x)
/** Gets the encoder's complexity configuration.
  * @see OPUS_SET_COMPLEXITY
  * @param[out] x <tt>opus_int32 *</tt>: Returns a value in the range 0-10,
  *                                      inclusive.
  * @hideinitializer */
#define OPUS_GET_COMPLEXITY(x) OPUS_GET_COMPLEXITY_REQUEST, __opus_check_int_ptr(x)

/** Configures the bitrate in the encoder.
  * Rates from 500 to 512000 bits per second are meaningful, as well as the
  * special values #OPUS_AUTO and #OPUS_BITRATE_MAX.
  * The value #OPUS_BITRATE_MAX can be used to cause the codec to use as much
  * rate as it can, which is useful for controlling the rate by adjusting the
  * output buffer size.
  * @see OPUS_GET_BITRATE
  * @param[in] x <tt>opus_int32</tt>: Bitrate in bits per second. The default
  *                                   is determined based on the number of
  *                                   channels and the input sampling rate.
  * @hideinitializer */
#define OPUS_SET_BITRATE(x) OPUS_SET_BITRATE_REQUEST, __opus_check_int(x)
/** Gets the encoder's bitrate configuration.
  * @see OPUS_SET_BITRATE
  * @param[out] x <tt>opus_int32 *</tt>: Returns the bitrate in bits per second.
  *                                      The default is determined based on the
  *                                      number of channels and the input
  *                                      sampling rate.
  * @hideinitializer */
#define OPUS_GET_BITRATE(x) OPUS_GET_BITRATE_REQUEST, __opus_check_int_ptr(x)

/** Enables or disables variable bitrate (VBR) in the encoder.
  * The configured bitrate may not be met exactly because frames must
  * be an integer number of bytes in length.
  * @warning Only the MDCT mode of Opus can provide hard CBR behavior.
  * @see OPUS_GET_VBR
  * @see OPUS_SET_VBR_CONSTRAINT
  * @param[in] x <tt>opus_int32</tt>: Allowed values:
  * <dl>
  * <dt>0</dt><dd>Hard CBR. For LPC/hybrid modes at very low bit-rate, this can
  *               cause noticeable quality degradation.</dd>
  * <dt>1</dt><dd>VBR (default). The exact type of VBR is controlled by
  *               #OPUS_SET_VBR_CONSTRAINT.</dd>
  * </dl>
  * @hideinitializer */
#define OPUS_SET_VBR(x) OPUS_SET_VBR_REQUEST, __opus_check_int(x)
/** Determine if variable bitrate (VBR) is enabled in the encoder.
  * @see OPUS_SET_VBR
  * @see OPUS_GET_VBR_CONSTRAINT
  * @param[out] x <tt>opus_int32 *</tt>: Returns one of the following values:
  * <dl>
  * <dt>0</dt><dd>Hard CBR.</dd>
  * <dt>1</dt><dd>VBR (default). The exact type of VBR may be retrieved via
  *               #OPUS_GET_VBR_CONSTRAINT.</dd>
  * </dl>
  * @hideinitializer */
#define OPUS_GET_VBR(x) OPUS_GET_VBR_REQUEST, __opus_check_int_ptr(x)

/** Enables or disables constrained VBR in the encoder.
  * This setting is ignored when the encoder is in CBR mode.
  * @warning Only the MDCT mode of Opus currently heeds the constraint.
  *  Speech mode ignores it completely, hybrid mode may fail to obey it
  *  if the LPC layer uses more bitrate than the constraint would have
  *  permitted.
  * @see OPUS_GET_VBR_CONSTRAINT
  * @see OPUS_SET_VBR
  * @param[in] x <tt>opus_int32</tt>: Allowed values:
  * <dl>
  * <dt>0</dt><dd>Unconstrained VBR.</dd>
  * <dt>1</dt><dd>Constrained VBR (default). This creates a maximum of one
  *               frame of buffering delay assuming a transport with a
  *               serialization speed of the nominal bitrate.</dd>
  * </dl>
  * @hideinitializer */
#define OPUS_SET_VBR_CONSTRAINT(x) OPUS_SET_VBR_CONSTRAINT_REQUEST, __opus_check_int(x)
/** Determine if constrained VBR is enabled in the encoder.
  * @see OPUS_SET_VBR_CONSTRAINT
  * @see OPUS_GET_VBR
  * @param[out] x <tt>opus_int32 *</tt>: Returns one of the following values:
  * <dl>
  * <dt>0</dt><dd>Unconstrained VBR.</dd>
  * <dt>1</dt><dd>Constrained VBR (default).</dd>
  * </dl>
  * @hideinitializer */
#define OPUS_GET_VBR_CONSTRAINT(x) OPUS_GET_VBR_CONSTRAINT_REQUEST, __opus_check_int_ptr(x)

/** Configures mono/stereo forcing in the encoder.
  * This can force the encoder to produce packets encoded as either mono or
  * stereo, regardless of the format of the input audio. This is useful when
  * the caller knows that the input signal is currently a mono source embedded
  * in a stereo stream.
  * @see OPUS_GET_FORCE_CHANNELS
  * @param[in] x <tt>opus_int32</tt>: Allowed values:
  * <dl>
  * <dt>#OPUS_AUTO</dt><dd>Not forced (default)</dd>
  * <dt>1</dt>         <dd>Forced mono</dd>
  * <dt>2</dt>         <dd>Forced stereo</dd>
  * </dl>
  * @hideinitializer */
#define OPUS_SET_FORCE_CHANNELS(x) OPUS_SET_FORCE_CHANNELS_REQUEST, __opus_check_int(x)
/** Gets the encoder's forced channel configuration.
  * @see OPUS_SET_FORCE_CHANNELS
  * @param[out] x <tt>opus_int32 *</tt>:
  * <dl>
  * <dt>#OPUS_AUTO</dt><dd>Not forced (default)</dd>
  * <dt>1</dt>         <dd>Forced mono</dd>
  * <dt>2</dt>         <dd>Forced stereo</dd>
  * </dl>
  * @hideinitializer */
#define OPUS_GET_FORCE_CHANNELS(x) OPUS_GET_FORCE_CHANNELS_REQUEST, __opus_check_int_ptr(x)

/** Configures the maximum bandpass that the encoder will select automatically.
  * Applications should normally use this instead of #OPUS_SET_BANDWIDTH
  * (leaving that set to the default, #OPUS_AUTO). This allows the
  * application to set an upper bound based on the type of input it is
  * providing, but still gives the encoder the freedom to reduce the bandpass
  * when the bitrate becomes too low, for better overall quality.
  * @see OPUS_GET_MAX_BANDWIDTH
  * @param[in] x <tt>opus_int32</tt>: Allowed values:
  * <dl>
  * <dt>OPUS_BANDWIDTH_NARROWBAND</dt>    <dd>4 kHz passband</dd>
  * <dt>OPUS_BANDWIDTH_MEDIUMBAND</dt>    <dd>6 kHz passband</dd>
  * <dt>OPUS_BANDWIDTH_WIDEBAND</dt>      <dd>8 kHz passband</dd>
  * <dt>OPUS_BANDWIDTH_SUPERWIDEBAND</dt><dd>12 kHz passband</dd>
  * <dt>OPUS_BANDWIDTH_FULLBAND</dt>     <dd>20 kHz passband (default)</dd>
  * </dl>
  * @hideinitializer */
#define OPUS_SET_MAX_BANDWIDTH(x) OPUS_SET_MAX_BANDWIDTH_REQUEST, __opus_check_int(x)

/** Gets the encoder's configured maximum allowed bandpass.
  * @see OPUS_SET_MAX_BANDWIDTH
  * @param[out] x <tt>opus_int32 *</tt>: Allowed values:
  * <dl>
  * <dt>#OPUS_BANDWIDTH_NARROWBAND</dt>    <dd>4 kHz passband</dd>
  * <dt>#OPUS_BANDWIDTH_MEDIUMBAND</dt>    <dd>6 kHz passband</dd>
  * <dt>#OPUS_BANDWIDTH_WIDEBAND</dt>      <dd>8 kHz passband</dd>
  * <dt>#OPUS_BANDWIDTH_SUPERWIDEBAND</dt><dd>12 kHz passband</dd>
  * <dt>#OPUS_BANDWIDTH_FULLBAND</dt>     <dd>20 kHz passband (default)</dd>
  * </dl>
  * @hideinitializer */
#define OPUS_GET_MAX_BANDWIDTH(x) OPUS_GET_MAX_BANDWIDTH_REQUEST, __opus_check_int_ptr(x)

/** Sets the encoder's bandpass to a specific value.
  * This prevents the encoder from automatically selecting the bandpass based
  * on the available bitrate. If an application knows the bandpass of the input
  * audio it is providing, it should normally use #OPUS_SET_MAX_BANDWIDTH
  * instead, which still gives the encoder the freedom to reduce the bandpass
  * when the bitrate becomes too low, for better overall quality.
  * @see OPUS_GET_BANDWIDTH
  * @param[in] x <tt>opus_int32</tt>: Allowed values:
  * <dl>
  * <dt>#OPUS_AUTO</dt>                    <dd>(default)</dd>
  * <dt>#OPUS_BANDWIDTH_NARROWBAND</dt>    <dd>4 kHz passband</dd>
  * <dt>#OPUS_BANDWIDTH_MEDIUMBAND</dt>    <dd>6 kHz passband</dd>
  * <dt>#OPUS_BANDWIDTH_WIDEBAND</dt>      <dd>8 kHz passband</dd>
  * <dt>#OPUS_BANDWIDTH_SUPERWIDEBAND</dt><dd>12 kHz passband</dd>
  * <dt>#OPUS_BANDWIDTH_FULLBAND</dt>     <dd>20 kHz passband</dd>
  * </dl>
  * @hideinitializer */
#define OPUS_SET_BANDWIDTH(x) OPUS_SET_BANDWIDTH_REQUEST, __opus_check_int(x)

/** Configures the type of signal being encoded.
  * This is a hint which helps the encoder's mode selection.
  * @see OPUS_GET_SIGNAL
  * @param[in] x <tt>opus_int32</tt>: Allowed values:
  * <dl>
  * <dt>#OPUS_AUTO</dt>        <dd>(default)</dd>
  * <dt>#OPUS_SIGNAL_VOICE</dt><dd>Bias thresholds towards choosing LPC or Hybrid modes.</dd>
  * <dt>#OPUS_SIGNAL_MUSIC</dt><dd>Bias thresholds towards choosing MDCT modes.</dd>
  * </dl>
  * @hideinitializer */
#define OPUS_SET_SIGNAL(x) OPUS_SET_SIGNAL_REQUEST, __opus_check_int(x)
/** Gets the encoder's configured signal type.
  * @see OPUS_SET_SIGNAL
  * @param[out] x <tt>opus_int32 *</tt>: Returns one of the following values:
  * <dl>
  * <dt>#OPUS_AUTO</dt>        <dd>(default)</dd>
  * <dt>#OPUS_SIGNAL_VOICE</dt><dd>Bias thresholds towards choosing LPC or Hybrid modes.</dd>
  * <dt>#OPUS_SIGNAL_MUSIC</dt><dd>Bias thresholds towards choosing MDCT modes.</dd>
  * </dl>
  * @hideinitializer */
#define OPUS_GET_SIGNAL(x) OPUS_GET_SIGNAL_REQUEST, __opus_check_int_ptr(x)


/** Configures the encoder's intended application.
  * The initial value is a mandatory argument to the encoder_create function.
  * @see OPUS_GET_APPLICATION
  * @param[in] x <tt>opus_int32</tt>: Returns one of the following values:
  * <dl>
  * <dt>#OPUS_APPLICATION_VOIP</dt>
  * <dd>Process signal for improved speech intelligibility.</dd>
  * <dt>#OPUS_APPLICATION_AUDIO</dt>
  * <dd>Favor faithfulness to the original input.</dd>
  * <dt>#OPUS_APPLICATION_RESTRICTED_LOWDELAY</dt>
  * <dd>Configure the minimum possible coding delay by disabling certain modes
  * of operation.</dd>
  * </dl>
  * @hideinitializer */
#define OPUS_SET_APPLICATION(x) OPUS_SET_APPLICATION_REQUEST, __opus_check_int(x)
/** Gets the encoder's configured application.
  * @see OPUS_SET_APPLICATION
  * @param[out] x <tt>opus_int32 *</tt>: Returns one of the following values:
  * <dl>
  * <dt>#OPUS_APPLICATION_VOIP</dt>
  * <dd>Process signal for improved speech intelligibility.</dd>
  * <dt>#OPUS_APPLICATION_AUDIO</dt>
  * <dd>Favor faithfulness to the original input.</dd>
  * <dt>#OPUS_APPLICATION_RESTRICTED_LOWDELAY</dt>
  * <dd>Configure the minimum possible coding delay by disabling certain modes
  * of operation.</dd>
  * </dl>
  * @hideinitializer */
#define OPUS_GET_APPLICATION(x) OPUS_GET_APPLICATION_REQUEST, __opus_check_int_ptr(x)

/** Gets the sampling rate the encoder or decoder was initialized with.
  * This simply returns the <code>Fs</code> value passed to opus_encoder_init()
  * or opus_decoder_init().
  * @param[out] x <tt>opus_int32 *</tt>: Sampling rate of encoder or decoder.
  * @hideinitializer
  */
#define OPUS_GET_SAMPLE_RATE(x) OPUS_GET_SAMPLE_RATE_REQUEST, __opus_check_int_ptr(x)

/** Gets the total samples of delay added by the entire codec.
  * This can be queried by the encoder and then the provided number of samples can be
  * skipped on from the start of the decoder's output to provide time aligned input
  * and output. From the perspective of a decoding application the real data begins this many
  * samples late.
  *
  * The decoder contribution to this delay is identical for all decoders, but the
  * encoder portion of the delay may vary from implementation to implementation,
  * version to version, or even depend on the encoder's initial configuration.
  * Applications needing delay compensation should call this CTL rather than
  * hard-coding a value.
  * @param[out] x <tt>opus_int32 *</tt>:   Number of lookahead samples
  * @hideinitializer */
#define OPUS_GET_LOOKAHEAD(x) OPUS_GET_LOOKAHEAD_REQUEST, __opus_check_int_ptr(x)

/** Configures the encoder's use of inband forward error correction (FEC).
  * @note This is only applicable to the LPC layer
  * @see OPUS_GET_INBAND_FEC
  * @param[in] x <tt>opus_int32</tt>: Allowed values:
  * <dl>
  * <dt>0</dt><dd>Disable inband FEC (default).</dd>
  * <dt>1</dt><dd>Enable inband FEC.</dd>
  * </dl>
  * @hideinitializer */
#define OPUS_SET_INBAND_FEC(x) OPUS_SET_INBAND_FEC_REQUEST, __opus_check_int(x)
/** Gets encoder's configured use of inband forward error correction.
  * @see OPUS_SET_INBAND_FEC
  * @param[out] x <tt>opus_int32 *</tt>: Returns one of the following values:
  * <dl>
  * <dt>0</dt><dd>Inband FEC disabled (default).</dd>
  * <dt>1</dt><dd>Inband FEC enabled.</dd>
  * </dl>
  * @hideinitializer */
#define OPUS_GET_INBAND_FEC(x) OPUS_GET_INBAND_FEC_REQUEST, __opus_check_int_ptr(x)

/** Configures the encoder's expected packet loss percentage.
  * Higher values with trigger progressively more loss resistant behavior in the encoder
  * at the expense of quality at a given bitrate in the lossless case, but greater quality
  * under loss.
  * @see OPUS_GET_PACKET_LOSS_PERC
  * @param[in] x <tt>opus_int32</tt>:   Loss percentage in the range 0-100, inclusive (default: 0).
  * @hideinitializer */
#define OPUS_SET_PACKET_LOSS_PERC(x) OPUS_SET_PACKET_LOSS_PERC_REQUEST, __opus_check_int(x)
/** Gets the encoder's configured packet loss percentage.
  * @see OPUS_SET_PACKET_LOSS_PERC
  * @param[out] x <tt>opus_int32 *</tt>: Returns the configured loss percentage
  *                                      in the range 0-100, inclusive (default: 0).
  * @hideinitializer */
#define OPUS_GET_PACKET_LOSS_PERC(x) OPUS_GET_PACKET_LOSS_PERC_REQUEST, __opus_check_int_ptr(x)

/** Configures the encoder's use of discontinuous transmission (DTX).
  * @note This is only applicable to the LPC layer
  * @see OPUS_GET_DTX
  * @param[in] x <tt>opus_int32</tt>: Allowed values:
  * <dl>
  * <dt>0</dt><dd>Disable DTX (default).</dd>
  * <dt>1</dt><dd>Enabled DTX.</dd>
  * </dl>
  * @hideinitializer */
#define OPUS_SET_DTX(x) OPUS_SET_DTX_REQUEST, __opus_check_int(x)
/** Gets encoder's configured use of discontinuous transmission.
  * @see OPUS_SET_DTX
  * @param[out] x <tt>opus_int32 *</tt>: Returns one of the following values:
  * <dl>
  * <dt>0</dt><dd>DTX disabled (default).</dd>
  * <dt>1</dt><dd>DTX enabled.</dd>
  * </dl>
  * @hideinitializer */
#define OPUS_GET_DTX(x) OPUS_GET_DTX_REQUEST, __opus_check_int_ptr(x)
/**@}*/

/** @defgroup opus_genericctls Generic CTLs
  *
  * These macros are used with the \c opus_decoder_ctl and
  * \c opus_encoder_ctl calls to generate a particular
  * request.
  *
  * When called on an \c OpusDecoder they apply to that
  * particular decoder instance. When called on an
  * \c OpusEncoder they apply to the corresponding setting
  * on that encoder instance, if present.
  *
  * Some usage examples:
  *
  * @code
  * int ret;
  * opus_int32 pitch;
  * ret = opus_decoder_ctl(dec_ctx, OPUS_GET_PITCH(&pitch));
  * if (ret == OPUS_OK) return ret;
  *
  * opus_encoder_ctl(enc_ctx, OPUS_RESET_STATE);
  * opus_decoder_ctl(dec_ctx, OPUS_RESET_STATE);
  *
  * opus_int32 enc_bw, dec_bw;
  * opus_encoder_ctl(enc_ctx, OPUS_GET_BANDWIDTH(&enc_bw));
  * opus_decoder_ctl(dec_ctx, OPUS_GET_BANDWIDTH(&dec_bw));
  * if (enc_bw != dec_bw) {
  *   printf("packet bandwidth mismatch!\n");
  * }
  * @endcode
  *
  * @see opus_encoder, opus_decoder_ctl, opus_encoder_ctl, opus_decoderctls, opus_encoderctls
  * @{
  */

/** Resets the codec state to be equivalent to a freshly initialized state.
  * This should be called when switching streams in order to prevent
  * the back to back decoding from giving different results from
  * one at a time decoding.
  * @hideinitializer */
#define OPUS_RESET_STATE 4028

/** Gets the final state of the codec's entropy coder.
  * This is used for testing purposes,
  * The encoder and decoder state should be identical after coding a payload
  * (assuming no data corruption or software bugs)
  *
  * @param[out] x <tt>opus_uint32 *</tt>: Entropy coder state
  *
  * @hideinitializer */
#define OPUS_GET_FINAL_RANGE(x) OPUS_GET_FINAL_RANGE_REQUEST, __opus_check_uint_ptr(x)

/** Gets the pitch of the last decoded frame, if available.
  * This can be used for any post-processing algorithm requiring the use of pitch,
  * e.g. time stretching/shortening. If the last frame was not voiced, or if the
  * pitch was not coded in the frame, then zero is returned.
  *
  * This CTL is only implemented for decoder instances.
  *
  * @param[out] x <tt>opus_int32 *</tt>: pitch period at 48 kHz (or 0 if not available)
  *
  * @hideinitializer */
#define OPUS_GET_PITCH(x) OPUS_GET_PITCH_REQUEST, __opus_check_int_ptr(x)

/** Gets the encoder's configured bandpass or the decoder's last bandpass.
  * @see OPUS_SET_BANDWIDTH
  * @param[out] x <tt>opus_int32 *</tt>: Returns one of the following values:
  * <dl>
  * <dt>#OPUS_AUTO</dt>                    <dd>(default)</dd>
  * <dt>#OPUS_BANDWIDTH_NARROWBAND</dt>    <dd>4 kHz passband</dd>
  * <dt>#OPUS_BANDWIDTH_MEDIUMBAND</dt>    <dd>6 kHz passband</dd>
  * <dt>#OPUS_BANDWIDTH_WIDEBAND</dt>      <dd>8 kHz passband</dd>
  * <dt>#OPUS_BANDWIDTH_SUPERWIDEBAND</dt><dd>12 kHz passband</dd>
  * <dt>#OPUS_BANDWIDTH_FULLBAND</dt>     <dd>20 kHz passband</dd>
  * </dl>
  * @hideinitializer */
#define OPUS_GET_BANDWIDTH(x) OPUS_GET_BANDWIDTH_REQUEST, __opus_check_int_ptr(x)

/** Configures the depth of signal being encoded.
  * This is a hint which helps the encoder identify silence and near-silence.
  * @see OPUS_GET_LSB_DEPTH
  * @param[in] x <tt>opus_int32</tt>: Input precision in bits, between 8 and 24
  *                                   (default: 24).
  * @hideinitializer */
#define OPUS_SET_LSB_DEPTH(x) OPUS_SET_LSB_DEPTH_REQUEST, __opus_check_int(x)
/** Gets the encoder's configured signal depth.
  * @see OPUS_SET_LSB_DEPTH
  * @param[out] x <tt>opus_int32 *</tt>: Input precision in bits, between 8 and
  *                                      24 (default: 24).
  * @hideinitializer */
#define OPUS_GET_LSB_DEPTH(x) OPUS_GET_LSB_DEPTH_REQUEST, __opus_check_int_ptr(x)
/**@}*/

/** @defgroup opus_decoderctls Decoder related CTLs
  * @see opus_genericctls, opus_encoderctls, opus_decoder
  * @{
  */

/** Configures decoder gain adjustment.
  * Scales the decoded output by a factor specified in Q8 dB units.
  * This has a maximum range of -32768 to 32767 inclusive, and returns
  * OPUS_BAD_ARG otherwise. The default is zero indicating no adjustment.
  * This setting survives decoder reset.
  *
  * gain = pow(10, x/(20.0*256))
  *
  * @param[in] x <tt>opus_int32</tt>:   Amount to scale PCM signal by in Q8 dB units.
  * @hideinitializer */
#define OPUS_SET_GAIN(x) OPUS_SET_GAIN_REQUEST, __opus_check_int(x)
/** Gets the decoder's configured gain adjustment. @see OPUS_SET_GAIN
  *
  * @param[out] x <tt>opus_int32 *</tt>: Amount to scale PCM signal by in Q8 dB units.
  * @hideinitializer */
#define OPUS_GET_GAIN(x) OPUS_GET_GAIN_REQUEST, __opus_check_int_ptr(x)

/**@}*/

/** @defgroup opus_libinfo Opus library information functions
  * @{
  */

/** Converts an opus error code into a human readable std::string.
  *
  * @param[in] error <tt>int</tt>: Error number
  * @returns Error std::string
  */
/*OPUS_EXPORT const char *opus_strerror(int error);*/

/** Gets the libopus version std::string.
  *
  * @returns Version std::string
  */
/*OPUS_EXPORT const char *opus_get_version_std::string(void);*/
/**@}*/

#ifdef __cplusplus
}
#endif

#endif /* OPUS_DEFINES_H */
/* Copyright (c) 2010-2011 Xiph.Org Foundation, Skype Limited
   Written by Jean-Marc Valin and Koen Vos */
/*
   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

   - Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.

   - Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER
   OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
   LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/**
 * @file opus.h
 * @brief Opus reference implementation API
 */

#ifndef OPUS_H
#define OPUS_H

/*#include "opus_types.h"*/
/*#include "opus_defines.h"*/

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @mainpage Opus
 *
 * The Opus codec is designed for interactive speech and audio transmission over the Internet.
 * It is designed by the IETF Codec Working Group and incorporates technology from
 * Skype's SILK codec and Xiph.Org's CELT codec.
 *
 * The Opus codec is designed to handle a wide range of interactive audio applications,
 * including Voice over IP, videoconferencing, in-game chat, and even remote live music
 * performances. It can scale from low bit-rate narrowband speech to very high quality
 * stereo music. Its main features are:

 * @li Sampling rates from 8 to 48 kHz
 * @li Bit-rates from 6 kb/s to 510 kb/s
 * @li Support for both constant bit-rate (CBR) and variable bit-rate (VBR)
 * @li Audio bandwidth from narrowband to full-band
 * @li Support for speech and music
 * @li Support for mono and stereo
 * @li Support for multichannel (up to 255 channels)
 * @li Frame sizes from 2.5 ms to 60 ms
 * @li Good loss robustness and packet loss concealment (PLC)
 * @li Floating point and fixed-point implementation
 *
 * Documentation sections:
 * @li @ref opus_encoder
 * @li @ref opus_decoder
 * @li @ref opus_repacketizer
 * @li @ref opus_libinfo
 * @li @ref opus_custom
 */

/** @defgroup opus_encoder Opus Encoder
  * @{
  *
  * @brief This page describes the process and functions used to encode Opus.
  *
  * Since Opus is a stateful codec, the encoding process starts with creating an encoder
  * state. This can be done with:
  *
  * @code
  * int          error;
  * OpusEncoder *enc;
  * enc = opus_encoder_create(Fs, channels, application, &error);
  * @endcode
  *
  * From this point, @c enc can be used for encoding an audio stream. An encoder state
  * @b must @b not be used for more than one stream at the same time. Similarly, the encoder
  * state @b must @b not be re-initialized for each frame.
  *
  * While opus_encoder_create() allocates memory for the state, it's also possible
  * to initialize pre-allocated memory:
  *
  * @code
  * int          size;
  * int          error;
  * OpusEncoder *enc;
  * size = opus_encoder_get_size(channels);
  * enc = malloc(size);
  * error = opus_encoder_init(enc, Fs, channels, application);
  * @endcode
  *
  * where opus_encoder_get_size() returns the required size for the encoder state. Note that
  * future versions of this code may change the size, so no assuptions should be made about it.
  *
  * The encoder state is always continuous in memory and only a shallow copy is sufficient
  * to copy it (e.g. memcpy())
  *
  * It is possible to change some of the encoder's settings using the opus_encoder_ctl()
  * interface. All these settings already default to the recommended value, so they should
  * only be changed when necessary. The most common settings one may want to change are:
  *
  * @code
  * opus_encoder_ctl(enc, OPUS_SET_BITRATE(bitrate));
  * opus_encoder_ctl(enc, OPUS_SET_COMPLEXITY(complexity));
  * opus_encoder_ctl(enc, OPUS_SET_SIGNAL(signal_type));
  * @endcode
  *
  * where
  *
  * @arg bitrate is in bits per second (b/s)
  * @arg complexity is a value from 1 to 10, where 1 is the lowest complexity and 10 is the highest
  * @arg signal_type is either OPUS_AUTO (default), OPUS_SIGNAL_VOICE, or OPUS_SIGNAL_MUSIC
  *
  * See @ref opus_encoderctls and @ref opus_genericctls for a complete list of parameters that can be set or queried. Most parameters can be set or changed at any time during a stream.
  *
  * To encode a frame, opus_encode() or opus_encode_float() must be called with exactly one frame (2.5, 5, 10, 20, 40 or 60 ms) of audio data:
  * @code
  * len = opus_encode(enc, audio_frame, frame_size, packet, max_packet);
  * @endcode
  *
  * where
  * <ul>
  * <li>audio_frame is the audio data in opus_int16 (or float for opus_encode_float())</li>
  * <li>frame_size is the duration of the frame in samples (per channel)</li>
  * <li>packet is the byte array to which the compressed data is written</li>
  * <li>max_packet is the maximum number of bytes that can be written in the packet (4000 bytes is recommended)</li>
  * </ul>
  *
  * opus_encode() and opus_encode_frame() return the number of bytes actually written to the packet.
  * The return value <b>can be negative</b>, which indicates that an error has occurred. If the return value
  * is 1 byte, then the packet does not need to be transmitted (DTX).
  *
  * Once the encoder state if no longer needed, it can be destroyed with
  *
  * @code
  * opus_encoder_destroy(enc);
  * @endcode
  *
  * If the encoder was created with opus_encoder_init() rather than opus_encoder_create(),
  * then no action is required aside from potentially freeing the memory that was manually
  * allocated for it (calling free(enc) for the example above)
  *
  */

/** Opus encoder state.
  * This contains the complete state of an Opus encoder.
  * It is position independent and can be freely copied.
  * @see opus_encoder_create,opus_encoder_init
  */
typedef struct OpusEncoder OpusEncoder;

/*OPUS_EXPORT OPUS_WARN_UNUSED_RESULT int opus_encoder_get_size(int channels);*/

/**
 */

/** Allocates and initializes an encoder state.
 * There are three coding modes:
 *
 * @ref OPUS_APPLICATION_VOIP gives best quality at a given bitrate for voice
 *    signals. It enhances the  input signal by high-pass filtering and
 *    emphasizing formants and harmonics. Optionally  it includes in-band
 *    forward error correction to protect against packet loss. Use this
 *    mode for typical VoIP applications. Because of the enhancement,
 *    even at high bitrates the output may sound different from the input.
 *
 * @ref OPUS_APPLICATION_AUDIO gives best quality at a given bitrate for most
 *    non-voice signals like music. Use this mode for music and mixed
 *    (music/voice) content, broadcast, and applications requiring less
 *    than 15 ms of coding delay.
 *
 * @ref OPUS_APPLICATION_RESTRICTED_LOWDELAY configures low-delay mode that
 *    disables the speech-optimized mode in exchange for slightly reduced delay.
 *    This mode can only be set on an newly initialized or freshly reset encoder
 *    because it changes the codec delay.
 *
 * This is useful when the caller knows that the speech-optimized modes will not be needed (use with caution).
 * @param [in] Fs <tt>opus_int32</tt>: Sampling rate of input signal (Hz)
 * @param [in] channels <tt>int</tt>: Number of channels (1/2) in input signal
 * @param [in] application <tt>int</tt>: Coding mode (@ref OPUS_APPLICATION_VOIP/@ref OPUS_APPLICATION_AUDIO/@ref OPUS_APPLICATION_RESTRICTED_LOWDELAY)
 * @param [out] error <tt>int*</tt>: @ref opus_errorcodes
 * @note Regardless of the sampling rate and number channels selected, the Opus encoder
 * can switch to a lower audio audio bandwidth or number of channels if the bitrate
 * selected is too low. This also means that it is safe to always use 48 kHz stereo input
 * and let the encoder optimize the encoding.
 */
/*OPUS_EXPORT OPUS_WARN_UNUSED_RESULT OpusEncoder *opus_encoder_create(
    opus_int32 Fs,
    int channels,
    int application,
    int *error
);*/

/** Initializes a previously allocated encoder state
  * The memory pointed to by st must be the size returned by opus_encoder_get_size.
  * This is intended for applications which use their own allocator instead of malloc.
  * @see opus_encoder_create(),opus_encoder_get_size()
  * To reset a previously initialized state use the OPUS_RESET_STATE CTL.
  * @param [in] st <tt>OpusEncoder*</tt>: Encoder state
  * @param [in] Fs <tt>opus_int32</tt>: Sampling rate of input signal (Hz)
  * @param [in] channels <tt>int</tt>: Number of channels (1/2) in input signal
  * @param [in] application <tt>int</tt>: Coding mode (OPUS_APPLICATION_VOIP/OPUS_APPLICATION_AUDIO/OPUS_APPLICATION_RESTRICTED_LOWDELAY)
  * @retval OPUS_OK Success or @ref opus_errorcodes
  */
/*OPUS_EXPORT int opus_encoder_init(
    OpusEncoder *st,
    opus_int32 Fs,
    int channels,
    int application
) OPUS_ARG_NONNULL(1);*/

/** Encodes an Opus frame.
  * The passed frame_size must an opus frame size for the encoder's sampling rate.
  * For example, at 48kHz the permitted values are 120, 240, 480, 960, 1920, and 2880.
  * Passing in a duration of less than 10ms (480 samples at 48kHz) will
  * prevent the encoder from using the LPC or hybrid modes.
  * @param [in] st <tt>OpusEncoder*</tt>: Encoder state
  * @param [in] pcm <tt>opus_int16*</tt>: Input signal (interleaved if 2 channels). length is frame_size*channels*sizeof(opus_int16)
  * @param [in] frame_size <tt>int</tt>: Number of samples per frame of input signal
  * @param [out] data <tt>char*</tt>: Output payload (at least max_data_bytes long)
  * @param [in] max_data_bytes <tt>opus_int32</tt>: Allocated memory for payload; don't use for controlling bitrate
  * @returns length of the data payload (in bytes) or @ref opus_errorcodes
  */
/*OPUS_EXPORT OPUS_WARN_UNUSED_RESULT opus_int32 opus_encode(
    OpusEncoder *st,
    const opus_int16 *pcm,
    int frame_size,
    unsigned char *data,
    opus_int32 max_data_bytes
) OPUS_ARG_NONNULL(1) OPUS_ARG_NONNULL(2) OPUS_ARG_NONNULL(4);*/

/** Encodes an Opus frame from floating point input.
  * The passed frame_size must an opus frame size for the encoder's sampling rate.
  * For example, at 48kHz the permitted values are 120, 240, 480, 960, 1920, and 2880.
  * Passing in a duration of less than 10ms (480 samples at 48kHz) will
  * prevent the encoder from using the LPC or hybrid modes.
  * @param [in] st <tt>OpusEncoder*</tt>: Encoder state
  * @param [in] pcm <tt>float*</tt>: Input in float format (interleaved if 2 channels), with a normal range of +/-1.0.
  *          Samples with a range beyond +/-1.0 are supported but will
  *          be clipped by decoders using the integer API and should
  *          only be used if it is known that the far end supports
  *          extended dynamic range.
  *          length is frame_size*channels*sizeof(float)
  * @param [in] frame_size <tt>int</tt>: Number of samples per frame of input signal
  * @param [out] data <tt>char*</tt>: Output payload (at least max_data_bytes long)
  * @param [in] max_data_bytes <tt>opus_int32</tt>: Allocated memory for payload; don't use for controlling bitrate
  * @returns length of the data payload (in bytes) or @ref opus_errorcodes
  */
/*OPUS_EXPORT OPUS_WARN_UNUSED_RESULT opus_int32 opus_encode_float(
    OpusEncoder *st,
    const float *pcm,
    int frame_size,
    unsigned char *data,
    opus_int32 max_data_bytes
) OPUS_ARG_NONNULL(1) OPUS_ARG_NONNULL(2) OPUS_ARG_NONNULL(4);*/

/** Frees an OpusEncoder allocated by opus_encoder_create.
  * @param[in] st <tt>OpusEncoder*</tt>: State to be freed.
  */
/*OPUS_EXPORT void opus_encoder_destroy(OpusEncoder *st);*/

/** Perform a CTL function on an Opus encoder.
  *
  * Generally the request and subsequent arguments are generated
  * by a convenience macro.
  * @see opus_encoderctls
  */
/*OPUS_EXPORT int opus_encoder_ctl(OpusEncoder *st, int request, ...) OPUS_ARG_NONNULL(1);*/
/**@}*/

/** @defgroup opus_decoder Opus Decoder
  * @{
  *
  * @brief This page describes the process and functions used to decode Opus.
  *
  * The decoding process also starts with creating a decoder
  * state. This can be done with:
  * @code
  * int          error;
  * OpusDecoder *dec;
  * dec = opus_decoder_create(Fs, channels, &error);
  * @endcode
  * where
  * @li Fs is the sampling rate and must be 8000, 12000, 16000, 24000, or 48000
  * @li channels is the number of channels (1 or 2)
  * @li error will hold the error code in case or failure (or OPUS_OK on success)
  * @li the return value is a newly created decoder state to be used for decoding
  *
  * While opus_decoder_create() allocates memory for the state, it's also possible
  * to initialize pre-allocated memory:
  * @code
  * int          size;
  * int          error;
  * OpusDecoder *dec;
  * size = opus_decoder_get_size(channels);
  * dec = malloc(size);
  * error = opus_decoder_init(dec, Fs, channels);
  * @endcode
  * where opus_decoder_get_size() returns the required size for the decoder state. Note that
  * future versions of this code may change the size, so no assuptions should be made about it.
  *
  * The decoder state is always continuous in memory and only a shallow copy is sufficient
  * to copy it (e.g. memcpy())
  *
  * To decode a frame, opus_decode() or opus_decode_float() must be called with a packet of compressed audio data:
  * @code
  * frame_size = opus_decode(dec, packet, len, decoded, max_size, 0);
  * @endcode
  * where
  *
  * @li packet is the byte array containing the compressed data
  * @li len is the exact number of bytes contained in the packet
  * @li decoded is the decoded audio data in opus_int16 (or float for opus_decode_float())
  * @li max_size is the max duration of the frame in samples (per channel) that can fit into the decoded_frame array
  *
  * opus_decode() and opus_decode_float() return the number of samples (per channel) decoded from the packet.
  * If that value is negative, then an error has occured. This can occur if the packet is corrupted or if the audio
  * buffer is too small to hold the decoded audio.
  *
  * Opus is a stateful codec with overlapping blocks and as a result Opus
  * packets are not coded independently of each other. Packets must be
  * passed into the decoder serially and in the correct order for a correct
  * decode. Lost packets can be replaced with loss concealment by calling
  * the decoder with a null pointer and zero length for the missing packet.
  *
  * A single codec state may only be accessed from a single thread at
  * a time and any required locking must be performed by the caller. Separate
  * streams must be decoded with separate decoder states and can be decoded
  * in parallel unless the library was compiled with NONTHREADSAFE_PSEUDOSTACK
  * defined.
  *
  */

/** Opus decoder state.
  * This contains the complete state of an Opus decoder.
  * It is position independent and can be freely copied.
  * @see opus_decoder_create,opus_decoder_init
  */
typedef struct OpusDecoder OpusDecoder;

/** Gets the size of an OpusDecoder structure.
  * @param [in] channels <tt>int</tt>: Number of channels
  * @returns size
  */
/*OPUS_EXPORT OPUS_WARN_UNUSED_RESULT int opus_decoder_get_size(int channels);*/

/** Allocates and initializes a decoder state.
  * @param [in] Fs <tt>opus_int32</tt>: Sample rate to decode at (Hz)
  * @param [in] channels <tt>int</tt>: Number of channels (1/2) to decode
  * @param [out] error <tt>int*</tt>: OPUS_OK Success or @ref opus_errorcodes
  *
  * Internally Opus stores data at 48000 Hz, so that should be the default
  * value for Fs. However, the decoder can efficiently decode to buffers
  * at 8, 12, 16, and 24 kHz so if for some reason the caller cannot use
  * data at the full sample rate, or knows the compressed data doesn't
  * use the full frequency range, it can request decoding at a reduced
  * rate. Likewise, the decoder is capable of filling in either mono or
  * interleaved stereo pcm buffers, at the caller's request.
  */
/*OPUS_EXPORT OPUS_WARN_UNUSED_RESULT OpusDecoder *opus_decoder_create(
    opus_int32 Fs,
    int channels,
    int *error
);*/

/** Initializes a previously allocated decoder state.
  * The state must be the size returned by opus_decoder_get_size.
  * This is intended for applications which use their own allocator instead of malloc. @see opus_decoder_create,opus_decoder_get_size
  * To reset a previously initialized state use the OPUS_RESET_STATE CTL.
  * @param [in] st <tt>OpusDecoder*</tt>: Decoder state.
  * @param [in] Fs <tt>opus_int32</tt>: Sampling rate to decode to (Hz)
  * @param [in] channels <tt>int</tt>: Number of channels (1/2) to decode
  * @retval OPUS_OK Success or @ref opus_errorcodes
  */
/*OPUS_EXPORT int opus_decoder_init(
    OpusDecoder *st,
    opus_int32 Fs,
    int channels
) OPUS_ARG_NONNULL(1);*/

/** Decode an Opus frame
  * @param [in] st <tt>OpusDecoder*</tt>: Decoder state
  * @param [in] data <tt>char*</tt>: Input payload. Use a NULL pointer to indicate packet loss
  * @param [in] len <tt>opus_int32</tt>: Number of bytes in payload*
  * @param [out] pcm <tt>opus_int16*</tt>: Output signal (interleaved if 2 channels). length
  *  is frame_size*channels*sizeof(opus_int16)
  * @param [in] frame_size Number of samples per channel of available space in *pcm,
  *  if less than the maximum frame size (120ms) some frames can not be decoded
  * @param [in] decode_fec <tt>int</tt>: Flag (0/1) to request that any in-band forward error correction data be
  *  decoded. If no such data is available the frame is decoded as if it were lost.
  * @returns Number of decoded samples or @ref opus_errorcodes
  */
/*OPUS_EXPORT OPUS_WARN_UNUSED_RESULT int opus_decode(
    OpusDecoder *st,
    const unsigned char *data,
    opus_int32 len,
    opus_int16 *pcm,
    int frame_size,
    int decode_fec
) OPUS_ARG_NONNULL(1) OPUS_ARG_NONNULL(4);*/

/** Decode an opus frame with floating point output
  * @param [in] st <tt>OpusDecoder*</tt>: Decoder state
  * @param [in] data <tt>char*</tt>: Input payload. Use a NULL pointer to indicate packet loss
  * @param [in] len <tt>opus_int32</tt>: Number of bytes in payload
  * @param [out] pcm <tt>float*</tt>: Output signal (interleaved if 2 channels). length
  *  is frame_size*channels*sizeof(float)
  * @param [in] frame_size Number of samples per channel of available space in *pcm,
  *  if less than the maximum frame size (120ms) some frames can not be decoded
  * @param [in] decode_fec <tt>int</tt>: Flag (0/1) to request that any in-band forward error correction data be
  *  decoded. If no such data is available the frame is decoded as if it were lost.
  * @returns Number of decoded samples or @ref opus_errorcodes
  */
/*OPUS_EXPORT OPUS_WARN_UNUSED_RESULT int opus_decode_float(
    OpusDecoder *st,
    const unsigned char *data,
    opus_int32 len,
    float *pcm,
    int frame_size,
    int decode_fec
) OPUS_ARG_NONNULL(1) OPUS_ARG_NONNULL(4);*/

/** Perform a CTL function on an Opus decoder.
  *
  * Generally the request and subsequent arguments are generated
  * by a convenience macro.
  * @see opus_genericctls
  */
/*OPUS_EXPORT int opus_decoder_ctl(OpusDecoder *st, int request, ...) OPUS_ARG_NONNULL(1);*/

/** Frees an OpusDecoder allocated by opus_decoder_create.
  * @param[in] st <tt>OpusDecoder*</tt>: State to be freed.
  */
/*OPUS_EXPORT void opus_decoder_destroy(OpusDecoder *st);*/

/** Parse an opus packet into one or more frames.
  * Opus_decode will perform this operation internally so most applications do
  * not need to use this function.
  * This function does not copy the frames, the returned pointers are pointers into
  * the input packet.
  * @param [in] data <tt>char*</tt>: Opus packet to be parsed
  * @param [in] len <tt>opus_int32</tt>: size of data
  * @param [out] out_toc <tt>char*</tt>: TOC pointer
  * @param [out] frames <tt>char*[48]</tt> encapsulated frames
  * @param [out] size <tt>short[48]</tt> sizes of the encapsulated frames
  * @param [out] payload_offset <tt>int*</tt>: returns the position of the payload within the packet (in bytes)
  * @returns number of frames
  */
/*OPUS_EXPORT int opus_packet_parse(
   const unsigned char *data,
   opus_int32 len,
   unsigned char *out_toc,
   const unsigned char *frames[48],
   short size[48],
   int *payload_offset
) OPUS_ARG_NONNULL(1) OPUS_ARG_NONNULL(4);*/

/** Gets the bandwidth of an Opus packet.
  * @param [in] data <tt>char*</tt>: Opus packet
  * @retval OPUS_BANDWIDTH_NARROWBAND Narrowband (4kHz bandpass)
  * @retval OPUS_BANDWIDTH_MEDIUMBAND Mediumband (6kHz bandpass)
  * @retval OPUS_BANDWIDTH_WIDEBAND Wideband (8kHz bandpass)
  * @retval OPUS_BANDWIDTH_SUPERWIDEBAND Superwideband (12kHz bandpass)
  * @retval OPUS_BANDWIDTH_FULLBAND Fullband (20kHz bandpass)
  * @retval OPUS_INVALID_PACKET The compressed data passed is corrupted or of an unsupported type
  */
/*OPUS_EXPORT OPUS_WARN_UNUSED_RESULT int opus_packet_get_bandwidth(const unsigned char *data) OPUS_ARG_NONNULL(1);*/

/** Gets the number of samples per frame from an Opus packet.
  * @param [in] data <tt>char*</tt>: Opus packet
  * @param [in] Fs <tt>opus_int32</tt>: Sampling rate in Hz
  * @returns Number of samples per frame
  * @retval OPUS_INVALID_PACKET The compressed data passed is corrupted or of an unsupported type
  */
/*OPUS_EXPORT OPUS_WARN_UNUSED_RESULT int opus_packet_get_samples_per_frame(const unsigned char *data, opus_int32 Fs) OPUS_ARG_NONNULL(1);*/

/** Gets the number of channels from an Opus packet.
  * @param [in] data <tt>char*</tt>: Opus packet
  * @returns Number of channels
  * @retval OPUS_INVALID_PACKET The compressed data passed is corrupted or of an unsupported type
  */
/*OPUS_EXPORT OPUS_WARN_UNUSED_RESULT int opus_packet_get_nb_channels(const unsigned char *data) OPUS_ARG_NONNULL(1);*/

/** Gets the number of frames in an Opus packet.
  * @param [in] packet <tt>char*</tt>: Opus packet
  * @param [in] len <tt>opus_int32</tt>: Length of packet
  * @returns Number of frames
  * @retval OPUS_INVALID_PACKET The compressed data passed is corrupted or of an unsupported type
  */
/*OPUS_EXPORT OPUS_WARN_UNUSED_RESULT int opus_packet_get_nb_frames(const unsigned char packet[], opus_int32 len) OPUS_ARG_NONNULL(1);*/

/** Gets the number of samples of an Opus packet.
  * @param [in] dec <tt>OpusDecoder*</tt>: Decoder state
  * @param [in] packet <tt>char*</tt>: Opus packet
  * @param [in] len <tt>opus_int32</tt>: Length of packet
  * @returns Number of samples
  * @retval OPUS_INVALID_PACKET The compressed data passed is corrupted or of an unsupported type
  */
/*OPUS_EXPORT OPUS_WARN_UNUSED_RESULT int opus_decoder_get_nb_samples(const OpusDecoder *dec, const unsigned char packet[], opus_int32 len) OPUS_ARG_NONNULL(1) OPUS_ARG_NONNULL(2);*/
/**@}*/

/** @defgroup opus_repacketizer Repacketizer
  * @{
  *
  * The repacketizer can be used to merge multiple Opus packets into a single packet
  * or alternatively to split Opus packets that have previously been merged.
  *
  */
/*
typedef struct OpusRepacketizer OpusRepacketizer;

OPUS_EXPORT OPUS_WARN_UNUSED_RESULT int opus_repacketizer_get_size(void);

OPUS_EXPORT OpusRepacketizer *opus_repacketizer_init(OpusRepacketizer *rp) OPUS_ARG_NONNULL(1);

OPUS_EXPORT OPUS_WARN_UNUSED_RESULT OpusRepacketizer *opus_repacketizer_create(void);

OPUS_EXPORT void opus_repacketizer_destroy(OpusRepacketizer *rp);

OPUS_EXPORT int opus_repacketizer_cat(OpusRepacketizer *rp, const unsigned char *data, opus_int32 len) OPUS_ARG_NONNULL(1) OPUS_ARG_NONNULL(2);

OPUS_EXPORT OPUS_WARN_UNUSED_RESULT opus_int32 opus_repacketizer_out_range(OpusRepacketizer *rp, int begin, int end, unsigned char *data, opus_int32 maxlen) OPUS_ARG_NONNULL(1) OPUS_ARG_NONNULL(4);

OPUS_EXPORT OPUS_WARN_UNUSED_RESULT int opus_repacketizer_get_nb_frames(OpusRepacketizer *rp) OPUS_ARG_NONNULL(1);

OPUS_EXPORT OPUS_WARN_UNUSED_RESULT opus_int32 opus_repacketizer_out(OpusRepacketizer *rp, unsigned char *data, opus_int32 maxlen) OPUS_ARG_NONNULL(1);
*/
/**@}*/

#ifdef __cplusplus
}
#endif

#endif /* OPUS_H */

#ifdef __cplusplus
extern "C" {
#endif

typedef OPUS_EXPORT const char *(opus_get_version_string_t)(
    void
    );

typedef OPUS_EXPORT const char *(opus_strerror_t)(
    int error
    );

typedef OPUS_EXPORT OpusEncoder *(opus_encoder_create_t)(
    opus_int32 Fs,
    int channels,
    int application,
    int *error
    );

typedef OPUS_EXPORT int (opus_encoder_ctl_t)(
    OpusEncoder *st,
    int request,
    ...
    );

typedef OPUS_EXPORT opus_int32 (opus_encode_t)(
    OpusEncoder *st,
    const opus_int16 *pcm,
    int frame_size,
    unsigned char *data,
    opus_int32 max_data_bytes
    );

typedef OPUS_EXPORT void (opus_encoder_destroy_t)(
    OpusEncoder *st
    );

typedef OPUS_EXPORT OpusDecoder *(opus_decoder_create_t)(
    opus_int32 Fs,
    int channels,
    int *error
    );

typedef OPUS_EXPORT int (opus_decoder_ctl_t)(
    OpusDecoder *st,
    int request,
    ...
    );

typedef OPUS_EXPORT int (opus_decode_t)(
    OpusDecoder *st,
    const unsigned char *data,
    opus_int32 len,
    opus_int16 *pcm,
    int frame_size,
    int decode_fec
    );

typedef OPUS_EXPORT void (opus_decoder_destroy_t)(
    OpusDecoder *st
    );

#ifdef __cplusplus
}
#endif

#endif

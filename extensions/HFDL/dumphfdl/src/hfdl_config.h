/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

/* #undef WITH_SOAPYSDR */
/* #undef WITH_PROFILING */
/* #undef WITH_STATSD */
#define WITH_SQLITE
/* #undef WITH_FFTW3F_THREADS */
/* #undef HAVE_PTHREAD_BARRIERS */
/* #undef WITH_ZMQ */
/* #undef DATADUMPS */
#ifdef DATADUMPS
#define COSTAS_DEBUG
#define SYMSYNC_DEBUG
#define CHAN_DEBUG
#define MF_DEBUG
#define AGC_DEBUG
#define EQ_DEBUG
#define CORR_DEBUG
#define DUMP_CONST
#undef DUMP_FFT
#undef FASTDDC_DEBUG
#endif

#define LIBZMQ_VER_MAJOR_MIN 3
#define LIBZMQ_VER_MINOR_MIN 2
#define LIBZMQ_VER_PATCH_MIN 0

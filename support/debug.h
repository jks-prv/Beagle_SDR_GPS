#ifndef _DEBUG_H_
#define _DEBUG_H_

#include "types.h"
#include "kiwi.gen.h"

#define	EC_EVENT		0
#define	EC_DUMP			1
#define	EC_TRIG1		2
#define	EC_TRIG2		3
#define	EC_TRIG3		4

#define	EV_PANIC		0
#define	EV_NEXTTASK		1
#define	EV_SPILOOP		2
#define	EV_WF			3
#define	EV_SND			4
#define	EV_GPS			5
#define	EV_DPUMP		6
#define	EV_PRINTF		7
#define NEVT			8

// use when there's a crash that doesn't leave a backtrace for gdb
//#define	EVENT_DUMP_WHILE_RUNNING

//#define EV_MEAS
#ifdef EV_MEAS
	void ev(int cmd, int event, int param, const char *s, const char *s2);
#else
	#define ev(c, e, p, s, s2)
#endif

//#define EV_MEAS_PRINTF
#if defined(EV_MEAS) && defined(EV_MEAS_PRINTF)
	#define evPrintf(c, e, p, s, s2) ev(c, e, p, s, s2)
#else
	#define evPrintf(c, e, p, s, s2)
#endif

//#define EV_MEAS_GPS
#if defined(EV_MEAS) && defined(EV_MEAS_GPS)
	#define evGPS(c, e, p, s, s2) ev(c, e, p, s, s2)
#else
	#define evGPS(c, e, p, s, s2)
#endif

//#define EV_MEAS_NEXTTASK
#if defined(EV_MEAS) && defined(EV_MEAS_NEXTTASK)
	#define evNT(c, e, p, s, s2) ev(c, e, p, s, s2)
#else
	#define evNT(c, e, p, s, s2)
#endif

//#define EV_MEAS_SPI_DEV
#if defined(EV_MEAS) && (defined(EV_MEAS_SPI_DEV) || defined(SPI_PUMP_CHECK))
	#define evSpiDev(c, e, p, s, s2) ev(c, e, p, s, s2)
#else
	#define evSpiDev(c, e, p, s, s2)
#endif

//#define EV_MEAS_SPI
#if defined(EV_MEAS) && (defined(EV_MEAS_SPI) || defined(SPI_PUMP_CHECK))
	#define evSpi(c, e, p, s, s2) ev(c, e, p, s, s2)
#else
	#define evSpi(c, e, p, s, s2)
#endif

//#define EV_MEAS_WF
#if defined(EV_MEAS) && defined(EV_MEAS_WF)
	#define evWF(c, e, p, s, s2) ev(c, e, p, s, s2)
#else
	#define evWF(c, e, p, s, s2)
#endif

//#define EV_MEAS_WF_CHUNK
#if defined(EV_MEAS) && defined(EV_MEAS_WF_CHUNK)
	#define evWFC(c, e, p, s, s2) ev(c, e, p, s, s2)
#else
	#define evWFC(c, e, p, s, s2)
#endif

//#define EV_MEAS_SND
#if defined(EV_MEAS) && defined(EV_MEAS_SND)
	#define evSnd(c, e, p, s, s2) ev(c, e, p, s, s2)
#else
	#define evSnd(c, e, p, s, s2)
#endif

//#define EV_MEAS_DPUMP
#if defined(EV_MEAS) && defined(EV_MEAS_DPUMP)
	#define evDP(c, e, p, s, s2) ev(c, e, p, s, s2)
#else
	#define evDP(c, e, p, s, s2)
#endif

//#define EV_MEAS_DPUMP_CHUNK
#if defined(EV_MEAS) && defined(EV_MEAS_DPUMP_CHUNK)
	#define evDPC(c, e, p, s, s2) ev(c, e, p, s, s2)
#else
	#define evDPC(c, e, p, s, s2)
#endif

char *evprintf(const char *fmt, ...);

#endif

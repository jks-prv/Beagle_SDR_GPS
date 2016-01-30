#include "types.h"
#include "config.h"
#include "kiwi.h"
#include "misc.h"
#include "web.h"
#include "spi.h"
#include "coroutines.h"
#include "debug.h"

#include <sys/file.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <setjmp.h>
#include <ctype.h>
#include <syslog.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>

static bool log_foreground_mode = false;

void xit(int err)
{
	void exit(int);
	
	fflush(stdout);
	spin_ms(1000);	// needed for syslog messages to be properly recorded
	exit(err);
}

void _panic(const char *str, bool coreFile, const char *file, int line)
{
	char *buf;
	
	if (ev_dump) ev(EC_DUMP, EV_PANIC, -1, "panic", "dump");
	asprintf(&buf, "%s: \"%s\" (%s, line %d)", coreFile? "DUMP":"PANIC", str, file, line);

	if (background_mode || log_foreground_mode) {
		syslog(LOG_ERR, "%s\n", buf);
	}
	
	printf("%s\n", buf);
	if (coreFile) abort();
	xit(-1);
}

void _sys_panic(const char *str, const char *file, int line)
{
	char *buf;
	asprintf(&buf, "SYS_PANIC: \"%s\" (%s, line %d)", str, file, line);

	if (background_mode || log_foreground_mode) {
		syslog(LOG_ERR, "%s: %m\n", buf);
	}
	
	perror(buf);
	xit(-1);
}

// regular or logging (via syslog()) printf
typedef enum { PRINTF_REG, PRINTF_LOG } printf_e;

static void ll_printf(printf_e type, conn_t *c, const char *fmt, va_list ap)
{
	int i, sl;
	char *s, *cp;
	static bool appending;
	static char *buf, *last_s, *start_s;
	#define VBUF 1024
	
	if (!do_sdr) {
	//if (!background_mode) {
		if ((buf = (char*) malloc(VBUF)) == NULL)
			panic("log malloc");
		vsnprintf(buf, VBUF, fmt, ap);

		// remove our override and call the actual underlying printf
		#undef printf
		printf("%s", buf);
		#define printf ALT_PRINTF
		
		evPrintf(EC_EVENT, EV_PRINTF, -1, "printf", buf);
	
		free(buf);
		return;
	}
	
	if (appending) {
		s = last_s;
	} else {
		if ((buf = (char*) malloc(VBUF)) == NULL)
			panic("log malloc");
		s = buf;
	
		// show state of all rx channels
		rx_chan_t *rx;
		for (rx = rx_chan, i=0; rx < &rx_chan[RX_CHANS]; rx++, i++) {
			*s++ = rx->busy? '0'+i : '.';
		}
		*s++ = ' ';
		
		// show rx channel number if message is associated with a particular rx channel
		if (c != NULL) {
			for (i=0; i < RX_CHANS; i++)
				*s++ = (i == c->rx_channel)? '0'+i : ' ';
		} else {
			for (i=0; i < RX_CHANS; i++) *s++ = ' ';
		}
		*s++ = ' ';
		
		start_s = s;
	}

	vsnprintf(s, VBUF, fmt, ap);
	sl = strlen(s);		// because vsnprintf returns length disregarding limit, not the actual length

	cp = &s[sl-1];
	if (*cp != '\n') {
		last_s = cp+1;
		appending = true;
		return;
	} else {
		appending = false;
	}
	
	// for logging, don't print an empty line at all
	if (!background_mode || strcmp(start_s, "\n") != 0) {
	
		if (type == PRINTF_LOG && (background_mode || log_foreground_mode)) {
			syslog(LOG_INFO, "%s", buf);
		}
	
		time_t t;
		char tb[32];
		time(&t);
		ctime_r(&t, tb);
		tb[24]=0;
		
		// remove our override and call the actual underlying printf
		#undef printf
		printf("%s %s", tb, buf);
		#define printf ALT_PRINTF

		evPrintf(EC_EVENT, EV_PRINTF, -1, "printf", buf);
	}
	
	if (buf) free(buf);
}

void alt_printf(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	ll_printf(PRINTF_REG, NULL, fmt, ap);
	va_end(ap);
}

void cprintf(conn_t *c, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	ll_printf(PRINTF_REG, c, fmt, ap);
	va_end(ap);
}

void clprintf(conn_t *c, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	ll_printf(PRINTF_LOG, c, fmt, ap);
	va_end(ap);
}

void lprintf(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	ll_printf(PRINTF_LOG, NULL, fmt, ap);
	va_end(ap);
}

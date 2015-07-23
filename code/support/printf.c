#include "types.h"
#include "config.h"
#include "wrx.h"
#include "misc.h"
#include "web.h"
#include "spi.h"
#include "coroutines.h"

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

static bool log_foreground_mode = TRUE;

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
	asprintf(&buf, "%s: \"%s\" (%s, line %d)", coreFile? "DUMP":"PANIC", str, file, line);

	if (background_mode || log_foreground_mode) {
		syslog(LOG_ERR, "%s\n", buf);
	}
	
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

void ll_printf(printf_e type, conn_t *c, const char *fmt, va_list ap)
{
	int i, sl;
	char *buf, *s, *cp;
	#define VBUF 1024
	
	if ((buf = (char*) malloc(VBUF)) == NULL)
		panic("log malloc");
	s = buf;
	
	// show state of all rx channels
	conn_t *co;
	for (co = conns; co < &conns[RX_CHANS*2]; co+=2) {
		*s++ = co->remote_port? '0'+co->rx_channel : '.';
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

	vsnprintf(s, VBUF, fmt, ap);
	sl = strlen(s);		// because vsnprintf returns length disregarding limit, not the actual length

	// for logging, don't print an empty line at all
	if (strcmp(s, "\n") != 0) {
	
		if (type == PRINTF_LOG && (background_mode || log_foreground_mode)) {
			syslog(LOG_INFO, "%s", buf);
		}
	
		// can't be doing continued-line output if we're adding a timestamp
		cp = &s[sl-1];
		if (*cp != '\n') {
			cp++; *cp++ = '\n'; *cp++ = 0;		// add a newline if there isn't one
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
	}
	
	free(buf);
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

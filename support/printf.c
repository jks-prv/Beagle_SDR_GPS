#include "types.h"
#include "config.h"
#include "kiwi.h"
#include "misc.h"
#include "str.h"
#include "web.h"
#include "spi.h"
#include "coroutines.h"
#include "debug.h"
#include "printf.h"
#include "ext_int.h"

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
#include <sys/mman.h>

static bool log_foreground_mode = false;
static bool log_ordinary_printfs = false;

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
	
	// errno might be overwritten if the malloc inside asprintf fails
	asprintf(&buf, "SYS_PANIC: \"%s\" (%s, line %d)", str, file, line);

	if (background_mode || log_foreground_mode) {
		syslog(LOG_ERR, "%s: %m\n", buf);
	}
	
	perror(buf);
	xit(-1);
}

// NB: when debugging use real_printf() to avoid loops!
void real_printf(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
}

static bool appending;
static char *buf, *last_s, *start_s;
static int brem;
log_save_t *log_save_p;
static log_save_t log_save;

static void ll_printf(u4_t type, conn_t *c, const char *fmt, va_list ap)
{
	int i, n, sl;
	char *s, *cp;
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
	
		if (buf) free(buf);
		buf = 0;
		return;
	}
	
	if (appending) {
		s = last_s;
	} else {
		brem = VBUF;
		if ((buf = (char*) malloc(VBUF)) == NULL)
			panic("log malloc");
		s = buf;
		start_s = s;
	}

	vsnprintf(s, brem, fmt, ap);
	sl = strlen(s);		// because vsnprintf returns length disregarding limit, not the actual length
	brem -= sl+1;
	
	cp = &s[sl-1];
	if (*cp != '\n' && brem && !(type & PRINTF_MSG)) {
		last_s = cp+1;
		appending = true;
		return;
	} else {
		appending = false;
	}
	
	// for logging, don't print an empty line at all
	if ((type & (PRINTF_REG | PRINTF_LOG)) && (!background_mode || strcmp(start_s, "\n") != 0)) {

		// remove non-ASCII since "systemctl status" gives [blob] message
		// unlike "systemctl log" which prints correctly
		int sl = strlen(buf);
		for (i=0; i < sl; i++)
			if (buf[i] > 0x7f) buf[i] = '?';

		char up_chan_stat[64], *s = up_chan_stat;
		
		// uptime
		u4_t up = timer_sec();
		u4_t sec = up % 60; up /= 60;
		u4_t min = up % 60; up /= 60;
		u4_t hr  = up % 24; up /= 24;
		u4_t days = up;
		if (days)
			sl = sprintf(s, "%dd:%02d:%02d:%02d ", days, hr, min, sec);
		else
			sl = sprintf(s, "%d:%02d:%02d ", hr, min, sec);
		s += sl;
	
		// show state of all rx channels
		rx_chan_t *rx;
		for (rx = rx_chan, i=0; rx < &rx_chan[RX_CHANS]; rx++, i++) {
			*s++ = rx->busy? '0'+i : '.';
		}
		*s++ = ' ';
		
		// show rx channel number if message is associated with a particular rx channel
		if (c != NULL) {
			if (c->type == STREAM_WATERFALL || c->type == STREAM_SOUND || c->type == STREAM_EXT) {
				for (i=0; i < RX_CHANS; i++)
					*s++ = (i == c->rx_channel)? '0'+i : ' ';
			} else {
				n = sprintf(s, "[%02d]", c->self_idx); s += n;
			}
		} else {
			for (i=0; i < RX_CHANS; i++) *s++ = ' ';
		}
		*s = 0;
		
		if (((type & PRINTF_LOG) && (background_mode || log_foreground_mode)) || log_ordinary_printfs) {
			syslog(LOG_INFO, "%s %s", up_chan_stat, buf);
		}
	
		time_t t;
		char tb[32];
		time(&t);
		ctime_r(&t, tb);
		tb[24]=0;
		
		// remove our override and call the actual underlying printf
		#undef printf
			printf("%s %s %s", tb, up_chan_stat, buf);
		#define printf ALT_PRINTF

		evPrintf(EC_EVENT, EV_PRINTF, -1, "printf", buf);

		#define DUMP_ORDINARY_PRINTFS TRUE
		if (DUMP_ORDINARY_PRINTFS ||
			!background_mode ||
			((type & PRINTF_LOG) && (background_mode || log_foreground_mode)) ||
			log_ordinary_printfs) {
				log_save_t *ls = log_save_p;
				if (ls == NULL) {
					#define DUMP_CHILD_TASK_MESSAGES
					#ifdef DUMP_CHILD_TASK_MESSAGES
					#define MAPPED_MAX (64*K)		// FIXME
						ls = log_save_p = (log_save_t *) mmap((caddr_t) 0, MAPPED_MAX, PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, -1, 0);
						assert(ls != MAP_FAILED);
					#else
						ls = log_save_p = &log_save;
					#endif

					ls->mem_ptr = ls->mem;
				}
				
				if (ls->idx < N_LOG_SAVE) {
					if (TaskIsChild()) {
						s = ls->mem_ptr;
						sprintf(s, "%s %s %s", tb, up_chan_stat, buf);
						ls->arr[ls->idx] = s;
						ls->mem_ptr += strlen(s) + SPACE_FOR_NULL;
						ls->malloced[ls->idx] = false;
					} else {
						asprintf(&ls->arr[ls->idx], "%s %s %s", tb, up_chan_stat, buf);
						ls->malloced[ls->idx] = true;
					}
					ls->idx++;
				} else {
					if (ls->malloced[N_LOG_SAVE/2]) {
						free(ls->arr[N_LOG_SAVE/2]);
						ls->malloced[N_LOG_SAVE/2] = false;
					}
					ls->not_shown++;
					for (i = N_LOG_SAVE/2 + 1; i < N_LOG_SAVE; i++) {
						ls->arr[i-1] = ls->arr[i];
						ls->malloced[i-1] = ls->malloced[i];
					}
					if (TaskIsChild()) {
						s = ls->mem_ptr;
						sprintf(s, "%s %s %s", tb, up_chan_stat, buf);
						ls->arr[N_LOG_SAVE-1] = s;
						ls->mem_ptr += strlen(s) + SPACE_FOR_NULL;
						ls->malloced[N_LOG_SAVE-1] = false;
					} else {
						asprintf(&ls->arr[N_LOG_SAVE-1], "%s %s %s", tb, up_chan_stat, buf);
						ls->malloced[N_LOG_SAVE-1] = true;
					}
				}
		}
	}
	
	// attempt to selectively record message remotely
	if (type & PRINTF_MSG) {
		for (conn_t *c = conns; c < &conns[N_CONNS]; c++) {
			struct mg_connection *mc;
			
			if (!c->valid || c->type != STREAM_MFG || ((mc = c->mc) == NULL))
				continue;
			if (type & PRINTF_FF)
				send_msg_encoded_mc(mc, "MSG", "status_msg_text", "\f%s", buf);
			else
				send_msg_encoded_mc(mc, "MSG", "status_msg_text", "%s", buf);
		}
	}
	
	if (buf) free(buf);
	buf = 0;
}

void alt_printf(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	ll_printf(PRINTF_REG, NULL, fmt, ap);
	va_end(ap);
}

void lfprintf(u4_t printf_type, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	ll_printf(printf_type, NULL, fmt, ap);
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

void mprintf(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	ll_printf(PRINTF_MSG, NULL, fmt, ap);
	va_end(ap);
}

void mprintf_ff(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	ll_printf(PRINTF_MSG|PRINTF_FF, NULL, fmt, ap);
	va_end(ap);
}

void mlprintf(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	ll_printf(PRINTF_MSG|PRINTF_LOG, NULL, fmt, ap);
	va_end(ap);
}

void mlprintf_ff(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	ll_printf(PRINTF_MSG|PRINTF_LOG|PRINTF_FF, NULL, fmt, ap);
	va_end(ap);
}

// encoded snprintf()
int esnprintf(char *str, size_t slen, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	int rv = vsnprintf(str, slen, fmt, ap);
	va_end(ap);

	char *str2 = str_encode(str);
	int slen2 = strlen(str2);
	
	// Passed sizeof str[slen] is meant to be far larger than current strlen(str)
	// so there is room to return the larger encoded result.
	check(slen2 <= slen);
	strcpy(str, str2);
	free(str2);

	return slen2;
}

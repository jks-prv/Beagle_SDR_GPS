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
	
	if (ev_dump) ev(EC_DUMP_CONT, EV_PRINTF, -1, "panic", "dump");
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
	
		free(buf);
		buf = NULL;
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

		char *sb, *sb2;
		bool want_logged = (type & PRINTF_LOG);
		
		// uptime
		u4_t up = timer_sec();
		u4_t sec = timer_ms() % 60000; up /= 60;
		u4_t min = up % 60; up /= 60;
		u4_t hr  = up % 24; up /= 24;
		u4_t days = up;
		sb = NULL;
		if (days) {
			asprintf(&sb, "%dd:", days);
            sb = kstr_wrap(sb);
		}
        asprintf(&sb2, "%02d:%02d:%s%.3f ", hr, min, (sec < 10000)? "0":"", (float) sec/1e3);
        sb = kstr_cat(sb, kstr_wrap(sb2));
	
		// show state of all rx channels
		rx_chan_t *rx;
		char ch_stat[RX_CHANS + 3 + SPACE_FOR_NULL];
		for (rx = rx_channels, i=0; rx < &rx_channels[RX_CHANS]; rx++, i++) {
			ch_stat[i] = rx->busy? '0'+i : '.';
		}
		ch_stat[i] = ' ';
		ch_stat[i+1] = '\0';
		sb = kstr_cat(sb, ch_stat);
		
		// show rx channel number if message is associated with a particular rx channel
        int chan = -1;
        if (c && (c->type == STREAM_WATERFALL || c->type == STREAM_SOUND))
            chan = c->rx_channel;
        if (c && c->type == STREAM_EXT)
            chan = c->ext_rx_chan;
        if (c == NULL || chan != -1) {
            for (i=0; i < RX_CHANS; i++) {
                ch_stat[i] = (c != NULL && i == chan)? '0'+i : ' ';
            }
            if (!background_mode) {
                ch_stat[i++] = ' ';
                ch_stat[i++] = want_logged? 'L':' ';
            }
            ch_stat[i] = '\0';
            sb = kstr_cat(sb, ch_stat);
        } else {
            if (background_mode)
                asprintf(&sb2, "[%02d]", c->self_idx);
            else
                asprintf(&sb2, "[%02d] %c", c->self_idx, want_logged? 'L':' ');
            sb = kstr_cat(sb, kstr_wrap(sb2));
        }
        
        sb2 = kstr_sp(sb);
		
		bool actually_log = ((want_logged && (background_mode || log_foreground_mode)) || log_ordinary_printfs);
		if (actually_log) {
			syslog(LOG_INFO, "%s %s", sb2, buf);
		}
	
		time_t t;
		time(&t);
		char tb[CTIME_R_BUFSIZE];
		ctime_r(&t, tb);
		tb[CTIME_R_NL-5] = '\0';    // remove the year
		
		// remove our override and call the actual underlying printf
		#undef printf
			printf("%s %s %s", tb, sb2, buf);
		#define printf ALT_PRINTF

		evPrintf(EC_EVENT, EV_PRINTF, -1, "printf", buf);

		#define DUMP_ORDINARY_PRINTFS TRUE
        #define DUMP_CHILD_TASK_MESSAGES
        // FIXME: synchronization problem
        
		if (DUMP_ORDINARY_PRINTFS || !background_mode || actually_log || log_ordinary_printfs) {

            // when msg generated by child task must use mmap'd buffer to communicate to parent task
            log_save_t *ls = log_save_p;
            if (ls == NULL) {
                #ifdef DUMP_CHILD_TASK_MESSAGES
                    int size = sizeof(log_save_t) + (N_LOG_SAVE * N_LOG_MSG_LEN);
                    ls = log_save_p = (log_save_t *) mmap((caddr_t) 0, size, PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, -1, 0);
                    assert(ls != MAP_FAILED);
                    memset(ls, 0, size);
                    char *p = ls->mem;
                    for (i = 0; i < N_LOG_SAVE; i++) {
                        ls->mbuf[i] = p;
                        p += N_LOG_MSG_LEN;
                    }
                #else
                    ls = log_save_p = &log_save;
                #endif
            }
            
            if (ls->idx < N_LOG_SAVE) {
                // potential race: hope that Linux doesn't timeslice between next two statements
                ls->idx++;
                int idx = ls->idx-1;
                if (TaskIsChild()) {
                    s = ls->arr[idx] = ls->mbuf[idx];
                    snprintf(s, N_LOG_MSG_LEN, "%s %s %s", tb, sb2, buf);
                    strcpy(&s[N_LOG_MSG_LEN-2], "\n");      // truncate msg
                    ls->malloced[idx] = false;
                } else {
                    asprintf(&ls->arr[idx], "%s %s %s", tb, sb2, buf);
                    ls->malloced[idx] = true;
                }
            } else {
                // free first message at top-half
                if (ls->malloced[N_LOG_SAVE/2]) {
                    free(ls->arr[N_LOG_SAVE/2]);
                    ls->arr[N_LOG_SAVE/2] = NULL;
                    ls->malloced[N_LOG_SAVE/2] = false;
                }
                ls->not_shown++;
                
                // scroll top-half of messages by one by rotating the buffer pointers
                char *tarr = ls->arr[N_LOG_SAVE/2 +1];
                bool tmalloced = ls->malloced[N_LOG_SAVE/2 +1];
                char *mbuf = ls->mbuf[N_LOG_SAVE/2 +1];
                for (i = N_LOG_SAVE/2 + 1; i < N_LOG_SAVE; i++) {
                    ls->arr[i-1] = ls->arr[i];
                    ls->malloced[i-1] = ls->malloced[i];
                    ls->mbuf[i-1] = ls->mbuf[i];
                }
                ls->arr[N_LOG_SAVE-1] = tarr;
                ls->malloced[N_LOG_SAVE-1] = tmalloced;
                ls->mbuf[N_LOG_SAVE-1] = mbuf;
                
                if (TaskIsChild()) {
                    s = ls->arr[N_LOG_SAVE-1] = ls->mbuf[N_LOG_SAVE-1];
                    snprintf(s, N_LOG_MSG_LEN, "%s %s %s", tb, sb2, buf);
                    strcpy(&s[N_LOG_MSG_LEN-2], "\n");      // truncate msg
                    ls->malloced[N_LOG_SAVE-1] = false;
                } else {
                    asprintf(&ls->arr[N_LOG_SAVE-1], "%s %s %s", tb, sb2, buf);
                    ls->malloced[N_LOG_SAVE-1] = true;
                }
            }
		}

	    kstr_free(sb);
	}
	
	// attempt to selectively record message remotely
	if (type & PRINTF_MSG) {
		for (conn_t *c = conns; c < &conns[N_CONNS]; c++) {
			struct mg_connection *mc;
			
			if (!c->valid || (c->type != STREAM_ADMIN && c->type != STREAM_MFG) || c->mc == NULL)
				continue;
			if (type & PRINTF_FF)
				send_msg_encoded(c, "MSG", "status_msg_text", "\f%s", buf);
			else
				send_msg_encoded(c, "MSG", "status_msg_text", "%s", buf);
		}
	}
	
	free(buf);
	buf = NULL;
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

void clfprintf(conn_t *c, u4_t printf_type, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	ll_printf(printf_type, c, fmt, ap);
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

#define N_DST_STATIC (255 + SPACE_FOR_NULL)
static char dst_static[N_DST_STATIC];

// result in a static buffer for use with e.g. a short-term immediate printf argument
// NB: not thread-safe
char *stprintf(const char *fmt, ...)
{
	if (fmt == NULL) return NULL;
	va_list ap;
	va_start(ap, fmt);
    vsnprintf(dst_static, N_DST_STATIC, fmt, ap);
    va_end(ap);
	return dst_static;
}

// encoded snprintf()
int esnprintf(char *str, size_t slen, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	int rv = vsnprintf(str, slen, fmt, ap);
	va_end(ap);

	char *str2 = kiwi_str_encode(str);
	int slen2 = strlen(str2);
	
	// Passed sizeof str[slen] is meant to be far larger than current strlen(str)
	// so there is room to return the larger encoded result.
	check(slen2 <= slen);
	strcpy(str, str2);
	free(str2);

	return slen2;
}

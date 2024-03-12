#pragma once

#include "types.h"
#include "conn.h"
#include "kiwi_assert.h"

#define PRINTF_U64_ARG(arg) \
    (u4_t) ((arg) >> 32), (u4_t) ((arg) & 0xffffffff)

// printf_type: regular or logging (via syslog()) printf
#define	PRINTF_FLAGS	0xff
#define PRINTF_REG		0x01
#define PRINTF_LOG		0x02
#define PRINTF_MSG		0x04
#define PRINTF_FF		0x08	// add a "form-feed" to stop appending to 'id-output-msg' on browser
#define PRINTF_REAL		0x10

// override printf so we can add a timestamp, log it, etc.
#ifdef KIWI
 #define ALT_PRINTF alt_printf
#else
 #define ALT_PRINTF printf
#endif

#define printf ALT_PRINTF
void alt_printf(const char *fmt, ...);

void printf_init();

// versions of printf & lprintf that preface message with rx channel
void cprintf(conn_t *c, const char *fmt, ...);
void clprintf(conn_t *c, const char *fmt, ...);
void clfprintf(conn_t *c, u4_t printf_type, const char *fmt, ...);
void real_printf(const char *fmt, ...);
void lfprintf(u4_t printf_type, const char *fmt, ...);
void lprintf(const char *fmt, ...);
void rcprintf(int rx_chan, const char *fmt, ...);
void rclprintf(int rx_chan, const char *fmt, ...);
void rcfprintf(int rx_chan, u4_t printf_type, const char *fmt, ...);
void mprintf(const char *fmt, ...);
void mprintf_ff(const char *fmt, ...);
void mlprintf(const char *fmt, ...);
void mlprintf_ff(const char *fmt, ...);
char *stnprintf(int which, const char *fmt, ...);
char *stprintf(const char *fmt, ...);   // extension API compatibility
C_LINKAGE(const char *aspf(const char *fmt, ...));
int esnprintf(char *str, size_t slen, const char *fmt, ...);
void printf_highlight(int which, const char *prefix);

void kiwi_backtrace(const char *id, u4_t printf_type=0);
void _panic(const char *str, bool coreFile, const char *file, int line);
void _real_panic(const char *str, bool coreFile, const char *file, int line);
void _sys_panic(const char *str, const char *file, int line);

#ifdef KIWI
 #define ALT_EXIT kiwi_exit_dont_use
#else
 #define ALT_EXIT exit
#endif

#define exit ALT_EXIT
void kiwi_exit(int err);
void kiwi_exit_dont_use(int err);

extern bool log_foreground_mode;

#define scall(x, y) if ((y) < 0) sys_panic(x);
#define scallz(x, y) if ((y) == 0) sys_panic(x);
#define scalle(x, y) if ((y) < 0) perror(x);

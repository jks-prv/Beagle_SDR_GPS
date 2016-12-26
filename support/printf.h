#ifndef _PRINTF_H_
#define _PRINTF_H_

#include "types.h"
#include "kiwi.h"

#define panic(s) _panic(s, FALSE, __FILE__, __LINE__);
#define dump_panic(s) _panic(s, TRUE, __FILE__, __LINE__);
#define sys_panic(s) _sys_panic(s, __FILE__, __LINE__);

#define check(e) \
	if (!(e)) { \
		lprintf("check failed: \"%s\" %s line %d\n", #e, __FILE__, __LINE__); \
		panic("check"); \
	}

#ifdef DEBUG
	#define D_PRF(x) printf x;
	#define D_STMT(x) x;
	#define assert(e) \
		if (!(e)) { \
			lprintf("assertion failed: \"%s\" %s line %d\n", #e, __FILE__, __LINE__); \
			panic("assert"); \
		}
	#define assert_dump(e) \
		if (!(e)) { \
			lprintf("assertion failed: \"%s\" %s line %d\n", #e, __FILE__, __LINE__); \
			dump_panic("assert_dump"); \
		}
	#define assert_exit(e) \
		if (!(e)) { \
			lprintf("assertion failed: \"%s\" %s line %d\n", #e, __FILE__, __LINE__); \
			goto error_exit; \
		}
#else
	#define D_PRF(x)
	#define D_STMT(x)
	#define assert(e)
	#define assert_exit(e)
#endif

// printf_type: regular or logging (via syslog()) printf
#define PRINTF_REG		0x01
#define PRINTF_LOG		0x02
#define PRINTF_MSG		0x04
#define PRINTF_FF		0x08	// add a "form-feed" to stop appending to 'id-status-msg' on browser

#define N_LOG_SAVE	256
struct log_save_t {
	int idx, not_shown;
	char *arr[N_LOG_SAVE];
	bool malloced[N_LOG_SAVE];
	char *mem_ptr;
	char mem[1];	// mem allocated starting here; must be last in struct
};
extern log_save_t *log_save_p;

void real_printf(const char *fmt, ...);
void lfprintf(u4_t printf_type, const char *fmt, ...);
void lprintf(const char *fmt, ...);
void mprintf(const char *fmt, ...);
void mprintf_ff(const char *fmt, ...);
void mlprintf(const char *fmt, ...);
void mlprintf_ff(const char *fmt, ...);
int esnprintf(char *str, size_t slen, const char *fmt, ...);

void _panic(const char *str, bool coreFile, const char *file, int line);
void _sys_panic(const char *str, const char *file, int line);
void xit(int err);

#define scall(x, y) if ((y) < 0) sys_panic(x);
#define scallz(x, y) if ((y) == 0) sys_panic(x);

#endif

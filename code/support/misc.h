#ifndef _MISC_H_
#define _MISC_H_

#include "types.h"
#include "wrx.h"

#define panic(s) _panic(s, __FILE__, __LINE__);
#define sys_panic(s) _sys_panic(s, __FILE__, __LINE__);

#ifdef DEBUG
	#define D_PRF(x) printf x;
	#define D_STMT(x) x;
	#define assert(e) \
		if (!(e)) { \
			lprintf("assertion failed: \"%s\" %s line %d\n", #e, __FILE__, __LINE__); \
			panic("assert"); \
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

#define MALLOC_DEBUG
#ifdef MALLOC_DEBUG
	void *wrx_malloc(const char *from, size_t size);
	void wrx_free(const char *from, void *ptr);
	char *wrx_strdup(const char *from, const char *s);
	void wrx_str_redup(char **ptr, const char *from, const char *s);
	int wrx_malloc_stat();
#else
	#define wrx_malloc(from, size) malloc(size)
	#define wrx_free(from, ptr) free(ptr)
	#define wrx_strdup(from, s) strdup(s)
	void wrx_str_redup(char **ptr, const char *from, const char *s);
	#define wrx_malloc_stat() 0
#endif

void lprintf(const char *fmt, ...);
void _panic(const char *str, const char *file, int line);
void _sys_panic(const char *str, const char *file, int line);
void xit(int err);

#define scall(x, y) if ((y) < 0) sys_panic(x);
#define scallz(x, y) if ((y) == 0) sys_panic(x);

int split(char *cp, int *argc, char *argv[], int nargs);
int str2enum(const char *s, const char *strs[], int len);
const char *enum2str(int e, const char *strs[], int len);

#ifdef CLIENT_SIDE
 #define timer_ms() 0
#else
 u4_t timer_ms(void);
 u4_t timer_us(void);
#endif

void clr_set_ctrl(u2_t clr, u2_t set);
u2_t getmem(u2_t addr);
void printmem(const char *str, u2_t addr);

void send_msg(conn_t *c, const char *msg, ...);
void send_msg_mc(struct mg_connection *mc, const char *msg, ...);

// DEPRECATED: still in WSPR code
void send_meta(conn_t *c, u1_t cmd, u4_t p1, u4_t p2);
void send_meta_mc(struct mg_connection *mc, u1_t cmd, u4_t p1, u4_t p2);
void send_meta_bytes(conn_t *c, u1_t cmd, u1_t *bytes, int nbytes);

float ecpu_use();


//#define EV_MEAS
#ifdef EV_MEAS
	void ev(int event, const char *s, const char *s2);
#else
	#define ev(e, s, s2)
#endif

#define EV_MEAS_SPI
#if defined(EV_MEAS) && defined(EV_MEAS_SPI)
	#define evSpi(e, s, s2) ev(e, s, s2)
#else
	#define evSpi(e, s, s2)
#endif

#define EV_MEAS_WF
#if defined(EV_MEAS) && defined(EV_MEAS_WF)
	#define evWf(e, s, s2) ev(e, s, s2)
#else
	#define evWf(e, s, s2)
#endif

//#define EV_MEAS_SND
#if defined(EV_MEAS) && defined(EV_MEAS_SND)
	#define evSnd(e, s, s2) ev(e, s, s2)
#else
	#define evSnd(e, s, s2)
#endif

#define	EV_TRIGGER		0
#define	EV_NEXTTASK		1
#define	EV_SPILOOP		2
#define	EV_WFSAMPLE		3
#define	EV_SND			4

char *evprintf(const char *fmt, ...);


#ifdef REG_RECORD
	typedef enum { REG_READ, REG_WRITE, REG_STR, REG_RESET } reg_type_t;
	
	typedef struct {
		u4_t iCount;
		u2_t rPC;
		u1_t n_irq;
		u4_t irq_masked;
	} t_hr_stamp;

	typedef void (callback_t)(u2_t addr, u1_t data, u4_t dup, u4_t time, char *str);

	void reg_record(int chan, reg_type_t type, u2_t addr, u1_t data, u4_t time, char *str);
	void reg_stamp(int write, u4_t iCount, u2_t rPC, u1_t n_irq, u4_t irq_masked);
	t_hr_stamp *reg_get_stamp();
	void reg_dump(u1_t chan, callback_t rdecode, callback_t wdecode, callback_t sdecode);
	void reg_cmp(callback_t adecode);
#else
	#define reg_record(chan, type, addr, data, time, str)
	#define reg_stamp(write, iCount, rPC, n_irq, irq_masked)
	#define reg_get_stamp()
	#define reg_dump(chan, rdecode, wdecode, sdecode)
	#define reg_cmp(adecode)
#endif

#endif

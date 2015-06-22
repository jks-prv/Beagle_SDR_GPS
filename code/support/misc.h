#ifndef _MISC_H_
#define _MISC_H_

#include "types.h"
#include "wrx.h"
#include "printf.h"

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

#endif

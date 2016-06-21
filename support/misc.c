#include "types.h"
#include "config.h"
#include "kiwi.h"
#include "misc.h"
#include "web.h"
#include "spi.h"
#include "cfg.h"
#include "coroutines.h"

#include <sys/file.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <setjmp.h>
#include <ctype.h>
#include <syslog.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>

#ifdef MALLOC_DEBUG

#define NMT 1024
struct mtrace_t {
	int i;
	const char *from;
	char *ptr;
	int size;
} mtrace[NMT];

static int nmt;

static void mt_enter(const char *from, void *ptr, int size)
{
	int i;
	mtrace_t *mt;
	
	for (i=0; i<NMT; i++) {
		mt = &mtrace[i];
		if (mt->ptr == ptr) panic("malloc dup");
		if (mt->ptr == NULL) {
			mt->i = i;
			mt->from = from;
			mt->ptr = (char*) ptr;
			mt->size = size;
			break;
		}
	}
	if (i == NMT) panic("malloc overflow");
	nmt++;
}

#define	MALLOC_MAX	(512*K)

void *kiwi_malloc(const char *from, size_t size)
{
	if (size > MALLOC_MAX) panic("malloc > MALLOC_MAX");
	void *ptr = malloc(size);
	memset(ptr, 0, size);
	mt_enter(from, ptr, size);
	return ptr;
}

char *kiwi_strdup(const char *from, const char *s)
{
	int sl = strlen(s)+1;
	if (sl == 0 || sl > 1024) panic("strdup size");
	char *ptr = strdup(s);
	mt_enter(from, (void*) ptr, sl);
	return ptr;
}

void kiwi_free(const char *from, void *ptr)
{
	int i;
	mtrace_t *mt;
	
	for (i=0; i<NMT; i++) {
		mt = &mtrace[i];
		if (mt->ptr == (char*) ptr) {
			mt->ptr = NULL;
			free(ptr);
			break;
		}
	}
	if (i == NMT) panic("free not found");
	nmt--;
}

int kiwi_malloc_stat()
{
	return nmt;
}

#endif

// either cfg_name or override are optional (set to NULL)
int set_option(int *option, const char* cfg_name, int *override)
{
	bool set = false;
	
	// override: 0=none, 1=force on, -1=force off
	if (override != NULL) {
		if (*override != 0) {
			*option = (*override == 1)? 1:0;
			set = true;
		}
	}
	
	if (!set && cfg_name != NULL) {
		int cfg = cfg_bool(cfg_name, NULL, CFG_OPTIONAL);
		if (cfg != NOT_FOUND) {
			*option = cfg;
		} else {
			*option = 0;	// default to false if not found
		}
		set = true;
	}
	
	if (!set) {
		lprintf("config name and override both NULL?\n");
		lprintf("or override is none?\n");
		panic("set_option");
	}
	
	return *option;
}

void get_chars(char *field, char *value, size_t size)
{
	memcpy(value, field, size);
	value[size] = '\0';
}

void set_chars(char *field, const char *value, const char fill, size_t size)
{
	int slen = strlen(value);
	assert(slen <= size);
	memset(field, (int) fill, size);
	memcpy(field, value, strlen(value));
}

void kiwi_str_redup(char **ptr, const char *from, const char *s)
{
	int sl = strlen(s)+1;
	if (sl == 0 || sl > 1024) panic("strdup size");
	if (*ptr) kiwi_free(from, (void*) *ptr);
	*ptr = strdup(s);
#ifdef MALLOC_DEBUG
	mt_enter(from, (void*) *ptr, sl);
#endif
}

int split(char *cp, int *argc, char *argv[], int nargs)
{
	int n=0;
	char **ap;
	
	for (ap = argv; (*ap = strsep(&cp, " \t\n")) != NULL;) {
		if (**ap != '\0') {
			n++;
			if (++ap >= &argv[nargs])
				break;
		}
	}
	
	return n;
}

char *str_escape(const char *s)
{
	int i;
	
	if (!s)
		s = "";
	int slen = strlen(s);
	int nesc = 0;
	
	for (i=0; i < slen; i++) {
		char c = s[i];
		if (c == '"' || c == '\'')
			nesc++;
	}
	
	char *newstr = (char *) kiwi_malloc("str_escape", slen + nesc + 1);
	char *ns = newstr;
	
	for (i=0; i < slen; i++) {
		char c = s[i];
		if (c == '"' || c == '\'') {
			*ns++ = '\\';
		}
		*ns++ = c;
	}
	
	*ns = '\0';
	return newstr;
}

int str2enum(const char *s, const char *strs[], int len)
{
	int i;
	for (i=0; i<len; i++) {
		if (strcasecmp(s, strs[i]) == 0) return i;
	}
	return NOT_FOUND;
}

const char *enum2str(int e, const char *strs[], int len)
{
	if (e < 0 || e >= len) return NULL;
	assert(strs[e] != NULL);
	return (strs[e]);
}

void kiwi_chrrep(char *str, const char from, const char to)
{
	char *cp;
	while ((cp = strchr(str, from)) != NULL) {
		*cp = to;
	}
}

// used by qsort
int qsort_floatcomp(const void* elem1, const void* elem2)
{
    if(*(const float*)elem1 < *(const float*)elem2)
        return -1;
    return *(const float*)elem1 > *(const float*)elem2;
}

u2_t ctrl_get()
{
	static SPI_MISO ctrl;
	
	spi_get_noduplex(CmdCtrlGet, &ctrl, sizeof(ctrl.word[0]));
	return ctrl.word[0];
}

void ctrl_clr_set(u2_t clr, u2_t set)
{
	if (clr) spi_set_noduplex(CmdCtrlClr, clr);
	if (set) spi_set_noduplex(CmdCtrlSet, set);
}

u2_t getmem(u2_t addr)
{
	static SPI_MISO mem;
	
	memset(mem.word, 0x55, sizeof(mem.word));
	spi_get_noduplex(CmdGetMem, &mem, 4, addr);
	assert(addr == mem.word[1]);
	
	return mem.word[0];
}

// the popen read can block, so do non-blocking i/o with an interspersed TaskSleep()
int non_blocking_cmd(const char *cmd, char *reply, int reply_size, int *status)
{
	int n, rem = reply_size, stat;
	char *bp = reply;
	
	NextTask("non_blocking_cmd");
	FILE *pf = popen(cmd, "r");
	if (pf == NULL) return 0;
	int pfd = fileno(pf);
	if (pfd <= 0) return 0;
	fcntl(pfd, F_SETFL, O_NONBLOCK);

	do {
		TaskSleep(50000);
		n = read(pfd, bp, rem);
		if (n > 0) {
			bp += n;
			rem -= n;
			if (rem <= 0)
				break;
			continue;
		}
	} while (n == -1 && errno == EAGAIN);

	// assuming we're always expecting a string
	if (rem <= 0) {
		bp = &reply[reply_size-1];
	}
	*bp = 0;
	stat = pclose(pf);
	if (status != NULL)
		*status = stat;
	return (bp - reply);
}

int non_blocking_cmd_popen(non_blocking_cmd_t *p)
{
	NextTask("non_blocking_cmd_popen");
	p->pf = popen(p->cmd, "r");
	if (p->pf == NULL) return 0;
	p->pfd = fileno(p->pf);
	if (p->pfd <= 0) return 0;
	fcntl(p->pfd, F_SETFL, O_NONBLOCK);

	return 1;
}

int non_blocking_cmd_read(non_blocking_cmd_t *p, char *reply, int reply_size)
{
	int n;

	do {
		TaskSleep(50000);
		n = read(p->pfd, reply, reply_size);
		if (n > 0) reply[(n == reply_size)? n-1 : n] = 0;	// assuming we're always expecting a string
	} while (n == -1 && errno == EAGAIN);

	return n;
}

int non_blocking_cmd_pclose(non_blocking_cmd_t *p)
{
	return pclose(p->pf);
}

void printmem(const char *str, u2_t addr)
{
	printf("%s %04x: %04x\n", str, addr, (int) getmem(addr));
}

u4_t kiwi_n2h_32(char *ip_str)
{
	int n;
	u4_t ip[4];
	n = sscanf(ip_str, "%d.%d.%d.%d", &ip[0], &ip[1], &ip[2], &ip[3]);
	assert(n == 4);
	return (ip[3]&0xff) | ((ip[2]&0xff)<<8) | ((ip[1]&0xff)<<16) | ((ip[0]&0xff)<<24);
}

void send_msg(conn_t *c, bool debug, const char *msg, ...)
{
	va_list ap;
	char *s;

	va_start(ap, msg);
	vasprintf(&s, msg, ap);
	va_end(ap);
	if (debug) cprintf(c, "send_msg: <%s>\n", s);
	mg_websocket_write(c->mc, WS_OPCODE_BINARY, s, strlen(s));
	free(s);
}

// sent direct to mg_connection
// caution: never use an mprint() here as this will result in a loop
void send_msg_mc(struct mg_connection *mc, bool debug, const char *msg, ...)
{
	va_list ap;
	char *s;

	va_start(ap, msg);
	vasprintf(&s, msg, ap);
	va_end(ap);
	size_t slen = strlen(s);
	if (debug) printf("send_msg_mc: %d <%s>\n", slen, s);
	mg_websocket_write(mc, WS_OPCODE_BINARY, s, slen);
	free(s);
}

void send_encoded_msg_mc(struct mg_connection *mc, const char *dst, const char *cmd, const char *fmt, ...)
{
	va_list ap;
	char *s;

	if (cmd == NULL || fmt == NULL) return;
	
	va_start(ap, fmt);
	vasprintf(&s, fmt, ap);
	va_end(ap);
	size_t slen = strlen(s)*3 + 8;
	char *buf = (char *) kiwi_malloc("send_encoded_msg_mc", slen);
	
	mg_url_encode(s, buf, slen-1);
	send_msg_mc(mc, FALSE, "%s %s=%s", dst, cmd, buf);
	kiwi_free("send_encoded_msg_mc", buf);
}

// DEPRECATED: still in WSPR code

#if 0
typedef struct {
	char hdr[4];
	u1_t flag, cmd;
	union {
		struct {
			u1_t p1[4];		// ba[2..5] on client-side
			u1_t p2[4];		// ba[6..9] on client-side
		};
		#define NMETA 1024
		u1_t bytes[NMETA];
	};
} meta_t;

static void meta_set(meta_t *meta, u1_t cmd, u4_t p1, u4_t p2)
{
	strncmp(meta->hdr, "FFT ", 4);
	meta->flag = 0xff;
	meta->cmd = cmd;
	
	meta->p1[0] = p1 & 0xff;
	meta->p1[1] = (p1>>8) & 0xff;
	meta->p1[2] = (p1>>16) & 0xff;
	meta->p1[3] = (p1>>24) & 0xff;
	
	meta->p2[0] = p2 & 0xff;
	meta->p2[1] = (p2>>8) & 0xff;
	meta->p2[2] = (p2>>16) & 0xff;
	meta->p2[3] = (p2>>24) & 0xff;
}

// sent on the waterfall port
void send_meta(conn_t *c, u1_t cmd, u4_t p1, u4_t p2)
{
	meta_t meta;
	meta_set(&meta, cmd, p1, p2);
	
	assert(c->type == STREAM_WATERFALL);
	app_to_web(c, (char*) &meta, 4+2+8);
}

// sent direct to mg_connection
void send_meta_mc(struct mg_connection *mc, u1_t cmd, u4_t p1, u4_t p2)
{
	meta_t meta;
	meta_set(&meta, cmd, p1, p2);

	mg_websocket_write(mc, WS_OPCODE_BINARY, (char*) &meta, 4+2+8);
}

void send_meta_bytes(conn_t *c, u1_t cmd, u1_t *bytes, int nbytes)
{
	meta_t meta;
	
	assert(c->type == STREAM_WATERFALL);
	assert(nbytes <= NMETA);

	strncmp(meta.hdr, "FFT ", 4);
	meta.flag = 0xff;
	meta.cmd = cmd;
	memcpy(meta.bytes, bytes, nbytes);
	app_to_web(c, (char*) &meta, 4+2+nbytes);
}
#endif

float ecpu_use()
{
	typedef struct {
		u1_t f0, g0, f1, g1, f2, g2, f3, g3;
	} ctr_t;
	ctr_t *c;
	static SPI_MISO cpu;
	
	if (down || do_fft) return 0;
	spi_get_noduplex(CmdGetCPUCtr, &cpu, sizeof(u2_t[3]));
	c = (ctr_t*) &cpu.word[0];
	u4_t gated = (c->g3 << 24) | (c->g2 << 16) | (c->g1 << 8) | c->g0;
	u4_t free_run = (c->f3 << 24) | (c->f2 << 16) | (c->f1 << 8) | c->f0;
	spi_set(CmdCPUCtrClr);
	return ((float) gated / (float) free_run * 100);
}

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

//#define kmprintf(x) printf x;
#define kmprintf(x)

#define NMT 1024
struct mtrace_t {
	int i;
	const char *from;
	char *ptr;
	int size;
} mtrace[NMT];

static int nmt;

static int mt_enter(const char *from, void *ptr, int size)
{
	int i;
	mtrace_t *mt;
	
	for (i=0; i<NMT; i++) {
		mt = &mtrace[i];
		if (mt->ptr == ptr) {
			kmprintf(("mt_enter \"%s\" #%d (\"%s\") %d %p\n", from, i, mt->from, size, ptr));
			panic("mt_enter dup");
		}
		if (mt->ptr == NULL) {
			mt->i = i;
			mt->from = from;
			mt->ptr = (char*) ptr;
			mt->size = size;
			break;
		}
	}
	
	if (i == NMT) panic("mt_enter overflow");
	nmt++;
	return i+1;
}

static void mt_remove(const char *from, void *ptr)
{
	int i;
	mtrace_t *mt;
	
	for (i=0; i<NMT; i++) {
		mt = &mtrace[i];
		if (mt->ptr == (char*) ptr) {
			mt->ptr = NULL;
			break;
		}
	}
	
	kmprintf(("mt_remove \"%s\" #%d %p\n", from, i+1, ptr));
	if (i == NMT) panic("mt_remove not found");
	nmt--;
}

#define	MALLOC_MAX	(512*K)

void *kiwi_malloc(const char *from, size_t size)
{
	if (size > MALLOC_MAX) panic("malloc > MALLOC_MAX");
	kmprintf(("kiwi_malloc-1 \"%s\" %d\n", from, size));
	void *ptr = malloc(size);
	memset(ptr, 0, size);
	int i = mt_enter(from, ptr, size);
	kmprintf(("kiwi_malloc-2 \"%s\" #%d %d %p\n", from, i, size, ptr));
	return ptr;
}

void *kiwi_realloc(const char *from, void *ptr, size_t size)
{
	if (size > MALLOC_MAX) panic("malloc > MALLOC_MAX");
	kmprintf(("kiwi_realloc-1 \"%s\" %d %p\n", from, size, ptr));
	mt_remove(from, ptr);
	ptr = realloc(ptr, size);
	int i = mt_enter(from, ptr, size);
	kmprintf(("kiwi_realloc-2 \"%s\" #%d %d %p\n", from, i, size, ptr));
	return ptr;
}

char *kiwi_strdup(const char *from, const char *s)
{
	int sl = strlen(s)+1;
	if (sl == 0 || sl > 1024) panic("strdup size");
	char *ptr = strdup(s);
	int i = mt_enter(from, (void*) ptr, sl);
	kmprintf(("kiwi_strdup \"%s\" #%d %d %p %p\n", from, i, sl, s, ptr));
	return ptr;
}

void kiwi_free(const char *from, void *ptr)
{
	mt_remove(from, ptr);
	free(ptr);
}

int kiwi_malloc_stat()
{
	return nmt;
}

#endif

void kiwi_str_redup(char **ptr, const char *from, const char *s)
{
	int sl = strlen(s)+1;
	if (sl == 0 || sl > 1024) panic("strdup size");
	if (*ptr) kiwi_free(from, (void*) *ptr);
	*ptr = strdup(s);
#ifdef MALLOC_DEBUG
	int i = mt_enter(from, (void*) *ptr, sl);
	kmprintf(("kiwi_str_redup \"%s\" #%d %d %p %p\n", from, i, sl, s, *ptr));
#endif
}


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

void str_unescape_quotes(char *str)
{
	char *s, *o;
	
	for (s = o = str; *s != '\0';) {
		if (*s == '\\' && (*(s+1) == '"' || *(s+1) == '\'')) {
			*o++ = *(s+1); s += 2;
		} else {
			*o++ = *s++;
		}
	}
	
	*o = '\0';
}

char *str_encode(char *s)
{
	size_t slen = strlen(s) * ENCODE_EXPANSION_FACTOR;
	slen++;		// null terminated
	// don't use kiwi_malloc() due to large number of these simultaneously active from dx list
	// and also because dx list has to use free() due to related allocations via strdup()
	char *buf = (char *) malloc(slen);
	mg_url_encode(s, buf, slen);
	return buf;
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
// NB: assumes first element in struct is float sort field
int qsort_floatcomp(const void *elem1, const void *elem2)
{
	const float f1 = *(const float *) elem1, f2 = *(const float *) elem2;
    if (f1 < f2)
        return -1;
    return f1 > f2;
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

u4_t kiwi_inet4_d2h(char *inet4_str)
{
	int n;
	u4_t a, b, c, d;
	n = sscanf(inet4_str, "%d.%d.%d.%d", &a, &b, &c, &d);
	if (n != 4) {
		n = sscanf(inet4_str, "::ffff:%d.%d.%d.%d", &a, &b, &c, &d); //IPv4-mapped address
		if (n != 4)
			return 0xffffffff; //IPv6
	}
	return INET4_DTOH(a, b, c, d);
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

void send_data_msg(conn_t *c, bool debug, u1_t cmd, u1_t *bytes, int nbytes)
{
	int size = 4 + sizeof(cmd) + nbytes;
	char *buf = (char *) kiwi_malloc("send_bytes_msg", size);
	char *s = buf;
	int n = sprintf(s, "DAT ");
	if (debug) cprintf(c, "send_data_msg: cmd=%d nbytes=%d size=%d\n", cmd, nbytes, size);
	s += n;
	*s++ = cmd;
	if (nbytes)
		memcpy(s, bytes, nbytes);
	mg_websocket_write(c->mc, WS_OPCODE_BINARY, buf, size);
	kiwi_free("send_bytes_msg", buf);
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
	
	char *buf = str_encode(s);
	free(s);
	send_msg_mc(mc, FALSE, "%s %s=%s", dst, cmd, buf);
	free(buf);
}

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

void print_max_min_f(const char *name, float *data, int len)
{
	int i;
	float max = 1e-38, min = 1e38;
	
	for (i=0; i < len; i++) {
		float s = data[i];
		if (s > max) max = s;
		if (s < min) min = s;
	}
	
	printf("min/max %s: %.0f..%.0f\n", name, min, max);
}

void print_max_min_c(const char *name, TYPECPX *data, int len)
{
	int i;
	float max = 1e-38, min = 1e38;
	
	for (i=0; i < len; i++) {
		float s;
		s = data[i].re;
		if (s > max) max = s;
		if (s < min) min = s;
		s = data[i].im;
		if (s > max) max = s;
		if (s < min) min = s;
	}
	
	printf("min/max %s: %.0f..%.0f\n", name, min, max);
}

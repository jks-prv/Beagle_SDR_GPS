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


void xit(int err)
{
	void exit(int);
	
	fflush(stdout);
	spin_ms(1000);	// needed for syslog messages to be properly recorded
	exit(err);
}

void _panic(const char *str, const char *file, int line)
{
	char *buf;
	asprintf(&buf, "PANIC: \"%s\" (%s, line %d)", str, file, line);

	if (background_mode) {
		syslog(LOG_ERR, "%s\n", buf);
	}
	
	xit(-1);
}

void _sys_panic(const char *str, const char *file, int line)
{
	char *buf;
	asprintf(&buf, "SYS_PANIC: \"%s\" (%s, line %d)", str, file, line);

	if (background_mode) {
		syslog(LOG_ERR, "%s: %m\n", buf);
	}
	
	perror(buf);
	xit(-1);
}

#ifdef MALLOC_DEBUG

#define NMT 1024
struct mtrace_t {
	int i;
	const char *from;
	char *ptr;
	int size;
} mtrace[NMT];

int nmt;

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

void *wrx_malloc(const char *from, size_t size)
{
	if (size > 131072) panic("malloc > 128k");
	void *ptr = malloc(size);
	mt_enter(from, ptr, size);
	return ptr;
}

char *wrx_strdup(const char *from, const char *s)
{
	int sl = strlen(s)+1;
	if (sl == 0 || sl > 1024) panic("strdup size");
	char *ptr = strdup(s);
	mt_enter(from, (void*) ptr, sl);
	return ptr;
}

void wrx_free(const char *from, void *ptr)
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

int wrx_malloc_stat()
{
	return nmt;
}

#endif

void wrx_str_redup(char **ptr, const char *from, const char *s)
{
	int sl = strlen(s)+1;
	if (sl == 0 || sl > 1024) panic("strdup size");
	if (*ptr) wrx_free(from, (void*) *ptr);
	*ptr = strdup(s);
#ifdef MALLOC_DEBUG
	mt_enter(from, (void*) *ptr, sl);
#endif
}

#define VBUF 1024

void lprintf(const char *fmt, ...)
{
	int sl;
	char *s;
	va_list ap;
	
	if ((s = (char*) malloc(VBUF)) == NULL)
		panic("log malloc");

	va_start(ap, fmt);
	vsnprintf(s, VBUF, fmt, ap);
	va_end(ap);
	sl = strlen(s);		// because vsnprintf returns length disregarding limit, not the actual

	if (background_mode) {
		syslog(LOG_INFO, "wrxd: %s", s);
	}

	time_t t;
	char tb[32];
	time(&t);
	ctime_r(&t, tb);
	tb[24]=0;
	printf("%s %s", tb, s);
	free(s);
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

int str2enum(const char *s, const char *strs[], int len)
{
	int i;
	for (i=0; i<len; i++) {
		if (strcasecmp(s, strs[i]) == 0) return i;
	}
	return ENUM_BAD;
}

const char *enum2str(int e, const char *strs[], int len)
{
	if (e < 0 || e >= len) return NULL;
	assert(strs[e] != NULL);
	return (strs[e]);
}

void clr_set_ctrl(u2_t clr, u2_t set)
{
	static u2_t ctrl;
	u2_t _ctrl = ctrl;
	
	ctrl &= ~clr;
	ctrl |= set;
	if (clr) spi_set(CmdCtrlClr, clr);
	if (set) spi_set(CmdCtrlSet, set);
	//printf("clr_set_ctrl: %04x -> %04x\n", _ctrl, ctrl);
}

u2_t getmem(u2_t addr)
{
	SPI_MISO mem;
	
	spi_get_noduplex(CmdGetMem, &mem, 4, addr);
	assert(addr == mem.word[1]);
	
	return mem.word[0];
}

void printmem(const char *str, u2_t addr)
{
	printf("%s %04x: %04x\n", str, addr, (int) getmem(addr));
}

void send_msg(conn_t *c, const char *msg, ...)
{
	va_list ap;
	char *s;

	va_start(ap, msg);
	vasprintf(&s, msg, ap);
	va_end(ap);
printf("send_msg %d: <%s>\n", c->rx_channel, s);
	app_to_web(c, s, strlen(s));
	free(s);
}

// sent direct to mg_connection
void send_msg_mc(struct mg_connection *mc, const char *msg, ...)
{
	va_list ap;
	char *s;

	va_start(ap, msg);
	vasprintf(&s, msg, ap);
	va_end(ap);
printf("send_msg_mc: <%s>\n", s);
	mg_websocket_write(mc, WS_OPCODE_BINARY, s, strlen(s));
	free(s);
}

// DEPRECATED: still in WSPR code

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

float ecpu_use()
{
	typedef struct {
		u1_t f0, g0, f1, g1, f2, g2, f3, g3;
	} ctr_t;
	ctr_t *c;
	SPI_MISO cpu;
	
	if (down || do_fft) return 0;
	spi_get_noduplex(CmdGetCPUCtr, &cpu, sizeof(u2_t[3]));
	c = (ctr_t*) &cpu.word[0];
	u4_t gate = (c->g3 << 24) | (c->g2 << 16) | (c->g1 << 8) | c->g0;
	u4_t free = (c->f3 << 24) | (c->f2 << 16) | (c->f1 << 8) | c->f0;
	spi_set_noduplex(CmdCPUCtrClr);
	return ((float) gate / (float) free * 100);
}

#ifdef EV_MEAS

typedef struct {
	int event, reset;
	const char *s, *s2, *task;
	u4_t tseq, tlast, tepoch;
} ev_t;
#define NEV 4*1024
//#define NEV 64
ev_t evs[NEV];
int evc;

#define NEVT 8
const char *evn[NEVT] = {
	"", "NextTask", "spiLoop", "wf",
};

static u4_t epoch, last_time, tlast[NEVT], triggered;

void ev(int event, const char *s, const char *s2)
{
	int i;
	ev_t *e = &evs[evc++];
	u4_t now = timer_us();
	int reset=0;
	u64_t freeS2 = (u64_t) s2 & 1;
	s2 = (char*) ((u64_t) s2 & ~1);
	
	if (!triggered && (event == EV_TRIGGER)) {
		epoch = last_time = now;
		for (i=0; i<NEVT; i++) tlast[i] = now;
		evc=0;
		triggered=1;
		if (freeS2) free((void*) s2);
		return;
	}
	
	if (!triggered) {
		if (freeS2) free((void*) s2);
		return;
	}
	
	if (event < 0) {
		event = -event;
		tlast[event] = now;
		reset=1;
	}
	
	e->event = event;
	e->reset = reset;
	e->s = s;
	e->s2 = s2;
	e->tseq = now - last_time;
	e->tlast = now - tlast[event];
	e->tepoch = now - epoch;
	e->task = TaskName();
	
	if (evc == NEV) {
		for (i=0; i<NEV; i++) {
			e = &evs[i];
			printf("%4d %8s%c %7.3f %7.3f %9.3f %16s, %16s %s\n", i, evn[e->event], e->reset? '*':' ',
				(float) e->tlast / 1000, (float) e->tseq / 1000, (float) e->tepoch / 1000,
				e->task, e->s, e->s2);
		}
		exit(0);
	}
	
	last_time = now;
}

char *evprintf(const char *fmt, ...)
{
	char *ret;
	va_list ap;
	
	va_start(ap, fmt);
	vasprintf(&ret, fmt, ap);
	va_end(ap);
	
	// hack: mark malloced string for later free
	assert(((u64_t) ret & 1) == 0);
	return (char*) ((u64_t) ret | 1);
}
#endif

#ifdef REG_RECORD

typedef struct {
	t_hr_stamp stamp;
	u2_t addr;
	u1_t data;
	reg_type_t type;
	u4_t dup;
	u4_t time;
	char *str;
} hr_buf_t;

#define NCHAN 2
#define NHP (256*1024)

static hr_buf_t hr_buf[NCHAN][NHP];
static u4_t nhp[NCHAN] = {0, 0};
static t_hr_stamp hr_stamp;
static u4_t last_iCount = 0;

void reg_record(int chan, reg_type_t type, u2_t addr, u1_t d, u4_t time, char *str)
{
	assert(chan < NCHAN);
	hr_buf_t *hp = &hr_buf[chan][nhp[chan]];
	
	if (type == REG_RESET) { nhp[chan] = 0; return; }

	if (nhp[chan] < NHP) {
		//if ((nhp[chan] > 0) && (addr == (hp-1)->addr) && (d == (hp-1)->data) &&
		//	((type != REG_STR) && ((hp-1)->type == type))) {
		if (0) {
			(hp-1)->dup++;
		} else {
			hp->stamp = hr_stamp;
			hp->addr = addr;
			hp->type = type;
			hp->data = d;
			hp->dup = 1;
			hp->time = time;
			hp->str = str;
			hp++;
			nhp[chan]++;
		}
	}
	if (nhp[chan] == NHP) {
		printf("NHP %d *************************************************\n", NHP);
		nhp[chan]++;
	}
}

// stamp the recorded hpib bus cycles with additional info
void reg_stamp(int write, u4_t iCount, u2_t rPC, u1_t n_irq, u4_t irq_masked)
{
#if 0
	hr_buf_t *hp = &hr_buf[0][nhp[chan]];

	hr_stamp.iCount = iCount;
	hr_stamp.rPC = rPC;
	hr_stamp.n_irq = n_irq;
	hr_stamp.irq_masked = irq_masked;
	
	// record interrupt events
	if (write && n_irq) {
		if (nhp[chan] < NHP) {
			hp->stamp = hr_stamp;
			hp++;
			nhp[chan]++;
		}
	}
#endif
}

t_hr_stamp *reg_get_stamp()
{
	return &hr_stamp;
}

void reg_dump(u1_t chan, callback_t rdecode, callback_t wdecode, callback_t sdecode)
{
	hr_buf_t *p;
	
	printf("NHP %d\n", nhp[chan]);
	for (p = &hr_buf[chan][0]; p != &hr_buf[chan][nhp[chan]]; p++) {
#if 0
		printf("%07d +%5d @%04x%c #%4d ", p->stamp.iCount, p->stamp.iCount - last_iCount, p->stamp.rPC,
			p->stamp.irq_masked? '*':' ', p->dup);
		last_iCount = p->stamp.iCount;
#endif
		if (p->stamp.n_irq) {
			//printf("---- IRQ ----\n");
		} else {
			switch (p->type) {
				case REG_READ:  rdecode(p->addr, p->data, p->dup, p->time, p->str); break;
				case REG_WRITE: wdecode(p->addr, p->data, p->dup, p->time, p->str); break;
				case REG_STR:   sdecode(p->addr, p->data, p->dup, p->time, p->str); break;
				default: break;
			}
			//printf("\n");
		}
	}
}

void reg_cmp(callback_t adecode)
{
	int max, min, n=0;
	hr_buf_t *p, *q, *r;
	
	printf("NHP %d %d %d\n", nhp[0], nhp[1], nhp[0]-nhp[1]);
	max = nhp[0]>nhp[1]? nhp[0]:nhp[1];
	min = nhp[0]<nhp[1]? nhp[0]:nhp[1];
	for (p = &hr_buf[0][0], q = &hr_buf[1][0]; p != &hr_buf[0][max]; p++, q++, n++) {
		if ((n < min) && ((p->addr != q->addr) || (p->data != q->data) || (p->type != q->type))) {
			printf("%d: ", n);
			if (p->addr != q->addr) {
				printf("addr "); adecode(p->addr, 0, p->type, p->time, p->str);
				printf(":"); adecode(q->addr, 0, q->type, p->time, p->str); printf(" ");
			} else {
				printf("addr "); adecode(p->addr, 0, p->type, p->time, p->str); printf(" ");
			}
			if (p->data != q->data) {
				printf("data %02x:%02x ", p->data, q->data);
			} else {
				printf("data %02x ", p->data);
			}
			if (p->type != q->type) {
				printf("type %d:%d ", p->type, q->type);
			} else {
				printf("type %d ", p->type);
			}
			printf("\n");
		}
		if (n >= min) {
			r = nhp[0]>nhp[1]? p:q;
			printf("%d: ", n);
			printf("addr "); adecode(r->addr, 0, r->type, p->time, p->str); printf(" ");
			printf("data %02x ", r->data);
			printf("type %d ", r->type);
			printf("(no cmp)\n");
		}
	}
}

#endif

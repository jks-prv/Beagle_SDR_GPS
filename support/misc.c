/*
--------------------------------------------------------------------------------
This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Library General Public
License as published by the Free Software Foundation; either
version 2 of the License, or (at your option) any later version.
This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Library General Public License for more details.
You should have received a copy of the GNU Library General Public
License along with this library; if not, write to the
Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
Boston, MA  02110-1301, USA.
--------------------------------------------------------------------------------
*/

// Copyright (c) 2014-2016 John Seamons, ZL/KF6VO

#include "types.h"
#include "config.h"
#include "kiwi.h"
#include "misc.h"
#include "str.h"
#include "web.h"
#include "spi.h"
#include "cfg.h"
#include "coroutines.h"
#include "net.h"

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

#ifdef HOST
	#include <wait.h>
#endif

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
	//printf("ctrl_clr_set(0x%04x, 0x%04x) ctrl_get=0x%04x\n", clr, set, ctrl_get());
}

u2_t getmem(u2_t addr)
{
	static SPI_MISO mem;
	
	memset(mem.word, 0x55, sizeof(mem.word));
	spi_get_noduplex(CmdGetMem, &mem, 4, addr);
	assert(addr == mem.word[1]);
	
	return mem.word[0];
}

int child_task(int poll_msec, funcP_t func, void *param)
{
	pid_t child;
	scall("fork", (child = fork()));
	
	if (child == 0) {
		TaskForkChild();
		func(param);	// this function should exit() with some other value if it wants
		exit(0);
	}
	
	int pid, status;
	do {
		TaskSleep(poll_msec);
		pid = waitpid(child, &status, WNOHANG);
		if (pid < 0) sys_panic("child_task waitpid");
	} while (pid == 0);

	int exited = WIFEXITED(status);
	int exit_status = WEXITSTATUS(status);
	//printf("child_task exited=%d exit_status=%d status=0x%08x\n", exited, exit_status, status);
	return (exited? exit_status : -1);
}

#define NON_BLOCKING_POLL_MSEC 50000

// child task that calls a function for every chunk of non-blocking command input read
static void _non_blocking_cmd(void *param)
{
	int n, func_rv = 0;
	nbcmd_args_t *args = (nbcmd_args_t *) param;
	args->bp = (char *) malloc(args->bsize);
	//printf("_non_blocking_cmd <%s> bsize %d\n", args->cmd, args->bsize);

	FILE *pf = popen(args->cmd, "r");
	if (pf == NULL) exit(-1);
	int pfd = fileno(pf);
	if (pfd <= 0) exit(-1);
	fcntl(pfd, F_SETFL, O_NONBLOCK);

	do {
		TaskSleep(NON_BLOCKING_POLL_MSEC);
		n = read(pfd, args->bp, args->bsize);
		if (n > 0) {
			args->bc = n;
			func_rv = args->func((void *) args);
			continue;
		}
	} while (n == -1 && errno == EAGAIN);

	free(args->bp);
	pclose(pf);

	exit(func_rv);
}

// like non_blocking_cmd() below, but run in a child process because pclose() can block
// for long periods of time under certain conditions
int non_blocking_cmd_child(const char *cmd, funcPR_t func, int param, int bsize)
{
	nbcmd_args_t *args = (nbcmd_args_t *) malloc(sizeof(nbcmd_args_t));
	args->cmd = cmd;
	args->func = func;
	args->func_param = param;
	args->bsize = bsize;
	int status = child_task(SEC_TO_MSEC(1), _non_blocking_cmd, (void *) args);
	free(args);
	return status;
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
		TaskSleep(NON_BLOCKING_POLL_MSEC);
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

// non_blocking_cmd() broken down into separately callable routines in case you have to
// do something with each chunk of data read
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
		TaskSleep(NON_BLOCKING_POLL_MSEC);
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

void send_mc(conn_t *c, char *s, int slen)
{
	if (c->mc == NULL) {
		clprintf(c, "send_mc: c->mc is NULL\n");
		clprintf(c, "send_mc: CONN-%d %p valid=%d type=%d [%s] auth=%d KA=%d KC=%d mc=%p rx=%d magic=0x%x ip=%s:%d other=%s%d %s\n",
			c->self_idx, c, c->valid, c->type, streams[c->type].uri, c->auth, c->keep_alive, c->keepalive_count, c->mc, c->rx_channel,
			c->magic, c->remote_ip, c->remote_port, c->other? "CONN-":"", c->other? c->other-conns:0, c->stop_data? "STOP":"");
		return;
	}
	mg_websocket_write(c->mc, WS_OPCODE_BINARY, s, slen);
}

void send_msg(conn_t *c, bool debug, const char *msg, ...)
{
	va_list ap;
	char *s;

	va_start(ap, msg);
	vasprintf(&s, msg, ap);
	va_end(ap);
	if (debug) cprintf(c, "send_msg: %p <%s>\n", c->mc, s);
	send_mc(c, s, strlen(s));
	free(s);
}

void send_msg_data(conn_t *c, bool debug, u1_t cmd, u1_t *bytes, int nbytes)
{
	int size = 4 + sizeof(cmd) + nbytes;
	char *buf = (char *) kiwi_malloc("send_bytes_msg", size);
	char *s = buf;
	int n = sprintf(s, "DAT ");
	if (debug) cprintf(c, "send_msg_data: cmd=%d nbytes=%d size=%d\n", cmd, nbytes, size);
	s += n;
	*s++ = cmd;
	if (nbytes)
		memcpy(s, bytes, nbytes);
	send_mc(c, buf, size);
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

void send_msg_encoded_mc(struct mg_connection *mc, const char *dst, const char *cmd, const char *fmt, ...)
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

char *kiwi_authkey()
{
	int fd;
	scall("open /dev/urandom", (fd = open("/dev/urandom", O_RDONLY)));
	u4_t u[8];
	assert(read(fd, u, 32) == 32);
	char *s;
	asprintf(&s, "%08x%08x%08x%08x%08x%08x%08x%08x", u[0], u[1], u[2], u[3], u[4], u[5], u[6], u[7]);
	return s;
}

struct print_max_min_int_t {
	int min_i, max_i;
	double min_f, max_f;
	int min_idx, max_idx;
};

static print_max_min_int_t *print_max_min_init(void **state)
{
	print_max_min_int_t **pp = (print_max_min_int_t **) state;
	if (*pp == NULL) {
		*pp = (print_max_min_int_t *) malloc(sizeof(print_max_min_int_t));
		print_max_min_int_t *p = *pp;
		memset(p, 0, sizeof(*p));
		p->min_i = 0x7fffffff; p->max_i = 0x80000000;
		p->min_f = 1e38; p->max_f = -1e38;
		p->min_idx = p->max_idx = -1;
	}
	return *pp;
}

void print_max_min_stream_i(void **state, const char *name, int index, int nargs, ...)
{
	va_list ap;
	va_start(ap, nargs);
	print_max_min_int_t *p = print_max_min_init(state);
	bool update = false;

	for (int i=0; i < nargs; i++) {
		int arg_i = va_arg(ap, int);
		if (arg_i > p->max_i) {
			p->max_i = arg_i;
			p->max_idx = index;
			update = true;
		}
		if (arg_i < p->min_i) {
			p->min_i = arg_i;
			p->min_idx = index;
			update = true;
		}
	}
	
	if (update)
		printf("min/max %s: %d(%d)..%d(%d)\n", name, p->min_i, p->min_idx, p->max_i, p->max_idx);

	va_end(ap);
}

void print_max_min_stream_f(void **state, const char *name, int index, int nargs, ...)
{
	va_list ap;
	va_start(ap, nargs);
	print_max_min_int_t *p = print_max_min_init(state);
	bool update = false;

	for (int i=0; i < nargs; i++) {
		double arg_f = va_arg(ap, double);
		if (arg_f > p->max_f) {
			p->max_f = arg_f;
			p->max_idx = index;
			update = true;
		}
		if (arg_f < p->min_f) {
			p->min_f = arg_f;
			p->min_idx = index;
			update = true;
		}
	}
	
	if (update)
		//printf("min/max %s: %e(%d)..%e(%d)\n", name, p->min_f, p->min_idx, p->max_f, p->max_idx);
		printf("min/max %s: %f(%d)..%f(%d)\n", name, p->min_f, p->min_idx, p->max_f, p->max_idx);

	va_end(ap);
}

void print_max_min_i(const char *name, int *data, int len)
{
	int i;
	int max = (int) 0x80000000U, min = (int) 0x7fffffffU;
	int max_idx, min_idx;
	
	for (i=0; i < len; i++) {
		int s = data[i];
		if (s > max) { max = s; max_idx = i; }
		if (s < min) { min = s; min_idx = i; }
	}
	
	printf("min/max %s: %d(%d)..%d(%d)\n", name, min, min_idx, max, max_idx);
}

void print_max_min_f(const char *name, float *data, int len)
{
	int i;
	float max = -1e38, min = 1e38;
	int max_idx, min_idx;
	
	for (i=0; i < len; i++) {
		float s = data[i];
		if (s > max) { max = s; max_idx = i; }
		if (s < min) { min = s; min_idx = i; }
	}
	
	//printf("min/max %s: %e(%d)..%e(%d)\n", name, min, min_idx, max, max_idx);
	printf("min/max %s: %f(%d)..%f(%d)\n", name, min, min_idx, max, max_idx);
}

void print_max_min_c(const char *name, TYPECPX *data, int len)
{
	int i;
	float max = -1e38, min = 1e38;
	
	for (i=0; i < len; i++) {
		float s;
		s = data[i].re;
		if (s > max) max = s;
		if (s < min) min = s;
		s = data[i].im;
		if (s > max) max = s;
		if (s < min) min = s;
	}
	
	printf("min/max %s: %e..%e\n", name, min, max);
}

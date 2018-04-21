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
#include "debug.h"

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
	if (i == NMT) {
		printf("mt_remove \"%s\"\n", from);
		panic("mt_remove not found");
	}
	nmt--;
}

#define	MALLOC_MAX	PHOTO_UPLOAD_MAX_SIZE

void *kiwi_malloc(const char *from, size_t size)
{
	//if (size > MALLOC_MAX) panic("malloc > MALLOC_MAX");
	kmprintf(("kiwi_malloc-1 \"%s\" %d\n", from, size));
	void *ptr = malloc(size);
	memset(ptr, 0, size);
	int i = mt_enter(from, ptr, size);
	kmprintf(("kiwi_malloc-2 \"%s\" #%d %d %p\n", from, i, size, ptr));
	return ptr;
}

void *kiwi_realloc(const char *from, void *ptr, size_t size)
{
	//if (size > MALLOC_MAX) panic("malloc > MALLOC_MAX");
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

void ctrl_positive_pulse(u2_t bits)
{
	spi_set_noduplex(CmdCtrlClr, bits);
	spi_set_noduplex(CmdCtrlSet, bits);
	spi_set_noduplex(CmdCtrlClr, bits);
}

stat_reg_t stat_get()
{
    static SPI_MISO status;
    stat_reg_t stat;
    
    spi_get_noduplex(CmdGetStatus, &status, sizeof(stat));
    stat.word = status.word[0];

    return stat;
}

u2_t getmem(u2_t addr)
{
	static SPI_MISO mem;
	
	memset(mem.word, 0x55, sizeof(mem.word));
	spi_get_noduplex(CmdGetMem, &mem, 4, addr);
	assert(addr == mem.word[1]);
	
	return mem.word[0];
}

void printmem(const char *str, u2_t addr)
{
	printf("%s %04x: %04x\n", str, addr, (int) getmem(addr));
}

void send_msg_buf(conn_t *c, char *s, int slen)
{
    if (c->internal_connection) {
        //clprintf(c, "send_msg_buf: internal_connection <%s>\n", s);
    } else {
        if (c->mc == NULL) {
            /*
            clprintf(c, "send_msg_buf: c->mc is NULL\n");
            clprintf(c, "send_msg_buf: CONN-%d %p valid=%d type=%d [%s] auth=%d KA=%d KC=%d mc=%p rx=%d magic=0x%x ip=%s:%d other=%s%d %s\n",
                c->self_idx, c, c->valid, c->type, streams[c->type].uri, c->auth, c->keep_alive, c->keepalive_count, c->mc, c->rx_channel,
                c->magic, c->remote_ip, c->remote_port, c->other? "CONN-":"", c->other? c->other-conns:0, c->stop_data? "STOP":"");
            */
            return;
        }
        mg_websocket_write(c->mc, WS_OPCODE_BINARY, s, slen);
    }
}

void send_msg(conn_t *c, bool debug, const char *msg, ...)
{
	va_list ap;
	char *s;

	va_start(ap, msg);
	vasprintf(&s, msg, ap);
	va_end(ap);
	if (debug) cprintf(c, "send_msg: %p <%s>\n", c->mc, s);
	send_msg_buf(c, s, strlen(s));
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
	send_msg_buf(c, buf, size);
	kiwi_free("send_bytes_msg", buf);
}

// sent direct to mg_connection -- only directly called in a few places where conn_t isn't available
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

void send_msg_encoded(conn_t *conn, const char *dst, const char *cmd, const char *fmt, ...)
{
	va_list ap;
	char *s;

	if (cmd == NULL || fmt == NULL) return;
	
	va_start(ap, fmt);
	vasprintf(&s, fmt, ap);
	va_end(ap);
	
	char *buf = kiwi_str_encode(s);
	free(s);
	send_msg(conn, FALSE, "%s %s=%s", dst, cmd, buf);
	free(buf);
}

// sent direct to mg_connection -- only directly called in a few places where conn_t isn't available
// caution: never use an mprint() here as this will result in a loop
void send_msg_mc_encoded(struct mg_connection *mc, const char *dst, const char *cmd, const char *fmt, ...)
{
	va_list ap;
	char *s;

	if (cmd == NULL || fmt == NULL) return;
	
	va_start(ap, fmt);
	vasprintf(&s, fmt, ap);
	va_end(ap);
	
	char *buf = kiwi_str_encode(s);
	free(s);
	send_msg_mc(mc, FALSE, "%s %s=%s", dst, cmd, buf);
	free(buf);
}

void input_msg_internal(conn_t *conn, const char *fmt, ...)
{
	va_list ap;
	char *s;

	if (fmt == NULL) return;
	
	va_start(ap, fmt);
	vasprintf(&s, fmt, ap);
	va_end(ap);
	
    assert(conn->internal_connection);
	nbuf_allocq(&conn->c2s, s, strlen(s));
	free(s);
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

void print_max_min_u1(const char *name, u1_t *data, int len)
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

int bits_required(u4_t v)
{
	int nb;
	
	if (v == 0) return 1;
	
	for (nb = 0; v != 0; nb++)
		v >>= 1;
	
	return nb;
}

u4_t snd_hdr[8];

int snd_file_open(const char *fn, int nchans, double srate)
{
    int fd = open(fn, O_WRONLY|O_CREAT|O_TRUNC, 0644);
    if (fd < 0) return fd;
    int srate_i = round(srate);
    snd_hdr[0] = FLIP32(0x2e736e64);    // ".snd"
    snd_hdr[1] = FLIP32(sizeof(snd_hdr));   // header size
    snd_hdr[2] = FLIP32(0xffffffff);    // unknown filesize
    snd_hdr[3] = FLIP32(3);             // 16-bit linear PCM
    snd_hdr[4] = FLIP32(srate_i);       // sample rate
    snd_hdr[5] = FLIP32(nchans*NIQ);    // number of channels
    snd_hdr[6] = FLIP32(0);
    snd_hdr[7] = FLIP32(0);
    write(fd, snd_hdr, sizeof(snd_hdr));
    return fd;
}

FILE *pgm_file_open(const char *fn, int *offset, int width, int height, int depth)
{
    FILE *fp = fopen(fn, "w");
    if (fp != NULL) {
        *offset = fprintf(fp, "P5 %d ", width);
        // reserve space for height (written later with pgm_file_height()) using a fixed-length field
        fprintf(fp, "%6d %d\n", height, depth);
    }
    return fp;
}

void pgm_file_height(FILE *fp, int offset, int height)
{
    fflush(fp);
    fseek(fp, offset, SEEK_SET);
    if (height > 999999) {
        height = 999999;
        lprintf("pgm_file_height: height limited to 999999!\n");
    }
    fprintf(fp, "%6d", height);
}

static const char *field = "ABCDEFGHIJKLMNOPQR";
static const char *square = "0123456789";
static const char *subsquare = "abcdefghijklmnopqrstuvwx";

void grid_to_latLon(char *grid, latLon_t *loc)
{
	double lat, lon;
	char c;
	int slen = strlen(grid);
	
	loc->lat = loc->lon = 999.0;
	if (slen < 4) return;
	
	c = tolower(grid[0]);
	if (c < 'a' || c > 'r') return;
	lon = (c-'a')*20 - 180;

	c = tolower(grid[1]);
	if (c < 'a' || c > 'r') return;
	lat = (c-'a')*10 - 90;

	c = grid[2];
	if (c < '0' || c > '9') return;
	lon += (c-'0') * SQ_LON_DEG;

	c = grid[3];
	if (c < '0' || c > '9') return;
	lat += (c-'0') * SQ_LAT_DEG;

	if (slen != 6) {	// assume center of square (i.e. "....ll")
		lon += SQ_LON_DEG /2.0;
		lat += SQ_LAT_DEG /2.0;
	} else {
		c = tolower(grid[4]);
		if (c < 'a' || c > 'x') return;
		lon += (c-'a') * SUBSQ_LON_DEG;

		c = tolower(grid[5]);
		if (c < 'a' || c > 'x') return;
		lat += (c-'a') * SUBSQ_LAT_DEG;

		lon += SUBSQ_LON_DEG /2.0;	// assume center of sub-square (i.e. "......44")
		lat += SUBSQ_LAT_DEG /2.0;
	}

	loc->lat = lat;
	loc->lon = lon;
	//wprintf("GRID %s%s = (%f, %f)\n", grid, (slen != 6)? "[ll]":"", lat, lon);
}

int latLon_to_grid6(latLon_t *loc, char *grid6)
{
	int i;
	double r, lat, lon;
	
	// longitude
	lon = loc->lon + 180.0;
	if (lon < 0 || lon >= 360.0) return -1;
	i = (int) lon / FLD_DEG_LON;
	grid6[0] = field[i];
	r = lon - (i * FLD_DEG_LON);
	
	i = (int) floor(r / SQ_LON_DEG);
	grid6[2] = square[i];
	r = r - (i * SQ_LON_DEG);
	
	i = (int) floor(r * (SUBSQ_PER_SQ / SQ_LON_DEG));
	grid6[4] = subsquare[i];
	
	// latitude
	lat = loc->lat + 90.0;
	if (lat < 0 || lat >= 180.0) return -1;
	i = (int) lat / FLD_DEG_LAT;
	grid6[1] = field[i];
	r = lat - (i * FLD_DEG_LAT);
	
	i = (int) floor(r / SQ_LAT_DEG);
	grid6[3] = square[i];
	r = r - (i * SQ_LAT_DEG);
	
	i = (int) floor(r * (SUBSQ_PER_SQ / SQ_LAT_DEG));
	grid6[5] = subsquare[i];
	
	return 0;
}

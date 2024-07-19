//////////////////////////////////////////////////////////////////////////
// Homemade GPS Receiver
// Copyright (C) 2013 Andrew Holme
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
// http://www.holmea.demon.co.uk/GPS/Main.htm
//////////////////////////////////////////////////////////////////////////

// Copyright (c) 2014-2017 John Seamons, ZL4VO/KF6VO

#include "types.h"
#include "kiwi.h"
#include "config.h"
#include "valgrind.h"
#include "mem.h"
#include "misc.h"
#include "str.h"
#include "coroutines.h"
#include "debug.h"
#include "peri.h"
#include "spi.h"
#include "shmem.h"      // SIG_SETUP_TRAMP

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <setjmp.h>
#include <errno.h>
#include <sched.h>
#include <math.h>
#include <sys/time.h>
#include <sys/resource.h>

// override
#undef D_STMT
#define D_STMT(x) x;

/*

	For our private thread implementation there are two methods for setting the initial sp & pc
	of a new thread. The original scheme from Andrew Holme depends on knowing the internal
	structure of a jmp_buf (cpu architecture dependent). To complicate matters, on Beagle Debian 8
	glibc started using "pointer mangling" as a "security by obscurity" measure.
	
	Until we figured out that it was just a simple xor, and that we could reverse-engineer the key
	pretty easily, we developed a new scheme for setting the initial sp & pc using the signal calls
	sigaltstack() and sigaction() with the SA_ONSTACK flag. This works fine. 
	
	The interest in moving to Debian 8 was motivated by needing a more recent valgrind that didn't
	have a bug interfacing with gdb. And also that SPIDEV DMA is fixed in the more recent Debian 8
	kernel.

	Both methods are included in the code. But note that the signal method doesn't work with valgrind
	even on Debian 7 (reasons unknown). The jmp_buf method with de-mangling is currently the default
	so that those few Kiwis still running Debian 7 will continue to work.

*/

#ifdef DEVSYS
	//#define SETUP_TRAMP_USING_JMP_BUF
#else
    #ifdef CPU_AM3359
        // 10/6/2019 this seems broken all of a sudden?!?
	    //#define SETUP_TRAMP_USING_JMP_BUF
    #endif
    
    #ifdef CPU_AM5729
    #endif
#endif

#if defined(HOST) && defined(USE_VALGRIND)
	#include <valgrind/valgrind.h>
#endif

#include "sanitizer.h"
#if defined(HOST) && defined(USE_ASAN)
	#ifdef SETUP_TRAMP_USING_JMP_BUF
		#undef SETUP_TRAMP_USING_JMP_BUF
	#endif
#endif

#define LOCK_CHECK_HANG
#define LOCK_HUNG_TIME 3

struct TASK;

struct TaskLL_t {
	struct TaskLL_t *next, *prev;
	TASK *t;
};

struct TaskQ_t {
	int p;
	TaskLL_t tll;
	TaskLL_t *last_run;
	int count, runnable;
};

// just for debugging -- easier to read values in gdb
struct run_t {
	int t;
	int v;
	const char *n;
	int p;
	int r;
} run[MAX_TASKS];

#if 1
    #define STACK_SIZE_U64_T	    (8*K)
    #define STACK_SIZE_REG          1       // 8k   largest is GPS channel with Galileo ~27%
    #define STACK_SIZE_MED          4       // 32k  WF is ~25%
    #define STACK_SIZE_LARGE        8       // 64k  DRM
#else
    #define STACK_SIZE_U64_T	    (16*K)
    #define STACK_SIZE_REG          1       // 16k
    #define STACK_SIZE_MED          2       // 32k
    #define STACK_SIZE_LARGE        4       // 64k
#endif

struct task_stack_t {
	u64_t elem[STACK_SIZE_U64_T];
} __attribute__ ((aligned(256)));

#define N_REG_STACK_EL      (REG_STACK_TASKS * STACK_SIZE_REG)
#define N_MED_STACK_EL      (MED_STACK_TASKS * STACK_SIZE_MED)
#define N_LARGE_STACK_EL    (LARGE_STACK_TASKS * STACK_SIZE_LARGE)
#define N_STACK_EL          (N_REG_STACK_EL + N_MED_STACK_EL + N_LARGE_STACK_EL)

static task_stack_t task_stacks[N_STACK_EL];
static int stack_map[MAX_TASKS], stack_nel[MAX_TASKS];

struct ctx_t {
	int id;
	bool init;
    u64_t *stack, *stack_last;
    u4_t stack_size_u64;            // in STACK_SIZE_U64_T (not bytes)
    u4_t stack_size_bytes;
    bool valgrind_stack_reg;
	int valgrind_stack_id;
#ifdef USE_ASAN
	void *fake_stack;
#endif
	union {
		jmp_buf jb;
		struct {
			#if defined(__x86_64__)
				u4_t x1, fp, sp, x2[4], pc;
			#endif
			#if defined(__arm__) || defined(__aarch64__)
				u4_t v[6], sl, fp, sp, pc;
			#endif
		};
	};
};

struct TASK {
	TaskLL_t tll;
	int id;
	u4_t priority;
	const char *name, *where;
	struct {
	    lock_t *hold, *wait;
	    TASK *next, *prev;
	    u4_t token;
	    bool waiting;
	} lock;
	bool valid, stopped, wakeup, sleeping, pending_sleep, busy_wait, long_run;
	char lock_marker;
	u4_t flags;
	u4_t saved_priority;

	TASK *interrupted_task;
	s64_t deadline;
	u4_t *wakeup_test;
	u4_t run, cmds;
	#define N_REASON 64
	char reason[N_REASON];

	TaskQ_t *tq;
	ctx_t *ctx;
	funcP_t funcP;
	void *create_param;
	void  *wake_param;
	u64_t tstart_us;
	#define N_HIST 12
	u4_t usec, longest, hist[N_HIST];
	u64_t pending_usec;
	const char *long_name;
	u4_t minrun;
	u64_t minrun_start_us;
	u4_t s1_func, s2_func;
	int stat1, stat2;
	const char *units1, *units2;
	u4_t wu_count, no_run_same;
	u_int64_t last_pc;
	u4_t last_run_time, last_last_run_time;
	u4_t spi_retry;
	int stack_hiwat;
	void *user_param;
};

static bool task_package_init;
static int max_task;
static TASK Tasks[MAX_TASKS], *cur_task, *last_task_run, *busy_helper_task, *itask;
static ctx_t ctx[MAX_TASKS]; 
static TaskQ_t TaskQ[NUM_PRIORITY];
static u64_t last_dump;
static u4_t idle_us;
static u4_t task_all_hist[N_HIST];
static u4_t previous_prio_inversion;

static int itask_tid;
static u64_t itask_last_tstart;


// NB: These use static buffers. The intent is that these are called as args in other printfs (short lifetime).

#define NTASK_BUF   64
static char task_s_buf[2][NTASK_BUF];

#define task_s(tp)      _task_s(tp, task_s_buf[0])
#define task_s2(tp)     _task_s(tp, task_s_buf[1])

static char *_task_s(TASK *tp, char *bp)
{
    assert(tp);
    if (tp->lock.wait != NULL || tp->lock.hold != NULL)
        snprintf(bp, NTASK_BUF, "%s:P%d:T%03d|K%d", tp->name? tp->name:"?", tp->priority, tp->id, tp->lock.token);
    else
        snprintf(bp, NTASK_BUF, "%s:P%d:T%03d", tp->name? tp->name:"?", tp->priority, tp->id);
    return bp;
}

char *Task_s(int id)
{
    if (id == -1) id = cur_task->id;
    TASK *t = Tasks + id;
    return task_s(t);
}

#define task_ls(tp)     _task_ls(tp, task_s_buf[0])
#define task_ls2(tp)    _task_ls(tp, task_s_buf[1])

static char *_task_ls(TASK *tp, char *bp)
{
    if (tp->lock.wait != NULL || tp->lock.hold != NULL)
        snprintf(bp, NTASK_BUF, "%s:P%d:T%03d(%s)|K%d", tp->name? tp->name:"?", tp->priority, tp->id, tp->where? tp->where : "-", tp->lock.token);
    else
        snprintf(bp, NTASK_BUF, "%s:P%d:T%03d(%s)", tp->name? tp->name:"?", tp->priority, tp->id, tp->where? tp->where : "-");
    return bp;
}

char *Task_ls(int id)
{
    if (id == -1) id = cur_task->id;
    TASK *t = Tasks + id;
    return task_ls(t);
}


// never got this to work
// should try explicit run queues
//#define USE_RUNNABLE

#ifdef USE_RUNNABLE
void runnable(TaskQ_t *tq, int chg)
{
	#ifdef USE_RUNNABLE
		int t_runnable = tq->runnable;
		if(!(t_runnable >= 0 && t_runnable <= tq->count && t_runnable <= MAX_TASKS))
		assert_dump(t_runnable >= 0 && t_runnable <= tq->count && t_runnable <= MAX_TASKS);
	
		tq->runnable += chg;
	
		int p;
		for (p = HIGHEST_PRIORITY; p >= LOWEST_PRIORITY; p--) {
			int ckrun=0;
			TaskQ_t *head = &TaskQ[p];
			TaskLL_t *tll = head->tll.next;
			while (tll) {
				if (!tll->t->stopped) ckrun++;
				tll = tll->next;
			}
			if (ckrun != head->runnable)
			assert_dump(ckrun == head->runnable);
		}
	
		t_runnable = tq->runnable;
		if(!(t_runnable >= 0 && t_runnable <= tq->count && t_runnable <= MAX_TASKS))
		assert_dump(t_runnable >= 0 && t_runnable <= tq->count && t_runnable <= MAX_TASKS);
	#endif
}
#else
    #define runnable(tq, chg)
#endif

#define RUNNABLE_YES(tp) \
    (tp)->stopped = FALSE; \
    run[(tp)->id].r = 1; \
    runnable((tp)->tq, 1); \
    (tp)->sleeping = FALSE; \
    (tp)->wakeup = TRUE;

#define RUNNABLE_NO(tp, chg) \
    (tp)->stopped = TRUE; \
    run[(tp)->id].r = 0; \
    runnable((tp)->tq, chg); \
    (tp)->sleeping = TRUE; \
    (tp)->wakeup = FALSE;

static void TenQ(TASK *t, int priority)
{
	t->priority = priority;

	TaskQ_t *tq = &TaskQ[t->priority];
	TaskLL_t *cur = &t->tll, *head = &tq->tll, *prev = head->next;
	if (prev) prev->prev = cur;
	cur->next = prev;
	cur->prev = head;
	head->next = cur;
	tq->count++;

	if (!t->stopped) runnable(tq, 1);

	t->tq = tq;
}

static void TdeQ(TASK *t)
{
	TaskQ_t *tq = t->tq;
	assert(tq == &TaskQ[t->priority]);

	TaskLL_t *next = t->tll.next, *prev = t->tll.prev;
	if (next) next->prev = prev;
	prev->next = next;
	t->tll.next = t->tll.prev = NULL;
	
	if (&t->tll == tq->last_run) {
	    //lprintf("### TdeQ: removing %s as last run on TaskQ\n", task_s(t));
	    tq->last_run = NULL;
	} else {
	    //lprintf("### TdeQ: NOT removing %s as last run on TaskQ\n", task_s(t));
	}

	if (!t->stopped) runnable(tq, -1);
	tq->count--;
}

// Print per-task accumulated usec runtime since last dump.
// NB: all these prints take so long that the "max mS" of the current task (usually the web server)
// will appear to go into the LRUN state.
static int soft_fail;

void TaskDump(u4_t flags)
{
	int i, j;
	TASK *t;
	u64_t now_us = timer_us64();
	u4_t elapsed = now_us - last_dump;
	last_dump = now_us;
	float f_elapsed = ((float) elapsed) / 1e6;
	float f_sum = 0;
	float f_idle = ((float) idle_us) / 1e6;
	idle_us = 0;
	u4_t printf_type = flags & PRINTF_FLAGS;
    NextTask("TaskDump");
	
	TASK *ct = cur_task;

	int tused = 1;      // count main() because it doesn't have a ctx[].init set
	for (i = TID_FIRST; i <= max_task; i++) {
		t = Tasks + i;
		if (t->valid && ctx[i].init) tused++;
		if (flags & TDUMP_CLR_HIST)
		    memset(t->hist, 0, sizeof(t->hist));
	}

    if (flags & TDUMP_CLR_HIST) {
        memset(task_all_hist, 0, sizeof(task_all_hist));
        lfprintf(printf_type, "HIST: cleared\n");
    }

	ct->flags |= CTF_NO_CHARGE;     // don't charge the current task with the time to print all this

	lfprintf(printf_type, "\n");
	#ifdef USE_ASAN
	    const char *asan_used = ", USE_ASAN";
	#else
	    const char *asan_used = "";
	#endif
	lfprintf(printf_type, "TASKS: used %d/%d(%d|%d|%d), soft_fail %d, spi_retry %d, spi_delay %d%s\n",
	    tused, MAX_TASKS, REG_STACK_TASKS, MED_STACK_TASKS, LARGE_STACK_TASKS,
	    soft_fail, spi.retry, spi_delay, asan_used);

	if (flags & TDUMP_LOG)
	//lfprintf(printf_type, "Tttt Pd# cccccccc xxx.xxx xxxxx.xxx xxx.x%% xxxxxx xxxxx xxxxx xxx xxxxx xxx xxxx.xxxuu xxx%% cN\n");
	  lfprintf(printf_type, "     I # RWSPBLHq   run S    max mS   cpu%%  #runs  cmds   st1       st2       deadline stk%% ch task______ where___________________\n");
	else
	//lfprintf(printf_type, "Tttt Pd# cccccccc xxx.xxx xxxxx.xxx xxx.x%% xxxxxx xxxxx xxxxx xxx xxxxx xxx xxxxx xxxxx xxxxx xxxx.xxxuu xxx%% cN\n");
	  lfprintf(printf_type, "     I # RWSPBLHq   run S    max mS   cpu%%  #runs  cmds   st1       st2       #wu   nrs retry   deadline stk%% ch task______ where___________________ longest ________________\n");

	for (i=0; i <= max_task; i++) {
		t = Tasks + i;
		if (!t->valid)
			continue;
		float f_usec = ((float) t->usec) / 1e6;
		f_sum += f_usec;
		float f_longest = ((float) t->longest) / 1e3;

		float deadline=0;
		const char *dline = "", *dunit = "";
		if (t->deadline > 0) {
		    if (t->deadline < now_us) {
		        dline = "has past";
		    } else {
                deadline = (float) (t->deadline - now_us);
                deadline /= 1e3;            // _mmm.uuu msec
                dunit = "ms";
                if (deadline >= 3600000) {  // >= 60 min
                    deadline /= 3600000;    // hhhh.fff hr
                    dunit = "Hr";
                } else
                if (deadline >= 60000) {    // >= 60 secs
                    deadline /= 60000;      // mmmm.fff min
                    dunit = "Mn";
                } else
                if (deadline >= 1000) {     // >= 1 sec
                    deadline /= 1000;       // ssss.fff sec
                    dunit = "s";
                }
                dline = stnprintf(0, "%8.3f%-2s", deadline, dunit);
            }
		}
		
		int rx_channel = (t->flags & CTF_RX_CHANNEL)? (t->flags & CTF_CHANNEL) : -1;
		const char *rx_s = (rx_channel == -1)? "  " : stnprintf(1, "c%d", rx_channel);

		if (flags & TDUMP_LOG)
		lfprintf(printf_type, "%c%03d %c%d%c %c%c%c%c%c%c%c%c %7.3f %9.3f %5.1f%% %6s %5d %5d %-3s %5d %-3s %10s %3d%c %s %-10s %-24s\n",
		    (t == ct)? '*':'T', i, (t->flags & CTF_PRIO_INVERSION)? 'I':'P', t->priority, t->lock_marker,
			t->stopped? 'T':'R', t->wakeup? 'W':'_', t->sleeping? 'S':'_', t->pending_sleep? 'P':'_', t->busy_wait? 'B':'_',
			t->lock.wait? 'L':'_', t->lock.hold? 'H':'_', t->minrun? 'q':'_',
			f_usec, f_longest, f_usec/f_elapsed*100,
			toUnits(t->run), t->cmds,
			t->stat1, t->units1? t->units1 : " ", t->stat2, t->units2? t->units2 : " ",
			dline, t->stack_hiwat*100 / t->ctx->stack_size_u64, (t->flags & CTF_STACK_MED)? 'M' : ((t->flags & CTF_STACK_LARGE)? 'L' : '%'),
			rx_s, t->name, t->where? t->where : "-"
		);
		else
		lfprintf(printf_type, "%c%03d %c%d%c %c%c%c%c%c%c%c%c %7.3f %9.3f %5.1f%% %6s %5d %5d %-3s %5d %-3s %5d %5d %5d %10s %3d%c %s %-10s %-24s %-24s\n",
		    (t == ct)? '*':'T', i, (t->flags & CTF_PRIO_INVERSION)? 'I':'P', t->priority, t->lock_marker,
			t->stopped? 'T':'R', t->wakeup? 'W':'_', t->sleeping? 'S':'_', t->pending_sleep? 'P':'_', t->busy_wait? 'B':'_',
			t->lock.wait? 'L':'_', t->lock.hold? 'H':'_', t->minrun? 'q':'_',
			f_usec, f_longest, f_usec/f_elapsed*100,
			toUnits(t->run), t->cmds,
			t->stat1, t->units1? t->units1 : " ", t->stat2, t->units2? t->units2 : " ",
			t->wu_count, t->no_run_same, t->spi_retry,
			dline, t->stack_hiwat*100 / t->ctx->stack_size_u64, (t->flags & CTF_STACK_MED)? 'M' : ((t->flags & CTF_STACK_LARGE)? 'L' : '%'),
			rx_s, t->name, t->where? t->where : "-", t->long_name? t->long_name : "-"
		);
		
		bool detail = false;
		if (t->lock.waiting)
			lfprintf(printf_type, " LockWaiting=T"), detail = true;
		if (t->lock.wait)
			lfprintf(printf_type, " LockWait=%s", t->lock.wait->name), detail = true;
		if (t->lock.hold)
			lfprintf(printf_type, " LockHold=%s", t->lock.hold->name), detail = true;
		if (t->lock.waiting || t->lock.wait || t->lock.hold) {
			lfprintf(printf_type, " LockToken=%d", t->lock.token);
			lfprintf(printf_type, " run=%d", run[t->id].r);
			detail = true;
		}
		if (t->flags & CTF_PRIO_INVERSION)
			lfprintf(printf_type, " InversionSaved=%d", t->saved_priority), detail = true;
		if (detail) lfprintf(printf_type, "\n");
        NextTask("TaskDump");

        /*
		if ((t->no_run_same > 200) && ev_dump) {
			evNT(EC_DUMP, EV_NEXTTASK, ev_dump, "no_run_same", evprintf("DUMP IN %.3f SEC", ev_dump/1000.0));
		}
		*/

		t->usec = t->longest = 0;
		t->long_name = NULL;
		t->run = t->cmds = t->wu_count = t->no_run_same = t->spi_retry = 0;
		if (t->s1_func & TSTAT_ZERO) t->stat1 = 0;
		if (t->s2_func & TSTAT_ZERO) t->stat2 = 0;
	}

	f_sum += f_idle;
	float f_pct = f_idle/f_elapsed*100;
	//lfprintf(printf_type, "Tttt Pd# cccccccc xxx.xxx xxxxx.xxx xxx.x%%
	  lfprintf(printf_type, "idle              %7.3f           %5.1f%%\n", f_idle, f_pct);
	float f_remain = fabsf(f_elapsed - f_sum);
	f_pct = f_remain/f_elapsed*100;
	//if (f_remain > 0.01)
	//lfprintf(printf_type, "Tttt Pd# cccccccc xxx.xxx xxxxx.xxx xxx.x%%
	  lfprintf(printf_type, "Linux             %7.3f           %5.1f%%\n", f_remain, f_pct);
    NextTask("TaskDump");

    const char *hist_name[N_HIST] = { "<1", "1", "2", "4", "8", "16", "32", "64", "128", "256", "512", ">=1k" };
    
    if (flags & TDUMP_HIST) {
        lfprintf(printf_type, "\n");
        lfprintf(printf_type, "HIST:           ");
        for (j = 0; j < N_HIST; j++) {
            if (task_all_hist[j])
                lfprintf(printf_type, "%d|%sm ", task_all_hist[j], hist_name[j]);
        }
        lfprintf(printf_type, " \n");
        NextTask("TaskDump");
    }

	for (i=0; i <= max_task; i++) {
		t = Tasks + i;
		if (!t->valid)
			continue;
		if (flags & TDUMP_HIST) {
		    lfprintf(printf_type, "T%03d %-10s ", i, t->name);
		    for (j = 0; j < N_HIST; j++) {
		        if (t->hist[j])
		            lfprintf(printf_type, "%d|%sm ", t->hist[j], hist_name[j]);
		    }
		    lfprintf(printf_type, " \n");
            NextTask("TaskDump");
		}
	}
}

static int _TaskStat(TASK *t, u4_t s1_func, int s1_val, const char *s1_units, u4_t s2_func, int s2_val, const char *s2_units)
{
	int r=0;
    
    t->s1_func |= s1_func & TSTAT_LATCH;
    if (s1_units) t->units1 = s1_units;
    switch (s1_func & TSTAT_MASK) {
    	case TSTAT_NC: break;
    	case TSTAT_SET: t->stat1 = s1_val; break;
    	case TSTAT_INCR: t->stat1++; break;
    	case TSTAT_MIN: if (s1_val < t->stat1 || t->stat1 == 0) t->stat1 = s1_val; break;
    	case TSTAT_MAX: if (s1_val > t->stat1) t->stat1 = s1_val; break;
    	default: break;
    }

    t->s2_func |= s2_func & TSTAT_LATCH;
    if (s2_units) t->units2 = s2_units;
    switch (s2_func & TSTAT_MASK) {
    	case TSTAT_NC: break;
    	case TSTAT_SET: t->stat2 = s2_val; break;
    	case TSTAT_INCR: t->stat2++; break;
    	case TSTAT_MIN: if (s2_val < t->stat2 || t->stat2 == 0) t->stat2 = s2_val; break;
    	case TSTAT_MAX: if (s2_val > t->stat2) t->stat2 = s2_val; break;
    	default: break;
    }
    
    if ((t->s1_func|t->s2_func) & TSTAT_CMDS) t->cmds++;

    if ((t->s1_func|t->s2_func) & TSTAT_SPI_RETRY) {
    	t->spi_retry++;
    	r = t->spi_retry;
		if ((r > 100) && ev_dump) {
			evNT(EC_DUMP, EV_NEXTTASK, ev_dump, "spi_retry", evprintf("DUMP IN %.3f SEC", ev_dump/1000.0));
		}
    }

	return r;
}

int TaskStat(u4_t s1_func, int s1_val, const char *s1_units, u4_t s2_func, int s2_val, const char *s2_units)
{
    return _TaskStat(cur_task, s1_func, s1_val, s1_units, s2_func, s2_val, s2_units);
}

static void task_init(TASK *t, int id, funcP_t funcP, void *param, const char *name, u4_t priority, u4_t flags, int f_arg)
{
	int i;
	
	memset(t, 0, sizeof(TASK));
	t->id = id;
	t->ctx = ctx + id;
	t->funcP = funcP;
	t->create_param = param;
	t->flags = flags;
	t->name = name;
	t->minrun_start_us = timer_us64();
	t->valid = TRUE;
	t->tll.t = t;
	
    #ifdef LOCK_CHECK_HANG
        t->lock_marker = ' ';
    #endif

	if (flags & CTF_BUSY_HELPER) {
		assert(!busy_helper_task);
		busy_helper_task = t;
	}

	if (flags & CTF_POLL_INTR) {
		assert(!itask);
		itask = t;
		itask_tid = id;
	}

	TenQ(t, priority);
	
	run[id].t = id;
	run[id].v = 1;
	run[id].n = name;
	run[id].p = priority;
	run[id].r = 0;
}

static void task_stack(int id)
{
	ctx_t *c = ctx + id;
	c->id = id;
	
	c->stack = (u64_t *) (task_stacks + stack_map[id]);
    c->stack_size_u64 = STACK_SIZE_U64_T * stack_nel[id];	
	c->stack_last = (u64_t *) (task_stacks + (stack_map[id] + stack_nel[id]));
	c->stack_size_bytes = c->stack_size_u64 * sizeof(u64_t);
	//printf("task_stack T%d %d (STACK_SIZE_U64_T) %d bytes %p-%p\n", id, c->stack_size_u64, c->stack_size_bytes, c->stack, c->stack_last);

#if defined(HOST) && defined(USE_VALGRIND)
	if (RUNNING_ON_VALGRIND) {
		if (!c->valgrind_stack_reg) {
			c->valgrind_stack_id = VALGRIND_STACK_REGISTER(c->stack, c->stack_last);
			c->valgrind_stack_reg = true;
		}

		return;
	}
#endif

#ifdef USE_ASAN
	return;
#endif

    // initialize stack with pattern detected by TaskCheckStacks()
	u64_t *s;
	u64_t magic = 0x8BadF00d00000000ULL;
	int i;
	for (s = c->stack, i=0; s < c->stack_last; s++, i++) {
		*s = magic | ((u64_t) s & 0xffffffff);
		//if (i == (t->ctx->stack_size_u64 - 1)) printf("T%03d W %08x|%08x\n", c->id, PRINTF_U64_ARG(*s));
	}
}

#ifdef SETUP_TRAMP_USING_JMP_BUF

// Debian 8 includes the glibc version that does pointer mangling of internal data structures
// like jmp_buf as a security measure. It's just a simple xor, so we can figure out the key easily.
static u4_t key;

static void find_key()
{
	#define	STACK_CORRECTION 0x04
	u4_t sp;
	setjmp(ctx[0].jb);
	sp = (u4_t) ((u64_t) &sp & 0xffffffff) - STACK_CORRECTION;
	key = ctx[0].sp ^ sp;
	// CAUTION: key computation will fail if a printf is used here
}

static void trampoline()
{
    TASK *t = cur_task;
	(t->funcP)(t->create_param);
	printf("task %s exited by returning\n", task_ls(t));
	TaskRemove(t->id);
}

#else

static ctx_t *new_ctx;		// only way trampoline() can know what associated task/ctx is
static volatile int new_task_req, new_task_svc;

static void trampoline(int signo)
{
	ctx_t *c = new_ctx;
    TASK *t = Tasks + c->id;
	
    // save new stack pointer
    if (setjmp(c->jb) == 0) {
		//printf("trampoline SETUP sp %p ctx %p T%d-%p %p-%p\n", &c, c, c->id, t, c->stack, c->stack_last);
		new_task_svc++;
    	return;
    }

#ifdef USE_ASAN
	//printf("TaskResume(trampoline) fake_stack=%p stack=%p %s\n", t->ctx->fake_stack, t->ctx->stack, t->name);
    #ifdef USE_ASAN2
	    __sanitizer_finish_switch_fiber(c->fake_stack, (const void **) &(c->stack), (size_t *) &(c->stack_size_bytes));
	#else
	    __sanitizer_finish_switch_fiber(c->fake_stack);
	#endif
#endif
	//printf("trampoline BOUNCE sp %p ctx %p T%d-%p %p-%p\n", &c, c, c->id, t, c->stack, c->stack_last);
	(t->funcP)(t->create_param);
	printf("task %s exited by returning\n", task_ls(t));
	TaskRemove(t->id);
}

#endif

// this can only be done while running on the main stack,
// i.e. not on task stacks created via sigaltstack()
static bool collect_needed;

void TaskCollect()
{
	int i;
	
	if (!collect_needed)
		return;

	for (i = TID_FIRST; i < MAX_TASKS; i++) {		// NB: start at task 1 as the main task uses the ordinary stack
		ctx_t *c = ctx + i;
		if (c->init) continue;
		task_stack(i);

#ifdef SETUP_TRAMP_USING_JMP_BUF
		setjmp(c->jb);
		c->pc = (u64_t) trampoline ^ key;
		c->sp = (u64_t) c->stack_last ^ key;		// careful, has to be top of stack
#else
		if (new_task_req != new_task_svc) {
			printf("create_task: req %d svc %d\n", new_task_req, new_task_svc);
			panic("previous create_task hadn't finished");
		}
		new_task_req++;
	
		stack_t stack;
		stack.ss_flags = 0;
		stack.ss_size = c->stack_size_u64 * sizeof(u64_t);
		stack.ss_sp = (void *) c->stack;
        //printf("i=%d ss=%d %d stk=%p\n", i, c->stack_size_u64, c->stack_size_bytes, c->stack);
		scall("sigaltstack", sigaltstack(&stack, 0));
			 
		struct sigaction sa;
		sa.sa_handler = &trampoline;
		sa.sa_flags = SA_ONSTACK;
		sigemptyset(&sa.sa_mask);
		scall("sigaction", sigaction(SIG_SETUP_TRAMP, &sa, 0));
		
		new_ctx = c;
		raise(SIG_SETUP_TRAMP);
#endif

		c->init = TRUE;
	}
	
	collect_needed = false;
}

static int our_pid, kiwi_server_pid;
#define LINUX_CHILD_PROCESS()   (our_pid != 0 && kiwi_server_pid != 0 && our_pid != kiwi_server_pid)

u4_t task_medium_priority;
static u4_t task_snd_intr_usec;

void TaskInit()
{
    TASK *t;
    
    //task_medium_priority = TASK_MED_PRI_OLD;
    task_medium_priority = TASK_MED_PRI_NEW;
	
    // change priority of process (and not pgrp) so it's not inherited by sub-processes (e.g. geo-location) which then negatively impact real-time response
    //setpriority(PRIO_PROCESS, getpid(), -20);

	kiwi_server_pid = getpid();
	printf("TASK MAX_TASKS %d(%d|%d|%d), stack memory %.1f MB, stack size %d|%d|%d k so(u64_t)\n",
	    MAX_TASKS, REG_STACK_TASKS, MED_STACK_TASKS, LARGE_STACK_TASKS,
	    ((float) sizeof(task_stacks))/M,
	    STACK_SIZE_REG * STACK_SIZE_U64_T / K, STACK_SIZE_MED * STACK_SIZE_U64_T / K, STACK_SIZE_LARGE * STACK_SIZE_U64_T / K);

	t = Tasks;
	cur_task = t;

	task_init(t, TID_MAIN, NULL, NULL, "main", MAIN_PRIORITY, 0, 0);
	t->ctx->stack_size_u64 = STACK_SIZE_U64_T;

	last_dump = t->tstart_us = timer_us64();
	//if (ev_dump) evNT(EC_DUMP, EV_NEXTTASK, ev_dump, "TaskInit", evprintf("DUMP IN %.3f SEC", ev_dump/1000.0));
	for (int p = LOWEST_PRIORITY; p <= HIGHEST_PRIORITY; p++) TaskQ[p].p = p;	// debugging aid
	
#ifdef SETUP_TRAMP_USING_JMP_BUF
	find_key();
	printf("TASK jmp_buf demangle key 0x%08x\n", key);
	//setjmp(ctx[0].jb);
	//printf("JMP_BUF: key 0x%x sp 0x%x:0x%x pc 0x%x:0x%x\n", key, ctx[0].sp, ctx[0].sp^key, ctx[0].pc, ctx[0].pc^key);
#endif

    int s = 0;
	for (int i = 0; i < MAX_TASKS; i++, t++) {
	    stack_map[i] = s;
	    
	    if (i < REG_STACK_TASKS) {
	        stack_nel[i] = STACK_SIZE_REG;
	        s += STACK_SIZE_REG;
	    } else
	    if (MED_STACK_TASKS && i < REG_STACK_TASKS + MED_STACK_TASKS) {
	        stack_nel[i] = STACK_SIZE_MED;
	        s += STACK_SIZE_MED;
	        t->flags |= CTF_STACK_MED;
	    } else {
	        stack_nel[i] = STACK_SIZE_LARGE;
	        s += STACK_SIZE_LARGE;
	        t->flags |= CTF_STACK_LARGE;
	    }
	}
	assert(s == N_STACK_EL);

	collect_needed = TRUE;
	TaskCollect();
	
	task_package_init = TRUE;
}

void TaskInitCfg()
{
	#define TASK_INTR_USEC_EARLY 1000
	task_snd_intr_usec = snd_intr_usec - TASK_INTR_USEC_EARLY;
}

void TaskCheckStacks(bool report)
{
	int i, j;
	TASK *t;
	u64_t *s;
	

#if defined(HOST) && defined(USE_VALGRIND)
	// as you'd expect, valgrind gets real upset about our stack checking scheme
	if (RUNNING_ON_VALGRIND)
		return;
#endif

#ifdef USE_ASAN
	return;
#endif

	u64_t magic = 0x8BadF00d00000000ULL;
    bool stk_panic = false;
    #define STK_PCT_PANIC 75
    
	for (i = TID_FIRST; i <= max_task; i++) {
		t = Tasks + i;
		if (!t->valid || !t->ctx->init) continue;
		int m, l = 0, u = t->ctx->stack_size_u64 - 1;
		
		// bisect stack until high-water mark is found
		do {
			m = l + (u-l)/2;
			s = &t->ctx->stack[m];
			if (*s == (magic | ((u64_t) s & 0xffffffff))) l = m; else u = m;
		} while ((u-l) > 1);
		
		int used = t->ctx->stack_size_u64 - m - 1;
		t->stack_hiwat = used;
		int pct = used*100/t->ctx->stack_size_u64;
		if (report) {
            printf("%s stack used %d/%d (%d%%) PEAK %s\n", task_s(t), used, t->ctx->stack_size_u64, pct, (pct >= STK_PCT_PANIC)? "DANGER":"");
        } else {
            if (pct >= STK_PCT_PANIC) {
                printf("DANGER: %s stack used %d/%d (%d%%) PEAK\n", task_s(t), used, t->ctx->stack_size_u64, pct);
                stk_panic = true;
            }
        }
	}
	if (stk_panic) panic("TaskCheckStacks");
}

bool itask_run;
static ipoll_from_e last_from = CALLED_FROM_INIT;
static const char *poll_from[] = { "INIT", "NEXTTASK", "LOCK", "SPI", "FASTINTR" };

void TaskPollForInterrupt(ipoll_from_e from)
{
	if (!itask) {
		return;
	}

	if (GPIO_READ_BIT(SND_INTR) && itask->sleeping) {
		evNT(EC_TRIG1, EV_NEXTTASK, -1, "PollIntr", evprintf("CALLED_FROM_%s TO INTERRUPT TASK <===========================",
			poll_from[from]));

		// can't call TaskWakeup() from within NextTask()
		if (from == CALLED_WITHIN_NEXTTASK) {
			itask->wu_count++;
			RUNNABLE_YES(itask);
            itask->wake_param = TO_VOID_PARAM(itask->last_run_time);  // return how long task ran last time
			evNT(EC_EVENT, EV_NEXTTASK, -1, "PollIntr", evprintf("from %s, return from CALLED_WITHIN_NEXTTASK of itask, wake_param=%d", poll_from[from], itask->wake_param));
		} else {
			TaskWakeup(itask_tid);
			evNT(EC_EVENT, EV_NEXTTASK, -1, "PollIntr", evprintf("from %s, return from TaskWakeup of itask", poll_from[from]));
		}
	} else {
		if (last_from != CALLED_WITHIN_NEXTTASK) {	// eliminate repeated messages from idle loop
			evNT(EC_EVENT, EV_NEXTTASK, -1, "PollIntr", evprintf("CALLED_FROM_%s %s <===========================",
				poll_from[from], itask->sleeping? "NO PENDING INTERRUPT" : "INTERRUPT TASK PENDING"));
			last_from = from;
		}
	}
}

#if 0
void TaskFastIntr()
{
	if (GPIO_READ_BIT(SND_INTR)) {
		TaskPollForInterrupt(CALLED_FROM_FASTINTR);
	}
}
#endif

void TaskRemove(int id)
{
    TASK *t = Tasks + id;
    TdeQ(t);
    t->stopped = TRUE;
	run[t->id].r = 0;
    t->valid = FALSE;
    run[t->id].v = 0;
    t->ctx->init = FALSE;
    collect_needed = TRUE;
    
    if (t->flags & CTF_TNAME_FREE) {
        kiwi_asfree((void *) t->name);
        t->name = NULL;
    }

    if (t->lock.hold) {
    	lprintf("TaskRemove: %s holding lock \"%s\"!\n", task_ls(t), t->lock.hold->name);
    	panic("TaskRemove");
    }

#if defined(HOST) && defined(USE_VALGRIND)
	//VALGRIND_STACK_DEREGISTER(t->ctx->valgrind_stack_id);
#endif
    NextTask("TaskRemove");
	if (t == cur_task && !t->valid) panic("shouldn't return");
}

void TaskLastRun()
{
	TASK *t = last_task_run;
	if (t) {
		printf("task last run: %s", task_ls(t));
		if (t->last_pc)
			printf("@0x%llx", t->last_pc);
		printf(" %.3f %.3f ms\n", (float) t->last_last_run_time / 1000, (float) t->last_run_time / 1000);
	}
}

u64_t TaskStartTime()
{
	return cur_task->tstart_us;
}

void TaskForkChild()
{
	cur_task->flags |= CTF_FORK_CHILD;
    our_pid = getpid();

    // change priority of child processes back to neutral so they don't negatively impact real-time response
    //setpriority(PRIO_PROCESS, our_pid, 0);
}

bool TaskIsChild()
{
	if (our_pid == 0 || kiwi_server_pid == 0) return false;     // too early to determine
	return (LINUX_CHILD_PROCESS());
}

// doesn't work as expected
//#define LOCK_TEST_HANG
#ifdef LOCK_TEST_HANG
    static int lock_test_hang;
#endif

#ifdef LOCK_CHECK_HANG
    static bool lock_panic;
#endif
#if defined(LOCK_CHECK_HANG) && defined(EV_MEAS_LOCK)
    static int expecting_spi_lock_next_task;
#endif

void _NextTask(const char *where, u4_t param, u_int64_t pc)
{
    if (!task_package_init) return;
    
	int i;
    TASK *t, *tn, *ct;
    u64_t now_us, enter_us = timer_us64();
    u4_t quanta;

	ct = cur_task;
    //real_printf("_NextTask ENTER %s\n", task_ls(ct));
	
    if (param == NT_FAST_CHECK) {
        u4_t diff = (u4_t) (enter_us - ct->tstart_us);
        if (diff < task_snd_intr_usec) return;
        //D_STMT(if (diff > snd_intr_usec) real_printf("OVER %6u %s\n", diff - task_snd_intr_usec, where));
	}
	
    quanta = enter_us - ct->tstart_us;
    ct->usec += quanta;
    
    #if defined(LOCK_CHECK_HANG) && defined(EV_MEAS_LOCK)
        if (expecting_spi_lock_next_task && ct->minrun == 0) {
            expecting_spi_lock_next_task--;
            evLock(EC_EVENT, EV_NEXTTASK, -1, "next_task", evprintf("dec expecting_spi_lock_next_task=%d", expecting_spi_lock_next_task));
            if (expecting_spi_lock_next_task == 0) {
                lprintf("expecting_spi_lock_next_task !!!!\n");
                lock_panic = true;
            }
        }
    #endif

    if (ct->flags & CTF_NO_CHARGE) {     // don't charge the current task
        ct->flags &= ~CTF_NO_CHARGE;
    } else {
        u4_t ms = quanta/K;
        i = ffs(ms);
        i = MIN(i, N_HIST-1);
        ct->hist[i]++;
        task_all_hist[i]++;
    }
    
    our_pid = getpid();

    if (quanta > ct->longest) {
    	ct->longest = quanta;
    	
    	#ifdef DEBUG
            ct->long_name = where;
            
            // fixme: remove at some point
            if (quanta > 2000000 && ct->id != 0) {
                printf("LRUN %s %s %7.3f\n", task_ls(ct), where, (float) quanta / 1000);
                //evNT(EC_DUMP, EV_NEXTTASK, -1, "NT", "LRUN");
            }
        #endif
    }

	D_STMT(ct->where = where);

    // don't switch until minrun expired (if any)
    if (ct->minrun && ((enter_us - ct->minrun_start_us) < ct->minrun))
    	return;
    
    // detect an SPI busy-waiting task that needs the assistance of the SPI busy-helper task
    bool no_run_same = false;
    if (param == NT_BUSY_WAIT) {
    	if (ct->busy_wait) {
    		if (busy_helper_task) {
				busy_helper_task->wu_count++;
				RUNNABLE_YES(busy_helper_task);
				evNT(EC_EVENT, EV_NEXTTASK, -1, "NextTask", evprintf("%s BUSY_WAIT second time, BUSY_HELPER WAKEUP", task_s(ct)));
			} else {
				no_run_same = true;
				evNT(EC_EVENT, EV_NEXTTASK, -1, "NextTask", evprintf("%s BUSY_WAIT second time, no_run_same SET", task_s(ct)));
			}
			ct->no_run_same++;
    		ct->busy_wait = false;
    	} else {
    		ct->busy_wait = true;
			evNT(EC_EVENT, EV_NEXTTASK, -1, "NextTask", evprintf("%s BUSY_WAIT first time",task_s(ct)));
    	}
    } else {
		ct->busy_wait = false;
    }
    
    if (param == NT_LONG_RUN) {
		ct->long_run = true;
	}
	
	// find next task to run
    int p, idle_count=0;
    TaskQ_t *head;
	evNT(EC_TASK_SCHED, EV_NEXTTASK, -1, "NextTask", "looking for task to run ...");

    // mark ourselves as "last run" on our task queue if we're still valid
	assert(ct->tq == &TaskQ[ct->priority]);
	ct->tq->last_run = ct->valid? &ct->tll : NULL;

    do {
        now_us = timer_us64();

		TaskPollForInterrupt(CALLED_WITHIN_NEXTTASK);
		
		// check for lock deadlock, but only on main Linux process
        #ifdef LOCK_CHECK_HANG
            static u64_t lock_check_us;
            if (!LINUX_CHILD_PROCESS() && now_us > lock_check_us) {     // limit calls to lock_check()
                lock_check_us = now_us + SEC_TO_USEC(1);
                lock_panic = lock_check();
                
                // mark tasks considered during search and print in task dump to help diagnose task queueing issues
                if (lock_panic) {
                    TASK *tp = Tasks;
                    for (i=0; i <= max_task; i++, tp++) {
                        if (!tp->valid) continue;
                        tp->lock_marker = ' ';
                    }
                }
            }
        #endif
    
   		 // search task queues
		for (p = HIGHEST_PRIORITY; p >= LOWEST_PRIORITY; p--) {
			head = &TaskQ[p];
			assert(p == head->p);
			
			#ifdef USE_RUNNABLE
				int t_runnable = head->runnable;
				//evNT(EC_EVENT, EV_NEXTTASK, -1, "NextTask", evprintf("looking P%d, count %d, runnable %d",
				//	p, head->count, t_runnable));
				if (!(t_runnable >= 0 && t_runnable <= head->count && t_runnable <= MAX_TASKS))
					assert(t_runnable >= 0 && t_runnable <= head->count && t_runnable <= MAX_TASKS);
			#else
				int t_runnable = 0;
				TaskLL_t *tll;
				
                #ifdef LOCK_TEST_HANG
                    if (lock_test_hang && p == DATAPUMP_PRIORITY) {
                        printf("LOCK_TEST_HANG ignoring DATAPUMP_PRIORITY\n");
                        continue;
                    }
                #endif
                
                #ifdef LOCK_CHECK_HANG
                    if (lock_panic && head->last_run) lprintf("P%d: last_run %s\n", p, task_s(head->last_run->t));
                #endif
					
				// count the number runnable
				for (tll = head->tll.next; tll; tll = tll->next) {
					assert(tll);
					TASK *tp = tll->t;
					assert(tp);
					assert(tp && tp->valid);    // tp check to keep static analyzer quiet
					
                    #ifdef LOCK_CHECK_HANG
				        if (lock_panic) lprintf("P%d: %s %s\n", p, task_s(tp), tp->stopped? "STOP":"RUN");
                    #endif
					
                    bool wake = false;
                    
                    if (tp->deadline > 0) {
                        if (tp->deadline < now_us) {
                            evNT(EC_EVENT, EV_NEXTTASK, -1, "NextTask", evprintf("deadline expired %s, Qrunnable %d", task_s(tp), tp->tq->runnable));
                            tp->deadline = 0;
                            wake = true;
                        }
                    } else
                    if (tp->wakeup_test != NULL) {
                        if (*tp->wakeup_test != 0) {
                            evNT(EC_EVENT, EV_NEXTTASK, -1, "NextTask", evprintf("wakeup_test completed %s, Qrunnable %d", task_s(tp), tp->tq->runnable));
                            tp->wakeup_test = NULL;
                            wake = true;
                        }
                    }
                    
                    // Return how long task ran last time.
                    // But ONLY do this if task waking due to deadline expiring or wakeup_test succeeding.
                    // Doesn't happen if deadline cancelled due to someone else doing a TaskWakeup(id, TWF_CANCEL_DEADLINE, param)
                    // In that case param gets returned when the loop in _TaskSleep() exits due to RUNNABLE_YES()
                    // happening in _TaskWakeup() and not here.
                    if (wake) {
                        RUNNABLE_YES(tp);
                        tp->wake_param = TO_VOID_PARAM(tp->last_run_time);
                    }
                    
					if (!tp->stopped)
						t_runnable++;
					
					#ifdef LOCK_CHECK_HANG
                        if (lock_panic && tp == busy_helper_task)
                            lprintf("P%d: busy_helper_task stopped %d lock.wait %d\n", p, tp->stopped, tp->lock.wait);
                    #endif
				}

                #ifdef LOCK_CHECK_HANG
				    if (lock_panic) lprintf("P%d: === runnable %d\n", p, t_runnable);
				#endif
			#endif

            //if (p == 2 && t_runnable) real_printf("P2-%d ", t_runnable); fflush(stdout);

			if (p == ct->priority && t_runnable == 1 && no_run_same) {
				evNT(EC_EVENT, EV_NEXTTASK, -1, "NextTask", evprintf("%s no_run_same TRIGGERED ***", task_s(ct)));
				no_run_same = false;
                //if (p == 2) real_printf("NRS?%s ", task_s(ct)); fflush(stdout);
				continue;
			}
			
			t = NULL;
			if (t_runnable) {
				// at this point the p/head queue should have at least one runnable task
				TaskLL_t *wrap = (head->last_run && head->last_run->next)? head->last_run->next : head->tll.next;
	            assert(wrap->t->priority == p);
	            
	            // start with the one after the last run (round robin) or, failing that, the first one in the queue
				TaskLL_t *tll = wrap;
				
				int looping = 0;
				do {
					assert(tll);
					t = tll->t;
					assert(t);
					assert(t && t->valid);      // t check to keep static analyzer quiet
					
					#ifdef LOCK_CHECK_HANG
					    if (lock_panic) t->lock_marker = '#';
					#endif
					
					// if we're a Linux child process ignore all tasks except those created by the child
					if (LINUX_CHILD_PROCESS() && !(t->flags & CTF_FORK_CHILD)) {
						//printf("LINUX_CHILD_PROCESS child %s ignoring %s\n", task_s(cur_task), task_s2(t));
						;   // NB: do not remove
					} else

					if (!t->stopped && t->long_run) {
						u4_t last_time_run = now_us - itask_last_tstart;
						if (!itask || !itask_run || (itask_run && last_time_run < 4000)) {
							evNT(EC_EVENT, EV_NEXTTASK, -1, "NextTask", evprintf("OKAY for LONG RUN %s, interrupt last ran @%.6f, %d us ago",
								task_s(t), (float) itask_last_tstart / 1000000, last_time_run));
							//if (ev_dump) evNT(EC_DUMP, EV_NEXTTASK, ev_dump, "NextTask", evprintf("DUMP IN %.3f SEC", ev_dump/1000.0));
							t->long_run = false;
							
							#ifdef LOCK_CHECK_HANG
                                if (lock_panic)
                                    t->lock_marker = 'L';
                                else
                                    break;
                            #else
                                break;
                            #endif
						} else {
                            // not eligible to run at this time
                            _TaskStat(t, TSTAT_MIN|TSTAT_ZERO, last_time_run, "min", TSTAT_MAX|TSTAT_ZERO, last_time_run, "max");
							#ifdef LOCK_CHECK_HANG
                                if (lock_panic) t->lock_marker = 'N';
                            #endif
                            #if 0
                            evNT(EC_EVENT, EV_NEXTTASK, -1, "not elig", evprintf("NOT OKAY for LONG RUN %s, interrupt last ran @%.6f, %d us ago",
                                task_s(t), (float) itask_last_tstart / 1000000, last_time_run));
                            if (ev_dump) evNT(EC_DUMP, EV_NEXTTASK, ev_dump, "not elig", evprintf("DUMP IN %.3f SEC", ev_dump/1000.0));
                            #endif
                        }
					} else {
						if (!t->stopped) {
						    #ifdef LOCK_CHECK_HANG
                                if (lock_panic)
                                    t->lock_marker = '*';
                                else
                                    break;
                            #else
                                break;
                            #endif
						}
					}
					
					t = NULL;
					tll = tll->next;
					if (tll == NULL) tll = head->tll.next;
					
					if (++looping == 100)
					    break;
				} while (tll != wrap);
				
				if (looping == 100)
				    panic("NextTask looping");
				
				if (t) {
					assert(t->valid);
					assert(!t->sleeping);

                    #ifdef LOCK_CHECK_HANG
					if (!lock_panic)
					    break;
                    #else
                        break;
                    #endif
				}
			} // if (t_runnable)
		} // for (p = HIGHEST_PRIORITY; p >= LOWEST_PRIORITY; p--)

        #ifdef LOCK_CHECK_HANG
            if (lock_panic) {
                dump();
                evLock(EC_DUMP, EV_NEXTTASK, -1, "lock panic", "DUMP lock_panic");
                panic("lock_check");
            }
        #endif

		idle_count++;
		
		#if 0
            static int is_idle;
            if (p < LOWEST_PRIORITY) {
                is_idle++;
            } else {
                if (is_idle) {
                    real_printf(".%d ", is_idle); fflush(stdout);
                    is_idle = 0;
                }
            }
        #endif
    } while (p < LOWEST_PRIORITY);		// if no eligible tasks keep looking
    
    // DANGER: Critical to have "!have_snd_users" test. Otherwise deadlock situation can occur:
    // Data pump is still running, interrupting faster than kiwi_usleep() rate below. This keeps
    // the scheduler hung at the data pump priority level, never running anything else!
	if (!need_hardware || (update_in_progress && !have_snd_users) || sd_copy_in_progress || LINUX_CHILD_PROCESS()) {
		kiwi_usleep(100000);		// pause so we don't hog the machine
	}

	// remember where we were last running
    if (ct->valid && setjmp(ct->ctx->jb)) {
#ifdef USE_ASAN
		//printf("TaskResume(NextTask) fake_stack=%p stack=%p %s\n", t->ctx->fake_stack, t->ctx->stack, t->name);
        #ifdef USE_ASAN2
            __sanitizer_finish_switch_fiber(ct->ctx->fake_stack, (const void **) &(ct->ctx->stack), (size_t *) &(ct->ctx->stack_size_bytes));
        #else
            __sanitizer_finish_switch_fiber(ct->ctx->fake_stack);
        #endif
#endif
    	return;		// returns here when task next run
    }

    now_us = timer_us64();
    u4_t just_idle_us = now_us - enter_us;
    idle_us += just_idle_us;
	if (t->minrun) t->minrun_start_us = now_us;
	
    #ifdef EV_MEAS_NEXTTASK
        if (idle_count > 1)
            evNT(EC_TASK_IDLE, EV_NEXTTASK, -1, "NextTask", evprintf("IDLE for %d spins, %.3f ms",
                idle_count, (float) just_idle_us / 1000));
        if (pc) {
            evNT(EC_TASK_SWITCH, EV_NEXTTASK, t->id, "NextTask", evprintf("from %s@0x%llx => to %s", task_ls(ct), pc, task_ls2(t)));
        } else {
            evNT(EC_TASK_SWITCH, EV_NEXTTASK, t->id, "NextTask", evprintf("from %s => to %s", task_ls(ct), task_ls2(t)));
        }
	#endif
	
	t->tstart_us = now_us;
	if (t->flags & CTF_POLL_INTR) itask_last_tstart = now_us;

	ct->last_last_run_time = ct->last_run_time;
	ct->last_run_time = quanta;
	D_STMT(ct->last_pc = pc);
	last_task_run = ct;

    cur_task = t;
    t->run++;

#ifdef USE_ASAN
	if (!t->valid) { // tell the address sanitizer to schedule the task to be removed
		__sanitizer_start_switch_fiber(nullptr, t->ctx->stack, t->ctx->stack_size_bytes);
		//printf("TaskRemove fake_stack=%p stack=%p %s (%s) id=%d\n", t->ctx->fake_stack, t->ctx->stack, t->name, where, t->id);
	} else {         // tell the address sanitizer about the task
		__sanitizer_start_switch_fiber(&(t->ctx->fake_stack), t->ctx->stack, t->ctx->stack_size_bytes);
		//printf("TaskJump  fake_stack=%p stack=%p %s (%s) id=%d\n", t->ctx->fake_stack, t->ctx->stack, t->name, where, t->id);
	}
#endif
    //real_printf("_NextTask LONGJMP %s\n", task_ls(t));
    longjmp(t->ctx->jb, 1);
    panic("longjmp() shouldn't return");
}

int _CreateTask(funcP_t funcP, const char *name, void *param, int priority, u4_t flags, int f_arg)
{
	int i;
    TASK *t;
    u4_t stack_size = flags & CTF_STACK_SIZE;
    
    for (i = TID_FIRST; i < MAX_TASKS; i++) {
        t = Tasks + i;
        u4_t t_stack_size = t->flags & CTF_STACK_SIZE;
        if (!t->valid && ctx[i].init && t_stack_size == stack_size) break;
    }
    if (i == MAX_TASKS) {
        if (flags & CTF_SOFT_FAIL) {
            soft_fail++;
            return -1;
        } else {
            dump();
            lprintf("create_task: stack_size=%s\n",
                (stack_size == CTF_STACK_REG)? "REG" : ((stack_size == CTF_STACK_MED)? "MED" : "LARGE"));
            panic("create_task: no tasks available");
        }
    }
    
	if (i > max_task) max_task = i;
	
	task_init(t, i, funcP, param, name, priority, flags, f_arg);

	return t->id;
}

static void taskSleepSetup(TASK *t, const char *reason, u64_t usec, u4_t *wakeup_test=NULL)
{
	// usec == 0 means sleep until someone does TaskWakeup() on us
	// usec > 0 is microseconds time in future (added to current time)
	
	if (usec > 0) {
    	t->deadline = timer_us64() + usec;
        t->wakeup_test = NULL;
    	kiwi_snprintf_buf(t->reason, "(%.3f msec) ", (float) usec/1000.0);
		evNT(EC_EVENT, EV_NEXTTASK, -1, "TaskSleep", evprintf("sleeping usec %lld %s Qrunnable %d", usec, task_ls(t), t->tq->runnable));
	} else {
	    assert(usec == 0);
        t->wakeup_test = wakeup_test;
		t->deadline = 0;
		if (wakeup_test != NULL) {
            kiwi_snprintf_buf(t->reason, "(test %p) ", wakeup_test);
            evNT(EC_EVENT, EV_NEXTTASK, -1, "TaskSleep", evprintf("sleeping test %p %s Qrunnable %d", wakeup_test, task_ls(t), t->tq->runnable));
		} else {
            strcpy(t->reason, "(evt) ");
            evNT(EC_EVENT, EV_NEXTTASK, -1, "TaskSleep", evprintf("sleeping event %s Qrunnable %d", task_ls(t), t->tq->runnable));
        }
	}
	
    kiwi_strncat(t->reason, reason, N_REASON);

	RUNNABLE_NO(t, /* prev_stopped */ t->stopped? 0:-1);
}

void *_TaskSleep(const char *reason, u64_t usec, u4_t *wakeup_test)
{
    TASK *t = cur_task;

    taskSleepSetup(t, reason, usec, wakeup_test);

	#if 0
        static bool trigger;
        if (t->id == 2 && !trigger) {
            evNT(EC_DUMP, EV_NEXTTASK, 30, "TaskInit", "DUMP 30 MSEC");
            trigger = true;
        }
	#endif

	do {
		NextTask(t->reason);
	} while (!t->wakeup);
    
	evNT(EC_EVENT, EV_NEXTTASK, -1, "TaskSleep", evprintf("woke %s Qrunnable %d", task_ls(t), t->tq->runnable));

    t->deadline = 0;
    t->stopped = FALSE;
	run[t->id].r = 1;
    t->sleeping = FALSE;
	t->wakeup = FALSE;
	return t->wake_param;
}

void TaskSleepID(int id, u64_t usec)
{
    TASK *t = Tasks + id;
    
    if (t == cur_task) return (void) TaskSleepUsec(usec);

    if (!t->valid) return;
	//printf("sleepID %s %d usec %lld\n", task_ls(t), usec);
	evNT(EC_EVENT, EV_NEXTTASK, -1, "TaskSleepID", evprintf("%s usec %lld", task_ls(t), usec));
	assert(cur_task->id != id);
	
	// must not force a task to sleep while it is holding or waiting for a lock -- make it pending instead
	if (t->lock.hold || t->lock.wait) {
		//lprintf("TaskSleepID: pending_sleep =========================================\n");
		t->pending_sleep = TRUE;
		t->pending_usec = usec;
	} else {
    	taskSleepSetup(t, "TaskSleepID", usec);
    }
}

void _TaskWakeup(tid_t tid, u4_t flags, void *wake_param)
{
    TASK *t = Tasks + tid;
    
    if (!t->valid) return;

    // Don't wakeup a task sleeping on a time interval (as opposed to unconditional sleeping)
    // This is generally the default.
    if ((flags & TWF_CANCEL_DEADLINE) == 0 && t->deadline > 0) {
        evNT(EC_EVENT, EV_NEXTTASK, -1, "TaskWakeup", evprintf("%s still deadline of %08x|%08x", task_ls(t), PRINTF_U64_ARG(t->deadline)));
        return;
    }

    t->deadline = 0;	// cancel any outstanding deadline

	if (!t->sleeping) {
		assert(!t->stopped || (t->stopped && t->lock.wait));
		t->pending_sleep = FALSE;
        evNT(EC_EVENT, EV_NEXTTASK, -1, "TaskWakeup", evprintf("%s not already sleeping, wakeup %d", task_ls(t), t->wakeup));
		return;	// not already sleeping
	}
	
	// Should be okay to do this to a third-party task because e.g., in the case of waking up
	// a task holding a lock, the lock code checks in a while loop that the lock is still busy.
	// I.e. a spurious wakeup is tolerated. Need to be careful to repeat this behavior with other
	// sleeping mechanisms.

	evNT(EC_EVENT, EV_NEXTTASK, -1, "TaskWakeup", evprintf("%s", task_ls(t)));
    t->wu_count++;
	if (flags & TWF_CHECK_WAKING) assert(!t->wakeup);
    RUNNABLE_YES(t);
	//printf("wa%d ", t->id); fflush(stdout);
    t->wake_param = wake_param;
    
    // if we're waking up a higher priority task, run it without delay
    if (t->priority > cur_task->priority) {
    	t->interrupted_task = cur_task;		// remember who we interrupted
    	evNT(EC_EVENT, EV_NEXTTASK, -1, "TaskWakeup", evprintf("HIGHER PRIORITY wake %s ...", task_ls(t)));
    	evNT(EC_EVENT, EV_NEXTTASK, -1, "TaskWakeup", evprintf("HIGHER PRIORITY ... from interrupted %s", task_ls(cur_task)));
    	kiwi_snprintf_buf(t->reason, "TaskWakeup: HIGHER PRIO %p", wake_param);
    	NextTask(t->reason);
    }
}

u4_t TaskPriority(int priority)
{
	if (!cur_task) return 0;
	if (priority == -1) return cur_task->priority;
	return cur_task->priority;
}

u4_t TaskID()
{
	if (!cur_task) return 0;
	return cur_task->id;
}

const char *_TaskName(const char *name, bool free_name)
{
    TASK *ct = cur_task;
    
	if (!ct) return "main";
	if (name != NULL) {
        if (ct->flags & CTF_TNAME_FREE) {
            kiwi_asfree((void *) ct->name);
            ct->flags &= ~CTF_TNAME_FREE;
        }
		ct->name = name;
		if (free_name) ct->flags |= CTF_TNAME_FREE;
	}
	return ct->name;
}

void TaskMinRun(u4_t minrun_us)
{
    TASK *t = cur_task;

	t->minrun = minrun_us;
}

u4_t TaskFlags()
{
    TASK *t = cur_task;

	return t->flags;
}

void TaskSetFlags(u4_t flags)
{
    TASK *t = cur_task;

	t->flags = flags;
}

void *TaskGetUserParam()
{
    return cur_task->user_param;
}

void TaskSetUserParam(void *param)
{
    cur_task->user_param = param;
}


// Locks: critical section "first come, first served"
//
// Locks are used to gain exclusive access to resources that will hold across task switching,
// e.g. the spi_lock allows the local buffers to be held across the NextTask() that occurs if
// the SPI request has to be retried due to the eCPU being busy.

//#define LOCK_TEST_DEADLOCK
#ifdef LOCK_TEST_DEADLOCK

lock_t test_lock;

static void lock_test_task1(void *param)
{
	printf("LOCK ENTER lock_test_task1\n");
	lock_enter(&test_lock);
	    TaskSleepSec(10);
	lock_leave(&test_lock);
	printf("LOCK LEAVE lock_test_task1\n");
	while (1) {
	    TaskSleepSec(10);
	}
}

static void lock_test_task2(void *param)
{
    TaskSleepSec(2);
	printf("LOCK ENTER lock_test_task2\n");
	lock_enter(&test_lock);
	printf("LOCK LEAVE lock_test_task2\n");
}

#endif

void _lock_init(lock_t *lock, const char *name)
{
	memset(lock, 0, sizeof(*lock));
	lock->name = name;
	// for NextTask() inside lock_enter()
    kiwi_snprintf_buf(lock->enter_name, "lock enter: %.*s", LEN_ENTER_NAME, name);
	lock->init = true;
	lock->magic_b = LOCK_MAGIC_B;
	lock->magic_e = LOCK_MAGIC_E;
	
	#ifdef LOCK_TEST_DEADLOCK
        static bool lock_test;
        if (!lock_test) {
            lock_test = true;
            lock_init(&test_lock);
            lock_register(&test_lock);
            CreateTask(lock_test_task1, 0, MAIN_PRIORITY);
            CreateTask(lock_test_task2, 0, MAIN_PRIORITY);
        }
    #endif
}

#define	N_LOCK_LIST		512
static int n_lock_list;
static lock_t *locks[N_LOCK_LIST];

void lock_register(lock_t *lock)
{
    int i;
    for (i = 0; i < n_lock_list; i++) {
        if (locks[i] == NULL) {
	        locks[i] = lock;
	        break;
        }
    }
    if (i == n_lock_list) {
        locks[n_lock_list] = lock;
        n_lock_list++;
    }
	check(n_lock_list < N_LOCK_LIST);
	//printf("lock_register %d\n", n_lock_list);
}

void lock_unregister(lock_t *lock)
{
    int i;
    for (i = 0; i < n_lock_list; i++) {
        if (lock == locks[i])
            locks[i] = NULL;
    }
	//printf("lock_unregister %d\n", n_lock_list);
}

void lock_dump()
{
	int i, j, nlocks = 0;
	lock_t *l;

	for (i=0; i < n_lock_list; i++) {
		l = locks[i];
		if (l && l->init) nlocks++;
	}
	lprintf("\n");
	lprintf("LOCKS: used %d/%d previous_prio_inversion=%d LINUX_CHILD_PROCESS=%d %s %s\n",
	    nlocks, N_LOCK_LIST, previous_prio_inversion, LINUX_CHILD_PROCESS()? 1:0, task_ls(cur_task),
	    (cur_task->flags & CTF_FORK_CHILD)? "CTF_FORK_CHILD":"");

	
	u4_t now = timer_sec();
	for (i=0; i < n_lock_list; i++) {
		l = locks[i];
		if (!l || !l->init) continue;
        TASK *owner = (TASK *) l->owner;
        int n_users = l->enter - l->leave;
        lprintf("L%d L%d|E%d (%d) prio_swap=%d prio_inversion=%d time_no_owner=%d \"%s\" \"%s\"",
            i, l->leave, l->enter, n_users, l->n_prio_swap, l->n_prio_inversion,
            l->timer_since_no_owner? (now - l->timer_since_no_owner) : 0, l->name, l->enter_name);
        if (n_users) {
            if (owner)
                lprintf(" held by: %s%s", task_ls(owner),
                    l->timer_since_no_owner? "BUT TIME_NO_OWNER!!!":"");
            else
                lprintf(" held by: no task");
        }
        lprintf("\n");
    
        TASK *t = Tasks;
        bool waiters = false;
        for (j=0; j <= max_task; j++) {
            if (t->lock.wait == l) {
                if (!waiters) {
                    lprintf("   waiters:");
                    waiters = true;
                }
                lprintf(" %s", task_s(t));
            }
            t++;
        }
        if (waiters)
            lprintf(" \n");
	}
}

#ifdef LOCK_CHECK_HANG

// Check for hung condition: there are waiters on a lock, but no task has owned the lock in a long time.
bool lock_check()
{
	int i;
	
	bool lock_panic = false;
	for (i=0; i < n_lock_list; i++) {
		lock_t *l = locks[i];
		if (!l || !l->init) continue;
		if (l->timer_since_no_owner == 0) continue;
		u4_t time_since_no_owner = timer_sec() - l->timer_since_no_owner;
		if (time_since_no_owner <= LOCK_HUNG_TIME) continue;
		if (l->owner != NULL) {
            int n_waiters = l->enter - l->leave -1;
            lprintf("lock_check: BAD OWNER TIME? \"%s\" (%d waiters) owner time > %d seconds, but HAS an owner %s\n",
                l->name, n_waiters, LOCK_HUNG_TIME, task_s((TASK *) l->owner));
		} else {
            int n_waiters = l->enter - l->leave;
            lprintf("lock_check: HUNG LOCK? \"%s\" (%d waiters) has had no owner for > %d seconds\n",
                l->name, n_waiters, LOCK_HUNG_TIME);
        }
        lock_panic = true;
	}
	return lock_panic;
}
#endif

#define check_lock(lock) \
	if (lock->magic_b != LOCK_MAGIC_B || lock->magic_e != LOCK_MAGIC_E) \
		lprintf("*** BAD LOCK MAGIC 0x%x 0x%x\n", lock->magic_b, lock->magic_e);

#ifdef EV_MEAS_LOCK
    static bool trace_spi_lock = true;
	#define evLock2(c, e, p, s, s2) \
	    if (lock != &spi_lock || trace_spi_lock) \
	        ev(c, e, p, s, s2)
#else
	#define evLock2(c, e, p, s, s2)
#endif

// don't disable: spi_pump() will deadlock without LOCK_PRIORITY_SWAP
#define LOCK_PRIORITY_INVERSION
#define LOCK_PRIORITY_SWAP

void lock_enter(lock_t *lock)
{
	if (!lock->init) return;
	check_lock(lock);
	TASK *ct = cur_task;
	
	if (ct->lock.hold != NULL) {
		lprintf("lock_enter: %s %s already holding lock %s ?\n",
			lock->name, task_ls(ct), ct->lock.hold->name);
		panic("double lock");
	}
	
    assert(ct->lock.wait == NULL);
    assert(ct->lock.next == NULL && ct->lock.prev == NULL);

    // add us to lock users list
    TASK *cur = (TASK *) lock->users;
    if (cur) {
        ct->lock.next = cur;
        cur->lock.prev = ct;
    } else {
        ct->lock.next = NULL;
    }
    ct->lock.prev = NULL;
    lock->users = ct;

    u4_t token = lock->enter++;
    ct->lock.token = token;

    while (token > lock->leave) {
        assert(ct->lock.hold == NULL);

        evLock2(EC_EVENT, EV_NEXTTASK, -1, "lock_enter", evprintf("WAIT %s %s L%d|%d|E%d",
            lock->name, task_ls(ct), lock->leave, token, lock->enter));

        // If we're higher priority than lowest token waiter swap tokens with them so we'll run before they do.
        // Remember that locks are serviced in token-order independent of task priority.
        // That's why we must change tokens to reflect our greater priority.
    #ifdef LOCK_PRIORITY_SWAP
        TASK *head = (TASK *) lock->users;
        if (head) {
            TASK *owner = (TASK *) lock->owner;
            if (owner) assert(owner->lock.token == lock->leave);
            u4_t lowest_token = lock->leave + (owner? 1:0);
            TASK *lp = head;
            while (lp != NULL) {
                if (lp->lock.token == lowest_token)
                    break;
                lp = lp->lock.next;
            }
            assert(lp != NULL);

            if (lp != NULL && ct->priority > lp->priority) {    // lp check to keep static analyzer quiet
                #ifdef EV_MEAS_LOCK
                    int n_waiters = lock->enter - lock->leave -1;
                    evLock(EC_EVENT, EV_NEXTTASK, -1, "lock_enter", evprintf("SWAP %s L%d|%d|E%d (%d)", lock->name, lock->leave, token, lock->enter, n_waiters));
                    TASK *ow = owner;
                    if (ow) evLock(EC_EVENT, EV_NEXTTASK, -1, "lock_enter", evprintf("SWAP ow %s", task_s(ow)));
                    evLock(EC_EVENT, EV_NEXTTASK, -1, "lock_enter", evprintf("SWAP ct %s", task_s(ct)));

                    bool okay = true;
                    TASK *tp = Tasks;
                    for (int i=0; i <= max_task; i++, tp++) {
                        if (!tp->valid || tp->lock.wait != lock) continue;
                        u4_t tok = tp->lock.token;
                        if (tp == lp) okay = (tok == (lock->leave + (ow? 1:0)));
                        evLock(EC_EVENT, EV_NEXTTASK, -1, "lock_enter", evprintf("SWAP %s %s%s", (tp == lp)? (okay? "S#" : "S*") :"tp",
                            task_s(tp), (tok == lock->leave + (ow? 1:0))? "#":""));
                    }
                    if (!okay && ev_dump == 1) evLock(EC_DUMP, EV_NEXTTASK, ev_dump, "lock_enter", evprintf("SWAP DUMP IN %.3f SEC", ev_dump/1000.0));
                #endif

                lp->lock.token = token;
                token = ct->lock.token = lowest_token;
                lock->n_prio_swap++;
                
                // Don't wait right away: it's possible we can now immediately acquire lock with our new lowest token value.
                // So check first.
                continue;       
            }
        }
    #endif
        
        TASK *ow = (TASK *) lock->owner;
        
        #ifdef EV_MEAS_LOCK
            if (ow) {
                evLock2(EC_EVENT, EV_NEXTTASK, -1, "lock_enter", evprintf("WAIT %s held by %s",
                    lock->name, task_ls(ow)));
            } else {
                evLock2(EC_EVENT, EV_NEXTTASK, -1, "lock_enter", evprintf("WAIT %s not held", lock->name));
            }
        #endif

		ct->lock.wait = lock;
		ct->stopped = TRUE;
		run[ct->id].r = 0;
		runnable(ct->tq, -1);
		
		// a lock _waiter_ should never be sleeping
		assert(ct->sleeping == FALSE);

#ifdef LOCK_PRIORITY_INVERSION
        #if 0
            if (ow && ct->priority > ow->priority && (ow->flags & CTF_NO_PRIO_INV)) {
                printf("### FYI: lock %s ct %s\n", lock->name, task_ls(ct));
                printf("### FYI: CTF_NO_PRIO_INV %s\n", task_ls(ow));
            }
        #endif
		if (ow && ct->priority > ow->priority && (ow->flags & CTF_NO_PRIO_INV) == 0) {
		    assert(ow->lock.hold == lock);
		    
		    if (ow->flags & CTF_PRIO_INVERSION) {
		        previous_prio_inversion++;      // someone else got there first, so leave the situation as is
		        //lprintf("### LOCK_PRIORITY_INVERSION: lock %s ct %s\n", lock->name, task_ls(ct));
		        //lprintf("### LOCK_PRIORITY_INVERSION: CTF_PRIO_INVERSION (saved=P%03d) already set in owner %s\n", ow->saved_priority, task_ls(ow));
		    } else {
                // priority inversion: temp raise priority of lock owner to our priority so it releases the lock faster
                ow->saved_priority = ow->priority;
                //printf("### LOCK_PRIORITY_INVERSION raising owner %s ...\n", task_s(ow));
                //printf("### LOCK_PRIORITY_INVERSION ... to match ct %s\n", task_s(ct));
                TdeQ(ow);
                TenQ(ow, ct->priority);
                ow->flags |= CTF_PRIO_INVERSION;
                lock->n_prio_inversion++;
            }
		    
            evLock(EC_EVENT, EV_NEXTTASK, -1, "lock_enter", evprintf("add PRIO INV %s held by %s orig prio %d",
                lock->name, task_ls(ow), ow->saved_priority));
		}
#endif
		
        ct->lock.waiting = true;        // we'll need to be woken up
    	NextTask(lock->enter_name);
    	token = ct->lock.token;         // in case changed by priority swapping

        evLock2(EC_EVENT, EV_NEXTTASK, -1, "lock_enter", evprintf("WOKEUP %s %s L%d|%d|E%d",
            lock->name, task_ls(ct), lock->leave, token, lock->enter));

		check_lock(lock);
    }
    
    assert(token == lock->leave);	// check that we really own lock
    
    lock->owner = ct;
    lock->timer_since_no_owner = 0;
	ct->lock.wait = NULL;
    ct->lock.hold = lock;

    #if defined(LOCK_CHECK_HANG) && defined(EV_MEAS_LOCK)
        if (lock == &spi_lock) {
            expecting_spi_lock_next_task = 0;
            evLock2(EC_EVENT, EV_NEXTTASK, -1, "lock_enter", evprintf("set expecting_spi_lock_next_task=0, %s", task_s(ct)));
        }
    #endif

    evLock2(EC_EVENT, EV_NEXTTASK, -1, "lock_enter", evprintf("ACQUIRE %s %s L%d|%d|E%d",
        lock->name, task_ls(ct), lock->leave, token, lock->enter));
}

void lock_leave(lock_t *lock)
{
	TASK *ct = cur_task;
	if (!lock->init) return;
	check_lock(lock);

    // remove priority inversion compensation
#ifdef LOCK_PRIORITY_INVERSION
    if (ct->flags & CTF_PRIO_INVERSION) {
        // priority inversion: temp raise priority of lock owner to our priority
        evLock(EC_EVENT, EV_NEXTTASK, -1, "lock_leave", evprintf("rem PRIO INV %s held by %s orig prio %d",
            lock->name, task_ls(ct), ct->saved_priority));
        TdeQ(ct);
        TenQ(ct, ct->saved_priority);
        ct->saved_priority = 0;
        ct->flags &= ~CTF_PRIO_INVERSION;
        if (ev_dump == 2) evLock(EC_DUMP_CONT, EV_NEXTTASK, ev_dump, "prio inv", evprintf("DUMP IN %.3f SEC", ev_dump/1000.0));
    }
#endif
		
    lock->leave++;      // unlock or select next waiter (if any) to run
    assert(ct->lock.wait == NULL);
    assert(ct->lock.hold == lock);
    ct->lock.hold = NULL;
    ct->lock.token = 0;

    evLock2(EC_EVENT, EV_NEXTTASK, -1, "lock_leave", evprintf("RELEASE %s %s L%d|E%d",
        lock->name, task_ls(ct), lock->leave, lock->enter));

    // wake up the ones that are waiting and let lock_enter() pick next one to acquire lock
	bool wake_higher_priority = false;
	
	bool removed_ourselves = false;
    int n_waiters = 0;
    TASK *head = (TASK *) lock->users;
    TASK *tp = head;
    while (tp != NULL) {
        
        // remove ourselves from lock users list
        if (tp == ct) {
            TASK *next = tp->lock.next;
            TASK *prev = tp->lock.prev;
            if (tp == head) {
                assert(prev == NULL);
                tp->lock.next = NULL;
                head = tp = next;
                if (head) head->lock.prev = NULL;
                lock->users = head;
            } else {
                assert(prev != NULL);
                tp->lock.prev = NULL;
                tp->lock.next = NULL;
                prev->lock.next = next;
                if (next) next->lock.prev = prev;
                tp = next;
            }
            removed_ourselves = true;
            continue;
        }
        
        assert(tp->lock.wait == lock);
        n_waiters++;

        // A task can be not waiting, and hence not in need of a wake up, if it was woken up in
        // a previous run of this loop from some other task releasing the lock.
        // This can happen when there are 3 or more tasks competing for the same lock.
        
    #ifdef LOCK_TEST_DEADLOCK
        if (tp->lock.waiting && lock != &test_lock) {
    #else
        if (tp->lock.waiting) {
    #endif
            tp->lock.waiting = false;
            tp->stopped = FALSE;
            
            // The lock _owner_ can subsequently sleep after acquiring the lock.
            // But a _waiter_ should never be sleeping.
            assert(tp->sleeping == FALSE);
            
            run[tp->id].r = 1;
            runnable(tp->tq, 1);
            
            if (tp->priority > ct->priority) wake_higher_priority = true;
    
            evLock2(EC_EVENT, EV_NEXTTASK, -1, "lock_leave", evprintf("WAKEUP %s %s", lock->name, task_ls(tp)));
        } else {
            evLock2(EC_EVENT, EV_NEXTTASK, -1, "lock_leave", evprintf("not waiting %s %s", lock->name, task_ls(tp)));
        }
        
    	tp = tp->lock.next;
    }
    
    assert(removed_ourselves == true);
	lock->owner = NULL;
    
    evLock2(EC_EVENT, EV_NEXTTASK, -1, "lock_leave", evprintf("EXIT %s %s %s, %d waiters",
        lock->name, task_ls(ct), ct->pending_sleep? "pending_sleep" : (wake_higher_priority? "HIGHER_PRIORITY" : "normal_exit"), n_waiters));

    if (lock->enter == lock->leave) {
        // no waiters on lock
        
        assert(lock->users == NULL);
        assert(n_waiters == 0);
        lock->enter = lock->leave = 0;      // reset so they don't potentially wrap
        lock->timer_since_no_owner = 0;

        #if defined(LOCK_CHECK_HANG) && defined(EV_MEAS_LOCK)
            if (lock == &spi_lock && !LINUX_CHILD_PROCESS()) {
                expecting_spi_lock_next_task = 0;
                evLock2(EC_EVENT, EV_NEXTTASK, -1, "lock_leave", evprintf("$$$$ in lock_leave set expecting_spi_lock_next_task=0, %s %d waiters", task_s(ct), n_waiters));
            }
        #endif
    } else {
        // waiters on lock

        lock->timer_since_no_owner = timer_sec();

        #if defined(LOCK_CHECK_HANG) && defined(EV_MEAS_LOCK)
            if (lock == &spi_lock && !LINUX_CHILD_PROCESS()) {
                expecting_spi_lock_next_task = 50;
                evLock2(EC_EVENT, EV_NEXTTASK, -1, "lock_leave", evprintf("$$$ set expecting_spi_lock_next_task=50, %s %d waiters", task_s(ct), n_waiters));
            }
        #endif

        #ifdef LOCK_TEST_HANG
            static int lock_test_hang_ct;
            if (lock == &spi_lock) {
                lock_test_hang_ct++;
                if (lock_test_hang_ct <= 20) printf("lock_test_hang_ct %d\n", lock_test_hang_ct);
                if (lock_test_hang_ct == 20) {
                    printf("**** LOCK_TEST_HANG ****\n");
                    //lock_test_hang = 1;
                    lock_panic = true;
                }
            }
        #endif
    }
    
    // If another task tried to TaskSleepID() us, but we were made pending because we were holding a lock,
    // then must TaskSleep()/NextTask() here.
    if (ct->pending_sleep) {
        ct->pending_sleep = FALSE;
        TaskSleepReasonUsec("lock_leave TaskSleepID", ct->pending_usec);
    } else {
        // If we're waking up a higher priority task, run it without delay. Similar to strategy in TaskWakeup()
        if (wake_higher_priority) NextTask("lock_leave HIGHER PRIORITY");
    }
}

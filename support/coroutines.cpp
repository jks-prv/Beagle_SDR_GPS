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

// Copyright (c) 2014-2017 John Seamons, ZL/KF6VO

#include "types.h"
#include "kiwi.h"
#include "config.h"
#include "misc.h"
#include "str.h"
#include "coroutines.h"
#include "debug.h"
#include "peri.h"
#include "spi.h"

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
	kernel. But valgrind doesn't work with either thread initialization method on Debian 8
	for unknown reasons. The valgrind facility for declaring private stacks seems to be broken.

	In the end we compiled a recent version of valgrind on Debian 7 to get around the gdb issue.
	But note that the signal method doesn't work with valgrind even on Debian 7 (reasons unknown).

	Both methods are included in the code. The jmp_buf method with de-mangling is currently the default.

*/

#ifdef DEVSYS
	//#define SETUP_TRAMP_USING_JMP_BUF
#else
	#define SETUP_TRAMP_USING_JMP_BUF
#endif

#if defined(HOST) && defined(USE_VALGRIND)
	#include <valgrind/valgrind.h>
#endif

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

// FIXME: 32K is really too big. 8*K causes the W/F thread to exceed the 50% red zone. Need to optimize.
#define STACK_SIZE_U64_T	(32*K)

struct Stack {
	u64_t elem[STACK_SIZE_U64_T];
} __attribute__ ((aligned(256)));

static Stack stacks[MAX_TASKS];

struct ctx_t {
	int id;
	bool init;
    u64_t *stack, *stack_last;
    bool valgrind_stack_reg;
	int valgrind_stack_id;
	union {
		jmp_buf jb;
		struct {
			#if defined(__x86_64__)
				u4_t x1, fp, sp, x2[4], pc;
			#endif
			#if defined(__ARM_EABI__)
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
	bool marked, runnable;
	u4_t flags;
	u4_t saved_priority;

	TASK *interrupted_task;
	s64_t deadline;
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
	u4_t usec, pending_usec, longest, hist[N_HIST];
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
};

static bool task_package_init;
static int max_task;
static TASK Tasks[MAX_TASKS], *cur_task, *last_task_run, *busy_helper_task, *itask;
static ctx_t ctx[MAX_TASKS]; 
static TaskQ_t TaskQ[NUM_PRIORITY];
static u64_t last_dump;
static u4_t idle_us;
static u4_t task_all_hist[N_HIST];

static int itask_tid;
static u64_t itask_last_tstart;

static char *task_s(TASK *tp)
{
    if (tp->lock.wait != NULL || tp->lock.hold != NULL)
        return stprintf("%s:P%d:T%02d|K%d", tp->name? tp->name:"?", tp->priority, tp->id, tp->lock.token);
    else
        return stprintf("%s:P%d:T%02d", tp->name? tp->name:"?", tp->priority, tp->id);
}

char *Task_s(int id)
{
    TASK *t = Tasks + id;
    return task_s(t);
}

static char *task_ls(TASK *tp)
{
    if (tp->lock.wait != NULL || tp->lock.hold != NULL)
        return stprintf("%s:P%d:T%02d(%s)|K%d", tp->name? tp->name:"?", tp->priority, tp->id, tp->where? tp->where : "-", tp->lock.token);
    else
        return stprintf("%s:P%d:T%02d(%s)", tp->name? tp->name:"?", tp->priority, tp->id, tp->where? tp->where : "-");
}

char *Task_ls(int id)
{
    TASK *t = Tasks + id;
    return task_ls(t);
}

//#define USE_RUNNABLE

void runnable(TaskQ_t *tq, int chg)
{
	#ifdef USE_RUNNABLE
		int runnable = tq->runnable;
		if(!(runnable >= 0 && runnable <= tq->count && runnable <= MAX_TASKS))
		assert_dump(runnable >= 0 && runnable <= tq->count && runnable <= MAX_TASKS);
	
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
	
		runnable = tq->runnable;
		if(!(runnable >= 0 && runnable <= tq->count && runnable <= MAX_TASKS))
		assert_dump(runnable >= 0 && runnable <= tq->count && runnable <= MAX_TASKS);
	#endif
}

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
	
	TASK *ct = cur_task;

	int tused = 0;
	for (i=0; i <= max_task; i++) {
		t = Tasks + i;
		if (t->valid && ctx[i].init) tused++;
		if (flags & TDUMP_CLR_HIST)
		    memset(t->hist, 0, sizeof(t->hist));
	}

    if (flags & TDUMP_CLR_HIST) {
        memset(task_all_hist, 0, sizeof(task_all_hist));
        lfprintf(printf_type, "HIST: cleared\n");
        return;
    }

	ct->flags |= CTF_NO_CHARGE;     // don't charge the current task with the time to print all this

	lfprintf(printf_type, "\n");
	lfprintf(printf_type, "TASKS: used %d/%d, spi_retry %d, spi_delay %d\n", tused, MAX_TASKS, spi_retry, spi_delay);

    const char *hist_name[N_HIST] = { "<1", "1", "2", "4", "8", "16", "32", "64", "128", "256", "512", ">=1k" };
    
    if (flags & TDUMP_HIST) {
        lfprintf(printf_type, "HIST: ");
        for (j = 0; j < N_HIST; j++) {
            if (task_all_hist[j])
                lfprintf(printf_type, "%d|%sm ", task_all_hist[j], hist_name[j]);
        }
        lfprintf(printf_type, " \n");
    }

	if (flags & TDUMP_LOG)
	//lfprintf(printf_type, "Ttt Pd# cccccccc xxx.xxx xxxxx.xxx xxx.x%% xxxxxxx xxxxx xxxxx xxx xxxxx xxx xxxx.xxxu xxx%%\n");
	  lfprintf(printf_type, "        RWSPBLHq   run S    max mS      %%   #runs  cmds   st1       st2      deadline stk%% task______ where___________________\n");
	else
	//lfprintf(printf_type, "Ttt Pd# cccccccc xxx.xxx xxxxx.xxx xxx.x%% xxxxxxx xxxxx xxxxx xxx xxxxx xxx xxxxx xxxxx xxxxx xxxx.xxxu xxx%%\n");
	  lfprintf(printf_type, "        RWSPBLHq   run S    max mS      %%   #runs  cmds   st1       st2       #wu   nrs retry  deadline stk%% task______ where___________________ longest ________________\n");

	for (i=0; i <= max_task; i++) {
		t = Tasks + i;
		if (!t->valid)
			continue;
		float f_usec = ((float) t->usec) / 1e6;
		f_sum += f_usec;
		float f_longest = ((float) t->longest) / 1e3;

		float deadline=0;
		char dunit = ' ';
		if (t->deadline > 0) {
			deadline = (t->deadline > now_us)? (float) (t->deadline - now_us) : 9999999;
			deadline /= 1e3;    // mmm.uuu msec
			dunit = 'm';
			if (deadline >= 10000) {
			    deadline /= 60000;      // mmmm.sss min
			    dunit = 'M';
			} else
			if (deadline >= 1000) {
			    deadline /= 1000;       // ssss.mmm sec
			    dunit = 's';
			}
		}

		if (flags & TDUMP_LOG)
		lfprintf(printf_type, "T%02d P%d%c %c%c%c%c%c%c%c%c %7.3f %9.3f %5.1f%% %7d %5d %5d %-3s %5d %-3s %8.3f%c %3d%% %-10s %-24s\n",
		    i, t->priority, t->marked? (t->runnable? '*':'#') : ' ',
			t->stopped? 'T':'R', t->wakeup? 'W':'_', t->sleeping? 'S':'_', t->pending_sleep? 'P':'_', t->busy_wait? 'B':'_',
			t->lock.wait? 'L':'_', t->lock.hold? 'H':'_', t->minrun? 'q':'_',
			f_usec, f_longest, f_usec/f_elapsed*100,
			t->run, t->cmds,
			t->stat1, t->units1? t->units1 : " ", t->stat2, t->units2? t->units2 : " ",
			deadline, dunit, t->stack_hiwat*100 / STACK_SIZE_U64_T,
			t->name, t->where? t->where : "-"
		);
		else
		lfprintf(printf_type, "T%02d P%d%c %c%c%c%c%c%c%c%c %7.3f %9.3f %5.1f%% %7d %5d %5d %-3s %5d %-3s %5d %5d %5d %8.3f%c %3d%% %-10s %-24s %-24s\n",
		    i, t->priority, t->marked? (t->runnable? '*':'#') : ' ',
			t->stopped? 'T':'R', t->wakeup? 'W':'_', t->sleeping? 'S':'_', t->pending_sleep? 'P':'_', t->busy_wait? 'B':'_',
			t->lock.wait? 'L':'_', t->lock.hold? 'H':'_', t->minrun? 'q':'_',
			f_usec, f_longest, f_usec/f_elapsed*100,
			t->run, t->cmds,
			t->stat1, t->units1? t->units1 : " ", t->stat2, t->units2? t->units2 : " ",
			t->wu_count, t->no_run_same, t->spi_retry,
			deadline, dunit, t->stack_hiwat*100 / STACK_SIZE_U64_T,
			t->name, t->where? t->where : "-",
			t->long_name? t->long_name : "-"
		);
		
		if (flags & TDUMP_HIST) {
		    for (j = 0; j < N_HIST; j++) {
		        if (t->hist[j])
		            lfprintf(printf_type, "%d|%sm ", t->hist[j], hist_name[j]);
		    }
		    lfprintf(printf_type, " \n");
		}

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
		if (detail) lfprintf(printf_type, " \n");

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
	//lfprintf(printf_type, "Ttt Pd cccccccc xxx.xxx xxxxx.xxx xxx.x%%
	  lfprintf(printf_type, "idle            %7.3f           %5.1f%%\n", f_idle, f_pct);
	float f_remain = fabsf(f_elapsed - f_sum);
	f_pct = f_remain/f_elapsed*100;
	//if (f_remain > 0.01)
	//lfprintf(printf_type, "Ttt Pd cccccccc xxx.xxx xxxxx.xxx xxx.x%%
	  lfprintf(printf_type, "Linux           %7.3f           %5.1f%%\n", f_remain, f_pct);
}

int TaskStatU(u4_t s1_func, int s1_val, const char *s1_units, u4_t s2_func, int s2_val, const char *s2_units)
{
	int r=0;
    TASK *t = cur_task;
    
    t->s1_func |= s1_func & TSTAT_LATCH;
    if (s1_units) t->units1 = s1_units;
    switch (s1_func & TSTAT_MASK) {
    	case TSTAT_NC: break;
    	case TSTAT_SET: t->stat1 = s1_val; break;
    	case TSTAT_INCR: t->stat1++; break;
    	case TSTAT_MAX: if (s1_val > t->stat1) t->stat1 = s1_val; break;
    	default: break;
    }

    t->s2_func |= s2_func & TSTAT_LATCH;
    if (s2_units) t->units2 = s2_units;
    switch (s2_func & TSTAT_MASK) {
    	case TSTAT_NC: break;
    	case TSTAT_SET: t->stat2 = s2_val; break;
    	case TSTAT_INCR: t->stat2++; break;
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
	c->stack = (u64_t *) (stacks + id);
	c->stack_last = (u64_t *) (stacks + (id + 1));
	//printf("task_stack T%d %p-%p\n", id, c->stack, c->stack_last);

#if defined(HOST) && defined(USE_VALGRIND)
	if (RUNNING_ON_VALGRIND) {
		if (!c->valgrind_stack_reg) {
			c->valgrind_stack_id = VALGRIND_STACK_REGISTER(c->stack, c->stack_last);
			c->valgrind_stack_reg = true;
		}

		return;
	}
#endif

	u64_t *s = c->stack;
	u64_t magic = 0x8BadF00d00000000ULL;
	int i;
	for (s = c->stack, i=0; s < c->stack_last; s++, i++) {
		*s = magic | ((u64_t) s & 0xffffffff);
		//if (i == (STACK_SIZE_U64_T-1)) printf("T%02d W %08x|%08x\n", c->id, PRINTF_U64_ARG(*s));
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

	for (i=1; i < MAX_TASKS; i++) {		// NB: start at task 1 as the main task uses the ordinary stack
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
		stack.ss_size = sizeof (Stack);
		stack.ss_sp = (void *) c->stack;
		scall("sigaltstack", sigaltstack(&stack, 0));
			 
		struct sigaction sa;
		sa.sa_handler = &trampoline;
		sa.sa_flags = SA_ONSTACK;
		sigemptyset(&sa.sa_mask);
		scall("sigaction", sigaction(SIGUSR2, &sa, 0));
		
		new_ctx = c;
		raise(SIGUSR2);
#endif

		c->init = TRUE;
	}
}

static int our_pid, kiwi_server_pid;

void TaskInit()
{
    TASK *t;
	
	kiwi_server_pid = getpid();
	printf("TASK MAX_TASKS %d, stack memory %d kB, stack size %d k u64_t\n", MAX_TASKS, sizeof(stacks)/K, STACK_SIZE_U64_T/K);

	t = Tasks;
	cur_task = t;
	task_init(t, 0, NULL, NULL, "main", MAIN_PRIORITY, 0, 0);
	last_dump = t->tstart_us = timer_us64();
	//if (ev_dump) evNT(EC_DUMP, EV_NEXTTASK, ev_dump, "TaskInit", evprintf("DUMP IN %.3f SEC", ev_dump/1000.0));
	for (int p = LOWEST_PRIORITY; p <= HIGHEST_PRIORITY; p++) TaskQ[p].p = p;	// debugging aid
	
#ifdef SETUP_TRAMP_USING_JMP_BUF
	find_key();
	printf("TASK jmp_buf demangle key 0x%08x\n", key);
	//setjmp(ctx[0].jb);
	//printf("JMP_BUF: key 0x%x sp 0x%x:0x%x pc 0x%x:0x%x\n", key, ctx[0].sp, ctx[0].sp^key, ctx[0].pc, ctx[0].pc^key);
#endif

	collect_needed = TRUE;
	TaskCollect();
	
	task_package_init = TRUE;
}

void TaskCheckStacks()
{
	int i, j;
	TASK *t;
	u64_t *s;
	

#if defined(HOST) && defined(USE_VALGRIND)
	// as you'd expect, valgrind gets real upset about our stack checking scheme
	if (RUNNING_ON_VALGRIND)
		return;
#endif

	u64_t magic = 0x8BadF00d00000000ULL;
	for (i=1; i <= max_task; i++) {
		t = Tasks + i;
		if (!t->valid || !t->ctx->init) continue;
		int m, l = 0, u = STACK_SIZE_U64_T-1;
		
		// bisect stack until high-water mark is found
		do {
			m = l + (u-l)/2;
			s = &t->ctx->stack[m];
			if (*s == (magic | ((u64_t) s & 0xffffffff))) l = m; else u = m;
		} while ((u-l) > 1);
		
		int used = STACK_SIZE_U64_T - m - 1;
		t->stack_hiwat = used;
		int pct = used*100/STACK_SIZE_U64_T;
		if (pct >= 50) {
			printf("DANGER: %s stack used %d/%d (%d%%)\n", task_s(t), used, STACK_SIZE_U64_T, pct);
			panic("TaskCheckStacks");
		}
	}
}

bool itask_run;
static ipoll_from_e last_from = CALLED_FROM_INIT;
static const char *poll_from[] = { "INIT", "NEXTTASK", "LOCK", "SPI", "FASTINTR" };

void TaskPollForInterrupt(ipoll_from_e from)
{
	if (!itask) {
		return;
	}

	if (GPIO_READ_BIT(GPIO0_15) && itask->sleeping) {
		evNT(EC_TRIG1, EV_NEXTTASK, -1, "PollIntr", evprintf("CALLED_FROM_%s TO INTERRUPT TASK <===========================",
			poll_from[from]));

		// can't call TaskWakeup() from within NextTask()
		if (from == CALLED_WITHIN_NEXTTASK) {
			itask->wu_count++;
			itask->stopped = FALSE;
			run[itask->id].r = 1;
			runnable(itask->tq, 1);
			itask->sleeping = FALSE;
			itask->wakeup = TRUE;
		} else {
			TaskWakeup(itask_tid, false, 0);
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
	if (GPIO_READ_BIT(GPIO0_15)) {
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
    
    if (t->flags & CTF_TNAME_FREE)
        free((void *) t->name);

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
}

bool TaskIsChild()
{
	if (our_pid == 0 || kiwi_server_pid == 0) return false;
	return (our_pid != kiwi_server_pid);
}

// doesn't work as expected
//#define LOCK_TEST_HANG
#ifdef LOCK_TEST_HANG
    static int lock_test_hang;
#endif

#define LOCK_CHECK_HANG
#ifdef LOCK_CHECK_HANG
    static bool lock_panic;
#endif
#if defined(LOCK_CHECK_HANG) && defined(EV_MEAS_LOCK)
    static int expecting_spi_lock_next_task;
#endif

#ifdef DEBUG
 void _NextTask(const char *where, u4_t param, u_int64_t pc)
#else
 void _NextTask(u4_t param)
#endif
{
    if (!task_package_init) return;
    
	int i;
    TASK *t, *tn, *ct;
    u64_t now_us, enter_us = timer_us64();
    u4_t quanta;

	ct = cur_task;
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
        for (i = 0; i < (N_HIST-1) && ms; i++) {
            ms >>= 1;
        }
        ct->hist[i]++;
        task_all_hist[i]++;
    }
    
    our_pid = getpid();

    if (quanta > ct->longest) {
    	ct->longest = quanta;
    	ct->long_name = where;
    	
		// fixme: remove at some point
		if (quanta > 2000000 && ct->id != 0) {
			printf("LRUN %s %s %7.3f\n", task_ls(ct), where, (float) quanta / 1000);
			//evNT(EC_DUMP, EV_NEXTTASK, -1, "NT", "LRUN");
		}
    }

	ct->where = where;

    // don't switch until minrun expired (if any)
    if (ct->minrun && ((enter_us - ct->minrun_start_us) < ct->minrun))
    	return;
    
    // detect an SPI busy-waiting task that needs the assistance of the SPI busy-helper task
    bool no_run_same = false;
    if (param == NT_BUSY_WAIT) {
    	if (ct->busy_wait) {
    		if (busy_helper_task) {
				busy_helper_task->wu_count++;
				busy_helper_task->stopped = FALSE;
				
				run[busy_helper_task->id].r = 1;
				runnable(busy_helper_task->tq, 1);
				
				busy_helper_task->sleeping = FALSE;
				busy_helper_task->wakeup = TRUE;
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
	evNT(EC_EVENT, EV_NEXTTASK, -1, "NextTask", "looking for task to run ...");

    // mark ourselves as "last run" on our task queue if we're still valid
	assert(ct->tq == &TaskQ[ct->priority]);
	ct->tq->last_run = ct->valid? &ct->tll : NULL;

    do {
		// update scheduling deadlines
		TASK *tp = Tasks;
		now_us = timer_us64();
		for (i=0; i <= max_task; i++, tp++) {
			if (!tp->valid) continue;
			bool wake = false;
			if (tp->deadline > 0) {
				if (tp->deadline < now_us) {
					wake = true;
					evNT(EC_EVENT, EV_NEXTTASK, -1, "NextTask", evprintf("deadline expired %s, Qrunnable %d", task_s(tp), tp->tq->runnable));
				}
			}
			
			if (wake) {
				tp->deadline = 0;
				tp->stopped = FALSE;
				run[tp->id].r = 1;
				runnable(tp->tq, 1);
				tp->sleeping = FALSE;
				tp->wakeup = TRUE;
			}
		}

		TaskPollForInterrupt(CALLED_WITHIN_NEXTTASK);
		
		// check for lock deadlock
        #ifdef LOCK_CHECK_HANG
            static u64_t lock_check_us;
            if (now_us > lock_check_us) {   // limit calls to lock_check()
                lock_check_us = now_us + SEC_TO_USEC(1);
                lock_panic = lock_check();
                
                // mark tasks considered during search and print in task dump to help diagnose task queueing issues
                if (lock_panic) {
                    TASK *tp = Tasks;
                    for (i=0; i <= max_task; i++, tp++) {
                        if (!tp->valid) continue;
                        tp->marked = tp->runnable = false;
                    }
                }
            }
        #endif
    
   		 // search task queues
		for (p = HIGHEST_PRIORITY; p >= LOWEST_PRIORITY; p--) {
			head = &TaskQ[p];
			assert(p == head->p);
			
			#ifdef USE_RUNNABLE
				int runnable = head->runnable;
				//evNT(EC_EVENT, EV_NEXTTASK, -1, "NextTask", evprintf("looking P%d, count %d, runnable %d",
				//	p, head->count, runnable));
				if (!(runnable >= 0 && runnable <= head->count && runnable <= MAX_TASKS))
					assert(runnable >= 0 && runnable <= head->count && runnable <= MAX_TASKS);
			#else
				int runnable = 0;
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
					TASK *t = tll->t;
					assert(t);
					assert(t->valid);
					
                    #ifdef LOCK_CHECK_HANG
				        if (lock_panic) lprintf("P%d: %s\n", p, task_s(t));
                    #endif
					
					if (!t->stopped)
						runnable++;
					
					#ifdef LOCK_CHECK_HANG
                        if (lock_panic && t == busy_helper_task)
                            lprintf("P%d: busy_helper_task stopped %d lock.wait %d\n", p, t->stopped, t->lock.wait);
                    #endif
				}

                #ifdef LOCK_CHECK_HANG
				    if (lock_panic) lprintf("P%d: runnable %d\n", p, runnable);
				#endif
			#endif

			if (p == ct->priority && runnable == 1 && no_run_same) {
				evNT(EC_EVENT, EV_NEXTTASK, -1, "NextTask", evprintf("%s no_run_same TRIGGERED ***", task_s(ct)));
				no_run_same = false;
				continue;
			}
			
			if (runnable) {
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
					assert(t->valid);
					
					#ifdef LOCK_CHECK_HANG
					    if (lock_panic) t->marked = true;
					#endif
					
					// ignore all tasks in children after fork() from child_task() unless marked
					if (our_pid != kiwi_server_pid && !(t->flags & CTF_FORK_CHILD)) {
						//printf("norun fork %s\n", task_ls(t));
						;
					} else

					if (!t->stopped && t->long_run) {
						u4_t last_time_run = now_us - itask_last_tstart;
						if (!itask || !itask_run || (itask_run && last_time_run < 2000)) {
							evNT(EC_EVENT, EV_NEXTTASK, -1, "NextTask", evprintf("OKAY for LONG RUN %s, interrupt last ran @%.6f, %d us ago",
								task_s(t), (float) itask_last_tstart / 1000000, last_time_run));
							//if (ev_dump) evNT(EC_DUMP, EV_NEXTTASK, ev_dump, "NextTask", evprintf("DUMP IN %.3f SEC", ev_dump/1000.0));
							t->long_run = false;
							
							#ifdef LOCK_CHECK_HANG
                                if (lock_panic)
                                    t->runnable = true;
                                else
                                    break;
                            #else
                                break;
                            #endif
						}
						// not eligible to run at this time
						#if 0
						evNT(EC_EVENT, EV_NEXTTASK, -1, "not elig", evprintf("NOT OKAY for LONG RUN %s, interrupt last ran @%.6f, %d us ago",
							task_s(t), (float) itask_last_tstart / 1000000, last_time_run));
						if (ev_dump) evNT(EC_DUMP, EV_NEXTTASK, ev_dump, "not elig", evprintf("DUMP IN %.3f SEC", ev_dump/1000.0));
						#endif
					} else {
						if (!t->stopped) {
						    #ifdef LOCK_CHECK_HANG
                                if (lock_panic)
                                    t->runnable = true;
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
			}
		}

        #ifdef LOCK_CHECK_HANG
            if (lock_panic) {
                dump();
                evLock(EC_DUMP, EV_NEXTTASK, -1, "lock panic", "DUMP lock_panic");
                panic("lock_check");
            }
        #endif

		idle_count++;
    } while (p < LOWEST_PRIORITY);		// if no eligible tasks keep looking
    
	if (!need_hardware || update_in_progress || sd_copy_in_progress || (our_pid != kiwi_server_pid)) {
		usleep(100000);		// pause so we don't hog the machine
	}
	
	// remember where we were last running
    if (ct->valid && setjmp(ct->ctx->jb)) {
    	return;		// returns here when task next run
    }
    
    now_us = timer_us64();
    u4_t just_idle_us = now_us - enter_us;
    idle_us += just_idle_us;
	if (t->minrun) t->minrun_start_us = now_us;
	
    #ifdef EV_MEAS_NEXTTASK
        if (idle_count > 1)
            evNT(EC_EVENT, EV_NEXTTASK, -1, "NextTask", evprintf("IDLE for %d spins, %.3f ms",
                idle_count, (float) just_idle_us / 1000));
        if (pc) {
            evNT(EC_TASK_SWITCH, EV_NEXTTASK, -1, "NextTask", evprintf("from %s@0x%llx ...", task_ls(ct), pc));
            evNT(EC_TASK_SWITCH, EV_NEXTTASK, -1, "NextTask", evprintf("... to %s", task_ls(t)));
        } else {
            evNT(EC_TASK_SWITCH, EV_NEXTTASK, -1, "NextTask", evprintf("from %s ...", task_ls(ct)));
            evNT(EC_TASK_SWITCH, EV_NEXTTASK, -1, "NextTask", evprintf("... to %s", task_ls(t)));
        }
	#endif
	
	t->tstart_us = now_us;
	if (t->flags & CTF_POLL_INTR) itask_last_tstart = now_us;
	t->last_last_run_time = t->last_run_time;
	t->last_run_time = quanta;
	t->last_pc = pc;
	last_task_run = ct;
    cur_task = t;
    t->run++;
	
    longjmp(t->ctx->jb, 1);
    panic("longjmp() shouldn't return");
}

int _CreateTask(funcP_t funcP, const char *name, void *param, int priority, u4_t flags, int f_arg)
{
	int i;
    TASK *t;
    
    for (i=1; i < MAX_TASKS; i++) {
        t = Tasks + i;
        if (!t->valid && ctx[i].init) break;
    }
    if (i == MAX_TASKS) panic("create_task: no tasks available");
    
	if (i > max_task) max_task = i;
	
	task_init(t, i, funcP, param, name, priority, flags, f_arg);

	return t->id;
}

static void taskSleepSetup(TASK *t, const char *reason, int usec)
{
	// usec == 0 means sleep until someone does TaskWakeup() on us
	// usec > 0 is microseconds time in future (added to current time)
	
	if (usec > 0) {
    	t->deadline = timer_us64() + usec;
    	sprintf(t->reason, "(%.3f msec) ", (float) usec/1000.0);
		evNT(EC_EVENT, EV_NEXTTASK, -1, "TaskSleep", evprintf("sleeping usec %d %s Qrunnable %d", usec, task_ls(t), t->tq->runnable));
	} else {
		t->deadline = usec;
		strcpy(t->reason, "(evt) ");
		evNT(EC_EVENT, EV_NEXTTASK, -1, "TaskSleep", evprintf("sleeping event %s Qrunnable %d", task_ls(t), t->tq->runnable));
	}
	
    kiwi_strncat(t->reason, reason, N_REASON);

	bool prev_stopped = t->stopped;
    t->stopped = TRUE;
	run[t->id].r = 0;
    runnable(t->tq, prev_stopped? 0:-1);
    t->sleeping = TRUE;
	t->wakeup = FALSE;
}

void *_TaskSleep(const char *reason, int usec)
{
    TASK *t = cur_task;

    taskSleepSetup(t, reason, usec);

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

void TaskSleepID(int id, int usec)
{
    TASK *t = Tasks + id;
    
    if (t == cur_task) return (void) TaskSleepUsec(usec);

    if (!t->valid) return;
	//printf("sleepID %s %d usec %d\n", task_ls(t), usec);
	evNT(EC_EVENT, EV_NEXTTASK, -1, "TaskSleepID", evprintf("%s usec %d", task_ls(t), usec));
	assert(cur_task->id != id);
	
	// must not force a task to sleep while it is holding or waiting for a lock -- make it pending instead
	if (t->lock.hold || t->lock.wait) {
		lprintf("TaskSleepID: pending_sleep =========================================\n");
		t->pending_sleep = TRUE;
		t->pending_usec = usec;
	} else {
    	taskSleepSetup(t, "TaskSleepID", usec);
    }
}

void TaskWakeup(int id, bool check_waking, void *wake_param)
{
    TASK *t = Tasks + id;
    
    if (!t->valid) return;
#if 1
	// FIXME: remove at some point
	// This is a hack for the benefit of "-rx 0" measurements where we don't want the
	// TaskSleepMsec(1000) in the audio task to cause a task switch while sleeping
	// because it's being woken up all the time.
    if (t->deadline > 0) {
        evNT(EC_EVENT, EV_NEXTTASK, -1, "TaskWakeup", evprintf("%s still deadline of %08x|%08x", task_ls(t), PRINTF_U64_ARG(t->deadline)));
        return;		// don't interrupt a task sleeping on a time interval
    }
#else
    t->deadline = 0;	// cancel any outstanding deadline
#endif

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
    t->stopped = FALSE;
	run[t->id].r = 1;
    runnable(t->tq, 1);
    t->sleeping = FALSE;
	if (check_waking) assert(!t->wakeup);
	//printf("wa%d ", t->id); fflush(stdout);
    t->wakeup = TRUE;
    t->wake_param = wake_param;
    
    // if we're waking up a higher priority task, run it without delay
    if (t->priority > cur_task->priority) {
    	t->interrupted_task = cur_task;		// remember who we interrupted
    	evNT(EC_EVENT, EV_NEXTTASK, -1, "TaskWakeup", evprintf("HIGHER PRIORITY wake %s ...", task_ls(t)));
    	evNT(EC_EVENT, EV_NEXTTASK, -1, "TaskWakeup", evprintf("HIGHER PRIORITY ... from interrupted %s", task_ls(cur_task)));
    	sprintf(t->reason, "TaskWakeup: HIGHER PRIO %p", wake_param);
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
            free((void *) ct->name);
            ct->flags &= ~CTF_TNAME_FREE;
        }
		ct->name = name;
		if (free_name) ct->flags |= CTF_TNAME_FREE;
	}
	return ct->name;
}

void TaskParams(u4_t minrun_us)
{
    TASK *t = cur_task;

	t->minrun = minrun_us;
}

u4_t TaskFlags()
{
    TASK *t = cur_task;

	return t->flags;
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
    asprintf(&lock->enter_name, "lock enter: %s", name);    // never freed, but doesn't matter
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

#define	N_LOCK_LIST		256
static int n_lock_list;
lock_t *locks[N_LOCK_LIST];

void lock_register(lock_t *lock)
{
	assert(n_lock_list < N_LOCK_LIST);
	locks[n_lock_list] = lock;
	n_lock_list++;
}

void lock_dump()
{
	int i, j;
	lprintf("\n");
	lprintf("LOCKS:\n");
	
	u4_t now = timer_sec();
	for (i=0; i < n_lock_list; i++) {
		lock_t *l = locks[i];
		if (l->init) {
		    TASK *owner = (TASK *) l->owner;
		    int n_users = l->enter - l->leave;
			lprintf("L%d L%d|E%d (%d) prio_swap=%d prio_inversion=%d time_no_owner=%d \"%s\" \"%s\"",
				i, l->leave, l->enter, n_users, l->n_prio_swap, l->n_prio_inversion,
				l->timer_since_no_owner? (now - l->timer_since_no_owner) : 0, l->name, l->enter_name);
			if (n_users) {
                if (owner)
                    lprintf(" held by: %s", task_ls(owner));
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
}

#ifdef LOCK_CHECK_HANG

// Check for hung condition: there are waiters on a lock, but no task has owned the lock in a long time.
bool lock_check()
{
	int i;
	
	bool lock_panic = false;
	for (i=0; i < n_lock_list; i++) {
		lock_t *l = locks[i];
		if (l->timer_since_no_owner == 0) continue;
		u4_t time_since_no_owner = timer_sec() - l->timer_since_no_owner;
		if (time_since_no_owner <= LOCK_HUNG_TIME) continue;
		int n_waiters = l->enter - l->leave;
        lprintf("lock_check: HUNG LOCK? \"%s\" (%d waiters) has had no owner for > %d seconds\n",
            l->name, n_waiters, LOCK_HUNG_TIME);
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

            if (ct->priority > lp->priority) {
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
		if (ow && ct->priority > ow->priority) {
		    assert(ow->lock.hold == lock);
		    // priority inversion: temp raise priority of lock owner to our priority
		    ow->saved_priority = ow->priority;
		    //printf("### LOCK_PRIORITY_INVERSION raising owner %s ...\n", task_s(ow));
		    //printf("### LOCK_PRIORITY_INVERSION ... to match ct %s\n", task_s(ct));
		    TdeQ(ow);
		    TenQ(ow, ct->priority);
		    ow->flags |= CTF_PRIO_INVERSION;
		    lock->n_prio_inversion++;
		    
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
        
        // remove us from lock users list
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
        assert(lock->users == NULL);
        lock->enter = lock->leave = 0;      // reset so they don't potentially wrap
        lock->timer_since_no_owner = 0;
    } else {
        lock->timer_since_no_owner = timer_sec();

        #if defined(LOCK_CHECK_HANG) && defined(EV_MEAS_LOCK)
            if (lock == &spi_lock) {
                expecting_spi_lock_next_task = 50;
                evLock2(EC_EVENT, EV_NEXTTASK, -1, "lock_leave", evprintf("set expecting_spi_lock_next_task=50, %s %d waiters", task_s(ct), n_waiters));
            }
        #endif

        #ifdef LOCK_TEST_HANG
            static int lock_test_hang_ct;
            if (lock == &spi_lock) {
                lock_test_hang_ct++;
                if (lock_test_hang_ct <= 20) printf("lock_test_hang_ct %d\n", lock_test_hang_ct);
                if (lock_test_hang_ct == 20) {
                    printf("**** LOCK_TEST_HANG ****\n");
                    lock_test_hang = 1;
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

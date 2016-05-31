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

// Copyright (c) 2014-2016 John Seamons, ZL/KF6VO

#include "types.h"
#include "kiwi.h"
#include "config.h"
#include "misc.h"
#include "coroutines.h"
#include "debug.h"
#include "peri.h"
#include "data_pump.h"

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

	Both methods are included in the code. The jmp_buf method with de-mangling is the default.

*/

#define SETUP_TRAMP_USING_JMP_BUF

#if defined(HOST) && defined(USE_VALGRIND)
	#include <valgrind/valgrind.h>
#endif

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
	int i;
	int v;
	const char *s;
	int p;
	int r;
} run[MAX_TASKS];

// FIXME: too big. 8*K causes the WF/FFT thread to exceed 50% red zone. Need to optimize.
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
	TaskQ_t *tq;
	int id;
	ctx_t *ctx;
	TASK *interrupted_task;
	u4_t priority, flags;
	s64_t deadline;
	u4_t run, cmds;
	funcP_t funcP;
	const char *name, *where;
	char reason[64];
	void *param;
	bool valid, stopped, wakeup, sleeping, busy_wait, long_run;
	int wake_param;
	lock_t *lock_wait;
	u4_t usec, longest;
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

static TASK Tasks[MAX_TASKS], *cur_task, *last_task_run, *busy_helper_task, *interrupt_task;
static ctx_t ctx[MAX_TASKS]; 
static TaskQ_t TaskQ[NUM_PRIORITY];
static u64_t last_dump;
static int interrupt_tid;
static u4_t idle_us;

void runnable(TaskQ_t *tq, int chg)
{
	int runnable = tq->runnable;
	if(!(runnable >= 0 && runnable <= tq->count && runnable <= MAX_TASKS))
	assert(runnable >= 0 && runnable <= tq->count && runnable <= MAX_TASKS);

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
		assert(ckrun == head->runnable);
	}

	runnable = tq->runnable;
	if(!(runnable >= 0 && runnable <= tq->count && runnable <= MAX_TASKS))
	assert(runnable >= 0 && runnable <= tq->count && runnable <= MAX_TASKS);
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

	if (!t->stopped) runnable(tq, -1);
	tq->count--;
}

// print per-task accumulated usec runtime since last dump
void TaskDump()
{
	int i;
	TASK *t;
	u64_t now_us = timer_us64();
	u4_t elapsed = now_us - last_dump;
	last_dump = now_us;
	float f_elapsed = ((float) elapsed) / 1e6;
	float f_sum = 0;
	float f_idle = ((float) idle_us) / 1e6;
	idle_us = 0;
	//lprintf("Ttt sss x.xxx xx.xxx xxx.x%% xxxxxxx xxxxx xxxxx xxx xxxxx xxx xxxxx xxxxx xxxxx xxxx.xxx xxx%%\n");
	  lprintf("   task run S max mS      %%   #runs  cmds   st1       st2       #wu   nrs retry deadline stk%%  spi_retry %d, spi_delay %d\n",
	  	spi_retry, spi_delay);
	for (i=0; i<MAX_TASKS; i++) {
		t = Tasks + i;
		if (t->valid) {
			float f_usec = ((float) t->usec) / 1e6;
			f_sum += f_usec;
			float f_longest = ((float) t->longest) / 1e3;
			float deadline=0;
			if (t->deadline > 0) {
				deadline = (t->deadline > now_us)? (float) (t->deadline - now_us) : 9999.999;
			}
			lprintf("T%02d %d%c%c %5.3f %6.3f %5.1f%% %7d %5d %5d %-3s %5d %-3s %5d %5d %5d %8.3f %3d%% %-10s %-24s %-24s\n", i,
				t->priority, t->sleeping? 'S' : (t->wakeup? 'W' : (t->lock_wait? 'L':'R')),
				t->minrun? 'q':'_', f_usec, f_longest, f_usec/f_elapsed*100,
				t->run, t->cmds, t->stat1, t->units1? t->units1 : " ",
				t->stat2, t->units2? t->units2 : " ", t->wu_count, t->no_run_same, t->spi_retry,
				deadline / 1e3, t->stack_hiwat*100 / STACK_SIZE_U64_T,
				t->name, t->where? t->where : "-"
				,t->long_name? t->long_name : "-"
				);

			if ((t->no_run_same > 200) && ev_dump) {
				evNT(EC_DUMP, EV_NEXTTASK, ev_dump, "no_run_same", evprintf("DUMP IN %.3f SEC", ev_dump/1000.0));
			}

			t->usec = t->longest = 0;
			t->long_name = NULL;
			t->run = t->cmds = t->wu_count = t->no_run_same = t->spi_retry = 0;
			if (t->s1_func & TSTAT_ZERO) t->stat1 = 0;
			if (t->s2_func & TSTAT_ZERO) t->stat2 = 0;
		}
	}
	f_sum += f_idle;
	float f_pct = f_idle/f_elapsed*100;
	lprintf("idle    %5.3f        %5.1f%%\n", f_idle, f_pct);
	float f_remain = fabsf(f_elapsed - f_sum);
	f_pct = f_remain/f_elapsed*100;
	//if (f_remain > 0.01)
	lprintf("Linux   %5.3f        %5.1f%%\n", f_remain, f_pct);
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

static volatile u64_t tstart_us;

static void task_init(TASK *t, int id, funcP_t funcP, void *param, const char *name, u4_t priority, u4_t flags)
{
	int i;
	
	memset(t, 0, sizeof(TASK));
	t->id = id;
	t->ctx = ctx + id;
	t->funcP = funcP;
	t->param = param;
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
		assert(!interrupt_task);
		interrupt_task = t;
		interrupt_tid = id;
	}

	TenQ(t, priority);
	
	run[id].i = id;
	run[id].v = 1;
	run[id].s = name;
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
			c->valgrind_stack_id= VALGRIND_STACK_REGISTER(c->stack, c->stack_last);
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
		//if (i == (STACK_SIZE_U64_T-1)) printf("T%02d W 0x%016llx\n", c->id, *s);
	}
}

#ifdef SETUP_TRAMP_USING_JMP_BUF

static void trampoline()
{
    TASK *t = cur_task;
    
	(t->funcP)(t->param);
	printf("task %s:P%d:T%02d exited by returning\n", t->name, t->priority, t->id);
	TaskRemove(t->id);
}

#else

static ctx_t *new_ctx;
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
	(t->funcP)(t->param);
	printf("task %s:P%d:T%02d exited by returning\n", t->name, t->priority, t->id);
	TaskRemove(t->id);
}

#endif

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
}

// this can only be done while running on the main stack,
// i.e. not on task stacks created via sigaltstack()
static bool collect_needed;

void TaskCollect()
{
	int i;
	
	if (!collect_needed)
		return;

	for (i=1; i < MAX_TASKS; i++) {
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

void TaskInit()
{
	static bool init;
    TASK *t;
	
	//printf("MAX_TASKS %d\n", MAX_TASKS);

	t = Tasks;
	last_dump = tstart_us = timer_us64();
	cur_task = t;
	task_init(t, 0, NULL, NULL, "main", MAIN_PRIORITY, 0);
	//if (ev_dump) evNT(EC_DUMP, EV_NEXTTASK, ev_dump, "TaskInit", evprintf("DUMP IN %.3f SEC", ev_dump/1000.0));
	for (int p = LOWEST_PRIORITY; p <= HIGHEST_PRIORITY; p++) TaskQ[p].p = p;	// debugging aid
	
	find_key();
	printf("TASK: jmp_buf demangle key 0x%08x\n", key);
	//setjmp(ctx[0].jb);
	//printf("JMP_BUF: key 0x%x sp 0x%x:0x%x pc 0x%x:0x%x\n", key, ctx[0].sp, ctx[0].sp^key, ctx[0].pc, ctx[0].pc^key);
	
	collect_needed = TRUE;
	TaskCollect();
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
	for (i=1; i < MAX_TASKS; i++) {
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
			printf("DANGER: T%02d:%s stack used %d/%d (%d%%)\n", i, t->name, used, STACK_SIZE_U64_T, pct);
			panic("TaskCheckStacks");
		}
	}
}

static bool interrupt_holdoff;

void TaskInterruptHoldoff(bool holdoff)
{
	interrupt_holdoff = holdoff;
	if (!holdoff) TaskPollForInterrupt(CALLED_AFTER_HOLDOFF);
}

static const char *poll_from[] = { "NEXTTASK", "HOLDOFF", "LOCK", "SPI" };

void TaskPollForInterrupt(ipoll_from_e from)
{
	if (interrupt_holdoff) {
		return;
	}

	if (interrupt_task && GPIO_READ_BIT(GPIO0_15) && interrupt_task->sleeping) {
		evNT(EC_TRIG1, EV_NEXTTASK, -1, "PollIntr", evprintf("%s TO INTERRUPT TASK <===========================",
			poll_from[from]));

		// can't call TaskWakeup() from within NextTask()
		if (from == CALLED_WITHIN_NEXTTASK) {
			interrupt_task->wu_count++;
			interrupt_task->stopped = FALSE;
			run[interrupt_task->id].r = 1;
			runnable(interrupt_task->tq, 1);
			interrupt_task->sleeping = FALSE;
			interrupt_task->wakeup = TRUE;
		} else {
			TaskWakeup(interrupt_tid, false, 0);
		}
	}
}

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
		printf("task last run: %s:P%d:T%02d(%s)",
			t->name, t->priority, t->id, t->where? t->where : "-");
		if (t->last_pc)
			printf("@0x%llx", t->last_pc);
		printf(" %.3f %.3f ms\n", (float) t->last_last_run_time / 1000, (float) t->last_run_time / 1000);
	}
}

#ifdef DEBUG
 void _NextTask(const char *where, u4_t param, u_int64_t pc)
#else
 void _NextTask(u4_t param)
#endif
{
	int i;
    TASK *t, *tn, *ct;
    u64_t now_us, enter_us = timer_us64();
    u4_t quanta;

    quanta = enter_us - tstart_us;
	ct = cur_task;
    ct->usec += quanta;
    if (quanta > ct->longest) {
    	ct->longest = quanta;
    	ct->long_name = where;
    	
		// fixme: remove at some point
		if (quanta > 2000000 && ct->id != 0) {
			printf("LRUN %s:%02d %s %7.3f\n", ct->name, ct->id, where, (float) quanta / 1000);
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
				evNT(EC_EVENT, EV_NEXTTASK, -1, "NextTask", evprintf("%s:P%d:T%02d BUSY_WAIT second time, BUSY_HELPER WAKEUP",
					ct->name, ct->priority, ct->id, ct->where? ct->where : "-"));
			} else {
				no_run_same = true;
				evNT(EC_EVENT, EV_NEXTTASK, -1, "NextTask", evprintf("%s:P%d:T%02d BUSY_WAIT second time, no_run_same SET",
					ct->name, ct->priority, ct->id, ct->where? ct->where : "-"));
			}
			ct->no_run_same++;
    		ct->busy_wait = false;
    	} else {
    		ct->busy_wait = true;
			evNT(EC_EVENT, EV_NEXTTASK, -1, "NextTask", evprintf("%s:P%d:T%02d BUSY_WAIT first time",
    			ct->name, ct->priority, ct->id, ct->where? ct->where : "-"));
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

	ct->tq->last_run = ct->valid? &ct->tll : NULL;

    do {
		// update scheduling deadlines
		TASK *tp = Tasks;
		now_us = timer_us64();
		for (i=0; i<MAX_TASKS; i++, tp++) {
			if (!tp->valid) continue;
			bool wake = false;
			if (tp->deadline > 0) {
				if (tp->deadline < now_us) {
					wake = true;
					evNT(EC_EVENT, EV_NEXTTASK, -1, "NextTask", evprintf("deadline expired %s:P%d:T%02d, Qrunnable %d",
						tp->name, tp->priority, tp->id, tp->tq->runnable));
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
    
   		 // search task queues
		for (p = HIGHEST_PRIORITY; p >= LOWEST_PRIORITY; p--) {
			head = &TaskQ[p];
			int runnable = head->runnable;
			//evNT(EC_EVENT, EV_NEXTTASK, -1, "NextTask", evprintf("looking P%d, count %d, runnable %d",
			//	p, head->count, runnable));
			if (!(runnable >= 0 && runnable <= head->count && runnable <= MAX_TASKS))
				assert(runnable >= 0 && runnable <= head->count && runnable <= MAX_TASKS);

			if (p == ct->priority && runnable == 1 && no_run_same) {
				evNT(EC_EVENT, EV_NEXTTASK, -1, "NextTask", evprintf("%s:P%d:T%02d no_run_same TRIGGERED ***",
					ct->name, ct->priority, ct->id, ct->where? ct->where : "-"));
				no_run_same = false;
				continue;
			}
			
			if (runnable) {
				// at this point the p/head queue should have at least one runnable task
				TaskLL_t *wrap = (head->last_run && head->last_run->next)? head->last_run->next : head->tll.next;
				TaskLL_t *tll = wrap;
				
				do {
					assert(tll);
					t = tll->t;
					assert(t);
					assert(t->valid);
					
					if (!t->stopped && t->long_run) {
						u4_t last_time_run = now_us - interrupt_task_last_run;
						if (!interrupt_task || last_time_run < 2000) {
							evNT(EC_EVENT, EV_NEXTTASK, -1, "NextTask", evprintf("OKAY for LONG RUN %s:P%d:T%02d, interrupt last ran @%.6f, %d us ago",
								t->name, t->priority, t->id, (float) interrupt_task_last_run / 1000000, last_time_run));
							//if (ev_dump) evNT(EC_DUMP, EV_NEXTTASK, ev_dump, "NextTask", evprintf("DUMP IN %.3f SEC", ev_dump/1000.0));
							t->long_run = false;
							break;
						}
						// not eligible to run at this time
						#if 0
						evNT(EC_EVENT, EV_NEXTTASK, -1, "NextTask", evprintf("NOT OKAY for LONG RUN %s:P%d:T%02d, interrupt last ran @%.6f, %d us ago",
							t->name, t->priority, t->id, (float) interrupt_task_last_run / 1000000, last_time_run));
						if (ev_dump) evNT(EC_DUMP, EV_NEXTTASK, ev_dump, "NextTask", evprintf("DUMP IN %.3f SEC", ev_dump/1000.0));
						#endif
					} else {
						if (!t->stopped) break;
					}
					
					t = NULL;
					tll = tll->next;
					if (tll == NULL) tll = head->tll.next;
				} while (tll != wrap);
				
				if (t) {
					assert(t->valid);
					assert(!t->sleeping);
					break;
				}
			}
		}
		idle_count++;
    } while (p < LOWEST_PRIORITY);		// if no eligible tasks keep looking
    
	// remember where we were last running
    if (ct->valid && setjmp(ct->ctx->jb)) {
    	return;		// returns here when task next run
    }
    
    now_us = timer_us64();
    idle_us += now_us - enter_us;
	tstart_us = now_us;
	if (t->minrun) t->minrun_start_us = now_us;
	
    #ifdef EV_MEAS_NEXTTASK
	if (idle_count > 1)
		evNT(EC_EVENT, EV_NEXTTASK, -1, "NextTask", evprintf("IDLE for %d spins, %.3f ms",
			idle_count, (float) idle_us / 1000));
    if (pc)
		evNT(EC_EVENT, EV_NEXTTASK, -1, "NextTask", evprintf("from %s:P%d:T%02d(%s)@0x%llx to %s:P%d:T%02d(%s)",
			ct->name, ct->priority, ct->id, ct->where? ct->where : "-", pc,
			t->name, t->priority, t->id, t->where? t->where : "-"));
	else
		evNT(EC_EVENT, EV_NEXTTASK, -1, "NextTask", evprintf("from %s:P%d:T%02d(%s) to %s:P%d:T%02d(%s)",
			ct->name, ct->priority, ct->id, ct->where? ct->where : "-",
			t->name, t->priority, t->id, t->where? t->where : "-"));
	#endif
	
	t->last_last_run_time = t->last_run_time;
	t->last_run_time = quanta;
	t->last_pc = pc;
	last_task_run = ct;
    cur_task = t;
    t->run++;
	
    longjmp(t->ctx->jb, 1);
    panic("longjmp() shouldn't return");
}

static int create_task(funcP_t funcP, const char *name, int priority, void *param, u4_t flags)
{
	int i;
    TASK *t;
    
	for (i=1; i<MAX_TASKS; i++) {
		t = Tasks + i;
		if (!t->valid && ctx[i].init) break;
	}
	if (i == MAX_TASKS) panic("create_task: no tasks available");
	
	task_init(t, i, funcP, param, name, priority, flags);

	return t->id;
}

int _CreateTask(func_t entry, const char *name, int priority, u4_t flags)
{
	return create_task((funcP_t) entry, name, priority, 0, flags);
}

int _CreateTaskP(funcP_t entry, const char *name, int priority, void *param)
{
	return create_task(entry, name, priority, param, 0);
}

static void taskSleepSetup(TASK *t, int usec)
{
	if (usec > 0) {
    	t->deadline = timer_us64() + usec;
    	sprintf(t->reason, "TaskSleep %.3f msec", (float)usec/1000.0);
		evNT(EC_EVENT, EV_NEXTTASK, -1, "TaskSleep", evprintf("sleeping usec %d %s:P%d:T%02d(%s) Qrunnable %d",
			usec, t->name, t->priority, t->id, t->where? t->where : "-", t->tq->runnable));
	} else {
		t->deadline = usec;
    	sprintf(t->reason, "TaskSleep event");
		evNT(EC_EVENT, EV_NEXTTASK, -1, "TaskSleep", evprintf("sleeping event %s:P%d:T%02d(%s) Qrunnable %d",
			t->name, t->priority, t->id, t->where? t->where : "-", t->tq->runnable));
	}
	
	bool prev_stopped = t->stopped;
    t->stopped = TRUE;
	run[t->id].r = 0;
    runnable(t->tq, prev_stopped? 0:-1);
    t->sleeping = TRUE;
	t->wakeup = FALSE;
}

int TaskSleep(int usec)
{
    TASK *t = cur_task;

    taskSleepSetup(t, usec);

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
    
	evNT(EC_EVENT, EV_NEXTTASK, -1, "TaskSleep", evprintf("woke %s:P%d:T%02d(%s) Qrunnable %d",
		t->name, t->priority, t->id, t->where? t->where : "-", t->tq->runnable));

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
    
    if (t == cur_task) return (void) TaskSleep(usec);

	//printf("sleepID T%02d %d usec %d\n", t->id, usec);
	evNT(EC_EVENT, EV_NEXTTASK, -1, "TaskSleepID", evprintf("%s:P%d:T%02d(%s) usec %d",
		t->name, t->priority, t->id, t->where? t->where : "-", usec));
	assert(cur_task->id != id);
    taskSleepSetup(t, usec);
}

void TaskWakeup(int id, bool check_waking, int wake_param)
{
    TASK *t = Tasks + id;
    
    if (!t->valid) return;
#if 1
	// fixme: remove at some point
	// This is a hack for the benefit of "-rx 0" measurements where we don't want the
	// TaskSleep(1000000) in the audio task to cause a task switch while sleeping
	// because it's being woken up all the time.
    if (t->deadline > 0) return;		// don't interrupt a task sleeping on a time interval
#else
    t->deadline = 0;	// cancel any outstanding deadline
#endif

	if (!t->sleeping) {
		assert(!t->stopped || (t->stopped && t->lock_wait));
		return;	// not already sleeping
	}

	evNT(EC_EVENT, EV_NEXTTASK, -1, "TaskWakeup", evprintf("%s:P%d:T%02d(%s)",
		t->name, t->priority, t->id, t->where? t->where : "-"));
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
    	evNT(EC_EVENT, EV_NEXTTASK, -1, "TaskWakeup", evprintf("HIGHER PRIORITY wake %s:P%d:T%02d(%s) from interrupted %s:P%d:T%02d(%s)",
			t->name, t->priority, t->id, t->where? t->where : "-",
			cur_task->name, cur_task->priority, cur_task->id, cur_task->where? cur_task->where : "-"));
    	sprintf(t->reason, "TaskWakeup: param %d", wake_param);
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

const char *TaskName()
{
	if (!cur_task) return "main";
	return cur_task->name;
}

void TaskParams(u4_t minrun_us)
{
    TASK *t = cur_task;

	t->minrun = minrun_us;
}

// Critical section "first come, first served"

void _lock_init(lock_t *lock, const char *name)
{
	memset(lock, 0, sizeof(*lock));
	lock->name = name;
	strcpy(lock->enter_name, "lock enter: ");
	strcat(lock->enter_name, name);
	lock->init = true;
	lock->magic_b = LOCK_MAGIC_B;
	lock->magic_e = LOCK_MAGIC_E;
}

#define check_lock() \
	if (lock->magic_b != LOCK_MAGIC_B || lock->magic_e != LOCK_MAGIC_E) \
		lprintf("*** BAD LOCK MAGIC 0x%x 0x%x\n", lock->magic_b, lock->magic_e);


void lock_enter(lock_t *lock)
{
	TASK *t = cur_task;
	if (!lock->init) return;
	check_lock();
    int token = lock->enter++;
    //bool dbg = (strcmp(lock->name, "&waterfall_hw_lock") == 0);
    bool dbg = false;
    bool waiting = false;

    while (token > lock->leave) {
		if (dbg && !waiting) {
			printf("LOCK t%d %s WAIT %s\n", TaskID(), TaskName(), lock->name);
			waiting = true;
		}
		if (lock != &spi_lock)
    	evNT(EC_EVENT, EV_NEXTTASK, -1, "lock_enter", evprintf("WAIT lock %s %s:P%d:T%02d(%s)",
			lock->name, t->name, t->priority, t->id, t->where? t->where : "-"));
		cur_task->lock_wait = lock;
		cur_task->stopped = TRUE;
		run[cur_task->id].r = 0;
		runnable(cur_task->tq, -1);
    	NextTask(lock->enter_name);
		check_lock();
    }
    
    lock->tid = TaskID();
    lock->tname = TaskName();
	if (dbg) printf("LOCK t%d %s ACQUIRE %s\n", TaskID(), TaskName(), lock->name);
	if (lock != &spi_lock)
	evNT(EC_EVENT, EV_NEXTTASK, -1, "lock_enter", evprintf("ACQUIRE lock %s %s:P%d:T%02d(%s)",
		lock->name, t->name, t->priority, t->id, t->where? t->where : "-"));
}

void lock_leave(lock_t *lock)
{
	TASK *t = cur_task;
	if (!lock->init) return;
	check_lock();
    //bool dbg = (strcmp(lock->name, "&waterfall_hw_lock") == 0);
    bool dbg = false;
    lock->leave++;
    lock->tid = 0;
    lock->tname = 0;
	if (dbg) printf("LOCK t%d %s RELEASE %s\n", TaskID(), TaskName(), lock->name);
	if (lock != &spi_lock)
	evNT(EC_EVENT, EV_NEXTTASK, -1, "lock_leave", evprintf("RELEASE lock %s %s:P%d:T%02d(%s)",
		lock->name, t->name, t->priority, t->id, t->where? t->where : "-"));

	bool wake_higher_priority = false;
    TASK *tp = Tasks;
    for (int i=0; i<MAX_TASKS; i++) {
    	if (tp->lock_wait == lock) {
    		tp->lock_wait = NULL;
    		tp->stopped = FALSE;
			run[tp->id].r = 1;
    		runnable(tp->tq, 1);
    		if (tp->priority > cur_task->priority) wake_higher_priority = true;
			if (lock != &spi_lock)
			evNT(EC_EVENT, EV_NEXTTASK, -1, "lock_leave", evprintf("WAKEUP lock %s %s:P%d:T%02d(%s)",
				lock->name, tp->name, t->priority, tp->id, tp->where? tp->where : "-"));
    	}
    	tp++;
    }
    
	//TaskPollForInterrupt(CALLED_FROM_LOCK);

	// if we're waking up a high-priority task, run it without delay
	// similar to strategy in TaskWakeup()
    if (wake_higher_priority) NextTask("lock_leave HIGHER PRIORITY");
}

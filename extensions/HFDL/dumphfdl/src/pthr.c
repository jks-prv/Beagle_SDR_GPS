#include "types.h"
#include "coroutines.h"
#include "pthr.h"
#define _GNU_SOURCE
#include <stdio.h>
#include <stdarg.h>

#define OPT_PTHR
#include "globals.h"

#define M_PTR ((int) mutex & 0xffff)
#define AM_PTR ((int) cond->assoc_mutex & 0xffff)
#define C_PTR ((int) cond & 0xffff)

int pthr_mutex_init(const char *id, pthr_mutex_t *mutex, const pthr_mutexattr_t *mutexattr)
{
    (void) mutexattr;
    mutex->id = id;
    mutex->lock = malloc(sizeof(lock_t));
    lock_initS((lock_t *) mutex->lock, id);
    lock_register((lock_t *) mutex->lock);
    pthr_printf("M-%04x INIT %s %s\n", M_PTR, mutex->id, Task_s(-1));
    return 0;
}

int pthr_mutex_lock(pthr_mutex_t *mutex)
{
    pthr_printf("M-%04x LOCK %s %s\n", M_PTR, mutex->id, Task_s(-1));
    lock_enter((lock_t *) mutex->lock);
    pthr_printf("M-%04x LOCK GOT %s %s\n", M_PTR, mutex->id, Task_s(-1));
    return 0;
}

int pthr_mutex_unlock(pthr_mutex_t *mutex)
{
    pthr_printf("M-%04x UNLOCK %s %s\n", M_PTR, mutex->id, Task_s(-1));
    lock_leave((lock_t *) mutex->lock);
    return 0;
}

int pthr_mutex_destroy(pthr_mutex_t *mutex)
{
    lock_unregister((lock_t *) mutex->lock);
    ((lock_t *) mutex->lock)->init = false;
    return 0;
}



int pthr_cond_init(const char *id, pthr_cond_t *restrict cond, pthr_mutex_t *restrict mutex, const pthr_condattr_t *restrict attr)
{
    (void) cond;
    (void) attr;
    cond->id = id;
    cond->assoc_mutex = mutex;
    pthr_printf("C-%04x AM-%04x INIT %s %s\n", C_PTR, AM_PTR, cond->id, Task_s(-1));
    return 0;
}

int pthr_cond_wait(pthr_cond_t *restrict cond, pthr_mutex_t *restrict mutex)
{
    char *s;
    asprintf(&s, "pthr_cond_wait %s", cond->id);
    pthr_printf("C-%04x AM-%04x WAIT %s %s\n", C_PTR, AM_PTR, cond->id, Task_s(-1));
    
    // next two lines are atomic since we have no preemption
    pthr_mutex_unlock(mutex);
    while (!cond->signalled) {
        TaskSleepReasonMsec(s, 20);
    }

    free(s);
    pthr_printf("C-%04x AM-%04x WAIT GOT-1 %s %s\n", C_PTR, AM_PTR, cond->id, Task_s(-1));
    pthr_mutex_lock(mutex);
    pthr_printf("C-%04x AM-%04x WAIT GOT-2 (re-locked) %s %s\n", C_PTR, AM_PTR, cond->id, Task_s(-1));
    cond->signalled = false;
    return 0;
}

int pthr_cond_signal(pthr_cond_t *cond)
{
    pthr_printf("C-%04x AM-%04x SIGNAL %s %s\n", C_PTR, AM_PTR, cond->id, Task_s(-1));
    cond->signalled = true;
    return 0;
}

int pthr_cond_broadcast(pthr_cond_t *cond)
{
    pthr_printf("C-%04x AM-%04x BCAST %s %s\n", C_PTR, AM_PTR, cond->id, Task_s(-1));
    cond->signalled = true;
    return 0;
}

int pthr_cond_destroy(pthr_cond_t *cond)
{
    (void) cond;
    return 0;
}


typedef struct {
    int rx_chan;
    const char *id;
    funcP_t func;
    void *arg;
} args_t;
    
static void pthr_bootstrap(void *_args)
{
    args_t *args = (args_t *) FROM_VOID_PARAM(_args);
    pthr_printf("pthr_bootstrap: %s rx_chan=%d (%p,%p)\n",
        args->id, args->rx_chan, args->func, args->arg);
	TaskSetUserParam(TO_VOID_PARAM(&hfdl_d[args->rx_chan]));
    args->func(args->arg);
}

int pthr_create(const char *id,
                          pthr_t *restrict thread,
                          const pthr_attr_t *restrict attr,
                          void *(*start_routine)(void *),
                          void *restrict arg)
{
    (void) thread;
    (void) attr;
    pthr_printf("pthr_create: %s (%p,%p)\n", id, start_routine, arg);
    
    args_t *args = (args_t *) malloc(sizeof(args_t));
    args->rx_chan = hfdl->rx_chan;
    args->id = id;
    args->func = (funcP_t) start_routine;
    args->arg = arg;
    CreateTaskSF((funcP_t) pthr_bootstrap, id, args, EXT_PRIORITY, 0, 0);
    return 0;
}

int pthr_detach(pthr_t thread)
{
    (void) thread;
    return 0;
}

int pthr_join(pthr_t thread, void **retval)
{
    (void) thread;
    (void) retval;
    return 0;
}


#ifdef KIWI
gpointer hfdl_g_async_queue_pop(const char *id, GAsyncQueue *q)
{
    gpointer qe;
    char *s;
    asprintf(&s, "async_queue_pop %s", id);

    do {
        qe = g_async_queue_try_pop(q);
        if (qe == NULL) {
            //printf("o"); fflush(stdout);
            TaskSleepReasonMsec(s, 20);
        }
    } while (qe == NULL);

    free(s);
    return qe;
}
#endif


// We used to simply "LIBS += -lpthread" in HFDL/Makefile to resolve these
// pthread routines for libglib. But that doesn't work anymore with macOS versions
// beyond 10.15 for some reason. So just define null routines here.
// This is backward compatible with maxOS 10.15 (Catalina).
// The routines in glib called by HFDL obviously never use any pthread routines.

#if defined(KIWI) && defined(XC)
    int pthread_sigmask() { return 0; }
    int pthread_mutexattr_init() { return 0; }
    int pthread_mutexattr_destroy() { return 0; }
    int pthread_mutexattr_settype() { return 0; }
    int pthread_rwlock_init() { return 0; }
    int pthread_mutex_trylock() { return 0; }
    int pthread_rwlock_destroy() { return 0; }
    int pthread_rwlock_trywrlock() { return 0; }
    int pthread_rwlock_tryrdlock() { return 0; }
    int pthread_key_create() { return 0; }
    int pthread_key_delete() { return 0; }
    int pthread_setspecific() { return 0; }
    void *pthread_getspecific (pthread_key_t __key) { return NULL; }
    int pthread_detach() { return 0; }
    int pthread_create() { return 0; }
    int pthread_attr_setstacksize() { return 0; }
    int pthread_rwlock_wrlock() { return 0; }
    int pthread_rwlock_unlock() { return 0; }
    int pthread_rwlock_rdlock() { return 0; }
    int pthread_join (pthread_t __th, void **__thread_return) { return 0; }
#endif

#pragma once

#ifndef KIWI
    #define KIWI
#endif

#ifdef KIWI
    #include <glib.h>
    gpointer hfdl_g_async_queue_pop(const char *id, GAsyncQueue *q);
#else
    #define hfdl_g_async_queue_pop(id, q) \
        g_async_queue_pop(q)
#endif

//#define PTHR_DEBUG
#ifdef PTHR_DEBUG
    #define pthr_printf(fmt, ...) \
        printf(fmt, ## __VA_ARGS__)
#else
    #define pthr_printf(fmt, ...)
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
} pthr_t;


typedef struct {
    const char *id;
    void *lock;
} pthr_mutex_t;

typedef struct {
} pthr_mutexattr_t;

int pthr_mutex_init(const char *id, pthr_mutex_t *mutex, const pthr_mutexattr_t *mutexattr);
int pthr_mutex_lock(pthr_mutex_t *mutex);
int pthr_mutex_unlock(pthr_mutex_t *mutex);
int pthr_mutex_destroy(pthr_mutex_t *mutex);


typedef struct {
    const char *id;
    pthr_mutex_t *assoc_mutex;
    volatile char signalled;        // volatile needed by clang
} pthr_cond_t;

typedef struct {
} pthr_condattr_t;

int pthr_cond_init(const char *id, pthr_cond_t *restrict cond, pthr_mutex_t *restrict mutex, const pthr_condattr_t *restrict attr);
int pthr_cond_wait(pthr_cond_t *restrict cond, pthr_mutex_t *restrict mutex);
int pthr_cond_signal(pthr_cond_t *cond);
int pthr_cond_broadcast(pthr_cond_t *cond);
int pthr_cond_destroy(pthr_cond_t *cond);


typedef struct {
} pthr_attr_t;

int pthr_create(const char *id, pthr_t *restrict thread,
                          const pthr_attr_t *restrict attr,
                          void *(*start_routine)(void *),
                          void *restrict arg);
int pthr_detach(pthr_t thread);
int pthr_join(pthr_t thread, void **retval);

#ifdef  __cplusplus
}
#endif

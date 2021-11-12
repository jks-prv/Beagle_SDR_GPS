/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <complex.h>
#include "pthr.h"
#include <liquid/liquid.h>
#include "hfdl_config.h"
#ifndef HAVE_PTHREAD_BARRIERS
#include "pthread_barrier.h"
#endif

enum producer_type {
	PRODUCER_NONE = 0,
	PRODUCER_SINGLE,
	PRODUCER_MULTI,
	PRODUCER_MAX
};

enum consumer_type {
	CONSUMER_NONE = 0,
	CONSUMER_SINGLE,
	CONSUMER_MULTI,
	CONSUMER_MAX
};

struct circ_buffer {
	cbuffercf buf;
	pthr_cond_t *cond;
	pthr_mutex_t *mutex;
};

struct shared_buffer {
	float complex *buf;
	pthr_barrier_t *data_ready;
	pthr_barrier_t *consumers_ready;
};

struct block_connection {
	union {
		struct circ_buffer circ_buffer;
		struct shared_buffer shared_buffer;
	};
	uint32_t flags;
};

// Block connection flags
#define BLOCK_CONNECTION_SHUTDOWN      (1 << 0)

struct producer {
	struct block_connection *out;
	size_t max_tu;                      // maximum transmission unit (samples)
	enum producer_type type;
};

struct consumer {
	struct block_connection *in;
	size_t min_ru;                      // minimum receive unit (samples)
	enum consumer_type type;
};

struct block {
	struct consumer consumer;
	struct producer producer;
	pthr_t thread;
	void *(*thread_routine)(void *);
	bool running;
};

// block.c
int32_t block_connect_one2one(const char *id, struct block *source, struct block *sink);
int32_t block_connect_one2many(struct block *source, size_t sink_count, struct block *sinks[sink_count]);
void block_disconnect_one2one(struct block *source, struct block *sink);
void block_disconnect_one2many(struct block *source, size_t sink_count, struct block *sinks[sink_count]);
int32_t block_start(const char *id, struct block *block);
int32_t block_set_start(const char *id, size_t block_cnt, struct block *block[block_cnt]);
void block_connection_one2one_shutdown(struct block_connection *connection);
void block_connection_one2many_shutdown(struct block_connection *connection);
bool block_connection_is_shutdown_signaled(struct block_connection *connection);
bool block_is_running(struct block *block);
bool block_set_is_any_running(size_t block_cnt, struct block *blocks[block_cnt]);

/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "pthr.h"                    // pthr_t
#include <glib.h>                       // GAsyncQueue
#include <libacars/libacars.h>          // la_proto_node
#include <libacars/list.h>              // la_list
#include "kvargs.h"                     // kvargs
#include "options.h"                    // options_descr_t
#include "metadata.h"                   // struct metadata

// default output specification - decoded text output to stdout
#define DEFAULT_OUTPUT "decoded:text:file:path=-"

// output queue high water mark
#define OUTPUT_QUEUE_HWM_DEFAULT 1000
// high water mark disabled
#define OUTPUT_QUEUE_HWM_NONE 0

// Data type on formatter input
typedef enum {
	FMTR_INTYPE_UNKNOWN           = 0,
	FMTR_INTYPE_DECODED_FRAME     = 1,
	FMTR_INTYPE_RAW_FRAME         = 2
} fmtr_input_type_t;

// Output formats
typedef enum {
	OFMT_UNKNOWN    = 0,
	OFMT_TEXT       = 1,
	OFMT_BASESTATION = 2
} output_format_t;

typedef struct octet_string* (fmt_decoded_fun_t)(struct metadata *, la_proto_node *);
typedef struct octet_string* (fmt_raw_fun_t)(struct metadata *, struct octet_string *);
typedef bool (intype_check_fun_t)(fmtr_input_type_t);

// Frame formatter descriptor
typedef struct {
	char *name;
	char *description;
    fmt_decoded_fun_t *format_decoded_msg;
    fmt_raw_fun_t *format_raw_msg;
    intype_check_fun_t *supports_data_type;
    output_format_t output_format;
} fmtr_descriptor_t;

// Frame formatter instance
typedef struct {
	fmtr_descriptor_t *td;           // type descriptor of the formatter used
	fmtr_input_type_t intype;        // what kind of data to pass to the input of this formatter
	la_list *outputs;                // list of output descriptors where the formatted message should be sent
} fmtr_instance_t;

typedef bool (output_format_check_fun_t)(output_format_t);
typedef void* (output_configure_fun_t)(kvargs *);
typedef void (output_ctx_destroy_fun_t)(void *);
typedef int32_t (output_init_fun_t)(void *);
typedef int32_t (output_produce_msg_fun_t)(void *, output_format_t, struct metadata *, struct octet_string *);
typedef void (output_shutdown_handler_fun_t)(void *);
typedef void (output_failure_handler_fun_t)(void *);

// Output descriptor
typedef struct {
	char *name;
	char *description;
	option_descr_t const *options;
	output_format_check_fun_t *supports_format;
	output_configure_fun_t *configure;
	output_ctx_destroy_fun_t *ctx_destroy;
	output_init_fun_t *init;
	output_produce_msg_fun_t *produce;
	output_shutdown_handler_fun_t *handle_shutdown;
	output_failure_handler_fun_t *handle_failure;
} output_descriptor_t;

// Output instance context (passed to the thread routine)
typedef struct {
	GAsyncQueue *q;                         // input queue
	void *priv;                             // output instance context (private)
	output_format_t format;                 // format of the data fed into the output
	bool active;                            // output thread is running
} output_ctx_t;

// Output instance
typedef struct {
	output_descriptor_t *td;                // type descriptor of the output
	pthr_t *output_thread;               // thread of this output instance
	output_ctx_t *ctx;                      // context data for the thread
} output_instance_t;

// Messages passed via output queues
typedef struct {
	struct octet_string *msg;               // formatted message
	struct metadata *metadata;              // opaque message metadata
	output_format_t format;                 // format of the data stored in msg
	uint32_t flags;                         // flags
} output_qentry_t;

// output queue entry flags
#define OUT_FLAG_ORDERED_SHUTDOWN (1 << 0)

fmtr_input_type_t fmtr_input_type_from_string(char const *str);
fmtr_descriptor_t *fmtr_descriptor_get(output_format_t fmt);
fmtr_instance_t *fmtr_instance_new(fmtr_descriptor_t *fmttd, fmtr_input_type_t intype);
void fmtr_instance_destroy(fmtr_instance_t *fmtr);

output_format_t output_format_from_string(char const *str);
output_descriptor_t *output_descriptor_get(char const *output_name);
output_instance_t *output_instance_new(output_descriptor_t *outtd, output_format_t format, void *priv);
void output_instance_destroy(output_instance_t *output);
output_qentry_t *output_qentry_copy(output_qentry_t const *q);
void output_qentry_destroy(output_qentry_t *q);
void output_queue_drain(GAsyncQueue *q);
void *output_thread(void *arg);
void output_queue_push(void *data, void *ctx);
void shutdown_outputs(la_list *fmtr_list);
bool output_thread_is_any_running(la_list *fmtr_list);

void output_usage();

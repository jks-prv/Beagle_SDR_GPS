/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

#include <stddef.h>             // size_t
#include <complex.h>            // float complex
#include "block.h"              // struct circ_buffer
#include "input-common.h"       // sample_format, convert_sample_buffer_fun

size_t get_sample_size(sample_format format);
float get_sample_full_scale_value(sample_format format);
convert_sample_buffer_fun get_sample_converter(sample_format format);
sample_format sample_format_from_string(char const *str);
void complex_samples_produce(struct circ_buffer *circ_buffer,
		float complex *samples, size_t num_samples);

/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include <stdint.h>
 
// help text pretty-printing constants and macros
#define USAGE_INDENT_STEP 4
#define USAGE_OPT_NAME_COLWIDTH 48
#define IND(n) (n * USAGE_INDENT_STEP)
 
// option name and description to be printed in the help text
typedef struct {
	char *name;
	char *description;
} option_descr_t;

void describe_option(char const *name, char const *description, int32_t indent);

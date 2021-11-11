/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <stdint.h>
#include <stdio.h>          // fprintf()
#include <string.h>         // strlen
#include "options.h"        // USAGE_OPT_NAME_COLWIDTH, USAGE_INDENT_STEP

void describe_option(char const *name, char const *description, int32_t indent) {
	int32_t descr_shiftwidth = USAGE_OPT_NAME_COLWIDTH - (int32_t)strlen(name) - indent * USAGE_INDENT_STEP;
	if(descr_shiftwidth < 1) {
		descr_shiftwidth = 1;
	}
	fprintf(stderr, "%*s%s%*s%s\n", IND(indent), "", name, descr_shiftwidth, "", description);
}


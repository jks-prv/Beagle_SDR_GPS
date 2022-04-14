#include "../types.h"
#include "kiwi_assert.h"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

void _sys_panic(const char *str, const char *file, int line)
{
	char *buf;
	
	// errno might be overwritten if the malloc inside asprintf fails
	asprintf(&buf, "SYS_PANIC: \"%s\" (%s, line %d)", str, file, line);
	perror(buf);
	exit(-1);
}

#define IDENT "ident-ident-ident-ident"
#define NOTES "notes-notes-notes-notes"

int main()
{
    FILE *fo;
    fo = fopen("dx_huge.json", "w");
    if (!fo) sys_panic("fo");
    fprintf(fo, "{\"dx\":[\n");
    bool first = true;
    
    int i;
    float f;
    int start_f = 5, end_i = 9999, step_f = 3;
    //int start_f = 10000, end_i = 255, step_f = 1;

    for (i = 0, f = start_f; i < end_i; i++, f += step_f) {
        bool last = (i == (end_i - 1));
        if (i & 1)
            fprintf(fo, "[%.2f,\"AM\",\"%.2f %s\",\"%s\",{\"WL\":1, \"b0\":2300, \"e0\":0200}]%s\n",
                f, f, IDENT, NOTES, !last? ",":"");
        else
            fprintf(fo, "[%.2f,\"AM\",\"%.2f %s\",\"%s\",{\"WL\":1}]%s\n",
                f, f, IDENT, NOTES, !last? ",":"");
        first = false;
    }
    
    fprintf(fo, "]}\n");

    fclose(fo);
	return 0;
}

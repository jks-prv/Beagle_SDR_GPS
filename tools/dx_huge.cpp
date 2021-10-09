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
    
    for (float f = 5; f < 30000; f += 3) {
        fprintf(fo, "%s[%.2f,\"AM\",\"%.2f %s\",\"%s\",{\"WL\":1}]\n",
            first? "":",", f, f, IDENT, NOTES);
        first = false;
    }
    
    fprintf(fo, "]}\n");
    

    fclose(fo);
	return 0;
}

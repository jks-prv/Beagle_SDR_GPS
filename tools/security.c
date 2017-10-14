#include "../types.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>

int main()
{
    int n;
    char *s, *s1, *s2, *x, *y;
    
    x = y = NULL;
    n = sscanf("x= y=abc", "x=%16ms y=%16ms", &x, &y);
    int xlen = (x == NULL)? -1 : strlen(x);
    int ylen = (y == NULL)? -1 : strlen(y);
    printf("scanf doesn't handle empty fields: n=%d x=%d<%s> y=%d<%s>\n", n, xlen, x, ylen, y);
    exit(0);
    
    // s is NULL terminated even when max field width
    s = NULL;
    n = sscanf("12345678", "%4ms", &s);
    printf("n=%d strlen=%lu s=<%s> s[4]=%d\n", n, strlen(s), s, s[4]);
    
    // s2 is really untouched (i.e. = 0x1139) if no conversion
    s1 = (char *) 0x1138;
    s2 = (char *) 0x1139;
    n = sscanf("12345 abcde", "%4ms XYZ %4ms", &s1, &s2);
    printf("n=%d s1=%p s2=%p\n", n, s1, s2);
    
    char dst[5+1];
    strcpy(dst, "abcde");
    sprintf(dst, "%.*s", 3, "12345");
    printf("sl=%lu <%s> dst[3]=%d\n", strlen(dst), dst, dst[3]);
    
    free(NULL);     // NULL arg is nop

	return 0;
}

#include <stdio.h>
#include <math.h>

int clog2(int value)
{
    int rv=0;
    value = value-1;
    for (rv=0; value > 0; rv = rv+1)
        value = value >> 1;
    return rv;
}

int main()
{
    int i;
    
    for (i=0; i<=33; i++)
        printf("%2d: %d %.0f\n", i, clog2(i), ceil(log2(i)));
    
    
	return 0;
}

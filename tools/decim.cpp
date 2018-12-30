#include <stdio.h>
#include <math.h>

int main()
{
	double clk = 66666600, out = 8250;
	int start = 1024;
	int i, least_i;
	double ratio = clk/out, re, diff, least_diff = 1e9;
	
	for (i = start; i > 1; i--) {
		re = ratio / i;
		diff = re - floor(re);
		if (diff < least_diff) {
			least_diff = diff;
			least_i = i;
		}
	}

	for (i = start; i > 1; i--) {
		re = ratio / i;
		diff = re - floor(re);
		if (diff < 0.01)
		printf("%4d %9.4f %.4f %s\n", i, re, diff, (i == least_i)? "****************":"");
	}
	
	return(0);
}

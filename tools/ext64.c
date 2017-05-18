#include "../types.h"
#include "../support/printf.h"

// Demonstrates that (u2_t << 16) loses its unsigned status unless re-cast.
// This results in sign extension into the upper 32-bits of a u64_t and
// silent discards of any other bits set there only if the sign-extension actually occurs!

int main()
{
	u2_t u2_1 = 0x8000;
	u64_t u64_1 = (u2_1 << 16) | 0x12300000000ULL;
	u64_t u64_2 = ((u4_t) (u2_1 << 16)) | 0x12300000000ULL;
	u64_t u64_3 = (0x8000 << 16) | 0x12300000000ULL;
	u64_t u64_4 = ((u4_t) (0x8000 << 16)) | 0x12300000000ULL;
	
	// note the 0x12300000000ULL bits are set given that no sign-extension occurs
	u2_1 = 0x4000;
	u64_t u64_5 = (u2_1 << 16) | 0x12300000000ULL;
	u64_t u64_6 = ((u4_t) (u2_1 << 16)) | 0x12300000000ULL;
	
	printf("u64_1 %08x|%08x\nu64_2 %08x|%08x\nu64_3 %08x|%08x\nu64_4 %08x|%08x\nu64_5 %08x|%08x\nu64_6 %08x|%08x\n",
	    PRINTF_U64_ARG(u64_1),
	    PRINTF_U64_ARG(u64_2),
	    PRINTF_U64_ARG(u64_3),
	    PRINTF_U64_ARG(u64_4),
	    PRINTF_U64_ARG(u64_5),
	    PRINTF_U64_ARG(u64_6)
	);
	
	return 0;
}

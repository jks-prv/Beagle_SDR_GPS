#pragma once

#define panic(s) _panic(s, FALSE, __FILE__, __LINE__);
#define dump_panic(s) _panic(s, TRUE, __FILE__, __LINE__);
#define sys_panic(s) _sys_panic(s, __FILE__, __LINE__);

#define check(e) \
	if (!(e)) { \
		lprintf("check failed: \"%s\" %s line %d\n", #e, __FILE__, __LINE__); \
		panic("check"); \
	}

#ifdef DEBUG
	#define D_PRF(x) printf x;
	#define D_STMT(x) x;
	
	#ifndef assert
		#define assert(e) \
			if (!(e)) { \
				lprintf("assertion failed: \"%s\" %s line %d\n", #e, __FILE__, __LINE__); \
				panic("assert"); \
			}
	#endif
	
#define assert_eq(v1, v2) \
    if (!((v1) == (v2))) { \
        printf("assertion failed: \"%s\"(%d) == \"%s\"(%d) %s line %d\n", \
            #v1, v1, #v2, v2, __FILE__, __LINE__); \
        panic("assert"); \
    }

#define assert_ne(v1, v2) \
    if (!((v1) != (v2))) { \
        printf("assertion failed: \"%s\"(%d) != \"%s\"(%d) %s line %d\n", \
            #v1, v1, #v2, v2, __FILE__, __LINE__); \
        panic("assert"); \
    }

#define assert_gt(v1, v2) \
    if (!((v1) > (v2))) { \
        printf("assertion failed: \"%s\"(%d) > \"%s\"(%d) %s line %d\n", \
            #v1, v1, #v2, v2, __FILE__, __LINE__); \
        panic("assert"); \
    }

#define assert_ge(v1, v2) \
    if (!((v1) >= (v2))) { \
        printf("assertion failed: \"%s\"(%d) >= \"%s\"(%d) %s line %d\n", \
            #v1, v1, #v2, v2, __FILE__, __LINE__); \
        panic("assert"); \
    }

#define assert_lt(v1, v2) \
    if (!((v1) < (v2))) { \
        printf("assertion failed: \"%s\"(%d) < \"%s\"(%d) %s line %d\n", \
            #v1, v1, #v2, v2, __FILE__, __LINE__); \
        panic("assert"); \
    }

#define assert_le(v1, v2) \
    if (!((v1) <= (v2))) { \
        printf("assertion failed: \"%s\"(%d) <= \"%s\"(%d) %s line %d\n", \
            #v1, v1, #v2, v2, __FILE__, __LINE__); \
        panic("assert"); \
    }

// e.g. array bounds checking
#define assert_ge_lt(v1, v2, v3) \
    if (!((v1) >= (v2) && (v1) < (v3))) { \
        printf("assertion failed: \"%s\"(%d) >= \"%s\"(%d) AND < \"%s\"(%d) %s line %d\n", \
            #v1, v1, #v2, v2, #v3, v3, __FILE__, __LINE__); \
        panic("assert"); \
    }

	#define assert_dump(e) \
		if (!(e)) { \
			lprintf("assertion failed: \"%s\" %s line %d\n", #e, __FILE__, __LINE__); \
			dump_panic("assert_dump"); \
		}
	#define assert_exit(e) \
		if (!(e)) { \
			lprintf("assertion failed: \"%s\" %s line %d\n", #e, __FILE__, __LINE__); \
			goto error_exit; \
		}
#else
	#define D_PRF(x)
	#define D_STMT(x)

	#ifndef assert
		#define assert(e)
	#endif
	
	#define assert_exit(e)
#endif

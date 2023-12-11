#pragma once

#define panic(s) _panic(s, FALSE, __FILE__, __LINE__);
#define real_panic(s) _real_panic(s, FALSE, __FILE__, __LINE__);
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
	
    #define assert_msg(msg, e) { \
        lprintf("%s: \"%s\" %s line %d\n", msg, #e, __FILE__, __LINE__); \
        panic("assert"); \
    }
	
	#ifndef assert
		#define assert(e) \
			if (!(e)) { \
			    assert_msg("assertion failed", e); \
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
    
    // array bounds checking
    #define assert_array_dim(ai, dim) \
        if (!((ai) >= (0) && (ai) < (dim))) { \
            printf("array bounds check failed: \"%s\"(%d) >= 0 AND < \"%s\"(%d) %s line %d\n", \
                #ai, ai, #dim, dim, __FILE__, __LINE__); \
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
	
    #define assert_msg(msg, e)
    #define assert_eq(v1, v2)
    #define assert_ne(v1, v2)
    #define assert_gt(v1, v2)
    #define assert_ge(v1, v2)
    #define assert_lt(v1, v2)
    #define assert_le(v1, v2)
    #define assert_eq(v1, v2)
    #define assert_array_dim(ai, dim)
	#define assert_dump(e)
	#define assert_exit(e)
#endif


#define SAN_ASSERT(e, stmt) \
    if (e) { stmt; } \
    else assert_msg("assertion failed", e);

// Applies NULL pointer check as prerequisite to stmt as required by clang static analyzer.
// Used instead of the typical "assert(v != NULL); stmt;"
#define SAN_NULL_PTR_CK(v, stmt) \
    if ((v) != NULL) { stmt; } \
    else assert_msg("null pointer", v);

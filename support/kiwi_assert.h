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

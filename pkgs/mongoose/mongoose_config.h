
#ifdef KIWISDR
 #define MG_ENABLE_IPV6 1
 
 #ifdef USE_SSL
  #define MG_TLS MG_TLS_BUILTIN
 #endif

 #define MONGOOSE_NEW_API
 #undef MG_ARCH
 #define MG_ARCH MG_ARCH_UNIX
#endif

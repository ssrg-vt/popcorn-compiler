#ifndef _PTH_INETERFACE_
#define _PTH_INETERFACE_

//#include "pth_p.h"

//Put in one of the config files
//#define CONFIG_POPCORN

#ifdef CONFIG_MUSL

    int __pthread_create(pthread_t *restrict, const pthread_attr_t *restrict, void *(*)(void *), void *restrict);
    #define kernel_pthread_create __pthread_create

    #ifdef CONFIG_POPCORN

    #else
        #define piperead pth_sc(read)
        #define pth_pipe pipe

    #endif

#elif defined CONFIG_NATIVE

    #define kernel_pthread_create clone
#else

    #error "No config available"
#endif



#endif //_PTH_INETERFACE_


//
//  segv.h
//  bgdm
//
//  Created by Bhagwan on 6/28/17.
//  Copyright © 2017 Bhagwan Software. All rights reserved.
//

#ifndef segv_h
#define segv_h

#include "core/TargetOS.h"

#ifdef TARGET_OS_WIN

#define __TRY           __try {
#define __EXCEPT(x)     } __except (BGDM_EXCEPTION(x)) {
#define __FINALLY       }

#define BGDM_EXCEPTION(msg) ExceptHandler(msg, GetExceptionCode(), GetExceptionInformation(), __FILE__, __FUNCTION__, __LINE__)
DWORD ExceptHandler(const char *msg, DWORD code, EXCEPTION_POINTERS *ep, const char *file, const char *func, int line);

#else
#include <errno.h>
#include <signal.h>
#include <setjmp.h>

#define STATUS_ARRAY_BOUNDS_EXCEEDED EDOM

#define __TRY           { struct sigaction sa_new = {}; \
    /* Use sigaction() not signal() because signal()    \
    handlers may reset on first firing, while           \
    sigaction() do not unless SA_RESETHAND is used */   \
    sa_new.sa_sigaction = segv_handler;                 \
    sa_new.sa_flags = SA_SIGINFO;                       \
    sigaction(SIGSEGV, &sa_new, &sa_old);               \
    state.initialized = 1;                              \
    if (!segv_protect()) {

    #define __EXCEPT(x)     } else { except_handler(x);

    #define __FINALLY       } {                         \
    segv_unprotect();                                   \
    sigaction(SIGSEGV, &sa_old, NULL);                  \
}}

#ifdef __cplusplus
extern "C" {
#endif
    
struct setjmp_wrapper {
    jmp_buf jmp;
    unsigned initialized : 1;
};
    
extern struct sigaction sa_old;
extern __thread struct setjmp_wrapper state;
    
void segv_handler(int sig, siginfo_t *info, void *where);

/* The inline trick below doesn't work
    the function returns to the IP right after
    the exception address, needed to macro it
    to preserve the right return address */
#define segv_protect()    _setjmp(state.jmp)
    
/* Must be inline so that second return
 doesn't use stack which was stepped on */
/*static int inline segv_protect()
{
    *  Manually handle signal mask in segv_handler()
        instead of using sigsetjmp()/siglongjmp() because
        Solaris makes the process copy-on-write and
        restores non-local variables on siglongjmp *
        state.initialized = 1;
    return setjmp(state.jmp);
}*/
    
static void inline segv_unprotect()
{
    state.initialized = 0;
}
    
#ifdef __cplusplus
}
#endif /* __cplusplus   */
#endif /* TARGET_OS_WIN */
#endif /* segv_h */

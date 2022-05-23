//
//  segv.c
//  bgdm
//
//  Created by Bhagwan on 6/28/17.
//  Copyright © 2017 Bhagwan Software. All rights reserved.
//

#include "segv.h"
#include <setjmp.h>
#include <signal.h>
#include <stdlib.h>

__thread struct setjmp_wrapper state;
struct sigaction sa_old = {};

void segv_handler(int sig, siginfo_t *info, void *where)
{
    sigset_t set;
    
    sigemptyset(&set);
    sigaddset(&set, SIGSEGV);
    /* sigproc_mask for single-threaded programs */
    pthread_sigmask(SIG_UNBLOCK, &set, NULL);
    
    if (state.initialized) {
        _longjmp(state.jmp, 1);
    } else {
        /* old_segv may be SIG_DFL which precludes a direct call */
        sigaction(SIGSEGV, &sa_old, NULL);
        raise(SIGSEGV);
    }
}

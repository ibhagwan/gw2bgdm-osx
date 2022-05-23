//
//  TargetOS.h
//  bgdm
//
//  Created by Bhagwan on 6/17/17.
//  Copyright © 2017 Bhagwan Software. All rights reserved.
//

#ifndef TargetOS_h
#define TargetOS_h

#if defined(_WIN32) || defined (_WIN64)
    //define something for Windows (32-bit and 64-bit, this part is common)
    #define TARGET_OS_WIN
    #ifdef _WIN64
        //define something for Windows (64-bit only)
        #define TARGET_OS_WIN64
    #else
        //define something for Windows (32-bit only)
        #define TARGET_OS_WIN32
#endif
#elif __APPLE__
    #include <TargetConditionals.h>
    #if TARGET_IPHONE_SIMULATOR
        // iOS Simulator
    #elif TARGET_OS_IPHONE
        // iOS device
    #elif TARGET_OS_MAC
        // Other kinds of Mac OS
    #else
    #   error "Unknown Apple platform"
    #endif
#elif __linux__
    // linux
#elif __unix__ // all unices not caught above
    // Unix
#elif defined(_POSIX_VERSION)
    // POSIX
#else
#   error "Unknown compiler"
#endif

#endif /* TargetOS_h */

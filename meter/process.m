//
//  process_osx.c
//  bgdm
//
//  Created by Bhagwan on 7/4/17.
//  Copyright © 2017 Bhagwan Software. All rights reserved.
//

#import <string.h>
#import <stdbool.h>
#import <sys/types.h>
#import "meter/process.h"
#import "offsets/Memory_OSX.h"


bool process_read(uint64_t base, void *dst, uint32_t bytes)
{
    if (!base || !dst)
        return false;
    
    if (readProcessMemory(getCurrentProcess(), (uintptr_t)base, dst, bytes, 0) == false)
    {
        memset(dst, 0, bytes);
        return false;
    }
    
    return true;
}

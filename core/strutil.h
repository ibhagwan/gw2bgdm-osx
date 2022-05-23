//
//  strutil.h
//  bgdm
//
//  Created by Bhagwan on 3/11/17.
//  Copyright © 2017 Bhagwan Software. All rights reserved.
//

#ifndef strutil_h
#define strutil_h

#include <wchar.h>

#ifdef __cplusplus
extern "C" {
#endif
    
char *trim(char *str);
wchar_t *wtrim(wchar_t *str);
    
#ifdef __cplusplus
}
#endif

#endif /* strutil_h */

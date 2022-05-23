//
//  Defines.h
//  bgdm
//
//  Created by Bhagwan on 6/16/17.
//  Copyright © 2017 Bhagwan Software. All rights reserved.
//
#ifndef BGDM_Defines_h
#define BGDM_Defines_h


#define NULL_IF_NIL(x)  ({ id xx = (x); (xx==nil)?[NSNull null]:xx; })
#define WAIT_UNTIL_DONE_ON_UI_THREAD    NO

#define ENSURE_THREAD_1_ARG(thread, arg)       \
if ( thread && thread != [NSThread currentThread] ) { \
    [self performSelector:_cmd onThread:thread withObject:arg waitUntilDone:NO]; \
    return; \
} \

#define ENSURE_THREAD_NO_ARGS(thread)   ENSURE_THREAD_1_ARG(thread, nil)


#define ENSURE_UI_THREAD_1_ARG(x)       \
if (![NSThread isMainThread]) { \
[self performSelectorOnMainThread:_cmd withObject:x waitUntilDone:WAIT_UNTIL_DONE_ON_UI_THREAD]; \
return; \
} \

#define ENSURE_UI_THREAD_NO_ARGS    ENSURE_UI_THREAD_1_ARG(nil)


#define ENSURE_UI_THREAD_WITH_OBJS(x,...)       \
if (![NSThread isMainThread]) { \
id o = [NSArray arrayWithObjects:@"" #x, ##__VA_ARGS__, nil];\
[self performSelectorOnMainThread:@selector(_dispatchWithObjectOnUIThread:) withObject:o waitUntilDone:WAIT_UNTIL_DONE_ON_UI_THREAD]; \
return; \
} \

#define ENSURE_UI_THREAD_WITH_OBJ(x,y,z) \
ENSURE_UI_THREAD_WITH_OBJS(x,NULL_IF_NIL(y),NULL_IF_NIL(z))


#define kDocumentsDirectory [NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES) objectAtIndex:0]


typedef enum {
    kConnectionStateDisconnected    = 0,
    kConnectionStateConnected,
    kConnectionStateNotRunning,
    kConnectionStateNotPatched,
    kConnectionStateNeedUpdate,
    kConnectionStateInitializing,
    kConnectionStateError,
    kConnectionStateEnd
} eConnectionState;



#import "Constants.h"
#import "Logging.h"


#endif

//
//  BGDMAppSrv.m
//  bgdm
//
//  Created by Bhagwan on 6/17/17.
//  Copyright © 2017 Bhagwan Software. All rights reserved.
//

#import "Defines.h"
#import "Constants.h"
#import "LocaleHelper.h"
#import "UserDefaults.h"
#import "entry_osx.h"
#import "BGDMBundSrv.h"

@implementation BGDMBundSrv

@synthesize version = _version;
@synthesize loggingLevel = _loggingLevel;
@synthesize locale = _locale;

+ (BGDMBundSrv *) sharedInstance
{
    static BGDMBundSrv *sharedInstance = nil;
    @synchronized(self)
    {
        if (!sharedInstance)
            sharedInstance = [[BGDMBundSrv alloc] init];
        return sharedInstance;
    }
}

- (id) init {
    
    __TRACE__
    
    if (self = [super init]) {
        
#if __has_feature(objc_arc)
        connection = [NSConnection connectionWithReceivePort:[NSPort port] sendPort:nil];
#else
        connection = [[NSConnection connectionWithReceivePort:[NSPort port] sendPort:nil] retain];
#endif
        
        if ([connection registerName:kBGDMBundSrvRegisteredName])
        {
            [connection setRootObject:self];
        }
        else
        {
#if !__has_feature(objc_arc)
            [connection release];
#endif
            connection=nil;
        }
    }
    return self;
}


- (void) dealloc
{
    __TRACE__
    
    if (connection) {
#if !__has_feature(objc_arc)
        [connection release];
#endif
        connection = nil;
    }
#if !__has_feature(objc_arc)
    [super dealloc];
#endif
}


- (NSString *) locale
{
    __TRACE__
    return SharedLocale.currentLocale;
}

- (NSUInteger) loggingLevel
{
    __TRACE__
    return gLoggingLevel;
}

- (void) setLoggingLevel:(NSUInteger)loggingLevel
{
    LogInfo(@"<logLevel:%@>", kLogLevelStringCAPS[loggingLevel]);
    gLoggingLevel = (LoggingLevel)loggingLevel;
}

- (NSInteger) initState
{
    return bgdm_init_state();
}

@end

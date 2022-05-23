//
//  BGDMAppSrv.m
//  bgdm
//
//  Created by Bhagwan on 6/17/17.
//  Copyright © 2017 Bhagwan Software. All rights reserved.
//

#import "Defines.h"
#import "Constants.h"
#import "UserDefaults.h"
#import "BGDMLibSrv.h"

@implementation BGDMLibSrv

@synthesize version = _version;
@synthesize loggingLevel = _loggingLevel;
@synthesize locale = _locale;

+ (BGDMLibSrv *) sharedInstance
{
    static BGDMLibSrv *sharedInstance = nil;
    @synchronized(self)
    {
        if (!sharedInstance)
            sharedInstance = [[BGDMLibSrv alloc] init];
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
        
        if ([connection registerName:kBGDMLibSrvRegisteredName])
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
    __TRACE__;
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
    return self.locale;
    
}

- (NSUInteger) loggingLevel
{
    __TRACE__
    return gLoggingLevel;
}

- (void) setLoggingLevel:(NSUInteger)loggingLevel
{
    __TRACE__
    LogInfo(@"<logLevel:%@>", kLogLevelStringCAPS[loggingLevel]);
    gLoggingLevel = (LoggingLevel)loggingLevel;
}


// First, we declare the function. Making it weak-linked
// ensures the preference pane won't crash if the function
// is removed from in a future version of Mac OS X.
extern void _CFBundleFlushBundleCaches(CFBundleRef bundle)
__attribute__((weak_import));

BOOL FlushBundleCache(NSBundle *prefBundle) {
    // Before calling the function, we need to check if it exists
    // since it was weak-linked.
    if (_CFBundleFlushBundleCaches != NULL) {
        NSLog(@"Flushing bundle cache with _CFBundleFlushBundleCaches");
        CFBundleRef cfBundle =
        CFBundleCreate(nil, (CFURLRef)[prefBundle bundleURL]);
        _CFBundleFlushBundleCaches(cfBundle);
        CFRelease(cfBundle);
        return YES; // Success
    }
    return NO; // Not available
}

- (Boolean) loadBundle:(NSString *)aPath
{
    LogDebug(@"loadBundle '%@'", aPath);
    NSBundle *bundle = [NSBundle bundleWithPath:aPath];
    return [bundle load];
}

- (Boolean) unloadBundle:(NSString *)aPath
{
    __TRACE__
    NSBundle *bundle = [NSBundle bundleWithPath:aPath];
    return [bundle unload];
}

@end

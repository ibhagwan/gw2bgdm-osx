//
//  BGDMAppClnt.m
//  bgdm
//
//  Created by Bhagwan on 6/17/17.
//  Copyright © 2017 Bhagwan Software. All rights reserved.
//

#import "Defines.h"
#import "Constants.h"
#import "LocaleHelper.h"
#import "UserDefaults.h"
#import "BGDMAppClnt.h"

@implementation BGDMAppClnt

@synthesize isConnected = _isConnected;
@synthesize version = _version;
@synthesize loggingLevel = _loggingLevel;
@synthesize locale = _locale;


+ (BGDMAppClnt *) sharedInstance
{
    static BGDMAppClnt *sharedInstance = nil;
    @synchronized(self)
    {
        if (!sharedInstance)
            sharedInstance = [[BGDMAppClnt alloc] init];
        
        [sharedInstance connect];
        return sharedInstance;
    }
}

- (id) init {
    
    if (self = [super init]) {
    }
    return self;
}


- (BOOL) isConnected
{
    return (clnt != nil);
}

- (void) disconenct
{
#if !__has_feature(objc_arc)
    [clnt release];
#endif
    clnt = nil;
}

- (BOOL) connect
{
    if ( ![self isConnected] )
    {
        @try {
            clnt = (id <BGDMAppSrvProtocol>)[NSConnection rootProxyForConnectionWithRegisteredName:kBGDMAppSrvRegisteredName host:nil];
            if (clnt) {
                [(NSDistantObject *)clnt setProtocolForProxy:@protocol(BGDMAppSrvProtocol)];
            }
        }
        @catch (NSException *ex)
        {
            [self disconenct];
        }
    }
    
    if ( [self isConnected] )
        return YES;
    
    return NO;
}

- (BOOL) validateConnection
{
    if ( [self isConnected] )
    {
        @try
        {
            [clnt version];
        }
        @catch (NSException *exception)
        {
            [self disconenct];
        }
    }
    
    if ( [self connect] )
        return YES;
    
    return NO;
}

- (float) version
{
    if ( [self isConnected] )
    {
        @try
        {
            return [clnt version];
        }
        @catch (NSException *ex)
        {
            [self disconenct];
        }
    }
    return 0;
}


- (NSUInteger) loggingLevel
{
    if ( [self isConnected] )
    {
        @try
        {
            return [clnt loggingLevel];
        }
        @catch (NSException *ex)
        {
            [self disconenct];
        }
    }
    return 0;
}

- (void) setLoggingLevel:(NSUInteger)logLevel
{
    if ( [self isConnected] )
    {
        @try
        {
            [clnt setLoggingLevel:logLevel];
        }
        @catch (NSException *ex)
        {
            [self disconenct];
        }
    }
}


- (NSString *) locale
{
    if ( [self isConnected] )
    {
        @try
        {
            return [clnt locale];
        }
        @catch (NSException *ex)
        {
            [self disconenct];
        }
    }
    return nil;
}


- (void) setLocale: (NSString *)loc
{
    if ( [self isConnected] )
    {
        @try
        {
            [clnt setLocale:loc];
        }
        @catch (NSException *ex)
        {
            [self disconenct];
        }
    }
}

- (NSString *) bundlePath
{
    if ( [self isConnected] )
    {
        @try
        {
            return [clnt bundlePath];
        }
        @catch (NSException *ex)
        {
            [self disconenct];
        }
    }
    return 0;
}


@end


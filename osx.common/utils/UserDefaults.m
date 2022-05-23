//
//  UserDefaults.m
//  DatAnywhere
//
//  Created by Bhagwan on 11/16/12.
//  Copyright (c) 2012 Bhagwan Software. All rights reserved.
//

#import "UserDefaults.h"
#import "Defines.h"

@implementation UserDefaults

@synthesize error = _error;

@synthesize locale = _locale;
@synthesize loggingLevel = _loggingLevel;
@synthesize startAtBoot = _startAtBoot;
@synthesize displayNotifications = _displayNotifications;


+ (UserDefaults *) sharedInstance
{
    static UserDefaults *sharedInstance = nil;
    @synchronized(self)
    {
        if (!sharedInstance)
            sharedInstance = [[UserDefaults alloc] init];
        return sharedInstance;
    }
}

- (id) init
{
    if ( self = [super init] ) {
        
        _userDefaults = [NSUserDefaults standardUserDefaults];
    }
    return self;
}


- (BOOL) boolForKey: (NSString *)key
{
    return [_userDefaults boolForKey:key];
}

- (BOOL) boolForKey: (NSString *)key defaultValue:(BOOL) defaultValue
{
    if ( [_userDefaults objectForKey:key] && [_userDefaults objectForKey:key] != [NSNull null])
        return [_userDefaults boolForKey:key];
    return defaultValue;
}

- (void) setBool: (BOOL)boolVal forKey: (NSString *)key
{
    [_userDefaults setBool:boolVal forKey:key];
    [_userDefaults synchronize];
}


- (NSInteger) integerForKey: (NSString *)key
{
    if ( [_userDefaults objectForKey:key] && [_userDefaults objectForKey:key] != [NSNull null])
        return [_userDefaults integerForKey:key];
    return -1;
}

- (NSInteger) integerForKey: (NSString *)key defaultValue:(int) defaultValue
{
    if ( [_userDefaults objectForKey:key] && [_userDefaults objectForKey:key] != [NSNull null])
        return [_userDefaults integerForKey:key];
    else
        return defaultValue;
}

- (void) setInteger: (NSInteger)integerVal forKey: (NSString *)key
{
    [_userDefaults setInteger:integerVal forKey:key];
    [_userDefaults synchronize];
}


- (NSString *) stringForKey: (NSString *)key
{
    if ( [_userDefaults objectForKey:key] && [_userDefaults objectForKey:key] != [NSNull null])
        return [_userDefaults stringForKey:key];
    return nil;
}

- (NSString *) stringForKey: (NSString *)key defaultValue:(NSString *) defaultValue
{
    if ( [_userDefaults objectForKey:key] && [_userDefaults objectForKey:key] != [NSNull null])
        return [_userDefaults stringForKey:key];
    else
        return defaultValue;
}

- (void) setString: (NSString *)stringVal forKey: (NSString *)key
{
    if (stringVal && stringVal.length>0)
        [_userDefaults setValue:stringVal forKey:key];
    else
        [_userDefaults removeObjectForKey:key];
    [_userDefaults synchronize];
}


- (id) objectForKey: (NSString *)key
{
    if ( [_userDefaults objectForKey:key] && [_userDefaults objectForKey:key] != [NSNull null])
        return [_userDefaults objectForKey:key];
    return nil;
}

- (void) setObject: (id)object forKey: (NSString *)key
{
    if (object && object != [NSNull null])
        [_userDefaults setValue:object forKey:key];
    else
        [_userDefaults removeObjectForKey:key];
    [_userDefaults synchronize];
}


- (NSInteger) loggingLevel
{
    _loggingLevel = [UserDefaultSharedInstance integerForKey:kBGDMPrefsLogLevel defaultValue:LoggingLevelInfo];
    return _loggingLevel;
}

- (void) setLoggingLevel:(NSInteger)loggingLevel
{
    _loggingLevel = loggingLevel;
    [self setInteger:gLoggingLevel forKey:kBGDMPrefsLogLevel];
}

- (NSString *) locale
{
    _locale = [self stringForKey:kBGDMPrefsAppLocale defaultValue:@"Base"];
    return _locale;
}

- (void) setLocale:(NSString *)locale
{
    _locale = locale;
    [self setString:locale forKey:kBGDMPrefsAppLocale];
}


- (BOOL) startAtBoot
{
    _startAtBoot = [self boolForKey:kBGDMPrefsStartAtBoot];
    return _startAtBoot;
}

- (void) setStartAtBoot: (BOOL)startAtBoot
{
    _startAtBoot = startAtBoot;
    [self setBool:_startAtBoot forKey:kBGDMPrefsStartAtBoot];
}

- (BOOL) displayNotifications
{
    _displayNotifications = [self boolForKey:kBGDMPrefsShowNotifications];
    return _displayNotifications;
}

- (void) setDisplayNotifications: (BOOL)displayNotifications
{
    _displayNotifications = displayNotifications;
    [self setBool:_startAtBoot forKey:kBGDMPrefsShowNotifications];
}



@end

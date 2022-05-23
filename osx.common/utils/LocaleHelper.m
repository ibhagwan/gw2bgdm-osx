//
//  LocalizedString.m
//  DatAnywhere
//
//  Created by Bhagwan on 11/15/12.
//  Copyright (c) 2012 Bhagwan Software. All rights reserved.
//

#import "LocaleHelper.h"

@implementation LocaleHelper

@synthesize currentLocale = _currentLocale;
@synthesize installedLocales = _installedLocales;

+ (LocaleHelper *) sharedInstance
{
    static LocaleHelper *sharedInstance = nil;
    @synchronized(self)
    {
        if (!sharedInstance)
            sharedInstance = [[LocaleHelper alloc] init];
        return sharedInstance;
    }
}


-  (id) init
{
    if ( self = [super init] ) {
        
        NSArray *availLocales = [[NSUserDefaults standardUserDefaults] objectForKey:@"AppleLanguages"];
        NSArray *localePaths = [[NSBundle mainBundle] pathsForResourcesOfType: @"lproj" inDirectory: nil];
        _currentLocale = [availLocales objectAtIndex:0];
        _installedLocales = [[NSMutableArray alloc] init];
        
        for (int i = 0; i < [localePaths count]; i++)
            //if ([availLocales containsObject:(NSString *)[localePaths objectAtIndex:i]] )
            [_installedLocales addObject:[[[localePaths objectAtIndex:i] lastPathComponent] stringByDeletingPathExtension]];
        
    }
    return self;
}


- (NSString*) localizedString: (NSString*) key
{
    NSString *path = [[NSBundle mainBundle] pathForResource:_currentLocale ofType:@"lproj"];
	NSString* str=[[NSBundle bundleWithPath:path] localizedStringForKey:key value:@"" table:nil];
    
    if (str != nil)
        return str;
    else
        return NSLocalizedString(key, nil);
}

- (NSString *)displayNameForKey:(id)key value:(id)value
{
    NSLocale *locale = [[NSLocale alloc] initWithLocaleIdentifier:_currentLocale];
    return [locale displayNameForKey:key value:value];
    locale = nil;
}

@end

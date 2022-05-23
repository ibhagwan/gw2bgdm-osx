//
//  LocaleHelper.h
//  DatAnywhere
//
//  Created by Bhagwan on 11/15/12.
//  Copyright (c) 2012 Bhagwan Software. All rights reserved.
//

#import <Foundation/Foundation.h>

#define SharedLocale    [LocaleHelper sharedInstance]
#define __nls(key)      [SharedLocale localizedString:key]

@interface LocaleHelper : NSObject

@property (nonatomic, retain) NSString *currentLocale;
@property (nonatomic, retain) NSMutableArray *installedLocales;

+ (LocaleHelper *) sharedInstance;
- (NSString *) localizedString: (NSString*) key;
- (NSString *)displayNameForKey:(id)key value:(id)value;

@end

//
//  KeychainWrapper.h
//  DatAnywhere
//
//  Created by Bhagwan on 11/12/12.
//  Copyright (c) 2012 Bhagwan Software. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "Keychain.h"

@interface KeychainWrapper : NSObject

+ (BOOL)setString:(NSString *)value forKey:(NSString *)key ERROR_PARAM_BY_REF;
+ (NSString *)stringForKey:(NSString *)key ERROR_PARAM_BY_REF;

+ (BOOL)setArray:(NSArray *)value forKey:(NSString *)key ERROR_PARAM_BY_REF;
+ (NSArray *)arrayForKey:(NSString *)key ERROR_PARAM_BY_REF;

+ (BOOL)setSet:(NSSet *)value forKey:(NSString *)key ERROR_PARAM_BY_REF;
+ (NSSet *)setForKey:(NSString *)key ERROR_PARAM_BY_REF;

@end

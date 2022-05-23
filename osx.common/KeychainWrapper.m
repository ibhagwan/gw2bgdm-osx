//
//  KeychainWrapper.m
//  DatAnywhere
//
//  Created by Bhagwan on 11/12/12.
//  Copyright (c) 2012 Bhagwan Software. All rights reserved.
//

#import <Security/Security.h>
#import "Defines.h"
#import "Keychain.h"
#import "KeychainWrapper.h"


#define kDelimeter @"-|-"


@implementation KeychainWrapper

+ (NSString *)serviceName
{
    static NSString *kDNServiceName = nil;
    if ( kDNServiceName == nil )
        kDNServiceName = [[[NSBundle mainBundle] infoDictionary] objectForKey:(NSString*)kCFBundleIdentifierKey];
    return kDNServiceName;
}


+(NSMutableDictionary *)newSearchDictionary: (NSString *)identifier
{
    NSMutableDictionary* searchDictionary = [NSMutableDictionary dictionary];
    NSData *encodedIdentifier = [identifier dataUsingEncoding:NSUTF8StringEncoding];
    
    [searchDictionary setObject: (id)kSecClassGenericPassword      forKey: (id)kSecClass];
    [searchDictionary setObject: encodedIdentifier                 forKey: (id)kSecAttrGeneric];
    [searchDictionary setObject: encodedIdentifier                 forKey: (id)kSecAttrAccount];
    [searchDictionary setObject: [KeychainWrapper serviceName]     forKey: (id)kSecAttrService];
    
    return searchDictionary;
}

+(BOOL)setObject:(NSString *)obj forKey:(NSString *)key ERROR_PARAM_BY_REF
{
    
    //
    //  If the object is nil, delete the item
    //
    if (obj == nil)
        return [self deleteObjectForKey:key error:error_];
    
    
#ifdef TARGET_OS_MAC
    
    if ( [[Keychain sharedKeychain] addGenericPassword:obj forService:[KeychainWrapper serviceName] account:key error:error_] )
        return YES;
    
    if ( [(*error_) code] == errSecDuplicateItem ) {
        if ([self deleteObjectForKey:key error:error_]) {
            if ( [[Keychain sharedKeychain] addGenericPassword:obj forService:[KeychainWrapper serviceName] account:key error:error_] )
                return YES;
        }
        
    }
    return NO;
    
#else //    TARGET_OS_IPHONE/TARGET_IPHONE_SIMULATOR
    OSStatus status;
    
    NSMutableDictionary *dict = [self newSearchDictionary:key];
    [dict setObject: [obj dataUsingEncoding:NSUTF8StringEncoding] forKey: (id) kSecValueData];
    
    
#if __has_feature(objc_arc)
    status = SecItemAdd ((__bridge CFDictionaryRef) dict, NULL);
#else
    status = SecItemAdd ((CFDictionaryRef) dict, NULL);
#endif
    
    if (status == errSecDuplicateItem) {
        
        //
        //  Delete the old key if already exists
        //  and retry to add the item
        //
        if ([self deleteObjectForKey:key error:error_]) {
#if __has_feature(objc_arc)
            status = SecItemAdd((__bridge CFDictionaryRef) dict, NULL);
#else
            status = SecItemAdd((CFDictionaryRef) dict, NULL);
#endif
        }
    }
    
    if (status != errSecSuccess) {
        LogError(@"SecItemAdd failed for key %@: %d", key, status);
        SetNSError(error, @"SecItemAdd failed for key %@: %d", key, status);
    }
    
    return (status == errSecSuccess);
#endif
}

/*+ (BOOL)updateObject:(NSString *)obj forKey:(NSString *)key {
    
    NSMutableDictionary *searchDictionary = [self newSearchDictionary::key];
    NSMutableDictionary *updateDictionary = [[NSMutableDictionary alloc] init];
    NSData *data = [obj dataUsingEncoding:NSUTF8StringEncoding];
    [updateDictionary setObject:data forKey:(id)kSecValueData];
    
    OSStatus status = SecItemUpdate((CFDictionaryRef)searchDictionary,菲律宾太阳城博赢,
                                    (CFDictionaryRef)updateDictionary);
  
#if !__has_feature(objc_arc)
    [searchDictionary release];
    [updateDictionary release];
#endif
 
    if (status != errSecSuccess) {
        LogError(@"SecItemUpdate failed for key %@: %d", key, status);
        SetNSError(error, @"SecItemUpdate failed for key %@: %d", key, status);
    }
 
    return (status == errSecSuccess);
}*/

+ (BOOL)deleteObjectForKey:(NSString *)key ERROR_PARAM_BY_REF {
    
#ifdef TARGET_OS_MAC
    
    if ([[Keychain sharedKeychain] removeGenericPasswordForService:[KeychainWrapper serviceName] account:key error:error_] )
        return YES;
    return NO;
    
#else //    TARGET_OS_IPHONE/TARGET_IPHONE_SIMULATOR
    
    NSMutableDictionary *searchDictionary = [self newSearchDictionary:key];
    
#if __has_feature(objc_arc)
    OSStatus status = SecItemDelete((__bridge CFDictionaryRef)searchDictionary);
#else
    OSStatus status = SecItemDelete((CFDictionaryRef)searchDictionary);
    [searchDictionary release];
#endif
    
    if (status != errSecSuccess) {
        LogError(@"SecItemDelete failed for key %@: %d", key, status);
        SetNSError(error_, @"SecItemDelete failed for key %@: %d", key, status);
    }
    
    return (status == errSecSuccess);
#endif
}

+(NSString *)objectForKey:(NSString *)key ERROR_PARAM_BY_REF
{
    
#ifdef TARGET_OS_MAC
    
    return [[[Keychain sharedKeychain] genericPasswordForService:[KeychainWrapper serviceName] account:key error:error_] copy];
    
#else //    TARGET_OS_IPHONE/TARGET_IPHONE_SIMULATOR
    
    NSMutableDictionary *query = [self newSearchDictionary:key];
    [query setObject:(id)kSecMatchLimitOne forKey:(id)kSecMatchLimit];
    [query setObject:(id)kCFBooleanTrue forKey:(id)kSecReturnData];
    
    CFDataRef data = nil;
    OSStatus status =
#if __has_feature(objc_arc)
    SecItemCopyMatching ( (__bridge CFDictionaryRef) query, (CFTypeRef *) &data );
#else
    SecItemCopyMatching((CFDictionaryRef) query, (CFTypeRef *)&data);
#endif
    if (status != errSecSuccess) {
        LogError(@"SecItemCopyMatching failed for key %@: %d", key, status);
        SetNSError(error_, @"SecItemCopyMatching failed for key %@: %d", key, status);
    }
    
    if (!data)
        return nil;
    
    NSString *s = [[NSString alloc]
                   initWithData:
#if __has_feature(objc_arc)
                   (__bridge_transfer NSData *)data
#else
                   (NSData *)data
#endif
                   encoding: NSUTF8StringEncoding];
    
#if !__has_feature(objc_arc)
    [s autorelease];
    CFRelease(data);
#endif
    
    return s;
    
#endif
}


+(BOOL)setString:(NSString *)value forKey:(NSString *)key ERROR_PARAM_BY_REF
{
    return [self setObject:value forKey:key error:error_];
}

+(NSString *)stringForKey:(NSString *)key ERROR_PARAM_BY_REF
{
    return [self objectForKey:key error:error_];
}

+(BOOL)setArray:(NSArray *)value forKey:(NSString *)key ERROR_PARAM_BY_REF
{
    NSString *components = [value componentsJoinedByString:kDelimeter];
    return [self setObject:components forKey:key error:error_];
}

+(NSArray *)arrayForKey:(NSString *)key ERROR_PARAM_BY_REF
{
    NSArray *array = nil;
    NSString *components = [self objectForKey:key error:error_];
    if (components)
        array = [NSArray arrayWithArray:[components componentsSeparatedByString:kDelimeter]];
    
    return array;
}

+(BOOL)setSet:(NSSet *)value forKey:(NSString *)key ERROR_PARAM_BY_REF
{
    return [self setArray:[value allObjects] forKey:key error:error_];
}

+(NSSet *)setForKey:(NSString *)key ERROR_PARAM_BY_REF
{
    NSSet *set = nil;
    NSArray *array = [self arrayForKey:key error:error_];
    if (array)
        set = [NSSet setWithArray:array];
    
    return set;
}

@end

//
//  Keychain.h
//  DatAnywhere
//
//  Created by Bhagwan on 11/13/12.
//  Copyright (c) 2012 Bhagwan Software. All rights reserved.
//

#import <Foundation/Foundation.h>

#if __has_feature(objc_arc)
#define ERROR_PARAM_BY_REF  error:(NSError * __unsafe_unretained *)error_
#else
#define ERROR_PARAM_BY_REF  error:(NSError **)error_
#endif


@interface Keychain : NSObject
+ (Keychain*) sharedKeychain;

- (NSString*) genericPasswordForService:(NSString*)service account:(NSString*)account ERROR_PARAM_BY_REF;
- (BOOL) addGenericPassword:(NSString*)password forService:(NSString*)service account:(NSString*)account ERROR_PARAM_BY_REF;
- (BOOL) removeGenericPasswordForService:(NSString*)service account:(NSString*)account ERROR_PARAM_BY_REF;

- (NSURL*) URLWithPasswordForURL:(NSURL*)url ERROR_PARAM_BY_REF;
- (BOOL) setPasswordForURL:(NSURL*)url ERROR_PARAM_BY_REF; //Remove if password is nil or add if not nil
- (BOOL) addPasswordForURL:(NSURL*)url ERROR_PARAM_BY_REF;
- (BOOL) removePasswordForURL:(NSURL*)url ERROR_PARAM_BY_REF;
@end

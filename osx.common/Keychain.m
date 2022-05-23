//
//  Keychain.m
//  DatAnywhere
//
//  Created by Bhagwan on 11/13/12.
//  Copyright (c) 2012 Bhagwan Software. All rights reserved.
//

#import <Security/Security.h>

#import "Defines.h"
#import "Keychain.h"
#import "NSURL+Parameters.h"



@implementation Keychain

+ (Keychain*) sharedKeychain
{
	static Keychain *keychain = nil;
    @synchronized(self)
    {
        if (!keychain)
            keychain = [[Keychain alloc] init];
        return keychain;
    }
}


+ (NSString *) logErrorForApi: (NSString *)apiName forURL:(NSURL *)url withOSStatus:(OSStatus)osstatus ERROR_PARAM_BY_REF
{
    NSString *strErr = [NSString stringWithFormat:@"%@ failed for URL '%@@%@%@' (error %i: \"%@\")",
                        apiName, [url user], [url host], [url path], osstatus, [NSMakeCollectable(SecCopyErrorMessageString(osstatus, NULL)) autorelease]];
    SetNSError2(error_, osstatus, @"%@", strErr);
    return strErr;
}

+ (NSString *) logErrorForApi: (NSString *)apiName forKey:(NSString *)key withOSStatus:(OSStatus)osstatus ERROR_PARAM_BY_REF
{
    NSString *strErr = [NSString stringWithFormat:@"%@ failed for key '%@' (error %i: \"%@\")",
                        apiName, key, osstatus, [NSMakeCollectable(SecCopyErrorMessageString(osstatus, NULL)) autorelease]];
    SetNSError2(error_, osstatus, @"%@", strErr);
    return strErr;
}

- (NSString*) genericPasswordForService:(NSString*)service account:(NSString*)account ERROR_PARAM_BY_REF
{
	const char*			utf8Service = [service UTF8String];
	const char*			utf8Account = [account UTF8String];
	NSString*			password = nil;
	UInt32				length;
	void*				data;
	OSStatus			error;
    
	error = SecKeychainFindGenericPassword(NULL, (UInt32)strlen(utf8Service), utf8Service, (UInt32)strlen(utf8Account), utf8Account, &length, &data, NULL);
	if(error == noErr) {
		password = [[[NSString alloc] initWithBytes:data length:length encoding:NSUTF8StringEncoding] autorelease];
		SecKeychainItemFreeContent(NULL, data);
	}
	else /*if(error != errSecItemNotFound)*/ {
        LogError([Keychain logErrorForApi:@"SecKeychainFindGenericPassword" forKey:account  withOSStatus:error error:error_]);
    }
    
	return password;
}

- (BOOL) addGenericPassword:(NSString*)password forService:(NSString*)service account:(NSString*)account ERROR_PARAM_BY_REF
{
	const char*			utf8Service = [service UTF8String];
	const char*			utf8Account = [account UTF8String];
	NSData*				data = [password dataUsingEncoding:NSUTF8StringEncoding];
	OSStatus			error;
    
	if(![data length])
        return NO;
    
	error = SecKeychainAddGenericPassword(NULL, (UInt32)strlen(utf8Service), utf8Service, (UInt32)strlen(utf8Account), utf8Account, (UInt32)[data length], [data bytes], NULL);
	if((error != noErr) /*&& (error != errSecDuplicateItem)*/) {
        LogError([Keychain logErrorForApi:@"SecKeychainAddGenericPassword" forKey:account  withOSStatus:error error:error_]);
		return NO;
	}
    
	return YES;
}

- (BOOL) removeGenericPasswordForService:(NSString*)service account:(NSString*)account ERROR_PARAM_BY_REF
{
	const char*			utf8Service = [service UTF8String];
	const char*			utf8Account = [account UTF8String];
	OSStatus			error;
	SecKeychainItemRef	item;
    
	error = SecKeychainFindGenericPassword(NULL, (UInt32)strlen(utf8Service), utf8Service, (UInt32)strlen(utf8Account), utf8Account, NULL, NULL, &item);
	if(error == noErr)
        error = SecKeychainItemDelete(item); //FIXME: Should we call CFRelease() on the item as well?
	if((error != noErr) && (error != errSecItemNotFound)) {
        LogError([Keychain logErrorForApi:@"SecKeychainItemDelete" forKey:account  withOSStatus:error error:error_]);
		return NO;
	}
    
	return YES;
}

static SecProtocolType _ProtocolFromURLScheme(NSString* scheme)
{
	scheme = [scheme lowercaseString];
    
	if([scheme isEqualToString:@"afp"])
        return kSecProtocolTypeAFP;
	else if([scheme isEqualToString:@"smb"])
        return kSecProtocolTypeSMB;
	else if([scheme isEqualToString:@"http"])
        return kSecProtocolTypeHTTP;
	else if([scheme isEqualToString:@"https"])
        return kSecProtocolTypeHTTPS;
	else if([scheme isEqualToString:@"ftp"])
        return kSecProtocolTypeFTP;
	else if([scheme isEqualToString:@"ftps"])
        return kSecProtocolTypeFTPS;
	else if([scheme isEqualToString:@"ssh"])
        return kSecProtocolTypeSSH;
    
	return kSecProtocolTypeAny;
}

- (BOOL) setPasswordForURL:(NSURL*)url ERROR_PARAM_BY_REF
{
	if([[url password] length])
        return [self addPasswordForURL:url error:error_];
	else
        return [self removePasswordForURL:url error:error_];
}

- (BOOL) addPasswordForURL:(NSURL*)url ERROR_PARAM_BY_REF
{
	const char*			utf8Host = [[url host] UTF8String];
	const char*			utf8Account = [[url user] UTF8String];
	const char*			utf8Path = [[url path] UTF8String];
	const char*			utf8Password = [[url passwordByReplacingPercentEscapes] UTF8String];
	UInt16				port = [[url port] unsignedShortValue];
	SecProtocolType		protocol = _ProtocolFromURLScheme([url scheme]);
	OSStatus			error;
    
	if(!utf8Account || !utf8Password)
        return NO;
    
	error = SecKeychainAddInternetPassword(NULL, (UInt32)strlen(utf8Host), utf8Host, 0, NULL, (UInt32)strlen(utf8Account), utf8Account, (UInt32)strlen(utf8Path), utf8Path, port, protocol, kSecAuthenticationTypeDefault, (UInt32)strlen(utf8Password), utf8Password, NULL);
	if((error != noErr) /*&& (error != errSecDuplicateItem)*/) {
        LogError([Keychain logErrorForApi:@"SecKeychainAddInternetPassword" forURL:url  withOSStatus:error error:error_]);
		return NO;
	}
    
	return YES;
}

- (BOOL) removePasswordForURL:(NSURL*)url ERROR_PARAM_BY_REF
{
	const char*			utf8Host = [[url host] UTF8String];
	const char*			utf8Account = [[url user] UTF8String];
	const char*			utf8Path = [[url path] UTF8String];
	UInt16				port = [[url port] unsignedShortValue];
	SecProtocolType		protocol = _ProtocolFromURLScheme([url scheme]);
	OSStatus			error;
	SecKeychainItemRef	item;
    
	if(!utf8Account)
        return NO;
    
	error = SecKeychainFindInternetPassword(NULL, (UInt32)strlen(utf8Host), utf8Host, 0, NULL, (UInt32)strlen(utf8Account), utf8Account, (UInt32)strlen(utf8Path), utf8Path, port, protocol, kSecAuthenticationTypeAny, NULL, NULL, &item);
	if(error == noErr)
        error = SecKeychainItemDelete(item); //FIXME: Should we call CFRelease() on the item as well?
	if((error != noErr) /*&& (error != errSecItemNotFound)*/) {
        LogError([Keychain logErrorForApi:@"SecKeychainItemDelete" forURL:url  withOSStatus:error error:error_]);
		return NO;
	}
    
	return YES;
}

- (NSURL*) URLWithPasswordForURL:(NSURL*)url ERROR_PARAM_BY_REF
{
	const char*			utf8Host = [[url host] UTF8String];
	const char*			utf8Account = [[url user] UTF8String];
	const char*			utf8Path = [[url path] UTF8String];
	const char*			utf8Password = [[url passwordByReplacingPercentEscapes] UTF8String];
	UInt16				port = [[url port] unsignedShortValue];
	SecProtocolType		protocol = _ProtocolFromURLScheme([url scheme]);
	UInt32				length;
	void*				data;
	OSStatus			error;
	NSString*			password;
    
	if(!utf8Account || utf8Password)
        return url;
    
	error = SecKeychainFindInternetPassword(NULL, (UInt32)strlen(utf8Host), utf8Host, 0, NULL, (UInt32)strlen(utf8Account), utf8Account, (UInt32)strlen(utf8Path), utf8Path, port, protocol, kSecAuthenticationTypeAny, &length, &data, NULL);
	if(error == noErr) {
		password = [[NSString alloc] initWithBytes:data length:length encoding:NSUTF8StringEncoding];
		url = [NSURL URLWithScheme:[url scheme] user:[url user] password:password host:[url host] port:port path:[url path]];
		[password release];
		SecKeychainItemFreeContent(NULL, data);
	}
	else /*if(error != errSecItemNotFound)*/ {
        LogError([Keychain logErrorForApi:@"SecKeychainFindInternetPassword" forURL:url  withOSStatus:error error:error_]);
    }
    
	return url;
}

@end

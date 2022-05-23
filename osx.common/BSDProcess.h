//
//  BSDProcess.h
//  bgdm
//
//  Created by Bhagwan on 6/22/17.
//  Copyright © 2017 Bhagwan Software. All rights reserved.
//

#ifndef BSDProcess_h
#define BSDProcess_h


@interface BSDProcess : NSObject
+ (NSArray*)getBSDProcessList;
+ (NSString *) bundleIdentifierForApplicationName:(NSString *)appName;
+ (NSDictionary *) processInfoProcessName:(NSString *)procName;
+ (NSString *) bundleIdentifierForProcessName:(NSString *)procName;
@end

#endif /* BSDProcess_h */

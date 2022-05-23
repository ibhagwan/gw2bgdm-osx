//
//  UserDefaults.h
//  DatAnywhere
//
//  Created by Bhagwan on 11/16/12.
//  Copyright (c) 2012 Bhagwan Software. All rights reserved.
//

#import <Foundation/Foundation.h>

#define UserDefaultSharedInstance   [UserDefaults sharedInstance]


@interface UserDefaults : NSObject {
    
    NSUserDefaults *_userDefaults;
}

+ (UserDefaults *) sharedInstance;

@property (nonatomic, readonly, strong) NSError *error;

@property (nonatomic, strong) NSString *locale;
@property (nonatomic, assign) NSInteger loggingLevel;
@property (nonatomic, assign) BOOL startAtBoot;
@property (nonatomic, assign) BOOL displayNotifications;

@end

//
//  BGDMAppClnt.h
//  bgdm
//
//  Created by Bhagwan on 6/17/17.
//  Copyright © 2017 Bhagwan Software. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "Protocols.h"

#define SharedLibClient     [BGDMLibClnt    sharedInstance]

@interface BGDMLibClnt : NSObject <BGDMLibSrvProtocol> {
    
    id <BGDMLibSrvProtocol> clnt;
}

@property (nonatomic, readonly) BOOL isConnected;
@property (nonatomic, readonly) float version;
@property (nonatomic, assign) NSUInteger loggingLevel;
@property (nonatomic, copy) NSString *locale;

+ (BGDMLibClnt *) sharedInstance;
- (BOOL) validateConnection;

- (Boolean) loadBundle:(NSString *)aPath;
- (Boolean) unloadBundle:(NSString *)aPath;

@end

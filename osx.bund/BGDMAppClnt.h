//
//  BGDMAppClnt.h
//  bgdm
//
//  Created by Bhagwan on 6/17/17.
//  Copyright © 2017 Bhagwan Software. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "Protocols.h"

#define SharedAppClient     [BGDMAppClnt    sharedInstance]

@interface BGDMAppClnt : NSObject <BGDMAppSrvProtocol> {
    
    id <BGDMAppSrvProtocol> clnt;
}

@property (nonatomic, readonly) BOOL isConnected;
@property (nonatomic, readonly) float version;
@property (nonatomic, assign) NSUInteger loggingLevel;
@property (nonatomic, copy) NSString *locale;

+ (BGDMAppClnt *) sharedInstance;
- (BOOL) validateConnection;

@end

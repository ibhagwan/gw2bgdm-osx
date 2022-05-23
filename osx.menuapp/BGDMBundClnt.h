//
//  BGDMAppClnt.h
//  bgdm
//
//  Created by Bhagwan on 6/17/17.
//  Copyright © 2017 Bhagwan Software. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "Protocols.h"

#define SharedBundClient     [BGDMBundClnt    sharedInstance]

@interface BGDMBundClnt : NSObject <BGDMBundSrvProtocol> {
    
    id <BGDMBundSrvProtocol> clnt;
}

@property (nonatomic, readonly) BOOL isConnected;
@property (nonatomic, readonly) float version;
@property (nonatomic, assign) NSUInteger loggingLevel;
@property (nonatomic, copy) NSString *locale;

+ (BGDMBundClnt *) sharedInstance;
- (BOOL) validateConnection;

@end

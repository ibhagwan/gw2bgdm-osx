//
//  BGDMAppSrv.h
//  bgdm
//
//  Created by Bhagwan on 6/17/17.
//  Copyright © 2017 Bhagwan Software. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "Protocols.h"

#define SharedBundServer     [BGDMBundSrv     sharedInstance]

@interface BGDMBundSrv : NSObject <BGDMBundSrvProtocol> {
    
    NSConnection* connection;
}

+ (BGDMBundSrv *) sharedInstance;

@end

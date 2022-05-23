//
//  BGDMAppSrv.h
//  bgdm
//
//  Created by Bhagwan on 6/17/17.
//  Copyright © 2017 Bhagwan Software. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "Protocols.h"

#define SharedLibServer     [BGDMLibSrv     sharedInstance]

@interface BGDMLibSrv : NSObject <BGDMLibSrvProtocol> {
    
    NSConnection* connection;
}

+ (BGDMLibSrv *) sharedInstance;

@end

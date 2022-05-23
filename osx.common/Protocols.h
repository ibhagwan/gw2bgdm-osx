//
//  Protocols.h
//  bgdm
//
//  Created by Bhagwan on 6/17/17.
//  Copyright © 2017 Bhagwan Software. All rights reserved.
//

#ifndef Protocols_h
#define Protocols_h


@protocol BGDMSrvClntProtocol
@property (nonatomic, readonly, assign) float version;
@property (nonatomic, assign) NSUInteger loggingLevel;
@property (nonatomic, copy) NSString *locale;
@end

@protocol BGDMBundSrvProtocol <BGDMSrvClntProtocol>
@property (nonatomic, readonly, assign) NSInteger initState;
@end

@protocol BGDMLibSrvProtocol <BGDMSrvClntProtocol>
- (Boolean) loadBundle:(NSString *)aPath;
- (Boolean) unloadBundle:(NSString *)aPath;
@end

@protocol BGDMAppSrvProtocol <BGDMSrvClntProtocol>
@property (nonatomic, readonly, copy) NSString *bundlePath;
@end

#endif /* Protocols_h */

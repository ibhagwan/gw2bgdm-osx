//
//  AppDelegate.h
//  bgdm.osx
//
//  Created by Bhagwan on 6/16/17.
//  Copyright © 2017 Bhagwan Software. All rights reserved.
//

#import <Cocoa/Cocoa.h>

@interface AppDelegate : NSObject <NSApplicationDelegate> {
    
    AuthorizationRef    _authRef;
    AuthorizationRef    _authTask;
    NSTimer             *heartbitTimer;
    BOOL                _isGW2running;
}

@property (assign, nonatomic) BOOL isGW2running;
@property (assign, nonatomic) NSInteger status;
@property (strong, readonly) NSString *statusText;
@property (strong, nonatomic) NSStatusItem *statusItem;
@property (nonatomic, readonly, retain) NSMutableArray *statusIcons;

- (IBAction) loadBundle:(id)sender;
- (IBAction) unloadBundle:(id)sender;
- (IBAction) loadModule:(id)sender;
- (IBAction) unloadModule:(id)sender;
- (IBAction) patchApp:(id)sender;
- (IBAction) unpatchApp:(id)sender;

@end


//
//  AppDelegate.m
//  bgdm.osx
//
//  Created by Bhagwan on 6/16/17.
//  Copyright © 2017 Bhagwan Software. All rights reserved.
//

#import "AppDelegate.h"
#import "Constants.h"
#import "Defines.h"
#import "UserDefaults.h"
#import "LocaleHelper.h"
#import "BGDMLibClnt.h"
#import "BGDMBundClnt.h"
#import "BGDMAppSrv.h"
#import "BSDProcess.h"
#import "offsets/offset_scan.h"
#import "offsets/Memory_OSX.h"

@interface AppDelegate ()


@end

@implementation AppDelegate

@synthesize isGW2running = _isGW2running;
@synthesize status = _status;
@synthesize statusItem = _statusItem;
@synthesize statusIcons = _statusIcons;

- (void)MessageBox:(NSString *)message
{
    ENSURE_UI_THREAD_1_ARG(message)
    
    NSAlert *alert = [[NSAlert alloc] init];
    alert.messageText = __nls(@"appName");
    alert.informativeText = message;
    [alert addButtonWithTitle:__nls(@"ok")];
    [alert runModal];
}


- (void)applicationDidFinishLaunching:(NSNotification *)aNotification {
    
    //
    //  Get our app locale & log level from settings
    //
    NSString *locale = UserDefaultSharedInstance.locale;
    gLoggingLevel = (LoggingLevel)UserDefaultSharedInstance.loggingLevel;
    if ( locale && locale.length > 0 )
        [self setLocale:locale];
    [self setLogLevel:gLoggingLevel];

    //
    //  Get our image cache
    //
    NSImage *image = nil;
    _statusIcons = [[NSMutableArray alloc] init];
    image = [NSImage imageNamed:kBGDMIconDisconnected]; [image setSize:NSMakeSize(16, 16)];
    [_statusIcons addObject:image];
    image = [NSImage imageNamed:kBGDMIconConnected]; [image setSize:NSMakeSize(16, 16)];
    [_statusIcons addObject:image];
    image = [NSImage imageNamed:kBGDMIconNotRunning]; [image setSize:NSMakeSize(16, 16)];
    [_statusIcons addObject:image];
    image = [NSImage imageNamed:kBGDMIconNotPatched]; [image setSize:NSMakeSize(16, 16)];
    [_statusIcons addObject:image];
    image = [NSImage imageNamed:kBGDMIconNeedUpdate]; [image setSize:NSMakeSize(16, 16)];
    [_statusIcons addObject:image];
    image = [NSImage imageNamed:kBGDMIconInitializing]; [image setSize:NSMakeSize(16, 16)];
    [_statusIcons addObject:image];
    image = [NSImage imageNamed:kBGDMIconError]; [image setSize:NSMakeSize(16, 16)];
    [_statusIcons addObject:image];
    
    
    //
    //  Start our app server
    //
    SharedAppServer;
    
    
    //
    //  Observe connection state changes
    //
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(connectionStateNotification:) name:kConnectionStateChangedNotification object:nil];
    
    
    //
    // Start our heartbit connection to the Lib/Bundle
    //
    [self scheduleTimerOnMainThread];
    
    
    //
    //  Install the menu bar icon & menu
    //
    self.statusItem = [[NSStatusBar systemStatusBar] statusItemWithLength:NSVariableStatusItemLength];
    
    [self refreshState];
    if (_status == kConnectionStateDisconnected) {
        [self loadBundle:nil];
        [self refreshState];
    }
    
    //[_statusItem.image setTemplate:YES];
    //_statusItem.highlightMode = NO;
    _statusItem.toolTip = __nls(@"tooltip");
    
    [_statusItem setAction:@selector(itemClicked:)];}


- (void)applicationWillTerminate:(NSNotification *)aNotification {
    
    [self authFree];
}


- (void)itemClicked:(id)sender {
    
    //Look for control click, close app if detect ctrl+click
    NSEvent *event = [NSApp currentEvent];
    if([event modifierFlags] & NSEventModifierFlagControl) {
        [[NSApplication sharedApplication] terminate:self];
        return;
    }
    
    NSMenu *menu = [self buildMenu];//[super menu];
    [_statusItem popUpStatusItemMenu:menu];
}

- (IBAction)aboutPanel:(id)sender
{
    [NSApp activateIgnoringOtherApps:YES];
    [NSApp orderFrontStandardAboutPanel:sender];
}

- (NSString *) locale
{
    return SharedLocale.currentLocale;
}

- (void) setLocale: (NSString *) locale
{
    SharedLocale.currentLocale = locale;
    SharedLibClient.locale = locale;
    SharedBundClient.locale = locale;
    UserDefaultSharedInstance.locale = locale;
}

- (void) setLogLevel: (LoggingLevel) logLevel
{
    gLoggingLevel = logLevel;
    SharedLibClient.loggingLevel = gLoggingLevel;
    SharedBundClient.loggingLevel = gLoggingLevel;
    UserDefaultSharedInstance.loggingLevel = gLoggingLevel;
}

-(NSInteger) refreshStatus
{
    _isGW2running = [BSDProcess processInfoProcessName:kGW2BinaryName] != nil;
    
    if ( !self.isGW2running )
        _status = kConnectionStateNotRunning;
    else if ( ![SharedLibClient validateConnection] )
        _status = kConnectionStateNotPatched;
    else if ( ![SharedBundClient validateConnection] )
        _status = kConnectionStateDisconnected;
    else if ( SharedBundClient.initState == 1)
        _status = kConnectionStateInitializing;
    else if ( SharedBundClient.initState == 2)
        _status = kConnectionStateConnected;
    else
        _status = kConnectionStateError;
    return _status;
}

-(void) setStatusImage: (NSUInteger) state
{
    ENSURE_UI_THREAD_NO_ARGS
    
    if (state >= kConnectionStateDisconnected && state < kConnectionStateEnd) {
        _statusItem.image = [_statusIcons objectAtIndex:state];
    }
}

-(NSString *) statusText
{
    switch (_status) {
        case(kConnectionStateConnected):
            return __nls(@"stateConnected");
            break;
        case(kConnectionStateDisconnected):
            return __nls(@"stateDisconnected");
            break;
        case(kConnectionStateNotRunning):
            return __nls(@"stateNotRunning");
            break;
        case(kConnectionStateNotPatched):
            return __nls(@"stateNotPatched");
            break;
        case(kConnectionStateNeedUpdate):
            return __nls(@"stateNeedUpdate");
            break;
        case(kConnectionStateInitializing):
            return __nls(@"stateInit");
            break;
        case(kConnectionStateError):
            return __nls(@"stateError");
            break;
    }
    return @"<internal err>";
}

-(void) setState: (NSUInteger) state
{

    _status = state;
    [self setStatusImage:state];
}

-(void) refreshState
{
    [self refreshStatus];
    [self setStatusImage:_status];
}


- (IBAction) setLanguage:(id)sender
{
    [self setLocale:[sender representedObject]];
}

- (IBAction) setLoglevelFromString:(id)sender
{
    NSString *logLevelString = [sender representedObject];
    for (int i = LoggingLevelTrace; i <= LoggingLevelCrit; i++) {
        if ( [kLogLevelString[i] isEqualToString:logLevelString] )
            [self setLogLevel:i];
    }
}

#pragma mark -
#pragma mark Privilege

- (void) authFree
{
    if (self->_authRef != nil) {
        AuthorizationFree( self->_authRef, kAuthorizationFlagDefaults );
        self->_authRef = nil;
    }
    
    if (self->_authRef != nil) {
        AuthorizationFree( self->_authRef, kAuthorizationFlagDefaults );
        self->_authRef = nil;
    }
}

- (bool) authAcquire
{
    if (self->_authRef != nil)
        return true;
    
    AuthorizationFlags myFlags = kAuthorizationFlagDefaults;
    
    OSStatus myStatus = AuthorizationCreate(NULL, kAuthorizationEmptyEnvironment, myFlags, &self->_authRef);
    if( myStatus != errAuthorizationSuccess ){
        [self authFree];
        return false;;
    }
    
    AuthorizationItem myItems = {kAuthorizationRightExecute, 0, NULL, 0};
    AuthorizationRights myRights = {1, &myItems};
    
    myFlags = kAuthorizationFlagDefaults | kAuthorizationFlagInteractionAllowed | kAuthorizationFlagPreAuthorize | kAuthorizationFlagExtendRights;
    myStatus = AuthorizationCopyRights( _authRef, &myRights, NULL, myFlags, NULL );
    
    if( myStatus != errAuthorizationSuccess ){
        [self authFree];
        return false;
    }
    return true;
}

- (bool) authAcquireTaskport
{
    if (self->_authTask != nil)
        return true;
    
    OSStatus stat;
    AuthorizationItem taskport_item[] = {{"system.privilege.taskport:"}};
    AuthorizationRights rights = {1, taskport_item}, *out_rights = NULL;
    
    AuthorizationFlags auth_flags = kAuthorizationFlagExtendRights | kAuthorizationFlagPreAuthorize | kAuthorizationFlagInteractionAllowed | ( 1 << 5);
    
    stat = AuthorizationCreate (NULL, kAuthorizationEmptyEnvironment, auth_flags, &self->_authTask);
    if (stat != errAuthorizationSuccess) {
        return false;
    }
    
    stat = AuthorizationCopyRights ( self->_authTask, &rights, kAuthorizationEmptyEnvironment, auth_flags, &out_rights);
    if (stat != errAuthorizationSuccess) {
        return false;
    }
    return true;
}

#pragma mark -
#pragma mark Load/Unload

- (IBAction) unloadBundle:(id)sender
{
    NSString *bundlePath = [SharedAppServer bundlePath];
    if( [SharedLibClient unloadBundle:bundlePath] ) {
        //[self MessageBox:__nls(@"unloadSuccess")];
    } else {
        [self MessageBox:__nls(@"unloadFailure")];
        [self setState:kConnectionStateConnected];
    }
}

- (IBAction) loadBundle:(id)sender
{
    ENSURE_UI_THREAD_NO_ARGS
    
    NSString *bundlePath = [SharedAppServer bundlePath];
    if( [SharedLibClient loadBundle:bundlePath] ) {
        [self setLogLevel:gLoggingLevel];
        //[self MessageBox:__nls(@"injectSuccess")];
    } else {
        [self MessageBox:__nls(@"injectFailure")];
        [self setState:kConnectionStateConnected];
    }
}

- (IBAction) unloadModule:(id)sender
{
    ENSURE_UI_THREAD_NO_ARGS
    
    if( [self authAcquire] )
    {
        [self setState:kConnectionStateDisconnected];
    }
    else
    {
        [self MessageBox:__nls(@"injectPrivFail")];
    }
    
}

- (IBAction) loadModule:(id)sender
{
    if( [self authAcquire] )
    {
        int s = 0;
        char* myArguments[] = { "", "", NULL };
        myArguments[0] = (char*)[[[NSBundle mainBundle] pathForResource:@"bgdm_" ofType:@"bundle"] UTF8String];
        myArguments[1] = (char*)[[[NSBundle mainBundle] pathForResource:@"mach_inject_bundle_stub" ofType:@"bundle"] UTF8String];
        
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
        OSStatus myStatus = AuthorizationExecuteWithPrivileges(self->_authRef, [[[NSBundle mainBundle] pathForResource:@"bgdm.loader" ofType:@""] UTF8String], kAuthorizationFlagDefaults, myArguments, NULL);
        wait(&s);
#pragma clang diagnostic pop
        
        if( myStatus != errAuthorizationSuccess )
            [self MessageBox:__nls(@"injectFailure")];
        else {
            [self MessageBox:__nls(@"injectSuccess")];
            [self setState:kConnectionStateConnected];
        }
        
    }
    else
    {
        [self MessageBox:__nls(@"injectPrivFail")];
    }
}

- (NSString *) guildWarsApplicationPath
{
    NSWorkspace * workspace = [NSWorkspace sharedWorkspace];
    NSString * appPath = [workspace fullPathForApplication:@"Guild Wars 2 64-bit"];
    return [NSString stringWithString:appPath];
}

- (IBAction) patchApp:(id)sender
{
    
    if( [self authAcquire] )
    {
        NSString * appPath = [self guildWarsApplicationPath];
        if (appPath == nil) {
            [self MessageBox:__nls(@"notInstalled")];
            return;
        }
        NSString * binaryPath = [NSString stringWithFormat:@"%@%@%@", appPath, @"/Contents/MacOS/", kGW2BinaryName];
        NSString * origPath = [NSString stringWithFormat:@"%@%@", binaryPath, @".orig"];
        
        if ( ![[NSFileManager defaultManager] fileExistsAtPath:binaryPath isDirectory:nil] ){
            [self MessageBox:__nls(@"notInstalled")];
            return;
        }
        
        // Do we need to backup the original binary?
        // Do not override older backups
        if ( ![[NSFileManager defaultManager] fileExistsAtPath:origPath isDirectory:nil] ) {
            NSError * err = NULL;
            if ( ![[NSFileManager defaultManager] copyItemAtPath:binaryPath toPath:origPath error:&err] ) {
                [self MessageBox:[err localizedDescription]];
                return;
            }
        }
        
        int s = 0;
        char* myArguments[] = { "", "", "", "", "", NULL };
        myArguments[0] = (char*)[[[NSBundle mainBundle] pathForResource:@"lib_bgdm" ofType:@"dylib"] UTF8String];
        myArguments[1] = (char*)[binaryPath UTF8String];
        myArguments[2] = (char*)[binaryPath UTF8String];
        myArguments[3] = "--all-yes";
        myArguments[4] = "--no-strip-codesig";
    
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
        OSStatus myStatus = AuthorizationExecuteWithPrivileges(self->_authRef, [[[NSBundle mainBundle] pathForResource:@"insert_dylib" ofType:@""] UTF8String], kAuthorizationFlagDefaults, myArguments, NULL);
        wait(&s);
#pragma clang diagnostic pop
    
        if( myStatus == errAuthorizationSuccess ) {
            [self MessageBox:__nls(@"patchSuccess")];
        } else {
            [self MessageBox:__nls(@"patchFailure")];
        }
    }
    else
    {
        [self MessageBox:__nls(@"privFail")];
    }
}

- (IBAction) unpatchApp:(id)sender
{
    
    if( [self authAcquire] )
    {
        NSString * appPath = [self guildWarsApplicationPath];
        if (appPath == nil) {
            [self MessageBox:__nls(@"notInstalled")];
            return;
        }
        
        NSString * binaryPath = [NSString stringWithFormat:@"%@%@%@", appPath, @"/Contents/MacOS/", kGW2BinaryName];
        NSString * origPath = [NSString stringWithFormat:@"%@%@", binaryPath, @".orig"];
        
        bool success = false;
        if ( [[NSFileManager defaultManager] fileExistsAtPath:origPath isDirectory:nil] ) {
        
            NSError * err = NULL;
            [[NSFileManager defaultManager] removeItemAtPath:binaryPath error:&err];
            if ( [[NSFileManager defaultManager] moveItemAtPath:origPath toPath:binaryPath error:&err] ) {
                success = true;
            }
        }
    
        if (success) {
            [self MessageBox:__nls(@"revertSuccess")];
        } else {
            [self MessageBox:__nls(@"revertFailure")];
        }

    }
    else
    {
        [self MessageBox:__nls(@"privFail")];
    }
}


- (IBAction) scanOffsets:(id)sender
{
    //int pid = pidFromProcessName( [kGW2BinaryName UTF8String] );
    if ( [self authAcquireTaskport] )
        scanOffsets(0, "bgdm", true);
}


#pragma mark -
#pragma mark Menu

- (NSMenu *)buildMenu
{

    [self refreshStatus];
    
    NSMenu *menu = [[NSMenu alloc] init];
    [menu addItemWithTitle:self.statusText   action:nil keyEquivalent:@""];
    [menu addItem:[NSMenuItem separatorItem]];
    
    [menu addItemWithTitle:__nls(@"patchGW2")       action:self.isGW2running ? nil : @selector(patchApp:)    keyEquivalent:@""];
    [menu addItemWithTitle:__nls(@"revertGW2")      action:self.isGW2running ? nil : @selector(unpatchApp:)  keyEquivalent:@""];

    [menu addItem:[NSMenuItem separatorItem]];
    [menu addItem:[self buildLangMenuItem]];
    [menu addItem:[self buildLoggingMenuItem]];
    [menu addItem:[NSMenuItem separatorItem]];
    
    //[menu addItem:[self buildDebugMenuItem]];
    //[menu addItem:[NSMenuItem separatorItem]];
    
    [menu addItemWithTitle:__nls(@"about")         action:@selector(aboutPanel:) keyEquivalent:@""];
    [menu addItemWithTitle:__nls(@"quit")          action:@selector(terminate:) keyEquivalent:@"q"];
    
    
    return menu;
}

- (NSMenuItem *)buildLangMenuItem
{
    NSMenu *langMenu = [[NSMenu alloc] initWithTitle:__nls(@"language")];
    NSMenuItem *langMenuItem = [[NSMenuItem alloc] initWithTitle:__nls(@"language") action:nil keyEquivalent:@""];
    
    NSArray *installedLocales = SharedLocale.installedLocales;
    for (int i = 0; i < [installedLocales count]; i++) {
        
        NSString *locale = [SharedLocale displayNameForKey:NSLocaleIdentifier value:[installedLocales objectAtIndex:i]];
        NSMutableString* str = (locale == nil) ? [NSMutableString stringWithString:@"English"] : [NSMutableString stringWithString:locale];
        NSMenuItem *menuItem = [langMenu addItemWithTitle:str  action:@selector(setLanguage:) keyEquivalent:@""];
        [menuItem setRepresentedObject:[installedLocales objectAtIndex:i]];
        
        //
        //  Mark current language
        //
        if ( [[installedLocales objectAtIndex:i] isEqualToString:SharedLocale.currentLocale] )
            [menuItem setState:NSOnState];
    }
    
    [langMenuItem setSubmenu:langMenu];
    return langMenuItem;
}

- (NSMenuItem *)buildLoggingMenuItem
{
    NSMenu *menu = [[NSMenu alloc] initWithTitle:__nls(@"logging")];
    NSMenuItem *menuItem = [[NSMenuItem alloc] initWithTitle:__nls(@"logging") action:nil keyEquivalent:@""];
    
    
    for (int i = LoggingLevelTrace; i <= LoggingLevelCrit; i++) {
        
        NSMutableString* str = [NSMutableString stringWithString:kLogLevelString[i]];
        NSMenuItem *mi = [menu addItemWithTitle:str  action:@selector(setLoglevelFromString:) keyEquivalent:@""];
        [mi setRepresentedObject:kLogLevelString[i]];
        
        //
        //  Mark current language
        //
        if ( [kLogLevelString[gLoggingLevel] isEqualToString:kLogLevelString[i]] )
            [mi setState:NSOnState];
    }
    
    [menuItem setSubmenu:menu];
    return menuItem;
}

- (NSMenuItem *)buildDebugMenuItem
{
    NSMenu *menu = [[NSMenu alloc] initWithTitle:__nls(@"debug")];
    NSMenuItem *menuItem = [[NSMenuItem alloc] initWithTitle:__nls(@"debug") action:nil keyEquivalent:@""];
    
    {
        NSMenuItem *mi = [menu addItemWithTitle:__nls(@"scanOffsets")  action:((self.isGW2running) ? @selector(scanOffsets:) : @selector(scanOffsets:)) keyEquivalent:@""];
        [mi setRepresentedObject:nil];
    }
    
    [menuItem setSubmenu:menu];
    return menuItem;
}



#pragma mark -
#pragma mark heartbit timer

- (void) scheduleTimerOnMainThread
{
    //
    //  [2012-12-17 15:30] Bhagwan:
    //  Apparently the notifications are executed on calling thread (i.e. on the download thread)
    //  and the timer is invalidated when the transfer is done, we must schedule it on main thread
    //
    ENSURE_UI_THREAD_NO_ARGS
    
    if ( !heartbitTimer ) {
        heartbitTimer = [NSTimer scheduledTimerWithTimeInterval:0.50
                                                         target:self
                                                       selector:@selector(heartbitTimerFired:)
                                                       userInfo:nil
                                                        repeats:YES];
    }
}

- (void) heartbitTimerFired: (NSTimer *)timer
{
    NSInteger oldStatus = _status;
    [self refreshStatus];
    if (oldStatus != _status && _status == kConnectionStateDisconnected) {
        [self loadBundle:nil];
    }
    [self notifyConnectionStateChange:kConnectionStateChangedNotification notificationCode:_status userInfo:nil];
        
}

#pragma mark -
#pragma mark notification handlers

- (void) notifyConnectionStateChange:(NSString *)aNotificationName notificationCode:(NSInteger)aNotificationCode userInfo:(NSDictionary *)aUserInfo
{
    NSMutableDictionary *userInfo = [NSMutableDictionary dictionary];
    [userInfo setObject:@(aNotificationCode) forKey:kNotificationCodeKey];
    if (aUserInfo) [userInfo addEntriesFromDictionary:aUserInfo];
    [[NSNotificationCenter defaultCenter] postNotificationName:aNotificationName object:self userInfo:userInfo];
}


- (void) connectionStateNotification: (NSNotification *)aNotification {
    
    //__TRACE__
    uint state = [[aNotification.userInfo objectForKey:kNotificationCodeKey] intValue];
    if (state < kConnectionStateDisconnected || state >= kConnectionStateEnd)
        NSAssert(FALSE, @"Invalid notification code %u %@", state, aNotification);
    [self setStatusImage:state];
}

@end

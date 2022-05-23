//
//  _bgdm.m
//  _bgdm
//
//  Created by Bhagwan on 6/22/17.
//  Copyright © 2017 Bhagwan Software. All rights reserved.
//

#import <Cocoa/Cocoa.h>
#include <objc/runtime.h>
#import "_bgdm.h"
#import "Constants.h"
#import "Defines.h"
#import "Logging.h"
#import "BGDMLibSrv.h"


@implementation _bgdm

+ (void)MessageBox:(NSString *)message
{
    ENSURE_UI_THREAD_1_ARG(message)
    
    NSAlert *alert = [[NSAlert alloc] init];
    alert.messageText = @"BGDM";
    alert.informativeText = message;
    [alert addButtonWithTitle:@"OK"];
    [alert runModal];
}

@end

void loggerCreate( const char *filename )
{
    static bool m_init = false;
    if (m_init) return;
    NSString *nsfile = [NSString stringWithUTF8String:filename];
    NSString *nspath = [NSString stringWithFormat:@"%@/%@", kDocumentsDirectory, nsfile];
    if ( ![[NSFileManager defaultManager] fileExistsAtPath:[nspath stringByDeletingLastPathComponent] isDirectory:nil] )
        [[NSFileManager defaultManager] createDirectoryAtPath:[nspath stringByDeletingLastPathComponent] withIntermediateDirectories:TRUE attributes:nil error:nil];
    const char* fullpath = [nspath UTF8String];
    freopen(fullpath, "w+", stdout);
    freopen(fullpath, "w+", stderr);
    CLogDebug(@"log path:  %@", nspath);
    CLogDebug(@"log level: %@", kLogLevelString[gLoggingLevel]);
    m_init = true;
}

void load( void ) __attribute__ ((constructor));
void load( void )
{
    
    //[_bgdm MessageBox:@"load called from lib_bgdm"];
    gLoggingLevel = LoggingLevelTrace;
    loggerCreate( [[NSString stringWithFormat:@"%@/%@", kBGDMDocFolderPath, kBGDMLogFile] UTF8String] );
    
    CLogDebug(@"-----  Loading BGDM module ...");
    
    //
    //  Initialize our interprocess server
    //
    SharedLibServer;
    //[SharedAppServer setLocale:[SharedAppClient locale]];
    //[SharedAppServer setLoggingLevel:[SharedAppClient loggingLevel]];
    
    CLogDebug(@"-----  BGDM module loaded successfully");
}

void unload( void ) __attribute__ ((destructor));
void unload( void )
{
    CLogDebug(@"-----  BGDM module unloaded successfully");
}

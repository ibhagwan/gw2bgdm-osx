//
//  load.m
//  bgdm.ext
//
//  Created by Bhagwan on 6/16/17.
//  Copyright © 2017 Bhagwan Software. All rights reserved.
//

#import <Cocoa/Cocoa.h>
#import <objc/runtime.h>
#import <glfw3_osx.h>
#import "Constants.h"
#import "Defines.h"
#import "Logging.h"
#import "localization.h"
#import "BGDMBundSrv.h"
#import "OpenGLhooks.h"
#import "imgui_osx.h"
#import "entry_osx.h"
#import "offset_scan.h"


static void error_callback(int error, const char* description)
{
    CLogError(@"glfw3 error %d: %s\n", error, description);
}

void loggerCreate( const char *filename )
{
    static bool m_init = false;
    if (m_init) return;
    NSString *nsfile = [NSString stringWithUTF8String:filename];
    NSString *nspath = [NSString stringWithFormat:@"%@/%@", kDocumentsDirectory, nsfile];
    if ( ![[NSFileManager defaultManager] fileExistsAtPath:[nspath stringByDeletingLastPathComponent] isDirectory:nil] )
        [[NSFileManager defaultManager] createDirectoryAtPath:[nspath stringByDeletingLastPathComponent] withIntermediateDirectories:TRUE attributes:nil error:nil];
    const char* fullpath = [nspath UTF8String];
    freopen(fullpath, "a+", stdout);
    freopen(fullpath, "a+", stderr);
    CLogDebug(@"log path:  %@", nspath);
    CLogDebug(@"log level: %@", kLogLevelString[gLoggingLevel]);
    m_init = true;
}


void load( void ) __attribute__ ((constructor));
void load( void )
{

    //[BGDMosx MessageBox:@"load called from bgdm_.bundle"];
    gLoggingLevel = LoggingLevelTrace;
    loggerCreate( [[NSString stringWithFormat:@"%@/%@", kBGDMDocFolderPath, kBGDMLogFile] UTF8String] );
    
    CLogDebug(@"-----  Loading BGDM bundle ...");
    
    //
    //  Initialize our interprocess server
    //
    SharedBundServer;

    //
    // Initialize glfw3
    //
    glfwSetErrorCallback(error_callback);
    if (!glfwInit())
        return;
    
    /*GLFWwindow* window = glfwCreateWindow(1280, 720, "ImGui OpenGL2 example", NULL, NULL);
    glfwMakeContextCurrent(window);
    ImGui_Init_Win(window);*/
    
    //
    // Override openGL methods
    initOpenGLHooks();
    
    //
    // Initialize the game hooks
    //
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_BACKGROUND, 0), ^{
        bgdm_init();
    });
    
    CLogDebug(@"-----  BGDM bundle loaded successfully");
}

void unload( void ) __attribute__ ((destructor));
void unload( void )
{
    CLogDebug(@"-----  Unloading BGDM bundle ...");
    
    // Shutdown BGDM
    bgdm_fini();
    
    // TODO: hangs the shutdown
    //ImGui_Shutdown();
    
    // TODO: raises an exception: "objc[44080]: Invalid or prematurely-freed autorelease pool 0x7f83940097a8."
    //glfwTerminate();
    
    CLogDebug(@"-----  BGDM bundle unloaded successfully");
}

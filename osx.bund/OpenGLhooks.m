//
//  opengl_hooks.c
//  bgdm
//
//  Created by Bhagwan on 6/22/17.
//  Copyright © 2017 Bhagwan Software. All rights reserved.
//

#import <AppKit/AppKit.h>
#import <Foundation/Foundation.h>
#import <objc/objc-runtime.h>
#import <dlfcn.h>
#import <mach-o/dyld.h>
#import <OpenGL/OpenGL.h>
#import <GLFW/glfw3.h>
#import <glfw3_osx.h>
#import "mach_override.h"
#import "jrswizzle.h"
#import "Defines.h"
#import "imgui_osx.h"
#import "OpenGLhooks.h"


typedef CGLError (*CGLFlushDrawable_s)(CGLContextObj ctx);
typedef CGError (*CGDisplayCursor_s)(CGDirectDisplayID display);
CGLFlushDrawable_s oCGLFlushDrawable;
CGDisplayCursor_s oCGDisplayShowCursor;
CGDisplayCursor_s oCGDisplayHideCursor;

typedef struct _Context {
    
    struct _Context *next;
    
    CGLContextObj cglctx;
    __unsafe_unretained NSOpenGLContext *nsctx;
    
    unsigned int uiWidth, uiHeight;
    unsigned int uiLeft, uiRight, uiTop, uiBottom;
    
    int iSocket;
    GLuint texture;
    
    // overlay texture in shared memory
    unsigned char *a_ucTexture;
    unsigned int uiMappedLength;
    
    bool bValid;
    bool bMesa;
    
    GLuint uiProgram;
    
    clock_t timeT;
    unsigned int frameCount;
    
    GLint maxVertexAttribs;
    GLboolean* vertexAttribStates;
    
} Context;


static Context *contexts = NULL;

static void newContext(Context * ctx) {
    
    ctx->iSocket = -1;
    ctx->texture = ~0U;
    ctx->timeT = clock();
    ctx->frameCount = 0;
    
    CLogDebug(@"OpenGL Version %s, Vendor %s, Renderer %s, Shader %s",
              glGetString(GL_VERSION), glGetString(GL_VENDOR),
              glGetString(GL_RENDERER), glGetString(GL_SHADING_LANGUAGE_VERSION));
    
    glLinkProgram(ctx->uiProgram);
    glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &ctx->maxVertexAttribs);
    ctx->vertexAttribStates = calloc((size_t) ctx->maxVertexAttribs, sizeof(GLboolean));
}

@implementation NSOpenGLContext (BGDM)

+ (void) swizzleMethod:(SEL)origSel withMethod:(SEL)newSel
{
    NSError *error = nil;
    if ( [self jr_swizzleMethod:origSel withMethod:newSel error:&error] ) {
        
        LogDebug(@"Successfully hooked %@", NSStringFromSelector(origSel));
    } else {
        
        NSString *strErr = [NSString stringWithFormat:@"Error hooking %@, error:%@", NSStringFromSelector(origSel), error.localizedDescription];
        LogError(@"%@", strErr);
        NSAssert1(false, @"%@", strErr);
    }
}


+ (void) hook
{
    LogDebug(@"Attempting to hook NSOpenGL");
    
    [self swizzleMethod:@selector(initWithCGLContextObj:)
             withMethod:@selector(initWithCGLContextObjOverride:)];
    
    [self swizzleMethod:@selector(initWithFormat:shareContext:)
             withMethod:@selector(initWithFormatOverride:shareContext:)];
    
    [self swizzleMethod:@selector(setView:)
             withMethod:@selector(setViewOverride:)];
    
    [self swizzleMethod:@selector(flushBuffer)
             withMethod:@selector(flushBufferOverride)];
}

- (void)dealloc
{
    __TRACE__
    LogDebug(@"dealloc");
}

+ (void)load
{
    //[self hook];
}

- (id) initWithFormatOverride:(NSOpenGLPixelFormat *)format shareContext:(NSOpenGLContext *)share
{
    __TRACE__
    LogDebug(@"initWithFormatOverride");
    return [self initWithFormatOverride:format shareContext:share];
}

- (NSOpenGLContext *)initWithCGLContextObjOverride:(struct _CGLContextObject *)context
{
    __TRACE__
    LogDebug(@"initWithCGLContextObjOverride");
    return [self initWithCGLContextObjOverride:context];
}

- (void) setViewOverride:(NSView*)aView
{
    static struct GLFWwindow *window = NULL;
    __TRACE__
    LogDebug(@"setViewOverride <view:%p> <window:%p>", aView, aView.window);
    if (window == NULL) {
        window = ImGui_Init((__bridge_retained void*)self, (__bridge_retained void*)aView);
    } else {
        NSView *oldView = (__bridge NSView *)glfwGetNSView(window);
        bool needUpdate = glfwSetNSView(window, (__bridge_retained void*)aView);
        if (needUpdate) {
            LogDebug(@"View was updated <view:%p> <window:%p> <oldView:%p> <oldWindow:%p>", aView, aView.window, oldView, oldView.window);
            ImGui_ResetInit();
            ImGui_ResetPost();
        }
    }
    [self setViewOverride:aView];
}

- (void) flushBufferOverride {
    
    Context *c = contexts;
    while (c) {
        if (c->nsctx == self && c->cglctx == [self CGLContextObj])
            break;
        c = c->next;
    }
    
    if (!c) {
        LogDebug(@"<context=%p>", self);
        c = malloc(sizeof(Context));
        if (!c) {
            LogError(@"malloc failed");
            return;
        }
        c->next = contexts;
        c->nsctx = (NSOpenGLContext *)self;
        c->cglctx = (CGLContextObj)[self CGLContextObj];
        contexts = c;
        newContext(c);
    }
    
    NSView *v = [c->nsctx view];
    int64_t width = 0, height = 0;
    if (v) {
        NSRect r = [v bounds];
        width = r.size.width;
        height = r.size.height;
    } else {
        //if (AVAIL(CGMainDisplayID)) {
            CGDirectDisplayID md = CGMainDisplayID();
            if (CGDisplayIsCaptured(md)) {
                width = CGDisplayPixelsWide(md);
                height = CGDisplayPixelsHigh(md);
            }
        //}
        if (!width && !height) {
            GLint viewport[4];
            glGetIntegerv(GL_VIEWPORT, viewport);
            width = viewport[2];
            height = viewport[3];
        }
    }
    
    ImGui_NewFrame();
    ImGui_Render(0, false, false);
    
    [self flushBufferOverride];
}
@end

void CGLFlushDrawableOverride_(CGLContextObj ctx) {
    
    Context *c = contexts;
    
    /* Sometimes, we can get a FlushDrawable where the current context is NULL.
     * Also, check that the context CGLFlushDrawable was called on is the active context.
     * If it isn't, who knows what context we're drawing on?
     *
     * Maybe we should even use CGLSetCurrentContext() to switch to the context that's being
     * flushed?
     */
    CGLContextObj current = CGLGetCurrentContext();
    if (current == NULL || ctx != current)
        goto skip;
    
    while (c) {
        if (c->cglctx == ctx) {
            // There is no NSOpenGLContext for this CGLContext, so we should draw.
            if (c->nsctx == NULL)
                break;
            // This context is coupled with a NSOpenGLContext, so skip.
            else
                goto skip;
        }
        c = c->next;
    }
    
    if (!c) {
        CLogDebug(@"<context=%p>", ctx);
        c = malloc(sizeof(Context));
        if (!c) {
            CLogError(@"malloc failed");
            return;
        }
        memset(c, 0, sizeof(Context));
        c->next = contexts;
        c->cglctx = ctx;
        c->nsctx = NULL;
        contexts = c;
        newContext(c);
    }
    
    int64_t width = 0, height = 0;
    //if (AVAIL(CGMainDisplayID)) {
        CGDirectDisplayID md = CGMainDisplayID();
        if (CGDisplayIsCaptured(md)) {
            width = CGDisplayPixelsWide(md);
            height = CGDisplayPixelsHigh(md);
        }
    //}
    if (!width && !height) {
        GLint viewport[4];
        glGetIntegerv(GL_VIEWPORT, viewport);
        width = viewport[2];
        height = viewport[3];
        // Are the viewport values crazy? Skip them in that case.
        if (height < 0 || width < 0 || height > 5000 || width > 5000)
            goto skip;
    }
    
skip:
    oCGLFlushDrawable(ctx);
}

CGError CGDisplayShowCursorOverride(CGDirectDisplayID display) {
    __CTRACE__
    return oCGDisplayShowCursor(display);
}

CGError CGDisplayHideCursorOverride(CGDirectDisplayID display) {
    __CTRACE__
    return oCGDisplayHideCursor(display);
}

    
static void machOverride(void *originalFunctionAddress,
                         const char *originalFunctionName,
                         const void *overrideFunctionAddress,
                         void **originalFunctionReentryIsland )
{
    if (!originalFunctionAddress) {
        CLogCrit(@"Unable to find entry point for %s", originalFunctionName);
        NSCAssert1(false, @"Unable to find entry point for %s", originalFunctionName);
        return;
    }
    
    mach_error_t err =mach_override_ptr(originalFunctionAddress, overrideFunctionAddress, (void **) originalFunctionReentryIsland);
    if (err != err_none) {
        
        CLogCrit(@"Unable to override function %s, error %#x", originalFunctionName, err);
        NSCAssert1(false, @"Unable to override function %s", originalFunctionName);
        return;
    }
    
    CLogDebug(@"Successfully hooked %s <orig:%p> <new:%p> <reentry:%p>",
              originalFunctionName, originalFunctionAddress, overrideFunctionAddress, *originalFunctionReentryIsland);
}

static void machOverrideFunc(const char *originalFunctionName,
                             const void *overrideFunctionAddress,
                             void **originalFunctionReentryIsland )
{
    void *ptr = dlsym(RTLD_DEFAULT, originalFunctionName);
    machOverride(ptr, originalFunctionName, overrideFunctionAddress, originalFunctionReentryIsland);
}

/*
typedef OSStatus (*SecKeychainFindGenericPassword_ptr)(CFTypeRef __nullable keychainOrArray,  UInt32 serviceNameLength, const char * __nullable serviceName, UInt32 accountNameLength, const char * __nullable accountName, UInt32 * __nullable passwordLength, void * __nullable * __nullable passwordData, SecKeychainItemRef * __nullable CF_RETURNS_RETAINED itemRef);

typedef OSStatus (*SecKeychainFindInternetPassword_ptr)(CFTypeRef __nullable keychainOrArray, UInt32 serverNameLength, const char * __nullable serverName, UInt32 securityDomainLength, const char * __nullable securityDomain, UInt32 accountNameLength, const char * __nullable accountName, UInt32 pathLength, const char * __nullable path, UInt16 port, SecProtocolType protocol, SecAuthenticationType authenticationType, UInt32 * __nullable passwordLength, void * __nullable * __nullable passwordData, SecKeychainItemRef * __nullable CF_RETURNS_RETAINED itemRef);

static SecKeychainFindGenericPassword_ptr oSecKeychainFindGenericPassword;
static SecKeychainFindInternetPassword_ptr oSecKeychainFindInternetPassword;

OSStatus SecKeychainFindGenericPassword_override(CFTypeRef __nullable keychainOrArray,  UInt32 serviceNameLength, const char * __nullable serviceName, UInt32 accountNameLength, const char * __nullable accountName, UInt32 * __nullable passwordLength, void * __nullable * __nullable passwordData, SecKeychainItemRef * __nullable CF_RETURNS_RETAINED itemRef)
{
    OSStatus status = oSecKeychainFindGenericPassword(keychainOrArray, serviceNameLength, serviceName, accountNameLength, accountName, passwordLength, passwordData, itemRef);
    CLogDebug(@"SecKeychainFindGenericPassword <status:%d> %s %s ", status, serviceName, accountName);
    return status;
}

OSStatus SecKeychainFindInternetPassword_override(CFTypeRef __nullable keychainOrArray, UInt32 serverNameLength, const char * __nullable serverName, UInt32 securityDomainLength, const char * __nullable securityDomain, UInt32 accountNameLength, const char * __nullable accountName, UInt32 pathLength, const char * __nullable path, UInt16 port, SecProtocolType protocol, SecAuthenticationType authenticationType, UInt32 * __nullable passwordLength, void * __nullable * __nullable passwordData, SecKeychainItemRef * __nullable CF_RETURNS_RETAINED itemRef)
{
    OSStatus status = oSecKeychainFindInternetPassword(keychainOrArray, serverNameLength, serverName, securityDomainLength, securityDomain, accountNameLength, accountName, pathLength, path, port, protocol, authenticationType, passwordLength, passwordData, itemRef);
    CLogDebug(@"SecKeychainFindInternetPassword <status:%d> %s %s %s", status, serverName, securityDomain, accountName);
    return status;
}
 */

void initOpenGLHooks() {
    
    __CTRACE__
    
    void *nsgl = NULL, *cgl = NULL;
    nsgl = dlsym(RTLD_DEFAULT, "NSClassFromString");
    cgl = dlsym(RTLD_DEFAULT, "CGLFlushDrawable");
    
    //
    //  NSOpenGL
    //
    if (nsgl) {
        [NSOpenGLContext hook];
    }
    
    //
    // CGL
    //
    if (cgl != NULL) {
        CLogDebug(@"Attempting to hook CGL");
        
        //machOverride(cgl, "CGLFlushDrawable", CGLFlushDrawableOverride_, (void **) &oCGLFlushDrawable);
        //machOverrideFunc("CGDisplayHideCursor", CGDisplayHideCursorOverride, (void **) &oCGDisplayHideCursor);
        //machOverrideFunc("CGDisplayShowCursor", CGDisplayShowCursorOverride, (void **) &oCGDisplayShowCursor);
    }
    
    //machOverrideFunc("SecKeychainFindGenericPassword", SecKeychainFindGenericPassword_override, (void **) &oSecKeychainFindGenericPassword);
    //machOverrideFunc("SecKeychainFindInternetPassword", SecKeychainFindInternetPassword_override, (void **) &oSecKeychainFindInternetPassword);
}


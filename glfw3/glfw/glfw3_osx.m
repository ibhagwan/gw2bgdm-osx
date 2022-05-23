//
//  glfw3_osx.m
//  glfw3_osx
//
//  Created by Bhagwan on 6/22/17.
//  Copyright © 2017 Bhagwan Software. All rights reserved.
//

#import <AppKit/AppKit.h>
#import <Foundation/Foundation.h>
#import <objc/objc-runtime.h>
#import <mach/mach_time.h>
#import <OpenGL/OpenGL.h>
#include <GLFW/glfw3.h>
#include "internal.h"

typedef bool (* GLFWinputfnc)(_GLFWwindow*);
static GLFWinputfnc _capKeyCb = NULL;
static GLFWinputfnc _capMouseCb = NULL;
static GLFWinputfnc _capInputCb = NULL;
static GLFWinputfnc _capDisableCb = NULL;

#define _GLFW_SHUOLD_DISABLE_CAP_RETURN     \
    if (__capDisableCb(self.window)) {      \
        return ( [self.window->ns.view isGLFWContentView] != nil ) ? false : true; }

static bool __capKeyCb(_GLFWwindow* window)
{
    if (window && _capKeyCb)
        return _capKeyCb(window);
    return false;
}

static bool __capMouseCb(_GLFWwindow* window)
{
    if (window && _capMouseCb)
        return _capMouseCb(window);
    return false;
}

static bool __capInputCb(_GLFWwindow* window)
{
    if (window && _capInputCb)
        return _capInputCb(window);
    return false;
}

static bool __capDisableCb(_GLFWwindow* window)
{
    if (window && _capDisableCb)
        return _capDisableCb(window);
    return false;
}

GLFWbool initializeAppKit(void);
GLFWbool acquireMonitor(_GLFWwindow* window);
void centerCursor(_GLFWwindow *window);
void updateCursorImage(_GLFWwindow* window);
int translateFlags(NSUInteger flags);
int translateKey(unsigned int key);
NSUInteger translateKeyToModifierFlag(int key);
float transformY(float y);

static void makeContextCurrentNSGL(_GLFWwindow* window)
{
    if (window)
        [window->context.nsgl.object makeCurrentContext];
    else
        [NSOpenGLContext clearCurrentContext];
    
    _glfwPlatformSetCurrentContext(window);
}

static void swapBuffersNSGL(_GLFWwindow* window)
{
    // ARP appears to be unnecessary, but this is future-proof
    [window->context.nsgl.object flushBuffer];
}

static void swapIntervalNSGL(int interval)
{
    _GLFWwindow* window = _glfwPlatformGetCurrentContext();
    
    GLint sync = interval;
    [window->context.nsgl.object setValues:&sync
                              forParameter:NSOpenGLCPSwapInterval];
}

static int extensionSupportedNSGL(const char* extension)
{
    // There are no NSGL extensions
    return GLFW_FALSE;
}

static GLFWglproc getProcAddressNSGL(const char* procname)
{
    CFStringRef symbolName = CFStringCreateWithCString(kCFAllocatorDefault,
                                                       procname,
                                                       kCFStringEncodingASCII);
    
    GLFWglproc symbol = CFBundleGetFunctionPointerForName(_glfw.nsgl.framework,
                                                          symbolName);
    
    CFRelease(symbolName);
    
    return symbol;
}

// Destroy the OpenGL context
//
static void destroyContextNSGL(_GLFWwindow* window)
{
    [window->context.nsgl.pixelFormat release];
    window->context.nsgl.pixelFormat = nil;
    
    [window->context.nsgl.object release];
    window->context.nsgl.object = nil;
}

//------------------------------------------------------------------------
// Delegate for window related notifications
//------------------------------------------------------------------------

@interface GLFWWindowDelegate : NSObject
{
    _GLFWwindow* window;
}

- (id)initWithGlfwWindow:(_GLFWwindow *)initWindow;

@end


//------------------------------------------------------------------------
// Content view class for the GLFW window
//------------------------------------------------------------------------

@interface GLFWContentView : NSView <NSTextInputClient>
{
    _GLFWwindow* window;
    NSTrackingArea* trackingArea;
    NSMutableAttributedString* markedText;
}

- (id)initWithGlfwWindow:(_GLFWwindow *)initWindow;

@end
 
/*

@interface GLFWViewDelegate : NSObject
{
    GLFWContentView* _target;
    NSView* _orig;
}

- (id)initWithGlfwWindow:(_GLFWwindow *)aWindow andView:(NSView*)aView;

@end

@interface GLFWViewProxy : NSProxy
{
    GLFWViewDelegate* _delegate;
    NSView <NSTextInputClient>* _view;
}

@property (nonatomic, retain) NSView* GLFWView;
@property (nonatomic, assign) GLFWViewDelegate* delegate;

- (id)initWithGlfwWindow:(_GLFWwindow *)aWindow andView:(NSView*)aView;

@end

@implementation GLFWViewProxy

@synthesize GLFWView = _view;
@synthesize delegate = _delegate;

- (id)initWithGlfwWindow:(_GLFWwindow *)aWindow andView:(NSView*)aView
{
    //self = [super init];
    if (aView == nil)
        return nil;
        //aView = [[NSView <NSTextInputClient> alloc] init];
    
    _view = [(NSView <NSTextInputClient>*)aView retain];
    _delegate = [[GLFWViewDelegate alloc] initWithGlfwWindow:aWindow andView:aView];
    
    return self;
}

- (void)dealloc
{
    [_view dealloc];
    [_delegate dealloc];
    [super dealloc];
}

- (BOOL)isKindOfClass:(Class)aClass
{
    return aClass == [GLFWViewProxy class] || [_view isKindOfClass:aClass];
}

- (NSMethodSignature *)methodSignatureForSelector:(SEL)selector {
    return [_view methodSignatureForSelector:selector];
}

- (void)forwardInvocation:(NSInvocation *)anInvocation {
    
    SEL aSelector = [anInvocation selector];
    
    NSString *str = NSStringFromSelector(aSelector);
    if ( ![str isEqualToString:@"frame"] &&
        ![str isEqualToString:@"convertRectToBacking:"] &&
        ![str isEqualToString:@"respondsToSelector:"] &&
        ![str isEqualToString:@"inputContext"] &&
        ![str isEqualToString:@"bounds"] &&
        ![str isEqualToString:@"_managesOpenGLDrawable"] &&
        ![str isEqualToString:@"_isLayerBacked"] &&
        ![str isEqualToString:@"_syncSurfaceIfPostponed"] &&
        ![str isEqualToString:@"wantsBestResolutionOpenGLSurface"] &&
        ![str isEqualToString:@"mouseMoved:"]) {
        NSLog(@"SELECTOR: %@ => %@", str, [_delegate respondsToSelector: aSelector] ? @"delgate" : @"view");
    }
    
    if ([_delegate respondsToSelector: aSelector]) {
        [anInvocation invokeWithTarget:_delegate];
    } else {
        [anInvocation invokeWithTarget:_view];
    }
}

- (BOOL)acceptsFirstResponder
{
    NSLog(@"becomeFirstResponder %p %p", self, _view.window);
    return YES;
}

- (BOOL)becomeFirstResponder
{
    NSLog(@"becomeFirstResponder %p %p", self, _view.window);
    return YES;
}

- (BOOL)didBecomeActiveFirstResponder
{
    NSLog(@"didBecomeActiveFirstResponder %p %p", self, _view.window);
    return YES;
}


@end

@implementation GLFWViewDelegate

- (id)initWithGlfwWindow:(_GLFWwindow *)aWindow andView:(NSView*)aView
{
    NSAssert((aView != nil), @"aView cannot be nil");
    self = [super init];
    _orig = aView;
    _target = [[GLFWContentView alloc] initWithGlfwWindow:aWindow];
    return self;
}

- (void)dealloc
{
    [_target dealloc];
    [super dealloc];
}

- (BOOL)respondsToSelector:(SEL)aSelector
{
    if ( aSelector == @selector(keyDown:) ||
        aSelector == @selector(keyUp:) ||
        aSelector == @selector(mouseMoved:) ||
        aSelector == @selector(mouseDown:) ||
        aSelector == @selector(mouseUp:) ||
        aSelector == @selector(conformsToProtocol:))
        return TRUE;
    return FALSE;
}

- (BOOL)conformsToProtocol:(Protocol *)aProtocol
{
    NSLog(@"conformsToProtocol %@: %d", NSStringFromProtocol(aProtocol), [_orig conformsToProtocol:aProtocol]);
    return [_orig conformsToProtocol:aProtocol];
}

- (void)keyDown:(NSEvent *)event
{
    [_target keyDown:event];
    [_orig keyDown:event];
}

- (void)keyUp:(NSEvent *)event
{
    [_target keyUp:event];
    [_orig keyUp:event];
}

- (void)mouseMoved:(NSEvent *)event
{
    [_target mouseMoved:event];
    [_orig mouseMoved:event];
}

- (void)mouseDown:(NSEvent *)event
{
    [_target mouseDown:event];
    [_orig mouseDown:event];
}

- (void)mouseUp:(NSEvent *)event
{
    [_target mouseUp:event];
    [_orig mouseUp:event];
}

@end


@interface NSWindow (GLFW3)

@property (nonatomic, retain) id oldFirstResponder;
@property (nonatomic, retain) GLFWViewProxy *glfwFirstResponder;

@end

@implementation NSWindow (GLFW3)


+ (void)swizzleMethodForClass:(Class)aClass method:(SEL)inOriginal withMethod:(SEL)inSwizzled
{
    Method originalMethod = class_getInstanceMethod(aClass, inOriginal);
    Method swizzledMethod = class_getInstanceMethod(aClass, inSwizzled);
    method_exchangeImplementations(originalMethod, swizzledMethod);
}

+ (void) hook
{
    [NSWindow swizzleMethodForClass:[NSWindow class] method:@selector(makeFirstResponder:)   withMethod:@selector(makeFirstResponderOverride:)];
}

+ (void)load
{
    [self hook];
}

- (GLFWViewProxy *)glfwFirstResponder {
    return (GLFWViewProxy *)objc_getAssociatedObject(self, @selector(glfwFirstResponder));
}

- (void)setGlfwFirstResponder:(GLFWViewProxy *)value {
    objc_setAssociatedObject(self, @selector(glfwFirstResponder), (id)value, OBJC_ASSOCIATION_ASSIGN);
}

- (id)oldFirstResponder {
    return (id)objc_getAssociatedObject(self, @selector(oldFirstResponder));
}

- (void)setOldFirstResponder:(id)value {
    objc_setAssociatedObject(self, @selector(oldFirstResponder), value, OBJC_ASSOCIATION_ASSIGN);
}

- (BOOL)makeFirstResponderOverride:(nullable NSResponder *)responder
{
    NSLog(@"makeFirstResponder self:%p resp:%p initial:%p view:%p", self, responder, self.initialFirstResponder, self.contentView);
    if ( self.glfwFirstResponder && responder == self.glfwFirstResponder.GLFWView ) {
        NSLog(@"makeFirstResponder overriding %p %p -> %p", self, responder, self.glfwFirstResponder);
        return [self makeFirstResponderOverride:(id)self.glfwFirstResponder];
    }
    return [self makeFirstResponderOverride:responder];
}

@end

static GLFWbool attachNativeView(_GLFWwindow* window, NSView *nsview)
{
    NSCAssert((nsview.window == window->ns.object), @"(nsview.window == window->ns.object)");
    
    if (window->ns.view != nil &&
        [window->ns.view isKindOfClass:[GLFWViewProxy class]] ) {
        
        if ( [window->ns.view GLFWView] == nsview )
            return GLFW_FALSE;
        else {
            NSCAssert(false, @"[window->ns.view GLFWView] == nsview");
            [window->ns.view dealloc];
            window->ns.view = nil;
        }
    }
    
    window->ns.view = [[GLFWViewProxy alloc] initWithGlfwWindow:window andView:nsview];
    if (window->ns.view == nil)
    {
        _glfwInputError(GLFW_PLATFORM_ERROR,
                        "Cocoa: Unable to attach view");
        return GLFW_FALSE;
    }
    
    if (window->ns.object) {
        //NSView *oldView = [[window->ns.object contentView] retain];
        //[window->ns.object setOldFirstResponder:oldView];
        //[window->ns.object setContentView:window->ns.view];
        //[window->ns.object setInitialFirstResponder:window->ns.view];
        //[window->ns.object setGlfwFirstResponder:window->ns.view];
        [window->ns.object makeFirstResponder:window->ns.view];
    }
    
    return GLFW_TRUE;
}
*/

/*
typedef void (*__event)(id, SEL, NSEvent*);
static IMP __original_keyDown;
static IMP __original_keyUp;

void __swizzle_keyDown(id self, SEL _cmd, NSEvent *event)
{
    _glfwInputError(GLFW_API_UNAVAILABLE, "keyDown");
    if ( [self GLFWwindow] ) {
        const int key = translateKey([event keyCode]);
        const int mods = translateFlags([event modifierFlags]);
        _glfwInputKey([self GLFWwindow], key, [event keyCode], GLFW_PRESS, mods);
    }
    
    [self interpretKeyEvents:[NSArray arrayWithObject:event]];
    ((__event)__original_keyDown)(self, _cmd, event);
}

void __swizzle_keyUp(id self, SEL _cmd, NSEvent *event)
{
    _glfwInputError(GLFW_API_UNAVAILABLE, "keyUp");
    if ( [self GLFWwindow] ) {
        const int key = translateKey([event keyCode]);
        const int mods = translateFlags([event modifierFlags]);
        _glfwInputKey([self GLFWwindow], key, [event keyCode], GLFW_RELEASE, mods);
    }
    ((__event)__original_keyUp)(self, _cmd, event);
}

bool swizzleMethod(id self, SEL selOrig, IMP impNew, IMP *impOrig)
{
    Method origMethod = class_getInstanceMethod([self class], selOrig);
    if (origMethod) {
        *impOrig = method_setImplementation(origMethod, impNew);
        if (*impOrig) return true;
    }
    NSCAssert1(false, @"swizzleMethod failed for %@", NSStringFromSelector(selOrig));
    return false;
}

void swizzleInput(id self)
{
    swizzleMethod(self, @selector(keyDown:), (IMP)__swizzle_keyDown, &__original_keyDown);
    swizzleMethod(self, @selector(keyUp:), (IMP)__swizzle_keyUp, &__original_keyUp);
    _glfwInputError(GLFW_API_UNAVAILABLE, "swizzleInput %p", self);
}
 
*/

@interface NSView (GLFW3)

@property (nonatomic, assign) _GLFWwindow *GLFWwindow;
@property (nonatomic, retain) id inputTracker;
@property (nonatomic, assign) id isGLFWContentView;

@end


@interface InputTracker : NSObject {
    NSView * _view;
    _GLFWwindow* _window;
    id _monitor;;
}

@property (nonatomic, retain) NSView* view;
@property (nonatomic, assign) _GLFWwindow* window;
@property (nonatomic, assign) _GLFWwindow* GLFWwindow;

- (id)initWithGlfwWindow:(_GLFWwindow *)aWindow andView:(NSView*)aView;

@end

@implementation InputTracker

@synthesize view = _view;
@synthesize window = _window;

- (_GLFWwindow *)GLFWwindow { return _window; }
- (void)setGLFWwindow:(_GLFWwindow *)value { _window = value; }

- (id)initWithGlfwWindow:(_GLFWwindow *)aWindow andView:(NSView*)aView
{
    if ( (self = [super init]) ) {
        _view = aView;
        _window = aWindow;
        
        _monitor = [NSEvent addLocalMonitorForEventsMatchingMask:(NSEventMaskKeyDown        | NSEventMaskKeyUp |
                                                                  NSEventMaskLeftMouseDown  | NSEventMaskLeftMouseUp |
                                                                  NSEventMaskRightMouseDown | NSEventMaskRightMouseUp |
                                                                  NSEventMaskOtherMouseDown | NSEventMaskOtherMouseUp |
                                                                  NSEventMaskMouseEntered   | NSEventMaskMouseExited |
                                                                  NSEventMaskMouseMoved     | NSEventMaskScrollWheel |
                                                                  NSEventMaskFlagsChanged )
                    
                                                                   handler:^(NSEvent *theEvent) {
                                                                       if ( ![self postKeyboardState:theEvent] )
                                                                           theEvent = nil;
                                                                       return theEvent;
                                                                   }];
    }
    return self;
}

- (void)dealloc
{
    [NSEvent removeMonitor:_monitor];
    [super dealloc];
}

- (bool)postKeyboardState:(NSEvent *)theEvent
{
    NSEventType type = [theEvent type];
    switch (type)
    {
        case(NSEventTypeKeyDown): return [self keyDown:theEvent];
        case(NSEventTypeKeyUp): return [self keyUp:theEvent];
        case(NSEventTypeLeftMouseDown): return [self mouseDown:theEvent];
        case(NSEventTypeLeftMouseUp): return [self mouseUp:theEvent];
        case(NSEventTypeRightMouseDown): return [self rightMouseDown:theEvent];
        case(NSEventTypeRightMouseUp): return [self rightMouseUp:theEvent];
        case(NSEventTypeOtherMouseDown): return [self otherMouseDown:theEvent];
        case(NSEventTypeOtherMouseUp): return [self otherMouseUp:theEvent];
        case(NSEventTypeMouseEntered): return [self mouseEntered:theEvent];
        case(NSEventTypeMouseExited): return [self mouseExited:theEvent];
        case(NSEventTypeMouseMoved): return [self mouseMoved:theEvent];
        case(NSEventTypeScrollWheel): return [self scrollWheel:theEvent];
        case(NSEventTypeFlagsChanged): return [self flagsChanged:theEvent];
        default:
            NSAssert1(false, @"Unknown event %@", theEvent);
            return true;
    }
    return true;
}


- (void)inputText:(NSEvent *)event withModifiers:(int)mods
{
    NSString* characters = [event characters];
    NSUInteger i, length = [characters length];
    const int plain = !(mods & GLFW_MOD_SUPER);
    
    for (i = 0;  i < length;  i++)
    {
        const unichar codepoint = [characters characterAtIndex:i];
        if ((codepoint & 0xff00) == 0xf700)
            continue;
        
        _glfwInputChar([self GLFWwindow], codepoint, mods, plain);
    }
}

- (bool)keyDown:(NSEvent *)event
{
    _GLFW_SHUOLD_DISABLE_CAP_RETURN
    
    _GLFWwindow* window = [self GLFWwindow];
    if ( window ) {
        const int key = translateKey([event keyCode]);
        const int mods = translateFlags([event modifierFlags]);
        _glfwInputKey(window, key, [event keyCode], GLFW_PRESS, mods);
        
        // NOT NEEDED JUST BUGS OUT AND CAUSES KEYS TO DUP AND MAKE AN ANNOYING SOUND
        // This will call NSTextInputClient:insertText | GLFWContentView:insertText
        //[_view interpretKeyEvents:[NSArray arrayWithObject:event]];
        
        if ( [window->ns.view isGLFWContentView] )
            // Already processed input in NSTextInputClient:insertText
            return false;
        
        [self inputText:event withModifiers:mods];
    }
    if (__capKeyCb(window) || __capInputCb(window))
        return false;
    return true;
}

- (bool)keyUp:(NSEvent *)event
{
    _GLFW_SHUOLD_DISABLE_CAP_RETURN
    
    _GLFWwindow* window = [self GLFWwindow];
    if ( window ) {
        const int key = translateKey([event keyCode]);
        const int mods = translateFlags([event modifierFlags]);
        _glfwInputKey(window, key, [event keyCode], GLFW_RELEASE, mods);
        
        if ( [window->ns.view isGLFWContentView] )
            // No need to process input twice
            return false;
    }
    if (__capKeyCb(window) || __capInputCb(window))
        return false;
    return true;
}


- (bool)mouseDown:(NSEvent *)event
{
    _GLFW_SHUOLD_DISABLE_CAP_RETURN
    
    if ( self.GLFWwindow ) {
        _glfwInputMouseClick(self.GLFWwindow,
                             GLFW_MOUSE_BUTTON_LEFT,
                             GLFW_PRESS,
                             translateFlags([event modifierFlags]));
    }
    if (__capMouseCb(self.GLFWwindow)) {
        return false;
    }
    return true;
}

- (bool)mouseUp:(NSEvent *)event
{
    _GLFW_SHUOLD_DISABLE_CAP_RETURN
    if ( self.GLFWwindow ) {
        _glfwInputMouseClick(self.GLFWwindow,
                             GLFW_MOUSE_BUTTON_LEFT,
                             GLFW_RELEASE,
                             translateFlags([event modifierFlags]));
    }
    if (__capMouseCb(self.GLFWwindow))
        return false;
    return true;
}

- (bool)mouseMoved:(NSEvent *)event
{
    _GLFW_SHUOLD_DISABLE_CAP_RETURN
    
    _GLFWwindow *window = self.GLFWwindow;
    if (!window) {
        return true;
    }
    
    if (window->cursorMode == GLFW_CURSOR_DISABLED)
    {
        const double dx = [event deltaX] - window->ns.cursorWarpDeltaX;
        const double dy = [event deltaY] - window->ns.cursorWarpDeltaY;
        
        _glfwInputCursorPos(window,
                            window->virtualCursorPosX + dx,
                            window->virtualCursorPosY + dy);
    }
    else
    {
        const NSRect contentRect = [window->ns.view frame];
        const NSPoint pos = [event locationInWindow];
        
        _glfwInputCursorPos(window, pos.x, contentRect.size.height - pos.y);
    }
    
    window->ns.cursorWarpDeltaX = 0;
    window->ns.cursorWarpDeltaY = 0;
    
    if (__capMouseCb(self.GLFWwindow))
        return false;
    return true;
}

- (bool)rightMouseDown:(NSEvent *)event
{
    _GLFW_SHUOLD_DISABLE_CAP_RETURN
    
    if ( self.GLFWwindow ) {
        _glfwInputMouseClick(self.GLFWwindow,
                             GLFW_MOUSE_BUTTON_RIGHT,
                             GLFW_PRESS,
                             translateFlags([event modifierFlags]));
    }
    if (__capMouseCb(self.GLFWwindow))
        return false;
    return true;
}

- (bool)rightMouseUp:(NSEvent *)event
{
    _GLFW_SHUOLD_DISABLE_CAP_RETURN
    
    if ( self.GLFWwindow ) {
        _glfwInputMouseClick(self.GLFWwindow,
                             GLFW_MOUSE_BUTTON_RIGHT,
                             GLFW_RELEASE,
                             translateFlags([event modifierFlags]));
    }
    if (__capMouseCb(self.GLFWwindow))
        return false;
    return true;
}

- (bool)otherMouseDown:(NSEvent *)event
{
    _GLFW_SHUOLD_DISABLE_CAP_RETURN
    
    if ( self.GLFWwindow ) {
        _glfwInputMouseClick(self.GLFWwindow,
                             (int) [event buttonNumber],
                             GLFW_PRESS,
                             translateFlags([event modifierFlags]));
    }
    if (__capMouseCb(self.GLFWwindow))
        return false;
    return true;
}

- (bool)otherMouseUp:(NSEvent *)event
{
    _GLFW_SHUOLD_DISABLE_CAP_RETURN
    
    if ( self.GLFWwindow ) {
        _glfwInputMouseClick(self.GLFWwindow,
                             (int) [event buttonNumber],
                             GLFW_RELEASE,
                             translateFlags([event modifierFlags]));
    }
    if (__capMouseCb(self.GLFWwindow))
        return false;
    return true;
}

- (bool)mouseExited:(NSEvent *)event
{
    _GLFW_SHUOLD_DISABLE_CAP_RETURN
    
    if ( self.GLFWwindow )
        _glfwInputCursorEnter(self.GLFWwindow, GLFW_FALSE);
    if (__capMouseCb(self.GLFWwindow))
        return false;
    return true;
}

- (bool)mouseEntered:(NSEvent *)event
{
    _GLFW_SHUOLD_DISABLE_CAP_RETURN
    
    if ( self.GLFWwindow )
        _glfwInputCursorEnter(self.GLFWwindow, GLFW_TRUE);
    if (__capMouseCb(self.GLFWwindow))
        return false;
    return true;
}

- (bool)flagsChanged:(NSEvent *)event
{
    _GLFW_SHUOLD_DISABLE_CAP_RETURN
    
    _GLFWwindow *window = self.GLFWwindow;
    if (!window)
        return true;
    
    int action;
    const unsigned int modifierFlags =
    [event modifierFlags] & NSEventModifierFlagDeviceIndependentFlagsMask;
    const int key = translateKey([event keyCode]);
    const int mods = translateFlags(modifierFlags);
    const NSUInteger keyFlag = translateKeyToModifierFlag(key);
    
    if (keyFlag & modifierFlags)
    {
        if (window->keys[key] == GLFW_PRESS)
            action = GLFW_RELEASE;
        else
            action = GLFW_PRESS;
    }
    else
        action = GLFW_RELEASE;
    
    _glfwInputKey(window, key, [event keyCode], action, mods);
    
    if (__capMouseCb(self.GLFWwindow))
        return false;
    return true;
}

- (bool)scrollWheel:(NSEvent *)event
{
    _GLFW_SHUOLD_DISABLE_CAP_RETURN
    
    if ( !self.GLFWwindow )
        return true;
    
    double deltaX, deltaY;
    
    deltaX = [event scrollingDeltaX];
    deltaY = [event scrollingDeltaY];
    
    if ([event hasPreciseScrollingDeltas])
    {
        deltaX *= 0.1;
        deltaY *= 0.1;
    }
    
    if (fabs(deltaX) > 0.0 || fabs(deltaY) > 0.0)
        _glfwInputScroll(self.GLFWwindow, deltaX, deltaY);
    
    if (__capMouseCb(self.GLFWwindow))
        return false;
    return true;
}


@end


@implementation NSView (GLFW3)

/*
+ (bool) swizzleMethodForClass:(id)_self method:(SEL)selOrig withMethod:(SEL)selNew
{
    Class c = [_self class];
    Method orig = class_getInstanceMethod(c, selOrig);
    Method new = class_getInstanceMethod(c, selNew);
    if (!orig || !new) {
        NSAssert1(false, @"swizzleMethod failed for %@", NSStringFromSelector(selOrig));
        return false;
    }
    if (class_addMethod(c, selOrig, method_getImplementation(new), method_getTypeEncoding(new)))
        class_replaceMethod(c, selNew, method_getImplementation(orig), method_getTypeEncoding(orig));
    else
        method_exchangeImplementations(orig, new);
    return true;
}

+ (bool) swizzleMethodDirect:(SEL)selOrig withMethod:(IMP)impNew origMethod:(IMP*)impOrig
{
    Method origMethod = class_getInstanceMethod(self, selOrig);
    if (origMethod) {
        *impOrig = method_setImplementation(origMethod, impNew);
        if (*impOrig) return true;
    }
    NSAssert1(false, @"swizzleMethod failed for %@", NSStringFromSelector(selOrig));
    return false;
}
 
 - (void) dealloc
 {
    if ( self.inputTracker )
        [self.inputTracker dealloc];
    //[super dealloc];
 }
 */

+ (void)hook:(id)_self withWindow:(_GLFWwindow*)aWindow
{
    [_self setGLFWwindow:aWindow];
    [_self setInputTracker:[[InputTracker alloc] initWithGlfwWindow:aWindow andView:_self]];
    
    if ( [_self isKindOfClass:[GLFWContentView class]] )
        [_self setIsGLFWContentView:[_self class]];
    else [_self setIsGLFWContentView:nil];
}


- (_GLFWwindow *)GLFWwindow {
    return (_GLFWwindow *)objc_getAssociatedObject(self, @selector(GLFWwindow));
}

- (void)setGLFWwindow:(_GLFWwindow *)value {
    objc_setAssociatedObject(self, @selector(GLFWwindow), (id)value, OBJC_ASSOCIATION_ASSIGN);
}

- (id)inputTracker {
    return (id)objc_getAssociatedObject(self, @selector(inputTracker));
}

- (void)setInputTracker:(id)value {
    objc_setAssociatedObject(self, @selector(inputTracker), (id)value, OBJC_ASSOCIATION_ASSIGN);
}

- (id)isGLFWContentView {
    return (id)objc_getAssociatedObject(self, @selector(isGLFWContentView));
}

- (void)setIsGLFWContentView:(id)value {
    objc_setAssociatedObject(self, @selector(isGLFWContentView), (id)value, OBJC_ASSOCIATION_ASSIGN);
}

@end


static GLFWbool attachNativeView(_GLFWwindow* window, NSView *nsview)
{
    if (window->ns.view != nil && window->ns.view == nsview && [window->ns.view GLFWwindow])
        return GLFW_FALSE;
    
    window->ns.view = nsview;
    if (window->ns.view == nil)
    {
        _glfwInputError(GLFW_PLATFORM_ERROR,
                        "Cocoa: Unable to attach view");
        return GLFW_FALSE;
    }
    [NSView hook:nsview withWindow:window];

    
    if (window->ns.object) {
        [window->ns.object makeFirstResponder:window->ns.view];
    }
    
    return GLFW_TRUE;
}


GLFWbool _glfwAttachContextNSGL(_GLFWwindow* window,
                                const _GLFWctxconfig* ctxconfig,
                                const _GLFWfbconfig* fbconfig,
                                NSView* nsview,
                                NSOpenGLContext *openglctx)
{
    if (!attachNativeView(window, nsview))
        return false;
    
    window->context.nsgl.object = (id)openglctx;
    if (window->context.nsgl.object == nil)
    {
        _glfwInputError(GLFW_VERSION_UNAVAILABLE,
                        "NSGL: Failed to create OpenGL context");
        return GLFW_FALSE;
    }
    
    window->context.makeCurrent = makeContextCurrentNSGL;
    window->context.swapBuffers = swapBuffersNSGL;
    window->context.swapInterval = swapIntervalNSGL;
    window->context.extensionSupported = extensionSupportedNSGL;
    window->context.getProcAddress = getProcAddressNSGL;
    window->context.destroy = destroyContextNSGL;
    
    return GLFW_TRUE;
}


static GLFWbool attachNativeWindow(_GLFWwindow* window, NSWindow *hwnd)
{
     if (window->ns.object != nil && window->ns.object == hwnd)
         return GLFW_FALSE;
    
    if (window->ns.delegate != nil) {
        [window->ns.object setDelegate:nil];
        [window->ns.delegate dealloc];
        window->ns.delegate = nil;
    }
    
    window->ns.delegate = [[GLFWWindowDelegate alloc] initWithGlfwWindow:window];
    if (window->ns.delegate == nil)
    {
        _glfwInputError(GLFW_PLATFORM_ERROR,
                        "Cocoa: Failed to create window delegate");
        return GLFW_FALSE;
    }
    
    window->ns.object = (id)hwnd;
    
    if (window->ns.object == nil)
    {
        _glfwInputError(GLFW_PLATFORM_ERROR, "Cocoa: Failed to attach window");
        return GLFW_FALSE;
    }

    
#if defined(_GLFW_USE_RETINA)
    //[window->ns.view setWantsBestResolutionOpenGLSurface:YES];
#endif /*_GLFW_USE_RETINA*/
    
    [window->ns.object setAcceptsMouseMovedEvents:YES];
    //[window->ns.object makeFirstResponder:window->ns.view];
    //[window->ns.object setDelegate:window->ns.delegate];
    //[window->ns.object setContentView:window->ns.view];
    //[window->ns.object setRestorable:NO];
    
    return GLFW_TRUE;
}

int _glfwPlatformAttachWindowCtx(_GLFWwindow* window,
                                 const _GLFWwndconfig* wndconfig,
                                 const _GLFWctxconfig* ctxconfig,
                                 const _GLFWfbconfig* fbconfig,
                                 NSWindow *hwnd,
                                 NSView *view,
                                 NSOpenGLContext *openglctx)
{
    if (!initializeAppKit())
        return GLFW_FALSE;
    
    if (!attachNativeWindow(window, hwnd))
        return GLFW_FALSE;
    
    if (ctxconfig->client != GLFW_NO_API)
    {
        if (ctxconfig->source == GLFW_NATIVE_CONTEXT_API)
        {
            if (!_glfwInitNSGL())
                return GLFW_FALSE;
            if (!_glfwAttachContextNSGL(window, ctxconfig, fbconfig, view, openglctx))
                return GLFW_FALSE;
        }
        else
        {
            _glfwInputError(GLFW_API_UNAVAILABLE, "Cocoa: EGL not available");
            return GLFW_FALSE;
        }
    }
    
    if (window->monitor)
    {
        _glfwPlatformShowWindow(window);
        _glfwPlatformFocusWindow(window);
        if (!acquireMonitor(window))
            return GLFW_FALSE;
        
        centerCursor(window);
    }
    
    return GLFW_TRUE;
}

GLFWwindow* glfwCreateWindowFromCtx(void *ctx, void *view)
{
    _GLFWfbconfig fbconfig;
    _GLFWctxconfig ctxconfig;
    _GLFWwndconfig wndconfig;
    _GLFWwindow* window;
    _GLFWwindow* previous;
    
    _GLFW_REQUIRE_INIT_OR_RETURN(NULL);
    if (!ctx) return NULL;
    
    //NSWindow * nswindow = [[NSApplication sharedApplication] mainWindow];
    NSView *nsview = view;
    if (nsview == nil) return NULL;
    NSWindow* nswindow = nsview.window;
    if (nswindow == nil) return NULL;
    
    CGSize size = nswindow.frame.size;
    const char* title = [nswindow.title UTF8String];
    int width = size.width;
    int height = size.height;
    
    if (width <= 0 || height <= 0)
    {
        _glfwInputError(GLFW_INVALID_VALUE,
                        "Invalid window size %ix%i",
                        width, height);
        
        return NULL;
    }
    
    fbconfig  = _glfw.hints.framebuffer;
    ctxconfig = _glfw.hints.context;
    wndconfig = _glfw.hints.window;
    
    wndconfig.width   = width;
    wndconfig.height  = height;
    wndconfig.title   = title;
    ctxconfig.share   = NULL;//(_GLFWwindow*) share;
    
    if (ctxconfig.share)
    {
        if (ctxconfig.client == GLFW_NO_API ||
            ctxconfig.share->context.client == GLFW_NO_API)
        {
            _glfwInputError(GLFW_NO_WINDOW_CONTEXT, NULL);
            return NULL;
        }
    }
    
    if (!_glfwIsValidContextConfig(&ctxconfig))
        return NULL;
    
    window = calloc(1, sizeof(_GLFWwindow));
    window->next = _glfw.windowListHead;
    _glfw.windowListHead = window;
    
    window->videoMode.width       = width;
    window->videoMode.height      = height;
    window->videoMode.redBits     = fbconfig.redBits;
    window->videoMode.greenBits   = fbconfig.greenBits;
    window->videoMode.blueBits    = fbconfig.blueBits;
    window->videoMode.refreshRate = _glfw.hints.refreshRate;
    
    window->monitor     = NULL;//(_GLFWmonitor*) monitor;
    window->resizable   = wndconfig.resizable;
    window->decorated   = wndconfig.decorated;
    window->autoIconify = wndconfig.autoIconify;
    window->floating    = wndconfig.floating;
    window->cursorMode  = GLFW_CURSOR_NORMAL;
    
    window->minwidth    = GLFW_DONT_CARE;
    window->minheight   = GLFW_DONT_CARE;
    window->maxwidth    = GLFW_DONT_CARE;
    window->maxheight   = GLFW_DONT_CARE;
    window->numer       = GLFW_DONT_CARE;
    window->denom       = GLFW_DONT_CARE;
    
    // Save the currently current context so it can be restored later
    previous = _glfwPlatformGetCurrentContext();
    if (ctxconfig.client != GLFW_NO_API)
        glfwMakeContextCurrent(NULL);
    
    // Open the actual window and create its context
    if (!_glfwPlatformAttachWindowCtx(window, &wndconfig, &ctxconfig, &fbconfig, nswindow, nsview, ctx))
    {
        //glfwMakeContextCurrent((GLFWwindow*) previous);
        //glfwDestroyWindow((GLFWwindow*) window);
        return NULL;
    }
    
    if (ctxconfig.client != GLFW_NO_API)
    {
        window->context.makeCurrent(window);
        
        // Retrieve the actual (as opposed to requested) context attributes
        if (!_glfwRefreshContextAttribs(&ctxconfig))
        {
            glfwMakeContextCurrent((GLFWwindow*) previous);
            glfwDestroyWindow((GLFWwindow*) window);
            return NULL;
        }
        
        // Restore the previously current context (or NULL)
        glfwMakeContextCurrent((GLFWwindow*) previous);
    }
    
    if (!window->monitor)
    {
        if (wndconfig.visible)
        {
            _glfwPlatformShowWindow(window);
            if (wndconfig.focused)
                _glfwPlatformFocusWindow(window);
        }
    }
    
    return (GLFWwindow*) window;
}


void _glfwPlatformPollEventsNoRelease(void)
{
    for (;;)
    {
        NSEvent* event = [NSApp nextEventMatchingMask:NSEventMaskAny
                                            untilDate:[NSDate distantPast]
                                               inMode:NSDefaultRunLoopMode
                                              dequeue:YES];
        if (event == nil)
            break;
        
        [NSApp sendEvent:event];
    }
    
}
void runOnMainQueueWithoutDeadlocking(void (^block)(void))
{
    if ([NSThread isMainThread])
    {
        block();
    }
    else
    {
        dispatch_sync(dispatch_get_main_queue(), block);
    }
}

GLFWAPI void glfwPollEventsMainThread(void)
{
    _GLFW_REQUIRE_INIT();
    runOnMainQueueWithoutDeadlocking(^{
        _glfwPlatformPollEventsNoRelease();
    });
}

void* glfwGetNSView(_GLFWwindow* window)
{
    if (window)
        return (void *)window->ns.view;
    return nil;
}

bool glfwSetNSView(_GLFWwindow* window, void *view)
{
    if (!window) return false;
    return attachNativeView(window, view);
}

bool glfwHookNSView(_GLFWwindow* window)
{
    return glfwSetNSView(window, window->ns.view);
}

void glfwSetInputCapCallbacks(_GLFWwindow* window, GLFWinputfnc capKeyFn, GLFWinputfnc capMouseFn, GLFWinputfnc capInputFn, GLFWinputfnc capDisableFn)
{
    _capKeyCb = capKeyFn;
    _capMouseCb = capMouseFn;
    _capInputCb = capInputFn;
    _capDisableCb = capDisableFn;
}

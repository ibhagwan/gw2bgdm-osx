//
//  glfw3_osx.h
//  glfw3_osx
//
//  Created by Bhagwan on 6/22/17.
//  Copyright © 2017 Bhagwan Software. All rights reserved.
//

#ifndef _glfw3_osx_h_
#define _glfw3_osx_h_

#ifdef __OBJC__
#import <Cocoa/Cocoa.h>

//! Project version number for glfw.
FOUNDATION_EXPORT double glfwVersionNumber;

//! Project version string for glfw.
FOUNDATION_EXPORT const unsigned char glfwVersionString[];
#endif  // __OBJC__

#include <GLFW/glfw3.h>

// In this header, you should import all the public headers of your framework using statements like #import <glfw/PublicHeader.h>
#ifdef __cplusplus
extern "C" {
#endif
    
GLFWwindow* glfwCreateWindowFromCtx(void *ctx, void *view);
void* glfwGetNSView(GLFWwindow* window);
bool glfwSetNSView(GLFWwindow* window, void *view);
bool glfwHookNSView(GLFWwindow* window);
void glfwPollEventsMainThread(void);
    
    
typedef bool (* GLFWinputfnc)(GLFWwindow*);
void glfwSetInputCapCallbacks(GLFWwindow* window, GLFWinputfnc capKeyFn, GLFWinputfnc capMouseFn, GLFWinputfnc capInputFn, GLFWinputfnc capDisableFn);

#ifdef __cplusplus
}
#endif


#endif /* _glfw3_osx_h_ */

//
//  main.m
//  osx.ui
//
//  Created by Bhagwan on 6/23/17.
//  Copyright © 2017 Bhagwan Software. All rights reserved.
//

//#import <Cocoa/Cocoa.h>
#import <stdio.h>
#import <GL/gl3w.h>
#import <GLFW/glfw3.h>
#import <glfw3_osx.h>
#import "Constants.h"
#import "Defines.h"
#import "osx.common/Logging.h"
#import "osx.bund/imgui_osx.h"
#import "osx.bund/entry_osx.h"

static void error_callback(int error, const char* description)
{
    fprintf(stderr, "Error %d: %s\n", error, description);
}

int main(int argc, const char * argv[]) {
    
    // global log level
    gLoggingLevel = LoggingLevelDebug;
    
    // Setup window
    glfwSetErrorCallback(error_callback);
    if (!glfwInit())
        return -1;
    
    bgdm_init();
    
    GLFWwindow* window = glfwCreateWindow(1280, 720, "ImGui OpenGL2 example", NULL, NULL);
    glfwMakeContextCurrent(window);
    glfwHookNSView(window);
    glfwHookNSView(window);
    
    if (ImGui_Init_Win(window)){
    
        // Main loop
        while (!glfwWindowShouldClose(window))
        {
            glfwPollEvents();
            ImGui_NewFrame();
            ImGui_Render(0, true, true);
            glfwSwapBuffers(window);
        }
    }
    
    bgdm_fini();
    
    glfwDestroyWindow(window);
    ImGui_Shutdown();
    glfwTerminate();
    
    //return NSApplicationMain(argc, argv);
    return 0;
}

//
//  main.m
//  bgdm.loader
//
//  Created by Bhagwan on 6/16/17.
//  Copyright © 2017 Bhagwan Software. All rights reserved.
//
#import <AppKit/AppKit.h>
#import <Foundation/Foundation.h>

#import "Defines.h"
#import "Logging.h"
#import "Constants.h"
#import "mach_inject_bundle/mach_inject_bundle.h"
#import "task_vaccine.h"
#import "BSDProcess.h"


int main(int argc, const char * argv[])
{
    
    @autoreleasepool {
        
        NSString* bundleIdentifier1 = [BSDProcess bundleIdentifierForApplicationName:kGW2ApplicationName];
        NSString* bundleIdentifier2 = [BSDProcess bundleIdentifierForProcessName:kGW2BinaryName];
        NSLog(@"bundleIdentifier1: %@", bundleIdentifier1);
        NSLog(@"bundleIdentifier2: %@", bundleIdentifier2);
        
        NSArray *apps = [NSRunningApplication runningApplicationsWithBundleIdentifier:kGW2bundleIdent];
        if ([apps count] < 1) {
            NSLog(@"Error: unable to detect GW2 process");
            return -1;
        }
        
        //
        //  Use first found app
        //
        pid_t pid = [[apps objectAtIndex:0] processIdentifier];
        
        NSString *bundlePath = [[NSBundle mainBundle] pathForResource:kBGDMbundle ofType:@"bundle"];
        
        NSLog(@"Bundle path: %@", bundlePath);
        NSLog(@"PID: %d", pid);
        
        task_t task;
        kern_return_t kr = task_for_pid(mach_task_self(), pid, &task);
        if (kr != KERN_SUCCESS) {
            NSLog(@"Error: task_for_pid() failed with error %#x", kr);
            return kr;
        }
        
        mach_error_t err;
        err = task_vaccine(task, [bundlePath fileSystemRepresentation]);
        //err = mach_inject_bundle_pid([bundlePath fileSystemRepresentation], pid);
        if (err == err_none) {
            NSLog(@"Success: BGDM loaded successfully");
        } else {
            NSLog(@"Error: mach_inject failed with error %#x", err);
        }
    }
    return 0;
}

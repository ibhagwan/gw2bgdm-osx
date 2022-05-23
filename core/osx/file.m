//
//  file.m
//  bgdm
//
//  Created by Bhagwan on 7/5/17.
//  Copyright © 2017 Bhagwan Software. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "osx.common/Logging.h"
#import "core/file.h"


bool file_copy(char const* from, char const* to, bool overwrite)
{
    if (!from || !to) {
        CLogError(@"Source/destination arguments cannot be NULL, <from:%s> <to:%s>", from, to);
        return false;
    }
    
    NSError * error = NULL;
    NSString *fromPath = [NSString stringWithUTF8String:from];
    NSString *toPath =  [NSString stringWithUTF8String:to];
    
    BOOL isDir;
    if ( ![[NSFileManager defaultManager] fileExistsAtPath:fromPath isDirectory:&isDir] || isDir == TRUE) {
        CLogError(@"Source file '%@' does not exist or is not a file", fromPath);
        return false;
    }
    
    BOOL destinationExists = [[NSFileManager defaultManager] fileExistsAtPath:toPath isDirectory:nil];
    if (destinationExists && !overwrite) {
        
        CLogDebug(@"Destination file '%@' already exists (overwrite==false)", toPath);
        return true;
    }
    
    if (destinationExists && overwrite) {
        
        if ( ![[NSFileManager defaultManager] removeItemAtPath:toPath error:&error] ) {
            CLogError(@"Unable to overwrite destination file '%@' (overwrite==true), error: %@", toPath, [error localizedDescription]);
            return false;
        }
    }
    
    if ( ![[NSFileManager defaultManager] copyItemAtPath:fromPath toPath:toPath error:&error] ) {
        
        CLogError(@"Error while copying '%@' to '%@', error: %@", fromPath, toPath, [error localizedDescription]);
        return false;
    }

    CLogDebug(@"Succesfully copied '%@' to '%@'", fromPath, toPath);
    return true;
}


bool directory_enum(char const* dir, void(*cb)(char const *))
{
    NSFileManager *fileMgr = [NSFileManager defaultManager];
    NSString *directory = [NSString stringWithUTF8String:dir];
    NSDirectoryEnumerator *dirEnum = [fileMgr enumeratorAtPath:directory];
    for (NSString *file in dirEnum) {
        BOOL isDirectory = FALSE;
        if ( [fileMgr fileExistsAtPath:[directory stringByAppendingPathComponent:file] isDirectory:&isDirectory] && isDirectory == FALSE) {
            if (cb)
                cb( [file UTF8String] );
        }
    }
    return true;
}

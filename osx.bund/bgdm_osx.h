//
//  bgdm_osx.h
//  bgdm
//
//  Created by Bhagwan on 7/5/17.
//  Copyright © 2017 Bhagwan Software. All rights reserved.
//
#import <OpenGL/gl.h>

#if defined(__OBJC__)
#import <Foundation/Foundation.h>

@interface BGDMosx : NSObject

+ (void)MessageBox:(NSString *)message;

+ (NSBundle *) bundleForSelf;
+ (NSString *) pathForResource:(NSString *)filename;
+ (NSString *) bundleVersion;

+ (BOOL) loadPNG:(NSString*)fileName textureID:(GLuint *)texName;
+ (void)textureFromPath:(NSString *)path textureID:(GLuint *)texName;

@end
#else

#ifdef __cplusplus
extern "C" {
#endif
    
bool textureFromPath(const char* path, GLuint *texName);
    
#ifdef __cplusplus
}
#endif

#endif

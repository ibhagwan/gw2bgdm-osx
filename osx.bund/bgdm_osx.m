//
//  bgdm_osx.m
//  bgdm
//
//  Created by Bhagwan on 7/5/17.
//  Copyright © 2017 Bhagwan Software. All rights reserved.
//

#import <AppKit/AppKit.h>
#import <OpenGL/gl.h>
#import "Defines.h"
#import "Logging.h"
#import "bgdm_osx.h"

@implementation BGDMosx

+ (void)MessageBox:(NSString *)message
{
    ENSURE_UI_THREAD_1_ARG(message)
    
    NSAlert *alert = [[NSAlert alloc] init];
    alert.messageText = @"BGDM";
    alert.informativeText = message;
    [alert addButtonWithTitle:@"OK"];
    [alert runModal];
}

+ (NSBundle *) bundleForSelf
{
    return [NSBundle bundleForClass:self];
}

+ (NSString *) pathForResource:(NSString *)filename
{
    /*NSString *bundlePath = [SharedAppClient bundlePath];
     
     if (bundlePath == nil)
     bundle = [NSBundle mainBundle];
     else
     bundle = [NSBundle bundleWithPath:[SharedAppClient bundlePath]];
     
     if (bundle == nil)
     return nil;*/
    NSBundle *bundle = [self bundleForSelf];
    return [bundle pathForResource:[[filename lastPathComponent] stringByDeletingPathExtension]
                            ofType:[[filename lastPathComponent] pathExtension]];
}

+ (NSString *) bundleVersion
{
    NSBundle *bundle = [self bundleForSelf];
    NSString *version = [[bundle infoDictionary] objectForKey:@"CFBundleShortVersionString"];
    NSString *buildNum = [[bundle infoDictionary] objectForKey:(NSString*)kCFBundleVersionKey];
    return [NSString stringWithFormat:@"%@.%@", version, buildNum];
}

+ (void)textureFromPath:(NSString *)path textureID:(GLuint *)texName {
    
    CFURLRef url = CFURLCreateWithFileSystemPath(NULL, (__bridge CFStringRef)path, kCFURLPOSIXPathStyle, NO);
    
    CGImageSourceRef myImageSourceRef = CGImageSourceCreateWithURL(url, NULL);
    CGImageRef myImageRef = CGImageSourceCreateImageAtIndex (myImageSourceRef, 0, NULL);
    size_t width = CGImageGetWidth(myImageRef);
    size_t height = CGImageGetHeight(myImageRef);
    
    CGRect rect = {{0, 0}, {width, height}};
    void * myData = calloc(width * 4, height);
    CGColorSpaceRef space = CGColorSpaceCreateDeviceRGB();
    CGContextRef myBitmapContext = CGBitmapContextCreate (myData,
                                                          width, height, 8,
                                                          width*4, space,
                                                          kCGBitmapByteOrder32Host |
                                                          kCGImageAlphaPremultipliedFirst);
    CGContextDrawImage(myBitmapContext, rect, myImageRef);
    CGContextRelease(myBitmapContext);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, (GLint)width);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glGenTextures(1, texName);
    glBindTexture(GL_TEXTURE_RECTANGLE_ARB, *texName);
    glTexParameteri(GL_TEXTURE_RECTANGLE_ARB,
                    GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D,
                 0,
                 GL_RGBA8,
                 (GLsizei)width, (GLsizei)height,
                 0,
                 GL_BGRA_EXT,
                 GL_UNSIGNED_INT_8_8_8_8_REV,
                 myData);
    free(myData);
}

/*
+ (void)textureFromPath2:(NSString *)path textureID:(GLuint *)texName
{
    NSImage *theImg = [[NSImage alloc] initWithContentsOfFile:path];
    NSBitmapImageRep* bitmap = [NSBitmapImageRep alloc];
    int samplesPerPixel = 0;
    NSSize imgSize = [theImg size];
    
    [theImg lockFocus];
    [bitmap initWithFocusedViewRect:
     NSMakeRect(0.0, 0.0, imgSize.width, imgSize.height)];
    [theImg unlockFocus];
    
    // Set proper unpacking row length for bitmap.
    glPixelStorei(GL_UNPACK_ROW_LENGTH, (GLint)[bitmap pixelsWide]);
    
    // Set byte aligned unpacking (needed for 3 byte per pixel bitmaps).
    glPixelStorei (GL_UNPACK_ALIGNMENT, 1);
    
    // Generate a new texture name if one was not provided.
    if (*texName == 0)
        glGenTextures (1, texName);
    glBindTexture (GL_TEXTURE_RECTANGLE_EXT, *texName);
    
    // Non-mipmap filtering (redundant for texture_rectangle).
    glTexParameteri(GL_TEXTURE_RECTANGLE_EXT, GL_TEXTURE_MIN_FILTER,  GL_LINEAR);
    samplesPerPixel = (int)[bitmap samplesPerPixel];
    
    // Nonplanar, RGB 24 bit bitmap, or RGBA 32 bit bitmap.
    if(![bitmap isPlanar] &&
       (samplesPerPixel == 3 || samplesPerPixel == 4))
    {
        glTexImage2D(GL_TEXTURE_RECTANGLE_EXT, 0,
                     samplesPerPixel == 4 ? GL_RGBA8 : GL_RGB8,
                     (GLsizei)[bitmap pixelsWide],
                     (GLsizei)[bitmap pixelsHigh],
                     0,
                     samplesPerPixel == 4 ? GL_RGBA : GL_RGB,
                     GL_UNSIGNED_BYTE,
                     [bitmap bitmapData]);
    }
    else
    {
        // Handle other bitmap formats.
    }
    
    // Clean up.
    //[bitmap release];
}
*/

+ (BOOL) loadPNG:(NSString*)fileName textureID:(GLuint *)texName
{
    //NSImage *image = [[NSImage alloc] initWithContentsOfFile:fileName];
    //NSBitmapImageRep *bitmap = [[NSBitmapImageRep alloc] initWithData:[image TIFFRepresentation]];
    NSBitmapImageRep *bitmap = (NSBitmapImageRep *)[NSBitmapImageRep imageRepWithContentsOfFile:fileName];
    if (!bitmap)
        return false;
    
    bool hasAlpha;
    if ([bitmap hasAlpha] == YES) {
        hasAlpha = true;
    } else {
        hasAlpha = false;
    }
    
    glGenTextures(1, texName);
    glBindTexture(GL_TEXTURE_2D, *texName);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    
    glTexImage2D(GL_TEXTURE_2D, 0, hasAlpha ? 4 : 3,
                 (GLsizei)[bitmap pixelsWide],
                 (GLsizei)[bitmap pixelsHigh], 0,
                 hasAlpha ? GL_RGBA : GL_RGB,
                 GL_UNSIGNED_BYTE,
                 [bitmap bitmapData]);
    
    //[bitmap release];
    //[image release];
    return true;
}

@end

bool textureFromPath(const char* path, GLuint *texName)
{
    return [BGDMosx loadPNG:[BGDMosx pathForResource:[NSString stringWithUTF8String:path]] textureID:texName];
}

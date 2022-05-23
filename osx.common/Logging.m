//
//  Logging.m
//  bgdm
//
//  Created by Bhagwan on 6/16/17.
//  Copyright © 2017 Bhagwan Software. All rights reserved.
//
#import "Logging.h"

LoggingLevel gLoggingLevel = LoggingLevelDebug;


NSString * const kLogLevelString[] = {
    @"Trace",
    @"Debug",
    @"Info",
    @"Warn",
    @"Error",
    @"Crit"
};

NSString * const kLogLevelStringCAPS[] = {
    @"TRACE",
    @"DEBUG",
    @"INFO",
    @"WARN",
    @"ERROR",
    @"CRIT"
};


void CocoaLog(LoggingLevel level, const char *file, int line, const char *function, const char *fmt, ...)
{
    if (gLoggingLevel > level) return;
        
    va_list args;
    va_start(args, fmt);
    
#if !__has_feature(objc_arc)
    NSString * str = [[[NSString alloc] initWithFormat:[NSString stringWithUTF8String:fmt] arguments:args] autorelease];
#else
    NSString * str = [[NSString alloc] initWithFormat:[NSString stringWithUTF8String:fmt] arguments:args];
#endif
    
    NSLog( @"<%@:%d (%s)> [%@] %@", [[NSString stringWithUTF8String:file] lastPathComponent], line,  function, kLogLevelStringCAPS[level], str );
    
    va_end(args);
}

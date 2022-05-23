//
//  Logging.h
//  bgdm
//
//  Created by Bhagwan on 6/16/17.
//  Copyright © 2017 Bhagwan Software. All rights reserved.
//
#pragma once

typedef enum {
    LoggingLevelTrace = 0,
    LoggingLevelDebug,
    LoggingLevelInfo,
    LoggingLevelWarn,
    LoggingLevelError,
    LoggingLevelCrit
} LoggingLevel;

extern LoggingLevel gLoggingLevel;

#if defined(__OBJC__)
#import <Foundation/Foundation.h>

extern NSString * const kLogLevelString[];
extern NSString * const kLogLevelStringCAPS[];

#ifdef DEBUG
#define DebugLog( s, ... ) NSLog( @"<%p %@:%d (%@)> %@", self, [[NSString stringWithUTF8String:__FILE__] lastPathComponent], __LINE__,  NSStringFromSelector(_cmd), [NSString stringWithFormat:(s), ##__VA_ARGS__] )
#define DebugLog1( s, ... ) NSLog( @"<%@:%d (%s)> %@", [[NSString stringWithUTF8String:__FILE__] lastPathComponent], __LINE__,  __FUNCTION__, [NSString stringWithFormat:(s), ##__VA_ARGS__] )
#else
#define DebugLog( s, ... )
#define DebugLog1( s, ... )
#endif

#define OutputLog( s, ... ) NSLog( @"<%p %@:%d (%@)> %@", self, [[NSString stringWithUTF8String:__FILE__] lastPathComponent], __LINE__,  NSStringFromSelector(_cmd), [NSString stringWithFormat:(s), ##__VA_ARGS__] )
#define OutputLog1( s, ... ) NSLog( @"<%@:%d (%s)> %@", [[NSString stringWithUTF8String:__FILE__] lastPathComponent], __LINE__,  __FUNCTION__, [NSString stringWithFormat:(s), ##__VA_ARGS__] )


#define MakeNSErrorFor(FUNC, ERROR_CODE, FORMAT,...)	\
[NSError errorWithDomain:kDNErrorDomain \
                    code:ERROR_CODE	\
                userInfo:[NSDictionary dictionaryWithObject: \
                            [NSString stringWithFormat:@"" FORMAT,##__VA_ARGS__] \
                                forKey:NSLocalizedDescriptionKey]]; \

#define MakeNSError(ERROR_CODE, FORMAT,...) MakeNSErrorFor(__func__, ERROR_CODE, FORMAT, ##__VA_ARGS__)

#define SetNSErrorFor(FUNC, ERROR_CODE, ERROR_VAR, FORMAT,...)	\
    if (ERROR_VAR) {	\
    NSString *errStr = [NSString stringWithFormat:@"%s: " FORMAT,FUNC,##__VA_ARGS__]; \
    *ERROR_VAR = [NSError errorWithDomain:kDNErrorDomain \
                                     code:ERROR_CODE	\
                                 userInfo:[NSDictionary dictionaryWithObject:errStr forKey:NSLocalizedDescriptionKey]]; \
} \

#define SetNSError(ERROR_VAR, FORMAT,...) SetNSErrorFor(__func__, -1, ERROR_VAR, FORMAT, ##__VA_ARGS__)
#define SetNSError2(ERROR_VAR, ERROR_CODE, FORMAT,...) SetNSErrorFor(__func__, ERROR_CODE, ERROR_VAR, FORMAT, ##__VA_ARGS__)

#define __TRACE__        if (gLoggingLevel <= LoggingLevelTrace) { OutputLog(@"[%@]", kLogLevelStringCAPS[LoggingLevelTrace]); }
#define LogTrace( ... )  if (gLoggingLevel <= LoggingLevelTrace) OutputLog(@"[%@] %@", kLogLevelStringCAPS[LoggingLevelTrace], [NSString stringWithFormat:__VA_ARGS__])
#define LogDebug( ... )  if (gLoggingLevel <= LoggingLevelDebug) OutputLog(@"[%@] %@", kLogLevelStringCAPS[LoggingLevelDebug], [NSString stringWithFormat:__VA_ARGS__])
#define LogInfo( ... )   if (gLoggingLevel <= LoggingLevelInfo)  OutputLog(@"[%@] %@", kLogLevelStringCAPS[LoggingLevelInfo], [NSString stringWithFormat:__VA_ARGS__])
#define LogWarn( ... )   if (gLoggingLevel <= LoggingLevelWarn)  OutputLog(@"[%@] %@", kLogLevelStringCAPS[LoggingLevelWarn], [NSString stringWithFormat:__VA_ARGS__])
#define LogError( ... )  if (gLoggingLevel <= LoggingLevelError) OutputLog(@"[%@] %@", kLogLevelStringCAPS[LoggingLevelError], [NSString stringWithFormat:__VA_ARGS__])
#define LogCrit( ... )   if (gLoggingLevel <= LoggingLevelCrit)  OutputLog(@"[%@] %@", kLogLevelStringCAPS[LoggingLevelCrit], [NSString stringWithFormat:__VA_ARGS__])
#define Log( i, ... )    if (gLoggingLevel <= i) OutputLog(@"[%@] %@", kLogLevelStringCAPS[i], [NSString stringWithFormat:__VA_ARGS__])

#define __CTRACE__        if (gLoggingLevel <= LoggingLevelTrace) { OutputLog1(@"[%@]", kLogLevelStringCAPS[LoggingLevelTrace]); }
#define CLogTrace( ... )  if (gLoggingLevel <= LoggingLevelTrace) OutputLog1(@"[%@] %@", kLogLevelStringCAPS[LoggingLevelTrace], [NSString stringWithFormat:__VA_ARGS__])
#define CLogDebug( ... )  if (gLoggingLevel <= LoggingLevelDebug) OutputLog1(@"[%@] %@", kLogLevelStringCAPS[LoggingLevelDebug], [NSString stringWithFormat:__VA_ARGS__])
#define CLogInfo( ... )   if (gLoggingLevel <= LoggingLevelInfo)  OutputLog1(@"[%@] %@", kLogLevelStringCAPS[LoggingLevelInfo], [NSString stringWithFormat:__VA_ARGS__])
#define CLogWarn( ... )   if (gLoggingLevel <= LoggingLevelWarn)  OutputLog1(@"[%@] %@", kLogLevelStringCAPS[LoggingLevelWarn], [NSString stringWithFormat:__VA_ARGS__])
#define CLogError( ... )  if (gLoggingLevel <= LoggingLevelError) OutputLog1(@"[%@] %@", kLogLevelStringCAPS[LoggingLevelError], [NSString stringWithFormat:__VA_ARGS__])
#define CLogCrit( ... )   if (gLoggingLevel <= LoggingLevelCrit)  OutputLog1(@"[%@] %@", kLogLevelStringCAPS[LoggingLevelCrit], [NSString stringWithFormat:__VA_ARGS__])
#define CLog( i, ... )    if (gLoggingLevel <= i) OutputLog1(@"[%@] %@", kLogLevelStringCAPS[i], [NSString stringWithFormat:__VA_ARGS__])
#else

#ifdef __cplusplus
extern "C" {
#endif

void CocoaLog(LoggingLevel level, const char *fmt, ...);
    
#ifdef __cplusplus
}
#endif

#define __TRACE__        CocoaLog(LoggingLevelTrace, __FILE__, __LINE__, __FUNCTION__, "");
#define LogTrace( ... )  CocoaLog(LoggingLevelTrace, __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)
#define LogDebug( ... )  CocoaLog(LoggingLevelDebug, __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)
#define LogInfo( ... )   CocoaLog(LoggingLevelInfo, __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)
#define LogWarn( ... )   CocoaLog(LoggingLevelWarn, __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)
#define LogError( ... )  CocoaLog(LoggingLevelError, __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)
#define LogCrit( ... )   CocoaLog(LoggingLevelCrit, __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)
#define Log( i, ... )    CocoaLog(i, __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)

#define __CTRACE__      __TRACE__
#define CLogTrace       LogTrace
#define CLogDebug       LogDebug
#define CLogInfo        LogInfo
#define CLogWarn        LogWarn
#define CLogError       LogError
#define CLogCrit        LogCrit
#define CLog            Log

#undef LOG_DEBUG
#undef LOG_INFO
#undef LOG_WARN
#undef LOG_ERR
#undef LOG_CRIT
#undef LOG

#define LOG_DEBUG(...) LogDebug(__VA_ARGS__)
#define LOG_INFO(...) LogInfo(__VA_ARGS__)
#define LOG_WARN(...) LogWarn(__VA_ARGS__)
#define LOG_ERR(...) LogError(__VA_ARGS__)
#define LOG_CRIT(...) LogCrit(__VA_ARGS__)
#define LOG(i, ...) Log(__VA_ARGS__)

#endif



#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __APPLE__
    
#include "osx.common/Logging.h"
    
void logger_create( const char *filename );
    
#define LOG_DEBUG(...) LogDebug(__VA_ARGS__)
#define LOG_INFO(...) LogInfo(__VA_ARGS__)
#define LOG_WARN(...) LogWarn(__VA_ARGS__)
#define LOG_ERR(...) LogError(__VA_ARGS__)
#define LOG_CRIT(...) LogCrit(__VA_ARGS__)
#define LOG(i, ...) Log(__VA_ARGS__)
    
#else
    
#include "core/logging.h"

extern logger_t s_logger;

#define LOG_DEBUG(...) logger_log(&s_logger, LOG_DEBUG, __VA_ARGS__)
#define LOG_INFO(...) logger_info(&s_logger, __VA_ARGS__)
#define LOG_WARN(...) logger_warning(&s_logger, __VA_ARGS__)
#define LOG_ERR(...) logger_error(&s_logger, __VA_ARGS__)
#define LOG_CRIT(...) logger_critical(&s_logger, __VA_ARGS__)
#define LOG(i, ...) logger_log(&s_logger, i, __VA_ARGS__)
    
#endif

#ifdef __cplusplus
}
#endif

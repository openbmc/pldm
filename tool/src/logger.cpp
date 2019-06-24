#include "inc/logger.hpp"

Logger* Logger::instance_ = 0;

int Logger::Log(LogLevel level, const char* fmt, va_list args)
{
    if (!instance_)
        return 0;
    return instance_->log_msg(level, fmt, args);
}

void Logger::Log(LogLevel level, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    Log(level, fmt, args);
    va_end(args);
}

void Logger::Debug(const char* fmt, ...)
{
    static char log_buf[LOG_MSG_LENGTH];
    va_list args;
    char log_type[10] = {'\0'};
    strcat(log_type, "DEBUG: ");
    va_start(args, fmt);
    std::vsnprintf(log_buf, LOG_MSG_LENGTH, fmt, args);
    va_end(args);
    fprintf(stdout, "%s %s\n", log_type, log_buf);
}

void Logger::Info(const char* fmt, ...)
{
    static char log_buf[LOG_MSG_LENGTH];
    va_list args;
    char log_type[10] = {'\0'};
    strcat(log_type, "INFO: ");
    va_start(args, fmt);
    std::vsnprintf(log_buf, LOG_MSG_LENGTH, fmt, args);
    va_end(args);
    fprintf(stdout, "%s %s\n", log_type, log_buf);
}

void Logger::Warning(const char* fmt, ...)
{
    static char log_buf[LOG_MSG_LENGTH];
    va_list args;
    char log_type[10] = {'\0'};
    strcat(log_type, "WARNING: ");
    va_start(args, fmt);
    std::vsnprintf(log_buf, LOG_MSG_LENGTH, fmt, args);
    va_end(args);
    fprintf(stdout, "%s %s\n", log_type, log_buf);
}

void Logger::Error(const char* fmt, ...)
{
    static char log_buf[LOG_MSG_LENGTH];
    va_list args;
    char log_type[10] = {'\0'};
    strcat(log_type, "ERROR: ");
    va_start(args, fmt);
    std::vsnprintf(log_buf, LOG_MSG_LENGTH, fmt, args);
    va_end(args);
    fprintf(stdout, "%s %s\n", log_type, log_buf);
}

#ifndef PLDM_LOGGER_H_
#define PLDM_LOGGER_H_

#include <cstdarg>
#include <cstdio>
#include <string.h>

#define LOG_MSG_LENGTH 1024

class Logger {
  public:
    Logger() {}
    virtual ~Logger() {}
    enum LogLevel {
        DEBUG_LVL,    // Messages used only for debugging
        INFO_LVL,     // Informational messages; something is unusual but not wrong
        WARNING_LVL,  // There's an indication of trouble, but it may be okay.
        ERROR_LVL,    // A problem has occurred, but processing can continue
    };
    virtual int log_msg(LogLevel level, const char* fmt, va_list args) const = 0;

    /** @brief print the message with selected arguments
     *
     *  @param[in] level - The type of information
     *  @param[in] format - The information
     *  @param[in] arguments - Arguments to print
     *
     *  @return - None
     */
    static int  Log(LogLevel level, const char* fmt, va_list args);

    /** @brief print the message with selected level
     *
     *  @param[in] level -  Log level
     *  @param[in] format - The information
     *
     *  @return - None
     */
    static void Log(LogLevel level, const char* fmt, ...);

    /** @brief print the debug log
     *
     *  @param[in] format - The information
     *
     *  @return - None
     */
    static void Debug(const char* fmt, ...);

    /** @brief print the info log
     *
     *  @param[in] format - The information
     *
     *  @return - None
     */
    static void Info(const char* fmt, ...);

    /** @brief print the warning log
     *
     *  @param[in] format - The information
     *
     *  @return - None
     */
    static void Warning(const char* fmt, ...);

    /** @brief print the error log
     *
     *  @param[in] format - The information
     *
     *  @return - None
     */
    static void Error(const char* fmt, ...);
  protected:
    static void set_instance(Logger* logger) { instance_ = logger; }
  private:
    // Disallow copying.
    Logger(const Logger&);
    void operator=(const Logger&);
    static Logger* instance_;
};

#define STR(x) #x
#define STRINGIFY(x) STR(x)
#define FILE_LINE __FILE__ ", Line " STRINGIFY(__LINE__) ": "
#define LOG_D(fmt, ...) Logger::Debug(FILE_LINE fmt, __VA_ARGS__)
#define LOG_W(fmt, ...) Logger::Warning(FILE_LINE fmt, __VA_ARGS__)
#define LOG_E(fmt, ...) Logger::Error(FILE_LINE fmt, __VA_ARGS__)
#define LOG_I(fmt, ...) Logger::Info(FILE_LINE fmt, __VA_ARGS__)
#endif  // PLDM_LOGGER_H_

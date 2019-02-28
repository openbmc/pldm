
#ifndef PL_LOG_H
#define PL_LOG_H

#include <syslog.h>

/* sys/syslog.h:
 * LOG_EMERG       0       system is unusable
 * LOG_ALERT       1       action must be taken immediately
 * LOG_CRIT        2       critical conditions
 * LOG_ERR         3       error conditions
 * LOG_WARNING     4       warning conditions
 * LOG_NOTICE      5       normal but significant condition
 * LOG_INFO        6       informational
 * LOG_DEBUG       7       debug-level messages
 */

/* Type of logging to be used */
#define NOLOG 0
#define SYSLOG 1
#define CONSOLELOG 2
#define JOURNALLOG 3
#define FILELOG 4

/* wrapper for full or short usage */
#define LOG_ERROR LOG_ERR
#define LOG_WARN LOG_WARNING

/* tool name to be used in display/logging */
#define LOG_NAME_DEFAULT "pldmtool"
/* message length for single line */
#define LOG_MSG_LENGTH 1024
/* file name used for logging */
#define LOG_FILE_NAME "pldmtool_log.txt"

/** @struct logpriv_s
 *
 * Structure for the log setup
 */
struct logpriv_s
{
    char* name;
    int type;
    int level;
};

/** @brief re-initialize the logger
 *
 *  @param - None
 *
 *  @return - None
 */
void log_reinit(void);

/** @brief print the non error information
 *
 *  @param[in] level - The type of information
 *  @param[in] format - The information
 *
 *  @return - None
 */
void lprintf(int level, const char* format, ...);

/** @brief print the error information
 *
 *  @param[in] level - The level of information
 *  @param[in] format - The information
 *
 *  @return - None
 */
void lperror(int level, const char* format, ...);

/** @brief print the error information
 *
 *  @param[in] name - Name used by logger
 *  @param[in] loggingtype - The type of logging
 *  @param[in] verbose - The level of logging
 *
 *  @return - None
 */
void log_init(const char* name, int loggingtype, int verbose);

/** @brief Halt the logger
 *
 *  @param - None
 *
 *  @return - None
 */
void log_halt(void);

/** @brief Get the level of logging
 *
 *  @param - None
 *
 *  @return - The level that is used
 */
int log_level_get(void);

/** @brief Set the level of logging
 *
 *  @param[in] level - The level to be set
 *
 *  @return - None
 */
void log_level_set(int level);

#endif /*PL_LOG_H*/

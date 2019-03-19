
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <systemd/sd-journal.h>
#include <unistd.h>

#include <pt_log.hpp>

/* global variables used  for logging */
struct logpriv_s* logpriv;
FILE* fhndl;

/*
 * re-initialize the logger
 */
void log_reinit(void)
{
    log_init(NULL, 0, 0);
}

/*
 * print the non error information
 */
void lprintf(int level, const char* format, ...)
{
    static char logmsg[LOG_MSG_LENGTH];
    va_list vptr;
    char logtype[9] = {'\0'};

    if (!logpriv)
        log_reinit();

    if (logpriv->level < level)
        return;

    va_start(vptr, format);
    vsnprintf(logmsg, LOG_MSG_LENGTH, format, vptr);
    va_end(vptr);

    if (level == LOG_INFO)
    {
        strcat(logtype, "INFO: ");
    }
    else if (level == LOG_DEBUG)
    {
        strcat(logtype, "DEBUG ::");
    }
    else
    {
        strcat(logtype, "");
    }

    if (logpriv->type == SYSLOG)
        syslog(level, "%s %s", logtype, logmsg);
    else if (logpriv->type == CONSOLELOG)
        fprintf(stdout, "%s %s\n", logtype, logmsg);
    else if (logpriv->type == FILELOG)
        fprintf(fhndl, "%s %s: %s\n", logtype, logmsg, strerror(errno));
    else if (logpriv->type == JOURNALLOG)
        ; // sd_journal_print(LOG_INFO, logmsg);
    return;
}

/*
 * print the error information
 */
void lperror(int level, const char* format, ...)
{
    static char logmsg[LOG_MSG_LENGTH];
    va_list vptr;

    if (!logpriv)
        log_reinit();

    if (logpriv->level < level)
        return;

    if (logpriv->type == SYSLOG)
        return;

    va_start(vptr, format);
    vsnprintf(logmsg, LOG_MSG_LENGTH, format, vptr);
    va_end(vptr);

    if (logpriv->type == SYSLOG)
        syslog(level, "ERR :: %s: %s", logmsg, strerror(errno));
    else if (logpriv->type == CONSOLELOG)
        fprintf(stderr, "ERR :: %s: %s\n", logmsg, strerror(errno));
    else if (logpriv->type == FILELOG)
        fprintf(fhndl, "ERR :: %s: %s\n", logmsg, strerror(errno));
    else if (logpriv->type == JOURNALLOG)
        ; // sd_journal_print(LOG_INFO, logmsg);

    return;
}

/*
 * initialize the logger method and details
 */
void log_init(const char* name, int loggingtype, int verbose)
{
    if (logpriv)
        return;

    logpriv = (struct logpriv_s*)malloc(sizeof(struct logpriv_s));
    if (!logpriv)
        return;

    if (name != NULL)
        logpriv->name = strdup(name);
    else
        logpriv->name = strdup(LOG_NAME_DEFAULT);

    if (logpriv->name == NULL)
        fprintf(stderr, "pldmtool: malloc failure\n");

    logpriv->type = loggingtype;
    logpriv->level = verbose + LOG_INFO;

    if (logpriv->type == SYSLOG)
        openlog(logpriv->name, LOG_CONS, LOG_LOCAL0);
    else if (logpriv->type == FILELOG)
    {
        fhndl = fopen(LOG_FILE_NAME, "w+");
    }
}

/*
 * Close the logger
 */
void log_halt(void)
{
    if (!logpriv)
        return;

    if (logpriv->name)
    {
        free(logpriv->name);
        logpriv->name = NULL;
    }

    if (logpriv->type == SYSLOG)
        closelog();
    else if (logpriv->type == FILELOG)
        fclose(fhndl);

    free(logpriv);
    logpriv = NULL;
}

/*
 * Get the level
 */
int log_level_get(void)
{
    return logpriv->level;
}

/*
 * Set the level
 */
void log_level_set(int level)
{
    logpriv->level = level;
}

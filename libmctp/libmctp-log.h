/* SPDX-License-Identifier: Apache-2.0 */

#ifndef _LIBMCTP_LOG_H
#define _LIBMCTP_LOG_H

/* libmctp-internal logging */
#ifndef pr_fmt
#define pr_fmt
#endif

#if defined(MCTP_LOG_STDERR)

#include <stdio.h>

#define MCTP_LOG_ERR 0
#define MCTP_LOG_WARNING 0
#define MCTP_LOG_NOTICE 0
#define MCTP_LOG_INFO 0
#define MCTP_LOG_DEBUG 0

#define mctp_prlog(x, fmt, ...) fprintf(stderr, fmt "\n", ##__VA_ARGS__)

#elif defined(MCTP_LOG_SYSLOG)

#include <syslog.h>

#define MCTP_LOG_ERR LOG_ERR
#define MCTP_LOG_WARNING LOG_WARNING
#define MCTP_LOG_NOTICE LOG_NOTICE
#define MCTP_LOG_INFO LOG_INFO
#define MCTP_LOG_DEBUG LOG_DEBUG

#define mctp_prlog(x, fmt, ...) syslog(x, fmt, ##__VA_ARGS__)

#elif defined(MCTP_LOG_CUSTOM)

#include <config.h>

#if !defined(mctp_prlog)
#error Custom logging implementation enabled, but no definition for mctp_prlog
#endif

#else
#error No log implementation found
#endif

#define mctp_prerr(fmt, ...) mctp_prlog(MCTP_LOG_ERR, fmt, ##__VA_ARGS__)
#define mctp_prwarn(fmt, ...) mctp_prlog(MCTP_LOG_WARNING, fmt, ##__VA_ARGS__)
#define mctp_prinfo(fmt, ...) mctp_prlog(MCTP_LOG_INFO, fmt, ##__VA_ARGS__)
#define mctp_prdebug(fmt, ...) mctp_prlog(MCTP_LOG_DEBUG, fmt, ##__VA_ARGS__)

#endif /* _LIBMCTP_LOG_H */

/* ***** BEGIN LICENSE BLOCK *****
* Version: MPL 1.1/GPL 2.0/LGPL 2.1
*
* The contents of this file are subject to the Mozilla Public License Version
* 1.1 (the "License"); you may not use this file except in compliance with
* the License. You may obtain a copy of the License at
* http://www.mozilla.org/MPL/
*
* Software distributed under the License is distributed on an "AS IS" basis,
* WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
* for the specific language governing rights and limitations under the
* License.
*
* The Original Code is the Netscape Portable Runtime (NSPR).
*
* The Initial Developer of the Original Code is
* Netscape Communications Corporation.
* Portions created by the Initial Developer are Copyright (C) 1998-2000
* the Initial Developer. All Rights Reserved.
*
* Contributor(s):
*
* Alternatively, the contents of this file may be used under the terms of
* either the GNU General Public License Version 2 or later (the "GPL"), or
* the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
* in which case the provisions of the GPL or the LGPL are applicable instead
* of those above. If you wish to allow use of your version of this file only
* under the terms of either the GPL or the LGPL, and not to allow others to
* use your version of this file under the terms of the MPL, indicate your
* decision by deleting the provisions above and replace them with the notice
* and other provisions required by the GPL or the LGPL. If you do not delete
* the provisions above, a recipient may use your version of this file under
* the terms of any one of the MPL, the GPL or the LGPL.
*
* ***** END LICENSE BLOCK ***** */

#ifndef __LOG_H__
#define __LOG_H__

#include <stdint.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Session logger interface. Callers retain the historical
 * LOG(module, level, (format, ...)) syntax, while configuration and the
 * destination are selected explicitly by the command line/configuration
 * layer. Messages emitted before the final path is known are buffered. */

typedef enum log_module_level_t
{
    LOG_NONE    = 0,
    LOG_ERROR   = 1,
    LOG_WARNING = 2,
    LOG_NOTICE  = 3,
    LOG_INFO    = 4,
    LOG_DEBUG   = 5,
    LOG_ALWAYS  = 6,
    LOG_WARN    = LOG_WARNING,
    LOG_MIN     = LOG_ERROR,
    LOG_MAX     = LOG_DEBUG
} log_module_level_t;

/*
** One of these structures is created for each module that uses logging.
**    "name" is the name of the module
**    "level" is the debugging level selected for that module
*/
typedef struct log_module_info_t
{
    const char             *name;
    log_module_level_t       level;
    struct log_module_info_t *next;
} log_module_info_t;

/*
 ** initialize logging
 */
void log_init(void);

/*
 ** destroy logging
 */
void log_destroy(void);

/*
 ** Create a new log module.
 */
log_module_info_t* create_log_module(const char *name);

/*
** Set the session log path. Returns zero if the file cannot be created.
*/
int set_log_file(const char *name);

/*
** Set the size of the logging buffer. If "buffer_size" is zero then the
** logging becomes "synchronous" (or unbuffered).
*/
void set_log_buffering(int buffer_size);

/*
** Print a record with the current thread-local module and severity.
*/
void log_print(const char *fmt, ...);

/* Set the module and severity for the immediately following log_print().
 * The context is thread-local so the legacy LOG(module, level, (args)) call
 * form remains safe when decoder and output threads log concurrently. */
void log_set_context(log_module_info_t *module, log_module_level_t level);

/* Configure the process logger. Messages are buffered until set_log_file()
 * selects the final session path. */
void log_configure(int enabled, log_module_level_t level);
int log_is_enabled(void);
const char *log_level_name(log_module_level_t level);
typedef int (*log_time_provider_t)(struct timespec *now);
void log_set_time_provider(log_time_provider_t provider);

/*
** Flush the log to its file.
*/
void log_flush(void);

void log_assert(const char *s, const char *file, int ln);

#define DEBUG 1

#if defined(DEBUG) || defined(FORCE_LOG)
#define LOGGING    1

#define LOG_TEST(_module, _level) \
    ((_module) != NULL && ((_level) == LOG_ALWAYS || (_module)->level >= (_level)))

/*
** Log something.
**    "module" is the address of a PRLogModuleInfo structure
**    "level" is the desired logging level
**    "args" is a variable length list of arguments to print, in the following
**       format:  ("printf style format string", ...)
*/
#define LOG(_module, _level, _args)       \
    do {                                  \
        if (LOG_TEST(_module, _level)) {  \
            log_set_context((_module), (_level)); \
            log_print _args;              \
        }                                 \
    } while (0)

#else /* defined(DEBUG) || defined(FORCE_LOG) */

#undef LOGGING
#define LOG_TEST(module, level)    0
#define LOG(module, level, args)

#endif /* defined(DEBUG) || defined(FORCE_LOG) */

#if defined(DEBUG) || defined(FORCE_ASSERT)

#define ASSERT(_expr) \
    ((_expr) ? ((void) 0) : log_assert(# _expr, __FILE__, __LINE__))

#define NOT_REACHED(_reasonStr) \
    log_assert(_reasonStr, __FILE__, __LINE__)

#else

#define ASSERT(expr)    ((void) 0)
#define NOT_REACHED(reasonStr)

#endif /* defined(DEBUG) || defined(FORCE_ASSERT) */

#ifdef __cplusplus
};
#endif

#endif /* __LOG_H__ */

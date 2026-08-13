/* Structured logger used by the host-native extractor. */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <pthread.h>
#if defined(_WIN32)
#include <windows.h>
#endif

#include "log.h"

typedef struct pending_log_line_s
{
    char *text;
    struct pending_log_line_s *next;
} pending_log_line_t;

static log_module_info_t *log_modules;
static FILE *log_file;
static int logger_enabled;
static log_module_level_t logger_level = LOG_INFO;
static pending_log_line_t *pending_head;
static pending_log_line_t *pending_tail;
static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;
static _Thread_local log_module_info_t *context_module;
static _Thread_local log_module_level_t context_level;
static log_time_provider_t time_provider;

static int system_time_provider(struct timespec *now)
{
#if defined(_WIN32)
    FILETIME file_time;
    ULARGE_INTEGER ticks;
    const unsigned long long windows_epoch = 116444736000000000ULL;

    GetSystemTimeAsFileTime(&file_time);
    ticks.LowPart = file_time.dwLowDateTime;
    ticks.HighPart = file_time.dwHighDateTime;
    if (ticks.QuadPart < windows_epoch)
        return -1;
    ticks.QuadPart -= windows_epoch;
    now->tv_sec = (time_t)(ticks.QuadPart / 10000000ULL);
    now->tv_nsec = (long)((ticks.QuadPart % 10000000ULL) * 100ULL);
    return 0;
#else
    return timespec_get(now, TIME_UTC) == TIME_UTC ? 0 : -1;
#endif
}

void log_set_time_provider(log_time_provider_t provider)
{
    time_provider = provider;
}

static void lock_log(void)
{
    (void)pthread_mutex_lock(&log_mutex);
}

static void unlock_log(void)
{
    (void)pthread_mutex_unlock(&log_mutex);
}

const char *log_level_name(log_module_level_t level)
{
    switch (level)
    {
    case LOG_ERROR: return "ERROR";
    case LOG_WARNING: return "WARNING";
    case LOG_NOTICE: return "NOTICE";
    case LOG_INFO: return "INFO";
    case LOG_DEBUG: return "DEBUG";
    case LOG_ALWAYS: return "ALWAYS";
    default: return "NONE";
    }
}

void log_configure(int enabled, log_module_level_t level)
{
    logger_enabled = enabled != 0;
    logger_level = level;
    for (log_module_info_t *module = log_modules; module; module = module->next)
        module->level = logger_enabled ? logger_level : LOG_NONE;
}

int log_is_enabled(void)
{
    return logger_enabled;
}

void log_init(void)
{
    log_configure(logger_enabled, logger_level);
}

log_module_info_t *create_log_module(const char *name)
{
    log_module_info_t *module = calloc(1, sizeof(*module));
    if (!module)
        return NULL;
    module->name = strdup(name ? name : "unknown");
    if (!module->name)
    {
        free(module);
        return NULL;
    }
    module->level = logger_enabled ? logger_level : LOG_NONE;
    module->next = log_modules;
    log_modules = module;
    return module;
}

void log_set_context(log_module_info_t *module, log_module_level_t level)
{
    context_module = module;
    context_level = level;
}

static void make_timestamp(char *buffer, size_t size)
{
    struct timespec now;
    struct tm local_tm;
    char date[40];
    char zone[16];

    if ((time_provider ? time_provider(&now) : system_time_provider(&now)) != 0)
        memset(&now, 0, sizeof(now));
#if defined(_WIN32)
    localtime_s(&local_tm, &now.tv_sec);
#else
    localtime_r(&now.tv_sec, &local_tm);
#endif
    strftime(date, sizeof(date), "%Y-%m-%dT%H:%M:%S", &local_tm);
    strftime(zone, sizeof(zone), "%z", &local_tm);
    if (strlen(zone) == 5)
        snprintf(buffer, size, "%s.%03ld%.3s:%s", date, now.tv_nsec / 1000000L,
                 zone, zone + 3);
    else
        snprintf(buffer, size, "%s.%03ldZ", date, now.tv_nsec / 1000000L);
}

static void emit_or_buffer(char *line)
{
    if (log_file)
    {
        fputs(line, log_file);
        fflush(log_file);
        free(line);
        return;
    }

    pending_log_line_t *pending = calloc(1, sizeof(*pending));
    if (!pending)
    {
        free(line);
        return;
    }
    pending->text = line;
    if (pending_tail)
        pending_tail->next = pending;
    else
        pending_head = pending;
    pending_tail = pending;
}

void log_print(const char *fmt, ...)
{
    char message[2048];
    char timestamp[80];
    char *line;
    const char *module_name;
    va_list args;
    int message_length;
    int line_length;

    if (!logger_enabled || !fmt)
        return;

    va_start(args, fmt);
    message_length = vsnprintf(message, sizeof(message), fmt, args);
    va_end(args);
    if (message_length < 0)
        return;

    make_timestamp(timestamp, sizeof(timestamp));
    module_name = context_module && context_module->name ? context_module->name : "unknown";
    line_length = snprintf(NULL, 0, "%s %-7s %-8s %s%s",
                           timestamp, log_level_name(context_level), module_name,
                           message, message_length > 0 && message[message_length < (int)sizeof(message) ? message_length - 1 : (int)sizeof(message) - 2] == '\n' ? "" : "\n");
    if (line_length < 0)
        return;
    line = malloc((size_t)line_length + 1);
    if (!line)
        return;
    snprintf(line, (size_t)line_length + 1, "%s %-7s %-8s %s%s",
             timestamp, log_level_name(context_level), module_name,
             message, message_length > 0 && message[message_length < (int)sizeof(message) ? message_length - 1 : (int)sizeof(message) - 2] == '\n' ? "" : "\n");

    lock_log();
    emit_or_buffer(line);
    unlock_log();
}

int set_log_file(const char *name)
{
    FILE *file;
    pending_log_line_t *pending;

    if (!logger_enabled)
        return 0;
    file = fopen(name, "a");
    if (!file)
        return -1;

    lock_log();
    log_file = file;
    pending = pending_head;
    while (pending)
    {
        pending_log_line_t *next = pending->next;
        fputs(pending->text, log_file);
        free(pending->text);
        free(pending);
        pending = next;
    }
    pending_head = pending_tail = NULL;
    fflush(log_file);
    unlock_log();
    return 0;
}

void set_log_buffering(int buffer_size)
{
    (void)buffer_size;
}

void log_flush(void)
{
    lock_log();
    if (log_file)
        fflush(log_file);
    unlock_log();
}

void log_destroy(void)
{
    pending_log_line_t *pending;
    log_module_info_t *module;

    lock_log();
    if (log_file)
        fclose(log_file);
    log_file = NULL;
    pending = pending_head;
    while (pending)
    {
        pending_log_line_t *next = pending->next;
        free(pending->text);
        free(pending);
        pending = next;
    }
    pending_head = pending_tail = NULL;
    module = log_modules;
    while (module)
    {
        log_module_info_t *next = module->next;
        free((char *)module->name);
        free(module);
        module = next;
    }
    log_modules = NULL;
    time_provider = NULL;
    unlock_log();
}

void log_assert(const char *expression, const char *file, int line)
{
    log_set_context(context_module, LOG_ERROR);
    log_print("assertion failed: %s (%s:%d)", expression, file, line);
    abort();
}

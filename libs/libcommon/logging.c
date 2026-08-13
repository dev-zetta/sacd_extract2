/**
 * SACD Extract structured session logging.
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include "logging.h"

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#if defined(_WIN32)
#define LOG_PATH_SEPARATOR '\\'
#else
#define LOG_PATH_SEPARATOR '/'
#endif

log_module_info_t *lm_main = NULL;
log_module_info_t *lm_input = NULL;
log_module_info_t *lm_toc = NULL;
log_module_info_t *lm_output = NULL;
log_module_info_t *lm_decoder = NULL;
log_module_info_t *lm_metadata = NULL;

static char *requested_log_path;
static char active_log_path[PATH_MAX];

static int path_exists(const char *path)
{
    struct stat st;
    return path && stat(path, &st) == 0;
}

static int make_session_path(const char *directory, char *path, size_t path_size)
{
    time_t now = time(NULL);
    struct tm local_tm;
    char stamp[32];
    unsigned int collision = 0;

#if defined(_WIN32)
    localtime_s(&local_tm, &now);
#else
    localtime_r(&now, &local_tm);
#endif
    if (strftime(stamp, sizeof(stamp), "%Y%m%d-%H%M%S", &local_tm) == 0)
        return -1;

    do
    {
        int written;
        if (collision == 0)
            written = snprintf(path, path_size, "%s%csacd_extract-%s.log",
                               directory, LOG_PATH_SEPARATOR, stamp);
        else
            written = snprintf(path, path_size, "%s%csacd_extract-%s-%u.log",
                               directory, LOG_PATH_SEPARATOR, stamp, collision);
        if (written < 0 || (size_t)written >= path_size)
            return -1;
        collision++;
    }
    while (path_exists(path));

    return 0;
}

log_module_level_t logging_parse_level(const char *name, int *valid)
{
    if (valid)
        *valid = 1;
    if (!name || strcasecmp(name, "info") == 0)
        return LOG_INFO;
    if (strcasecmp(name, "error") == 0)
        return LOG_ERROR;
    if (strcasecmp(name, "warning") == 0 || strcasecmp(name, "warn") == 0)
        return LOG_WARNING;
    if (strcasecmp(name, "notice") == 0)
        return LOG_NOTICE;
    if (strcasecmp(name, "debug") == 0)
        return LOG_DEBUG;
    if (valid)
        *valid = 0;
    return LOG_INFO;
}

void init_logging(int enabled, log_module_level_t level, const char *requested_path)
{
    requested_log_path = requested_path ? strdup(requested_path) : NULL;

    lm_main = create_log_module("main");
    lm_input = create_log_module("input");
    lm_toc = create_log_module("toc");
    lm_output = create_log_module("output");
    lm_decoder = create_log_module("decoder");
    lm_metadata = create_log_module("metadata");

    log_configure(enabled, level);
    log_init();
}

int logging_open_session(const char *directory)
{
    if (!log_is_enabled())
        return 0;
    if (active_log_path[0] != '\0')
        return 0;

    if (requested_log_path)
    {
        if (strlen(requested_log_path) >= sizeof(active_log_path))
            return -1;
        strcpy(active_log_path, requested_log_path);
    }
    else
    {
        const char *base = directory && directory[0] ? directory : ".";
        if (make_session_path(base, active_log_path, sizeof(active_log_path)) != 0)
            return -1;
    }

    if (set_log_file(active_log_path) != 0)
    {
        fprintf(stderr, "Unable to create log file '%s': %s\n",
                active_log_path, strerror(errno));
        active_log_path[0] = '\0';
        return -1;
    }
    return 0;
}

const char *logging_file_path(void)
{
    return active_log_path[0] ? active_log_path : NULL;
}

void destroy_logging(void)
{
    if (log_is_enabled() && active_log_path[0] == '\0')
        (void)logging_open_session(".");
    log_destroy();
    free(requested_log_path);
    requested_log_path = NULL;
    active_log_path[0] = '\0';
    lm_main = lm_input = lm_toc = lm_output = lm_decoder = lm_metadata = NULL;
}

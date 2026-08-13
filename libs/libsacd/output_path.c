#include <stdio.h>
#include <string.h>

#include "output_path.h"

static output_rename_hook_t rename_hook = rename;

int output_path_with_marker(const char *path, const char *marker, char *output, size_t output_size)
{
    const char *slash;
    const char *dot;
    size_t prefix;
    int written;

    if (!path || !marker || !output || output_size == 0)
        return -1;
    slash = strrchr(path, '/');
#if defined(_WIN32)
    {
        const char *backslash = strrchr(path, '\\');
        if (!slash || (backslash && backslash > slash))
            slash = backslash;
    }
#endif
    dot = strrchr(path, '.');
    if (!dot || (slash && dot < slash))
        dot = path + strlen(path);
    prefix = (size_t)(dot - path);
    written = snprintf(output, output_size, "%.*s.%s%s", (int)prefix, path, marker, dot);
    return written < 0 || (size_t)written >= output_size ? -1 : 0;
}

int output_publish_atomic(const char *source, const char *destination)
{
    return rename_hook(source, destination);
}

void output_set_rename_hook(output_rename_hook_t hook)
{
    rename_hook = hook ? hook : rename;
}

#ifndef SACD_OUTPUT_PATH_H_INCLUDED
#define SACD_OUTPUT_PATH_H_INCLUDED

#include <stddef.h>

typedef int (*output_rename_hook_t)(const char *source, const char *destination);

int output_path_with_marker(const char *path, const char *marker, char *output, size_t output_size);
int output_publish_atomic(const char *source, const char *destination);
void output_set_rename_hook(output_rename_hook_t hook);

#endif

#include <string.h>

#include "toc_recovery.h"

int area_toc_header_valid(const uint8_t *data, const char *expected_id)
{
    return data && expected_id && memcmp(data, expected_id, 8) == 0;
}

int master_toc_header_valid(const uint8_t *data)
{
    return data && memcmp(data, "SACDMTOC", 8) == 0;
}

void area_toc_copy_backup(uint8_t *destination, const uint8_t *backup, size_t size)
{
    if (destination && backup && size)
        memcpy(destination, backup, size);
}

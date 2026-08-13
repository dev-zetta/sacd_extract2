#ifndef SACD_TOC_RECOVERY_H_INCLUDED
#define SACD_TOC_RECOVERY_H_INCLUDED

#include <stddef.h>
#include <stdint.h>

int area_toc_header_valid(const uint8_t *data, const char *expected_id);
int master_toc_header_valid(const uint8_t *data);
void area_toc_copy_backup(uint8_t *destination, const uint8_t *backup, size_t size);

#endif

#ifndef SACD_MEDIA_RECOVERY_H_INCLUDED
#define SACD_MEDIA_RECOVERY_H_INCLUDED

#include <stddef.h>
#include <stdint.h>

#include "sacd_input.h"

#define SACD_SECTOR_RETRIES 2u

typedef sacd_input_read_result_t (*media_read_callback_t)(void *userdata, uint32_t lsn,
                                                          uint32_t blocks, uint8_t *buffer);

typedef struct media_recovery_result_s
{
    uint32_t valid_blocks;
    uint32_t holes;
    int fatal;
    int error_number;
    char error_string[256];
} media_recovery_result_t;

media_recovery_result_t media_read_recover(media_read_callback_t reader, void *userdata,
                                           uint32_t lsn, uint32_t blocks, size_t sector_size,
                                           uint8_t *buffer, uint8_t *valid_map);
media_recovery_result_t media_read_recover_limited(media_read_callback_t reader, void *userdata,
                                                   uint32_t lsn, uint32_t blocks, size_t sector_size,
                                                   uint8_t *buffer, uint8_t *valid_map,
                                                   uint32_t hole_limit);

int media_error_budget_add(uint32_t current, uint32_t added, uint32_t maximum,
                           uint32_t *new_total);

#endif

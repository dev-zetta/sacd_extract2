#include <stdio.h>
#include <string.h>

#include "media_recovery.h"

media_recovery_result_t media_read_recover_limited(media_read_callback_t reader, void *userdata,
                                                   uint32_t lsn, uint32_t blocks, size_t sector_size,
                                                   uint8_t *buffer, uint8_t *valid_map,
                                                   uint32_t hole_limit)
{
    media_recovery_result_t recovery;
    sacd_input_read_result_t batch;
    uint32_t prefix;

    memset(&recovery, 0, sizeof(recovery));
    memset(valid_map, 0, blocks);
    batch = reader(userdata, lsn, blocks, buffer);
    prefix = batch.blocks_read > blocks ? blocks : batch.blocks_read;
    if (prefix)
    {
        memset(valid_map, 1, prefix);
        recovery.valid_blocks = prefix;
    }
    if (batch.status == SACD_INPUT_FATAL)
    {
        recovery.fatal = 1;
        recovery.error_number = batch.error_number;
        snprintf(recovery.error_string, sizeof(recovery.error_string), "%s", batch.error_string);
        return recovery;
    }
    if (prefix == blocks)
        return recovery;

    for (uint32_t index = prefix; index < blocks; ++index)
    {
        sacd_input_read_result_t sector;
        unsigned int attempt;
        for (attempt = 0; attempt <= SACD_SECTOR_RETRIES; ++attempt)
        {
            sector = reader(userdata, lsn + index, 1, buffer + index * sector_size);
            if (sector.status == SACD_INPUT_FATAL)
            {
                recovery.fatal = 1;
                recovery.error_number = sector.error_number;
                snprintf(recovery.error_string, sizeof(recovery.error_string), "%s", sector.error_string);
                return recovery;
            }
            if (sector.blocks_read == 1)
            {
                valid_map[index] = 1;
                recovery.valid_blocks++;
                break;
            }
        }
        if (!valid_map[index])
        {
            memset(buffer + index * sector_size, 0, sector_size);
            recovery.holes++;
            if (recovery.holes >= hole_limit)
                break;
        }
    }
    return recovery;
}

media_recovery_result_t media_read_recover(media_read_callback_t reader, void *userdata,
                                           uint32_t lsn, uint32_t blocks, size_t sector_size,
                                           uint8_t *buffer, uint8_t *valid_map)
{
    return media_read_recover_limited(reader, userdata, lsn, blocks, sector_size,
                                      buffer, valid_map, UINT32_MAX);
}

int media_error_budget_add(uint32_t current, uint32_t added, uint32_t maximum,
                           uint32_t *new_total)
{
    uint32_t total = UINT32_MAX - current < added ? UINT32_MAX : current + added;
    if (new_total)
        *new_total = total;
    return total > maximum;
}

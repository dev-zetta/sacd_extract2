#include "scarletbook_output.h"
#include "output_status.h"

int sacd_output_merge_exit_status(int current_status, int output_result)
{
    if (current_status == SACD_OUTPUT_RESULT_INTERRUPTED ||
        output_result == SACD_OUTPUT_RESULT_INTERRUPTED)
        return SACD_OUTPUT_RESULT_INTERRUPTED;
    if (current_status == SACD_OUTPUT_RESULT_FATAL ||
        output_result == SACD_OUTPUT_RESULT_FATAL)
        return SACD_OUTPUT_RESULT_FATAL;
    if (current_status == SACD_OUTPUT_RESULT_PARTIAL ||
        output_result == SACD_OUTPUT_RESULT_PARTIAL)
        return SACD_OUTPUT_RESULT_PARTIAL;
    return SACD_OUTPUT_RESULT_CLEAN;
}

int sacd_output_queue_continues(int output_result)
{
    return output_result == SACD_OUTPUT_RESULT_CLEAN ||
           output_result == SACD_OUTPUT_RESULT_PARTIAL;
}

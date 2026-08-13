execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env POSIXLY_CORRECT=1
            "${EXTRACTOR}" --tracks 2 positional.iso --dsf --multichannel --version
    RESULT_VARIABLE mixed_result
    OUTPUT_VARIABLE mixed_output
    ERROR_VARIABLE mixed_error)
if(NOT mixed_result EQUAL 0)
    message(FATAL_ERROR "Mixed-order GNU-style options failed: ${mixed_result}: ${mixed_error}")
endif()
foreach(expected
        "Input -i (iso or connection) [positional.iso]"
        "Asked multi channels -m"
        "Asked dsf -s"
        "Tracks selected: -t: 2")
    string(FIND "${mixed_output}" "${expected}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "Mixed-order output lacks '${expected}':\n${mixed_output}")
    endif()
endforeach()

execute_process(
    COMMAND "${EXTRACTOR}" --version legacy.iso --mch-tracks --output-dsf
    RESULT_VARIABLE legacy_result
    OUTPUT_VARIABLE legacy_output
    ERROR_VARIABLE legacy_error)
if(NOT legacy_result EQUAL 0 OR
   NOT legacy_output MATCHES "Asked multi channels -m" OR
   NOT legacy_output MATCHES "Asked dsf -s")
    message(FATAL_ERROR "Legacy aliases or post-input options regressed: ${legacy_result}: ${legacy_error}")
endif()

execute_process(
    COMMAND "${EXTRACTOR}" first.iso --version second.iso
    RESULT_VARIABLE duplicate_result
    ERROR_VARIABLE duplicate_error)
if(NOT duplicate_result EQUAL 2 OR
   NOT duplicate_error MATCHES "Only one input source may be specified")
    message(FATAL_ERROR "Multiple positional inputs must fail with exit 2")
endif()

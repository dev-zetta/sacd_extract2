file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
file(WRITE "${WORK}/sacd_extract.cfg"
     "logging=1\nlog_file=${WORK}/config.log\nmax_read_errors=99\n")
execute_process(
    COMMAND "${EXTRACTOR}" --no-log --max-read-errors 3 --version
    WORKING_DIRECTORY "${WORK}"
    RESULT_VARIABLE disabled_result
    OUTPUT_VARIABLE disabled_output
    ERROR_VARIABLE disabled_error)
if(NOT disabled_result EQUAL 0)
    message(FATAL_ERROR "CLI precedence run failed: ${disabled_result}: ${disabled_error}")
endif()
if(EXISTS "${WORK}/config.log")
    message(FATAL_ERROR "--no-log did not override config logging")
endif()
string(FIND "${disabled_output}" "Maximum media defects per output [3]" max_position)
if(max_position EQUAL -1)
    message(FATAL_ERROR "CLI max-read-errors did not override config")
endif()

file(WRITE "${WORK}/sacd_extract.cfg" "logging=0\n")
execute_process(
    COMMAND "${EXTRACTOR}" --log-file "${WORK}/cli.log" --version
    WORKING_DIRECTORY "${WORK}"
    RESULT_VARIABLE enabled_result
    ERROR_VARIABLE enabled_error)
if(NOT enabled_result EQUAL 0 OR NOT EXISTS "${WORK}/cli.log")
    message(FATAL_ERROR "--log-file did not override disabled config: ${enabled_result}: ${enabled_error}")
endif()

execute_process(
    COMMAND "${EXTRACTOR}" --help --log-file "${WORK}/help.log"
    WORKING_DIRECTORY "${WORK}"
    RESULT_VARIABLE help_result
    ERROR_VARIABLE help_error)
if(NOT help_result EQUAL 0 OR NOT EXISTS "${WORK}/help.log")
    message(FATAL_ERROR "Explicit help logging failed: ${help_result}: ${help_error}")
endif()
file(READ "${WORK}/help.log" help_log)
if(NOT help_log MATCHES "NOTICE .* main .* invocation mode=help")
    message(FATAL_ERROR "Help session log lacks its structured invocation record")
endif()

file(MAKE_DIRECTORY "${WORK}/commented")
file(WRITE "${WORK}/commented/sacd_extract.cfg" "# logging=1\n; log_file=ignored.log\n")
execute_process(
    COMMAND "${EXTRACTOR}" --version
    WORKING_DIRECTORY "${WORK}/commented"
    RESULT_VARIABLE comment_result)
file(GLOB comment_logs "${WORK}/commented/*.log")
if(NOT comment_result EQUAL 0 OR comment_logs)
    message(FATAL_ERROR "Commented logging settings must be ignored")
endif()

execute_process(
    COMMAND "${EXTRACTOR}" --unknown-option
    WORKING_DIRECTORY "${WORK}"
    RESULT_VARIABLE invalid_result)
if(NOT invalid_result EQUAL 2)
    message(FATAL_ERROR "Unknown options must return exit status 2, got ${invalid_result}")
endif()

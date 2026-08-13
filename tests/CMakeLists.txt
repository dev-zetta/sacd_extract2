add_library(unity STATIC vendor/unity/src/unity.c)
target_include_directories(unity PUBLIC vendor/unity/src)

function(add_sacd_unit_test name)
    add_executable(${name} ${ARGN})
    target_link_libraries(${name} PRIVATE sacd_extract_core unity)
    add_test(NAME ${name} COMMAND ${name})
endfunction()

add_sacd_unit_test(test_media_recovery test_media_recovery.c)
add_sacd_unit_test(test_output_path test_output_path.c)
add_sacd_unit_test(test_output_status test_output_status.c)
add_sacd_unit_test(test_logging test_logging.c)
add_sacd_unit_test(test_frame_recovery test_frame_recovery.c)
add_sacd_unit_test(test_cuesheet test_cuesheet.c)
add_sacd_unit_test(test_toc_recovery test_toc_recovery.c)
add_sacd_unit_test(test_xml_writer test_xml_writer.c)

add_test(NAME test_cli_config_precedence
         COMMAND ${CMAKE_COMMAND}
                 -DEXTRACTOR=$<TARGET_FILE:sacd_extract>
                 -DWORK=${CMAKE_CURRENT_BINARY_DIR}/cli-config
                 -P ${CMAKE_CURRENT_SOURCE_DIR}/test_cli_config.cmake)

add_test(NAME test_cli_argument_order
         COMMAND ${CMAKE_COMMAND}
                 -DEXTRACTOR=$<TARGET_FILE:sacd_extract>
                 -P ${CMAKE_CURRENT_SOURCE_DIR}/test_cli_argument_order.cmake)

set(SACD_TEST_ISO "" CACHE FILEPATH "Optional SACD ISO used by integration tests")
if(SACD_TEST_ISO)
    if(NOT EXISTS "${SACD_TEST_ISO}")
        message(FATAL_ERROR "SACD_TEST_ISO does not exist: ${SACD_TEST_ISO}")
    endif()
    add_test(NAME integration_metadata
             COMMAND $<TARGET_FILE:sacd_extract> -P -i ${SACD_TEST_ISO})
    set_tests_properties(integration_metadata PROPERTIES LABELS integration)

    add_test(NAME integration_cue
             COMMAND ${CMAKE_COMMAND}
                     -DEXTRACTOR=$<TARGET_FILE:sacd_extract>
                     -DISO=${SACD_TEST_ISO}
                     -DWORK=${CMAKE_CURRENT_BINARY_DIR}/integration-cue
                     -P ${CMAKE_CURRENT_SOURCE_DIR}/run_cue_integration.cmake)
    set_tests_properties(integration_cue PROPERTIES LABELS integration)

    add_test(NAME integration_selected_track
             COMMAND ${CMAKE_COMMAND}
                     -DEXTRACTOR=$<TARGET_FILE:sacd_extract>
                     -DISO=${SACD_TEST_ISO}
                     -DWORK=${CMAKE_CURRENT_BINARY_DIR}/integration-track
                     -P ${CMAKE_CURRENT_SOURCE_DIR}/run_track_integration.cmake)
    set_tests_properties(integration_selected_track PROPERTIES LABELS integration TIMEOUT 900)
endif()

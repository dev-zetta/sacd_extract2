file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}/output")
execute_process(
    COMMAND "${EXTRACTOR}" --stereo --flac --flac-rate 88200 --tracks 1
            --track-output-dir "${WORK}/output" --input "${ISO}"
    WORKING_DIRECTORY "${WORK}"
    RESULT_VARIABLE result)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Selected-track FLAC extraction failed: ${result}")
endif()
file(GLOB_RECURSE flac_files "${WORK}/output/*.flac")
file(GLOB_RECURSE partial_files "${WORK}/output/*.partial.flac")
file(GLOB_RECURSE inprogress_files "${WORK}/output/*.inprogress.flac")
list(LENGTH flac_files flac_count)
list(LENGTH partial_files partial_count)
list(LENGTH inprogress_files inprogress_count)
if(NOT flac_count EQUAL 1 OR NOT partial_count EQUAL 0 OR NOT inprogress_count EQUAL 0)
    message(FATAL_ERROR "Expected exactly one clean FLAC output")
endif()
list(GET flac_files 0 flac_file)
file(READ "${flac_file}" magic LIMIT 4 HEX)
if(NOT magic STREQUAL "664c6143")
    message(FATAL_ERROR "Output is not a native FLAC stream")
endif()

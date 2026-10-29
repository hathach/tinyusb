# The SystemView capture's manifest beside a traced image, <target>.sysview.json, which test/hil/sysview_ci.py checks
# before flashing: board, example, every -D the build dir was configured with, the image's sha256, and the commit
# and cleanliness of the tree when it last linked.
#
# Included: defines the functions below. Run as
#   cmake -DCONFIG=<configure-time json> -DELF=<image> -DOUT=<manifest> -DTOP=<repo> -P sysview_manifest.cmake
# (sysview_manifest's POST_BUILD): completes CONFIG into OUT.

if (CMAKE_SCRIPT_MODE_FILE)
  file(READ ${CONFIG} manifest)
  file(SHA256 ${ELF} sha)
  execute_process(COMMAND git -C ${TOP} rev-parse HEAD OUTPUT_VARIABLE commit
                  OUTPUT_STRIP_TRAILING_WHITESPACE RESULT_VARIABLE rc ERROR_QUIET)
  if (NOT rc EQUAL 0)
    set(commit "unknown")
  endif ()
  # tracked changes anywhere; untracked files only where the build reads sources, since CI's own fetches
  # (pico-sdk, ninja) sit untracked at the top
  execute_process(COMMAND git -C ${TOP} status --porcelain --untracked-files=no OUTPUT_VARIABLE tracked
                  RESULT_VARIABLE rc1 ERROR_QUIET)
  execute_process(COMMAND git -C ${TOP} ls-files --others --exclude-standard -- src hw examples lib
                  OUTPUT_VARIABLE untracked RESULT_VARIABLE rc2 ERROR_QUIET)
  if (rc1 EQUAL 0 AND rc2 EQUAL 0 AND tracked STREQUAL "" AND untracked STREQUAL "")
    set(dirty false)
  else ()
    set(dirty true)
  endif ()
  string(JSON manifest SET "${manifest}" elfSha256 "\"${sha}\"")
  string(JSON manifest SET "${manifest}" commit "\"${commit}\"")
  string(JSON manifest SET "${manifest}" dirty ${dirty})
  file(WRITE ${OUT} "${manifest}\n")
  return()
endif ()

# Call before project(): only then does a -D given to this configure still carry the command line's marks
# (type UNINITIALIZED, or this help string) before option() or set(CACHE) retype it. The names accumulate
# across configures, as a later configure without them leaves their values in place.
function(sysview_record_command_line)
  set(names $CACHE{SYSVIEW_COMMAND_LINE_VARS})
  get_cmake_property(cache CACHE_VARIABLES)
  foreach (var IN LISTS cache)
    get_property(type CACHE ${var} PROPERTY TYPE)
    get_property(help CACHE ${var} PROPERTY HELPSTRING)
    if (type STREQUAL "UNINITIALIZED" OR help STREQUAL "No help, variable specified on the command line.")
      list(APPEND names ${var})
    endif ()
  endforeach ()
  list(REMOVE_DUPLICATES names)
  set(SYSVIEW_COMMAND_LINE_VARS "${names}" CACHE INTERNAL "every -D a configure of this build dir was given")
endfunction()

# Writes TARGET's manifest config, the command-line set at its current values, and completes it each time TARGET links.
function(sysview_manifest TARGET BOARD EXAMPLE)
  set(manifest "{}")
  string(JSON manifest SET "${manifest}" board "\"${BOARD}\"")
  string(JSON manifest SET "${manifest}" example "\"${EXAMPLE}\"")
  string(JSON manifest SET "${manifest}" commandLine "{}")
  foreach (var IN LISTS SYSVIEW_COMMAND_LINE_VARS)
    string(REPLACE "\\" "\\\\" value "$CACHE{${var}}")
    string(REPLACE "\"" "\\\"" value "${value}")
    string(JSON manifest SET "${manifest}" commandLine ${var} "\"${value}\"")
  endforeach ()
  file(CONFIGURE OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${TARGET}.sysview.in.json CONTENT "${manifest}\n")
  add_custom_command(TARGET ${TARGET} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -DCONFIG=${CMAKE_CURRENT_BINARY_DIR}/${TARGET}.sysview.in.json
            -DELF=$<TARGET_FILE:${TARGET}> -DOUT=$<TARGET_FILE_DIR:${TARGET}>/${TARGET}.sysview.json
            -DTOP=${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../..
            -P ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/sysview_manifest.cmake
    VERBATIM)
endfunction()

# cmake -DCONFIG=<configure-time json> -DELF=<image> -DOUT=<manifest> -DTOP=<repo> -P sysview_manifest.cmake
# The SystemView capture's manifest (test/hil/sysview_ci.py checks it before flashing): CONFIG's board, example and
# command-line -D set, plus the image's sha256 and the checkout's commit when the image last linked.
file(READ ${CONFIG} manifest)
file(SHA256 ${ELF} sha)
execute_process(COMMAND git -C ${TOP} rev-parse HEAD OUTPUT_VARIABLE commit
                OUTPUT_STRIP_TRAILING_WHITESPACE RESULT_VARIABLE rc ERROR_QUIET)
if (NOT rc EQUAL 0)
  set(commit "unknown")
endif ()
execute_process(COMMAND git -C ${TOP} status --porcelain --untracked-files=no OUTPUT_VARIABLE status
                RESULT_VARIABLE rc ERROR_QUIET)
if (rc EQUAL 0 AND status STREQUAL "")
  set(dirty false)
else ()
  set(dirty true)
endif ()
string(JSON manifest SET "${manifest}" elfSha256 "\"${sha}\"")
string(JSON manifest SET "${manifest}" commit "\"${commit}\"")
string(JSON manifest SET "${manifest}" dirty ${dirty})
file(WRITE ${OUT} "${manifest}\n")

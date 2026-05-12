# Embed a file as a C++ byte array header.
#
# Usage:
#   embed_asset(<absolute-path-to-file> <output-dir> <list-var-name>)
#
# Produces <output-dir>/embedded/<sanitized_name>.h declaring:
#   namespace mp::embedded {
#       extern const unsigned char <sym>[];
#       extern const unsigned int  <sym>_len;
#   }
# Adds the generated header path to <list-var-name>.

function(embed_asset SRC OUTPUT_DIR LIST_VAR)
    if(NOT EXISTS ${SRC})
        message(FATAL_ERROR "embed_asset: missing source file ${SRC}\nRun scripts/fetch-assets.sh first.")
    endif()

    file(RELATIVE_PATH REL_PATH ${CMAKE_SOURCE_DIR} ${SRC})
    string(REGEX REPLACE "[^A-Za-z0-9]" "_" SYM ${REL_PATH})
    string(REGEX REPLACE "^_+" "" SYM ${SYM})

    set(OUT_HEADER ${OUTPUT_DIR}/embedded/${SYM}.h)

    add_custom_command(
        OUTPUT  ${OUT_HEADER}
        COMMAND ${Python3_EXECUTABLE}
                ${CMAKE_SOURCE_DIR}/cmake/embed_asset.py
                ${SRC}
                ${OUT_HEADER}
                ${SYM}
        DEPENDS ${SRC} ${CMAKE_SOURCE_DIR}/cmake/embed_asset.py
        COMMENT "Embedding ${REL_PATH}"
        VERBATIM
    )

    list(APPEND ${LIST_VAR} ${OUT_HEADER})
    set(${LIST_VAR} ${${LIST_VAR}} PARENT_SCOPE)
endfunction()

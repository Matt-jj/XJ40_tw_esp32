cmake_minimum_required(VERSION 3.16.0)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# ---------------------------------------------------------------------------
# Embed the current git short hash as FIRMWARE_GIT_HASH.
#
# The version number (version.h) gives the human timeline; the hash is the
# provenance check - it identifies the exact commit a binary came from even
# if the version was not bumped. Generated live at build time, never stored.
# ---------------------------------------------------------------------------
execute_process(
    COMMAND git rev-parse --short HEAD
    WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
    OUTPUT_VARIABLE GIT_SHORT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)

if(GIT_SHORT_HASH)
    # Mark builds made from a modified tree - the hash alone would misrepresent
    # them as matching that commit exactly.
    execute_process(
        COMMAND git status --porcelain --untracked-files=no
        WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
        OUTPUT_VARIABLE GIT_DIRTY
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    if(GIT_DIRTY)
        set(GIT_SHORT_HASH "${GIT_SHORT_HASH}-dirty")
    endif()

    idf_build_set_property(COMPILE_DEFINITIONS
                           "FIRMWARE_GIT_HASH=\"${GIT_SHORT_HASH}\"" APPEND)

    # Re-run cmake when HEAD moves, so the baked hash cannot go stale.
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
                 "${CMAKE_CURRENT_LIST_DIR}/.git/HEAD")
else()
    message(WARNING "git hash unavailable - firmware will report 'nogit'")
endif()

project(XJ40_tw_esp32)

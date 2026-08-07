cmake_minimum_required(VERSION 3.24)

if(NOT DEFINED BUNDLE_ROOT OR "${BUNDLE_ROOT}" STREQUAL "")
    message(FATAL_ERROR "BUNDLE_ROOT must be provided")
endif()
if(NOT DEFINED KEEP_WEBENGINE_LOCALES OR "${KEEP_WEBENGINE_LOCALES}" STREQUAL "")
    set(KEEP_WEBENGINE_LOCALES en-US ru)
endif()

file(REAL_PATH "${BUNDLE_ROOT}" bundle_root)
if(NOT IS_DIRECTORY "${bundle_root}")
    message(FATAL_ERROR "Bundle root does not exist: ${bundle_root}")
endif()

foreach(locale IN LISTS KEEP_WEBENGINE_LOCALES)
    if(NOT locale MATCHES "^[A-Za-z0-9_-]+$")
        message(FATAL_ERROR "Invalid WebEngine locale name: ${locale}")
    endif()
endforeach()

file(GLOB_RECURSE bundle_entries LIST_DIRECTORIES true "${bundle_root}/*")
set(locale_directories)
foreach(entry IN LISTS bundle_entries)
    if(IS_DIRECTORY "${entry}")
        get_filename_component(entry_name "${entry}" NAME)
        if(entry_name STREQUAL "qtwebengine_locales")
            list(APPEND locale_directories "${entry}")
        endif()
    endif()
endforeach()

list(LENGTH locale_directories locale_directory_count)
if(NOT locale_directory_count EQUAL 1)
    message(FATAL_ERROR
        "Expected exactly one qtwebengine_locales directory under ${bundle_root}, "
        "found ${locale_directory_count}"
    )
endif()
list(GET locale_directories 0 locale_directory)

foreach(locale IN LISTS KEEP_WEBENGINE_LOCALES)
    if(NOT EXISTS "${locale_directory}/${locale}.pak")
        message(FATAL_ERROR
            "Required WebEngine locale is missing: ${locale_directory}/${locale}.pak"
        )
    endif()
endforeach()

file(GLOB locale_files LIST_DIRECTORIES false "${locale_directory}/*.pak")
set(removed_count 0)
set(removed_bytes 0)
foreach(locale_file IN LISTS locale_files)
    get_filename_component(locale_name "${locale_file}" NAME_WE)
    if(NOT locale_name IN_LIST KEEP_WEBENGINE_LOCALES)
        file(SIZE "${locale_file}" locale_bytes)
        file(REMOVE "${locale_file}")
        if(EXISTS "${locale_file}")
            message(FATAL_ERROR "Cannot remove WebEngine locale: ${locale_file}")
        endif()
        math(EXPR removed_count "${removed_count} + 1")
        math(EXPR removed_bytes "${removed_bytes} + ${locale_bytes}")
    endif()
endforeach()

string(JOIN ", " kept_locales ${KEEP_WEBENGINE_LOCALES})
message(STATUS
    "Kept WebEngine locales: ${kept_locales}; removed ${removed_count} files "
    "(${removed_bytes} bytes)"
)

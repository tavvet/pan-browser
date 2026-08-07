cmake_minimum_required(VERSION 3.24)

foreach(required_variable IN ITEMS BUNDLE_ROOT OUTPUT_JSON OUTPUT_MARKDOWN)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} must be provided")
    endif()
endforeach()

file(REAL_PATH "${BUNDLE_ROOT}" bundle_root)
if(NOT IS_DIRECTORY "${bundle_root}")
    message(FATAL_ERROR "Bundle root does not exist: ${bundle_root}")
endif()

if(NOT DEFINED BUNDLE_LABEL OR "${BUNDLE_LABEL}" STREQUAL "")
    get_filename_component(BUNDLE_LABEL "${bundle_root}" NAME)
endif()

function(json_escape input output)
    set(escaped "${input}")
    string(REPLACE "\\" "\\\\" escaped "${escaped}")
    string(REPLACE "\"" "\\\"" escaped "${escaped}")
    string(REPLACE "\n" "\\n" escaped "${escaped}")
    string(REPLACE "\r" "\\r" escaped "${escaped}")
    string(REPLACE "\t" "\\t" escaped "${escaped}")
    set(${output} "${escaped}" PARENT_SCOPE)
endfunction()

function(format_mib bytes output)
    math(EXPR tenths "(${bytes} * 10 + 524288) / 1048576")
    math(EXPR whole "${tenths} / 10")
    math(EXPR fraction "${tenths} % 10")
    set(${output} "${whole}.${fraction} MiB" PARENT_SCOPE)
endfunction()

get_filename_component(json_directory "${OUTPUT_JSON}" DIRECTORY)
get_filename_component(markdown_directory "${OUTPUT_MARKDOWN}" DIRECTORY)
foreach(output_directory IN ITEMS "${json_directory}" "${markdown_directory}")
    if(NOT output_directory STREQUAL "")
        file(MAKE_DIRECTORY "${output_directory}")
    endif()
endforeach()

file(GLOB_RECURSE discovered_entries
    LIST_DIRECTORIES false
    RELATIVE "${bundle_root}"
    "${bundle_root}/*"
)
set(bundle_files)
foreach(relative_path IN LISTS discovered_entries)
    set(absolute_path "${bundle_root}/${relative_path}")
    if(NOT IS_SYMLINK "${absolute_path}" AND NOT IS_DIRECTORY "${absolute_path}")
        list(APPEND bundle_files "${relative_path}")
    endif()
endforeach()
list(SORT bundle_files)

set(total_bytes 0)
foreach(relative_path IN LISTS bundle_files)
    file(SIZE "${bundle_root}/${relative_path}" file_bytes)
    math(EXPR total_bytes "${total_bytes} + ${file_bytes}")
endforeach()
list(LENGTH bundle_files file_count)

string(TIMESTAMP generated_at "%Y-%m-%dT%H:%M:%SZ" UTC)
json_escape("${BUNDLE_LABEL}" json_label)
format_mib(${total_bytes} total_mib)

file(WRITE "${OUTPUT_JSON}"
    "{\n"
    "  \"schemaVersion\": 1,\n"
    "  \"label\": \"${json_label}\",\n"
    "  \"generatedAtUtc\": \"${generated_at}\",\n"
    "  \"totalBytes\": ${total_bytes},\n"
    "  \"fileCount\": ${file_count},\n"
    "  \"files\": [\n"
)

set(separator "")
set(size_entries)
foreach(relative_path IN LISTS bundle_files)
    file(SIZE "${bundle_root}/${relative_path}" file_bytes)
    json_escape("${relative_path}" json_path)
    file(APPEND "${OUTPUT_JSON}"
        "${separator}    {\"path\": \"${json_path}\", \"bytes\": ${file_bytes}}"
    )
    set(separator ",\n")

    set(padded_size "00000000000000000000${file_bytes}")
    string(LENGTH "${padded_size}" padded_length)
    math(EXPR padded_start "${padded_length} - 20")
    string(SUBSTRING "${padded_size}" ${padded_start} 20 padded_size)
    string(REPLACE ";" "\\;" list_path "${relative_path}")
    list(APPEND size_entries "${padded_size}|${file_bytes}|${list_path}")
endforeach()
file(APPEND "${OUTPUT_JSON}" "\n  ]\n}\n")

list(SORT size_entries COMPARE STRING ORDER DESCENDING)
list(LENGTH size_entries size_entry_count)
if(size_entry_count GREATER 25)
    list(SUBLIST size_entries 0 25 size_entries)
endif()

file(WRITE "${OUTPUT_MARKDOWN}"
    "# Bundle audit: ${BUNDLE_LABEL}\n\n"
    "- Generated: `${generated_at}`\n"
    "- Files: `${file_count}`\n"
    "- Total: `${total_bytes}` bytes (${total_mib})\n\n"
    "## Largest files\n\n"
    "| File | Bytes | Size |\n"
    "|---|---:|---:|\n"
)

foreach(entry IN LISTS size_entries)
    string(REGEX MATCH "^[0-9]+\\|([0-9]+)\\|(.*)$" unused "${entry}")
    set(file_bytes "${CMAKE_MATCH_1}")
    set(relative_path "${CMAKE_MATCH_2}")
    string(REPLACE "|" "\\|" markdown_path "${relative_path}")
    format_mib(${file_bytes} file_mib)
    file(APPEND "${OUTPUT_MARKDOWN}"
        "| `${markdown_path}` | ${file_bytes} | ${file_mib} |\n"
    )
endforeach()

message(STATUS
    "Audited ${BUNDLE_LABEL}: ${file_count} files, ${total_bytes} bytes (${total_mib})"
)

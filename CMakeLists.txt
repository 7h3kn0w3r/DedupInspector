cmake_minimum_required(VERSION 3.16)
project(DedupExplorer LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (NOT WIN32)
    message(FATAL_ERROR "DedupExplorer targets Win32 API and can only be built for Windows (MSVC or MinGW-w64).")
endif()

# Static runtime -> single portable EXE, no redistributables needed.
if (MSVC)
    foreach(flag_var
            CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE
            CMAKE_CXX_FLAGS_MINSIZEREL CMAKE_CXX_FLAGS_RELWITHDEBINFO)
        if (${flag_var} MATCHES "/MD")
            string(REGEX REPLACE "/MD" "/MT" ${flag_var} "${${flag_var}}")
        endif()
    endforeach()
else()
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static -static-libgcc -static-libstdc++")
endif()

add_compile_definitions(UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)

set(SOURCES
    src/main.cpp
    src/mainwindow.cpp
    src/mft_parser.cpp
    src/reparse_parser.cpp
    src/stream_parser.cpp
    src/ckhr_parser.cpp
    src/chunk_parser.cpp
    src/recovery_engine.cpp
    src/utils.cpp
)

set(RESOURCES
    resources/app.rc
)

add_executable(DedupExplorer WIN32 ${SOURCES} ${RESOURCES})

target_include_directories(DedupExplorer PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(DedupExplorer PRIVATE
    comctl32
    ole32
    oleaut32
    uuid
    shell32
    shlwapi
    Cabinet     # Windows Compression API (compressapi.h) - XPRESS/XPRESS_HUFF/MSZIP decompression
)

if (MSVC)
    target_compile_options(DedupExplorer PRIVATE /W4 /permissive- /utf-8)
    target_link_options(DedupExplorer PRIVATE /SUBSYSTEM:WINDOWS)
else()
    target_compile_options(DedupExplorer PRIVATE -Wall -Wextra -municode)
endif()

# Strip in release builds to keep the binary small and portable.
set_target_properties(DedupExplorer PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

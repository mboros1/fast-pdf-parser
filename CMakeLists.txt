cmake_minimum_required(VERSION 3.15)
project(fast-pdf-parser VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build type configuration
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Compiler flags
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -fsanitize=address,undefined")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -DNDEBUG")

# Find packages from Conan
find_package(Threads REQUIRED)
find_package(nlohmann_json REQUIRED CONFIG)

# MuPDF - we'll handle this separately since it's not in Conan Center
find_package(PkgConfig REQUIRED)
pkg_check_modules(MUPDF mupdf)
if(NOT MUPDF_FOUND)
    message(STATUS "MuPDF not found via pkg-config, trying manual search")
    find_path(MUPDF_INCLUDE_DIR mupdf/fitz.h
        PATHS /usr/local/include /usr/include /opt/homebrew/include
    )
    find_library(MUPDF_LIBRARY
        NAMES mupdf mupdf-third
        PATHS /usr/local/lib /usr/lib /opt/homebrew/lib
    )
    if(MUPDF_INCLUDE_DIR AND MUPDF_LIBRARY)
        set(MUPDF_FOUND TRUE)
        set(MUPDF_INCLUDE_DIRS ${MUPDF_INCLUDE_DIR})
        set(MUPDF_LIBRARIES ${MUPDF_LIBRARY})
    else()
        message(FATAL_ERROR "MuPDF not found. Please install MuPDF development libraries.")
    endif()
endif()

# Library sources
set(SOURCES
    src/fast_pdf_parser.cpp
    src/thread_pool.cpp
    src/text_extractor.cpp
    src/json_serializer.cpp
    src/batch_processor.cpp
)

# Create static library
add_library(fastpdfparser STATIC ${SOURCES})

target_include_directories(fastpdfparser
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${MUPDF_INCLUDE_DIRS}
)

target_link_libraries(fastpdfparser
    PUBLIC
        Threads::Threads
        nlohmann_json::nlohmann_json
    PRIVATE
        ${MUPDF_LIBRARIES}
)

target_compile_options(fastpdfparser PRIVATE ${MUPDF_CFLAGS_OTHER})

# CLI executable
add_executable(fast-pdf-parser src/main.cpp)
target_link_libraries(fast-pdf-parser PRIVATE fastpdfparser)

# Benchmarking executable
add_executable(benchmark benchmark/benchmark.cpp)
target_link_libraries(benchmark PRIVATE fastpdfparser)

# Tests
enable_testing()
add_subdirectory(tests)

# Installation
install(TARGETS fastpdfparser fast-pdf-parser
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

install(DIRECTORY include/
    DESTINATION include
)

# Package configuration
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/fast-pdf-parser-config-version.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion
)
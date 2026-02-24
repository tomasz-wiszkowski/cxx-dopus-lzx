cmake_minimum_required(VERSION 4.00)
project(opusPlugin VERSION 1.0 LANGUAGES C CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set C standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

# Platform detection
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
set(PLATFORM_X64 TRUE)
else()
    set(PLATFORM_X64 FALSE)
endif()

# Configuration-specific settings
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(IS_DEBUG TRUE)
else()
    set(IS_DEBUG FALSE)
endif()

set (CPP_DIRECTIVES PLUGIN_EXPORTS)
if (IS_DEBUG)
    list(APPEND CPP_DIRECTIVES _DEBUG)
else()
    list(APPEND CPP_DIRECTIVES NDEBUG)
endif()

if (PLATFORM_X64)
list(APPEND CPP_DIRECTIVES _WIN64)
else()
    list(APPEND CPP_DIRECTIVES WIN32)
endif()

include(FetchContent)
FetchContent_Declare(
    unlzx
    GIT_REPOSITORY https://github.com/tomasz-wiszkowski/cxx-unlzx.git
    GIT_TAG        v1.2
    GIT_SHALLOW    TRUE
)

set(BUILD_TESTS OFF)
set(BUILD_EXECUTABLE OFF)

FetchContent_MakeAvailable(unlzx)

# Add subdirectories
# add_subdirectory(external/dependency EXCLUDE_FROM_ALL)
add_subdirectory(external)
add_subdirectory(src)

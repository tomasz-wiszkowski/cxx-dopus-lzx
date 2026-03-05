# Source files
set(PLUGIN_SOURCES
    dllmain.cc
    plugin.cc
    text_utils.cc
    path.cc
)

# Header files
set(PLUGIN_HEADERS
    dopus_wstring_view_span.hh
    plugin.hh
    text_utils.hh
    path.hh
    custom_columns.hh
)

# Create shared library (DLL)
add_library(${PLUGIN_NAME} SHARED ${PLUGIN_SOURCES} ${PLUGIN_HEADERS})

# Include directories
target_link_libraries(${PLUGIN_NAME} PRIVATE OpusSDK::headers unlzx_lib Pathcch Shlwapi Shell32)
target_include_directories(${PLUGIN_NAME} PRIVATE ${unlzx_SOURCE_DIR}/src)
target_compile_definitions(${PLUGIN_NAME} PRIVATE ${CPP_DIRECTIVES})
target_compile_definitions(${PLUGIN_NAME} PRIVATE DOPUS_PLUGIN_HELPER UNICODE)

# Compiler flags
if(MSVC)
    target_compile_options(${PLUGIN_NAME} PRIVATE
        /W4      # Warning level
        /wd4100  # disable the unreferenced formal parameter
        /wd4201  # disable the nameless struct/union
        /wd4530  # disable the C++ exception handler used, but unwind semantics are not enabled
        /EHs-c-  # disable C++ exceptions
    )

    # Conformance mode
    target_compile_options(${PLUGIN_NAME} PRIVATE /permissive-)

    # SDL checks
    target_compile_options(${PLUGIN_NAME} PRIVATE /sdl)
else()
    # GCC/Clang flags
    target_compile_options(${PLUGIN_NAME} PRIVATE
        -Wall
        -fno-exceptions  # Disable C++ exceptions
        $<$<CONFIG:Debug>:-O0 -g>
        $<$<CONFIG:Release>:-Os -fomit-frame-pointer>
    )
endif()

# Linker flags
if(MSVC)
    set_target_properties(${PLUGIN_NAME} PROPERTIES
        LINK_FLAGS_DEBUG "/DEBUG /INCREMENTAL"
        LINK_FLAGS_RELEASE "/DEBUG /INCREMENTAL:NO /OPT:REF /OPT:ICF"
    )

    # Set subsystem to Windows
    target_link_options(${PLUGIN_NAME} PRIVATE /SUBSYSTEM:WINDOWS)
endif()

# Set output name
set_target_properties(${PLUGIN_NAME} PROPERTIES
    PREFIX ""
)

set_target_properties(${PLUGIN_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_SOURCE_DIR}/bin/Debug"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/bin/Release"
)

message(STATUS "Building tests...")

include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.14.0
    GIT_SHALLOW    TRUE
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
add_compile_definitions(GTEST_LINKED_AS_SHARED_LIBRARY=0)
add_compile_definitions(GTEST_CREATE_SHARED_LIBRARY=0)
# Ensure gtest is compiled as a static library
set(BUILD_SHARED_LIBS OFF)
FetchContent_MakeAvailable(googletest)
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(gtest PRIVATE "-Wno-character-conversion")
    target_compile_options(gtest_main PRIVATE "-Wno-character-conversion")
endif()

add_executable(${PLUGIN_NAME}_test
    path.cc
    path.hh
    path_test.cc
)
target_link_libraries(${PLUGIN_NAME}_test PUBLIC gtest gtest_main PRIVATE Pathcch Shlwapi Shell32)
target_include_directories(${PLUGIN_NAME}_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

if(MSVC)
    target_compile_options(${PLUGIN_NAME}_test PRIVATE
        /W4
        /wd4100  # disable the unreferenced formal parameter
        /wd4201  # disable the nameless struct/union
        /wd4530  # disable the C++ exception handler used, but unwind semantics are not enabled
        /EHs-c-  # disable C++ exceptions
    )
endif()

include(GoogleTest)
gtest_discover_tests(${PLUGIN_NAME}_test)

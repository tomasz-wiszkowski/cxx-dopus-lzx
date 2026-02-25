# Source files
set(PLUGIN_SOURCES
    dllmain.cpp
    plugin.cpp
    text_utils.cc
)

# Header files
set(PLUGIN_HEADERS
    dopus_wstring_view_span.hh
    plugin.hpp
    stdafx.h
    text_utils.hh
)

# Create shared library (DLL)
add_library(${PLUGIN_NAME} SHARED ${PLUGIN_SOURCES} ${PLUGIN_HEADERS})

# Include directories
target_link_libraries(${PLUGIN_NAME} PRIVATE OpusSDK::headers unlzx_lib)
target_include_directories(${PLUGIN_NAME} PRIVATE ${unlzx_SOURCE_DIR}/src)
target_compile_definitions(${PLUGIN_NAME} PRIVATE ${CPP_DIRECTIVES})
target_compile_definitions(${PLUGIN_NAME} PRIVATE DOPUS_PLUGIN_HELPER UNICODE)

# Compiler flags
if(MSVC)
    target_compile_options(${PLUGIN_NAME} PRIVATE
        /W4      # Warning level
        /wd4100  # Unreferenced formal parameter
        /EHs-c-  # Disable C++ exceptions
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
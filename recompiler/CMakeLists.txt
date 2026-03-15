# Recompiler tool (nesrecomp)

add_executable(nesrecomp
    src/main.cpp
    src/rom.cpp
    src/decoder.cpp
    src/decoder_6502.cpp
    src/analyzer.cpp
    src/bank_tracker.cpp
    src/symbol_table.cpp
    src/generator.cpp
    src/ir/ir_builder.cpp
    src/ir/ir_optimizer.cpp
    src/codegen/c_emitter.cpp
    src/ram_mapping.cpp
)

target_include_directories(nesrecomp
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Enable position-independent code for shared library support
set_target_properties(nesrecomp PROPERTIES
    POSITION_INDEPENDENT_CODE ON
)

# Platform-specific settings
if(APPLE)
    target_compile_definitions(nesrecomp PRIVATE NES_PLATFORM_MACOS)
elseif(WIN32)
    target_compile_definitions(nesrecomp PRIVATE NES_PLATFORM_WINDOWS)
else()
    target_compile_definitions(nesrecomp PRIVATE NES_PLATFORM_LINUX)
endif()

# Test analyzer executable (for testing analyzer without full codegen)
add_executable(test_analyzer
    src/test_analyzer.cpp
    src/rom.cpp
    src/decoder_6502.cpp
    src/analyzer.cpp
    src/bank_tracker.cpp
    src/ram_mapping.cpp
)

target_include_directories(test_analyzer
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(APPLE)
    target_compile_definitions(test_analyzer PRIVATE NES_PLATFORM_MACOS)
elseif(WIN32)
    target_compile_definitions(test_analyzer PRIVATE NES_PLATFORM_WINDOWS)
else()
    target_compile_definitions(test_analyzer PRIVATE NES_PLATFORM_LINUX)
endif()

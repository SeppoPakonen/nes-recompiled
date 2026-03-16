# runtime/CMakeLists.txt
# NES Runtime Library

add_library(nesrt STATIC
    src/nesrt.c
    src/interpreter.c
    src/cpu6502_interp.c
    src/ppu.c
    src/audio.c
    src/audio_stats.c
    src/mapper.c
    src/nesrt_trace.c
    src/exec_trace.c
    src/platform_sdl.cpp
    src/ram_code_cache.cpp
)

# ImGui sources (if using SDL2)
find_package(SDL2 QUIET)
if(SDL2_FOUND)
    target_sources(nesrt PRIVATE
        third_party/imgui/imgui.cpp
        third_party/imgui/imgui_draw.cpp
        third_party/imgui/imgui_tables.cpp
        third_party/imgui/imgui_widgets.cpp
        third_party/imgui/imgui_demo.cpp
        third_party/imgui/backends/imgui_impl_sdl2.cpp
        third_party/imgui/backends/imgui_impl_sdlrenderer2.cpp
    )
    target_include_directories(nesrt PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui
    )
endif()

target_include_directories(nesrt PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(nesrt PUBLIC c_std_11 cxx_std_17)

# Debug mode option
option(NES_DEBUG "Enable debug logging" OFF)
option(NES_DEBUG_VRAM "Enable VRAM debug logging" OFF)
option(NES_DEBUG_FRAME "Enable frame debug logging" OFF)
option(NES_DEBUG_REGS "Enable register debug logging" OFF)
option(NES_DEBUG_PPU "Enable PPU debug logging" OFF)

if(NES_DEBUG)
    target_compile_definitions(nesrt PUBLIC NES_DEBUG)
    message(STATUS "NES debug logging enabled")
endif()

if(NES_DEBUG_VRAM)
    target_compile_definitions(nesrt PUBLIC NES_DEBUG_VRAM)
endif()
if(NES_DEBUG_FRAME)
    target_compile_definitions(nesrt PUBLIC NES_DEBUG_FRAME)
endif()
if(NES_DEBUG_REGS)
    target_compile_definitions(nesrt PUBLIC NES_DEBUG_REGS)
endif()
if(NES_DEBUG_PPU)
    target_compile_definitions(nesrt PUBLIC NES_DEBUG_PPU)
endif()

# Platform-specific settings
if(APPLE)
    target_compile_definitions(nesrt PRIVATE NES_PLATFORM_MACOS)
elseif(WIN32)
    target_compile_definitions(nesrt PRIVATE NES_PLATFORM_WINDOWS)
else()
    target_compile_definitions(nesrt PRIVATE NES_PLATFORM_LINUX)
endif()

# SDL2 for platform layer (optional)
if(SDL2_FOUND)
    target_link_libraries(nesrt PUBLIC SDL2::SDL2)
    target_compile_definitions(nesrt PUBLIC NES_HAS_SDL2)
endif()

cmake_minimum_required(VERSION 3.28)
project(YamiPan LANGUAGES C CXX)

# PC port of Forbidden Siren 2 (Europe, SCES_538.51), reconstructed function by function.
# src/yamipan is the Pan layer (modern C++), src/sdk SCE's API on top of it, and everything else
# the game, kept authentic (.claude/skills/yamipan-port/references/cpp-port.md).

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
set(BUILD_SHARED_LIBS OFF)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/YamiPan)
list(PREPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

if(WIN32 AND NOT MSVC)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static")
endif()

# The prebuilt FFmpeg and shadercross trees are not committed: a sibling checkout's are reused.
set(YAMIPAN_SIBLING_EXTERN "${CMAKE_CURRENT_SOURCE_DIR}/../MioPan/extern"
    CACHE PATH "extern/ of a checkout holding the prebuilt FFmpeg and shadercross")

include(sdl3)
include(imgui)
include(shaders)
include(ffmpeg)

# ---- the game layer, with SCE's API under it -----------------------------------------------------
file(GLOB_RECURSE YAMIPAN_GAME_SOURCES CONFIGURE_DEPENDS
    src/game/*.c src/game/*.cpp
    src/iop/*.c src/iop/*.cpp
    src/lib/*.c src/lib/*.cpp
    src/rena/*.c src/rena/*.cpp
    src/rx/*.c src/rx/*.cpp
    src/sdk/*.c src/sdk/*.cpp
    src/sdpr2/*.c src/sdpr2/*.cpp
    src/sound/*.c src/sound/*.cpp)

# The game's own .c files keep the names its strings give them (rxfile.c); like every TU in RosePan
# they build as C++, since the SDK's declarations are C++ here.
set_source_files_properties(${YAMIPAN_GAME_SOURCES} PROPERTIES LANGUAGE CXX)

# The process entry belongs to the host (src/main.cpp); the game's main stays as the ROM has it.
set_source_files_properties(src/game/gamesystem.cpp PROPERTIES COMPILE_DEFINITIONS main=gameMain)

add_library(yamipan_game OBJECT ${YAMIPAN_GAME_SOURCES})
target_include_directories(yamipan_game PUBLIC src src/sdk)
target_link_libraries(yamipan_game PUBLIC SDL3::SDL3)
# NOT OPTIONAL. Reconstructed code reads the game's file formats in place and walks packets through
# u_int*; GCC exploits the aliasing violation from -O2 up (MioPan's release-only crashes).
target_compile_options(yamipan_game PRIVATE -fno-strict-aliasing -fpermissive -w)
# StageScript::findEntry keeps the ROM's test of this for null (a sub-scene has no scripts[0]).
set_source_files_properties(src/game/scriptrunner.cpp PROPERTIES COMPILE_OPTIONS -fno-delete-null-pointer-checks)

# ---- the Pan layer -------------------------------------------------------------------------------
file(GLOB_RECURSE YAMIPAN_HOST_SOURCES CONFIGURE_DEPENDS src/yamipan/*.cpp)

add_executable(YamiPan src/main.cpp ${YAMIPAN_HOST_SOURCES} $<TARGET_OBJECTS:yamipan_game>)
add_dependencies(YamiPan compile_shaders)
target_include_directories(YamiPan PRIVATE src)
target_link_libraries(YamiPan PRIVATE SDL3::SDL3 imgui)
target_compile_options(YamiPan PRIVATE -Wall -Wextra -Wpedantic -Wshadow -Wno-missing-field-initializers)

if(YAMIPAN_HAVE_FFMPEG)
    target_compile_definitions(YamiPan PRIVATE YAMIPAN_HAVE_FFMPEG=1)
    target_compile_definitions(yamipan_game PRIVATE YAMIPAN_HAVE_FFMPEG=1)
    target_link_libraries(YamiPan PRIVATE ffmpeg::avcodec ffmpeg::avutil ffmpeg::swscale)
    add_custom_command(TARGET YamiPan POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${YAMIPAN_FFMPEG_ROOT}/bin $<TARGET_FILE_DIR:YamiPan>)
endif()

add_custom_command(TARGET YamiPan POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/resources
            $<TARGET_FILE_DIR:YamiPan>/resources)

# The build stamp in the log banner: scoped to logger.cpp so a new commit rebuilds only it.
find_package(Git QUIET)
set(YAMIPAN_BUILD_COMMIT unknown)
if(GIT_FOUND)
    execute_process(COMMAND ${GIT_EXECUTABLE} rev-parse --short=12 HEAD
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE YAMIPAN_BUILD_COMMIT OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
endif()
set_source_files_properties(src/yamipan/core/logger.cpp PROPERTIES
    COMPILE_DEFINITIONS "YAMIPAN_BUILD_COMMIT=\"${YAMIPAN_BUILD_COMMIT}\"")

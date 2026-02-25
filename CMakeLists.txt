cmake_minimum_required(VERSION 3.20)
project(dungeons LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS_DEBUG   "-g -O0 -DDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG")

# ─── Fetch raylib ────────────────────────────────────────────────────────────
include(FetchContent)
FetchContent_Declare(raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG        5.5
    GIT_SHALLOW    TRUE
)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(raylib)

# ─── Gather all .cpp under src/ ──────────────────────────────────────────────
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS
    ${CMAKE_SOURCE_DIR}/src/*.cpp
)

add_executable(${PROJECT_NAME} ${SOURCES})

# ─── Include dirs ────────────────────────────────────────────────────────────
# Root src/ so you can do #include "core/ecs.h" style includes,
# plus flat dirs so existing #include "level.h" still works.
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/vendor
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_SOURCE_DIR}/src/input
    ${CMAKE_SOURCE_DIR}/src/player
    ${CMAKE_SOURCE_DIR}/src/audio
    ${CMAKE_SOURCE_DIR}/src/rendering
    ${CMAKE_SOURCE_DIR}/src/rendering/HUD
    ${CMAKE_SOURCE_DIR}/src/rendering/materials
    ${CMAKE_SOURCE_DIR}/src/networking
    ${CMAKE_SOURCE_DIR}/src/physics
    ${CMAKE_SOURCE_DIR}/src/ai
)

# ─── Platform options ─────────────────────────────────────────────────────────
option(PLATFORM_RPI "Build for Raspberry Pi (OpenGL ES 2.0)" OFF)

if(PLATFORM_RPI)
    set(PLATFORM "DRM" CACHE STRING "" FORCE)  # tells raylib to use DRM/KMS
    add_compile_definitions(PLATFORM_RPI)
endif()

# ─── Link ────────────────────────────────────────────────────────────────────
target_link_libraries(${PROJECT_NAME} PRIVATE raylib)

if(EMSCRIPTEN)
    set(WEB_SHELL "${CMAKE_SOURCE_DIR}/shell.html" CACHE STRING "HTML shell template")
    set(EM_FLAGS
        "-s USE_GLFW=3"
        "-s ASYNCIFY"
        "-s TOTAL_MEMORY=67108864"
        "-s ALLOW_MEMORY_GROWTH=1"
        "--preload-file ${CMAKE_SOURCE_DIR}/assets@/assets"
    )
    if(EXISTS "${WEB_SHELL}")
        list(APPEND EM_FLAGS "--shell-file ${WEB_SHELL}")
    endif()
    string(JOIN " " EM_FLAGS_STR ${EM_FLAGS})

    set_target_properties(${PROJECT_NAME} PROPERTIES
        SUFFIX ".html"
        LINK_FLAGS "${EM_FLAGS_STR}"
    )
elseif(PLATFORM_RPI)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        m pthread dl
        GLESv2 EGL drm gbm
    )
elseif(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE opengl32 gdi32 winmm)
elseif(APPLE)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        "-framework IOKit"
        "-framework Cocoa"
        "-framework OpenGL"
    )
elseif(UNIX)
    target_link_libraries(${PROJECT_NAME} PRIVATE m pthread dl GL X11)
endif()

# ─── Copy assets to build dir (native only) ──────────────────────────────────
if(NOT EMSCRIPTEN)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/assets
            $<TARGET_FILE_DIR:${PROJECT_NAME}>/assets
        COMMENT "Copying assets to build directory"
    )
endif()

# ─── Auto-convert shaders for ES100 builds (web + RPi) ───────────────────────
if(EMSCRIPTEN OR PLATFORM_RPI)
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    add_custom_command(TARGET ${PROJECT_NAME} PRE_BUILD
        COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_SOURCE_DIR}/convert_shaders.py
            ${CMAKE_SOURCE_DIR}/assets/shaders/gl330
            ${CMAKE_SOURCE_DIR}/assets/shaders/es100
        COMMENT "Converting GL330 shaders to ES100"
    )
endif()

# ─── Warnings ────────────────────────────────────────────────────────────────
target_compile_options(${PROJECT_NAME} PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wno-missing-braces>
    $<$<CXX_COMPILER_ID:MSVC>:/W3>
)
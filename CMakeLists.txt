cmake_minimum_required(VERSION 3.16)

project(third_person_shooter_engine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(TPS_ENGINE_ENABLE_IPO "Enable interprocedural optimization (LTO)" ON)
option(TPS_ENGINE_ENABLE_NATIVE_ARCH "Enable -march=native for local builds" OFF)
option(TPS_ENGINE_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

set(TPS_ENGINE_SOURCES
    main.cpp
    engine/core/Engine.cpp
    engine/core/Profiler.cpp
    engine/graphics/Renderer.cpp
    engine/input/InputManager.cpp
    engine/physics/Physics.cpp
    game/Game.cpp
    game/player/Player.cpp
    game/enemies/Enemy.cpp
)

add_executable(third_person_shooter ${TPS_ENGINE_SOURCES})

target_include_directories(third_person_shooter PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

if(MSVC)
    target_compile_options(third_person_shooter PRIVATE /W4 /permissive-)
    if(TPS_ENGINE_WARNINGS_AS_ERRORS)
        target_compile_options(third_person_shooter PRIVATE /WX)
    endif()
else()
    target_compile_options(third_person_shooter PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion
        -Wshadow
        -Wnon-virtual-dtor
    )

    if(TPS_ENGINE_WARNINGS_AS_ERRORS)
        target_compile_options(third_person_shooter PRIVATE -Werror)
    endif()

    target_compile_options(third_person_shooter PRIVATE
        $<$<CONFIG:Release>:-O3>
        $<$<CONFIG:Release>:-DNDEBUG>
    )

    if(TPS_ENGINE_ENABLE_NATIVE_ARCH)
        target_compile_options(third_person_shooter PRIVATE $<$<CONFIG:Release>:-march=native>)
    endif()
endif()

if(TPS_ENGINE_ENABLE_IPO)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT ipo_supported OUTPUT ipo_error)
    if(ipo_supported)
        set_property(TARGET third_person_shooter PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
    else()
        message(STATUS "IPO/LTO not supported: ${ipo_error}")
    endif()
endif()

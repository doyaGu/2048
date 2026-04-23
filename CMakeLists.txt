cmake_minimum_required(VERSION 3.20)

project(game2048 VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
list(PREPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

option(GAME2048_BUILD_TESTS "Build unit tests" ON)
option(GAME2048_ENABLE_WARNINGS "Enable strict compiler warnings" ON)

function(game2048_apply_warnings target_name)
    if (NOT GAME2048_ENABLE_WARNINGS)
        return()
    endif()

    if (MSVC)
        target_compile_options(${target_name} PRIVATE /W4 /permissive- /EHsc)
    else()
        target_compile_options(${target_name} PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
    endif()
endfunction()

function(game2048_configure_raylib)
    set(GAME2048_RAYLIB_HINT_DIR "")
    if (DEFINED raylib_DIR AND NOT raylib_DIR STREQUAL "")
        set(GAME2048_RAYLIB_HINT_DIR "${raylib_DIR}")
    elseif (DEFINED Raylib_DIR AND NOT Raylib_DIR STREQUAL "")
        set(GAME2048_RAYLIB_HINT_DIR "${Raylib_DIR}")
    endif()

    set(GAME2048_RAYLIB_HINT_ROOT "")
    if (DEFINED raylib_ROOT AND NOT raylib_ROOT STREQUAL "")
        set(GAME2048_RAYLIB_HINT_ROOT "${raylib_ROOT}")
    elseif (DEFINED Raylib_ROOT AND NOT Raylib_ROOT STREQUAL "")
        set(GAME2048_RAYLIB_HINT_ROOT "${Raylib_ROOT}")
    endif()

    find_package(raylib CONFIG QUIET)
    if (NOT raylib_FOUND)
        if (GAME2048_RAYLIB_HINT_DIR)
            set(raylib_DIR "${GAME2048_RAYLIB_HINT_DIR}")
            set(Raylib_DIR "${GAME2048_RAYLIB_HINT_DIR}")
        endif()
        if (GAME2048_RAYLIB_HINT_ROOT)
            set(raylib_ROOT "${GAME2048_RAYLIB_HINT_ROOT}")
            set(Raylib_ROOT "${GAME2048_RAYLIB_HINT_ROOT}")
        endif()
        find_package(raylib MODULE REQUIRED)
    endif()

    add_library(game2048_raylib_dep INTERFACE)

    if (TARGET raylib)
        target_link_libraries(game2048_raylib_dep INTERFACE raylib)
    elseif (TARGET Raylib::Raylib)
        target_link_libraries(game2048_raylib_dep INTERFACE Raylib::Raylib)
    elseif (TARGET raylib_static)
        target_link_libraries(game2048_raylib_dep INTERFACE raylib_static)
    elseif (DEFINED raylib_LIBRARIES)
        target_link_libraries(game2048_raylib_dep INTERFACE ${raylib_LIBRARIES})
        if (DEFINED raylib_INCLUDE_DIRS)
            target_include_directories(game2048_raylib_dep INTERFACE ${raylib_INCLUDE_DIRS})
        endif()
    elseif (DEFINED Raylib_LIBRARIES)
        target_link_libraries(game2048_raylib_dep INTERFACE ${Raylib_LIBRARIES})
        if (DEFINED Raylib_INCLUDE_DIRS)
            target_include_directories(game2048_raylib_dep INTERFACE ${Raylib_INCLUDE_DIRS})
        endif()
    else()
        message(FATAL_ERROR "raylib was found, but no supported CMake target or library variables were exposed.")
    endif()

    message(STATUS "Using raylib from find_package(raylib)")
endfunction()

game2048_configure_raylib()

add_library(game2048_core
    src/board.cpp
    src/board_fast.cpp
    src/game.cpp
    src/input_bindings.cpp
    src/interaction_session.cpp
    src/rng.cpp
    src/stats.cpp
)

target_include_directories(game2048_core
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

game2048_apply_warnings(game2048_core)

add_library(game2048_ai
    src/ai/ai_engine.cpp
    src/ai/benchmark.cpp
    src/ai/evaluator.cpp
    src/ai/expectimax.cpp
    src/ai/greedy.cpp
    src/ai/transposition_table.cpp
)

target_include_directories(game2048_ai
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_link_libraries(game2048_ai PUBLIC game2048_core)

game2048_apply_warnings(game2048_ai)

add_executable(game2048
    src/main.cpp
    src/app.cpp
    src/animation.cpp
    src/cli_options.cpp
    src/gamepad_input.cpp
    src/input.cpp
    src/input_source.cpp
    src/input_system.cpp
    src/pointer_input.cpp
    src/layout.cpp
    src/renderer.cpp
    src/ui.cpp
)

target_link_libraries(game2048 PRIVATE game2048_ai game2048_raylib_dep)
game2048_apply_warnings(game2048)

if (WIN32)
    target_compile_definitions(game2048 PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

if (GAME2048_BUILD_TESTS)
    enable_testing()

    add_executable(game2048_tests
        src/animation.cpp
        src/cli_options.cpp
        tests/test_cli_options.cpp
        src/gamepad_input.cpp
        src/input_system.cpp
        src/layout.cpp
        src/pointer_input.cpp
        tests/test_animation.cpp
        tests/test_main.cpp
        tests/test_moves.cpp
        tests/test_board_equivalence.cpp
        tests/test_eval.cpp
        tests/test_game.cpp
        tests/test_interaction.cpp
        tests/test_input_system.cpp
        tests/test_pointer_input.cpp
        tests/test_gamepad_input.cpp
    )

    target_link_libraries(game2048_tests PRIVATE game2048_ai game2048_raylib_dep)
    game2048_apply_warnings(game2048_tests)

    add_test(NAME game2048_tests COMMAND game2048_tests)
endif()

cmake_minimum_required(VERSION 3.20)

project(game2048 VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
list(PREPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

option(GAME2048_BUILD_TESTS "Build unit tests" ON)
option(GAME2048_BUILD_GUI "Build raylib GUI executable" ON)
option(GAME2048_ENABLE_WARNINGS "Enable strict compiler warnings" ON)
option(GAME2048_NATIVE_OPT "Enable native CPU optimizations for release builds" OFF)
option(GAME2048_ENABLE_AVX512 "Enable AVX512 n-tuple kernels when supported" ON)

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

function(game2048_apply_native_opt target_name)
    if (GAME2048_NATIVE_OPT AND NOT MSVC)
        target_compile_options(${target_name} PRIVATE $<$<CONFIG:Release>:-march=native>)
    endif()
endfunction()

function(game2048_configure_target target_name)
    target_include_directories(${target_name} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
    game2048_apply_warnings(${target_name})
    game2048_apply_native_opt(${target_name})
endfunction()

function(game2048_configure_raylib)
    find_package(raylib CONFIG QUIET)
    if (NOT raylib_FOUND)
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
    else()
        message(FATAL_ERROR "raylib was found, but no supported target or variables were exposed.")
    endif()
    message(STATUS "Using raylib from find_package(raylib)")
endfunction()

if (GAME2048_BUILD_GUI)
    game2048_configure_raylib()
endif()

add_library(game2048_core
    src/core/board.cpp
    src/core/board_fast.cpp
    src/core/game.cpp
    src/core/rng.cpp
    src/shared/stats.cpp
)
game2048_configure_target(game2048_core)

add_library(game2048_value
    src/value/ntuple_kernel.cpp
    src/value/ntuple.cpp
    src/search/evaluator.cpp
    src/search/greedy.cpp
)
target_link_libraries(game2048_value PUBLIC game2048_core)
game2048_configure_target(game2048_value)
if (GAME2048_ENABLE_AVX512 AND CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64|amd64")
    target_compile_definitions(game2048_value PRIVATE GAME2048_ENABLE_AVX512=1)
endif()

add_library(game2048_search
    src/search/ai_engine.cpp
    src/search/expectimax.cpp
    src/search/transposition_table.cpp
)
target_link_libraries(game2048_search PUBLIC game2048_value)
game2048_configure_target(game2048_search)

add_library(game2048_training
    src/training/benchmark.cpp
    src/training/selection.cpp
)
target_link_libraries(game2048_training PUBLIC game2048_search)
game2048_configure_target(game2048_training)

add_library(game2048_experiment
    src/experiment/profile.cpp
    src/experiment/runner.cpp
)
target_link_libraries(game2048_experiment PUBLIC game2048_training)
game2048_configure_target(game2048_experiment)

add_library(game2048_cli_lib
    src/cli/cli_app.cpp
    src/cli/cli_options.cpp
)
target_link_libraries(game2048_cli_lib PUBLIC game2048_experiment)
game2048_configure_target(game2048_cli_lib)

add_executable(game2048_cli
    src/cli/main_cli.cpp
)
target_link_libraries(game2048_cli PRIVATE game2048_cli_lib)
game2048_configure_target(game2048_cli)

if (GAME2048_BUILD_GUI)
    add_library(game2048_runtime
        src/runtime/ai_worker.cpp
        src/runtime/runtime_engine.cpp
    )
    target_link_libraries(game2048_runtime PUBLIC game2048_search)
    game2048_configure_target(game2048_runtime)

    add_library(game2048_gui_lib
        src/gui/app.cpp
        src/gui/runtime_event_mapper.cpp
        src/input/gamepad_input.cpp
        src/input/input.cpp
        src/input/input_bindings.cpp
        src/input/input_source.cpp
        src/input/input_system.cpp
        src/input/pointer_input.cpp
        src/ui/animation.cpp
        src/ui/layout.cpp
        src/ui/overlays.cpp
        src/ui/panels.cpp
        src/ui/renderer.cpp
        src/ui/widgets.cpp
        src/ui/ui.cpp
    )
    target_link_libraries(game2048_gui_lib PUBLIC game2048_runtime game2048_cli_lib game2048_raylib_dep)
    game2048_configure_target(game2048_gui_lib)

    add_executable(game2048_gui
        src/gui/main.cpp
    )
    target_link_libraries(game2048_gui PRIVATE game2048_gui_lib)
    game2048_configure_target(game2048_gui)

    if (WIN32)
        target_compile_definitions(game2048_gui PRIVATE _CRT_SECURE_NO_WARNINGS)
    endif()
endif()

if (GAME2048_BUILD_TESTS)
    enable_testing()

    add_executable(game2048_cli_tests
        tests/test_app_cli.cpp
        tests/test_cli_options.cpp
        tests/test_experiment_profile.cpp
        tests/test_main.cpp
        tests/test_moves.cpp
        tests/test_board_equivalence.cpp
        tests/test_eval.cpp
        tests/test_game.cpp
    )
    target_link_libraries(game2048_cli_tests PRIVATE game2048_cli_lib)
    game2048_configure_target(game2048_cli_tests)
    add_test(NAME game2048_cli_tests COMMAND game2048_cli_tests)
endif()

if (GAME2048_BUILD_TESTS AND GAME2048_BUILD_GUI)
    add_executable(game2048_tests
        tests/test_app_cli.cpp
        tests/test_cli_options.cpp
        tests/test_experiment_profile.cpp
        tests/test_animation.cpp
        tests/test_main.cpp
        tests/test_runtime_event_mapper.cpp
        tests/test_runtime_engine.cpp
        tests/test_moves.cpp
        tests/test_board_equivalence.cpp
        tests/test_eval.cpp
        tests/test_game.cpp
        tests/test_input_system.cpp
        tests/test_pointer_input.cpp
        tests/test_gamepad_input.cpp
    )
    target_link_libraries(game2048_tests PRIVATE game2048_gui_lib)
    game2048_configure_target(game2048_tests)
    add_test(NAME game2048_tests COMMAND game2048_tests)
endif()

cmake_minimum_required(VERSION 3.20)

project(game2048 VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(GAME2048_BUILD_TESTS "Build unit tests" ON)
option(GAME2048_ENABLE_WARNINGS "Enable strict compiler warnings" ON)

set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)

add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../raylib" "${CMAKE_BINARY_DIR}/raylib")

add_library(game2048_core
    src/board.cpp
    src/board_fast.cpp
    src/game.cpp
    src/rng.cpp
    src/stats.cpp
)

target_include_directories(game2048_core
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

if (GAME2048_ENABLE_WARNINGS)
    if (MSVC)
        target_compile_options(game2048_core PRIVATE /W4 /permissive- /EHsc)
    else()
        target_compile_options(game2048_core PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
    endif()
endif()

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

if (GAME2048_ENABLE_WARNINGS)
    if (MSVC)
        target_compile_options(game2048_ai PRIVATE /W4 /permissive- /EHsc)
    else()
        target_compile_options(game2048_ai PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
    endif()
endif()

add_executable(game2048
    src/main.cpp
    src/app.cpp
    src/animation.cpp
    src/input.cpp
    src/layout.cpp
    src/renderer.cpp
    src/ui.cpp
)

target_link_libraries(game2048 PRIVATE game2048_ai raylib)

if (GAME2048_ENABLE_WARNINGS)
    if (MSVC)
        target_compile_options(game2048 PRIVATE /W4 /permissive- /EHsc)
    else()
        target_compile_options(game2048 PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
    endif()
endif()

if (WIN32)
    target_compile_definitions(game2048 PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

if (GAME2048_BUILD_TESTS)
    enable_testing()

    add_executable(game2048_tests
        tests/test_main.cpp
        tests/test_moves.cpp
        tests/test_board_equivalence.cpp
        tests/test_eval.cpp
        tests/test_game.cpp
    )

    target_link_libraries(game2048_tests PRIVATE game2048_ai)

    if (GAME2048_ENABLE_WARNINGS)
        if (MSVC)
            target_compile_options(game2048_tests PRIVATE /W4 /permissive- /EHsc)
        else()
            target_compile_options(game2048_tests PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
        endif()
    endif()

    add_test(NAME game2048_tests COMMAND game2048_tests)
endif()

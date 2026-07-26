# CMakeLists.txt for the LANDS engine

cmake_minimum_required(VERSION 3.20)
project(lands_engine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# libraries

add_library(lands STATIC
    src/WinEvaluator.cpp
    src/Card.cpp
    src/Deck.cpp
    src/PlayerState.cpp
)

target_include_directories(lands PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

find_package(GTest REQUIRED)

enable_testing()

add_executable(test_WinEvaluator
    tests/test_WinEvaluator.cpp
)

add_executable(test_Deck
    tests/test_Deck.cpp
)

add_executable(test_PlayerState
    tests/test_PlayerState.cpp
)

target_link_libraries(test_WinEvaluator PRIVATE
    lands GTest::gtest GTest::gtest_main
)

target_link_libraries(test_Deck PRIVATE
    lands GTest::gtest GTest::gtest_main
)

target_link_libraries(test_PlayerState PRIVATE
    lands GTest::gtest GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(test_WinEvaluator)
gtest_discover_tests(test_Deck)
gtest_discover_tests(test_PlayerState)
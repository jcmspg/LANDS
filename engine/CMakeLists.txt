# CMakeLists.txt for the LANDS engine

cmake_minimum_required(VERSION 3.20)
project(lands_engine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Opt-in AddressSanitizer + UndefinedBehaviorSanitizer build:
#   cmake -S engine -B engine/build-san -DLANDS_SANITIZE=ON
option(LANDS_SANITIZE "Build with ASan and UBSan" OFF)
if(LANDS_SANITIZE)
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=undefined)
    add_link_options(-fsanitize=address,undefined)
endif()

# libraries

add_library(lands STATIC
    src/WinEvaluator.cpp
    src/Card.cpp
    src/Deck.cpp
    src/PlayerState.cpp
    src/LandGameMatch.cpp
    src/LandGameMatchSnapshot.cpp
    src/LandRules.cpp
    src/Graveyard.cpp
    src/ActionStack.cpp
    src/RandomBot.cpp
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

add_executable(test_LandGameMatch
    tests/test_LandGameMatch.cpp
)

add_executable(test_LandRules
    tests/test_LandRules.cpp
)

add_executable(test_Graveyard
    tests/test_Graveyard.cpp
)

add_executable(test_ActionStack
    tests/test_ActionStack.cpp
)

add_executable(test_MatchProperties
    tests/test_MatchProperties.cpp
)

add_executable(test_RandomBot
    tests/test_RandomBot.cpp
)

add_executable(test_Snapshot
    tests/test_Snapshot.cpp
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

target_link_libraries(test_LandGameMatch PRIVATE
    lands GTest::gtest GTest::gtest_main
)

target_link_libraries(test_LandRules PRIVATE
    lands GTest::gtest GTest::gtest_main
)

target_link_libraries(test_Graveyard PRIVATE
    lands GTest::gtest GTest::gtest_main
)

target_link_libraries(test_ActionStack PRIVATE
    lands GTest::gtest GTest::gtest_main
)

target_link_libraries(test_MatchProperties PRIVATE
    lands GTest::gtest GTest::gtest_main
)

target_link_libraries(test_RandomBot PRIVATE
    lands GTest::gtest GTest::gtest_main
)

target_link_libraries(test_Snapshot PRIVATE
    lands GTest::gtest GTest::gtest_main
)

# simulation harness (GDD Phase 1 test criterion)
add_executable(lands_sim
    sim/simulate.cpp
)
target_link_libraries(lands_sim PRIVATE lands)

include(GoogleTest)
gtest_discover_tests(test_WinEvaluator)
gtest_discover_tests(test_Deck)
gtest_discover_tests(test_PlayerState)
gtest_discover_tests(test_LandGameMatch)
gtest_discover_tests(test_LandRules)
gtest_discover_tests(test_Graveyard)
gtest_discover_tests(test_ActionStack)
gtest_discover_tests(test_MatchProperties)
gtest_discover_tests(test_RandomBot)
gtest_discover_tests(test_Snapshot)

add_test(NAME simulation_smoke COMMAND lands_sim --matches 500)
add_test(NAME simulation_stalling COMMAND lands_sim --matches 100 --idle-percent 85 --timer-percent 2)

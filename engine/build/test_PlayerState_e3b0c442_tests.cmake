add_test([=[PlayerState.Constructor]=]  /home/joao/projects/LANDS/engine/build/test_PlayerState [==[--gtest_filter=PlayerState.Constructor]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[PlayerState.Constructor]=]
  PROPERTIES
    
    DEF_SOURCE_LINE [==[/home/joao/projects/LANDS/engine/tests/test_PlayerState.cpp:6]==]
    WORKING_DIRECTORY [==[/home/joao/projects/LANDS/engine/build]==]
    SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==]
    
)
add_test([=[PlayerState.DrawFromDeck]=]  /home/joao/projects/LANDS/engine/build/test_PlayerState [==[--gtest_filter=PlayerState.DrawFromDeck]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[PlayerState.DrawFromDeck]=]
  PROPERTIES
    
    DEF_SOURCE_LINE [==[/home/joao/projects/LANDS/engine/tests/test_PlayerState.cpp:12]==]
    WORKING_DIRECTORY [==[/home/joao/projects/LANDS/engine/build]==]
    SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==]
    
)
add_test([=[PlayerState.PlayLand]=]  /home/joao/projects/LANDS/engine/build/test_PlayerState [==[--gtest_filter=PlayerState.PlayLand]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[PlayerState.PlayLand]=]
  PROPERTIES
    
    DEF_SOURCE_LINE [==[/home/joao/projects/LANDS/engine/tests/test_PlayerState.cpp:23]==]
    WORKING_DIRECTORY [==[/home/joao/projects/LANDS/engine/build]==]
    SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==]
    
)
set(test_PlayerState_TESTS [==[PlayerState.Constructor]==] [==[PlayerState.DrawFromDeck]==] [==[PlayerState.PlayLand]==])

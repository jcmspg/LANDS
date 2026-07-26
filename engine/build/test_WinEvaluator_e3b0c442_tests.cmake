add_test([=[WinEvaluator.TestEvaluate]=]  /home/joao/projects/LANDS/engine/build/test_WinEvaluator [==[--gtest_filter=WinEvaluator.TestEvaluate]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[WinEvaluator.TestEvaluate]=]
  PROPERTIES
    
    DEF_SOURCE_LINE [==[/home/joao/projects/LANDS/engine/tests/test_WinEvaluator.cpp:8]==]
    WORKING_DIRECTORY [==[/home/joao/projects/LANDS/engine/build]==]
    SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==]
    
)
set(test_WinEvaluator_TESTS [==[WinEvaluator.TestEvaluate]==])

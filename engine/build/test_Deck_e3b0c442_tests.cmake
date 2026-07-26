add_test([=[Deck.DeckSize]=]  /home/joao/projects/LANDS/engine/build/test_Deck [==[--gtest_filter=Deck.DeckSize]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[Deck.DeckSize]=]
  PROPERTIES
    
    DEF_SOURCE_LINE [==[/home/joao/projects/LANDS/engine/tests/test_Deck.cpp:6]==]
    WORKING_DIRECTORY [==[/home/joao/projects/LANDS/engine/build]==]
    SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==]
    
)
add_test([=[Deck.Draw]=]  /home/joao/projects/LANDS/engine/build/test_Deck [==[--gtest_filter=Deck.Draw]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[Deck.Draw]=]
  PROPERTIES
    
    DEF_SOURCE_LINE [==[/home/joao/projects/LANDS/engine/tests/test_Deck.cpp:11]==]
    WORKING_DIRECTORY [==[/home/joao/projects/LANDS/engine/build]==]
    SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==]
    
)
add_test([=[Deck.Shuffle]=]  /home/joao/projects/LANDS/engine/build/test_Deck [==[--gtest_filter=Deck.Shuffle]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[Deck.Shuffle]=]
  PROPERTIES
    
    DEF_SOURCE_LINE [==[/home/joao/projects/LANDS/engine/tests/test_Deck.cpp:17]==]
    WORKING_DIRECTORY [==[/home/joao/projects/LANDS/engine/build]==]
    SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==]
    
)
add_test([=[Deck.PutOnTop]=]  /home/joao/projects/LANDS/engine/build/test_Deck [==[--gtest_filter=Deck.PutOnTop]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[Deck.PutOnTop]=]
  PROPERTIES
    
    DEF_SOURCE_LINE [==[/home/joao/projects/LANDS/engine/tests/test_Deck.cpp:23]==]
    WORKING_DIRECTORY [==[/home/joao/projects/LANDS/engine/build]==]
    SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==]
    
)
set(test_Deck_TESTS [==[Deck.DeckSize]==] [==[Deck.Draw]==] [==[Deck.Shuffle]==] [==[Deck.PutOnTop]==])

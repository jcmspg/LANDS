#include "lands/WinEvaluator.hpp"
#include "lands/LandType.hpp"

#include <gtest/gtest.h>

namespace lands {

TEST(WinEvaluator, TestEvaluate) {
    WinEvaluator win_evaluator;

    std::array<uint8_t, 5> land_count{0, 0, 0, 0, 0};
    EXPECT_EQ(win_evaluator.evaluate(land_count), WinCondition::NONE);

    land_count = {5, 0, 0, 0, 0};
    EXPECT_EQ(win_evaluator.evaluate(land_count), WinCondition::MONO);

    land_count = {5, 5, 5, 5, 5};
    EXPECT_EQ(win_evaluator.evaluate(land_count), WinCondition::MONO);

    land_count = {1, 1, 1, 1, 2};
    EXPECT_EQ(win_evaluator.evaluate(land_count), WinCondition::DOMAIN);

    land_count = {4, 4, 4, 4, 4};
    EXPECT_EQ(win_evaluator.evaluate(land_count), WinCondition::DOMAIN);

    land_count = {1, 2, 3, 0, 5};
    EXPECT_EQ(win_evaluator.evaluate(land_count), WinCondition::MONO);

    land_count = {1, 2, 3, 4, 0};
    EXPECT_EQ(win_evaluator.evaluate(land_count), WinCondition::NONE);

    land_count = {1, 2, 3, 4, 5};
    EXPECT_EQ(win_evaluator.evaluate(land_count), WinCondition::MONO);
    
    
    
}

TEST(WinEvaluator, TiebreakPrefersMoreDistinctLandTypes) {
    WinEvaluator win_evaluator;
    const std::array<uint8_t, 5> two_types{1, 1, 0, 0, 0};
    const std::array<uint8_t, 5> many_of_one{4, 0, 0, 0, 0};

    EXPECT_EQ(win_evaluator.tiebreak_winner(two_types, many_of_one), 0);    // 2 types beat 4 lands
    EXPECT_EQ(win_evaluator.tiebreak_winner(many_of_one, two_types), 1);
}

TEST(WinEvaluator, TiebreakFallsBackToTotalLands) {
    WinEvaluator win_evaluator;
    const std::array<uint8_t, 5> more{2, 1, 0, 0, 0};
    const std::array<uint8_t, 5> fewer{1, 1, 0, 0, 0};

    EXPECT_EQ(win_evaluator.tiebreak_winner(more, fewer), 0);
    EXPECT_EQ(win_evaluator.tiebreak_winner(fewer, more), 1);
}

TEST(WinEvaluator, TiebreakTrueDraw) {
    WinEvaluator win_evaluator;
    EXPECT_EQ(win_evaluator.tiebreak_winner({1, 2, 0, 0, 0}, {2, 1, 0, 0, 0}), -1);
    EXPECT_EQ(win_evaluator.tiebreak_winner({0, 0, 0, 0, 0}, {0, 0, 0, 0, 0}), -1);
}

} // namespace lands
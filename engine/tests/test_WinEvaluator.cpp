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

} // namespace lands
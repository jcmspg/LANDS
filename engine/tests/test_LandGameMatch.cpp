#include "gtest/gtest.h"
#include "lands/LandGameMatch.hpp"

namespace lands {

TEST(LandGameMatch, InitialState) {
    LandGameMatch match(12345);
    EXPECT_EQ(match.deck_size(), Deck::deck_size_ - 5 - 6);
    EXPECT_EQ(match.deck_size(), 39);
    EXPECT_EQ(match.hand_size(0), 5);
    EXPECT_EQ(match.hand_size(1), 6);
    EXPECT_EQ(match.winner(), WinCondition::NONE);

    EXPECT_EQ(match.land_count(0, LandType::ISLAND), 0);
    EXPECT_EQ(match.land_count(1, LandType::ISLAND), 0);
    EXPECT_EQ(match.land_count(0, LandType::MOUNTAIN), 0);
    EXPECT_EQ(match.land_count(1, LandType::MOUNTAIN), 0);
    EXPECT_EQ(match.land_count(0, LandType::PLAINS), 0);
    EXPECT_EQ(match.land_count(1, LandType::PLAINS), 0);
    EXPECT_EQ(match.land_count(0, LandType::SWAMP), 0);
    EXPECT_EQ(match.land_count(1, LandType::SWAMP), 0);
    EXPECT_EQ(match.land_count(0, LandType::FOREST), 0);
    EXPECT_EQ(match.land_count(1, LandType::FOREST), 0);
}


} // namespace lands
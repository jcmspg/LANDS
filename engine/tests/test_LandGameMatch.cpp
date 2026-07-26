#include "gtest/gtest.h"
#include "lands/LandGameMatch.hpp"

#include <stdexcept>

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

TEST(LandGameMatch, TurnManagement) {

    LandGameMatch match2(42);
    EXPECT_EQ(match2.active_player(), 0);
    EXPECT_EQ(match2.turn_number(), 1);
    EXPECT_EQ(match2.hand_size(0), 5);
    EXPECT_EQ(match2.hand_size(1), 6);
    EXPECT_EQ(match2.deck_size(), 39);
    EXPECT_FALSE(match2.end_turn(1));           // not P2's turn
    EXPECT_TRUE(match2.end_turn(0));            // → P2, still turn 1, no draw
    EXPECT_EQ(match2.active_player(), 1);
    EXPECT_EQ(match2.turn_number(), 1);
    EXPECT_EQ(match2.hand_size(1), 6);
    EXPECT_EQ(match2.deck_size(), 39);
    EXPECT_TRUE(match2.end_turn(1));            // → P1, turn 2, P1 draws
    EXPECT_EQ(match2.active_player(), 0);
    EXPECT_EQ(match2.turn_number(), 2);
    EXPECT_EQ(match2.hand_size(0), 6);          // 5 + 1
    EXPECT_EQ(match2.deck_size(), 38);
    

}

TEST(LandGameMatch, PlayLand) {

    LandGameMatch match3(12345);

    EXPECT_TRUE(match3.play_land(0, 0));
    EXPECT_TRUE(match3.played_land_this_turn());
    EXPECT_FALSE(match3.play_land(0, 0));  // second play same turn
    
    // assert hand/lands unchanged from after the first play
    EXPECT_TRUE(match3.end_turn(0));
    EXPECT_FALSE(match3.played_land_this_turn());
    EXPECT_TRUE(match3.play_land(1, 0));   // opponent can play
    
}

TEST(LandGameMatch, GettersRejectBadPlayerIndex) {
    LandGameMatch match(12345);
    EXPECT_THROW(match.hand_size(-1), std::out_of_range);
    EXPECT_THROW(match.hand_size(2), std::out_of_range);
    EXPECT_THROW(match.land_count(-1, LandType::PLAINS), std::out_of_range);
    EXPECT_THROW(match.land_count(2, LandType::PLAINS), std::out_of_range);

    constexpr auto invalid =
        static_cast<LandType>(static_cast<uint8_t>(LandType::NUM_LAND_TYPES));
    EXPECT_THROW(match.land_count(0, invalid), std::out_of_range);
}

TEST(LandGameMatch, ActionsRejectBadPlayerIndex) {
    LandGameMatch match(12345);
    EXPECT_FALSE(match.play_land(-1, 0));
    EXPECT_FALSE(match.play_land(2, 0));
    EXPECT_FALSE(match.end_turn(-1));
    EXPECT_FALSE(match.end_turn(2));
}

} // namespace lands
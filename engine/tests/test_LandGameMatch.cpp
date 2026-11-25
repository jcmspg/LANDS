#include "gtest/gtest.h"
#include "lands/LandGameMatch.hpp"

#include <stdexcept>

namespace lands {

namespace {

uint32_t first_card_id(const LandGameMatch& match, int player_index) {
    return match.hand(player_index).front().get_identifier();
}

} // namespace

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

    EXPECT_EQ(match.graveyard_size(), 0);
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

    EXPECT_TRUE(match3.play_land(0, first_card_id(match3, 0)));
    EXPECT_TRUE(match3.played_land_this_turn());
    EXPECT_FALSE(match3.play_land(0, first_card_id(match3, 0)));  // second play same turn
    
    // assert hand/lands unchanged from after the first play
    EXPECT_TRUE(match3.end_turn(0));
    EXPECT_FALSE(match3.played_land_this_turn());
    EXPECT_TRUE(match3.play_land(1, first_card_id(match3, 1)));   // opponent can play
    
}

TEST(LandGameMatch, PlayLandMovesExactlyOneCardToBoard) {
    LandGameMatch match(12345);

    const Card played = match.hand(0).front();

    EXPECT_TRUE(match.play_land(0, played.get_identifier()));
    EXPECT_EQ(match.hand_size(0), 4);           // 5 - 1, not 5 - 2
    EXPECT_EQ(match.hand_size(1), 6);

    size_t total_lands = 0;
    for (uint8_t t = 0; t < static_cast<uint8_t>(LandType::NUM_LAND_TYPES); ++t) {
        total_lands += match.land_count(0, static_cast<LandType>(t));
    }
    EXPECT_EQ(total_lands, 1);                  // added once, not twice
    EXPECT_EQ(match.land_count(0, played.get_land_type()), 1);
}

TEST(LandGameMatch, PlayLandByCardIdIgnoresHandPosition) {
    LandGameMatch match(12345);

    const Card last = match.hand(0).back();
    EXPECT_TRUE(match.play_land(0, last.get_identifier()));
    EXPECT_EQ(match.land_count(0, last.get_land_type()), 1);
    for (const Card& card : match.hand(0)) {
        EXPECT_NE(card.get_identifier(), last.get_identifier());
    }
}

TEST(LandGameMatch, PlayLandRejectsCardNotInOwnHand) {
    LandGameMatch match(12345);

    const uint32_t opponent_card = first_card_id(match, 1);
    EXPECT_FALSE(match.play_land(0, opponent_card));    // opponent's card
    EXPECT_FALSE(match.play_land(0, 9999));             // no such card
    EXPECT_EQ(match.hand_size(0), 5);
    EXPECT_EQ(match.hand_size(1), 6);
    EXPECT_FALSE(match.played_land_this_turn());
    EXPECT_EQ(match.stack_size(), 0);
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

TEST(LandGameMatch, HandRejectsBadPlayerIndex) {
    LandGameMatch match(12345);
    EXPECT_THROW(match.hand(-1), std::out_of_range);
    EXPECT_THROW(match.hand(2), std::out_of_range);
}

TEST(LandGameMatch, ActionsRejectBadPlayerIndex) {
    LandGameMatch match(12345);
    EXPECT_FALSE(match.play_land(-1, 0));
    EXPECT_FALSE(match.play_land(2, 0));
    EXPECT_FALSE(match.end_turn(-1));
    EXPECT_FALSE(match.end_turn(2));
}

TEST(LandGameMatch, Stack) {
    LandGameMatch match(12345);
    EXPECT_EQ(match.stack_size(), 0);
    EXPECT_TRUE(match.play_land(0, first_card_id(match, 0)));
    EXPECT_EQ(match.stack_size(), 0);
    EXPECT_TRUE(match.end_turn(0));
    EXPECT_EQ(match.stack_size(), 0);
    EXPECT_EQ(match.winner(), WinCondition::NONE);
}

} // namespace lands
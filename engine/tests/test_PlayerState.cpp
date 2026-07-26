#include "lands/PlayerState.hpp"
#include <gtest/gtest.h>

namespace lands {

TEST(PlayerState, Constructor) {
    PlayerState player_state;
    EXPECT_EQ(player_state.hand_size(), 0);
    EXPECT_EQ(player_state.land_counts(), (std::array<uint8_t, 5>{0, 0, 0, 0, 0}));
}

TEST(PlayerState, DrawFromDeck) {
    PlayerState player_state;
    Deck deck;
    deck.put_on_top(Card(LandType::PLAINS, 0));

    player_state.draw_from(deck);
    EXPECT_EQ(player_state.hand_size(), 1);
    EXPECT_EQ(player_state.land_counts(), (std::array<uint8_t, 5>{0, 0, 0, 0, 0}));
    EXPECT_EQ(player_state.hand()[0].get_land_type(), LandType::PLAINS);
}

TEST(PlayerState, PlayLand) {
    PlayerState player_state;
    Deck deck;
    deck.put_on_top(Card(LandType::PLAINS, 0));

    player_state.draw_from(deck);
    EXPECT_EQ(player_state.hand_size(), 1);
    EXPECT_EQ(player_state.land_counts(), (std::array<uint8_t, 5>{0, 0, 0, 0, 0}));
    EXPECT_TRUE(player_state.hand()[0].get_land_type() == LandType::PLAINS);

    EXPECT_TRUE(player_state.play_land(0));
    EXPECT_EQ(player_state.hand_size(), 0);
    EXPECT_EQ(player_state.land_counts(), (std::array<uint8_t, 5>{1, 0, 0, 0, 0}));
}

TEST(PlayerState, PlayLandBadIndexReturnsFalse) {
    PlayerState player_state;
    EXPECT_FALSE(player_state.play_land(0));
    EXPECT_FALSE(player_state.play_land(99));
}

} // namespace lands
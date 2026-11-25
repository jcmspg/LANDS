#include "lands/PlayerState.hpp"
#include <gtest/gtest.h>
#include <stdexcept>

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

TEST(PlayerState, PlayLandByCardId) {
    PlayerState player_state;
    Deck deck;
    deck.put_on_top(Card(LandType::PLAINS, 100));
    deck.put_on_top(Card(LandType::FOREST, 200));

    player_state.draw_from(deck);   // FOREST 200
    player_state.draw_from(deck);   // PLAINS 100

    EXPECT_TRUE(player_state.play_land(100));   // the id, not the hand position
    EXPECT_EQ(player_state.hand_size(), 1);
    EXPECT_EQ(player_state.hand()[0].get_identifier(), 200);
    EXPECT_EQ(player_state.land_counts(), (std::array<uint8_t, 5>{1, 0, 0, 0, 0}));
}

TEST(PlayerState, PlayLandUnknownCardIdReturnsFalse) {
    PlayerState player_state;
    EXPECT_FALSE(player_state.play_land(0));
    EXPECT_FALSE(player_state.play_land(99));
    EXPECT_EQ(player_state.land_counts(), (std::array<uint8_t, 5>{0, 0, 0, 0, 0}));
}

TEST(PlayerState, HasCard) {
    PlayerState player_state;
    Deck deck;
    deck.put_on_top(Card(LandType::ISLAND, 7));

    EXPECT_FALSE(player_state.has_card(7));
    player_state.draw_from(deck);
    EXPECT_TRUE(player_state.has_card(7));
    EXPECT_FALSE(player_state.has_card(8));
}

TEST(PlayerState, TakeFromHandByCardId) {
    PlayerState player_state;
    Deck deck;
    deck.put_on_top(Card(LandType::ISLAND, 7));

    player_state.draw_from(deck);
    EXPECT_EQ(player_state.take_from_hand(7), Card(LandType::ISLAND, 7));
    EXPECT_EQ(player_state.hand_size(), 0);
    EXPECT_EQ(player_state.land_counts(), (std::array<uint8_t, 5>{0, 0, 0, 0, 0}));  // not on the board
    EXPECT_THROW(player_state.take_from_hand(7), std::out_of_range);
}

} // namespace lands
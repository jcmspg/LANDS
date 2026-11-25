#include <gtest/gtest.h>
#include <set>
#include <stdexcept>
#include "lands/Graveyard.hpp"

namespace lands {

TEST(Graveyard, AddCard) {
    Graveyard graveyard;
    Card card(LandType::FOREST, 1);
    graveyard.add(card);
    EXPECT_EQ(graveyard.size(), 1);
    EXPECT_EQ(graveyard.cards().at(0), card);
}

TEST(Graveyard, TakeCard) {
    Graveyard graveyard;
    Card card(LandType::FOREST, 1);
    graveyard.add(card);
    EXPECT_EQ(graveyard.take_card(card.get_identifier()), card);
    EXPECT_EQ(graveyard.size(), 0);
}

TEST(Graveyard, TakeCardPicksByIdNotPosition) {
    Graveyard graveyard;
    Card first(LandType::FOREST, 1);
    Card second(LandType::ISLAND, 2);
    graveyard.add(first);
    graveyard.add(second);
    EXPECT_EQ(graveyard.take_card(2), second);
    EXPECT_EQ(graveyard.size(), 1);
    EXPECT_EQ(graveyard.cards().at(0), first);
}

TEST(Graveyard, TakeMissingCardThrows) {
    Graveyard graveyard;
    EXPECT_THROW(graveyard.take_card(1), std::out_of_range);
    graveyard.add(Card(LandType::FOREST, 1));
    EXPECT_THROW(graveyard.take_card(2), std::out_of_range);
    EXPECT_EQ(graveyard.size(), 1);
}

TEST(Graveyard, Size) {
    Graveyard graveyard;
    Card card(LandType::FOREST, 1);
    graveyard.add(card);
    EXPECT_EQ(graveyard.size(), 1);
}

} // namespace lands
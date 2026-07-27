#include <gtest/gtest.h>
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
    EXPECT_EQ(graveyard.take_card(0), card);
    EXPECT_EQ(graveyard.size(), 0);
}

TEST(Graveyard, Size) {
    Graveyard graveyard;
    Card card(LandType::FOREST, 1);
    graveyard.add(card);
    EXPECT_EQ(graveyard.size(), 1);
}

} // namespace lands
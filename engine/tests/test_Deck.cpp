#include "lands/Deck.hpp"
#include <gtest/gtest.h>

namespace lands {

TEST(Deck, DeckSize) {
    Deck deck;
    EXPECT_EQ(deck.deck_size(), Deck::deck_size_);
}

TEST(Deck, Draw) {
    Deck deck;
    deck.draw();
    EXPECT_EQ(deck.deck_size(), Deck::deck_size_ - 1);
}

TEST(Deck, Shuffle) {
    Deck deck;
    deck.shuffle(12345);
    EXPECT_EQ(deck.deck_size(), Deck::deck_size_);
}

TEST(Deck, PutOnTop) {
    Deck deck;
    Card card(LandType::PLAINS, 0);
    deck.put_on_top(card);
    EXPECT_EQ(deck.deck_size(), Deck::deck_size_ + 1);
}


} // namespace lands
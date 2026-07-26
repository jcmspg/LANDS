#include "lands/Deck.hpp"
#include <gtest/gtest.h>
#include <stdexcept>

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

TEST(Deck, DrawEmptyThrows) {
    Deck deck;
    while (deck.deck_size() > 0) {
        deck.draw();
    }
    EXPECT_THROW(deck.draw(), std::runtime_error);
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
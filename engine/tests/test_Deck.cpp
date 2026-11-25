#include "lands/Deck.hpp"
#include <gtest/gtest.h>
#include <set>
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

TEST(Deck, ShuffleAssignsUniqueIds) {
    Deck deck;
    deck.shuffle(12345);

    std::set<uint32_t> ids;
    while (deck.deck_size() > 0) {
        ids.insert(deck.draw().get_identifier());
    }
    EXPECT_EQ(ids.size(), Deck::deck_size_);
    EXPECT_EQ(*ids.begin(), 0);
    EXPECT_EQ(*ids.rbegin(), Deck::deck_size_ - 1);
}

TEST(Deck, ShuffleIdsDoNotRevealLandType) {
    Deck deck;
    deck.shuffle(12345);

    // before the shuffle, ids 0-9 were Plains, 10-19 Island, ... (id / 10 == type)
    int mismatches = 0;
    while (deck.deck_size() > 0) {
        const Card card = deck.draw();
        if (card.get_identifier() / 10 != static_cast<uint32_t>(card.get_land_type())) {
            ++mismatches;
        }
    }
    EXPECT_GT(mismatches, 0);
}

TEST(Deck, ShuffleIsDeterministicPerSeed) {
    Deck a;
    Deck b;
    a.shuffle(777);
    b.shuffle(777);
    while (a.deck_size() > 0) {
        EXPECT_EQ(a.draw(), b.draw());
    }
}

TEST(Deck, ShuffleKeepsTenOfEachLandType) {
    Deck deck;
    deck.shuffle(12345);

    int counts[5] = {};
    while (deck.deck_size() > 0) {
        counts[static_cast<size_t>(deck.draw().get_land_type())]++;
    }
    for (int count : counts) {
        EXPECT_EQ(count, 10);
    }
}

TEST(Deck, PutOnTop) {
    Deck deck;
    Card card(LandType::PLAINS, 0);
    deck.put_on_top(card);
    EXPECT_EQ(deck.deck_size(), Deck::deck_size_ + 1);
}


} // namespace lands
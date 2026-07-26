#include "lands/Deck.hpp"
#include <algorithm>
#include <random>
#include <stdexcept>

namespace lands {

// create a deck of 50 cards, 10 of each land type
Deck::Deck() : cards_() {
    uint32_t id = 0;
    for (uint8_t t = 0; t < static_cast<uint8_t>(LandType::NUM_LAND_TYPES); ++t) {
        for (int n = 0; n < 10; ++n) {
            cards_.push_back(Card(static_cast<LandType>(t), id++));
        }
    }
}

size_t Deck::deck_size() const {
    return cards_.size();
}

void Deck::shuffle(uint64_t seed) {
    std::mt19937_64 rng(seed);
    std::shuffle(cards_.begin(), cards_.end(), rng);
}

void Deck::put_on_top(const Card& card) {
    cards_.push_back(card);
}

Card Deck::draw() {
    if (cards_.empty()) {
        throw std::runtime_error("Deck::draw: deck is empty");
    }
    Card card = cards_.back();
    cards_.pop_back();
    return card;
}


} // namespace lands


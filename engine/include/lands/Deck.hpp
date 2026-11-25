#ifndef LANDS_DECK_HPP
#define LANDS_DECK_HPP

#include "lands/Card.hpp"

#include <cstdint>
#include <cstddef>
#include <random>
#include <vector>

namespace lands {

class Deck {
    public:

        static constexpr size_t deck_size_ = 50;


        Deck();
        ~Deck() = default;

        size_t deck_size() const;
        Card draw();
        void shuffle(uint64_t seed);
        void put_on_top(const Card& card);
        std::vector<Card> peek_top(size_t count) const;   // top card first; fewer if the deck is short

    private:
        std::vector<Card> cards_; // back() = top card
};

} // namespace lands

#endif // LANDS_DECK_HPP
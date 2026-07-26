#ifndef LANDS_GRAVEYARD_HPP
#define LANDS_GRAVEYARD_HPP

#include <cstdint>

namespace lands {

class Graveyard {
    public:
        void add(Card card);
        size_t size() const;
        const std::vector<Card>& cards() const;
        Card take_card(size_t index);   // Forest later, returns Card from vector
    };

    private:
        std::vector<Card> cards_;
}

#endif // LANDS_GRAVEYARD_HPP
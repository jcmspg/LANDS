#ifndef LANDS_GRAVEYARD_HPP
#define LANDS_GRAVEYARD_HPP

#include <cstdint>
#include <cstddef>
#include <vector>
#include "lands/Card.hpp"

namespace lands {

class Graveyard {
    public:
        void add(Card card);
        size_t size() const;
        const std::vector<Card>& cards() const;
        Card take_card(uint32_t card_id);   // Forest later; throws if the card is not in the graveyard

    private:
        std::vector<Card> cards_;
};

} // namespace lands



#endif // LANDS_GRAVEYARD_HPP